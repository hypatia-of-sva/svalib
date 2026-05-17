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
    /* we have to recalculate to start with Monday, not Sunday */
    g_time_info.weekday      = (time_weekday_t) ((time_struct.tm_wday - 1)%7);
    g_time_info.week_in_year = (uint8_t) floor((float)(g_time_info.day_of_year - g_time_info.weekday) / 7.0f) + 1;
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








strid_t strid(const char* str) {
    if(str == NULL) return STRID_INVALID;
    return strid_from_len(str, strlen(str));
}
strid_t strid_from_len(const char* str, size_t len) {
    uint8_t* buf;
    if(str == NULL) return STRID_INVALID;
    // max 254 chars so that the max allocation size is 256 bytes == 4 cache lines
    if(len > 254) return STRID_INVALID;
    buf = calloc(len+2, 1);
    if(buf == NULL) return STRID_INVALID;
    buf[0] = len;
    memcpy(&buf[1], str, len);
    return &buf[1];
}
strid_t strid_dup(strid_t id) {
    if(id == STRID_INVALID) return STRID_INVALID;
    return strid_from_len(id, id[-1]);
}
bool strid_equals(strid_t a, strid_t b) {
    if(a == STRID_INVALID || b == STRID_INVALID) return (a == STRID_INVALID && b == STRID_INVALID);
    if(a[-1] != b[-1]) return false;
    else return (strcmp((const char*)a,(const char*)b) == 0);
}
int strid_cmp(strid_t a, strid_t b) {
    if(a == STRID_INVALID) return 1;
    if(b == STRID_INVALID) return -1;
    return strcmp((const char*)a,(const char*)b);
}
int strid_subindex(strid_t outer, strid_t inner) {
    int i, inner_len = inner[-1], outer_len = outer[-1];
    char initial;
    if(outer == STRID_INVALID || inner == STRID_INVALID) return -1;
    if(inner_len > outer_len) return -1;
                // returns true-1 = 0 on equality, i.e. at index 0, and false-1 = -1 on inequality
    else if(inner_len == outer_len) return (strid_equals(outer, inner)-1);
    
    initial = inner[0];
    for(i = 0; i < outer_len - inner_len + 1; i++) {
        if(outer[i] == initial && strncmp(&outer[i], inner, inner_len) == 0)
            return i;
    }
    return -1;
}
void strid_free(strid_t id) {
    if(id != STRID_INVALID)
        free(&id[-1]);
}
size_t strid_len(strid_t id) {
    if(id == STRID_INVALID) return (size_t)-1;
    return (size_t)id[-1];
}




    // str needs to have been _allocated_ by the standard allocator (like strids), otherwise use strbuf_concat
strbuf_t strbuf_from_str(char* str) {
    strbuf_t b;
    b.str = str;
    b.len = strlen(str);
    b.cap = b.len + 1;
    return b;
}
strbuf_t strbuf_alloc(size_t initial_cap) {
    strbuf_t b;
    b.cap = initial_cap+1;
    b.len = 0;
    b.str = calloc(1, initial_cap+1);
    if(b.str == NULL) return STRBUF_ALLOCATION_FAILURE;
    else return b;
}
strbuf_t strbuf_reserve(strbuf_t b, size_t additional_cap) {
    return strbuf_resize(b, b.cap+additional_cap);
}
strbuf_t strbuf_resize(strbuf_t b, size_t new_min_cap) {
    char* newbuf;
    size_t new_cap = size_min(new_min_cap, b.len+1);
    newbuf = realloc(b.str, new_cap+1);
    if(newbuf == NULL) return STRBUF_ALLOCATION_FAILURE;
    b.str = newbuf;
    return b;
}
strbuf_t strbuf_dup(strbuf_t b) {
    strbuf_t bret;
    bret.cap = b.cap;
    bret.len = b.len;
    bret.str = calloc(1, b.cap);
    if(bret.str == NULL) return STRBUF_ALLOCATION_FAILURE;
    memmove(bret.str, b.str, b.cap);
    return bret;
}
void strbuf_free(strbuf_t b) {
    free(b.str);
}
strbuf_t strbuf_zero(strbuf_t b) {
    memset(b.str, 0, b.cap);
    b.len = 0;
    return b;
}
   // the next six functions return != buf for range or allocation failure. end is _inclusive_, begin/end are 0-indexed
