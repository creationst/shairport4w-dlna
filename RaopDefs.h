/* Copyright (c) 2012  Eric Milles <eric.milles@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef RAOPDefs_h
#define RAOPDefs_h

//#include "NTPTimestamp.h"
//#include "Platform.h"
//#include <Poco/ByteOrder.h>


#define RTP_BASE_HEADER_SIZE    sizeof(RTPPacketHeader)
#define RTP_DATA_HEADER_SIZE    sizeof(DataPacketHeader)
#define RTP_SYNC_PACKET_SIZE    sizeof(SyncPacket)
#define RTP_TIMING_PACKET_SIZE  sizeof(TimingPacket)
#define RTP_RESEND_REQUEST_SIZE sizeof(ResendRequestPacket)

#define EXTENSION_MASK 0x10
#define MARKER_MASK    0x80
#define PAYLOAD_MASK   0x7F

#define PAYLOAD_TYPE_STREAM_DATA     0x60
#define PAYLOAD_TYPE_STREAM_SYNC     0x54
#define PAYLOAD_TYPE_TIMING_REQUEST  0x52
#define PAYLOAD_TYPE_TIMING_RESPONSE 0x53
#define PAYLOAD_TYPE_RESEND_REQUEST  0x55
#define PAYLOAD_TYPE_RESEND_RESPONSE 0x56


/** number of samples per second for RAOP audio data */
extern const unsigned int RAOP_SAMPLES_PER_SECOND;

/** number of bits per sample for RAOP audio data */
extern const unsigned int RAOP_BITS_PER_SAMPLE;

/** number of channels for RAOP audio data */
extern const unsigned int RAOP_CHANNEL_COUNT;

/** maximum number of samples per channel in an RAOP audio data packet */
extern const unsigned int RAOP_PACKET_MAX_SAMPLES_PER_CHANNEL;

/** maximum number of bytes in an RAOP audio data packet */
extern const size_t RAOP_PACKET_MAX_DATA_SIZE;

/** maximum number of bytes in an RAOP audio data packet including headers */
extern const size_t RAOP_PACKET_MAX_SIZE;


//------------------------------------------------------------------------------


struct RTPPacketHeader
{
	//byte_t version     : 2;
	//byte_t padding     : 1;
	//byte_t extension   : 1;
	//byte_t csrcCount   : 4;
	//byte_t marker      : 1;
	//byte_t payloadType : 7;

	byte header0;
	byte header1;
	uint16_t seqNum;

	RTPPacketHeader()
	:
		header0(0x80),
		header1(0x00),
		seqNum(0)
	{
	}

	bool getExtension() const
	{
		return ((header0 & EXTENSION_MASK) != 0);
	}

	void setExtension(const bool value = true)
	{
		header0 = (value ? (header0 | EXTENSION_MASK) : (header0 & ~EXTENSION_MASK));
	}

	bool getMarker() const
	{
		return ((header1 & MARKER_MASK) != 0);
	}

	void setMarker(const bool value = true)
	{
		header1 = (value ? (header1 | MARKER_MASK) : (header1 & ~MARKER_MASK));
	}

	byte getPayloadType() const
	{
		return (header1 & PAYLOAD_MASK);
	}

	void setPayloadType(const byte value)
	{
		header1 = (header1 & ~PAYLOAD_MASK) | (value & PAYLOAD_MASK);
	}
};


struct DataPacketHeader : RTPPacketHeader
{
	uint32_t rtpTime;
	uint32_t ssrc;
};

//
//struct SyncPacket : RTPPacketHeader
//{
//	uint32_t rtpTimeLessLatency;
//	NTPTimestamp lastSyncTime;
//	uint32_t rtpTime;
//};
//
//
//struct TimingPacket : RTPPacketHeader
//{
//	uint32_t padding;
//	NTPTimestamp referenceTime;
//	NTPTimestamp receivedTime;
//	NTPTimestamp sendTime;
//
//	TimingPacket() : padding(0) {}
//};
//
//
//struct ResendRequestPacket : RTPPacketHeader
//{
//	uint16_t missedSeqNum;
//	uint16_t missedPktCnt;
//};
//
//
////------------------------------------------------------------------------------
//
//
//inline void ByteOrder_toNetwork(DataPacketHeader& header)
//{
//	header.seqNum = Poco::ByteOrder::toNetwork(header.seqNum);
//	header.rtpTime = Poco::ByteOrder::toNetwork(header.rtpTime);
//	header.ssrc = Poco::ByteOrder::toNetwork(header.ssrc);
//}
//
//
//inline void ByteOrder_toNetwork(SyncPacket& packet)
//{
//	packet.seqNum = Poco::ByteOrder::toNetwork(packet.seqNum);
//	packet.rtpTime = Poco::ByteOrder::toNetwork(packet.rtpTime);
//	packet.rtpTimeLessLatency = Poco::ByteOrder::toNetwork(packet.rtpTimeLessLatency);
//	packet.lastSyncTime = ByteOrder_toNetwork(packet.lastSyncTime);
//}
//
//
//inline void ByteOrder_fromNetwork(TimingPacket& packet)
//{
//	packet.seqNum = Poco::ByteOrder::fromNetwork(packet.seqNum);
//	packet.referenceTime = ByteOrder_fromNetwork(packet.referenceTime);
//	packet.receivedTime = ByteOrder_fromNetwork(packet.receivedTime);
//	packet.sendTime = ByteOrder_fromNetwork(packet.sendTime);
//}
//
//
//inline void ByteOrder_toNetwork(TimingPacket& packet)
//{
//	packet.seqNum = Poco::ByteOrder::toNetwork(packet.seqNum);
//	packet.referenceTime = ByteOrder_toNetwork(packet.referenceTime);
//	packet.receivedTime = ByteOrder_toNetwork(packet.receivedTime);
//	packet.sendTime = ByteOrder_toNetwork(packet.sendTime);
//}
//
//
//inline void ByteOrder_fromNetwork(ResendRequestPacket& packet)
//{
//	packet.seqNum = Poco::ByteOrder::fromNetwork(packet.seqNum);
//	packet.missedSeqNum = Poco::ByteOrder::fromNetwork(packet.missedSeqNum);
//	packet.missedPktCnt = Poco::ByteOrder::fromNetwork(packet.missedPktCnt);
//}


#endif // RAOPDefs_h
