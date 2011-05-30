/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_

#include "typedefs.h"
#include "rtp_rtcp_defines.h"

#include "list_wrapper.h"

namespace webrtc {
/**
 * Performs codec-independent forward error correction.
 */
class ForwardErrorCorrection
{
public:
    /**
     * The ListWrapper parameters of #GenerateFEC() should reference structs of this type.
     */
    struct Packet
    {
        WebRtc_UWord16 length;                    /**> Length of packet in bytes. */
        WebRtc_UWord8 data[IP_PACKET_SIZE];  /**> Packet data. */
    };

    /**
     * The received list parameter of #DecodeFEC() must reference structs of this type.
     * The lastMediaPktInFrame is not required to be used for correct recovery, but will
     * reduce delay by allowing #DecodeFEC() to pre-emptively determine frame completion.
     * If set, we assume a FEC stream, and the following assumptions must hold:\n
     *
     * 1. The media packets in a frame have contiguous sequence numbers, i.e. the frame's
     *    FEC packets have sequence numbers either lower than the first media packet or
     *    higher than the last media packet.\n
     * 2. All FEC packets have a sequence number base equal to the first media packet in
     *    the corresponding frame.\n
     *
     * The ssrc member is needed to ensure we can restore the SSRC field of recovered
     * packets. In most situations this could be retrieved from other media packets, but
     * in the case of an FEC packet protecting a single missing media packet, we have no
     * other means of obtaining it.
     */
    struct ReceivedPacket
    {
        WebRtc_UWord16 seqNum;      /**> Sequence number of packet. */
        WebRtc_UWord32 ssrc;        /**> SSRC of the current frame. Must be set for FEC
                                       packets, but not required for media packets. */
        bool isFec;               /**> Set to true if this is an FEC packet and false
                                       otherwise. */
        bool lastMediaPktInFrame; /**> Set to true to mark the last media packet in the
                                       frame and false otherwise. */
        Packet* pkt;              /**> Pointer to the packet storage. */
    };

    /**
     * The recovered list parameter of #DecodeFEC() will reference structs of this type.
     */
    struct RecoveredPacket
    {
        bool wasRecovered;   /**> Will be true if this packet was recovered by the FEC.
                                   Otherwise it was a media packet passed in through the
                                   received packet list. */
        WebRtc_UWord16 seqNum; /**> Sequence number of the packet. This is mostly for
                                  implementation convenience but could be utilized by the
                                  user if so desired. */
        Packet* pkt;         /**> Pointer to the packet storage. */
    };

    /**
     * Constructor.
     *
     * \param[in] id Module ID
     */
    ForwardErrorCorrection(const WebRtc_Word32 id);

    /**
     * Destructor. Before freeing an instance of the class, #DecodeFEC() must be called
     * in a particular fashion to free oustanding memory. Refer to #DecodeFEC().
     */
    virtual ~ForwardErrorCorrection();

    /**
     * Generates a list of FEC packets from supplied media packets.
     *
     * \param[in]  mediaPacketList  List of media packets to protect, of type #Packet.
     *                               All packets must belong to the same frame and the
     *                               list must not be empty.
     * \param[out] fecPacketList    List of FEC packets, of type #Packet. Must be empty
     *                               on entry. The memory available through the list
     *                               will be valid until the next call to GenerateFEC().
     * \param[in]  protectionFactor FEC protection overhead in the [0, 255] domain. To
     *                               obtain 100% overhead, or an equal number of FEC
     *                               packets as media packets, use 255.
     *
     * \return 0 on success, -1 on failure.
     */
     WebRtc_Word32 GenerateFEC(const ListWrapper& mediaPacketList, ListWrapper& fecPacketList,
         WebRtc_UWord8 protectionFactor);

    /**
     * Decodes a list of media and FEC packets. It will parse the input received packet
     * list, storing FEC packets internally and inserting media packets to the output
     * recovered packet list. The recovered list will be sorted by ascending sequence
     * number and have duplicates removed. The function should be called as new packets
     * arrive, with the recovered list being progressively assembled with each call.
     * The received packet list will be empty at output.\n
     *
     * The user will allocate packets submitted through the received list. The function
     * will handle allocation of recovered packets and optionally deleting of all packet
     * memory. The user may delete the recovered list packets, in which case they must
     * remove deleted packets from the recovered list.\n
     *
     * Before deleting an instance of the class, call the function with an empty received
     * packet list and the completion parameter set to true. This will free any
     * outstanding memory.
     *
     * \param[in]  receivedPacketList  List of new received packets, of type
     *                                  #ReceivedPacket, beloning to a single frame. At
     *                                  output the list will be empty, with packets
     *                                  either stored internally, or accessible through
     *                                  the recovered list.
     * \param[out] recoveredPacketList List of recovered media packets, of type
     *                                  #RecoveredPacket, belonging to a single frame.
     *                                  The memory available through the list will be
     *                                  valid until the next call to DecodeFEC() in which
     *                                  the completion parameter is set to true.
     * \param[in] lastFECSeqNum        Estimated last seqNumber before this frame
     * \param[in,out] frameComplete    Set to true on input to indicate the start of a
     *                                  new frame. On output, this will be set to true if
     *                                  all media packets in the frame have been
     *                                  recovered. Note that the frame may be complete
     *                                  without this parameter having been set, as it may
     *                                  not always be possible to determine frame
     *                                  completion.
     *
     * \return 0 on success, -1 on failure.
     */
    WebRtc_Word32 DecodeFEC(ListWrapper& receivedPacketList,
                          ListWrapper& recoveredPacketList,
                          const WebRtc_UWord16 lastFECSeqNum,
                          bool& frameComplete);
    /**
     * Gets the size in bytes of the FEC/ULP headers, which must be accounted for as
     * packet overhead.
     * \return Packet overhead in bytes.
     */
    static WebRtc_UWord16 PacketOverhead();

private:
    WebRtc_Word32  _id;
    Packet*      _generatedFecPackets;
    ListWrapper     _fecPacketList;
    WebRtc_UWord16 _seqNumBase;
    bool         _lastMediaPacketReceived;
    bool         _fecPacketReceived;
};
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_
