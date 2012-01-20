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
#include <cstring>

#include "forward_error_correction.h"
#include "forward_error_correction_internal.h"
#include "rtp_utility.h"
#include "trace.h"

namespace webrtc {

// Minimum RTP header size in bytes.
const uint8_t kRtpHeaderSize = 12;

// FEC header size in bytes.
const uint8_t kFecHeaderSize = 10;

// ULP header size in bytes (L bit is set).
const uint8_t kUlpHeaderSizeLBitSet = (2 + kMaskSizeLBitSet);

// ULP header size in bytes (L bit is cleared).
const uint8_t kUlpHeaderSizeLBitClear = (2 + kMaskSizeLBitClear);

// Transport header size in bytes. Assume UDP/IPv4 as a reasonable minimum.
const uint8_t kTransportOverhead = 28;

// Used to link media packets to their protecting FEC packets.
//
struct ProtectedPacket {
  uint16_t seqNum;
  ForwardErrorCorrection::Packet* pkt;
};

//
// Used for internal storage of FEC packets in a list.
//
struct FecPacket {
    std::list<ProtectedPacket*> protectedPktList;
    uint16_t seqNum;
    uint32_t ssrc;  // SSRC of the current frame.
    ForwardErrorCorrection::Packet* pkt;
};

bool ForwardErrorCorrection::CompareRecoveredPackets(RecoveredPacket* first,
                                                     RecoveredPacket* second) {
  if ((first->seqNum > second->seqNum &&
      (first->seqNum - kMaxMediaPackets) < second->seqNum) ||
      // We have a wrap in sequence number if the first sequence number is low,
      // defined and lower than kMaxMediaPackets and second sequence number is
      // high defined as max sequence number (65535) - kMaxMediaPackets.
      (first->seqNum < kMaxMediaPackets &&   // Wrap guard.
          second->seqNum > (65535 - kMaxMediaPackets))) {
    return false;
  }
  return true;
}

ForwardErrorCorrection::ForwardErrorCorrection(int32_t id)
    : _id(id),
      _generatedFecPackets(kMaxMediaPackets),
      _seqNumBase(0),
      _lastMediaPacketReceived(false),
      _fecPacketReceived(false) {
}

ForwardErrorCorrection::~ForwardErrorCorrection() {
}

// Input packet
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                    RTP Header (12 octets)                     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                         RTP Payload                           |
//   |                                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

// Output packet
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                    FEC Header (10 octets)                     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                      FEC Level 0 Header                       |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                     FEC Level 0 Payload                       |
//   |                                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
int32_t ForwardErrorCorrection::GenerateFEC(
    const std::list<Packet*>& mediaPacketList,
    uint8_t protectionFactor,
    int numImportantPackets,
    bool useUnequalProtection,
    std::list<Packet*>* fecPacketList) {
  if (mediaPacketList.empty()) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s media packet list is empty", __FUNCTION__);
    return -1;
  }
  if (!fecPacketList->empty()) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s FEC packet list is not empty", __FUNCTION__);
    return -1;
  }
  const uint16_t numMediaPackets = mediaPacketList.size();
  const uint8_t lBit = numMediaPackets > 16 ? 1 : 0;
  const uint16_t numMaskBytes = (lBit == 1)?
      kMaskSizeLBitSet : kMaskSizeLBitClear;

  if (numMediaPackets > kMaxMediaPackets) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "%s can only protect %d media packets per frame; %d requested",
                 __FUNCTION__, kMaxMediaPackets, numMediaPackets);
    return -1;
  }

  // Error checking on the number of important packets.
  // Can't have more important packets than media packets.
  if (numImportantPackets > numMediaPackets) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
        "Number of important packets (%d) greater than number of media "
        "packets (%d)", numImportantPackets, numMediaPackets);
    return -1;
  }
  if (numImportantPackets < 0) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                 "Number of important packets (%d) less than zero",
                 numImportantPackets);
    return -1;
  }
  // Do some error checking on the media packets.
  std::list<Packet*>::const_iterator mediaListIt = mediaPacketList.begin();
  while (mediaListIt != mediaPacketList.end()) {
    Packet* mediaPacket = *mediaListIt;
    assert(mediaPacket);

    if (mediaPacket->length < kRtpHeaderSize) {
      WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                   "%s media packet (%d bytes) is smaller than RTP header",
                   __FUNCTION__, mediaPacket->length);
      return -1;
    }

    // Ensure our FEC packets will fit in a typical MTU.
    if (mediaPacket->length + PacketOverhead() + kTransportOverhead >
        IP_PACKET_SIZE) {
      WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
          "%s media packet (%d bytes) with overhead is larger than MTU(%d)",
          __FUNCTION__, mediaPacket->length, IP_PACKET_SIZE);
      return -1;
    }
    mediaListIt++;
  }
  // Result in Q0 with an unsigned round.
  uint32_t numFecPackets = (numMediaPackets * protectionFactor + (1 << 7)) >> 8;
  // Generate at least one FEC packet if we need protection.
  if (protectionFactor > 0 && numFecPackets == 0) {
    numFecPackets = 1;
  }
  if (numFecPackets == 0) {
    return 0;
  }
  assert(numFecPackets <= numMediaPackets);

  // Prepare FEC packets by setting them to 0.
  for (uint32_t i = 0; i < numFecPackets; i++) {
    memset(_generatedFecPackets[i].data, 0, IP_PACKET_SIZE);
    _generatedFecPackets[i].length = 0;  // Use this as a marker for untouched
    // packets.
    fecPacketList->push_back(&_generatedFecPackets[i]);
  }

  // -- Generate packet masks --
  uint8_t* packetMask = new uint8_t[numFecPackets * numMaskBytes];
  memset(packetMask, 0, numFecPackets * numMaskBytes);
  internal::GeneratePacketMasks(numMediaPackets, numFecPackets,
                                numImportantPackets, useUnequalProtection,
                                packetMask);

  GenerateFecBitStrings(mediaPacketList, packetMask, numFecPackets);

  GenerateFecUlpHeaders(mediaPacketList, packetMask, numFecPackets);

  delete [] packetMask;
  return 0;
}

