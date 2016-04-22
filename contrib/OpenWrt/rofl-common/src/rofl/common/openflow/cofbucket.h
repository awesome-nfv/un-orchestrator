/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COFBUCKET_H
#define COFBUCKET_H 1

#include <string>
#include <vector>
#include <list>
#include <endian.h>
#include <ostream>
#ifndef htobe16
	#include "../endian_conversion.h"
#endif

#include "rofl/common/openflow/openflow.h"
#include "rofl/common/croflexception.h"
#include "rofl/common/cmemory.h"
#include "rofl/common/openflow/cofactions.h"

namespace rofl {
namespace openflow {

/* error classes */
class eBucketBase 	: public RoflException {}; // error base class for class cofbucket
class eBucketInval 	: public eBucketBase {}; // parameter is invalid
class eBucketBadLen : public eBucketBase {}; // invalid length



class cofbucket {

	uint8_t ofp_version;

	uint64_t packet_count; // packet count for this bucket
	uint64_t byte_count; // byte count for this bucket
    uint16_t weight;
    uint32_t watch_port;
    uint32_t watch_group;

	cofactions actions; // list of OpenFlow actions

public: // per instance methods

	/**
	 *
	 */
	cofbucket(
			uint8_t ofp_version = openflow::OFP_VERSION_UNKNOWN,
			uint16_t weigth = 0,
			uint32_t watch_port = 0,
			uint32_t watch_group = 0);


	/**
	 *
	 */
	cofbucket(
			uint8_t ofp_version,
			uint8_t *bucket,
			size_t bclen);


	/**
	 *
	 */
	virtual
	~cofbucket();


	/**
	 *
	 */
	cofbucket&
	operator= (
			const cofbucket& b);

	/**
	 *
	 */
	bool
	operator== (
			const cofbucket& b);


	/**
	 *
	 */
	uint8_t*
	pack(uint8_t* bucket, size_t bclen);


	/**
	 *
	 */
	void
	unpack(uint8_t* bucket, size_t bclen);


	/**
	 *
	 */
	size_t
	length() const;


	/**
	 *
	 */
	void
	get_bucket_stats(
			cmemory& body);


	/**
	 *
	 */
	void
	check_prerequisites() const;

public:

	/**
	 *
	 */
	uint8_t
	get_version() const { return ofp_version; };

	/**
	 *
	 */
	void
	set_version(uint8_t ofp_version) { this->ofp_version = ofp_version; };

	/**
	 *
	 */
	uint64_t
	get_packet_count() const { return packet_count; };

	/**
	 *
	 */
	void
	set_packet_count(uint64_t packet_count) { this->packet_count = packet_count; };

	/**
	 *
	 */
	uint64_t
	get_byte_count() const { return byte_count; };

	/**
	 *
	 */
	void
	set_byte_count(uint64_t byte_count) { this->byte_count = byte_count; };

	/**
	 *
	 */
	void
	set_weight(uint16_t weight) { this->weight = weight; };

	/**
	 *
	 */
	uint16_t
	get_weight() const { return weight; };

	/**
	 *
	 */
	void
	set_watch_port(uint32_t watch_port) { this->watch_port = watch_port; };

	/**
	 *
	 */
	uint32_t
	get_watch_port() const { return watch_port; };

	/**
	 *
	 */
	void
	set_watch_group(uint32_t watch_group) { this->watch_group = watch_group; };

	/**
	 *
	 */
	uint32_t
	get_watch_group() const { return watch_group; };

	/**
	 *
	 */
	cofactions&
	set_actions() { return actions; };

	/**
	 *
	 */
	cofactions const&
	get_actions() const { return actions; };

private:

	/** pack bucket
	 */
	uint8_t*
	pack_of12(uint8_t* buf, size_t buflen);

	/** unpack bucket
	 */
	void
	unpack_of12(uint8_t *buf, size_t buflen);

	/** pack bucket
	 */
	uint8_t*
	pack_of13(uint8_t* buf, size_t buflen);

	/** unpack bucket
	 */
	void
	unpack_of13(uint8_t *buf, size_t buflen);

public:

	friend std::ostream&
	operator<< (std::ostream& os, cofbucket const& bucket) {
		switch (bucket.get_version()) {
		case rofl::openflow::OFP_VERSION_UNKNOWN: {
			os << indent(0) << "<cofbucket ";
				os << "ofp-version:" 	<< (int)bucket.ofp_version 	<< " >" << std::endl;
		} break;
		case rofl::openflow12::OFP_VERSION:
		case rofl::openflow13::OFP_VERSION: {
			os << indent(0) << "<cofbucket ";
				os << "ofp-version:" 	<< (int)bucket.ofp_version 	<< " >" << std::endl;
				os << std::hex;
				os << indent(2) << "<weight: 0x" 		<< (int)bucket.weight 	<< " >" << std::endl;
				os << indent(2) << "<watch-group: 0x" 	<< (int)bucket.watch_group 	<< " >" << std::endl;
				os << indent(2) << "<watch-port: 0x" 	<< (int)bucket.watch_port 	<< " >" << std::endl;
				os << indent(2) << "<packet-count: 0x"	<< (int)bucket.packet_count << " >" << std::endl;
				os << indent(2) << "<byte-count: 0x" 	<< (int)bucket.byte_count 	<< " >" << std::endl;
				os << std::dec;
				os << indent(2) << "<actions: >"	<< std::endl;
				indent i(4);
				os << bucket.actions;

		} break;
		default: {
			throw eBadVersion();
		};
		}
		return os;
	};
};

}; // end of namespace
}; // end of namespace

#endif
