/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __ROFL_COMMON_ENDIANNESS_H__
#define __ROFL_COMMON_ENDIANNESS_H__

#include <stdint.h>

/**
 * This header file adds macro implementations for htonb and ntohb 
 *
 * This was originally a rofl-pipeline(rofl-datapath) that has been copied
 * during the split of repositories.
 *
 * Never distribute this header file
 */

#if defined(BIG_ENDIAN_DETECTED)
	//Host -> Network
	#define HTONB16(x) x 
	#define HTONB32(x) x
	#define HTONB64(x) x

	//Network -> Host
	#define NTOHB16(x) x 
	#define NTOHB32(x) x
	#define NTOHB64(x) x

	//
	// Conditional byte swappers. Convienience wrapper
	//
	#define COND_NTOHB16(nbo, x) x 
	#define COND_NTOHB32(nbo, x) x
	#define COND_NTOHB64(nbo, x) x
#elif defined(LITTLE_ENDIAN_DETECTED)
	#if defined(BYTESWAP_HEADER_DETECTED)
		#include <byteswap.h>
		
		//Host -> Network
		#define HTONB16(x) __bswap_16(x)  
		#define HTONB32(x) __bswap_32(x) 
		#define HTONB64(x) __bswap_64(x) 
	
		//Network -> Host
		#define NTOHB16(x) __bswap_16(x)
		#define NTOHB32(x) __bswap_32(x)
		#define NTOHB64(x) __bswap_64(x)
	#else
		//Host -> Network
		#define HTONB16(x) ( (((x) >> 8) & 0x00FF) | (((x) << 8) & 0xFF00) )
		#define HTONB32(x) \
			( (((x) >> 24) & 0x000000FF) | (((x) >>  8) & 0x0000FF00) | \
			(((x) <<  8) & 0x00FF0000) | (((x) << 24) & 0xFF000000) )

		 
		#define HTONB64(x) \
			( (((x) >> 56) & 0x00000000000000FF) | (((x) >> 40) & 0x000000000000FF00) | \
			(((x) >> 24) & 0x0000000000FF0000) | (((x) >>  8) & 0x00000000FF000000) | \
			(((x) <<  8) & 0x000000FF00000000) | (((x) << 24) & 0x0000FF0000000000) | \
			(((x) << 40) & 0x00FF000000000000) | (((x) << 56) & 0xFF00000000000000) )
		
		//Network -> Host
		#define NTOHB16(x) HTONB16(x)
		#define NTOHB32(x) HTONB32(x)
		#define NTOHB64(x) HTONB64(x)
	
	#endif

	//
	// Conditional byte swappers. Convienience wrapper
	//
	#define COND_NTOHB16(nbo, x) ( (nbo)? x : NTOHB16(x) )
	#define COND_NTOHB32(nbo, x) ( (nbo)? x : NTOHB32(x) )
	#define COND_NTOHB64(nbo, x) ( (nbo)? x : NTOHB64(x) )
#else
	#error Unsupported endianness
#endif //endianness check

#endif //__ROFL_COMMON_ENDIANNESS_H__
