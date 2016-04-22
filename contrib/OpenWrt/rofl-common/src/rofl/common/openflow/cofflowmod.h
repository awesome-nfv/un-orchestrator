/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef COFFLOWMOD_H
#define COFFLOWMOD_H 1

#include <string>
#include <vector>
#include <algorithm>
#include <endian.h>
#include <strings.h>

#include "rofl/common/croflexception.h"
#include "rofl/common/cmemory.h"
#include "rofl/common/caddress.h"
#include "rofl/common/openflow/cofmatch.h"
#include "rofl/common/openflow/cofaction.h"
#include "rofl/common/openflow/cofinstruction.h"
#include "rofl/common/openflow/cofinstructions.h"
#include "rofl/common/openflow/coxmatch.h"

namespace rofl {
namespace openflow {

class eFlowModBase 		: public RoflException {}; // error base class cflowentry
class eFlowModInvalid 	: public eFlowModBase {}; // invalid parameter
class eFlowModOutOfMem 	: public eFlowModBase {}; // out of memory


class cofflowmod {
public:

	/**
	 *
	 */
	cofflowmod(
			uint8_t ofp_version = rofl::openflow::OFP_VERSION_UNKNOWN);

	/**
	 *
	 */
	virtual
	~cofflowmod();

	/**
	 *
	 */
	cofflowmod(
			const cofflowmod& fe);

	/**
	 *
	 */
	cofflowmod&
	operator= (
			const cofflowmod& fe);

	/**
	 *
	 */
	void
	clear();

	/**
	 *
	 */
	void
	check_prerequisites() const;

public: // setter methods for ofp_flow_mod structure

	/**
	 *
	 */
	void
	set_version(uint8_t ofp_version) {
		this->ofp_version = ofp_version;
		match.set_version(ofp_version);
		actions.set_version(ofp_version);
		instructions.set_version(ofp_version);
	};

	/**
	 *
	 */
	uint8_t
	get_version() const { return ofp_version; };

	/**
	 *
	 */
	rofl::openflow::cofmatch&
	set_match() { return match; };

	/**
	 *
	 */
	const rofl::openflow::cofmatch&
	get_match() const { return match; };

	/**
	 *
	 */
	rofl::openflow::cofactions&
	set_actions() { return actions; };

	/**
	 *
	 */
	const rofl::openflow::cofactions&
	get_actions() const { return actions; };

	/**
	 *
	 */
	rofl::openflow::cofinstructions&
	set_instructions() { return instructions; };

	/**
	 *
	 */
	const rofl::openflow::cofinstructions&
	get_instructions() const { return instructions; };

	/**
	 *
	 */
	void
	set_command(uint8_t command) { this->command = command; };

	/**
	 *
	 */
	uint8_t
	get_command() const { return command; };

	/**
	 *
	 */
	void
	set_table_id(uint8_t table_id) { this->table_id = table_id; }

	/**
	 */
	uint8_t
	get_table_id() const { return table_id; };

	/**
	 *
	 */
	void
	set_idle_timeout(uint16_t idle_timeout) { this->idle_timeout = idle_timeout; };

	/**
	 *
	 */
	uint16_t
	get_idle_timeout() const { return idle_timeout; };

	/**
	 *
	 */
	void
	set_hard_timeout(uint16_t hard_timeout) { this->hard_timeout = hard_timeout; };

	/**
	 */
	uint16_t
	get_hard_timeout() const { return hard_timeout; };

	/**
	 *
	 */
	void
	set_cookie(uint64_t cookie) { this->cookie = cookie; };

	/**
	 *
	 */
	uint64_t
	get_cookie() const { return cookie; };

	/**
	 *
	 */
	void
	set_cookie_mask(uint64_t cookie_mask) { this->cookie_mask = cookie_mask; };

	/**
	 *
	 */
	uint64_t
	get_cookie_mask() const { return cookie_mask; };

	/**
	 *
	 */
	void
	set_priority(uint16_t priority) { this->priority = priority; };

	/**
	 *
	 */
	uint16_t
	get_priority() const { return priority; };

	/**
	 *
	 */
	void
	set_buffer_id(uint32_t buffer_id) { this->buffer_id = buffer_id; };

	/**
	 *
	 */
	uint32_t
	get_buffer_id() const { return buffer_id; };

	/**
	 *
	 */
	void
	set_out_port(uint32_t out_port) { this->out_port = out_port; };

	/**
	 *
	 */
	uint32_t
	get_out_port() const { return out_port; };

	/**
	 *
	 */
	void
	set_out_group(uint32_t out_group) { this->out_group = out_group; };

	/**
	 *
	 */
	uint32_t
	get_out_group() const { return out_group; };

	/**
	 *
	 */
	void
	set_flags(uint16_t flags) { this->flags = flags; };

	/**
	 *
	 */
	uint16_t
	get_flags() const { return flags; };

public:

	/**
	 *
	 */
	virtual size_t
	length() const;

	/**
	 *
	 */
	virtual void
	pack(
			uint8_t *buf, size_t buflen);

	/**
	 *
	 */
	virtual void
	unpack(
			uint8_t *buf, size_t buflen);


public:

	friend std::ostream&
	operator<< (std::ostream& os, cofflowmod const& fe) {
		os << rofl::indent(0) << "<cofflowmod ofp-version:" << (int)fe.get_version() << " >" << std::endl;

		switch (fe.get_command()) {
		case rofl::openflow::OFPFC_ADD:
			os << rofl::indent(2) << "<command: ADD >" 			<< std::endl; break;
		case rofl::openflow::OFPFC_DELETE:
			os << rofl::indent(2) << "<command: DELETE >" 		<< std::endl; break;
		case rofl::openflow::OFPFC_DELETE_STRICT:
			os << rofl::indent(2) << "<command: DELETE-STRICT >"<< std::endl; break;
		case rofl::openflow::OFPFC_MODIFY:
			os << rofl::indent(2) << "<command: MODIFY >" 		<< std::endl; break;
		case rofl::openflow::OFPFC_MODIFY_STRICT:
			os << rofl::indent(2) << "<command: MODIFY-STRICT >"<< std::endl; break;
		default:
			os << rofl::indent(2) << "<command: UNKNOWN >" 		<< std::endl; break;
		}

		os << std::hex;
		os << rofl::indent(2) << "<cookie: 0x" 		<< (unsigned long long)fe.get_cookie() 		<< " >" << std::endl;
		os << rofl::indent(2) << "<cookie-mask: 0x"	<< (unsigned long long)fe.get_cookie_mask()	<< " >" << std::endl;
		os << std::dec;
		os << rofl::indent(2) << "<table-id: " 		<< (unsigned int)fe.get_table_id() 		<< " >" << std::endl;
		os << rofl::indent(2) << "<idle-timeout: " 	<< (unsigned int)fe.get_idle_timeout() 	<< " >" << std::endl;
		os << rofl::indent(2) << "<hard-timeout: " 	<< (unsigned int)fe.get_hard_timeout() 	<< " >" << std::endl;

		os << std::hex;
		os << rofl::indent(2) << "<priority: 0x" 	<< (unsigned int)fe.get_priority() 		<< " >" << std::endl;
		os << rofl::indent(2) << "<buffer-id: 0x"	<< (unsigned int)fe.get_buffer_id() 	<< " >" << std::endl;
		os << rofl::indent(2) << "<out-port: 0x" 	<< (unsigned int)fe.get_out_port() 		<< " >" << std::endl;
		os << rofl::indent(2) << "<out-group: 0x"	<< (unsigned int)fe.get_out_group() 	<< " >" << std::endl;
		os << rofl::indent(2) << "<flags: 0x" 		<< (unsigned int)fe.get_flags() 		<< " >" << std::endl;
		os << std::dec;

		rofl::indent i(2);
		os << fe.get_match();

		switch (fe.get_version()) {
		case rofl::openflow10::OFP_VERSION: {
			os << fe.get_actions();
		} break;
		case rofl::openflow12::OFP_VERSION:
		case rofl::openflow13::OFP_VERSION: {
			os << fe.get_instructions();
		} break;
		}

		return os;
	};

private: // data structures

	uint8_t 			ofp_version;

	cofmatch 			match;
	cofactions			actions;	// for OFP 1.0 only
	cofinstructions 	instructions;

	uint8_t				command;
	uint8_t				table_id;
	uint16_t			idle_timeout;
	uint16_t			hard_timeout;
	uint64_t			cookie;
	uint64_t			cookie_mask;
	uint16_t 			priority;
	uint32_t			buffer_id;
	uint32_t			out_port;
	uint32_t			out_group;
	uint16_t			flags;

	struct ofp10_flow_mod {
		struct rofl::openflow10::ofp_match match;
		uint64_t cookie;
		uint16_t command;
		uint16_t idle_timeout;
		uint16_t hard_timeout;
		uint16_t priority;
		uint32_t buffer_id;
		uint16_t out_port;
		uint16_t flags;
		uint8_t actions[0];
	} __attribute__((packed));

	struct ofp13_flow_mod {
		uint64_t cookie;
		uint64_t cookie_mask;
		uint8_t table_id;
		uint8_t command;
		uint16_t idle_timeout;
		uint16_t hard_timeout;
		uint16_t priority;
		uint32_t buffer_id;
		uint32_t out_port;
		uint32_t out_group;
		uint16_t flags;
		uint8_t pad[2];
		uint8_t match[0];
		//uint8_t instructions[0];
	} __attribute__((packed));
};

}; // end of namespace openflow
}; // end of namespace rofl

#endif