void ForwardErrorCorrection::GenerateFecBitStrings(
    const std::list<Packet*>& mediaPacketList,
    uint8_t* packetMask,
    uint32_t numFecPackets) {
  uint8_t mediaPayloadLength[2];
  const uint8_t lBit = mediaPacketList.size() > 16 ? 1 : 0;
  const uint16_t numMaskBytes = (lBit == 1) ?
      kMaskSizeLBitSet : kMaskSizeLBitClear;
  const uint16_t ulpHeaderSize = (lBit == 1) ?
      kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear;
  const uint16_t fecRtpOffset = kFecHeaderSize + ulpHeaderSize - kRtpHeaderSize;

  for (uint32_t i = 0; i < numFecPackets; i++) {
    std::list<Packet*>::const_iterator mediaListIt = mediaPacketList.begin();
    uint32_t pktMaskIdx = i * numMaskBytes;
    uint32_t mediaPktIdx = 0;
    uint16_t fecPacketLength = 0;
    while (mediaListIt != mediaPacketList.end()) {
      // Each FEC packet has a multiple byte mask.
      if (packetMask[pktMaskIdx] & (1 << (7 - mediaPktIdx))) {
        Packet* mediaPacket = *mediaListIt;

        // Assign network-ordered media payload length.
        ModuleRTPUtility::AssignUWord16ToBuffer(
            mediaPayloadLength,
            mediaPacket->length - kRtpHeaderSize);

        fecPacketLength = mediaPacket->length + fecRtpOffset;
        // On the first protected packet, we don't need to XOR.
        if (_generatedFecPackets[i].length == 0) {
          // Copy the first 2 bytes of the RTP header.
          memcpy(_generatedFecPackets[i].data, mediaPacket->data, 2);
          // Copy the 5th to 8th bytes of the RTP header.
          memcpy(&_generatedFecPackets[i].data[4], &mediaPacket->data[4], 4);
          // Copy network-ordered payload size.
          memcpy(&_generatedFecPackets[i].data[8], mediaPayloadLength, 2);

          // Copy RTP payload, leaving room for the ULP header.
          memcpy(&_generatedFecPackets[i].data[kFecHeaderSize + ulpHeaderSize],
                 &mediaPacket->data[kRtpHeaderSize],
                 mediaPacket->length - kRtpHeaderSize);
        } else {
          // XOR with the first 2 bytes of the RTP header.
          _generatedFecPackets[i].data[0] ^= mediaPacket->data[0];
          _generatedFecPackets[i].data[1] ^= mediaPacket->data[1];

          // XOR with the 5th to 8th bytes of the RTP header.
          for (uint32_t j = 4; j < 8; j++) {
            _generatedFecPackets[i].data[j] ^= mediaPacket->data[j];
          }

          // XOR with the network-ordered payload size.
          _generatedFecPackets[i].data[8] ^= mediaPayloadLength[0];
          _generatedFecPackets[i].data[9] ^= mediaPayloadLength[1];

          // XOR with RTP payload, leaving room for the ULP header.
          for (int32_t j = kFecHeaderSize + ulpHeaderSize;
              j < fecPacketLength; j++) {
            _generatedFecPackets[i].data[j] ^=
                mediaPacket->data[j - fecRtpOffset];
          }
        }
        if (fecPacketLength > _generatedFecPackets[i].length) {
          _generatedFecPackets[i].length = fecPacketLength;
        }
      }
      mediaListIt++;
      mediaPktIdx++;
      if (mediaPktIdx == 8) {
        // Switch to the next mask byte.
        mediaPktIdx = 0;
        pktMaskIdx++;
      }
    }
    assert(_generatedFecPackets[i].length);
    //Note: This shouldn't happen: means packet mask is wrong or poorly designed
  }
}

