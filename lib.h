/* we keep the math and threading libraries from libc since they're pretty good ? */
#include <threads.h>
#include <math.h>
/* also these are used everywhere (and really should be language primitives): */
#include <stdbool.h>
#include <stdint.h>





/* library maintenance: */

typedef struct lib_init_params_t {

} lib_init_params_t;

    /* should overtake gfx_init, snd_init etc, and also fs/io, threading, cam, socket etc things.
     this should also overtake libc init, and give back cmdline args etc. */
void lib_init(lib_init_params_t params);
void lib_cleanup(void);





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


typedef enum int_exceptions_flagbit_t {
    INT_EXCEPTIONS_DIVIDE_BY_ZERO_BIT = 0x01,
    INT_EXCEPTIONS_OVERFLOW_BIT = 0x02,
    INT_EXCEPTIONS_UNDERFLOW_BIT = 0x04,
    INT_EXCEPTIONS_LAST = INT_EXCEPTIONS_UNDERFLOW_BIT
} int_exceptions_flagbit_t;
#define INT_EXCEPTIONS_ALL 0x07
typedef uint8_t int_exceptions_mask_t
int_exceptions_mask_t int_exceptions_get(void);
void int_exceptions_clear(void);





intmax ?
(u)intN_t, (u)int_leastN_t




/* TODO limits.h stuff */


typedef enum int_endianness_t {
    INT_ENDIANNESS_LITTLE_ENDIAN = 0,
    INT_ENDIANNESS_BIG_ENDIAN = 1,
    INT_ENDIANNESS_MAX_ENUM = 0x7f
} int_endianness_t;

int_endianness_t int_native_endianness(void);








<?c
const char* int_types[] = {"int8", "int16", "int32", "int64", "int128", "intmax",
                            "uint8", "uint16", "uint32", "uint64", "uint128", "uintmax"};
const char* bitops[] = {
    "leading_zeros", "leading_ones", "trailing_zeros", "trailing_ones",
    "first_leading_zero", "first_leading_one", "first_trailing_zero", "first_trailing_one",
    "count_zeros", "count_ones", "bit_width", "bit_floor", "bit_ceil",
}

for(int i = 0; i < sizeof(int_types)/sizeof(char*); i++) {
    char* t1 = int_types[i];
?>
typedef struct @t1@_div_t {
    @t1@_t quotient, remainder;
} @t1@_div_t;
@t1@_t @t1@_abs(@t1@_t value);
@t1@_div_t @t1@_div(@t1@_t numer, @t1@_t denom);
bool32_t @t1@_has_single_bit(@t1@_t value);
<?c
    for(int j = 0; j < sizeof(bitops)/sizeof(char*); j++) {
        char* F = bitops[j];
?>
uint32_t @t1@_@F@(@t1@_t value);
<?c
    }
/** checked arithmetic: */
    for(int j = 0; j < sizeof(int_types)/sizeof(char*); j++) {
        char* t2 = int_types[j];
?>
bool32_t @t1@_checked_conversion_from_@t2@(@t1@_t* result, @t2@_t val);
<?c
        for(int k = 0; k < sizeof(int_types)/sizeof(char*); k++) {
            char* t3 = int_types[k];
?>
bool32_t @t1@_checked_addition_of_@t2@_and_@t3@(@t1@_t* result, @t2@_t a, @t3@_t b);
bool32_t @t1@_checked_subtraction_of_@t2@_and_@t3@(@t1@_t* result, @t2@_t a, @t3@_t b);
bool32_t @t1@_checked_multiplication_of_@t2@_and_@t3@(@t1@_t* result, @t2@_t a, @t3@_t b);
<?c
        }
    }
}
?>





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
bool32_t float_exceptions_clear(void);
float_rounding_direction_t float_rounding_direction_get(void);
bool32_t float_rounding_direction_set(float_rounding_direction_t m);

bool32_t float_flush_denormals_to_zero(bool32_t flag);


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









<?c
const char* float_types[] = {"float16", "float32", "float64", "float80", "float128",
                             "decimal32", "decimal64", "decimal128"};
