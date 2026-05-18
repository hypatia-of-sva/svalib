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



/* memory library */

mem_t mem_alloc(size_t size) {
    if(size == 0) return (mem_t)NULL;
    return (mem_t) malloc(size);
}
mem_t mem_alloc_from_str(const char* str) {
    if(str == NULL) return (mem_t)NULL;
    return mem_alloc_from_str_len(str, strlen(str));
}
mem_t mem_alloc_from_str_len(const char* str, size_t len) {
    mem_t m = mem_alloc(len+1);
    if(m == NULL) return m;
    mem_copy(m, len+1, str, len, len);
    return m;
}
mem_t mem_alloc_align(size_t size, size_t align) {
    if(!is_power_of_2(align)) return (mem_t)NULL;
#ifdef WIN32
// we can also use _aligned_offset_malloc here - but there is no equivalent of that on unix side without writing a new allocator.
    return (mem_t) _aligned_alloc(size, align);
#else
    void* ptr;
    int code = posix_memalign(&ptr, align, size);
    if(code != 0) return (mem_t)NULL;
    else return (mem_t)ptr;
#endif
}
void mem_resize(mem_t* block, size_t old_size, size_t new_size) {
    if(block == NULL) return;
    if(old_size == 0) {
        block[0] = mem_alloc(new_size);
        return;
    }
    if(new_size == 0) {
        mem_free(block);
        return;
    }
    block[0] = realloc(block[0], new_size);
}
void mem_free(mem_t* block, size_t size) {
    if(block == NULL || size == 0) return;
    free(block[0]);
}
void mem_copy(mem_t dst, size_t dst_size, mem_t src, size_t src_size, size_t len) {
    if(dst == NULL || dst_size < len || src == NULL || src_size < len) return;
    memmove(dst, src, len);
}
void mem_zero(mem_t block, size_t len) {
    if(block == NULL || size == 0) return;
    memset(block, 0, len);
}
 // returns offset into block or (size_t)-1
size_t mem_find(mem_t block, size_t block_size, mem_t search_data, size_t search_data_size) {
    // basically the same code as strid_subindex or strbuf_find_subindex, but we can use memchr for the initial-search
    int curr; uint8_t initial;
    if(block == NULL || search_data == NULL || block_size == 0 || search_data_size == 0 || search_data_size > block_size) return (size_t)-1;
                // returns true-1 = 0 on equality, i.e. at index 0, and false-1 = -1 on inequality
    else if(block_size == search_data_size) return (size_t)((memcmp(block, search_data, block_size)==0)-1);
    
    initial = ((uint8_t*)search_data)[0];
    curr = 0;
    while(block_size >= search_data_size) {
        new_pos = memchr(&((uint8_t*)block)[curr], initial, block_size);
        if(new_pos == NULL) return -1;
        curr = ((uintptr_t) new_pos) - ((uintptr_t) block);
        if(memcmp(&(((uint8_t*)block)[curr]), search_data, search_data_size) == 0)
            return curr;
    }
    return -1;
}
size_t mem_find_last(mem_t block, size_t block_size, mem_t search_data, size_t search_data_size) {
    // basically the same code as strbuf_find_last_subindex; no memrchr for us to use here!
    int i; uint8_t final;
    if(block == NULL || search_data == NULL || search_data_size > block_size) return (size_t)-1;
                // returns true-1 = 0 on equality, i.e. at index 0, and false-1 = -1 on inequality
    else if(block_size == search_data_size) return (size_t)((memcmp(block, search_data, block_size)==0)-1);
    
    final = ((uint8_t*)search_data)[search_data_size-1];
    for(i = block_size-1; i >= search_data_size - 1; i--) {
        if(((uint8_t*)block)[i] == final && memcmp(&(((uint8_t*)block)[i-(search_data_size-1)]), search_data, search_data_size) == 0)
            return i;
    }
    return -1;
}
bool mem_equals(mem_t a, size_t a_size, mem_t b, size_t b_size) {
    if(a == NULL || b == NULL) return (a == NULL && b == NULL);
    if(s_size != b_size) return false;
    return (memcmp(a, b, a_size) == 0);
}
int mem_cmp(mem_t a, size_t a_size, mem_t b, size_t b_size) {
    if(a == NULL) return 1;
    if(b == NULL) return -1;
    return memcmp(a, b, size_min(a_size, b_size));
}