void ForwardErrorCorrection::GenerateFecUlpHeaders(
    const std::list<Packet*>& mediaPacketList,
    uint8_t* packetMask,
    uint32_t numFecPackets) {
  // -- Generate FEC and ULP headers --
  //
  // FEC Header, 10 bytes
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |E|L|P|X|  CC   |M| PT recovery |            SN base            |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |                          TS recovery                          |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |        length recovery        |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //
  // ULP Header, 4 bytes (for L = 0)
  //    0                   1                   2                   3
  //    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |       Protection Length       |             mask              |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  //   |              mask cont. (present only when L = 1)             |
  //   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  std::list<Packet*>::const_iterator mediaListIt = mediaPacketList.begin();
  Packet* mediaPacket = *mediaListIt;
  assert(mediaPacket != NULL);
  const uint8_t lBit = mediaPacketList.size() > 16 ? 1 : 0;
  const uint16_t numMaskBytes = (lBit == 1)?
      kMaskSizeLBitSet : kMaskSizeLBitClear;
  const uint16_t ulpHeaderSize = (lBit == 1)?
      kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear;

  for (uint32_t i = 0; i < numFecPackets; i++) {
    // -- FEC header --
    _generatedFecPackets[i].data[0] &= 0x7f; // Set E to zero.
    if (lBit == 0) {
      _generatedFecPackets[i].data[0] &= 0xbf; // Clear the L bit.
    } else {
      _generatedFecPackets[i].data[0] |= 0x40; // Set the L bit.
    }
    // Two byte sequence number from first RTP packet to SN base.
    // We use the same sequence number base for every FEC packet,
    // but that's not required in general.
    memcpy(&_generatedFecPackets[i].data[2], &mediaPacket->data[2], 2);

    // -- ULP header --
    // Copy the payload size to the protection length field.
    // (We protect the entire packet.)
    ModuleRTPUtility::AssignUWord16ToBuffer(&_generatedFecPackets[i].data[10],
        _generatedFecPackets[i].length - kFecHeaderSize - ulpHeaderSize);

    // Copy the packet mask.
    memcpy(&_generatedFecPackets[i].data[12], &packetMask[i * numMaskBytes],
           numMaskBytes);
  }
}