const char* unary_functions[] = {
    "acos", "asin", "atan", "cos", "sin", "tan",
    "acospi", "asinpi", "atanpi", "cospi", "sinpi", "tanpi",
    "acosh", "asinh", "atanh", "cosh", "sinh", "tanh",
    "exp", "exp2", "exp10", "expm1", "exp2m1", "exp10m1",
    "log", "log2". "log10", "logp1", "log2p1", "log10p1",
    "sign", "abs", "sqrt", "rsqrt", "cbrt", "erf", "erfc", "lgamma", "tgamma",
    "radians_from_degrees", "degrees_from_radians",
    "ceil", "floor", "fract_from_floor", "trunc",
    "round_default", "round_default_raise_inexact", "round_with_half_away_from_zero",
    "round_with_half_to_even", "next_greater", "next_lesser"
}
const char* binary_functions[] = {
    "atan2", "atan2pi", "hypot", "pow", "powr", "fdim", "step",
    "add", "sub", "mul", "div", "mod", "IEEE_remainder", "copysign_over_value", "next_in_direction_of",
    "max", "min", "max_abs", "min_abs"
}
const char* trinary_functions[] = {
    "fma", "clamp", "lerp", "smoothstep"
}
const char* binary_with_int_functions[] = {
    "x_mul_exp2n", "x_mul_exp10n", "1_plus_x_pown", "pown", "rootn"
}
const char* binary_with_ptr_functions[] = {
    "split_into_binary_mantissa_and_log2", "split_into_decimal_mantissa_and_log10",
    "split_into_fraction_and_floor", "split_into_fraction_and_ceil", "split_into_fraction_and_round_default",
    "split_into_fraction_and_round_with_half_away_from_zero", "split_into_fraction_and_round_with_half_to_even",
}
const char* compare_funcs[] = {
    "qnansafe_is_greater_than", "qnansafe_is_greater_than_or_equal_to",
    "qnansafe_is_less_than", "qnansafe_is_less_than_or_equal_to",
    "qnansafe_is_less_or_greater_than", "qnansafe_pair_is_unordered",
    "nansignalling_is_equal_to"
}
const char* round_with_direction_funcs[] = {
    "round_with_direction_and_test_bitwidth", "round_with_direction_and_test_u_bitwidth",
    "with_direction_and_test_bitwidth_raise_inexact", "with_direction_and_test_u_bitwidth_raise_inexact",
}


