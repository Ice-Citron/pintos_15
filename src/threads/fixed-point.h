#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

/* Number of fractional bits in a fixed-point value. */
#define FIXED_Q 14

/* Scaling factor used to convert between integers and fixed-point values. */
#define FIXED_F (1 << FIXED_Q)

/* A signed 32-bit fixed-point number with FIXED_Q fractional bits. */
typedef int32_t fixed_t;