void ForwardErrorCorrection::ResetState(
    std::list<RecoveredPacket*>* recoveredPacketList) {
  _seqNumBase = 0;
  _lastMediaPacketReceived = false;
  _fecPacketReceived = false;

  // Free the memory for any existing recovered packets, if the user hasn't.
  while (!recoveredPacketList->empty()) {
    std::list<RecoveredPacket*>::iterator recoveredPacketListIt =
        recoveredPacketList->begin();
    RecoveredPacket* recPacket = *recoveredPacketListIt;
    delete recPacket->pkt;
    delete recPacket;
    recoveredPacketList->pop_front();
  }
  assert(recoveredPacketList->empty());

  // Free the FEC packet list.
  while (!_fecPacketList.empty()) {
    std::list<FecPacket*>::iterator fecPacketListIt = _fecPacketList.begin();
    FecPacket* fecPacket = *fecPacketListIt;
    std::list<ProtectedPacket*>::iterator protectedPacketListIt;
    protectedPacketListIt = fecPacket->protectedPktList.begin();
    while (protectedPacketListIt != fecPacket->protectedPktList.end()) {
      delete *protectedPacketListIt;
      protectedPacketListIt++;
      fecPacket->protectedPktList.pop_front();
    }
    assert(fecPacket->protectedPktList.empty());
    delete fecPacket->pkt;
    delete fecPacket;
    _fecPacketList.pop_front();
  }
  assert(_fecPacketList.empty());
}

void ForwardErrorCorrection::InsertMediaPacket(
    ReceivedPacket* rxPacket,
    std::list<RecoveredPacket*>* recoveredPacketList) {
  if (rxPacket->lastMediaPktInFrame) {
    if (_lastMediaPacketReceived) {
      // We already received the last packet.
      WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                   "%s last media packet marked more than once per frame",
                   __FUNCTION__);
    }
    _lastMediaPacketReceived = true;
  }
  bool duplicatePacket = false;
  std::list<RecoveredPacket*>::iterator recoveredPacketListIt =
      recoveredPacketList->begin();

  while (recoveredPacketListIt != recoveredPacketList->end()) {
    RecoveredPacket* recPacket = *recoveredPacketListIt;
    if (rxPacket->seqNum == recPacket->seqNum) {
      // Duplicate packet, no need to add to list.
      duplicatePacket = true;
      break;
    }
    recoveredPacketListIt++;
  }
  if (duplicatePacket) {
    // Delete duplicate media packet data.
    delete rxPacket->pkt;
    return;
  }
  RecoveredPacket* recoverdPacketToInsert = new RecoveredPacket;
  recoverdPacketToInsert->wasRecovered = false;
  recoverdPacketToInsert->seqNum = rxPacket->seqNum;
  recoverdPacketToInsert->pkt = rxPacket->pkt;
  recoverdPacketToInsert->pkt->length = rxPacket->pkt->length;

  recoveredPacketList->push_back(recoverdPacketToInsert);
}