for(int i = 0; i < sizeof(float_types)/sizeof(char*); i++) {
    char* T = float_types[i];
?>
float_number_type_t @T@_classify(@T@_t x);
bool32_t @T@_is_nan(@T@_t x);
bool32_t @T@_sign_bit(@T@_t x);
@T@_t @T@_make_qnan(const char* tagp);
@T@_t @T@_make_snan(const char* tagp);
<?c
    for(int k = 0; k < sizeof(unary_functions)/sizeof(char*); k++) {
        char* F = unary_functions[k];
?>
@T@_t @T@_@F@(@T@_t x);
<?c
    }
    for(int k = 0; k < sizeof(binary_functions)/sizeof(char*); k++) {
        char* F = binary_functions[k];
?>
@T@_t @T@_@F@(@T@_t x, @T@_t y);
<?c
    }
    for(int k = 0; k < sizeof(trinary_functions)/sizeof(char*); k++) {
        char* F = trinary_functions[k];
?>
@T@_t @T@_@F@(@T@_t x, @T@_t y, @T@_t z);
<?c
    }
    for(int k = 0; k < sizeof(binary_with_int_functions)/sizeof(char*); k++) {
        char* F = binary_with_int_functions[k];
        for(int j = 0; j < sizeof(int_types)/sizeof(char*); j++) {
            char* Ti = int_types[j];
?>
@T@_t @T@_@F@_@Ti@(@T@_t x, @Ti@_t n);
<?c
        }
    }
    for(int k = 0; k < sizeof(binary_with_ptr_functions)/sizeof(char*); k++) {
        char* F = binary_with_ptr_functions[k];
        for(int j = 0; j < sizeof(int_types)/sizeof(char*); j++) {
            char* Ti = int_types[j];
?>
@T@_t @T@_@F@_@Ti@(@T@_t x, @Ti@_t* p);
<?c
        }
        for(int j = 0; j < sizeof(float_types)/sizeof(char*); j++) {
            char* T2 = float_types[j];
?>
@T@_t @T@_@F@_@T2@(@T@_t x, @T2@_t* p);
<?c
        }
    }
    for(int k = 0; k < sizeof(round_with_direction_funcs)/sizeof(char*); k++) {
        char* F = round_with_direction_funcs[k];
?>
@T@_t @T@_@F@(@T@_t x, float_rounding_direction_t direction, uint32_t bitwidth);
<?c
    }

    for(int j = 0; j < sizeof(float_types)/sizeof(char*); j++) {
        char* T2 = float_types[j];
?>
float_pair_ordering_t @T@_qnansafe_compare_to_@T2@(@T@_t x, @T2@_t y);
<?c
        for(int k = 0; k < sizeof(compare_funcs)/sizeof(char*); k++) {
            char* F = compare_funcs[k];
?>
bool32_t @T@_@F@_@T2@(@T@_t x, @T2@_t y)
<?c
        }
    }

?>
/* pre-converted functions; int versions will, if necessarily, call round_default before. */
<?c
    for(int j = 0; j < sizeof(float_types)/sizeof(char*) + sizeof(int_types)/sizeof(char); j++) {
        char* T2;
        if(j < sizeof(float_types)/sizeof(char*)) {
            T2 = float_types[j];
        } else {
            T2 = int_types[j - sizeof(float_types)/sizeof(char*)];
        }

        for(int k = 0; k < sizeof(unary_functions)/sizeof(char*); k++) {
            char* F = unary_functions[k];
?>
@T2@_t @T2@_from_@T@_@F@(@T@_t x);
<?c
        }
        for(int k = 0; k < sizeof(binary_functions)/sizeof(char*); k++) {
            char* F = binary_functions[k];
?>
@T2@_t @T2@_from_@T@_@F@(@T@_t x, @T@_t y);
<?c
        }
        for(int k = 0; k < sizeof(trinary_functions)/sizeof(char*); k++) {
            char* F = trinary_functions[k];
?>
@T2@_t @T2@_from_@T@_@F@(@T@_t x, @T@_t y, @T@_t z);
<?c
        }
        for(int k = 0; k < sizeof(binary_with_int_functions)/sizeof(char*); k++) {
            char* F = binary_with_int_functions[k];
            for(int l = 0; l < sizeof(int_types)/sizeof(char*); l++) {
                char* Ti = int_types[l];
?>
@T2@_t @T2@_from_@T@_@F@_@Ti@(@T@_t x, @Ti@_t n);
<?c
            }
        }
    }
}
?>


/* this is the only not allowed for decimal floating point */
float32_t  float32_IEE_remainder_and_quotient_i32(float32_t x, float32_t y, int32_t* quo);
float64_t  float64_IEE_remainder_and_quotient_i32(float64_t x, float64_t y, int32_t* quo);



















/* array functions: */
void int8_sort_inplace(int8_t* array, size_t len);
void int16_sort_inplace(int16_t* array, size_t len);
void int32_sort_inplace(int32_t* array, size_t len);
void int64_sort_inplace(int64_t* array, size_t len);
void uint8_sort_inplace(uint8_t* array, size_t len);
void uint16_sort_inplace(uint16_t* array, size_t len);
void uint32_sort_inplace(uint32_t* array, size_t len);
void uint64_sort_inplace(uint64_t* array, size_t len);
/* float ordering: with 1 for a > b, 0 for a == b, -1 for a < b:
int comp(float a, float b) {
    if((bits(a) == bits(b))) {
        return 0;
    }
    if(signbit(a) == 0 && signbit(b) == 1) {
        return 1;
    }
    if(signbit(a) == 1 && signbit(b) == 0) {
        return -1;
    }
    if(exponentbits(a) != exponentbits(b)) {
        return int_comp(exponentbits(a), exponentbits(b));
    }
    return int_comp(mantissabits(a), mantissabits(b));
}
this can be easily understood to be the same as the signed integer comparison.
I.e., float32_sort_inplace is works like an int32 in sign-magnitude representation.
By this construction, NaNs are sorted between the finite values and the infinities.
*/
void float32_sort_inplace(float32_t* array, size_t len);
void float64_sort_inplace(float64_t* array, size_t len);

