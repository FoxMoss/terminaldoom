// Emacs style mode select   -*- C++ -*-
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//	Fixed point implementation.
//
//-----------------------------------------------------------------------------

// static const char
// rcsid[] = "$Id: m_bbox.c,v 1.1 1997/02/03 22:45:10 b1 Exp $";

#include "stdlib.h"

#include "doomtype.h"
#include "i_system.h"
#include <stdint.h>

#ifdef __GNUG__
#pragma implementation "m_fixed.h"
#endif
#include "m_fixed.h"

// Fixme. __USE_C_FIXED__ or something.

fixed_t FixedMul(fixed_t a, fixed_t b) {
  return ((long long)a * (long long)b) >> FRACBITS;
}

//
// FixedDiv, C version.
//

fixed_t FixedDiv(fixed_t a, fixed_t b) {
  if ((abs(a) >> 14) >= abs(b))
    return (a ^ b) < 0 ? MININT : MAXINT;
  return FixedDiv2(a, b);
}

// pure vibe code, not implementing this shit...
typedef uint32_t u32;

/**
 * FixedDiv2(a,b) = floor((a<<16)/b)
 * computed in pure 32-bit.
 */
fixed_t FixedDiv2(fixed_t a, fixed_t b) {
  if (b == 0) {
    // divide by zero → saturate, or handle error as you like
    return (a >= 0) ? INT32_MAX : INT32_MIN;
  }

  // --- 1) handle sign ---
  int neg = ((a ^ b) < 0);
  u32 ua = (a < 0) ? (u32)(-a) : (u32)a;
  u32 ub = (b < 0) ? (u32)(-b) : (u32)b;

  // --- 2) integer quotient and remainder ---
  u32 Q = ua / ub;
  u32 R = ua % ub;

  // --- 3) build 16-bit fractional part by long division ---
  u32 frac = 0;
  for (int bit = 15; bit >= 0; --bit) {
    R <<= 1;
    if (R >= ub) {
      R -= ub;
      frac |= (1u << bit);
    }
  }

  // --- 4) combine integer and fractional parts ---
  //    (Q<<16) | frac = floor((a<<16)/b) for non-negatives
  u32 result = (Q << 16) | frac;

  return neg ? -(fixed_t)result : (fixed_t)result;
}