void ForwardErrorCorrection::InsertFECPacket(ReceivedPacket* rxPacket) {
  _fecPacketReceived = true;

  // Check for duplicate.
  bool duplicatePacket = false;
  std::list<FecPacket*>::iterator fecPacketListIt = _fecPacketList.begin();
  while (fecPacketListIt != _fecPacketList.end()) {
    FecPacket* fecPacket = *fecPacketListIt;
    if (rxPacket->seqNum == fecPacket->seqNum) {
      duplicatePacket = true;
      break;
    }
    fecPacketListIt++;
  }
  if (duplicatePacket) {
    // Delete duplicate FEC packet data.
    delete rxPacket->pkt;
    rxPacket->pkt = NULL;
    return;
  }
  FecPacket* fecPacket = new FecPacket;
  fecPacket->pkt = rxPacket->pkt;
  fecPacket->seqNum = rxPacket->seqNum;
  fecPacket->ssrc = rxPacket->ssrc;

  // We store this for determining frame completion later.
  _seqNumBase = ModuleRTPUtility::BufferToUWord16(
      &fecPacket->pkt->data[2]);

  const uint16_t maskSizeBytes = (fecPacket->pkt->data[0] & 0x40) ?
      kMaskSizeLBitSet : kMaskSizeLBitClear; // L bit set?

  for (uint16_t byteIdx = 0; byteIdx < maskSizeBytes; byteIdx++) {
    uint8_t packetMask = fecPacket->pkt->data[12 + byteIdx];
    for (uint16_t bitIdx = 0; bitIdx < 8; bitIdx++) {
      if (packetMask & (1 << (7 - bitIdx))) {
        ProtectedPacket* protectedPacket = new ProtectedPacket;
        fecPacket->protectedPktList.push_back(protectedPacket);
        // This wraps naturally with the sequence number.
        protectedPacket->seqNum = static_cast<uint16_t>(_seqNumBase +
            (byteIdx << 3) + bitIdx);
        protectedPacket->pkt = NULL;
      }
    }
  }
  if (fecPacket->protectedPktList.empty()) {
    // All-zero packet mask; we can discard this FEC packet.
    delete fecPacket->pkt;
    delete fecPacket;
  } else {
    _fecPacketList.push_back(fecPacket);
  }
}

void ForwardErrorCorrection::InsertPackets(
    std::list<ReceivedPacket*>* receivedPacketList,
    std::list<RecoveredPacket*>* recoveredPacketList) {

  while (!receivedPacketList->empty()) {
    ReceivedPacket* rxPacket = receivedPacketList->front();

    if (rxPacket->isFec) {
      InsertFECPacket(rxPacket);
    } else {
      // Insert packet in end of list.
      InsertMediaPacket(rxPacket, recoveredPacketList);
    }
    // Delete the received packet "wrapper", but not the packet data.
    delete rxPacket;
    receivedPacketList->pop_front();
  }
  assert(receivedPacketList->empty());
  // Sort our recovered packet list.
  recoveredPacketList->sort(CompareRecoveredPackets);
}

