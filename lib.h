/* we keep the math and threading libraries from libc since they're pretty good ? */
#include <threads.h>
#include <math.h>
/* also these are used everywhere (and really should be language primitives): */
#include <stdbool.h>
#include <stdint.h>





/* fundamental things */

#define NULL ((void*)0)

void unreachable(void);






/* int types: */

typedef signed char         int8_t;
typedef unsigned char       uint8_t;
typedef signed short        int16_t;
typedef unsigned short      uint16_t;
typedef signed int          int32_t;
typedef unsigned int        uint32_t;
typedef signed long long    int64_t;
typedef unsigned long long  uint64_t;
typedef signed __int128     int128_t;
typedef unsigned __int128   uint128_t;
typedef int128_t            intmax_t;
typedef uint128_t           uintmax_t;

#define INT8_MIN            -(1<<7)
#define INT8_MAX            (1<<7)-1
#define UINT8_MAX           (1<<8)-1
#define INT16_MIN           -(1<<15)
#define INT16_MAX           (1<<15)-1
#define UINT16_MAX          (1<<16)-1
#define INT32_MIN           -(1<<31)
#define INT32_MAX           (1<<31)-1
#define UINT32_MAX          (1<<32)-1
#define INT64_MIN           -(1<<63)
#define INT64_MAX           (1<<63)-1
#define UINT64_MAX          (1<<64)-1


typedef _m512i max_align_t;

#if (sizeof(void*) == 4)
typedef int32_t ptrdiff_t, intptr_t;
typedef uint32_t size_t, uintptr_t;
#elif (sizeof(void*) == 8)
typedef int64_t ptrdiff_t, intptr_t;
typedef uint64_t size_t, uintptr_t;
#else
#error only 32 or 64 bit pointer system supported
#endif


typedef enum float_exceptions_flagbit_t {
    INT_EXCEPTIONS_DIVIDE_BY_ZERO_BIT = 0x01,
    INT_EXCEPTIONS_OVERFLOW_BIT = 0x02,
    INT_EXCEPTIONS_UNDERFLOW_BIT = 0x04,
    INT_EXCEPTIONS_LAST = INT_EXCEPTIONS_UNDERFLOW_BIT
} float_exceptions_flagbit_t;
#define INT_EXCEPTIONS_ALL 0x07
typedef uint8_t int_exceptions_mask_t
int_exceptions_mask_t int_exceptions_get(void);
void int_exceptions_clear(void);





intmax ?
(u)intN_t, (u)int_leastN_t


typedef struct intmax_div_t {
    intmax_t quotient, remainder;
} intmax_div_t;

intmax_t intmax_abs(intmax_t j);
intmax_div_t intmax_div(intmax_t numer, intmax_t denom);



/* TODO limits.h stuff */


typedef enum int_endianness_t {
    INT_ENDIANNESS_LITTLE_ENDIAN = 0,
    INT_ENDIANNESS_BIG_ENDIAN = 1,
    INT_ENDIANNESS_MAX_ENUM = 0x7f
} int_endianness_t;

int_endianness_t int_native_endianness(void);



uint32_t uint8_leading_zeros(uint8_t value);
uint32_t uint16_leading_zeros(uint16_t value);
uint32_t uint32_leading_zeros(uint32_t value);
uint32_t uint64_leading_zeros(uint64_t value);
uint32_t uint8_leading_ones(uint8_t value);
uint32_t uint16_leading_ones(uint16_t value);
uint32_t uint32_leading_ones(uint32_t value);
uint32_t uint64_leading_ones(uint64_t value);
uint32_t uint8_trailing_zeros(uint8_t value);
uint32_t uint16_trailing_zeros(uint16_t value);
uint32_t uint32_trailing_zeros(uint32_t value);
uint32_t uint64_trailing_zeros(uint64_t value);
uint32_t uint8_trailing_ones(uint8_t value);
uint32_t uint16_trailing_ones(uint16_t value);
uint32_t uint32_trailing_ones(uint32_t value);
uint32_t uint64_trailing_ones(uint64_t value);
uint32_t uint8_first_leading_zero(uint8_t value);
uint32_t uint16_first_leading_zero(uint16_t value);
uint32_t uint32_first_leading_zero(uint32_t value);
uint32_t uint64_first_leading_zero(uint64_t value);
uint32_t uint8_first_leading_one(uint8_t value);
uint32_t uint16_first_leading_one(uint16_t value);
uint32_t uint32_first_leading_one(uint32_t value);
uint32_t uint64_first_leading_one(uint64_t value);
uint32_t uint8_first_trailing_zero(uint8_t value);
uint32_t uint16_first_trailing_zero(uint16_t value);
uint32_t uint32_first_trailing_zero(uint32_t value);
uint32_t uint64_first_trailing_zero(uint64_t value);
uint32_t uint8_first_trailing_one(uint8_t value);
uint32_t uint16_first_trailing_one(uint16_t value);
uint32_t uint32_first_trailing_one(uint32_t value);
uint32_t uint64_first_trailing_one(uint64_t value);
uint32_t uint8_count_zeros(uint8_t value);
uint32_t uint16_count_zeros(uint16_t value);
uint32_t uint32_count_zeros(uint32_t value);
uint32_t uint64_count_zeros(uint64_t value);
uint32_t uint8_count_ones(uint8_t value);
uint32_t uint16_count_ones(uint16_t value);
uint32_t uint32_count_ones(uint32_t value);
uint32_t uint64_count_ones(uint64_t value);
bool32_t uint8_has_single_bit(uint8_t value);
bool32_t uint16_has_single_bit(uint16_t value);
bool32_t uint32_has_single_bit(uint32_t value);
bool32_t uint64_has_single_bit(uint64_t value);
uint32_t uint8_bit_width(uint8_t value);
uint32_t uint16_bit_width(uint16_t value);
uint32_t uint32_bit_width(uint32_t value);
uint32_t uint64_bit_width(uint64_t value);
uint32_t uint8_bit_floor(uint8_t value);
uint32_t uint16_bit_floor(uint16_t value);
uint32_t uint32_bit_floor(uint32_t value);
uint32_t uint64_bit_floor(uint64_t value);
uint32_t uint8_bit_ceil(uint8_t value);
uint32_t uint16_bit_ceil(uint16_t value);
uint32_t uint32_bit_ceil(uint32_t value);
uint32_t uint64_bit_ceil(uint64_t value);





