#include "lib.c"

#include <time.h>
#if !defined(_WIN32) && (defined(__WIN32__) || defined(WIN32) || defined(__MINGW32__))
#include <Windows.h>
bool32_t RtlGenRandom(void* RandomBuffer, uint32_t RandomBufferLength);
#elif
#include <unistd.h>
#include <sys/time.h>
#endif


/* time library: */
/* time stamp code from GLFW:
    GLFW 3.4 POSIX - www.glfw.org
    ------------------------------------------------------------------------
    Copyright (c) 2002-2006 Marcus Geelnard
    Copyright (c) 2006-2017 Camilla Löwy <elmindreda@glfw.org>

    This software is provided 'as-is', without any express or implied
    warranty. In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
        claim that you wrote the original software. If you use this software
        in a product, an acknowledgment in the product documentation would
        be appreciated but is not required.

    2. Altered source versions must be plainly marked as such, and must not
        be misrepresented as being the original software.

    3. This notice may not be removed or altered from any source
        distribution.
*/



static time_info_t g_time_info;
time_info_t* time_now(bool local_time) {
    struct tm time_struct, utc_struct;
    struct timespec spec_struct;
    time_t timer;

    timer = time(NULL);
    if(timer == (time_t)(-1)) {
        return NULL;
    }

    g_time_info.unix_time = (int64_t) timer;

    if(local_time) {
        if(localtime_r(&timer, &time_struct) == NULL) {
            return NULL;
        }
        if(gmtime_r(&timer, &utc_struct) == NULL) {
            return NULL;
        }
    } else {
        if(gmtime_r(&timer, &time_struct) == NULL) {
            return NULL;
        }
    }

    if(timespec_get(&spec_struct, TIME_UTC) == 0) {
        return NULL;
    }

    g_time_info.year         = time_struct.tm_year + 1900;
    g_time_info.day_of_year  = time_struct.tm_yday;
    g_time_info.month        = (time_month_t) time_struct.tm_mon;
    g_time_info.day_of_month = time_struct.tm_mday;
    g_time_info.weekday      = (time_weekday_t) time_struct.tm_wday;
    g_time_info.hours        = time_struct.tm_hour;
    g_time_info.minutes      = time_struct.tm_min;
    g_time_info.seconds      = time_struct.tm_sec;
    if(time_struct.tm_isdst > 0) {
        g_time_info.dst = TIME_DST_STATE_IN_EFFECT;
    } else if(time_struct.tm_isdst == 0) {
        g_time_info.dst = TIME_DST_STATE_NOT_IN_EFFECT;
    } else if(time_struct.tm_isdst < 0) {
        g_time_info.dst = TIME_DST_STATE_UNKNOWN;
    }
    if(local_time) {
        /* we have to convert to float to capture UTC+1.5 etc. */
        g_time_info.timezone_relative_to_UTC = (float)(time_struct.tm_hour - utc_struct.tm_hour)
                + ((float)(time_struct.tm_min - utc_struct.tm_min))/60.0f;
    } else {
        g_time_info.timezone_relative_to_UTC = 0;
    }
    g_time_info.leapyear     = ((g_time_info.year % 4) == 0)
        && (((g_time_info.year % 100) != 0) || ((g_time_info.year % 400) == 0));

    g_time_info.nanoseconds  = spec_struct.tv_nsec % 1000;
    g_time_info.microseconds  = (spec_struct.tv_nsec/1000) % 1000;
    g_time_info.milliseconds  = (spec_struct.tv_nsec/1000000) % 1000;

#if !defined(_WIN32) && (defined(__WIN32__) || defined(WIN32) || defined(__MINGW32__))
    QueryPerformanceFrequency((LARGE_INTEGER*) &g_time_info.time_stamp_frequency_in_Hz);
#elif
    /* as opposed to GLFW, we assume the monotonic clock is available, since it was in Linux since
       2011, and using the CLOCK_REALTIME clock is much less reliable for delta time calculation
       due to the influence on it by NTP. */
    struct timespec posix_ts;
    if (clock_gettime(CLOCK_MONOTONIC, &posix_ts) == 0) {
        return NULL;
    }

    g_time_info.time_stamp_frequency_in_Hz = 1000000000;
#endif

    g_time_info.time_stamp = time_stamp();

    return &g_time_info;
}
uint64_t time_stamp(void) {
#if !defined(_WIN32) && (defined(__WIN32__) || defined(WIN32) || defined(__MINGW32__))
    uint64_t value;
    QueryPerformanceCounter((LARGE_INTEGER*) &value);
    return value;
#elif
    struct timespec ts = {};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000000000 + (uint64_t) ts.tv_nsec;
#endif
}



