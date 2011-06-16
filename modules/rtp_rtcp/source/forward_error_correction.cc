/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "forward_error_correction.h"
#include "fec_private_tables.h"
#include "rtp_utility.h"

#include "trace.h"
#include <cassert>
#include <cstring>

#include "forward_error_correction_internal.h"

namespace webrtc {

// Minimum RTP header size in bytes.
const WebRtc_UWord8 kRtpHeaderSize = 12;

// FEC header size in bytes.
const WebRtc_UWord8 kFecHeaderSize = 10;

// ULP header size in bytes (L bit is set).
const WebRtc_UWord8 kUlpHeaderSizeLBitSet = (2 + kMaskSizeLBitSet);

// ULP header size in bytes (L bit is cleared).
const WebRtc_UWord8 kUlpHeaderSizeLBitClear = (2 + kMaskSizeLBitClear);

//Transport header size in bytes. Assume UDP/IPv4 as a reasonable minimum.
const WebRtc_UWord8 kTransportOverhead = 28;

//
// Used for internal storage of FEC packets in a list.
//
struct FecPacket
{
    ListWrapper protectedPktList;        /**> List containing #ProtectedPacket types.*/
    WebRtc_UWord16 seqNum;               /**> Sequence number. */
    WebRtc_UWord32 ssrc;                 /**> SSRC of the current frame. */
    ForwardErrorCorrection::Packet* pkt; /**> Pointer to the packet storage. */
};

//
// Used to link media packets to their protecting FEC packets.
//
struct ProtectedPacket
{
    WebRtc_UWord16 seqNum;               /**> Sequence number. */
    ForwardErrorCorrection::Packet* pkt; /**> Pointer to the packet storage. */
};

ForwardErrorCorrection::ForwardErrorCorrection(const WebRtc_Word32 id) :
    _id(id),
    _generatedFecPackets(NULL),
    _fecPacketList(),
    _seqNumBase(0),
    _lastMediaPacketReceived(false),
    _fecPacketReceived(false)
{
}

ForwardErrorCorrection::~ForwardErrorCorrection()
{
    if (_generatedFecPackets != NULL)
    {
        delete [] _generatedFecPackets;
    }
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
WebRtc_Word32
ForwardErrorCorrection::GenerateFEC(const ListWrapper& mediaPacketList,
                                    WebRtc_UWord8 protectionFactor,
                                    WebRtc_UWord32 numImportantPackets,
                                    ListWrapper& fecPacketList)
{
    if (mediaPacketList.Empty())
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
            "%s media packet list is empty", __FUNCTION__);
        return -1;
    }