the same also for complex, quaternion, fraction types


void int8_partition_inplace(int8_t* array, size_t len, int8_t pivot, size_t* partition_index);
void int16_partition_inplace(int16_t* array, size_t len, int16_t pivot, size_t* partition_index);
void int32_partition_inplace(int32_t* array, size_t len, int32_t pivot, size_t* partition_index);
void int64_partition_inplace(int64_t* array, size_t len, int64_t pivot, size_t* partition_index);
void uint8_partition_inplace(uint8_t* array, size_t len, uint8_t pivot, size_t* partition_index);
void uint16_partition_inplace(uint16_t* array, size_t len, uint16_t pivot, size_t* partition_index);
void uint32_partition_inplace(uint32_t* array, size_t len, uint32_t pivot, size_t* partition_index);
void uint64_partition_inplace(uint64_t* array, size_t len, uint64_t pivot, size_t* partition_index);



find
search (requires being sorted)
min
max
min_abs
max_abs
euclidean_norm
scale / scalar_mul
shift / scalar_add
    or these both together?
pointwise add, mul, sub, div etc.
normalize_to_max_abs_value
normalize_to_euclidean_value





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
    uint16_t            day_of_year, week_in_year;
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

uint16_t mem_area_code_get(void);
void mem_area_code_set(uint16_t code);


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




better idea:


typedef enum buf_result_t {
    BUF_OK = 0,
    
} buf_result_t;

typedef struct {
    uint8_t* byte_ptr;
    size_t len, cap;
} *buf_t, const *cbuf_t;
typedef struct slice_t {
    uint8_t* byte_ptr;
    size_t len;
} *slice_t, const *cslice_t;
typedef struct view_t {
    const uint8_t* byte_ptr;
    size_t len;
} *view_t, const *cview_t;


buf_result_t buf_alloc(buf_t b, size_t size, size_t align, size_t offset);
buf_result_t buf_reserve(buf_t b, size_t new_cap)
    { return buf_resize(b, b.cap+new_cap); }
buf_result_t buf_resize(buf_t b, size_t new_cap);
buf_result_t buf_copy(buf_t dst, view_t src, size_t dst_offset);
buf_result_t buf_concat(buf_t dst, view_t src)
    { return buf_copy(dst, src, dst.len); }

void buf_free(buf_t b);

slice_t slice_sub(slice_t s, size_t begin, size_t end);
void slice_zero(slice_t s);

view_t view_str(const char* str);
view_t view_sub(view_t v, size_t begin, size_t end);
bool view_equals(view_t a, view_t b);
view_t view_find(view_t v1, view_t v2); // returns NULL if not there
view_t view_find_last(view_t v1, view_t v2);

buf_result_t view_split_by_byte(view_t v, uint8_t delim, buf_t* bufs, size_t nr_bufs);
buf_result_t view_split_by_view(view_t v, view_t delim, buf_t* bufs, size_t nr_bufs);

// util functions that are basically just casts
void buf_zero(buf_t b) { slice_zero((slice_t)b); }
slice_t buf_slice(buf_t b) { return (slice_t) b; }
view_t buf_view(buf_t b) { return (view_t) b; }
slice_t buf_subslice(buf_t b, size_t begin, size_t end)
view_t buf_subview(buf_t b, size_t begin, size_t end)
    { return view_sub((view_t) b, begin, end); }
bool buf_equals(buf_t a, buf_t b)
    { return view_equals((view_t) a, (view_t)b); }
view_t buf_find(buf_t b, view_t v)
    { return view_find((view_t) b, (view_t)v); }
view_t buf_find_last(buf_t b, view_t v)
    { return view_find_last((view_t) b, (view_t)v); }

view_t slice_subview(slice_t s, size_t begin, size_t end)
    { return view_sub((view_t) s, begin, end); }