/** checked arithmetic: */
/* these functions only take 128 bit operands, since all other types can be implicitly converted without loss of any precision. */
bool int8_checked_conversion(int8_t* result, intmax_t a);
bool int16_checked_conversion(int16_t* result, intmax_t a);
bool int32_checked_conversion(int32_t* result, intmax_t a);
bool int64_checked_conversion(int64_t* result, intmax_t a);
bool uint8_checked_conversion(uint8_t* result, intmax_t a);
bool uint16_checked_conversion(uint16_t* result, intmax_t a);
bool uint32_checked_conversion(uint32_t* result, intmax_t a);
bool uint64_checked_conversion(uint64_t* result, intmax_t a);

bool int8_checked_addition(int8_t* result, intmax_t a, intmax_t b);
bool int16_checked_addition(int16_t* result, intmax_t a, intmax_t b);
bool int32_checked_addition(int32_t* result, intmax_t a, intmax_t b);
bool int64_checked_addition(int64_t* result, intmax_t a, intmax_t b);
bool uint8_checked_addition(uint8_t* result, intmax_t a, intmax_t b);
bool uint16_checked_addition(uint16_t* result, intmax_t a, intmax_t b);
bool uint32_checked_addition(uint32_t* result, intmax_t a, intmax_t b);
bool uint64_checked_addition(uint64_t* result, intmax_t a, intmax_t b);

bool int8_checked_subtraction(int8_t* result, intmax_t a, intmax_t b);
bool int16_checked_subtraction(int16_t* result, intmax_t a, intmax_t b);
bool int32_checked_subtraction(int32_t* result, intmax_t a, intmax_t b);
bool int64_checked_subtraction(int64_t* result, intmax_t a, intmax_t b);
bool uint8_checked_subtraction(uint8_t* result, intmax_t a, intmax_t b);
bool uint16_checked_subtraction(uint16_t* result, intmax_t a, intmax_t b);
bool uint32_checked_subtraction(uint32_t* result, intmax_t a, intmax_t b);
bool uint64_checked_subtraction(uint64_t* result, intmax_t a, intmax_t b);

bool int8_checked_multiplication(int8_t* result, intmax_t a, intmax_t b);
bool int16_checked_multiplication(int16_t* result, intmax_t a, intmax_t b);
bool int32_checked_multiplication(int32_t* result, intmax_t a, intmax_t b);
bool int64_checked_multiplication(int64_t* result, intmax_t a, intmax_t b);
bool uint8_checked_multiplication(uint8_t* result, intmax_t a, intmax_t b);
bool uint16_checked_multiplication(uint16_t* result, intmax_t a, intmax_t b);
bool uint32_checked_multiplication(uint32_t* result, intmax_t a, intmax_t b);
bool uint64_checked_multiplication(uint64_t* result, intmax_t a, intmax_t b);








/* TODO stdatomic.h */







/* bool types: */

#define true 1
#define false 0

typedef int8_t bool8_t;
typedef int16_t bool16_t;
typedef int32_t bool32_t;
typedef int64_t bool64_t;




/* float types and environment: */

typedef enum float_exceptions_flagbit_t {
    FLOAT_EXCEPTIONS_DIVIDE_BY_ZERO_BIT = 0x01,
    FLOAT_EXCEPTIONS_INEXACT_BIT = 0x02,
    FLOAT_EXCEPTIONS_INVALID_BIT = 0x04,
    FLOAT_EXCEPTIONS_OVERFLOW_BIT = 0x08,
    FLOAT_EXCEPTIONS_UNDERFLOW_BIT = 0x10,
    FLOAT_EXCEPTIONS_LAST = FLOAT_EXCEPTIONS_UNDERFLOW_BIT
} float_exceptions_flagbit_t;
#define FLOAT_EXCEPTIONS_ALL 0x1f
typedef uint8_t float_exceptions_mask_t
typedef enum float_rounding_direction_t {
    FLOAT_ROUNDING_DIRECTION_DOWNWARD = 0,
    FLOAT_ROUNDING_DIRECTION_TO_NEAREST = 1,
    FLOAT_ROUNDING_DIRECTION_TO_NEAREST_FROM_ZERO = 2,
    FLOAT_ROUNDING_DIRECTION_TOWARD_ZERO = 3,
    FLOAT_ROUNDING_DIRECTION_UPWARD = 4,
        /* only usable in #pragma */
    FLOAT_ROUNDING_DIRECTION_DYNAMIC = 5,
    FLOAT_ROUNDING_DIRECTION_MAX_ENUM = 0x7f
} float_rounding_direction_t;
typedef enum float_number_type_t {
    FLOAT_NUMBER_TYPE_INFINITE = 0,
    FLOAT_NUMBER_TYPE_SIGNALING_NAN = 1,
    FLOAT_NUMBER_TYPE_QUIET_NAN = 2,
    FLOAT_NUMBER_TYPE_NORMAL = 3,
    FLOAT_NUMBER_TYPE_SUBNORMAL = 4,
    FLOAT_NUMBER_TYPE_ZERO = 5,
    FLOAT_NUMBER_TYPE_MAX_ENUM = 0x7f
} float_number_type_t;
typedef enum float_pair_ordering_t {
    FLOAT_PARI_ORDERING_EQUAL_TO = 0,
    FLOAT_PARI_ORDERING_LESS_THAN = 1,
    FLOAT_PARI_ORDERING_GREATER_THAN = 2,
    FLOAT_PARI_ORDERING_UNORDERED_PAIR = 3,
    FLOAT_PARI_ORDERING_MAX_ENUM = 0x7f
} float_pair_ordering_t;


