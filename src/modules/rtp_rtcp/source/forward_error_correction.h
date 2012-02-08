/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_

#include <list>
#include <vector>

#include "typedefs.h"
#include "rtp_rtcp_defines.h"

namespace webrtc {

// Forward declaration.
struct FecPacket;

/**
 * Performs codec-independent forward error correction (FEC), based on RFC 5109.
 * Option exists to enable unequal protection (UEP) across packets.
 * This is not to be confused with protection within packets
 * (referred to as uneven level protection (ULP) in RFC 5109).
 */
class ForwardErrorCorrection {
 public:
  // Maximum number of media packets we can protect
  static const int kMaxMediaPackets = 48;

  struct Packet {
    uint16_t length;               /**> Length of packet in bytes. */
    uint8_t data[IP_PACKET_SIZE];  /**> Packet data. */
  };

  /**
   * The received list parameter of #DecodeFEC() must reference structs of this
   * type. The lastMediaPktInFrame is not required to be used for correct
   * recovery, but will reduce delay by allowing #DecodeFEC() to pre-emptively
   * determine frame completion. If set, we assume a FEC stream, and the
   * following assumptions must hold:\n
   *
   * 1. The media packets in a frame have contiguous sequence numbers, i.e. the
   *    frame's FEC packets have sequence numbers either lower than the first
   *    media packet or higher than the last media packet.\n
   * 2. All FEC packets have a sequence number base equal to the first media
   *    packet in the corresponding frame.\n
   *
   * The ssrc member is needed to ensure we can restore the SSRC field of
   * recovered packets. In most situations this could be retrieved from other
   * media packets, but in the case of an FEC packet protecting a single
   * missing media packet, we have no other means of obtaining it.
   */
  struct ReceivedPacket {
    uint16_t seqNum;    /**> Sequence number of packet. */
    uint32_t ssrc;      /**> SSRC of the current frame. Must be set for FEC
                             packets, but not required for media packets. */
    bool isFec;          /**> Set to true if this is an FEC packet and false
                              otherwise. */
    bool lastMediaPktInFrame; /**> Set to true to mark the last media packet in
                                   the frame and false otherwise. */
    Packet* pkt;              /**> Pointer to the packet storage. */
  };

  /**
   * The recovered list parameter of #DecodeFEC() will reference structs of
   * this type.
   */
  struct RecoveredPacket {
    bool wasRecovered;  /**> Will be true if this packet was recovered by
                             the FEC. Otherwise it was a media packet passed in
                             through the received packet list. */
    uint16_t seqNum;    /**> Sequence number of the packet. This is mostly for
                             implementation convenience but could be utilized
                             by the user if so desired. */
    Packet* pkt;        /**> Pointer to the packet storage. */
  };

  /**
   * \param[in] id Module ID
   */
  ForwardErrorCorrection(int32_t id);

  virtual ~ForwardErrorCorrection();

  /**
   * Generates a list of FEC packets from supplied media packets.
   *
   * \param[in]  mediaPacketList     List of media packets to protect, of type
   *                                 #Packet. All packets must belong to the
   *                                 same frame and the list must not be empty.
   * \param[in]  protectionFactor    FEC protection overhead in the [0, 255]
   *                                 domain. To obtain 100% overhead, or an
   *                                 equal number of FEC packets as media
   *                                 packets, use 255.
   * \param[in] numImportantPackets  The number of "important" packets in the
   *                                 frame. These packets may receive greater
   *                                 protection than the remaining packets. The
   *                                 important packets must be located at the
   *                                 start of the media packet list. For codecs
   *                                 with data partitioning, the important
   *                                 packets may correspond to first partition
   *                                 packets.
   * \param[in] useUnequalProtection Parameter to enable/disable unequal
   *                                 protection  (UEP) across packets. Enabling
   *                                 UEP will allocate more protection to the
   *                                 numImportantPackets from the start of the
   *                                 mediaPacketList.
   * \param[out] fecPacketList       List of FEC packets, of type #Packet. Must
   *                                 be empty on entry. The memory available
   *                                 through the list will be valid until the
   *                                 next call to GenerateFEC().
   *
   * \return 0 on success, -1 on failure.
   */
  int32_t GenerateFEC(const std::list<Packet*>& mediaPacketList,
                      uint8_t protectionFactor,
                      int numImportantPackets,
                      bool useUnequalProtection,
                      std::list<Packet*>* fecPacketList);

