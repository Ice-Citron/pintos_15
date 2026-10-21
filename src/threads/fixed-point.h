#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* Number of fractional bits in a fixed-point value.  */
#define FIXED_Q 14

/* Scaling factor used to convert between integers and fixed-point values.  */
#define FIXED_F (1 << FIXED_Q)

/* A signed 17.14 fixed-point number.  Wrapping the raw value in a struct
   makes fixed_t a distinct type, so the compiler rejects an attempt to
   mix it with a plain int without any explicit conversion.  */
typedef struct
  {
    int32_t raw;  /* Value scaled by FIXED_F. */
  } fixed_t;

/* Converts integer N to fixed-point.  */
static inline fixed_t
fp_from_int (int n)
{
  return (fixed_t) { n * FIXED_F };
}

/* Converts X to an integer, rounding toward zero.  */
static inline int
fp_to_int_trunc (fixed_t x)
{
  return x.raw / FIXED_F;
}

/* Converts X to the nearest integer, rounding halfway cases away from
   zero.  */
static inline int
fp_to_int_round (fixed_t x)
{
  int64_t adjusted;

  if (x.raw >= 0)
    adjusted = (int64_t) x.raw + FIXED_F / 2;
  else
    adjusted = (int64_t) x.raw - FIXED_F / 2;

  return adjusted / FIXED_F;
}

/* Returns X + Y.  */
static inline fixed_t
fp_add (fixed_t x, fixed_t y)
{
  return (fixed_t) { x.raw + y.raw };
}

/* Returns X - Y.  */
static inline fixed_t
fp_sub (fixed_t x, fixed_t y)
{
  return (fixed_t) { x.raw - y.raw };
}

/* Returns X + N, where N is an integer.  */
static inline fixed_t
fp_add_int (fixed_t x, int n)
{
  return fp_add (x, fp_from_int (n));
}

/* Returns X - N, where N is an integer.  */
static inline fixed_t
fp_sub_int (fixed_t x, int n)
{
  return fp_sub (x, fp_from_int (n));
}

/* Returns X * N, where N is an integer.  */
static inline fixed_t
fp_mul_int (fixed_t x, int n)
{
  return (fixed_t) { x.raw * n };
}

/* Returns X / N, where N is a nonzero integer.  */
static inline fixed_t
fp_div_int (fixed_t x, int n)
{
  return (fixed_t) { x.raw / n };
}

/* Returns X * Y.  The product of two scaled values is scaled twice, so it is
   divided by FIXED_F once.  The multiplication is done in 64 bits because the
   intermediate can overflow 32 bits even when the result fits.  */
static inline fixed_t
fp_mul (fixed_t x, fixed_t y)
{
  return (fixed_t) { ((int64_t) x.raw) * y.raw / FIXED_F };
}

/* Returns X / Y, where Y is nonzero.  Dividing two scaled values cancels the
   scaling, so X is multiplied by FIXED_F first.  The multiplication is done
   in 64 bits because X * FIXED_F can overflow 32 bits.  */
static inline fixed_t
fp_div (fixed_t x, fixed_t y)
{
  return (fixed_t) { ((int64_t) x.raw) * FIXED_F / y.raw };
}

#endif /* threads/fixed-point.h */