float_exceptions_mask_t float_exceptions_get(void);
void float_exceptions_clear(void);
float_rounding_direction_t float_rounding_direction_get(void);
void float_rounding_direction_set(float_rounding_direction_t m);



/* TODO float.h stuff */



typedef float  float32_t;
typedef double float64_t;

#define FLOAT32_INFINITY  _FLOAT32_POS_INFINITY
#define FLOAT64_INFINITY  _FLOAT64_POS_INFINITY
#define FLOAT32_NAN       _FLOAT32_QUIET_NAN_PAYLOAD_0
#define FLOAT64_NAN       _FLOAT64_QUIET_NAN_PAYLOAD_0
#define FLOAT32_E         ...
#define FLOAT64_E         ...
#define FLOAT32_PI        ...
#define FLOAT64_PI        ...


/* needs to be rewritten */
    #define FLOAT_CONSTANT_ILOGB0 INT_MIN or -INT_MAX
    #define FLOAT_CONSTANT_ILOGBNAN INT_MIN or INT_MAX
    #define FLOAT_CONSTANT_LLOGB0 LONG_MIN or -LONG_MAX
    #define FLOAT_CONSTANT_LLOGBNAN LONG_MIN or LONG_MAX



float_number_type_t float32_classify(float32_t x);
float_number_type_t float64_classify(float64_t x);
bool32_t float32_is_nan(float32_t x);
bool32_t float64_is_nan(float64_t x);

bool32_t float32_sign_bit(float32_t x);
bool32_t float64_sign_bit(float64_t x);

float32_t float32_radians_from_degrees(float32_t x);
float64_t float64_radians_from_degrees(float64_t x);
float32_t float32_degrees_from_radians(float32_t x);
float64_t float64_degrees_from_radians(float64_t x);
float32_t float32_sign(float32_t x);
float64_t float64_sign(float64_t x);
float32_t float32_acos(float32_t x);
float64_t float64_acos(float64_t x);
float32_t float32_asin(float32_t x);
float64_t float64_asin(float64_t x);
float32_t float32_atan(float32_t x);
float64_t float64_atan(float64_t x);
float32_t float32_cos(float32_t x);
float64_t float64_cos(float64_t x);
float32_t float32_sin(float32_t x);
float64_t float64_sin(float64_t x);
float32_t float32_tan(float32_t x);
float64_t float64_tan(float64_t x);
float32_t float32_acospi(float32_t x);
float64_t float64_acospi(float64_t x);
float32_t float32_asinpi(float32_t x);
float64_t float64_asinpi(float64_t x);
float32_t float32_atanpi(float32_t x);
float64_t float64_atanpi(float64_t x);
float32_t float32_cospi(float32_t x);
float64_t float64_cospi(float64_t x);
float32_t float32_sinpi(float32_t x);
float64_t float64_sinpi(float64_t x);
float32_t float32_tanpi(float32_t x);
float64_t float64_tanpi(float64_t x);
float32_t float32_acosh(float32_t x);
float64_t float64_acosh(float64_t x);
float32_t float32_asinh(float32_t x);
float64_t float64_asinh(float64_t x);
float32_t float32_atanh(float32_t x);
float64_t float64_atanh(float64_t x);
float32_t float32_cosh(float32_t x);
float64_t float64_cosh(float64_t x);
float32_t float32_sinh(float32_t x);
float64_t float64_sinh(float64_t x);
float32_t float32_tanh(float32_t x);
float64_t float64_tanh(float64_t x);
float32_t float32_exp(float32_t x);
float64_t float64_exp(float64_t x);
float32_t float32_exp2(float32_t x);
float64_t float64_exp2(float64_t x);
float32_t float32_exp10(float32_t x);
float64_t float64_exp10(float64_t x);
float32_t float32_expm1(float32_t x);
float64_t float64_expm1(float64_t x);
float32_t float32_exp2m1(float32_t x);
float64_t float64_exp2m1(float64_t x);
float32_t float32_exp10m1(float32_t x);
float64_t float64_exp10m1(float64_t x);
float32_t float32_log(float32_t x);
float64_t float64_log(float64_t x);
float32_t float32_log2(float32_t x);
float64_t float64_log2(float64_t x);
float32_t float32_log10(float32_t x);
float64_t float64_log10(float64_t x);
float32_t float32_logp1(float32_t x);
float64_t float64_logp1(float64_t x);
float32_t float32_log2p1(float32_t x);
float64_t float64_log2p1(float64_t x);
float32_t float32_log10p1(float32_t x);
float64_t float64_log10p1(float64_t x);
float32_t float32_cbrt(float32_t x);
float64_t float64_cbrt(float64_t x);
float32_t float32_abs(float32_t x);
float64_t float64_abs(float64_t x);
float32_t float32_rsqrt(float32_t x);
float64_t float64_rsqrt(float64_t x);
float32_t float32_sqrt(float32_t x);
float64_t float64_sqrt(float64_t x);
float32_t float32_erf(float32_t x);
float64_t float64_erf(float64_t x);
float32_t float32_erfc(float32_t x);
float64_t float64_erfc(float64_t x);
float32_t float32_lgamma(float32_t x);
float64_t float64_lgamma(float64_t x);
float32_t float32_tgamma(float32_t x);
float64_t float64_tgamma(float64_t x);