view_t slice_view(slice_t s) { return (view_t) s; }
bool slice_equals(slice_t a, slice_t b)
    { return view_equals((view_t) a, (view_t)b); }
view_t slice_find(slice_t s, view_t v)
    { return view_find(view_t) s, (view_t)v); }
view_t slice_find_last(slice_t s, view_t v)
    { return view_find_last(view_t) s, (view_t)v); }







// much, much simpler interface for just raw pointers:



void* mem_alloc(size_t cap, size_t align, size_t offset);
void* mem_resize(void* ptr, size_t old_cap, size_t new_cap);
void mem_copy(void* dst, size_t dst_cap, void* src, size_t src_cap, size_t len);
void mem_free(void* ptr, size_t cap);
void mem_zero(void* ptr, size_t len);

void* mem_from_str(const char* str);
bool mem_equals(void* a, void* b, size_t len);
void* mem_find(const void* area, size_t cap, const void* search, size_t search_cap); // returns NULL if not there
void* mem_find_last(const void* area, size_t cap, const void* search, size_t search_cap);

















// different idea: two kinds of strings
// a: string identifiers, only there to be copied and compared:
// these have the length at index -1 and are capped to 254 chars
typedef const char* strid_t;
#define STRID_INVALID ((strid_t)NULL)
strid_t strid(const char* str);
strid_t strid_from_len(const char* str, size_t len);
strid_t strid_dup(strid_t id);
bool strid_equals(strid_t a, strid_t b);
int strid_cmp(strid_t a, strid_t b);
int strid_subindex(strid_t outer, strid_t inner);
void strid_free(strid_t id);
size_t strid_len(strid_t id);



// b: string format buffers, for concatting / formatting into and parsing out of:






typedef enum strbuf_pad_type_t {
    STRBUF_PAD_TYPE_RIGHT = 0,
    STRBUF_PAD_TYPE_LEFT = 1,
    STRBUF_PAD_TYPE_MAX_ENUM = 0x7f
} strbuf_pad_type_t;
typedef enum strbuf_sign_behavior_t {
    STRBUF_SIGN_BEHAVIOR_ONLY_NEGATIVE = 0,
    STRBUF_SIGN_BEHAVIOR_POSITIVE_AS_PLUS = 1,
    STRBUF_SIGN_BEHAVIOR_POSITIVE_AS_SPACE = 2,
    STRBUF_SIGN_BEHAVIOR_MAX_ENUM = 0x7f
} strbuf_sign_behavior_t;
typedef enum strbuf_int_format_t {
    STRBUF_INT_FORMAT_DECIMAL = 0,
    STRBUF_INT_FORMAT_OCTAL = 1,
    STRBUF_INT_FORMAT_OCTAL_WITH_PREFIX_NONZERO = 2,
    STRBUF_INT_FORMAT_HEXADECIMAL_LOWER_CASE = 3,
    STRBUF_INT_FORMAT_HEXADECIMAL_UPPER_CASE = 4,
    STRBUF_INT_FORMAT_HEXADECIMAL_LOWER_CASE_WITH_PREFIX_NONZERO = 5,
    STRBUF_INT_FORMAT_HEXADECIMAL_UPPER_CASE_WITH_PREFIX_NONZERO = 6,
    STRBUF_INT_FORMAT_MAX_ENUM = 0x7f
} strbuf_int_format_t;
typedef enum strbuf_float_format_t {
    STRBUF_FLOAT_FORMAT_NO_EXPONENT = 0,
    STRBUF_FLOAT_FORMAT_NO_EXPONENT_FORCE_DECIMAL_POINT = 1,
    STRBUF_FLOAT_FORMAT_SCIENTIFIC = 2,
    STRBUF_FLOAT_FORMAT_SCIENTIFIC_UPPER_CASE_E = 3,
    STRBUF_FLOAT_FORMAT_SCIENTIFIC_FORCE_DECIMAL_POINT = 4,
    STRBUF_FLOAT_FORMAT_SCIENTIFIC_FORCE_DECIMAL_POINT_UPPER_CASE_E = 5,
    STRBUF_FLOAT_FORMAT_MIXED = 6,
    STRBUF_FLOAT_FORMAT_MIXED_UPPER_CASE_E = 7,
    STRBUF_FLOAT_FORMAT_MIXED_FORCE_DECIMAL_POINT = 8,
    STRBUF_FLOAT_FORMAT_MIXED_FORCE_DECIMAL_POINT_UPPER_CASE_E = 9,
    STRBUF_FLOAT_FORMAT_MAX_ENUM = 0x7f
} strbuf_float_format_t;


