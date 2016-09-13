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

#include <stdint.h>

#include <list>
#include <memory>
#include <vector>

#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction_internal.h"
#include "webrtc/typedefs.h"

namespace webrtc {

// Performs codec-independent forward error correction (FEC), based on RFC 5109.
// Option exists to enable unequal protection (UEP) across packets.
// This is not to be confused with protection within packets
// (referred to as uneven level protection (ULP) in RFC 5109).
class ForwardErrorCorrection {
 public:
  // Maximum number of media packets we can protect
  static constexpr size_t kMaxMediaPackets = 48u;

  // TODO(holmer): As a next step all these struct-like packet classes should be
  // refactored into proper classes, and their members should be made private.
  // This will require parts of the functionality in forward_error_correction.cc
  // and receiver_fec.cc to be refactored into the packet classes.
  class Packet {
   public:
    Packet() : length(0), data(), ref_count_(0) {}
    virtual ~Packet() {}

    // Add a reference.
    virtual int32_t AddRef();

    // Release a reference. Will delete the object if the reference count
    // reaches zero.
    virtual int32_t Release();

    size_t length;                 // Length of packet in bytes.
    uint8_t data[IP_PACKET_SIZE];  // Packet data.

   private:
    int32_t ref_count_;  // Counts the number of references to a packet.
  };

  // TODO(holmer): Refactor into a proper class.
  class SortablePacket {
   public:
    // Functor which returns true if the sequence number of |first|
    // is < the sequence number of |second|.
    struct LessThan {
      template <typename S, typename T>
      bool operator() (const S& first, const T& second);
    };

    uint16_t seq_num;
  };

  // The received list parameter of DecodeFec() references structs of this type.
  //
  // The ssrc member is needed to ensure that we can restore the SSRC field of
  // recovered packets. In most situations this could be retrieved from other
  // media packets, but in the case of an FEC packet protecting a single
  // missing media packet, we have no other means of obtaining it.
  // TODO(holmer): Refactor into a proper class.
  class ReceivedPacket : public SortablePacket {
   public:
    ReceivedPacket();
    ~ReceivedPacket();

    uint32_t ssrc;  // SSRC of the current frame. Must be set for FEC
                    // packets, but not required for media packets.
    bool is_fec;    // Set to true if this is an FEC packet and false
                    // otherwise.
    rtc::scoped_refptr<Packet> pkt;  // Pointer to the packet storage.
  };

  // The recovered list parameter of #DecodeFec() references structs of
  // this type.
  // TODO(holmer): Refactor into a proper class.
  class RecoveredPacket : public SortablePacket {
   public:
    RecoveredPacket();
    ~RecoveredPacket();

    bool was_recovered;  // Will be true if this packet was recovered by
                         // the FEC. Otherwise it was a media packet passed in
                         // through the received packet list.
    bool returned;  // True when the packet already has been returned to the
                    // caller through the callback.
    uint8_t length_recovery[2];  // Two bytes used for recovering the packet
                                 // length with XOR operations.
    rtc::scoped_refptr<Packet> pkt;  // Pointer to the packet storage.
  };

  using PacketList = std::list<std::unique_ptr<Packet>>;
  using ReceivedPacketList = std::list<std::unique_ptr<ReceivedPacket>>;
  using RecoveredPacketList = std::list<std::unique_ptr<RecoveredPacket>>;

  ForwardErrorCorrection();
  virtual ~ForwardErrorCorrection();