/* PRNG library for fast generation:
    Based on the public domain xoshiro256++ and splitmix64 algorithms 
    written in 2019/2015 by David Blackman and Sebastiano Vigna (vigna@acm.org)
    Original Code from: https://prng.di.unimi.it/
    https://prng.di.unimi.it/xoshiro256plusplus.c
    https://prng.di.unimi.it/splitmix64.c
    Paper: https://dl.acm.org/doi/10.1145/3460772
    Preprint version: https://vigna.di.unimi.it/ftp/papers/ScrambledLinear.pdf
*/
static uint64_t g_prng_state[4];
void prng_seed(uint64_t seed) {
    /* 4 rounds of splitmix64 with seed as input state */
    for(int i = 0; i < 4; i++) {
        uint64_t z = (seed += 0x9e3779b97f4a7c15);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        g_prng_state[i] = z ^ (z >> 31);
    }
}
static uint64_t prng_rotl(uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}
uint64_t prng_value(void) {
    /* 1 round of xoshiro256++ */

    const uint64_t result = prng_rotl(g_prng_state[0] + g_prng_state[3], 23) + g_prng_state[0];

    const uint64_t t = g_prng_state[1] << 17;

    g_prng_state[2] ^= g_prng_state[0];
    g_prng_state[3] ^= g_prng_state[1];
    g_prng_state[1] ^= g_prng_state[2];
    g_prng_state[0] ^= g_prng_state[3];

    g_prng_state[2] ^= t;

    g_prng_state[3] = prng_rotl(g_prng_state[3], 45);

    return result;
}



uint64_t entropy_uint64(void) {
    uint64_t val;
    bool32_t success;

#if !defined(_WIN32) && (defined(__WIN32__) || defined(WIN32) || defined(__MINGW32__))
    success = RtlGenRandom(&val, sizeof(uint64_t));
#elif /* linux / glibc */
    success = (getentropy(&val, sizeof(uint64_t)) != -1);
#endif

    if(success) {
        return val;
    } else {
        return 0;
    }
}




















/* memory library */