typedef struct strbuf_t {
    char* str;
    size_t len, cap;
} strbuf_t;
typedef struct format_params_t {
    uint32_t min_length, precision;
    strbuf_pad_type_t pad_type;
    char pad_char;
    strbuf_sign_behavior_t sign_behavior;
} format_params_t;
#define STRBUF_ALLOCATION_FAILURE (strbuf_t)({NULL,0,0})
#define STRBUF_ERROR (strbuf_t)({NULL,1,0})

strbuf_t strbuf_from_str(char* str); // str needs to have been _allocated_ by the standard allocator (like strids), otherwise use strbuf_concat
strbuf_t strbuf_alloc(size_t initial_cap);
strbuf_t strbuf_reserve(strbuf_t b, size_t additional_cap);
strbuf_t strbuf_resize(strbuf_t b, size_t new_min_cap);
strbuf_t strbuf_dup(strbuf_t b);
void strbuf_free(strbuf_t b);
strbuf_t strbuf_zero(strbuf_t b);
    // the next six functions return != buf for range or allocation failure. end is _inclusive_, begin/end are 0-indexed
strbuf_t strbuf_replace_range(strbuf_t buf, size_t begin, size_t end, const char* str);
strbuf_t strbuf_replace_range_len(strbuf_t buf, size_t begin, size_t end, const char* str, size_t len);
strbuf_t strbuf_dup_slice(strbuf_t buf, size_t begin, size_t end);
strbuf_t strbuf_split_by_char(strbuf_t buf, char delim, strbuf_t** bufs, size_t* nr_bufs);
strbuf_t strbuf_split_by_str(strbuf_t buf, char* delim, strbuf_t** bufs, size_t* nr_bufs);
strbuf_t strbuf_split_by_str_len(strbuf_t buf, char* delim, size_t delim_len, strbuf_t** bufs, size_t* nr_bufs);

bool strbuf_equals(strbuf_t a, strbuf_t b);    // NOTE: this function treats buffers with the same content and lengths but different caps as the same
int strbuf_cmp(strbuf_t a, strbuf_t b);
int strbuf_find_subindex(strbuf_t outer, size_t offset, strid_t inner);
int strbuf_find_last_subindex(strbuf_t outer, size_t offset, strid_t inner);

strbuf_t strbuf_concat(strbuf_t buf, const char* str);
strbuf_t strbuf_concat_len(strbuf_t buf, const char* str, size_t len);
// use params = NULL for default params (default sign behavior, no padding, default precision etc.) and format = 0 for default format (%u or %f respectively)
strbuf_t strbuf_concat_int8(strbuf_t buf, int8_t val, format_params_t* params);
strbuf_t strbuf_concat_int16(strbuf_t buf, int16_t val, format_params_t* params);
strbuf_t strbuf_concat_int32(strbuf_t buf, int32_t val, format_params_t* params);
strbuf_t strbuf_concat_int64(strbuf_t buf, int64_t val, format_params_t* params);
strbuf_t strbuf_concat_uint8(strbuf_t buf, uint8_t val, format_params_t* params, strbuf_int_format_t format);
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


typedef buf_t path_t;

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














/* webcam library based on sr_webcam (https://github.com/kosua20/sr_webcam): */
typedef void* webcam_t;
typedef void (*webcam_callback_t)(webcam_t cam, void* data); /* data is RGB-image with buffersize 3*width*height with the out-values of the open function */

webcam_t webcam_open(int id, int* width, int* height, int* framerate, webcam_callback_t callback); /* all pointer params are in-out! */
void webcam_start(webcam_t cam);
void swebcam_stop(webcam_t cam);
void webcam_close(webcam_t cam);