  //
  // Generates a list of FEC packets from supplied media packets.
  //
  // Input:  media_packets          List of media packets to protect, of type
  //                                Packet. All packets must belong to the
  //                                same frame and the list must not be empty.
  // Input:  protection_factor      FEC protection overhead in the [0, 255]
  //                                domain. To obtain 100% overhead, or an
  //                                equal number of FEC packets as
  //                                media packets, use 255.
  // Input:  num_important_packets  The number of "important" packets in the
  //                                frame. These packets may receive greater
  //                                protection than the remaining packets.
  //                                The important packets must be located at the
  //                                start of the media packet list. For codecs
  //                                with data partitioning, the important
  //                                packets may correspond to first partition
  //                                packets.
  // Input:  use_unequal_protection Parameter to enable/disable unequal
  //                                protection (UEP) across packets. Enabling
  //                                UEP will allocate more protection to the
  //                                num_important_packets from the start of the
  //                                media_packets.
  // Input:  fec_mask_type          The type of packet mask used in the FEC.
  //                                Random or bursty type may be selected. The
  //                                bursty type is only defined up to 12 media
  //                                packets. If the number of media packets is
  //                                above 12, the packet masks from the random
  //                                table will be selected.
  // Output: fec_packets            List of pointers to generated FEC packets,
  //                                of type Packet. Must be empty on entry.
  //                                The memory available through the list will
  //                                be valid until the next call to
  //                                GenerateFec().
  //
  // Returns 0 on success, -1 on failure.
  //
  int GenerateFec(const PacketList& media_packets,
                  uint8_t protection_factor, int num_important_packets,
                  bool use_unequal_protection, FecMaskType fec_mask_type,
                  std::list<Packet*>* fec_packets);

  //
  // Decodes a list of received media and FEC packets. It will parse the
  // |received_packets|, storing FEC packets internally, and move
  // media packets to |recovered_packets|. The recovered list will be
  // sorted by ascending sequence number and have duplicates removed.
  // The function should be called as new packets arrive, and
  // |recovered_packets| will be progressively assembled with each call.
  // When the function returns, |received_packets| will be empty.
  //
  // The caller will allocate packets submitted through |received_packets|.
  // The function will handle allocation of recovered packets.
  //
  // Input:  received_packets   List of new received packets, of type
  //                            ReceivedPacket, belonging to a single
  //                            frame. At output the list will be empty,
  //                            with packets either stored internally,
  //                            or accessible through the recovered list.
  // Output: recovered_packets  List of recovered media packets, of type
  //                            RecoveredPacket, belonging to a single
  //                            frame. The memory available through the
  //                            list will be valid until the next call to
  //                            DecodeFec().
  //
  // Returns 0 on success, -1 on failure.
  //
  int DecodeFec(ReceivedPacketList* received_packets,
                RecoveredPacketList* recovered_packets);

  // Get the number of generated FEC packets, given the number of media packets
  // and the protection factor.
  static int NumFecPackets(int num_media_packets, int protection_factor);

  // Gets the maximum size of the FEC headers in bytes, which must be
  // accounted for as packet overhead.
  size_t MaxPacketOverhead() const;

  // Reset internal states from last frame and clear |recovered_packets|.
  // Frees all memory allocated by this class.
  void ResetState(RecoveredPacketList* recovered_packets);

 private:
  // Used to link media packets to their protecting FEC packets.
  //
  // TODO(holmer): Refactor into a proper class.
  class ProtectedPacket : public ForwardErrorCorrection::SortablePacket {
   public:
    rtc::scoped_refptr<ForwardErrorCorrection::Packet> pkt;
  };

  using ProtectedPacketList = std::list<std::unique_ptr<ProtectedPacket>>;

  // Used for internal storage of received FEC packets in a list.
  //
  // TODO(holmer): Refactor into a proper class.
  class ReceivedFecPacket : public ForwardErrorCorrection::SortablePacket {
   public:
    ProtectedPacketList protected_packets;
    uint32_t ssrc;  // SSRC of the current frame.
    rtc::scoped_refptr<ForwardErrorCorrection::Packet> pkt;
  };

  using ReceivedFecPacketList = std::list<std::unique_ptr<ReceivedFecPacket>>;