float32_t float32_atan2(float32_t y, float32_t x);
float64_t float64_atan2(float64_t y, float64_t x);
float32_t float32_atan2pi(float32_t y, float32_t x);
float64_t float64_atan2pi(float64_t y, float64_t x);
float32_t float32_hypot(float32_t x, float32_t y);
float64_t float64_hypot(float64_t x, float64_t y);
float32_t float32_pow(float32_t x, float32_t y);
float64_t float64_pow(float64_t x, float64_t y);
float32_t float32_powr(float32_t y, float32_t x);
float64_t float64_powr(float64_t y, float64_t x);
float32_t  float32_fdim(float32_t x, float32_t y);
float64_t  float64_fdim(float64_t x, float64_t y);
float32_t  float32_fma(float32_t x, float32_t y, float32_t z);
float64_t  float64_fma(float64_t x, float64_t y, float64_t z);

float32_t float32_multiply_with_i32pow2(float32_t x, int32_t p);
float64_t float64_multiply_with_i32pow2(float64_t x, int32_t p);
float32_t float32_multiply_with_i64pow2(float32_t x, int64_t p);
float64_t float64_multiply_with_i64pow2(float64_t x, int64_t p);
float32_t float32_1_plus_x_pown64(float32_t x, int64_t n);
float64_t float64_1_plus_x_pown64(float64_t x, int64_t n);
float32_t float32_pown64(float32_t x, int64_t n);
float64_t float64_pown64(float64_t x, int64_t n);
float32_t float32_rootn64(float32_t x, int64_t n);
float64_t float64_rootn64(float64_t x, int64_t n);

float32_t float32_split_into_mantissa_fraction_and_i32log2(float32_t x, int32_t* p_log2);
float64_t float64_split_into_mantissa_fraction_and_i32log2(float64_t x, int32_t* p_log2);
float32_t float32_split_into_mantissa_fraction_and_i64log2(float32_t x, int64_t* p_log2);
float64_t float64_split_into_mantissa_fraction_and_i64log2(float64_t x, int64_t* p_log2);
float32_t float32_split_into_fraction_and_floor(float32_t x, float32_t* p_floor);
float64_t float64_split_into_fraction_and_floor(float64_t x, float64_t* p_floor);
int32_t float32_i32log2(float32_t x);
int32_t float64_i32log2(float64_t x);
int64_t float32_i64log2(float32_t x);
int64_t float64_i64log2(float64_t x);

float32_t float32_ceil(float32_t x);
float64_t float64_ceil(float64_t x);
float32_t float32_floor(float32_t x);
float64_t float64_floor(float64_t x);
float32_t float32_fract_from_floor(float32_t x);
float64_t float64_fract_from_floor(float64_t x);
float32_t float32_trunc(float32_t x);
float64_t float64_trunc(float64_t x);
float32_t float32_round_default(float32_t x);
float64_t float64_round_default(float64_t x);
float32_t float32_round_default_raise_inexact(float32_t x);
float64_t float64_round_default_raise_inexact(float64_t x);
float32_t float32_round_with_half_away_from_zero(float32_t x);
float64_t float64_round_with_half_away_from_zero(float64_t x);
float32_t float32_round_with_half_to_even(float32_t x);
float64_t float64_round_with_half_to_even(float64_t x);

float32_t float32_round_with_direction_and_test_bitwidth(float32_t x, float_rounding_direction_t direction, uint32_t bitwidth);
float64_t float64_round_with_direction_and_test_bitwidth(float64_t x, float_rounding_direction_t direction, uint32_t bitwidth);
float32_t float32_round_with_direction_and_test_u_bitwidth(float32_t x, float_rounding_direction_t direction, uint32_t bitwidth);
float64_t float64_round_with_direction_and_test_u_bitwidth(float64_t x, float_rounding_direction_t direction, uint32_t bitwidth);
float32_t float32_round_with_direction_and_test_bitwidth_raise_inexact(float32_t x, float_rounding_direction_t direction, uint32_t bitwidth);
float64_t float64_round_with_direction_and_test_bitwidth_raise_inexact(float64_t x, float_rounding_direction_t direction, uint32_t bitwidth);
float32_t float32_round_with_direction_and_test_u_bitwidth_raise_inexact(float32_t x, float_rounding_direction_t direction, uint32_t bitwidth);
float64_t float64_round_with_direction_and_test_u_bitwidth_raise_inexact(float64_t x, float_rounding_direction_t direction, uint32_t bitwidth);

int32_t float32_i32round_default(float32_t x);
int32_t float64_i32round_default(float64_t x);
int64_t float32_i64round_default(float32_t x);
int64_t float64_i64round_default(float64_t x);
int32_t float32_i32round_with_half_away_from_zero(float32_t x);
int32_t float64_i32round_with_half_away_from_zero(float64_t x);
int64_t float32_i64round_with_half_away_from_zero(float32_t x);
int64_t float64_i64round_with_half_away_from_zero(float64_t x);