  /**
   *  Decodes a list of media and FEC packets. It will parse the input received
   *  packet list, storing FEC packets internally and inserting media packets to
   *  the output recovered packet list. The recovered list will be sorted by
   *  as cending sequence number and have duplicates removed. The function
   *  should be called as new packets arrive, with the recovered list being
   *  progressively assembled with each call. The received packet list will be
   *  empty at output.\n
   *
   *  The user will allocate packets submitted through the received list. The
   *  function will handle allocation of recovered packets and optionally
   *  deleting of all packet memory. The user may delete the recovered list
   *  packets, in which case they must remove deleted packets from the
   *  recovered list.\n
   *
   *  Before deleting an instance of the class, call the function with an empty
   *  received packet list and the completion parameter set to true. This will
   *  free any outstanding memory.
   *
   * \param[in]  receivedPacketList  List of new received packets, of type
   *                                 #ReceivedPacket, beloning to a single
   *                                 frame. At output the list will be empty,
   *                                 with packets  either stored internally,
   *                                 or accessible through the recovered list.
   * \param[out] recoveredPacketList List of recovered media packets, of type
   *                                 #RecoveredPacket, belonging to a single
   *                                 frame. The memory available through the
   *                                 list will be valid until the next call to
   *                                 DecodeFEC() in which the completion
   *                                 parameter is set to true.
   * \param[in] lastFECSeqNum        Estimated last seqNumber before this frame.
   * \param[in,out] frameComplete    Set to true on input to indicate the start
   *                                 of a new frame. On output, this will be
   *                                 set to true if all media packets in the
   *                                 frame have been recovered. Note that the
   *                                 frame may be complete without this
   *                                 parameter having been set, as it may not
   *                                 always be possible to determine frame
   *                                 completion.
   *
   * \return 0 on success, -1 on failure.
   */
  int32_t DecodeFEC(std::list<ReceivedPacket*>* receivedPacketList,
                    std::list<RecoveredPacket*>* recoveredPacketList,
                    uint16_t lastFECSeqNum,
                    bool& frameComplete);
  /**
   * Gets the size in bytes of the FEC/ULP headers, which must be accounted for
   * as packet overhead.
   * \return Packet overhead in bytes.
   */
  static uint16_t PacketOverhead();

 private:
  // True if first is <= than second.
  static bool CompareRecoveredPackets(RecoveredPacket* first,
                                      RecoveredPacket* second);

  void GenerateFecUlpHeaders(const std::list<Packet*>& mediaPacketList,
                             uint8_t* packetMask,
                             uint32_t numFecPackets);

  void GenerateFecBitStrings(const std::list<Packet*>& mediaPacketList,
                             uint8_t* packetMask,
                             uint32_t numFecPackets);

  // Reset internal states from last frame and clear the recoveredPacketList.
  void ResetState(std::list<RecoveredPacket*>* recoveredPacketList);

  // Insert received packets into FEC or recovered list.
  void InsertPackets(std::list<ReceivedPacket*>* receivedPacketList,
                     std::list<RecoveredPacket*>* recoveredPacketList);

  // Insert media packet into recovered packet list. We delete duplicates.
  void InsertMediaPacket(ReceivedPacket* rxPacket,
                         std::list<RecoveredPacket*>* recoveredPacketList);

  // Insert packet into FEC list. We delete duplicates.
  void InsertFECPacket(ReceivedPacket* rxPacket);

  // Insert into recovered list in correct position.
  void InsertRecoveredPacket(
      RecoveredPacket* recPacketToInsert,
      std::list<RecoveredPacket*>* recoveredPacketList);

  // Attempt to recover missing packets.
  void AttemptRecover(std::list<RecoveredPacket*>* recoveredPacketList);

  // Recover a missing packet.
  void RecoverPacket(const FecPacket& fecPacket,
                     RecoveredPacket* recPacketToInsert);

  // Get number of protected packet in the fecPacket.
  uint32_t NumberOfProtectedPackets(
      const FecPacket& fecPacket,
      std::list<RecoveredPacket*>* recoveredPacketList);

  int32_t _id;
  std::vector<Packet> _generatedFecPackets;
  std::list<FecPacket*> _fecPacketList;
  uint16_t _seqNumBase;
  bool _lastMediaPacketReceived;
  bool _fecPacketReceived;
};
} // namespace webrtc
#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_
