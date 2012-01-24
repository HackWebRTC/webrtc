/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>

#include "receiver_fec.h"
#include "rtp_receiver_video.h"
#include "rtp_utility.h"

// RFC 5109
namespace webrtc {
ReceiverFEC::ReceiverFEC(const WebRtc_Word32 id, RTPReceiverVideo* owner)
    : _owner(owner),
      _fec(new ForwardErrorCorrection(id)),
      _payloadTypeFEC(-1),
      _lastFECSeqNum(0),
      _frameComplete(true) {
}

ReceiverFEC::~ReceiverFEC() {
  // Clean up DecodeFEC()
  while (!_receivedPacketList.empty()){
    ForwardErrorCorrection::ReceivedPacket* receivedPacket =
        _receivedPacketList.front();
    delete receivedPacket->pkt;
    delete receivedPacket;
    _receivedPacketList.pop_front();
  }
  assert(_receivedPacketList.empty());

  if (_fec != NULL) {
    bool frameComplete = true;
    _fec->DecodeFEC(&_receivedPacketList, &_recoveredPacketList,_lastFECSeqNum,
                    frameComplete);
    delete _fec;
  }
}

void ReceiverFEC::SetPayloadTypeFEC(const WebRtc_Word8 payloadType) {
  _payloadTypeFEC = payloadType;
}

/*
    0                   1                    2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3  4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |F|   block PT  |  timestamp offset         |   block length    |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+


RFC 2198          RTP Payload for Redundant Audio Data    September 1997

   The bits in the header are specified as follows:

   F: 1 bit First bit in header indicates whether another header block
       follows.  If 1 further header blocks follow, if 0 this is the
       last header block.
       If 0 there is only 1 byte RED header

   block PT: 7 bits RTP payload type for this block.

   timestamp offset:  14 bits Unsigned offset of timestamp of this block
       relative to timestamp given in RTP header.  The use of an unsigned
       offset implies that redundant data must be sent after the primary
       data, and is hence a time to be subtracted from the current
       timestamp to determine the timestamp of the data for which this
       block is the redundancy.

   block length:  10 bits Length in bytes of the corresponding data
       block excluding header.
*/

WebRtc_Word32 ReceiverFEC::AddReceivedFECPacket(
    const WebRtcRTPHeader* rtpHeader,
    const WebRtc_UWord8* incomingRtpPacket,
    const WebRtc_UWord16 payloadDataLength,
    bool& FECpacket,
    bool oldPacket) {
  if (_payloadTypeFEC == -1) {
    return -1;
  }

  WebRtc_UWord8 REDHeaderLength = 1;

  // Add to list without RED header, aka a virtual RTP packet
  // we remove the RED header

  ForwardErrorCorrection::ReceivedPacket* receivedPacket =
      new ForwardErrorCorrection::ReceivedPacket;
  receivedPacket->pkt = new ForwardErrorCorrection::Packet;

  // get payload type from RED header
  WebRtc_UWord8 payloadType =
      incomingRtpPacket[rtpHeader->header.headerLength] & 0x7f;

  // use the payloadType to decide if it's FEC or coded data
  if(_payloadTypeFEC == payloadType) {
    receivedPacket->isFec = true;
    FECpacket = true;
    // We don't need to parse old FEC packets.
    // Old FEC packets are sent to jitter buffer as empty packets in the
    // callback in rtp_receiver_video.
    if (oldPacket)  return 0;
  } else {
    receivedPacket->isFec = false;
    FECpacket = false;
  }
  receivedPacket->lastMediaPktInFrame = rtpHeader->header.markerBit;
  receivedPacket->seqNum = rtpHeader->header.sequenceNumber;

  WebRtc_UWord16 blockLength = 0;
  if(incomingRtpPacket[rtpHeader->header.headerLength] & 0x80) {
    // f bit set in RED header
    REDHeaderLength = 4;
    WebRtc_UWord16 timestampOffset =
        (incomingRtpPacket[rtpHeader->header.headerLength + 1]) << 8;
    timestampOffset += incomingRtpPacket[rtpHeader->header.headerLength+2];
    timestampOffset = timestampOffset >> 2;
    if(timestampOffset != 0) {
      // timestampOffset should be 0, however, this is a valid error case in
      // the event of garbage payload.
      return -1;
    }

    blockLength =
        (0x03 & incomingRtpPacket[rtpHeader->header.headerLength + 2]) << 8;
    blockLength += (incomingRtpPacket[rtpHeader->header.headerLength + 3]);

    // check next RED header
    if(incomingRtpPacket[rtpHeader->header.headerLength+4] & 0x80) {
      // more than 2 blocks in packet not supported
      assert(false);
      return -1;
    }
    if(blockLength > payloadDataLength - REDHeaderLength) {
      // block length longer than packet
      assert(false);
      return -1;
    }
  }

  ForwardErrorCorrection::ReceivedPacket* secondReceivedPacket = NULL;
  if (blockLength > 0) {
    // handle block length, split into 2 packets
    REDHeaderLength = 5;

    // copy the RTP header
    memcpy(receivedPacket->pkt->data,
           incomingRtpPacket,
           rtpHeader->header.headerLength);

    // replace the RED payload type
    receivedPacket->pkt->data[1] &= 0x80;         // reset the payload
    receivedPacket->pkt->data[1] += payloadType;  // set the media payload type

    // copy the payload data
    memcpy(receivedPacket->pkt->data + rtpHeader->header.headerLength,
           incomingRtpPacket + rtpHeader->header.headerLength + REDHeaderLength,
           blockLength);

    receivedPacket->pkt->length = blockLength;

    secondReceivedPacket = new ForwardErrorCorrection::ReceivedPacket;
    secondReceivedPacket->pkt = new ForwardErrorCorrection::Packet;

    secondReceivedPacket->isFec = true;
    secondReceivedPacket->lastMediaPktInFrame = false;
    secondReceivedPacket->seqNum = rtpHeader->header.sequenceNumber;

    // copy the FEC payload data
    memcpy(secondReceivedPacket->pkt->data,
           incomingRtpPacket + rtpHeader->header.headerLength +
               REDHeaderLength + blockLength,
           payloadDataLength - REDHeaderLength - blockLength);

    secondReceivedPacket->pkt->length = payloadDataLength - REDHeaderLength -
        blockLength;

  } else if(receivedPacket->isFec) {
    // everything behind the RED header
    memcpy(receivedPacket->pkt->data,
           incomingRtpPacket + rtpHeader->header.headerLength + REDHeaderLength,
           payloadDataLength - REDHeaderLength);
    receivedPacket->pkt->length = payloadDataLength - REDHeaderLength;
    receivedPacket->ssrc =
        ModuleRTPUtility::BufferToUWord32(&incomingRtpPacket[8]);

  } else {
    // copy the RTP header
    memcpy(receivedPacket->pkt->data,
           incomingRtpPacket,
           rtpHeader->header.headerLength);

    // replace the RED payload type
    receivedPacket->pkt->data[1] &= 0x80;         // reset the payload
    receivedPacket->pkt->data[1] += payloadType;  // set the media payload type

    // copy the media payload data
    memcpy(receivedPacket->pkt->data + rtpHeader->header.headerLength,
           incomingRtpPacket + rtpHeader->header.headerLength + REDHeaderLength,
           payloadDataLength - REDHeaderLength);

    receivedPacket->pkt->length = rtpHeader->header.headerLength +
        payloadDataLength - REDHeaderLength;
  }

  if(receivedPacket->isFec) {
    AddReceivedFECInfo(rtpHeader, NULL, FECpacket);
  }

  if(receivedPacket->pkt->length == 0) {
    delete receivedPacket->pkt;
    delete receivedPacket;
    return 0;
  }

  // Send any old media packets to jitter buffer, don't push them onto
  // received list for FEC decoding (we don't do FEC decoding on old packets).
  if (oldPacket && receivedPacket->isFec == false) {
    if (ParseAndReceivePacket(receivedPacket->pkt) != 0) {
      return -1;
    }

    delete receivedPacket->pkt;
    delete receivedPacket;
  } else {
    _receivedPacketList.push_back(receivedPacket);
    if (secondReceivedPacket) {
      _receivedPacketList.push_back(secondReceivedPacket);
    }
  }
  return 0;
}

void ReceiverFEC::AddReceivedFECInfo(const WebRtcRTPHeader* rtpHeader,
                                     const WebRtc_UWord8* incomingRtpPacket,
                                     bool& FECpacket) {
  // store the highest FEC seq num received
  if (_lastFECSeqNum >= rtpHeader->header.sequenceNumber) {
    if (_lastFECSeqNum > 0xff00 && rtpHeader->header.sequenceNumber < 0x0ff ) {
      // wrap
      _lastFECSeqNum = rtpHeader->header.sequenceNumber;
    } else {
      // old seqNum
    }
  } else {
    // check for a wrap
    if(rtpHeader->header.sequenceNumber > 0xff00 && _lastFECSeqNum < 0x0ff ) {
      // old seqNum
    } else {
      _lastFECSeqNum = rtpHeader->header.sequenceNumber;
    }
  }

  if (incomingRtpPacket) {
    // get payload type from RED header
    WebRtc_UWord8 payloadType =
        incomingRtpPacket[rtpHeader->header.headerLength] & 0x7f;

    // use the payloadType to decide if it's FEC or coded data
    if(_payloadTypeFEC == payloadType) {
      FECpacket = true;
    } else {
      FECpacket = false;
    }
  }
}

WebRtc_Word32 ReceiverFEC::ProcessReceivedFEC(const bool forceFrameDecode) {
  if (!_receivedPacketList.empty()) {
    if (_fec->DecodeFEC(&_receivedPacketList, &_recoveredPacketList,
                        _lastFECSeqNum, _frameComplete) != 0) {
      return -1;
    }
    assert(_receivedPacketList.empty());
  }
  if (forceFrameDecode) {
    _frameComplete = true;
  }
  if (_frameComplete) {
    while (!_recoveredPacketList.empty()) {
      ForwardErrorCorrection::RecoveredPacket* recoveredPacket =
          _recoveredPacketList.front();

      if (ParseAndReceivePacket(recoveredPacket->pkt) != 0) {
        return -1;
      }
      delete recoveredPacket->pkt;
      delete recoveredPacket;
      _recoveredPacketList.pop_front();
    }
    assert(_recoveredPacketList.empty());
  }
  return 0;
}

int ReceiverFEC::ParseAndReceivePacket(
    const ForwardErrorCorrection::Packet* packet) {
  WebRtcRTPHeader header;
  memset(&header, 0, sizeof(header));
  ModuleRTPUtility::RTPHeaderParser parser(packet->data,
                                           packet->length);
  if (!parser.Parse(header)) {
    return -1;
  }
  if (_owner->ReceiveRecoveredPacketCallback(
      &header,
      &packet->data[header.header.headerLength],
      packet->length - header.header.headerLength) != 0) {
    return -1;
  }
  return 0;
}
} // namespace webrtc