float32_t  float32_mod(float32_t x, float32_t y);
float64_t  float64_mod(float64_t x, float64_t y);
float32_t  float32_IEE_remainder(float32_t x, float32_t y);
float64_t  float64_IEE_remainder(float64_t x, float64_t y);
float32_t  float32_IEE_remainder_and_quotient_i32(float32_t x, float32_t y, int32_t* quo);
float64_t  float64_IEE_remainder_and_quotient_i32(float64_t x, float64_t y, int32_t* quo);


float32_t  float32_copysign_over_value(float32_t valueparam, float32_t signparam);
float64_t  float64_copysign_over_value(float64_t valueparam, float64_t signparam);

float32_t  float32_qnan(const char* tagp);
float64_t  float64_qnan(const char* tagp);
    /* not in libc */
float32_t  float32_snan(const char* tagp);
float64_t  float64_snan(const char* tagp);


float32_t  float32_next_in_direction_of(float32_t x, float32_t direction);
float64_t  float64_next_in_direction_of(float64_t x, float64_t direction);
float32_t  float32_next_greater(float32_t x);
float64_t  float64_next_greater(float64_t x);
float32_t  float32_next_lesser(float32_t x);
float64_t  float64_next_lesser(float64_t x);


float32_t  float32_max(float32_t x, float32_t y);
float64_t  float64_max(float64_t x, float64_t y);
float32_t  float32_min(float32_t x, float32_t y);
float64_t  float64_min(float64_t x, float64_t y);
float32_t  float32_max_abs(float32_t x, float32_t y);
float64_t  float64_max_abs(float64_t x, float64_t y);
float32_t  float32_min_abs(float32_t x, float32_t y);
float64_t  float64_min_abs(float64_t x, float64_t y);



float32_t  float64_to_f32_add(float64_t x, float64_t y);
float32_t  float64_to_f32_sub(float64_t x, float64_t y);
float32_t  float64_to_f32_mul(float64_t x, float64_t y);
float32_t  float64_to_f32_div(float64_t x, float64_t y);
float32_t  float64_to_f32_fma(float64_t x, float64_t y);
float32_t  float64_to_f32_sqrt(float64_t x, float64_t y);






float_pair_ordering_t float32_qnansafe_compare_to_float32(float32_t x, float32_t y);
float_pair_ordering_t float32_qnansafe_compare_to_float64(float32_t x, float64_t y);
float_pair_ordering_t float64_qnansafe_compare_to_float32(float64_t x, float32_t y);
float_pair_ordering_t float64_qnansafe_compare_to_float64(float64_t x, float64_t y);


bool32_t float32_qnansafe_is_greater_than_float32(float32_t x, float32_t y);
bool32_t float32_qnansafe_is_greater_than_float64(float32_t x, float64_t y);
bool32_t float64_qnansafe_is_greater_than_float32(float64_t x, float32_t y);
bool32_t float64_qnansafe_is_greater_than_float64(float64_t x, float64_t y);

bool32_t float32_qnansafe_is_greater_than_or_equal_to_float32(float32_t x, float32_t y);
bool32_t float32_qnansafe_is_greater_than_or_equal_to_float64(float32_t x, float64_t y);
bool32_t float64_qnansafe_is_greater_than_or_equal_to_float32(float64_t x, float32_t y);
bool32_t float64_qnansafe_is_greater_than_or_equal_to_float64(float64_t x, float64_t y);

bool32_t float32_qnansafe_is_less_than_float32(float32_t x, float32_t y);
bool32_t float32_qnansafe_is_less_than_float64(float32_t x, float64_t y);
bool32_t float64_qnansafe_is_less_than_float32(float64_t x, float32_t y);
bool32_t float64_qnansafe_is_less_than_float64(float64_t x, float64_t y);

bool32_t float32_qnansafe_is_less_than_or_equal_to_float32(float32_t x, float32_t y);
bool32_t float32_qnansafe_is_less_than_or_equal_to_float64(float32_t x, float64_t y);
bool32_t float64_qnansafe_is_less_than_or_equal_to_float32(float64_t x, float32_t y);
bool32_t float64_qnansafe_is_less_than_or_equal_to_float64(float64_t x, float64_t y);

bool32_t float32_qnansafe_is_less_or_greater_than_float32(float32_t x, float32_t y);
bool32_t float32_qnansafe_is_less_or_greater_than_float64(float32_t x, float64_t y);
bool32_t float64_qnansafe_is_less_or_greater_than_float32(float64_t x, float32_t y);
bool32_t float64_qnansafe_is_less_or_greater_than_float64(float64_t x, float64_t y);

bool32_t float32_qnansafe_pair_is_unordered_float32(float32_t x, float32_t y);
bool32_t float32_qnansafe_pair_is_unordered_float64(float32_t x, float64_t y);
bool32_t float64_qnansafe_pair_is_unordered_float32(float64_t x, float32_t y);
bool32_t float64_qnansafe_pair_is_unordered_float64(float64_t x, float64_t y);

bool32_t float32_nansignalling_is_equal_to_float32(float32_t x, float32_t y);
bool32_t float32_nansignalling_is_equal_to_float64(float32_t x, float64_t y);
bool32_t float64_nansignalling_is_equal_to_float32(float64_t x, float32_t y);
bool32_t float64_nansignalling_is_equal_to_float64(float64_t x, float64_t y);








/* fraction support */

#define FRACTION32_POSITIVE_INFINITY { INT32_MAX, 1 }
#define FRACTION32_NEGATIVE_INFINITY { INT32_MIN, 1 }
#define FRACTION64_POSITIVE_INFINITY { INT64_MAX, 1 }
#define FRACTION64_NEGATIVE_INFINITY { INT64_MIN, 1 }
#define FRACTION_NAN { 0, 0 }
#define FRACTION_ZERO { 0, 1 }

