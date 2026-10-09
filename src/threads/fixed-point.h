#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* Number of fractional bits in a fixed-point value. */
#define FIXED_Q 14

/* Scaling factor used to convert between integers and fixed-point values. */
#define FIXED_F (1 << FIXED_Q)

/* A signed 32-bit fixed-point number with FIXED_Q fractional bits. */
typedef int32_t fixed_t;

/* Converts integer N to fixed-point. */
static inline fixed_t
fp_from_int (int n)
{
    return n * FIXED_F;
}

/* Converts X to an integer, rounding toward zero. */
static inline int
fp_to_int_trunc (fixed_t x)
{
    return x / FIXED_F;
}

/*  Converts X to the nearest integer, rounding halfway cases away from zero. */
static inline int
fp_to_int_round (fixed_t x)
{
    int64_t adjusted;

    if (x >= 0)
    adjusted = (int64_t) x + FIXED_F / 2;
    else
        adjusted = (int64_t) x - FIXED_F / 2;

    return adjusted / FIXED_F;

}

/* Returns X + Y. */
static inline fixed_t
fp_add (fixed_t x, fixed_t y)
{
    return x + y;
}

/* Returns X - Y. */
static inline fixed_t
fp_sub (fixed_t x, fixed_t y)
{
    return x - y;
}

/* Returns X + N, where N is an integer. */
static inline fixed_t
fp_add_int (fixed_t x, int n)
{
    return x + fp_from_int (n);
}

/* Returns X - N, where N is an integer. */
static inline fixed_t
fp_sub_int (fixed_t x, int n)
{
    return x - n * fp_from_int (n);
}

#endif /* threads/fixed-point.h*/