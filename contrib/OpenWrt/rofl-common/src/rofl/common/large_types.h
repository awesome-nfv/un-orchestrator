/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ROFL_COMMON_LT_H__
#define __ROFL_COMMON_LT_H__

#include <stdint.h>
#include "endianness.h"

/*
 * This header file defines a structure for a type of 128 bits
 *
 * This was originally a rofl-pipeline(rofl-datapath) that has been copied
 * during the split of repositories.
 *
 * Never distribute this header file
 */

// This is defined as a 16 vector so the pipeline doesn't has to deal with byte order at all (e.g. upper and lower u64, etc)
typedef struct uint128_{
	uint8_t val[16];
}uint128__t;

//This is a wrapper that helps accessing uin128__t types 
typedef struct wrap_u128{
	uint64_t hi;
	uint64_t lo;
} __attribute__ ((__may_alias__)) w128_t;

#define UINT128__T_HI(x) ((w128_t*)&x)->hi
#define UINT128__T_LO(x) ((w128_t*)&x)->lo

#if defined(LITTLE_ENDIAN_DETECTED)
	#define HTONB128(x) do{ \
		((w128_t*)&(x))->hi = HTONB64(((w128_t*)&(x))->hi); \
		((w128_t*)&(x))->lo = HTONB64(((w128_t*)&(x))->lo); \
	}while(0)
	#define NTOHB128(x) HTONB128(x) 

	//Conditional byte swap
	#define COND_NTOHB128(cond, x) do { if(cond)NTOHB128(x); }while(0)
#else
	#define HTONB128(x) do{}while(0)
	#define NTOHB128(x) do{}while(0)

	//Conditional byte swap
	#define COND_NTOHB128(cond, x) do{}while(0);
#endif //LITTLE_ENDIAN_DETECTED

//Helpers
#define UINT128__T_IS_NOT_ZERO(x) ( (UINT128__T_HI(x) != 0x0ULL) || (UINT128__T_LO(x) != 0x0ULL) )
#define UINT128__T_IS_ZERO(x) ! UINT128__T_IS_NOT_ZERO(x) 

#endif //__ROFL_COMMON_LT_H__