strbuf_t strbuf_replace_range(strbuf_t buf, size_t begin, size_t end, const char* str) {
    size_t end = end_index+1;
    return strbuf_replace_range_len(buf, begin, end, str, strlen(str));
}
strbuf_t strbuf_replace_range_len(strbuf_t buf, size_t begin, size_t end_index, const char* str, size_t len) {
    size_t end = end_index+1;
    if(begin >= end || len < end - begin) return STRBUF_ERROR;
    if(end > buf.cap) (void) strbuf_resize(buf, end);
    memmove(&buf.str[begin], str, end-begin);
    return buf;
}
strbuf_t strbuf_dup_slice(strbuf_t buf, size_t begin, size_t end_index) {
    strbuf_t bret; size_t end = end_index+1;
    if(begin > buf.cap || end > buf.cap || begin >= end) return STRBUF_ERROR;
    bret.cap = end-begin+1;
    bret.len = end-begin;
    bret.str = calloc(1, bret.cap);
    if(bret.str == NULL) return STRBUF_ALLOCATION_FAILURE;
    memmove(bret.str, &buf.str[begin], bret.len);
    return bret;
}
strbuf_t strbuf_split_by_char(strbuf_t buf, char delim, strbuf_t** bufs, size_t* nr_bufs) {
    return strbuf_split_by_str_len(buf, &delim, 1, bufs, nr_bufs);
}
strbuf_t strbuf_split_by_str(strbuf_t buf, char* delim, strbuf_t** bufs, size_t* nr_bufs) {
    return strbuf_split_by_str_len(buf, delim, strlen(delim), bufs, nr_bufs);
}
strbuf_t strbuf_split_by_str_len(strbuf_t buf, char* delim, size_t delim_len, strbuf_t** bufs, size_t* nr_bufs) {
    char initial; int curr, last; int nr, nr_copied; strbuf_t* bs;
    if(bufs == NULL || nr_bufs == NULL || delim_len > buf.len) return STRBUF_ERROR;
    
    // first round: find the number of delims in buf
    nr = 0;
    initial = delim[0]; curr = 0;
    while(curr < buf.len - delim_len+1) {
        if(buf.str[curr] == initial && strncmp(&buf.str[curr], delim, delim_len) == 0) {
            nr++; curr += delim_len;
        } else {
            curr++;
        }
    }
    
    // second round: copy out the strings seperated by delims
    bs = calloc(nr+1, sizeof(strbuf_t));
    last = 0; nr_copied = 0;
    while(curr < buf.len - delim_len+1) {
        if(buf.str[curr] == initial && strncmp(&buf.str[curr], delim, delim_len) == 0) {
            if(last == curr) bs[nr_copied] = strbuf_from_str(strid(""));
            else bs[nr_copied] = strbuf_dup_slice(buf, last, curr-1);
            nr_copied++; curr += delim_len; last = curr;
        } else {
            curr++;
        }
    }
    if(last == buf.len) bs[nr_copied] = strbuf_from_str(strid(""));
    else bs[nr_copied] = strbuf_dup_slice(buf, last, buf.len-1);
    nr_copied++;
    
    if(nr+1 != nr_copied) return STRBUF_ERROR;
    nr_bufs[0] = nr;
    bufs[0] = bs;
    return buf;
}

    // NOTE: this function treats buffers with the same content and lengths but different caps as the same
bool strbuf_equals(strbuf_t a, strbuf_t b) {
    if(a.len != b.len) return false;
    else return (strcmp(a.str,b.str) == 0);
}
int strbuf_cmp(strbuf_t a, strbuf_t b) {
    return strcmp(a.str,b.str);
}
int strbuf_find_subindex(strbuf_t outer, size_t offset, strid_t inner) {
    // basically the same code as strid_subindex
    int i, inner_len = inner[-1], outer_len = outer.len;
    char initial;
    if(inner_len > outer_len) return -1;
                // returns true-1 = 0 on equality, i.e. at index 0, and false-1 = -1 on inequality
    else if(inner_len == outer_len) return ((strcmp(outer.str, inner)==0)-1);
    
    initial = inner[0];
    for(i = 0; i < outer_len - inner_len + 1; i++) {
        if(outer.str[i] == initial && strncmp(&outer.str[i], inner, inner_len) == 0)
            return i;
    }
    return -1;
}
int strbuf_find_last_subindex(strbuf_t outer, size_t offset, strid_t inner) {
    // basically the same code as strid_subindex, but in reverse
    int i, inner_len = inner[-1], outer_len = outer.len;
    char final;
    if(inner_len > outer_len) return -1;
                // returns true-1 = 0 on equality, i.e. at index 0, and false-1 = -1 on inequality
    else if(inner_len == outer_len) return ((strcmp(outer.str, inner)==0)-1);
    
    final = inner[inner_len-1];
    for(i = outer_len-1; i >= inner_len - 1; i--) {
        if(outer.str[i] == final && strncmp(&outer.str[i-(inner_len-1)], inner, inner_len) == 0)
            return i;
    }
    return -1;
}

