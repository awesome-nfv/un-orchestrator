/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ftcpframe.h"

using namespace rofl;

ftcpframe::ftcpframe(
		uint8_t* data,
		size_t datalen) :
		fframe(data, datalen),
		tcp_hdr(0),
		data(0),
		datalen(0)
{
	initialize();
}



ftcpframe::~ftcpframe()
{

}



void
ftcpframe::initialize()
{
	tcp_hdr = (struct tcp_hdr_t*)soframe();
	if (framelen() > sizeof(struct tcp_hdr_t))
	{
		data = soframe() + (tcp_hdr->offset * sizeof(uint32_t));
		datalen = framelen() - (tcp_hdr->offset * sizeof(uint32_t));
	}
	else
	{
		data = NULL;
		datalen = 0;
	}
}


bool
ftcpframe::complete() const
{
	//initialize();

	if (framelen() < sizeof(struct tcp_hdr_t))
		return false;

	if (framelen() < (tcp_hdr->offset * sizeof(uint32_t)))
		return false;

	return true;
}


size_t
ftcpframe::need_bytes() const
{
	if (complete())
		return 0;

	if (framelen() < sizeof(struct tcp_hdr_t))
		return (sizeof(struct tcp_hdr_t) - framelen());

	if (framelen() < (tcp_hdr->offset * sizeof(uint32_t)))
		return ((tcp_hdr->offset * sizeof(uint32_t)) - framelen());

	return 0; // just to make gcc happy
}


void
ftcpframe::payload_insert(
		uint8_t *data,
		size_t datalen) throw (eFrameOutOfRange)
{
	initialize();

	if (datalen > (framelen() - (tcp_hdr->offset * sizeof(uint32_t))))
	{
		throw eFrameOutOfRange();
	}
	memcpy(this->data, data, datalen);
	this->datalen = datalen;
}


uint8_t*
ftcpframe::payload() const throw (eFrameNoPayload)
{
	//initialize(); // commented out 2012-12-13
	if (!data)
		throw eFrameNoPayload();
	return data;
}


size_t
ftcpframe::payloadlen() const throw (eFrameNoPayload)
{
	//initialize(); // commented out 2012-12-13
	if (!datalen)
		throw eFrameNoPayload();
	return datalen;
}


void
ftcpframe::validate(uint16_t total_len) const
{
	//initialize();

	if (!complete())
	{
		throw eTcpFrameTooShort();
	}

	// TODO: verify checksum here?
}


void
ftcpframe::tcp_calc_checksum(
		caddress_in4 const& ip_src,
		caddress_in4 const& ip_dst,
		uint8_t ip_proto,
		uint16_t length)
{
	int wnum;
	uint32_t sum = 0; //sum
	uint16_t* word16;
	
	//Set 0 to checksum
	tcp_hdr->checksum = 0x0;

	/*
	* part -I- (IPv4 pseudo header)
	*/
	
	word16 = (uint16_t*)(void*)ip_src.somem();
	sum += *(word16+1);
	sum += *(word16);

	word16 = (uint16_t*)(void*)ip_dst.somem();
	sum += *(word16+1);
	sum += *(word16);
	sum += htons(ip_proto);
	
	sum += htons(length); 

	/*
	* part -II- (TCP header + payload)
	*/
	
	// pointer on 16bit words
	// number of 16bit words
	word16 = (uint16_t*)tcp_hdr;
	wnum = (length / sizeof(uint16_t));

	for (int i = 0; i < wnum; i++){
		sum += (uint32_t)word16[i];
	}
	
	if(length & 0x1)
		//Last byte
		sum += (uint32_t)( ((uint8_t*)(void*)tcp_hdr)[length-1]);

	//Fold it
	do{
		sum = (sum & 0xFFFF)+(sum >> 16);
	}while (sum >> 16); 

	tcp_hdr->checksum =(uint16_t) ~sum;

//	fprintf(stderr," %x \n", tcp_hdr->checksum);
}


uint16_t
ftcpframe::get_sport() const
{
	return be16toh(tcp_hdr->sport);
}


void
ftcpframe::set_sport(uint16_t port)
{
	tcp_hdr->sport = htobe16(port);
}


uint16_t
ftcpframe::get_dport() const
{
	return be16toh(tcp_hdr->dport);
}


void
ftcpframe::set_dport(uint16_t port)
{
	tcp_hdr->dport = htobe16(port);
}