/* String library */

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
    if(outer == STRID_INVALID || inner == STRID_INVALID) return -1;
    return strid_subindex_str_len(outer, inner, inner[-1]);
}
int strid_subindex_str(strid_t outer, char* inner) {
    if(outer == STRID_INVALID || inner == NULL) return -1;
    return strid_subindex_str_len(outer, inner, strlen(inner));
}
int strid_subindex_str_len(strid_t outer, char* inner, size_t inner_len) {
    int i, outer_len;
    char initial;
    if(outer == STRID_INVALID || inner == NULL || inner_len > outer_len) return -1;
    outer_len = outer[-1];
                // returns true-1 = 0 on equality, i.e. at index 0, and false-1 = -1 on inequality
    else if(inner_len == outer_len) return ((strcmp(outer, inner)==0)-1);
    
    initial = inner[0];
    for(i = 0; i < outer_len - inner_len + 1; i++) {
        if(outer[i] == initial && strncmp(&outer[i], inner, inner_len) == 0)
            return i;
    }
    return -1;
}
int strid_subindex_char(strid_t id, char c) {
    int i, len = id[-1];
    if(id == STRID_INVALID || len == 0) return -1;
    
    for(i = 0; i < len; i++) {
        if(id[i] == c)
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








    // str needs to have been _allocated_ by the standard allocator (like strids), otherwise use strbuf_alloc+strbuf_concat
strbuf_result_t strbuf_from_str(strbuf_t* b, char* str) {
    strbuf_t ret;
    if(b == NULL || str == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    ret.str = str;
    ret.len = strlen(str);
    ret.cap = ret.len + 1;
    b[0] = ret;
    return STRBUF_OK;
}
strbuf_result_t strbuf_alloc(strbuf_t* b, size_t initial_cap) {
    strbuf_t ret;
    if(b == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    ret.cap = initial_cap+1;
    ret.len = 0;
    ret.str = calloc(1, initial_cap+1);
    if(ret.str == NULL) return STRBUF_ERROR_FAILED_ALLOCATION;
    b[0] = ret;
    return STRBUF_OK;
}
strbuf_result_t strbuf_reserve(strbuf_t* b, size_t additional_cap) {
    if(b == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    return strbuf_resize(b, b[0].cap+additional_cap);
}
strbuf_result_t strbuf_resize(strbuf_t* b, size_t new_min_cap) {
    char* newbuf;
    if(b == NULL || b[0].str == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    size_t newcap = size_min(new_min_cap, b[0].len+1);
    newbuf = realloc(b[0].str, newcap);
    if(newbuf == NULL) return STRBUF_ERROR_FAILED_ALLOCATION;
    b[0].str = newbuf;
    b[0].cap = newcap;
    return STRBUF_OK;
}
strbuf_result_t strbuf_dup(strbuf_t* copy, strbuf_t original) {
    strbuf_t bret;
    if(copy == NULL || original.str == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    bret.cap = original.cap;
    bret.len = original.len;
    bret.str = calloc(1, original.cap);
    if(bret.str == NULL) return STRBUF_ERROR_FAILED_ALLOCATION;
    memmove(bret.str, original.str, original.cap);
    copy[0] = bret;
    return STRBUF_OK;
}
strbuf_result_t strbuf_free(strbuf_t* b) {
    if(b == NULL || b[0].str == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    free(b[0].str);
    b[0].str = NULL;
    b[0].cap = 0;
    b[0].len = 0;
    return STRBUF_OK;
}
strbuf_t strbuf_zero(strbuf_t* b) {
    if(b == NULL || b[0].str == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    memset(b[0].str, 0, b[0].cap);
    b[0].len = 0;
    return STRBUF_OK;
}
   // end is _inclusive_, begin/end are 0-indexed
strbuf_result_t strbuf_replace_range(strbuf_t* buf, size_t begin, size_t end, const char* str) {
    return strbuf_replace_range_len(buf, begin, end, str, strlen(str));
}
strbuf_result_t strbuf_replace_range_len(strbuf_t* buf, size_t begin, size_t end_index, const char* str, size_t len) {
    strbuf_result_t r; size_t end = end_index+1;
    if(buf == NULL || buf[0].str == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    if(begin >= end || len < end - begin) return STRBUF_ERROR_PARAMETER_OUT_OF_RANGE;
    if(end > buf[0].cap) {
        r = strbuf_resize(buf, end);
        if(r != STRBUF_OK) return r;
    }
    memmove(&buf[0].str[begin], str, end-begin);
    return STRBUF_OK;
}
strbuf_result_t strbuf_dup_slice(strbuf_t* slice, strbuf_t buf, size_t begin, size_t end_index) {
    strbuf_t bret; size_t end = end_index+1;
    if(slice == NULL || buf.str == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    if(begin > buf.cap || end > buf.cap || begin >= end) return STRBUF_ERROR_PARAMETER_OUT_OF_RANGE;
    bret.cap = end-begin+1;
    bret.len = end-begin;
    bret.str = calloc(1, bret.cap);
    if(bret.str == NULL) return STRBUF_ERROR_FAILED_ALLOCATION;
    memmove(bret.str, &buf.str[begin], bret.len);
    slice[0] = bret;
    return STRBUF_OK;
}
strbuf_result_t strbuf_split_by_char(strbuf_t buf, char delim, strbuf_t** bufs, size_t* nr_bufs) {
    return strbuf_split_by_str_len(buf, &delim, 1, bufs, nr_bufs);
}
strbuf_result_t strbuf_split_by_str(strbuf_t buf, char* delim, strbuf_t** bufs, size_t* nr_bufs) {
    return strbuf_split_by_str_len(buf, delim, strlen(delim), bufs, nr_bufs);
}
strbuf_result_t strbuf_split_by_str_len(strbuf_t buf, char* delim, size_t delim_len, strbuf_t** bufs, size_t* nr_bufs) {
    char initial; int curr, last; int nr, nr_copied; strbuf_t* bs; strbuf_result_t r;
    if(buf.str == NULL || bufs == NULL || nr_bufs == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    if(delim_len > buf.len) return STRBUF_ERROR_PARAMETER_OUT_OF_RANGE;
    
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
            else {
                r = strbuf_dup_slice(&bs[nr_copied], buf, last, curr-1);
                if(r != STRBUF_OK) {
                    for(int i = 0; i < nr_copied; i++) {
                        strbuf_free(&bs[i]);
                    }
                    return r;
                }
            }
            nr_copied++; curr += delim_len; last = curr;
        } else {
            curr++;
        }
    }
    if(last == buf.len) bs[nr_copied] = strbuf_from_str(strid(""));
    else bs[nr_copied] = strbuf_dup_slice(buf, last, buf.len-1);
    nr_copied++;
    
    if(nr+1 != nr_copied) return STRBUF_ERROR_UNKNOWN;
    nr_bufs[0] = nr+1;
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
    int i, inner_len, outer_len = outer.len;
    char initial;
    if(inner == STRID_INVALID) return -1;
    inner_len = inner[-1];
    if(outer.str == NULL || inner_len > outer_len) return -1;
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
    int i, inner_len, outer_len = outer.len;
    char final;
    if(inner == STRID_INVALID) return -1;
    inner_len = inner[-1];
    if(outer.str == NULL || inner_len > outer_len) return -1;
                // returns true-1 = 0 on equality, i.e. at index 0, and false-1 = -1 on inequality
    else if(inner_len == outer_len) return ((strcmp(outer.str, inner)==0)-1);
    
    final = inner[inner_len-1];
    for(i = outer_len-1; i >= inner_len - 1; i--) {
        if(outer.str[i] == final && strncmp(&outer.str[i-(inner_len-1)], inner, inner_len) == 0)
            return i;
    }
    return -1;
}

strbuf_result_t strbuf_concat(strbuf_t* buf, const char* str) {
    return strbuf_concat_len(buf, str, strlen(str));
}
strbuf_result_t strbuf_concat_len(strbuf_t* buf, const char* str, size_t len) {
    if(buf == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    return strbuf_replace_range_len(buf, buf[0].len, buf[0].len+len, str, len);
}

static strbuf_result_t strbuf_fmtstr_int(strbuf_t* fmt, size_t bits, format_params_t* params) {
    strbuf_t ret; strbuf_result_t r; const char* s;
    
    if(fmt == NULL) return STRBUF_ERROR_UNKNOWN;
    r = strbuf_alloc(&ret, 10);
    if(r != STRBUF_OK) return r;
    r = strbuf_concat(&ret, "%");
    if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
    if(format == STRBUF_UINT_FORMAT_OCTAL_WITH_PREFIX_NONZERO ||
        format == STRBUF_UINT_FORMAT_HEXADECIMAL_LOWER_CASE_WITH_PREFIX_NONZERO ||
        format == STRBUF_UINT_FORMAT_HEXADECIMAL_UPPER_CASE_WITH_PREFIX_NONZERO) {
        r = strbuf_concat(&ret, "#");
        if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
    }
    if(params != NULL) {
        switch(params.sign_behavior) {
        case STRBUF_SIGN_BEHAVIOR_ONLY_NEGATIVE:
            break;
        case STRBUF_SIGN_BEHAVIOR_POSITIVE_AS_PLUS:
            r = strbuf_concat(&ret, "+");
            if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
            break;
        case STRBUF_SIGN_BEHAVIOR_POSITIVE_AS_SPACE  :
            r = strbuf_concat(&ret, " ");
            if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
            break;
        default:
            strbuf_free(&ret);
            return STRBUF_ERROR_PARAMETER_INVALID_ENUM;
        }
        r = strbuf_concat(&ret, ".*");
        if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
    }
    if(bits == 8) s = PRIi8;
    else if(bits == 16) s = PRIi16;
    else if(bits == 32) s = PRIi32;
    else if(bits == 64) s = PRIi63;
    else {
        strbuf_free(&ret);
        return STRBUF_ERROR_UNKNOWN;
    }
    r = strbuf_concat(&ret, s);
    if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
    
    fmt[0] = ret;
    return STRBUF_OK;
}
static strbuf_result_t strbuf_fmtstr_uint(strbuf_t* fmt, size_t bits, format_params_t* params, strbuf_uint_format_t format) {
    strbuf_t ret; strbuf_result_t r; const char* s;
    
    if(fmt == NULL) return STRBUF_ERROR_UNKNOWN;
    r = strbuf_alloc(&ret, 10);
    if(r != STRBUF_OK) return r;
    r = strbuf_concat(&ret, "%");
    if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
    if(format == STRBUF_UINT_FORMAT_OCTAL_WITH_PREFIX_NONZERO ||
        format == STRBUF_UINT_FORMAT_HEXADECIMAL_LOWER_CASE_WITH_PREFIX_NONZERO ||
        format == STRBUF_UINT_FORMAT_HEXADECIMAL_UPPER_CASE_WITH_PREFIX_NONZERO) {
        r = strbuf_concat(&ret, "#");
        if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
    }
    if(params != NULL) {
        switch(params.sign_behavior) {
        case STRBUF_SIGN_BEHAVIOR_ONLY_NEGATIVE:
            break;
        case STRBUF_SIGN_BEHAVIOR_POSITIVE_AS_PLUS:
            r = strbuf_concat(&ret, "+");
            if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
            break;
        case STRBUF_SIGN_BEHAVIOR_POSITIVE_AS_SPACE  :
            r = strbuf_concat(&ret, " ");
            if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
            break;
        default:
            strbuf_free(&ret);
            return STRBUF_ERROR_PARAMETER_INVALID_ENUM;
        }
        r = strbuf_concat(&ret, ".*");
        if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
    }
    switch(format) {
    case STRBUF_UINT_FORMAT_DECIMAL:
        if(bits == 8) s = PRIu8;
        else if(bits == 16) s = PRIu16;
        else if(bits == 32) s = PRIu32;
        else if(bits == 64) s = PRIu63;
        else {
            strbuf_free(&ret);
            return STRBUF_ERROR_UNKNOWN;
        }
        break;
    case STRBUF_UINT_FORMAT_OCTAL:
    case STRBUF_UINT_FORMAT_OCTAL_WITH_PREFIX:
        if(bits == 8) s = PRIo8;
        else if(bits == 16) s = PRIo16;
        else if(bits == 32) s = PRIo32;
        else if(bits == 64) s = PRIo63;
        else {
            strbuf_free(&ret);
            return STRBUF_ERROR_UNKNOWN;
        }
        break;
    case STRBUF_UINT_FORMAT_HEXADECIMAL_LOWER_CASE:
    case STRBUF_UINT_FORMAT_HEXADECIMAL_LOWER_CASE_WITH_PREFIX:
        if(bits == 8) s = PRIx8;
        else if(bits == 16) s = PRIx16;
        else if(bits == 32) s = PRIx32;
        else if(bits == 64) s = PRIx63;
        else {
            strbuf_free(&ret);
            return STRBUF_ERROR_UNKNOWN;
        }
        break;
    case STRBUF_UINT_FORMAT_HEXADECIMAL_UPPER_CASE:
    case STRBUF_UINT_FORMAT_HEXADECIMAL_UPPER_CASE_WITH_PREFIX:
        if(bits == 8) s = PRIX8;
        else if(bits == 16) s = PRIX16;
        else if(bits == 32) s = PRIX32;
        else if(bits == 64) s = PRIX63;
        else {
            strbuf_free(&ret);
            return STRBUF_ERROR_UNKNOWN;
        }
        break;
    default:
        strbuf_free(&ret);
        return STRBUF_ERROR_PARAMETER_INVALID_ENUM;
    }
    r = strbuf_concat(&ret, s);
    if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
    
    fmt[0] = ret;
    return STRBUF_OK;
}
static strbuf_result_t strbuf_fmtstr_float(strbuf_t* fmt, size_t bits, format_params_t* params, strbuf_float_format_t format) {
    strbuf_t ret; strbuf_result_t r; const char* s;
    
    if(fmt == NULL) return STRBUF_ERROR_UNKNOWN;
    r = strbuf_alloc(&ret, 10);
    if(r != STRBUF_OK) return r;
    r = strbuf_concat(&ret, "%");
    if(r != STRBUF_OK) { strbuf_free(&ret); return r; } 
    if(format == STRBUF_FLOAT_FORMAT_NO_EXPONENT_FORCE_DECIMAL_POINT ||
        format == STRBUF_FLOAT_FORMAT_SCIENTIFIC_FORCE_DECIMAL_POINT ||
        format == STRBUF_FLOAT_FORMAT_SCIENTIFIC_FORCE_DECIMAL_POINT_UPPER_CASE_E ||
        format == STRBUF_FLOAT_FORMAT_MIXED_FORCE_DECIMAL_POINT ||
        format == STRBUF_FLOAT_FORMAT_MIXED_FORCE_DECIMAL_POINT_UPPER_CASE_E) {
        r = strbuf_concat(&ret, "#");
        if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
    }
    if(params != NULL) {
        switch(params.sign_behavior) {
        case STRBUF_SIGN_BEHAVIOR_ONLY_NEGATIVE:
            break;
        case STRBUF_SIGN_BEHAVIOR_POSITIVE_AS_PLUS:
            r = strbuf_concat(&ret, "+");
            if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
            break;
        case STRBUF_SIGN_BEHAVIOR_POSITIVE_AS_SPACE  :
            r = strbuf_concat(&ret, " ");
            if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
            break;
        default:
            strbuf_free(&ret);
            return STRBUF_ERROR_PARAMETER_INVALID_ENUM;
        }
        r = strbuf_concat(&ret, ".*");
        if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
    }
    switch(format) {
    case STRBUF_FLOAT_FORMAT_NO_EXPONENT:
    case STRBUF_FLOAT_FORMAT_NO_EXPONENT_FORCE_DECIMAL_POINT:
        s = "f";
        break;
    case STRBUF_FLOAT_FORMAT_SCIENTIFIC:
    case STRBUF_FLOAT_FORMAT_SCIENTIFIC_FORCE_DECIMAL_POINT:
        s = "e";
        break;
    case STRBUF_FLOAT_FORMAT_SCIENTIFIC_UPPER_CASE_E:
    case STRBUF_FLOAT_FORMAT_SCIENTIFIC_FORCE_DECIMAL_POINT_UPPER_CASE_E:
        s = "E";
        break;
    case STRBUF_FLOAT_FORMAT_MIXED:
    case STRBUF_FLOAT_FORMAT_MIXED_FORCE_DECIMAL_POINT:
        s = "g";
        break;
    case STRBUF_FLOAT_FORMAT_MIXED_UPPER_CASE_E:
    case STRBUF_FLOAT_FORMAT_MIXED_FORCE_DECIMAL_POINT_UPPER_CASE_E:
        s = "G";
        break;
    default:
        strbuf_free(&ret);
        return STRBUF_ERROR_PARAMETER_INVALID_ENUM;
    }
    r = strbuf_concat(&ret, s);
    if(r != STRBUF_OK) { strbuf_free(&ret); return r; }
    
    fmt[0] = ret;
    return STRBUF_OK;
}

<?c
enum {T_INT, T_UINT, T_FLOAT} type_kind[10] = {T_INT, T_INT, T_INT, T_INT, T_UINT, T_UINT, T_UINT, T_UINT, T_FLOAT, T_FLOAT};
char** type_size[10] = {"8", "16", "32", "64", "8", "16", "32", "64", "32", "64"};
for(int i = 0; i < 10; i++) {
    char* tk, ts, params_end, fmststr_params_end;
    ts = type_size[i];
    switch(type_kind[i]) {
    case T_INT:
        tk = "int";
        params_end =  "";
        fmststr_params_end = "";
        break;
    case T_UINT:
        tk = "uint";
        params_end =  ", strbuf_uint_format_t format";
        fmststr_params_end = ", format";
        break;
    case T_FLOAT:
        tk = "float";
        params_end =  ", strbuf_float_format_t format";
        fmststr_params_end = ", format";
        break;
    }
?>
strbuf_result_t strbuf_concat_@tk@@ts@(strbuf_t* buf, @tk@@ts@_t val, format_params_t* params @fmststr_params_end@) {
    strbuf_t fmt; size_t len_format, len_padding, len_alloc, test_len; bool do_padding = false; strbuf_result_t r;
    if(buf == NULL || buf[0].str == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    
    r = strbuf_fmtstr_@tk@(&fmt, @ts@, params @fmststr_params_end@);
    if(r != STRBUF_OK) { (void) strbuf_free(&fmt); return r; }
    
    if(params != NULL) {
        len_format = snprintf(NULL, 0, fmt.str, params[0].precision, val);
        if(len_format < params[0].min_length) {
            len_alloc = params[0].min_length;
            len_padding = params[0].min_length - len_format;
            do_padding = true;
        } else {
            len_alloc = len_format;
        }
    } else {
        len_format = snprintf(NULL, 0, fmt.str, val);
        len_alloc = len_format;
    }
    
    // we reserve 1 byte more to allow for the NUL byte
    r = strbuf_reserve(buf, len_alloc+1);
    if(r != STRBUF_OK) { (void) strbuf_free(&fmt); return r; }
    
    if(params != NULL && do_padding && params[0].pad_type == STRBUF_PAD_TYPE_LEFT) {
        for(int i = 0; i < len_padding; i++) {
            buf[0].str[buf[0].len] = params[0].pad_char;
            buf[0].len++;
        }
    }
    
    if(params != NULL) {
        test_len = snprintf(&buf[0].str[buf[0].len], len_format, fmt.str, params.precision, val);
    } else {
        test_len = snprintf(&buf[0].str[buf[0].len], len_format, fmt.str, val);
    }
    if(test_len != len_format) { (void) strbuf_free(&fmt); return STRBUF_ERROR_UNKNOWN; }
    buf[0].len += test_len;
    
    if(params != NULL && do_padding && params[0].pad_type == STRBUF_PAD_TYPE_RIGHT) {
        for(int i = 0; i < len_padding; i++) {
            buf[0].str[buf[0].len] = params[0].pad_char;
            buf[0].len++;
        }
    }
    
    (void) strbuf_free(&fmt);
    
    return STRBUF_OK;
}
<?c
}
?>

strbuf_result_t strbuf_concat_char(strbuf_t* buf, char val) {
    return strbuf_concat_len(buf, &val, 1);
}
strbuf_result_t strbuf_concat_ptr(strbuf_t* buf, void* ptr) {
    size_t len_format, test_len; strbuf_result_t r;
    if(buf == NULL || buf[0].str == NULL) return STRBUF_ERROR_PARAMETER_NULL_POINTER;
    
    // we reserve 1 byte more to allow for the NUL byte
    len_format = snprintf(NULL, 0, "%p", val);
    r = strbuf_reserve(buf, len_format+1);
    if(r != STRBUF_OK) { return r; }
    
    test_len = snprintf(&buf[0].str[buf[0].len], len_format, "%p", val);
    if(test_len != len_format) { return STRBUF_ERROR_UNKNOWN; }
    buf[0].len += test_len;
    
    return STRBUF_OK;
}

// all of these return the length of what's parsed, typically to be +=-ed to the offset variable
// width = 0 is the default behavior, and will set it to ignore the width, with a number > 0 it will not consume more than those chars
//  (and in the case of strbuf_read_char _exactly_ that many chars)
// None of these functions change buf, they return 0 if nothing was to parse / if offset was out of range, and will then not write anything to the out pointers.
size_t strbuf_read_whitespace(strbuf_t buf, size_t offset, size_t width) {
    int len = 0;
    if(buf.str == NULL || offset >= buf.cap) return 0;
    while((len < width || width == 0) && offset+len < buf.cap && isspace(buf.str[offset+len])) {
        len++;
    }
    return len;
}
size_t strbuf_read_const(strbuf_t buf, size_t offset, size_t width, strid_t expected_const) {
    if(expected_const == STRID_INVALID) return 0;
    size_t len = strid_len(expected_const);
    if(buf.str == NULL || offset + len >= buf.cap) return 0;
    for(int i = 0; i < len; i++) {
        if(buf.str[offset+i] != expected_const[i])
            return 0;
    }
    return len;
}
size_t strbuf_read_identifier(strbuf_t buf, size_t offset, size_t width, strid_t allowed_chars_first, strid_t allowed_chars, strid_t* out_id) {
    int len = 0;
    if(buf.str == NULL || offset >= buf.cap || allowed_chars_first == STRID_INVALID || allowed_chars == STRID_INVALID || out_id == NULL) return 0;
    for(int i = offset; i < buf.cap; i++) {
        if(strid_subindex_char(allowed_chars_first, buf.str[i]) >= 0) {
            len = 1;
            for(int j = 0; i+j<buf.cap && j < width; j++) {
                if(strid_subindex_char(allowed_chars, buf.str[i]) < 0) {
                    out_id[0] = strid_from_len(&buf.str[i-len+1], len);
                    return len;
                }
                len++;
            }
            out_id[0] = strid_from_len(&buf.str[i-len+1], len);
            return len;
        }
    }
    return 0;
}
    // inverse bc it excludes instead
size_t strbuf_read_identifier_inverse(strbuf_t buf, size_t offset, size_t width, strid_t not_allowed_chars_first, strid_t not_allowed_chars, strid_t* out_id) {
    int len = 0;
    if(buf.str == NULL || offset >= buf.cap || not_allowed_chars_first == STRID_INVALID || not_allowed_chars == STRID_INVALID) return 0;
    for(int i = offset; i < buf.cap; i++) {
        if(strid_subindex_char(not_allowed_chars_first, buf.str[i]) < 0) {
            len = 1;
            for(int j = 0; i+j<buf.cap && j < width; j++) {
                if(strid_subindex_char(not_allowed_chars, buf.str[i]) >= 0) {
                    out_id[0] = strid_from_len(&buf.str[i-len+1], len);
                    return len;
                }
                len++;
            }
            out_id[0] = strid_from_len(&buf.str[i-len+1], len);
            return len;
        }
    }
    return 0;
}

static size_t strbuf_read_generic_nr_literal(strbuf_t buf, size_t offset, size_t width, void* out, char* modifier) {
    int len, nr; strbuf_t fmt; strbuf_result_t r;
    if(buf.str == NULL || offset >= buf.cap || out == NULL) return 0;
    r = strbuf_alloc(&fmt, 20);
    if(r != STRBUF_OK) return 0;
    r = strbuf_concat(&fmt, "%");
    if(r != STRBUF_OK) { (void) strbuf_free(&fmt); return r; }
    if(width != 0) {
        r = strbuf_concat_int32(&fmt, width), NULL;
        if(r != STRBUF_OK) { (void) strbuf_free(&fmt); return r; }
    }
    r = strbuf_concat(&fmt, modifier);
    if(r != STRBUF_OK) { (void) strbuf_free(&fmt); return r; }
    
    nr = sscanf(&buf.str[offset], fmt, &out, &len);
    
    if(nr != 1) return 0;
    else return len;
}
size_t strbuf_read_decimal_int_literal(strbuf_t buf, size_t offset, size_t width, int64_t* out) {
    return strbuf_read_generic_nr_literal(buf, oddset, width, out, SCNi64);
}
size_t strbuf_read_decimal_uint_literal(strbuf_t buf, size_t offset, size_t width, uint64_t* out) {
    return strbuf_read_generic_nr_literal(buf, oddset, width, out, SCNu64);
}
size_t strbuf_read_octal_uint_literal(strbuf_t buf, size_t offset, size_t width, uint64_t* out) {
    return strbuf_read_generic_nr_literal(buf, oddset, width, out, SCNo64);
}
size_t strbuf_read_hexadecimal_uint_literal(strbuf_t buf, size_t offset, size_t width, uint64_t* out) {
    return strbuf_read_generic_nr_literal(buf, oddset, width, out, SCNx64);
}
size_t strbuf_read_float_literal(strbuf_t buf, size_t offset, size_t width, float64_t* out) {
    return strbuf_read_generic_nr_literal(buf, oddset, width, out, SCNf64);
}
size_t strbuf_read_char(strbuf_t buf, size_t offset, size_t width, char* out) {
    return strbuf_read_generic_nr_literal(buf, oddset, width, out, "c");
}
size_t strbuf_read_ptr(strbuf_t buf, size_t offset, size_t width, void** out) {
    return strbuf_read_generic_nr_literal(buf, oddset, width, out, "p");
}
















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