    if (!fecPacketList.Empty())
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
            "%s FEC packet list is not empty", __FUNCTION__);
        return -1;
    }

    const WebRtc_UWord16 numMediaPackets = mediaPacketList.GetSize();
    const WebRtc_UWord8 lBit = numMediaPackets > 16 ? 1 : 0;
    const WebRtc_UWord16 numMaskBytes =
        (lBit == 1)? kMaskSizeLBitSet : kMaskSizeLBitClear;
    const WebRtc_UWord16 ulpHeaderSize =
        (lBit == 1)? kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear;
    const WebRtc_UWord16 fecRtpOffset =
        kFecHeaderSize + ulpHeaderSize - kRtpHeaderSize;
    const WebRtc_UWord16 maxMediaPackets = numMaskBytes * 8;

    if (numMediaPackets > maxMediaPackets)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
            "%s can only protect %d media packets per frame; %d requested",
            __FUNCTION__, maxMediaPackets, numMediaPackets);
        return -1;
    }

    // Error checking on the number of important packets.
    // Can't have more important packets than media packets.
    if (numImportantPackets > numMediaPackets)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
            "Number of Important packet greater than number of Media Packets %d %d",
            numImportantPackets, numMediaPackets);
        return -1;
    }
    if (numImportantPackets < 0)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
            "Number of Important packets less than zero %d %d",
            numImportantPackets, numMediaPackets);
        return -1;
    }

    // Do some error checking on the media packets.
    Packet* mediaPacket;
    ListItem* mediaListItem = mediaPacketList.First();
    while (mediaListItem != NULL)
    {
        mediaPacket = static_cast<Packet*>(mediaListItem->GetItem());

        if (mediaPacket->length < kRtpHeaderSize)
        {
            WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                "%s media packet (%d bytes) is smaller than RTP header",
                __FUNCTION__, mediaPacket->length);
            return -1;
        }

        // Ensure our FEC packets will fit in a typical MTU.
        if (mediaPacket->length + PacketOverhead() + kTransportOverhead >
            IP_PACKET_SIZE)
        {
            WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                "%s media packet (%d bytes) with overhead is larger than MTU (%d bytes)",
                __FUNCTION__, mediaPacket->length, IP_PACKET_SIZE);
            return -1;
        }

        mediaListItem = mediaPacketList.Next(mediaListItem);
    }

    // Result in Q0 with an unsigned round.
    WebRtc_UWord32 numFecPackets = (numMediaPackets * protectionFactor + (1 << 7)) >> 8;
    if (numFecPackets == 0)
    {
        return 0;
    }
    assert(numFecPackets <= numMediaPackets);

    // -- Initialize FEC list --
    if (_generatedFecPackets != NULL)
    {
        delete [] _generatedFecPackets;
        _generatedFecPackets = NULL;
    }

    _generatedFecPackets = new Packet[numFecPackets];
    for (WebRtc_UWord32 i = 0; i < numFecPackets; i++)
    {
        memset(_generatedFecPackets[i].data, 0, IP_PACKET_SIZE);
        _generatedFecPackets[i].length = 0; // Use this as a marker for untouched
                                            // packets.
        fecPacketList.PushBack(&_generatedFecPackets[i]);
    }

    // -- Generate packet masks --
    WebRtc_UWord8 packetMask[numFecPackets * numMaskBytes];
    memset(packetMask, 0, numFecPackets * numMaskBytes);
    internal::GeneratePacketMasks(numMediaPackets, numFecPackets,
        numImportantPackets, packetMask);

    // -- Generate FEC bit strings --
    WebRtc_UWord8 mediaPayloadLength[2];
    mediaListItem = mediaPacketList.First();
    for (WebRtc_UWord32 i = 0; i < numFecPackets; i++)
    {
        mediaListItem = mediaPacketList.First();
        WebRtc_UWord32 pktMaskIdx = i * numMaskBytes;
        WebRtc_UWord32 mediaPktIdx = 0;
        WebRtc_UWord16 fecPacketLength = 0;
        while (mediaListItem != NULL)
        {
            // Each FEC packet has a multiple byte mask.
            if (packetMask[pktMaskIdx] & (1 << (7 - mediaPktIdx)))
            {
                mediaPacket = static_cast<Packet*>(mediaListItem->GetItem());

                // Assign network-ordered media payload length.
                ModuleRTPUtility::AssignUWord16ToBuffer(mediaPayloadLength,
                    mediaPacket->length - kRtpHeaderSize);
                fecPacketLength = mediaPacket->length + fecRtpOffset;
                // On the first protected packet, we don't need to XOR.
                if (_generatedFecPackets[i].length == 0)
                {
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
                }
                else
                {
                    // XOR with the first 2 bytes of the RTP header.
                    _generatedFecPackets[i].data[0] ^= mediaPacket->data[0];
                    _generatedFecPackets[i].data[1] ^= mediaPacket->data[1];

                    // XOR with the 5th to 8th bytes of the RTP header.
                    for (WebRtc_UWord32 j = 4; j < 8; j++)
                    {
                        _generatedFecPackets[i].data[j] ^= mediaPacket->data[j];
                    }

                    // XOR with the network-ordered payload size.
                    _generatedFecPackets[i].data[8] ^= mediaPayloadLength[0];
                    _generatedFecPackets[i].data[9] ^= mediaPayloadLength[1];

                    // XOR with RTP payload, leaving room for the ULP header.
                    for (WebRtc_Word32 j = kFecHeaderSize + ulpHeaderSize;
                        j < fecPacketLength; j++)
                    {
                        _generatedFecPackets[i].data[j] ^=
                            mediaPacket->data[j - fecRtpOffset];
                    }
                }

                if (fecPacketLength > _generatedFecPackets[i].length)
                {
                    _generatedFecPackets[i].length = fecPacketLength;
                }
            }

            mediaListItem = mediaPacketList.Next(mediaListItem);
            mediaPktIdx++;
            if (mediaPktIdx == 8)
            {
                // Switch to the next mask byte.
                mediaPktIdx = 0;
                pktMaskIdx++;
            }
        }

        if (_generatedFecPackets[i].length == 0)
        {
            //Note: This shouldn't happen: means packet mask is wrong or poorly designed
            WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
                "Packet mask has row of zeros %d %d",
                numMediaPackets, numFecPackets);
            return -1;

        }
    }

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
    mediaListItem = mediaPacketList.First();
    mediaPacket = static_cast<Packet*>(mediaListItem->GetItem());
    assert(mediaPacket != NULL);
    for (WebRtc_UWord32 i = 0; i < numFecPackets; i++)
    {
        // -- FEC header --
        _generatedFecPackets[i].data[0] &= 0x7f; // Set E to zero.
        if (lBit == 0)
        {
            _generatedFecPackets[i].data[0] &= 0xbf; // Clear the L bit.
        }
        else
        {
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

    return 0;
}

WebRtc_Word32
ForwardErrorCorrection::DecodeFEC(ListWrapper& receivedPacketList,
                                  ListWrapper& recoveredPacketList,
                                  const WebRtc_UWord16 lastFECSeqNum,
                                  bool& frameComplete)
{
    // TODO: can we check for multiple ULP headers, and return an error?

    // Allow an empty received packet list when complete is true as a teardown indicator.
    if (receivedPacketList.Empty() && !frameComplete)
    {
        WEBRTC_TRACE(kTraceError, kTraceRtpRtcp, _id,
            "%s received packet list is empty, but we're not tearing down here",
            __FUNCTION__);
        return -1;
    }

    ListItem* packetListItem = NULL;
    ListItem* protectedPacketListItem = NULL;
    FecPacket* fecPacket = NULL;
    RecoveredPacket* recPacket = NULL;
    if (frameComplete)
    {
        // We have a new frame.
        _seqNumBase = 0;
        _lastMediaPacketReceived = false;
        _fecPacketReceived = false;

        // Free the memory for any existing recovered packets, if the user hasn't.
        if (!recoveredPacketList.Empty())
        {
            packetListItem = recoveredPacketList.First();
            while (packetListItem != NULL)
            {
                recPacket = static_cast<RecoveredPacket*>(packetListItem->GetItem());
                delete recPacket->pkt;
                delete recPacket;
                recPacket = NULL;
                packetListItem = recoveredPacketList.Next(packetListItem);
                recoveredPacketList.PopFront();
            }
            assert(recoveredPacketList.Empty());
        }

        // Free the FEC packet list.
        packetListItem = _fecPacketList.First();
        while (packetListItem != NULL)
        {
            fecPacket = static_cast<FecPacket*>(packetListItem->GetItem());
            protectedPacketListItem = fecPacket->protectedPktList.First();
            while (protectedPacketListItem != NULL)
            {
                delete static_cast<ProtectedPacket*>(protectedPacketListItem->GetItem());
                protectedPacketListItem =
                    fecPacket->protectedPktList.Next(protectedPacketListItem);
                fecPacket->protectedPktList.PopFront();
            }
            assert(fecPacket->protectedPktList.Empty());
            delete fecPacket->pkt;
            delete fecPacket;
            fecPacket = NULL;
            packetListItem = _fecPacketList.Next(packetListItem);
            _fecPacketList.PopFront();
        }
        assert(_fecPacketList.Empty());
    }

    // -- Insert packets into FEC or recovered list --
    ReceivedPacket* rxPacket = NULL;
    RecoveredPacket* recPacketToInsert = NULL;
    ProtectedPacket* protectedPacket = NULL;
    ListItem* recPacketListItem = NULL;
    ListItem* fecPacketListItem = NULL;
    packetListItem = receivedPacketList.First();
    while (packetListItem != NULL)
    {
        rxPacket = static_cast<ReceivedPacket*>(packetListItem->GetItem());

        if (!rxPacket->isFec) // Media packet
        {
            // Insert into recovered packet list. This is essentially an insertion
            // sort, with duplicate removal. We search from the back of the list as we
            // expect packets to arrive in order.
            if (rxPacket->lastMediaPktInFrame)
            {
                if (_lastMediaPacketReceived)
                {
                    // We already received the last packet.
                    WEBRTC_TRACE(kTraceWarning, kTraceRtpRtcp, _id,
                        "%s last media packet marked more than once per frame",
                        __FUNCTION__);
                }
                _lastMediaPacketReceived = true;
            }

            bool duplicatePacket = false;
            recPacketListItem = recoveredPacketList.Last();
            ListItem* nextItem = NULL;
            while (recPacketListItem != NULL)
            {
                recPacket = static_cast<RecoveredPacket*>(recPacketListItem->GetItem());
                if (rxPacket->seqNum == recPacket->seqNum)
                {
                    // Duplicate packet, no need to add to list.
                    duplicatePacket = true;
                    break;
                }
                else if ((rxPacket->seqNum < recPacket->seqNum ||
                    rxPacket->seqNum > recPacket->seqNum + 48) &&   // Wrap guard.
                    rxPacket->seqNum > recPacket->seqNum - 48)      //
                {
                    nextItem = recPacketListItem;
                    recPacketListItem = recoveredPacketList.Previous(recPacketListItem);
                }
                else
                {
                    // Found the correct position.
                    break;
                }
            }

            if (duplicatePacket)
            {
                // Delete duplicate media packet data.
                delete rxPacket->pkt;
            }else
            {
                recPacketToInsert = new RecoveredPacket;
                recPacketToInsert->wasRecovered = false;
                recPacketToInsert->seqNum = rxPacket->seqNum;
                recPacketToInsert->pkt = rxPacket->pkt;
                recPacketToInsert->pkt->length = rxPacket->pkt->length;

                if (nextItem == NULL)
                {
                    // Insert at the back.
                    recoveredPacketList.PushBack(recPacketToInsert);
                }else
                {
                    // Insert in sorted position.
                    recoveredPacketList.InsertBefore(nextItem, new ListItem(recPacketToInsert));
                }
            }
        }else // FEC packet
        {
            _fecPacketReceived = true;
            WebRtc_UWord8 packetMask;

            // Check for duplicate.
            bool duplicatePacket = false;
            fecPacketListItem = _fecPacketList.First();
            while (fecPacketListItem != NULL)
            {
                fecPacket = static_cast<FecPacket*>(fecPacketListItem->GetItem());
                if (rxPacket->seqNum == fecPacket->seqNum)
                {
                    duplicatePacket = true;
                    break;
                }
                fecPacketListItem = _fecPacketList.Next(fecPacketListItem);
            }
            if (duplicatePacket)
            {
                // Delete duplicate FEC packet data.
                delete rxPacket->pkt;
                rxPacket->pkt = NULL;

            }else
            {
                fecPacket = new FecPacket;
                fecPacket->pkt = rxPacket->pkt;
                fecPacket->seqNum = rxPacket->seqNum;
                fecPacket->ssrc = rxPacket->ssrc;

                // We store this for determining frame completion later.
                _seqNumBase = ModuleRTPUtility::BufferToUWord16(&fecPacket->pkt->data[2]);
                const WebRtc_UWord16 maskSizeBytes = (fecPacket->pkt->data[0] & 0x40) ?
                    kMaskSizeLBitSet : kMaskSizeLBitClear; // L bit set?

                for (WebRtc_UWord16 byteIdx = 0; byteIdx < maskSizeBytes; byteIdx++)
                {
                    packetMask = fecPacket->pkt->data[12 + byteIdx];
                    for (WebRtc_UWord16 bitIdx = 0; bitIdx < 8; bitIdx++)
                    {
                        if (packetMask & (1 << (7 - bitIdx)))
                        {
                            protectedPacket = new ProtectedPacket;
                            fecPacket->protectedPktList.PushBack(protectedPacket);
                            // This wraps naturally with the sequence number.
                            protectedPacket->seqNum = static_cast<WebRtc_UWord16>
                                (_seqNumBase + (byteIdx << 3) + bitIdx);
                            protectedPacket->pkt = NULL;
                        }
                    }
                }

                if (fecPacket->protectedPktList.Empty())
                {
                    // All-zero packet mask; we can discard this FEC packet.
                    delete fecPacket->pkt;
                    delete fecPacket;
                    fecPacket = NULL;
                }
                else
                {
                    _fecPacketList.PushBack(fecPacket);
                }
            }
        }

        packetListItem = receivedPacketList.Next(packetListItem);

        // Delete the received packet "wrapper", but not the packet data.
        delete rxPacket;
        rxPacket = NULL;
        receivedPacketList.PopFront();
    }
    assert(receivedPacketList.Empty());

    // -- Attempt to recover packets --
    WebRtc_UWord16 protectedPacketsFound;
    WebRtc_UWord8 mediaPayloadLength[2];
    WebRtc_UWord8 protectionLength[2];
    fecPacketListItem = _fecPacketList.First();
    ListItem* fecPacketListItemToDiscard = NULL;
    while (fecPacketListItem != NULL)
    {
        // Search for each FEC packet's protected media packets.
        fecPacket = static_cast<FecPacket*>(fecPacketListItem->GetItem());
        protectedPacketListItem = fecPacket->protectedPktList.First();
        recPacketListItem = recoveredPacketList.First();
        protectedPacketsFound = 0;
        fecPacketListItemToDiscard = NULL;
        while (protectedPacketListItem != NULL)
        {
            protectedPacket =
                static_cast<ProtectedPacket*>(protectedPacketListItem->GetItem());

            if (protectedPacket->pkt != NULL)
            {
                // We already have the required packet.
                protectedPacketsFound++;
            }
            else
            {
                // Search for the required packet.
                while (recPacketListItem != NULL)
                {
                    recPacket =
                        static_cast<RecoveredPacket*>(recPacketListItem->GetItem());
                    recPacketListItem = recoveredPacketList.Next(recPacketListItem);
                    if (protectedPacket->seqNum == recPacket->seqNum)
                    {
                        protectedPacket->pkt = recPacket->pkt;
                        protectedPacketsFound++;
                        break;
                    }
                }

                // Since the recovered packet list is already sorted, we don't need to
                // restart at the beginning of the list unless the previous protected
                // packet wasn't found.
                if (protectedPacket->pkt == NULL)
                {
                    recPacketListItem = recoveredPacketList.First();
                }
            }

            protectedPacketListItem =
                fecPacket->protectedPktList.Next(protectedPacketListItem);
        }

        if (protectedPacketsFound == fecPacket->protectedPktList.GetSize() - 1)
        {
            // Recovery possible.
            WebRtc_UWord8 lengthRecovery[2];
            const WebRtc_UWord16 ulpHeaderSize = fecPacket->pkt->data[0] & 0x40 ?
                kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear; // L bit set?

            RecoveredPacket* recPacketToInsert = new RecoveredPacket;
            recPacketToInsert->wasRecovered = true;
            recPacketToInsert->pkt = new Packet;
            memset(recPacketToInsert->pkt->data, 0, IP_PACKET_SIZE);

            // Copy the protection length from the ULP header.
            memcpy(&protectionLength, &fecPacket->pkt->data[10], 2);

            // Copy the first 2 bytes of the FEC header.
            memcpy(recPacketToInsert->pkt->data, fecPacket->pkt->data, 2);

            // Copy the 5th to 8th bytes of the FEC header.
            memcpy(&recPacketToInsert->pkt->data[4], &fecPacket->pkt->data[4], 4);

            // Set the SSRC field.
            ModuleRTPUtility::AssignUWord32ToBuffer(&recPacketToInsert->pkt->data[8],
                fecPacket->ssrc);

            // Copy the length recovery field.
            memcpy(&lengthRecovery, &fecPacket->pkt->data[8], 2);

            // Copy FEC payload, skipping the ULP header.
            memcpy(&recPacketToInsert->pkt->data[kRtpHeaderSize],
                &fecPacket->pkt->data[kFecHeaderSize + ulpHeaderSize],
                ModuleRTPUtility::BufferToUWord16(protectionLength));

            protectedPacketListItem = fecPacket->protectedPktList.First();
            while (protectedPacketListItem != NULL)
            {
                protectedPacket =
                    static_cast<ProtectedPacket*>(protectedPacketListItem->GetItem());

                if (protectedPacket->pkt == NULL)
                {
                    // This is the packet we're recovering.
                    recPacketToInsert->seqNum = protectedPacket->seqNum;
                }
                else
                {
                    // XOR with the first 2 bytes of the RTP header.
                    for (WebRtc_UWord32 i = 0; i < 2; i++)
                    {
                        recPacketToInsert->pkt->data[i] ^= protectedPacket->pkt->data[i];
                    }

                    // XOR with the 5th to 8th bytes of the RTP header.
                    for (WebRtc_UWord32 i = 4; i < 8; i++)
                    {
                        recPacketToInsert->pkt->data[i] ^= protectedPacket->pkt->data[i];
                    }

                    // XOR with the network-ordered payload size.
                    ModuleRTPUtility::AssignUWord16ToBuffer(mediaPayloadLength,
                        protectedPacket->pkt->length - kRtpHeaderSize);
                    lengthRecovery[0] ^= mediaPayloadLength[0];
                    lengthRecovery[1] ^= mediaPayloadLength[1];

                    // XOR with RTP payload.
                    // TODO: Are we doing more XORs than required here?
                    for (WebRtc_Word32 i = kRtpHeaderSize; i < protectedPacket->pkt->length;
                        i++)
                    {
                        recPacketToInsert->pkt->data[i] ^= protectedPacket->pkt->data[i];
                    }
                }
                protectedPacketListItem =
                    fecPacket->protectedPktList.Next(protectedPacketListItem);
            }

            // Set the RTP version to 2.
            recPacketToInsert->pkt->data[0] |= 0x80; // Set the 1st bit.
            recPacketToInsert->pkt->data[0] &= 0xbf; // Clear the 2nd bit.

            // Assume a recovered marker bit indicates the last media packet in a frame.
            if (recPacketToInsert->pkt->data[1] & 0x80)
            {
                if (_lastMediaPacketReceived)
                {
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

            // Insert into recovered list in correct position.
            recPacketListItem = recoveredPacketList.Last();
            ListItem* nextItem = NULL;
            while (recPacketListItem != NULL)
            {
                recPacket = static_cast<RecoveredPacket*>(recPacketListItem->GetItem());
                if ((recPacketToInsert->seqNum < recPacket->seqNum ||
                    recPacketToInsert->seqNum > recPacket->seqNum + 48) &&   // Wrap guard.
                    recPacketToInsert->seqNum > recPacket->seqNum - 48)      //
                {
                    nextItem = recPacketListItem;
                    recPacketListItem = recoveredPacketList.Previous(recPacketListItem);
                }
                else
                {
                    // Found the correct position.
                    break;
                }
            }

            if (nextItem == NULL)
            {
                // Insert at the back.
                recoveredPacketList.PushBack(recPacketToInsert);
            }
            else
            {
                // Insert in sorted position.
                recoveredPacketList.InsertBefore(nextItem,
                    new ListItem(recPacketToInsert));
            }

            protectedPacketsFound++;
            assert(protectedPacketsFound == fecPacket->protectedPktList.GetSize());
            fecPacketListItemToDiscard = fecPacketListItem;
        }

        if (fecPacketListItemToDiscard != NULL)
        {
            // A packet has been recovered. We need to check the FEC list again, as this
            // may allow additional packets to be recovered.
            fecPacketListItem = _fecPacketList.First();
            if (fecPacketListItem == fecPacketListItemToDiscard)
            {
                // If we're deleting the first item, we need to get the next first.
                fecPacketListItem = _fecPacketList.Next(fecPacketListItem);
            }
        }
        else
        {
            // Store this in case a discard is required.
            fecPacketListItemToDiscard = fecPacketListItem;
            fecPacketListItem = _fecPacketList.Next(fecPacketListItem);
        }

        if (protectedPacketsFound == fecPacket->protectedPktList.GetSize())
        {
            // Either all protected packets arrived or have been recovered.
            // We can discard this FEC packet.
            protectedPacketListItem = fecPacket->protectedPktList.First();
            while (protectedPacketListItem != NULL)
            {
                delete static_cast<ProtectedPacket*>(protectedPacketListItem->GetItem());
                protectedPacketListItem =
                    fecPacket->protectedPktList.Next(protectedPacketListItem);
                fecPacket->protectedPktList.PopFront();
            }
            assert(fecPacket->protectedPktList.Empty());
            delete fecPacket->pkt;
            delete fecPacket;
            fecPacket = NULL;
            assert(fecPacketListItemToDiscard != NULL);
            _fecPacketList.Erase(fecPacketListItemToDiscard);
        }
    }

    // Check if we have a complete frame.
    frameComplete = false;

    if (_lastMediaPacketReceived)
    {
        if(!_fecPacketReceived)
        {
            // best estimate we have if we have not received a FEC packet
            _seqNumBase = lastFECSeqNum + 1;
        }
        // With this we assume the user is attempting to decode a FEC stream.
        WebRtc_UWord16 seqNumIdx = 0;
        frameComplete = true;
        recPacketListItem = recoveredPacketList.First();
        while (recPacketListItem != NULL && frameComplete == true)
        {
            recPacket = static_cast<RecoveredPacket*>(recPacketListItem->GetItem());
            recPacketListItem = recoveredPacketList.Next(recPacketListItem);
            if (recPacket->seqNum !=
                static_cast<WebRtc_UWord16>(_seqNumBase + seqNumIdx)) // Wraps naturally.
            {
                frameComplete = false;
            }
            seqNumIdx++;
        }
    }

    return 0;
}

WebRtc_UWord16
ForwardErrorCorrection::PacketOverhead()
{
    return kFecHeaderSize + kUlpHeaderSizeLBitSet;
}
} // namespace webrtc