void ForwardErrorCorrection::RecoverPacket(
    const FecPacket& fecPacket,
    RecoveredPacket* recPacketToInsert) {
  uint8_t lengthRecovery[2];
  const uint16_t ulpHeaderSize = fecPacket.pkt->data[0] & 0x40 ?
      kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear;  // L bit set?

  recPacketToInsert->wasRecovered = true;
  recPacketToInsert->pkt = new Packet;
  memset(recPacketToInsert->pkt->data, 0, IP_PACKET_SIZE);

  uint8_t protectionLength[2];
  // Copy the protection length from the ULP header.
  memcpy(&protectionLength, &fecPacket.pkt->data[10], 2);

  // Copy the first 2 bytes of the FEC header.
  memcpy(recPacketToInsert->pkt->data, fecPacket.pkt->data, 2);

  // Copy the 5th to 8th bytes of the FEC header.
  memcpy(&recPacketToInsert->pkt->data[4], &fecPacket.pkt->data[4], 4);

  // Set the SSRC field.
  ModuleRTPUtility::AssignUWord32ToBuffer(&recPacketToInsert->pkt->data[8],
                                          fecPacket.ssrc);

  // Copy the length recovery field.
  memcpy(&lengthRecovery, &fecPacket.pkt->data[8], 2);

  // Copy FEC payload, skipping the ULP header.
  memcpy(&recPacketToInsert->pkt->data[kRtpHeaderSize],
         &fecPacket.pkt->data[kFecHeaderSize + ulpHeaderSize],
         ModuleRTPUtility::BufferToUWord16(protectionLength));

  std::list<ProtectedPacket*>::const_iterator protectedPacketListIt =
      fecPacket.protectedPktList.begin();

  while (protectedPacketListIt != fecPacket.protectedPktList.end()) {
    ProtectedPacket* protectedPacket = *protectedPacketListIt;
    if (protectedPacket->pkt == NULL) {
      // This is the packet we're recovering.
      recPacketToInsert->seqNum = protectedPacket->seqNum;
    } else {
      // XOR with the first 2 bytes of the RTP header.
      for (uint32_t i = 0; i < 2; i++) {
        recPacketToInsert->pkt->data[i] ^= protectedPacket->pkt->data[i];
      }
      // XOR with the 5th to 8th bytes of the RTP header.
      for (uint32_t i = 4; i < 8; i++) {
        recPacketToInsert->pkt->data[i] ^= protectedPacket->pkt->data[i];
      }
      // XOR with the network-ordered payload size.
      uint8_t mediaPayloadLength[2];
      ModuleRTPUtility::AssignUWord16ToBuffer(
          mediaPayloadLength,
          protectedPacket->pkt->length - kRtpHeaderSize);
      lengthRecovery[0] ^= mediaPayloadLength[0];
      lengthRecovery[1] ^= mediaPayloadLength[1];

      // XOR with RTP payload.
      // TODO: Are we doing more XORs than required here?
      for (int32_t i = kRtpHeaderSize;
          i < protectedPacket->pkt->length;
          i++) {
        recPacketToInsert->pkt->data[i] ^= protectedPacket->pkt->data[i];
      }
    }
    protectedPacketListIt++;
  }
  // Set the RTP version to 2.
  recPacketToInsert->pkt->data[0] |= 0x80;  // Set the 1st bit.
  recPacketToInsert->pkt->data[0] &= 0xbf;  // Clear the 2nd bit.

  // Assume a recovered marker bit indicates the last media packet in a frame.
  if (recPacketToInsert->pkt->data[1] & 0x80) {
    if (_lastMediaPacketReceived) {
      // Multiple marker bits are illegal.
      WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
          "%s recovered media packet contains a marker bit, but the last "
          "media packet in this frame has already been marked",
          __FUNCTION__);
    }
    _lastMediaPacketReceived = true;
  }
  // Set the SN field.
  ModuleRTPUtility::AssignUWord16ToBuffer(&recPacketToInsert->pkt->data[2],
                                          recPacketToInsert->seqNum);
  // Recover the packet length.
  recPacketToInsert->pkt->length =
      ModuleRTPUtility::BufferToUWord16(lengthRecovery) + kRtpHeaderSize;
}

uint32_t ForwardErrorCorrection::NumberOfProtectedPackets(
    const FecPacket& fecPacket,
    std::list<RecoveredPacket*>* recoveredPacketList) {
  uint32_t protectedPacketsFound = 0;
  std::list<ProtectedPacket*>::const_iterator protectedPacketListIt =
      fecPacket.protectedPktList.begin();

  while (protectedPacketListIt != fecPacket.protectedPktList.end()) {
    ProtectedPacket* protectedPacket = *protectedPacketListIt;
    if (protectedPacket->pkt != NULL) {
      // We already have the required packet.
      protectedPacketsFound++;
    } else {
      // Search for the required packet.
      std::list<RecoveredPacket*>::iterator recoveredPacketListIt =
          recoveredPacketList->begin();

      while (recoveredPacketListIt != recoveredPacketList->end()) {
        RecoveredPacket* recPacket = *recoveredPacketListIt;
        recoveredPacketListIt++;
        if (protectedPacket->seqNum == recPacket->seqNum) {
          protectedPacket->pkt = recPacket->pkt;
          protectedPacketsFound++;
          break;
        }
      }
      // Since the recovered packet list is already sorted, we don't need to
      // restart at the beginning of the list unless the previous protected
      // packet wasn't found.
      if (protectedPacket->pkt == NULL) {
        recoveredPacketListIt = recoveredPacketList->begin();
      }
    }
    protectedPacketListIt++;
  }
  return protectedPacketsFound;
}

