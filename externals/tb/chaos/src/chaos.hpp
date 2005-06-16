// 
//  
//  chaos~
//  Copyright (C) 2004  Tim Blechmann
//  
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//  
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//  
//  You should have received a copy of the GNU General Public License
//  along with this program; see the file COPYING.  If not, write to
//  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//  Boston, MA 02111-1307, USA.

#ifndef __chaos_hpp

#define FLEXT_ATTRIBUTES 1

#define _USE_MATH_DEFINES /* tg says: define this before including cmath */
#include "flext.h"
#include "chaos_defs.hpp"
#include <cmath>

#include <cstdlib>

/* internal we can work with a higher precision than pd */
#ifdef DOUBLE_PRECISION
typedef double data_t;
#define CHAOS_ABS(x) fabs(x)
#else
typedef float data_t;
#define CHAOS_ABS(x) fabsf(x)
#endif

inline data_t chaos_mod(data_t x, data_t y)
{
#ifdef DOUBLE_PRECISION
	return fmod(x,y);
#else
	return fmodf(x,y);
#endif
}

inline data_t rand_range(data_t low, data_t high)
{
	return low + ( (rand() * (high - low)) / RAND_MAX);
}



#define __chaos_hpp
#endif /* __chaos_hpp */
