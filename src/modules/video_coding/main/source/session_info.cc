/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "packet.h"
#include "session_info.h"

#include <string.h>
#include <cassert>

namespace webrtc {

VCMSessionInfo::VCMSessionInfo():
    _markerBit(false),
    _sessionNACK(false),
    _completeSession(false),
    _frameType(kVideoFrameDelta),
    _previousFrameLoss(false),
    _lowSeqNum(-1),
    _highSeqNum(-1),
    _highestPacketIndex(0),
    _emptySeqNumLow(-1),
    _emptySeqNumHigh(-1),
    _markerSeqNum(-1)
{
}

VCMSessionInfo::~VCMSessionInfo()
{
}

void
VCMSessionInfo::UpdateDataPointers(const WebRtc_UWord8* frame_buffer,
                                   const WebRtc_UWord8* prev_buffer_address) {
  for (int i = 0; i <= _highestPacketIndex; ++i)
    _packets[i].dataPtr = frame_buffer + (_packets[i].dataPtr -
                                          prev_buffer_address);
}

WebRtc_Word32
VCMSessionInfo::GetLowSeqNum() const
{
    return _lowSeqNum;
}

WebRtc_Word32
VCMSessionInfo::GetHighSeqNum() const
{
    if (_emptySeqNumHigh != -1)
    {
        return _emptySeqNumHigh;
    }
    return _highSeqNum;
}

void
VCMSessionInfo::Reset() {
  for (int i = 0; i <= _highestPacketIndex; ++i)
    _packets[i].Reset();
  _lowSeqNum = -1;
  _highSeqNum = -1;
  _emptySeqNumLow = -1;
  _emptySeqNumHigh = -1;
  _markerBit = false;
  _completeSession = false;
  _frameType = kVideoFrameDelta;
  _previousFrameLoss = false;
  _sessionNACK = false;
  _highestPacketIndex = 0;
  _markerSeqNum = -1;
}

WebRtc_UWord32
VCMSessionInfo::GetSessionLength()
{
    WebRtc_UWord32 length = 0;
    for (WebRtc_Word32 i = 0; i <= _highestPacketIndex; ++i)
    {
        length += _packets[i].sizeBytes;
    }
    return length;
}

void
VCMSessionInfo::SetStartSeqNumber(WebRtc_UWord16 seqNumber)
{
    _lowSeqNum = seqNumber;
    _highSeqNum = seqNumber;
}

bool
VCMSessionInfo::HaveStartSeqNumber()
{
    if (_lowSeqNum == -1 || _highSeqNum == -1)
    {
        return false;
    }
    return true;
}

WebRtc_UWord32
VCMSessionInfo::InsertBuffer(WebRtc_UWord8* ptrStartOfLayer,
                             WebRtc_Word32 packetIndex,
                             const VCMPacket& packet)
{
    WebRtc_UWord32 moveLength = 0;
    WebRtc_UWord32 returnLength = 0;
    int i = 0;

    // need to calc offset before updating _packetSizeBytes
    WebRtc_UWord32 offset = 0;
    WebRtc_UWord32 packetSize = 0;

    // Shallow copy without overwriting the dataPtr and the sizeBytes
    const WebRtc_UWord8* dataPtr = _packets[packetIndex].dataPtr;
    const WebRtc_UWord32 sizeBytes = _packets[packetIndex].sizeBytes;
    _packets[packetIndex] = packet;
    _packets[packetIndex].dataPtr = dataPtr;
    _packets[packetIndex].sizeBytes = sizeBytes;

    // Store this packet length. Add length since we could have data present
    // already (e.g. multicall case).
    packetSize = packet.sizeBytes;
    if (!packet.bits)
    {
        packetSize += (packet.insertStartCode ? kH264StartCodeLengthBytes : 0);
    }

    // count only the one in our layer
    for (i = 0; i < packetIndex; ++i)
    {
        offset += _packets[i].sizeBytes;
    }

    // Set the data pointer to pointing to the first part of this packet.
    if (_packets[packetIndex].dataPtr == NULL)
        _packets[packetIndex].dataPtr = ptrStartOfLayer + offset;

    _packets[packetIndex].sizeBytes += packetSize;

    // Calculate the total move length and move the data pointers in advance.
    for (i = packetIndex + 1; i <= _highestPacketIndex; ++i)
    {
        moveLength += _packets[i].sizeBytes;
        if (_packets[i].dataPtr != NULL)
            _packets[i].dataPtr += packetSize;
    }
    if (moveLength > 0)
    {
        memmove((void*)(_packets[packetIndex].dataPtr + packetSize),
                _packets[packetIndex].dataPtr, moveLength);
    }

    if (packet.dataPtr != NULL)
    {
        const unsigned char startCode[] = {0, 0, 0, 1};
        if (packet.insertStartCode)
        {
            memcpy((void*)(_packets[packetIndex].dataPtr), startCode,
                   kH264StartCodeLengthBytes);
        }
        memcpy((void*)(_packets[packetIndex].dataPtr
            + (packet.insertStartCode ? kH264StartCodeLengthBytes : 0)),
            packet.dataPtr,
            packet.sizeBytes);
    }
    returnLength = packetSize;

    if (packet.markerBit)
    {
        _markerBit = true;
        _markerSeqNum = packet.seqNum;
    }

    UpdateCompleteSession();

    return returnLength;
}

void
VCMSessionInfo::UpdateCompleteSession()
{
    if (_packets[0].isFirstPacket && _markerBit)
    {
        // Do we have all the packets in this session?
        bool completeSession = true;

        for (int i = 0; i <= _highestPacketIndex; ++i)
        {
            if (_packets[i].completeNALU == kNaluUnset)
            {
                completeSession = false;
                break;
            }
        }
        _completeSession = completeSession;
    }
}

bool VCMSessionInfo::IsSessionComplete()
{
    return _completeSession;
}

// Find the start and end index of packetIndex packet.
// startIndex -1 if start not found endIndex = -1 if end index not found
void
VCMSessionInfo::FindNaluBorder(WebRtc_Word32 packetIndex,
                               WebRtc_Word32& startIndex,
                               WebRtc_Word32& endIndex)
{
        if (_packets[packetIndex].completeNALU == kNaluStart ||
            _packets[packetIndex].completeNALU == kNaluComplete)
        {
            startIndex = packetIndex;
        }
        else // Need to find the start
        {
            for (startIndex = packetIndex - 1; startIndex >= 0; --startIndex)
            {

                if ((_packets[startIndex].completeNALU == kNaluComplete &&
                    _packets[startIndex].sizeBytes > 0) ||
                     // Found previous NALU.
                     (_packets[startIndex].completeNALU == kNaluEnd &&
                      startIndex > 0))
                {
                    startIndex++;
                    break;
                }
                // This is where the NALU start.
                if (_packets[startIndex].completeNALU == kNaluStart)
                {
                    break;
                }
            }
        }

        if (_packets[packetIndex].completeNALU == kNaluEnd ||
            _packets[packetIndex].completeNALU == kNaluComplete)
        {
            endIndex = packetIndex;
        }
        else
        {
            // Find the next NALU
            for (endIndex = packetIndex + 1; endIndex <= _highestPacketIndex;
                 ++endIndex)
            {
                if ((_packets[endIndex].completeNALU == kNaluComplete &&
                    _packets[endIndex].completeNALU > 0) ||
                    // Found next NALU.
                    _packets[endIndex].completeNALU == kNaluStart)
                {
                    endIndex--;
                    break;
                }
                if (_packets[endIndex].completeNALU == kNaluEnd)
                {
                    // This is where the NALU end.
                    break;
                }
            }
            if (endIndex > _highestPacketIndex)
            {
                endIndex = -1;
            }
        }
}

// Deletes all packets between startIndex and endIndex
WebRtc_UWord32
VCMSessionInfo::DeletePackets(WebRtc_UWord8* ptrStartOfLayer,
                              WebRtc_Word32 startIndex,
                              WebRtc_Word32 endIndex)
{

    //Get the number of bytes to delete.
    //Clear the size of these packets.
    WebRtc_UWord32 bytesToDelete = 0; /// The number of bytes to delete.
    for (int j = startIndex;j <= endIndex; ++j)
    {
        bytesToDelete += _packets[j].sizeBytes;
        _packets[j].Reset();
    }
    if (bytesToDelete > 0)
    {
        // Get the offset we want to move to.
        int destOffset = 0;
        for (int j = 0;j < startIndex;j++)
        {
           destOffset += _packets[j].sizeBytes;
        }

        // Get the number of bytes to move and move the data pointers in advance
        WebRtc_UWord32 numberOfBytesToMove = 0;
        for (int j = endIndex + 1; j <= _highestPacketIndex; ++j)
        {
            if (_packets[j].dataPtr != NULL)
                _packets[j].dataPtr -= bytesToDelete;
            numberOfBytesToMove += _packets[j].sizeBytes;
        }
        memmove((void*)(ptrStartOfLayer + destOffset),(void*)(ptrStartOfLayer +
            destOffset + bytesToDelete), numberOfBytesToMove);

    }

    return bytesToDelete;
}

int
VCMSessionInfo::BuildVP8FragmentationHeader(
                                        WebRtc_UWord8* frame_buffer,
                                        int frame_buffer_length,
                                        RTPFragmentationHeader* fragmentation) {
  int new_length = 0;
  // Allocate space for max number of partitions
  fragmentation->VerifyAndAllocateFragmentationHeader(kMaxVP8Partitions);
  fragmentation->fragmentationVectorSize = 0;
  memset(fragmentation->fragmentationLength, 0,
         kMaxVP8Partitions * sizeof(WebRtc_UWord32));
  if (_lowSeqNum < 0)
      return new_length;
  int i = FindNextPartitionBeginning(0);
  while (i <= _highestPacketIndex) {
    const int partition_id =
        _packets[i].codecSpecificHeader.codecHeader.VP8.partitionId;
    const int partition_end = FindPartitionEnd(i);
    fragmentation->fragmentationOffset[partition_id] =
        _packets[i].dataPtr - frame_buffer;
    assert(fragmentation->fragmentationOffset[partition_id] <
           static_cast<WebRtc_UWord32>(frame_buffer_length));
    fragmentation->fragmentationLength[partition_id] =
        _packets[partition_end].dataPtr + _packets[partition_end].sizeBytes -
        _packets[i].dataPtr;
    assert(fragmentation->fragmentationLength[partition_id] <=
           static_cast<WebRtc_UWord32>(frame_buffer_length));
    new_length += fragmentation->fragmentationLength[partition_id];
    i = FindNextPartitionBeginning(partition_end + 1);
    if (partition_id + 1 > fragmentation->fragmentationVectorSize)
      fragmentation->fragmentationVectorSize = partition_id + 1;
  }
  // Set all empty fragments to start where the previous fragment ends,
  // and have zero length.
  if (fragmentation->fragmentationLength[0] == 0)
      fragmentation->fragmentationOffset[0] = 0;
  for (i = 1; i < fragmentation->fragmentationVectorSize; ++i) {
    if (fragmentation->fragmentationLength[i] == 0)
      fragmentation->fragmentationOffset[i] =
          fragmentation->fragmentationOffset[i - 1] +
          fragmentation->fragmentationLength[i - 1];
    assert(i == 0 ||
           fragmentation->fragmentationOffset[i] >=
           fragmentation->fragmentationOffset[i - 1]);
  }
  assert(new_length <= frame_buffer_length);
  return new_length;
}

int VCMSessionInfo::FindNextPartitionBeginning(int packet_index) const {
  while (packet_index <= _highestPacketIndex) {
    if (_packets[packet_index].completeNALU == kNaluUnset) {
      // Missing packet
      ++packet_index;
      continue;
    }
    const bool beginning = _packets[packet_index].codecSpecificHeader.
        codecHeader.VP8.beginningOfPartition;
    if (beginning)
      return packet_index;
    ++packet_index;
  }
  return packet_index;
}

int VCMSessionInfo::FindPartitionEnd(int packet_index) const {
  const int partition_id = _packets[packet_index].codecSpecificHeader.
      codecHeader.VP8.partitionId;
  while (packet_index <= _highestPacketIndex) {
    const bool beginning = _packets[packet_index].codecSpecificHeader.
        codecHeader.VP8.beginningOfPartition;
    const bool packet_loss_found =
        (_packets[packet_index].completeNALU == kNaluUnset || (!beginning &&
         !InSequence(_packets[packet_index].seqNum,
                     _packets[packet_index - 1].seqNum)));
    const int current_partition_id = _packets[packet_index].codecSpecificHeader.
          codecHeader.VP8.partitionId;
    if (packet_loss_found || current_partition_id != partition_id) {
      // Missing packet, the previous packet was the last in sequence.
      return packet_index - 1;
    }
    ++packet_index;
  }
  return packet_index - 1;
}

bool VCMSessionInfo::InSequence(WebRtc_UWord16 seqNum,
                                WebRtc_UWord16 prevSeqNum) {
  // prevSeqNum is allowed to wrap around here
  return (static_cast<WebRtc_UWord16>(prevSeqNum + 1) == seqNum);
}

WebRtc_UWord32
VCMSessionInfo::MakeDecodable(WebRtc_UWord8* ptrStartOfLayer)
{
    if (_lowSeqNum < 0) // No packets in this session
    {
        return 0;
    }

    WebRtc_Word32 startIndex = 0;
    WebRtc_Word32 endIndex = 0;
    int packetIndex = 0;
    WebRtc_UWord32 returnLength = 0;
    for (packetIndex = 0; packetIndex <= _highestPacketIndex; ++packetIndex)
    {
        if (_packets[packetIndex].completeNALU == kNaluUnset) // Found a lost packet
        {
            FindNaluBorder(packetIndex, startIndex, endIndex);
            if (startIndex == -1)
            {
                startIndex = 0;
            }
            if (endIndex == -1)
            {
                endIndex = _highestPacketIndex;
            }

            returnLength += DeletePackets(ptrStartOfLayer,
                                          packetIndex, endIndex);
            packetIndex = endIndex;
        }// end lost packet
    }

    // Make sure the first packet is decodable (Either complete nalu or start
    // of NALU)
    if (_packets[0].sizeBytes > 0)
    {
        switch (_packets[0].completeNALU)
        {
            case kNaluComplete: // Packet can be decoded as is.
                break;

            case kNaluStart:
                // Packet contain beginning of NALU- No need to do anything.
                break;
            case kNaluIncomplete: //Packet is not beginning or end of NALU
                // Need to find the end of this NALU and delete all packets.
                FindNaluBorder(0, startIndex, endIndex);
                if (endIndex == -1) // No end found. Delete
                {
                    endIndex = _highestPacketIndex;
                }
                // Delete this NALU.
                returnLength += DeletePackets(ptrStartOfLayer, 0, endIndex);
                break;
            case kNaluEnd:    // Packet is the end of a NALU
                // Delete this NALU
                returnLength += DeletePackets(ptrStartOfLayer, 0, 0);
                break;
            default:
                assert(false);
        }
    }

    return returnLength;
}

WebRtc_Word32
VCMSessionInfo::ZeroOutSeqNum(WebRtc_Word32* list,
                              WebRtc_Word32 numberOfSeqNum)
{
    if ((NULL == list) || (numberOfSeqNum < 1))
    {
        return -1;
    }
    if (_lowSeqNum == -1)
    {
        // no packets in this frame
        return 0;
    }

    // Find end point (index of entry that equals _lowSeqNum)
    int index = 0;
    for (; index < numberOfSeqNum; index++)
    {
        if (list[index] == _lowSeqNum)
        {
            list[index] = -1;
            break;
        }
    }

    // Zero out between first entry and end point
    int i = 0;
    while ( i <= _highestPacketIndex && index < numberOfSeqNum)
    {
        if (_packets[i].completeNALU != kNaluUnset)
        {
            list[index] = -1;
        }
        else
        {
            _sessionNACK = true;
        }
        i++;
        index++;
    }
    if (!_packets[0].isFirstPacket)
    {
        _sessionNACK = true;
    }
    return 0;
}

WebRtc_Word32
VCMSessionInfo::ZeroOutSeqNumHybrid(WebRtc_Word32* list,
                                    WebRtc_Word32 numberOfSeqNum,
                                    float rttScore)
{
    if ((NULL == list) || (numberOfSeqNum < 1))
    {
        return -1;
    }
    if (_lowSeqNum == -1)
    {
        // no media packets in this frame
        return 0;
    }

    WebRtc_Word32 index = 0;
    // Find end point (index of entry that equals _lowSeqNum)
    for (; index < numberOfSeqNum; index++)
    {
        if (list[index] == _lowSeqNum)
        {
            list[index] = -1;
            break;
        }
    }

    // TODO(mikhal): 1. update score based on RTT value 2. add partition data
    // use the previous available
    bool isBaseAvailable = false;
    if ((index > 0) && (list[index] == -1))
    {
        // found first packet, for now let's go only one back
        if ((list[index - 1] == -1) || (list[index - 1] == -2))
        {
            // This is indeed the first packet, as previous packet was populated
            isBaseAvailable = true;
        }
    }
    bool allowNack = false;
    if (!_packets[0].isFirstPacket || !isBaseAvailable)
    {
        allowNack = true;
    }

    // Zero out between first entry and end point
    WebRtc_Word32 i = 0;
    // Score place holder - based on RTT and partition (when available).
    const float nackScoreTh = 0.25f;

    WebRtc_Word32 highMediaPacket;
    if (_markerSeqNum != -1)
    {
        highMediaPacket = _markerSeqNum;
    }
    else
    {
        // Estimation
        highMediaPacket = _emptySeqNumLow - 1 > _highSeqNum ?
                          _emptySeqNumLow - 1: _highSeqNum;
    }

    while (list[index] <= highMediaPacket && index < numberOfSeqNum)
    {
        if (_packets[i].completeNALU != kNaluUnset)
        {
            list[index] = -1;
        }
        else
        {
            // compute score of the packet
            float score = 1.0f;
            // multiply internal score (importance) by external score (RTT)
            score *= rttScore;
            if (score > nackScoreTh)
            {
                allowNack = true;
            }
            else
            {
                list[index] = -1;
            }
        }
        i++;
        index++;
    }
    // Empty packets follow the data packets, and therefore have a higher
    // sequence number. We do not want to NACK empty packets.

    if ((_emptySeqNumLow != -1) && (_emptySeqNumHigh != -1) &&
        (index < numberOfSeqNum))
    {
        // first make sure that we are at least at the minimum value
        // (if not we are missing last packet(s))
        while (list[index] < _emptySeqNumLow && index < numberOfSeqNum)
        {
            index++;
        }

        // mark empty packets
        while (list[index] <= _emptySeqNumHigh && index < numberOfSeqNum)
        {
            list[index] = -2;
            index++;
        }
    }

    _sessionNACK  = allowNack;
    return 0;
}

WebRtc_Word32
VCMSessionInfo::GetHighestPacketIndex()
{
    return _highestPacketIndex;
}

bool
VCMSessionInfo::HaveLastPacket()
{
    return _markerBit;
}

void
VCMSessionInfo::ForceSetHaveLastPacket()
{
    _markerBit = true;
    UpdateCompleteSession();
}

bool
VCMSessionInfo::IsRetransmitted()
{
    return _sessionNACK;
}

void
VCMSessionInfo::UpdatePacketSize(WebRtc_Word32 packetIndex,
                                 WebRtc_UWord32 length)
{
    // sanity
    if (packetIndex >= kMaxPacketsInJitterBuffer || packetIndex < 0)
    {
        // not allowed
        assert(!"SessionInfo::UpdatePacketSize Error: invalid packetIndex");
        return;
    }
    _packets[packetIndex].sizeBytes = length;
}

WebRtc_Word64
VCMSessionInfo::InsertPacket(const VCMPacket& packet,
                             WebRtc_UWord8* ptrStartOfLayer)
{
    // not allowed
    assert(!packet.insertStartCode || !packet.bits);
    // Check if this is first packet (only valid for some codecs)
    if (packet.isFirstPacket)
    {
        // the first packet in the frame always signals the frametype
        _frameType = packet.frameType;
    }
    else if (_frameType == kFrameEmpty && packet.frameType != kFrameEmpty)
    {
        // Update the frame type with the first media packet
        _frameType = packet.frameType;
    }
    if (packet.frameType == kFrameEmpty)
    {
        // Update seq number as an empty packet
        return InformOfEmptyPacket(packet.seqNum);
    }

    // Check sequence number and update highest and lowest sequence numbers
    // received. Move data if this seq num is lower than previously lowest.

    if (packet.seqNum > _highSeqNum)
    {
        // This packet's seq num is higher than previously highest seq num;
        // normal case if we have a wrap, only update with wrapped values
        if (!(_highSeqNum < 0x00ff && packet.seqNum > 0xff00))
        {
            _highSeqNum = packet.seqNum;
        }
    }
    else if (_highSeqNum > 0xff00 && packet.seqNum < 0x00ff)
    {
        // wrap
        _highSeqNum = packet.seqNum;
    }
    int packetIndex = packet.seqNum - (WebRtc_UWord16)_lowSeqNum;
    if (_lowSeqNum < 0x00ff && packet.seqNum > 0xff00)
    {
        // negative wrap
        packetIndex = packet.seqNum - 0x10000 - _lowSeqNum;
    }
    if (packetIndex < 0)
    {
        if (_lowSeqNum > 0xff00 && packet.seqNum < 0x00ff)
        {
            // we have a false detect due to the wrap
            packetIndex = (0xffff - (WebRtc_UWord16)_lowSeqNum) + packet.seqNum
                          + (WebRtc_UWord16)1;
        } else
        {
            // This packet's seq num is lower than previously lowest seq num,
            // but no wrap We need to move the data in all arrays indexed by
            // packetIndex and insert the new packet's info
            // How many packets should we leave room for (positions to shift)?
            // Example - this seq num is 3 lower than previously lowest seq num
            // Before: |--prev packet with lowest seq num--|--|...|
            // After:  |--new lowest seq num--|--|--|--prev packet with
            // lowest seq num--|--|...|

            WebRtc_UWord16 positionsToShift   = (WebRtc_UWord16)_lowSeqNum -
                                                                packet.seqNum;
            WebRtc_UWord16 numOfPacketsToMove = _highestPacketIndex + 1;

            // sanity, do we have room for the shift?
            if ((positionsToShift + numOfPacketsToMove) >
                kMaxPacketsInJitterBuffer)
            {
                return -1;
            }

            // Shift _packetSizeBytes array
            memmove(&_packets[positionsToShift],
                    &_packets[0], numOfPacketsToMove * sizeof(VCMPacket));
            for (int i = 0; i < positionsToShift; ++i)
                _packets[i].Reset();

            _highestPacketIndex += positionsToShift;
            _lowSeqNum = packet.seqNum;
            packetIndex = 0; // (seqNum - _lowSeqNum) = 0
        }
    } // if (_lowSeqNum > seqNum)

    // sanity
    if (packetIndex >= kMaxPacketsInJitterBuffer )
    {
        return -1;
    }
    if (packetIndex < 0 )
    {
        return -1;
    }

    // Check for duplicate packets
    if (_packets[packetIndex].sizeBytes != 0)
    {
        // We have already received a packet with this seq number, ignore it.
        return -2;
    }

    // update highest packet index
    _highestPacketIndex = packetIndex > _highestPacketIndex ?
                                        packetIndex :_highestPacketIndex;

    return InsertBuffer(ptrStartOfLayer, packetIndex, packet);
}


WebRtc_Word32
VCMSessionInfo::InformOfEmptyPacket(const WebRtc_UWord16 seqNum)
{
    // Empty packets may be FEC or filler packets. They are sequential and
    // follow the data packets, therefore, we should only keep track of the high
    // and low sequence numbers and may assume that the packets in between are
    // empty packets belonging to the same frame (timestamp).

    if (_emptySeqNumLow == -1 && _emptySeqNumHigh == -1)
    {
        _emptySeqNumLow = seqNum;
        _emptySeqNumHigh = seqNum;
    }
    else
    {
        if (seqNum > _emptySeqNumHigh)
        {
            // This packet's seq num is higher than previously highest seq num;
            // normal case if we have a wrap, only update with wrapped values
            if (!(_emptySeqNumHigh < 0x00ff && seqNum > 0xff00))
            {
                _emptySeqNumHigh = seqNum;
            }
        }
        else if (_emptySeqNumHigh > 0xff00 && seqNum < 0x00ff)
        {
             // wrap
             _emptySeqNumHigh = seqNum;
        }
        if (_emptySeqNumLow < 0x00ff && seqNum > 0xff00)
        {
            // negative wrap
            if (seqNum - 0x10000 - _emptySeqNumLow < 0)
            {
                _emptySeqNumLow = seqNum;
            }
        }
    }
    return 0;
}

WebRtc_UWord32
VCMSessionInfo::PrepareForDecode(WebRtc_UWord8* ptrStartOfLayer,
                                 VideoCodecType codec)
{
    WebRtc_UWord32 currentPacketOffset = 0;
    WebRtc_UWord32 length = GetSessionLength();
    WebRtc_UWord32 realDataBytes = 0;
    if (length == 0)
    {
        return length;
    }
    bool previousLost = false;
    for (int i = 0; i <= _highestPacketIndex; i++)
    {
        if (_packets[i].bits)
        {
            if (currentPacketOffset > 0)
            {
                WebRtc_UWord8* ptrFirstByte = ptrStartOfLayer +
                                              currentPacketOffset;

                if (_packets[i - 1].sizeBytes == 0 || previousLost)
                {
                    // It is be better to throw away this packet if we are
                    // missing the previous packet.
                    memset(ptrFirstByte, 0, _packets[i].sizeBytes);
                    previousLost = true;
                }
                else if (_packets[i].sizeBytes > 0) // Ignore if empty packet
                {
                    // Glue with previous byte
                    // Move everything from [this packet start + 1,
                    // end of buffer] one byte to the left
                    WebRtc_UWord8* ptrPrevByte = ptrFirstByte - 1;
                    *ptrPrevByte = (*ptrPrevByte) | (*ptrFirstByte);
                    WebRtc_UWord32 lengthToEnd = length -
                                                 (currentPacketOffset + 1);
                    memmove((void*)ptrFirstByte, (void*)(ptrFirstByte + 1),
                            lengthToEnd);
                    _packets[i].sizeBytes--;
                    length--;
                    previousLost = false;
                    realDataBytes += _packets[i].sizeBytes;
                }
            }
            else
            {
                memset(ptrStartOfLayer, 0, _packets[i].sizeBytes);
                previousLost = true;
            }
        }
        else if (_packets[i].sizeBytes == 0 && codec == kVideoCodecH263)
        {
            WebRtc_UWord8* ptrFirstByte = ptrStartOfLayer + currentPacketOffset;
            memmove(ptrFirstByte + 10, ptrFirstByte,
                    length - currentPacketOffset);
            memset(ptrFirstByte, 0, 10);
            _packets[i].sizeBytes = 10;
            length += _packets[i].sizeBytes;
            previousLost = true;
        }
        else
        {
            realDataBytes += _packets[i].sizeBytes;
            previousLost = false;
        }
        currentPacketOffset += _packets[i].sizeBytes;
    }
    if (realDataBytes == 0)
    {
        // Drop the frame since all it contains are zeros
        length = 0;
        for (int i = 0; i <= _highestPacketIndex; ++i)
            _packets[i].Reset();
    }
    return length;
}

}