typedef struct fraction32_t { int32_t numer; uint32_t denom; } fraction32_t;
typedef struct fraction64_t { int64_t numer; uint64_t denom; } fraction64_t;

fraction32_t fraction32_create(int32_t numer, int32_t denom);
fraction64_t fraction64_create(int64_t numer, int64_t denom);
fraction32_t fraction32_approximate_f32(float32_t x);
fraction32_t fraction32_approximate_f64(float64_t x);
fraction64_t fraction64_approximate_f32(float32_t x);
fraction64_t fraction64_approximate_f64(float64_t x);

fraction32_t fraction32_add(fraction32_t x, fraction32_t y);
fraction64_t fraction64_add(fraction64_t x, fraction64_t y);
fraction32_t fraction32_sub(fraction32_t x, fraction32_t y);
fraction64_t fraction64_sub(fraction64_t x, fraction64_t y);
fraction32_t fraction32_mul(fraction32_t x, fraction32_t y);
fraction64_t fraction64_mul(fraction64_t x, fraction64_t y);
fraction32_t fraction32_div(fraction32_t x, fraction32_t y);
fraction64_t fraction64_div(fraction64_t x, fraction64_t y);

float32_t    fraction32_evaluate_f32(fraction32_t f);
float64_t    fraction32_evaluate_f64(fraction32_t f);
float32_t    fraction64_evaluate_f32(fraction64_t f);
float64_t    fraction64_evaluate_f64(fraction64_t f);







/* complex number support */

typedef struct complex32_t { float32_t real, imag; } complex32_t;
typedef struct complex64_t { float64_t real, imag; } complex64_t;

complex32_t complex32_add(complex32_t z, complex32_t w);
complex64_t complex64_add(complex64_t z, complex64_t w);
complex32_t complex32_sub(complex32_t z, complex32_t w);
complex64_t complex64_sub(complex64_t z, complex64_t w);
complex32_t complex32_mul(complex32_t z, complex32_t w);
complex64_t complex64_mul(complex64_t z, complex64_t w);
complex32_t complex32_div(complex32_t z, complex32_t w);
complex64_t complex64_div(complex64_t z, complex64_t w);
complex32_t complex32_pow(complex32_t z, complex32_t w);
complex64_t complex64_pow(complex64_t z, complex64_t w);

complex32_t complex32_acos(complex32_t z);
complex64_t complex64_acos(complex64_t z);
complex32_t complex32_asin(complex32_t z);
complex64_t complex64_asin(complex64_t z);
complex32_t complex32_atan(complex32_t z);
complex64_t complex64_atan(complex64_t z);
complex32_t complex32_cos(complex32_t z);
complex64_t complex64_cos(complex64_t z);
complex32_t complex32_sin(complex32_t z);
complex64_t complex64_sin(complex64_t z);
complex32_t complex32_tan(complex32_t z);
complex64_t complex64_tan(complex64_t z);
complex32_t complex32_acosh(complex32_t z);
complex64_t complex64_acosh(complex64_t z);
complex32_t complex32_asinh(complex32_t z);
complex64_t complex64_asinh(complex64_t z);
complex32_t complex32_atanh(complex32_t z);
complex64_t complex64_atanh(complex64_t z);
complex32_t complex32_cosh(complex32_t z);
complex64_t complex64_cosh(complex64_t z);
complex32_t complex32_sinh(complex32_t z);
complex64_t complex64_sinh(complex64_t z);
complex32_t complex32_tanh(complex32_t z);
complex64_t complex64_tanh(complex64_t z);
complex32_t complex32_exp(complex32_t z);
complex64_t complex64_exp(complex64_t z);
complex32_t complex32_log(complex32_t z);
complex64_t complex64_log(complex64_t z);
complex32_t complex32_sqrt(complex32_t z);
complex64_t complex64_sqrt(complex64_t z);
complex32_t complex32_conj(complex32_t z);
complex64_t complex64_conj(complex64_t z);
complex32_t complex32_proj(complex32_t z);
complex64_t complex64_proj(complex64_t z);

float32_t complex32_abs(complex32_t z);
float64_t complex64_abs(complex64_t z);
float32_t complex32_arg(complex32_t z);
float64_t complex64_arg(complex64_t z);


/* quaternion number support */

typedef struct quaternion32_t { float32_t x,y,z,w; } quaternion32_t;
typedef struct quaternion64_t { float64_t x,y,z,w; } quaternion64_t;

quaternion32_t quaternion32_add(quaternion32_t z, quaternion32_t w);
quaternion64_t quaternion64_add(quaternion64_t z, quaternion64_t w);
quaternion32_t quaternion32_sub(quaternion32_t z, quaternion32_t w);
quaternion64_t quaternion64_sub(quaternion64_t z, quaternion64_t w);
quaternion32_t quaternion32_mul(quaternion32_t z, quaternion32_t w);
quaternion64_t quaternion64_mul(quaternion64_t z, quaternion64_t w);
quaternion32_t quaternion32_div(quaternion32_t z, quaternion32_t w);
quaternion64_t quaternion64_div(quaternion64_t z, quaternion64_t w);

quaternion32_t quaternion32_norm(quaternion32_t z);
quaternion64_t quaternion64_norm(quaternion64_t z);

float32_t quaternion32_abs(quaternion32_t z);
float64_t quaternion64_abs(quaternion64_t z);