void ForwardErrorCorrection::AttemptRecover(
    std::list<RecoveredPacket*>* recoveredPacketList) {
  std::list<FecPacket*>::iterator fecPacketListIt = _fecPacketList.begin();
  while (fecPacketListIt != _fecPacketList.end()) {
    // Store this in case a discard is required.
    std::list<FecPacket*>::iterator fecPacketListItToDiscard = fecPacketListIt;

    // Search for each FEC packet's protected media packets.
    FecPacket* fecPacket = *fecPacketListIt;
    uint32_t protectedPacketsFound =
        NumberOfProtectedPackets(*fecPacket, recoveredPacketList);

    if (protectedPacketsFound == fecPacket->protectedPktList.size() - 1) {
      // Recovery possible.
      RecoveredPacket* packetToInsert = new RecoveredPacket;
      RecoverPacket(*fecPacket, packetToInsert);

      // Add recovered packet in back of list.
      recoveredPacketList->push_back(packetToInsert);

      // Sort our recovered packet list.
      recoveredPacketList->sort(CompareRecoveredPackets);

      protectedPacketsFound++;
      assert(protectedPacketsFound == fecPacket->protectedPktList.size());

      // A packet has been recovered. We need to check the FEC list again, as
      // this may allow additional packets to be recovered.
      // Restart for first FEC packet.
      fecPacketListIt = _fecPacketList.begin();
      if (_fecPacketList.begin() == fecPacketListItToDiscard) {
        // If we're deleting the first item, we need to get the next first.
        fecPacketListIt++;
      }
    } else {
      fecPacketListIt++;
    }
    if (protectedPacketsFound == fecPacket->protectedPktList.size()) {
      // Either all protected packets arrived or have been recovered.
      // We can discard this FEC packet.
      std::list<ProtectedPacket*>::iterator protectedPacketListIt =
          fecPacket->protectedPktList.begin();
      while (protectedPacketListIt != fecPacket->protectedPktList.end()) {
        delete *protectedPacketListIt;
        protectedPacketListIt++;
        fecPacket->protectedPktList.pop_front();
      }
      assert(fecPacket->protectedPktList.empty());
      delete fecPacket->pkt;
      delete fecPacket;
      _fecPacketList.erase(fecPacketListItToDiscard);
    }
  }
}

int32_t ForwardErrorCorrection::DecodeFEC(
    std::list<ReceivedPacket*>* receivedPacketList,
    std::list<RecoveredPacket*>* recoveredPacketList,
    uint16_t lastFECSeqNum,
    bool& frameComplete) {
  // TODO: can we check for multiple ULP headers, and return an error?

  // Allow an empty received packet list when complete is true as a teardown
  // indicator.
  if (receivedPacketList->empty() && !frameComplete) {
    WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
        "%s received packet list is empty, but we're not tearing down here",
        __FUNCTION__);
    return -1;
  }

  if (frameComplete) {
    // We have a new frame.
    ResetState(recoveredPacketList);
  }
  InsertPackets(receivedPacketList, recoveredPacketList);

  AttemptRecover(recoveredPacketList);

  // Check if we have a complete frame.
  frameComplete = false;

  if (_lastMediaPacketReceived) {
    frameComplete = true;
    if(!_fecPacketReceived) {
      // best estimate we have if we have not received a FEC packet
      _seqNumBase = lastFECSeqNum + 1;
    }
    // With this we assume the user is attempting to decode a FEC stream.
    uint16_t seqNumIdx = 0;
    std::list<RecoveredPacket*>::iterator recPacketListIt =
        recoveredPacketList->begin();
    while (recPacketListIt != recoveredPacketList->end() &&
        frameComplete == true) {
      RecoveredPacket* recPacket = *recPacketListIt;
      if (recPacket->seqNum !=
          static_cast<uint16_t>(_seqNumBase + seqNumIdx)) {
        frameComplete = false;
        break;
      }
      recPacketListIt++;
      seqNumIdx++;
    }
  }
  return 0;
}

uint16_t ForwardErrorCorrection::PacketOverhead() {
  return kFecHeaderSize + kUlpHeaderSizeLBitSet;
}
} // namespace webrtc