strbuf_t strbuf_concat(strbuf_t buf, const char* str) {
    return strbuf_concat_len(buf, str, strlen(str));
}
strbuf_t strbuf_concat_len(strbuf_t buf, const char* str, size_t len) {
    return strbuf_replace_range_len(buf, buf.len, buf.len+len, str, len);
}


static strbuf_t strbuf_fmt_uint(size_t bits, format_params_t* params, strbuf_int_format_t format) {
    strbuf_t fmt = strbuf_alloc(10), b;
    fmt = strbuf_concat(fmt, "%");
    if(format == STRBUF_INT_FORMAT_OCTAL_WITH_PREFIX_NONZERO ||
        format == STRBUF_INT_FORMAT_HEXADECIMAL_LOWER_CASE_WITH_PREFIX_NONZERO ||
        format == STRBUF_INT_FORMAT_HEXADECIMAL_UPPER_CASE_WITH_PREFIX_NONZERO) {
        fmt = strbuf_concat(fmt, "#");
    }
    if(params != NULL) {
        switch(params.sign_behavior) {
        case STRBUF_SIGN_BEHAVIOR_ONLY_NEGATIVE:
            break;
        case STRBUF_SIGN_BEHAVIOR_POSITIVE_AS_PLUS:
            fmt = strbuf_concat(fmt, "+");
            break;
        case STRBUF_SIGN_BEHAVIOR_POSITIVE_AS_SPACE  :
            fmt = strbuf_concat(fmt, " ");
            break;
        default:
            strbuf_free(fmt);
            return STRBUF_ERROR;
        }
        fmt = strbuf_concat(fmt, ".*");
    }
    switch(format) {
    case STRBUF_INT_FORMAT_DECIMAL:
        if(bits == 8) fmt = strbuf_concat(PRIu8);
        else if(bits == 16) fmt = strbuf_concat(PRIu16);
        else if(bits == 32) fmt = strbuf_concat(PRIu32);
        else if(bits == 64) fmt = strbuf_concat(PRIu63);
        else {
            strbuf_free(fmt);
            return STRBUF_ERROR;
        }
        break;
    case STRBUF_INT_FORMAT_OCTAL:
    case STRBUF_INT_FORMAT_OCTAL_WITH_PREFIX:
        if(bits == 8) fmt = strbuf_concat(PRIo8);
        else if(bits == 16) fmt = strbuf_concat(PRIo16);
        else if(bits == 32) fmt = strbuf_concat(PRIo32);
        else if(bits == 64) fmt = strbuf_concat(PRIo63);
        else {
            strbuf_free(fmt);
            return STRBUF_ERROR;
        }
        break;
    case STRBUF_INT_FORMAT_HEXADECIMAL_LOWER_CASE:
    case STRBUF_INT_FORMAT_HEXADECIMAL_LOWER_CASE_WITH_PREFIX:
        fmt = strbuf_concat(PRIx8);
        break;
    case STRBUF_INT_FORMAT_HEXADECIMAL_UPPER_CASE:
    case STRBUF_INT_FORMAT_HEXADECIMAL_UPPER_CASE_WITH_PREFIX:
        fmt = strbuf_concat(PRIX8);
        break;
    default:
        strbuf_free(fmt);
        return STRBUF_ERROR;
    }
    
}