static int mem_header_size(size_t asked_alloc_size) {
    /* since we store the type in the lowest 2 bits of the size, we assume them
       to be 0 in the actual allocation, i.e. the allocation has to be a multiple of 4bytes.
       This leaves the maximum size to be stored in n bits as 2^n-4. From that, we have
       to subtract the header size, to see if the asked for allocation can fit in the rest
       of the actual underlying allocation.
       (This also means the stored allocation size is always bigger, by the size of the header,
       than the usable size of the buffer.)
    */
    if(asked_alloc_size < (1<<8)-4-1) {
        return 1;
    } else if(asked_alloc_size < (1<<16)-4-2) {
        return 2;
    } else if(asked_alloc_size < (1<<32)-4-4) {
        return 4;
    } else if(asked_alloc_size < (1<<64)-4-8) {
        return 8;
    }
    return -1;
}
static size_t mem_actual_alloc_size(size_t needed_size) {
    switch(needed_size%4) {
    case 0:
        return needed_size;
    case 1:
        return needed_size+3;
    case 2:
        return needed_size+2;
    case 3:
        return needed_size+1;
    default:
        return -1;
    }
}
static void mem_cap_set(int header_size, uint64_t value, void* ptr_to_actual_alloc) {
    /* we have to store the size in big endian to be able to read
     * the type bits from the last byte stored; we do this manually */
    uint8_t* write_ptr = (uint8_t*) ptr_to_actual_alloc;
    switch(header_size) {
    case 1:
        write_ptr[0] = value&0xFF | 0;
        break;
    case 2:
        write_ptr[0] = value&0xFF00;
        write_ptr[1] = value&0x00FF | 1;
        break;
    case 4:
        write_ptr[0] = value&0xFF000000;
        write_ptr[1] = value&0x00FF0000;
        write_ptr[2] = value&0x0000FF00;
        write_ptr[3] = value&0x000000FF | 2;
        break;
    case 8:
        write_ptr[0] = value&0xFF00000000000000;
        write_ptr[1] = value&0x00FF000000000000;
        write_ptr[2] = value&0x0000FF0000000000;
        write_ptr[3] = value&0x000000FF00000000;
        write_ptr[4] = value&0x00000000FF000000;
        write_ptr[5] = value&0x0000000000FF0000;
        write_ptr[6] = value&0x000000000000FF00;
        write_ptr[7] = value&0x00000000000000FF | 3;
        break;
    default:
        return -1;
    }
}
/* now we read the type from the last bits and reconstruct the number backwards: */
static int mem_header_size_get(mem_t block) {
    uint8_t* read_ptr = (uint8_t*) block;
    return 1 << (read_ptr[-1] & 3);
}
static uint64_t mem_cap_get(mem_t block) {
    uint8_t* read_ptr = (uint8_t*) block;
    uint64_t value = 0;
    switch(mem_header_size_get(block)) {
    case 1:
        value |= read_ptr[-1]&0xFC;
        break;
    case 2:
        value |= read_ptr[-1]&0xFC;
        value |= (read_ptr[-2]&0xFF << 8);
        break;
    case 4:
        value |= read_ptr[-1]&0xFC;
        value |= (read_ptr[-2]&0xFF << 8);
        value |= (read_ptr[-3]&0xFF << 16);
        value |= (read_ptr[-4]&0xFF << 24);
        break;
    case 8:
        value |= read_ptr[-1]&0xFC;
        value |= (read_ptr[-2]&0xFF << 8);
        value |= (read_ptr[-3]&0xFF << 16);
        value |= (read_ptr[-4]&0xFF << 24);
        value |= (read_ptr[-5]&0xFF << 32);
        value |= (read_ptr[-6]&0xFF << 40);
        value |= (read_ptr[-7]&0xFF << 48);
        value |= (read_ptr[-8]&0xFF << 56);
        break;
    default:
        return -1;
    }
    return value;
}

#define MEM_UNDERLYING_ALLOC malloc
#define MEM_UNDERLYING_RESIZE realloc
#define MEM_UNDERLYING_FREE free

mem_t mem_alloc(size_t asked_alloc_size) {
    int header_size = mem_header_size(asked_alloc_size);
    if(header_size < 0) return NULL;
    size_t underlying_size = mem_actual_alloc_size(asked_alloc_size + header_size);
    void* underlying_alloc = MEM_UNDERLYING_ALLOC(underlying_size);
    mem_cap_set(header_size, underlying_size, underlying_alloc);
    return (mem_t) ((uintptr_t) (underlying_alloc) + header_size);
}
void mem_resize(mem_t* block, size_t new_asked_alloc_size) {
    int old_header_size = mem_header_size_get(block);
    int new_header_size = mem_header_size(new_asked_alloc_size);
}
void mem_free(mem_t block);
size_t mem_capacity(mem_t block) {
    return (size_t) (mem_cap_get(block));
}
void mem_copy(mem_t src, mem_t dst);
void mem_zero(mem_t block);
void mem_search(mem_t block, mem_t search_data);



/* string library */

str_t str_new(char* text);
void str_free(str_t s);
size_t str_len(str_t s);
str_t str_concat(str_t s, char* text);
str_t str_concat_int(str_t s, int64_t i);
str_t str_concat_float(str_t s, double d);
