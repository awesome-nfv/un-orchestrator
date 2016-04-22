/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CRANDOM_H
#define CRANDOM_H 1

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include <limits>

#include "rofl/common/croflexception.h"
#include "rofl/common/cmemory.h"

namespace rofl
{

class eRandomBase : public eMemBase {}; // error base class crandom
class eRandomOpenFailed : public eRandomBase {}; // open system-call failed
class eRandomReadFailed : public eRandomBase {}; // read system-call failed

class crandom : public cmemory {
#define DEV_URANDOM "/dev/urandom"
public:

	/**
	 * @brief	returns a random number between 0 and 1
	 */
	static double
	draw_random_number();

	// constructor with default random number length of 4 bytes
	crandom(size_t vallen = sizeof(uint32_t));
	// destructor
	virtual
	~crandom();
	// copy constructor
	crandom(crandom &r)
	{
		*this = r;
	};
	// assignment operator
	crandom&
	operator=(const crandom &r)
	{
		if (this == &r)
			return *this;
		cmemory::operator= (r);
		return *this;
	};

	/** return random number of length "length"
	 */
	crandom& rand(size_t length);
	/** return length of random number
	 */
	size_t randlen();
	/** convenience method: return uint8_t
	 */
	uint8_t uint8();
	/** convenience method: return uint16_t
	 */
	uint16_t uint16();
	/** convenience method: return uint32_t
	 */
	uint32_t uint32();
	/** convenience method: return uint64_t
	 */
	uint64_t uint64();

public:

	friend std::ostream&
	operator<< (std::ostream& os, crandom const& rand) {
		os << "<crandom ";
			os << dynamic_cast<cmemory const&>( rand );
		os << ">";
		return os;
	};

};

}; // end of namespace

#endif