strbuf_t strbuf_concat_int8(strbuf_t buf, int8_t val, format_params_t* params);
strbuf_t strbuf_concat_int16(strbuf_t buf, int16_t val, format_params_t* params);
strbuf_t strbuf_concat_int32(strbuf_t buf, int32_t val, format_params_t* params);
strbuf_t strbuf_concat_int64(strbuf_t buf, int64_t val, format_params_t* params);
strbuf_t strbuf_concat_uint8(strbuf_t buf, uint8_t val, format_params_t* params, strbuf_int_format_t format) {
}
strbuf_t strbuf_concat_uint16(strbuf_t buf, uint16_t val, format_params_t* params, strbuf_int_format_t format);
strbuf_t strbuf_concat_uint32(strbuf_t buf, uint32_t val, format_params_t* params, strbuf_int_format_t format);
strbuf_t strbuf_concat_uint64(strbuf_t buf, uint64_t val, format_params_t* params, strbuf_int_format_t format);
strbuf_t strbuf_concat_float32(strbuf_t buf, float32_t val, format_params_t* params, char decimal_point, strbuf_float_format_t format);
strbuf_t strbuf_concat_float64(strbuf_t buf, float64_t val, format_params_t* params, char decimal_point, strbuf_float_format_t format);
strbuf_t strbuf_concat_char(strbuf_t buf, char val);
strbuf_t strbuf_concat_ptr(strbuf_t buf, void* ptr);

// all of these return the length of what's parsed, typically to be +=-ed to the offset variable
// width = 0 is the default behavior, and will set it to ignore the width, with a number > 0 it will not consume more than those chars (and in the case of strbuf_read_char _exactly_ that many chars)
size_t strbuf_read_const(strbuf_t buf, size_t offset, size_t width, strid_t expected_const);
size_t strbuf_read_identifier(strbuf_t buf, size_t offset, size_t width, strid_t allowed_chars_first, strid_t allowed_chars, strid_t* out_id);
size_t strbuf_read_identifier_inverse(strbuf_t buf, size_t offset, size_t width, strid_t not_allowed_chars_first, strid_t not_allowed_chars, strid_t* out_id); // inverse bc it excludes instead
size_t strbuf_read_decimal_int_literal(strbuf_t buf, size_t offset, size_t width, int64_t* out);
size_t strbuf_read_octal_int_literal(strbuf_t buf, size_t offset, size_t width, int64_t* out, bool expect_prefix);
size_t strbuf_read_hexadecimal_int_literal(strbuf_t buf, size_t offset, size_t width, int64_t* out, bool expect_prefix);
size_t strbuf_read_decimal_uint_literal(strbuf_t buf, size_t offset, size_t width, uint64_t* out);
size_t strbuf_read_octal_uint_literal(strbuf_t buf, size_t offset, size_t width, uint64_t* out, bool expect_prefix);
size_t strbuf_read_hexadecimal_uint_literal(strbuf_t buf, size_t offset, size_t width, uint64_t* out, bool expect_prefix);
size_t strbuf_read_float_literal(strbuf_t buf, size_t offset, size_t width, float64_t* out, bool allow_exponent);
size_t strbuf_read_char(strbuf_t buf, size_t offset, size_t width, char* out);
size_t strbuf_read_ptr(strbuf_t buf, size_t offset, size_t width, void** out);








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








/* webcam library based on sr_webcam (https://github.com/kosua20/sr_webcam): */

webcam_t webcam_open(int id, int* width, int* height, int* framerate, webcam_callback_t callback) {
    sr_webcam_device* dev; int code;
    code = sr_webcam_create(&dev, id);
    if(code < 0) {
        webcam_delete((webcam_t)dev);
        return NULL;
    }
    sr_webcam_set_format(dev, width[0], height[0], framerate[0]);
    sr_webcam_set_callback(dev, callback);
    code = sr_webcam_open(dev);
    if(code < 0) {
        webcam_delete((webcam_t)dev);
        return NULL;
    }
    sr_webcam_get_dimensions(dev, width, height);
    sr_webcam_get_framerate(dev, framerate);
    return (webcam_t)dev;
}
void webcam_start(webcam_t cam) {
    sr_webcam_device* dev = (sr_webcam_device*) cam;
    sr_webcam_start(dev);
}
void swebcam_stop(webcam_t cam) {
    sr_webcam_device* dev = (sr_webcam_device*) cam;
    sr_webcam_stop(dev);
}
void webcam_close(webcam_t cam) {
    sr_webcam_device* dev = (sr_webcam_device*) cam;
    sr_webcam_delete(dev);
}