  // Analyzes |media_packets| for holes in the sequence and inserts zero columns
  // into the |packet_mask| where those holes are found. Zero columns means that
  // those packets will have no protection.
  // Returns the number of bits used for one row of the new packet mask.
  // Requires that |packet_mask| has at least 6 * |num_fec_packets| bytes
  // allocated.
  int InsertZerosInBitMasks(const PacketList& media_packets,
                            uint8_t* packet_mask, int num_mask_bytes,
                            int num_fec_packets);


  void GenerateFecUlpHeaders(const PacketList& media_packets,
                             uint8_t* packet_mask, int num_fec_packets,
                             bool l_bit);

  void GenerateFecBitStrings(const PacketList& media_packets,
                             uint8_t* packet_mask, int num_fec_packets,
                             bool l_bit);

  // Inserts the |received_packets| into the internal received FEC packet list
  // or into |recovered_packets|.
  void InsertPackets(ReceivedPacketList* received_packets,
                     RecoveredPacketList* recovered_packets);

  // Inserts the |received_packet| into |recovered_packets|. Deletes duplicates.
  void InsertMediaPacket(ReceivedPacket* received_packet,
                         RecoveredPacketList* recovered_packets);

  // Assigns pointers to the recovered packet from all FEC packets which cover
  // it.
  // Note: This reduces the complexity when we want to try to recover a packet
  // since we don't have to find the intersection between recovered packets and
  // packets covered by the FEC packet.
  void UpdateCoveringFecPackets(RecoveredPacket* packet);

  // Insert |received_packet| into internal FEC list. Deletes duplicates.
  void InsertFecPacket(ReceivedPacket* received_packet,
                       const RecoveredPacketList* recovered_packets);

  // Assigns pointers to already recovered packets covered by |fec_packet|.
  static void AssignRecoveredPackets(
      ReceivedFecPacket* fec_packet,
      const RecoveredPacketList* recovered_packets);

  // Insert |rec_packet_to_insert| into |recovered_packets| in correct position.
  void InsertRecoveredPacket(RecoveredPacket* rec_packet_to_insert,
                             RecoveredPacketList* recovered_packets);

  // Attempt to recover missing packets, using the internally stored
  // received FEC packets.
  void AttemptRecover(RecoveredPacketList* recovered_packets);

  // Initializes packet recovery using the received |fec_packet|.
  static bool StartPacketRecovery(const ReceivedFecPacket* fec_packet,
                                  RecoveredPacket* recovered_packet);

  // Performs XOR between |src| and |dst| and stores the result in |dst|.
  static void XorPackets(const Packet* src, RecoveredPacket* dst);

  // Finish up the recovery of a packet.
  static bool FinishPacketRecovery(RecoveredPacket* recovered_packet);

  // Recover a missing packet.
  bool RecoverPacket(const ReceivedFecPacket* fec_packet,
                     RecoveredPacket* rec_packet_to_insert);

  // Get the number of missing media packets which are covered by |fec_packet|.
  // An FEC packet can recover at most one packet, and if zero packets are
  // missing the FEC packet can be discarded. This function returns 2 when two
  // or more packets are missing.
  static int NumCoveredPacketsMissing(const ReceivedFecPacket* fec_packet);

  // Discards old packets in |recovered_packets|, which are no longer relevant
  // for recovering lost packets.
  static void DiscardOldRecoveredPackets(
      RecoveredPacketList* recovered_packets);
  static uint16_t ParseSequenceNumber(uint8_t* packet);

  std::vector<Packet> generated_fec_packets_;
  ReceivedFecPacketList received_fec_packets_;

  // Arrays used to avoid dynamically allocating memory when generating
  // the packet masks in the ULPFEC headers.
  // (There are never more than |kMaxMediaPackets| FEC packets generated.)
  uint8_t packet_mask_[kMaxMediaPackets * kMaskSizeLBitSet];
  uint8_t tmp_packet_mask_[kMaxMediaPackets * kMaskSizeLBitSet];
};
}  // namespace webrtc
#endif  // WEBRTC_MODULES_RTP_RTCP_SOURCE_FORWARD_ERROR_CORRECTION_H_