/* time library: */

typedef enum time_month_t {
    TIME_MONTH_JANUARY   = 0,
    TIME_MONTH_FEBRUARY  = 1,
    TIME_MONTH_MARCH     = 2,
    TIME_MONTH_APRIL     = 3,
    TIME_MONTH_MAY       = 4,
    TIME_MONTH_JUNE      = 5,
    TIME_MONTH_JULY      = 6,
    TIME_MONTH_AUGUST    = 7,
    TIME_MONTH_SEPTEMBER = 8,
    TIME_MONTH_OCTOBER   = 9,
    TIME_MONTH_NOVEMBER  = 10,
    TIME_MONTH_DECEMBER  = 11,
    TIME_MONTH_MAX_ENUM  = 0x7f
} time_month_t;
typedef enum time_dst_state_t {
    TIME_DST_STATE_IN_EFFECT = 0,
    TIME_DST_STATE_NOT_IN_EFFECT = 1,
    TIME_DST_STATE_UNKNOWN = 2,
    TIME_DST_STATE_MAX_ENUM = 0x7f
} time_dst_state_t;
typedef enum time_weekday_t {
    TIME_WEEKDAY_MONDAY = 0,
    TIME_WEEKDAY_TUESDAY = 1,
    TIME_WEEKDAY_WEDNESDAY = 2,
    TIME_WEEKDAY_THURSDAY = 3,
    TIME_WEEKDAY_FRIDAY = 4,
    TIME_WEEKDAY_SATURDAY = 5,
    TIME_WEEKDAY_SUNDAY = 6,
    TIME_WEEKDAY_MAX_ENUM = 0x7f
} time_weekday_t;
typedef struct time_info_t {
    int32_t             year;
    uint16_t            day_of_year
    time_month_t        month;
    uint8_t             day_of_month;
    time_weekday_t      weekday;
    bool                leapyear;
    time_dst_state_t    dst;
    float               timezone_relative_to_UTC;
    uint8_t             hours, minutes, seconds;
    uint16_t            milliseconds, microseconds, nanoseconds;
    int64_t             unix_time;
    uint64_t            time_stamp, time_stamp_frequency_in_Hz;
} time_info_t;

/* pointer only valid until next call */
time_info_t* time_now(bool local_time);
/* faster function call to get the time_stamp field of the struct */
uint64_t time_stamp(void);



/* PRNG */

/* these are _fast_, but not cryptographically safe PRNGs!
 * Uses public domain xoshiro256++ and splitmix64 algorithms
 * (See https://prng.di.unimi.it/, https://dl.acm.org/doi/10.1145/3460772)
 */
void prng_seed(uint64_t seed); /* seed 0 is safe but predictable */
uint64_t prng_value(void);


/* this uses Hardware entropy sources, and is blocking. Use it mostly for prng_seed.
 * returns 0 on error
 */
uint64_t entropy_uint64(void);



/* memory library */

/* mem_t stores capacity in header block */
typedef void* mem_t;

mem_t mem_alloc(size_t size);
void mem_resize(mem_t* block, size_t new_size);
void mem_free(mem_t block);
size_t mem_capacity(mem_t block);
void mem_copy(mem_t src, mem_t dst);
void mem_zero(mem_t block); 
void mem_search(mem_t block, mem_t search_data);



/* string library */

/* str_t stores capacity and str_len in header block */
typedef char* str_t;
typedef const char* str_view_t;

str_t str_new(char* text);
void str_free(str_t s);
size_t str_len(str_t s);
str_t str_copy(str_t src, str_t dst);
str_t str_concat(str_t s, char* text);
str_t str_concat_int(str_t s, int64_t i);
str_t str_concat_float(str_t s, double d);

/* TODO inttypes.h format specifiers into conversion functions, as well as
    [str|wcs]to[i|u]max*/


/* actually, it might be easier to have the following types: */
typedef struct mem_t {
    void* ptr;
    size_t cap;
} mem_t;
typedef struct str_t {
    char* ptr;
    size_t len, cap;
} str_t;
typedef struct str_view_t {
    const char* ptr;
    size_t cap;
} str_view_t;
/* This means we have the size and the data not local, but we also don't need to access the data area at all and could simply use registers for that, which probably is faster anyway, also substring gets easier... */



typedef enum char_type_t {
    CHAR_TYPE_CONTROL = 0,
    CHAR_TYPE_LETTER_LOWERCASE = 1,
    CHAR_TYPE_LETTER_UPPERCASE = 2,
    CHAR_TYPE_DIGIT = 3,
    CHAR_TYPE_WHITESPACE = 4,
    CHAR_TYPE_PUNCTUATION = 5,
    CHAR_TYPE_MAX_ENUM = 0x7f
} char_type_t;
char_type_t char_type(uint32_t character);
uint32_t char_to_uppercase(uint32_t character);
uint32_t char_to_lowercase(uint32_t character);


/* I/O and Filesystem library */


typedef str_view_t path_t;

bool32_t path_exists(path_t p);
path_t   path_make_absolute(path_t p);
bool32_t path_create_empty(path_t p);
bool32_t path_rename(path_t old_path, path_t new_path);
bool32_t path_delete(path_t p);
size_t path_filesize(path_t p);
bool32_t path_isdir(path_t p);
bool32_t path_dir_list(path_t p, size_t* file_nr, path_t** list);





typedef struct stream_internal_t *stream_t;

typedef enum stream_buffering_type_t {
    STREAM_BUFFERING_TYPE_FULL = 0,
    STREAM_BUFFERING_TYPE_LINE = 1,
    STREAM_BUFFERING_TYPE_NOT = 2,
    STREAM_BUFFERING_TYPE_MAX_ENUM = 0x7f
} stream_buffering_type_t;
typedef enum stream_type_t {
    STREAM_TYPE_TEXT = 0,
    STREAM_TYPE_BINARY = 1,
    STREAM_TYPE_MAX_ENUM = 0x7f
} stream_type_t;
typedef enum stream_mode_t {
    STREAM_MODE_CREATE_WRITEONLY_ONLY_IF_NOT_EXISTING
    STREAM_MODE_CREATE_WRITEONLY_OR_TRUNCATE_TO_0_BYTES
    STREAM_MODE_CREATE_WRITEONLY_OR_APPEND
    STREAM_MODE_CREATE_READWRITE_ONLY_IF_NOT_EXISTING
    STREAM_MODE_CREATE_READWRITE_OR_TRUNCATE_TO_0_BYTES
    STREAM_MODE_CREATE_READWRITE_OR_APPEND
    STREAM_MODE_READONLY_EXISTING
    STREAM_MODE_UPDATE_EXISTING
} stream_mode_t;
typedef enum stream_result_t {
    STREAM_OK = 0,
    STREAM_EOF = -1,
    STREAM_ERROR = -2,
    STREAM_RESULT_MAX_ENUM = 0x7fffffff
}

extern stream_t stream_stdin, stream_stdout, stream_stderr;


stream_t stream_create_path(path_t p, stream_type_t type, stream_mode_t mode, stream_buffering_type_t buffer_type, mem_t buffer);
stream_t stream_create_tmpfile(void);
stream_t stream_recreate_path(stream_t old, path_t p, stream_type_t type, stream_mode_t mode);
stream_result_t stream_close(stream_t s);
stream_result_t stream_flush(stream_t s);

stream_result_t stream_print(stream_t s, str_t text);
str_t stream_read(stream_t s, size_t max_size);
#define print(x) stream_print(stream_stdout, x)
#define err_print(x) stream_print(stream_stderr, x)
#define read(s) stream_read(stream_stdin, s)












/* Assertion and Error Logging */

/* Based on RFC 5424 Severity Levels; this is used instead of a NDEBUG macro */
typedef enum err_severity_t {
    ERR_SEVERITY_EMERGENCY = 0,
    ERR_SEVERITY_ALERT = 1,
    ERR_SEVERITY_CRITICAL = 2,
    ERR_SEVERITY_ERROR = 3,
    ERR_SEVERITY_WARNING = 4,
    ERR_SEVERITY_NOTICE = 5,
    ERR_SEVERITY_INFORMATIONAL = 6,
    ERR_SEVERITY_DEBUG = 7,
    ERR_SEVERITY_MAX_ENUM = 0x7f
} err_severity_t;

extern err_severity_t err_report_level, err_abort_level;

#define assert(expression, message, level)                                  \
    do {                                                                    \
        bool cond = (expression);                                           \
        if(!cond && err_report_level >= level) {                            \
            print #expression, message,__FILE__, __LINE__, __func__ etc.    \
                                                                            \
            if(err_report_level >= level) {                                 \
                some sort of aborting                                       \
            }                                                               \
        }                                                                   \
    } while(0)







/* signal handlers */

typedef enum signal_number_t {
    SIGNAL_NUMBER_NORMAL_TERMINATION = 0,
    SIGNAL_NUMBER_ABNORMAL_TERMINATION = 1,
    SIGNAL_NUMBER_FLOATING_POINT_ERROR = 2,
    SIGNAL_NUMBER_ILLEGAL_INSTRUCTION = 3,
    SIGNAL_NUMBER_INTERACTIVE_SIGNAL = 4,
    SIGNAL_NUMBER_SEGMENTATION_FAULT = 5,
    SIGNAL_NUMBER_MAx_ENUM = 0x7f
} signal_number_t;

typedef __sig_atomic_t signal_atomic_number_t;
typedef void (*signal_handler_t)(int);
extern signal_handler_t signal_default_handler, signal_error_handler, signal_ignore_handler;

signal_handler_t signal_set_handler(signal_number_t nr, signal_handler_t handler);
bool32_t signal_raise(signal_number_t nr);



/* non-local jumps */

/* we do need a longjmp replacement, but we make the names reeeeally long to discourage its use */
typedef struct nonlocalenvironmentjump_callingenvironment_jump_buffer_internal_data_t *nonlocalenvironmentjump_callingenvironment_jump_buffer_data_t;
int nonlocalenvironmentjump_save_callingenvironment_to_jump_buffer_data_and_branch(nonlocalenvironmentjump_callingenvironment_jump_buffer_data_t callingenvironment_jump_buffer_data);
void nonlocalenvironmentjump_restore_callingenvironment_from_jump_buffer_data_and_thus_jump_back(nonlocalenvironmentjump_callingenvironment_jump_buffer_data_t callingenvironment_jump_buffer_data, int branch_return_code_value);






/* Some kind of locale.h replacement? Clearly should not be global and mutable, but a default language, country, currency triplet might be sensible */


/* headers and facilities which are effectively compeletely removed:
 *  stdarg.h
 *
 * offsetof; use &s.m instead, since it violates the rule of not using types as arguments.
 *
 * nullptr_t and nullptr, we keep using NULL since it's only an aesthetic issue
 *
 * INT .. _WIDTH macros, since they just replicate sizeof
 *
 * decimal floating point and long double support, since they are not available consistently
 * and would have to be implemented as softfloat
 *
 * iscanonical and canonicalize, since all representations are usually canonical
 */
