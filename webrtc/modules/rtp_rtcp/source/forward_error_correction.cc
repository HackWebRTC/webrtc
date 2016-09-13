/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/rtp_rtcp/source/forward_error_correction.h"

#include <string.h>

#include <algorithm>
#include <iterator>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/byte_io.h"
#include "webrtc/modules/rtp_rtcp/source/forward_error_correction_internal.h"

namespace webrtc {

// FEC header size in bytes.
constexpr size_t kFecHeaderSize = 10;

// ULP header size in bytes (L bit is set).
constexpr size_t kUlpHeaderSizeLBitSet = (2 + kMaskSizeLBitSet);

// ULP header size in bytes (L bit is cleared).
constexpr size_t kUlpHeaderSizeLBitClear = (2 + kMaskSizeLBitClear);

// Transport header size in bytes. Assume UDP/IPv4 as a reasonable minimum.
constexpr size_t kTransportOverhead = 28;

// Maximum number of media packets that can be protected.
constexpr size_t ForwardErrorCorrection::kMaxMediaPackets;

// Maximum number of FEC packets stored internally.
constexpr size_t kMaxFecPackets = ForwardErrorCorrection::kMaxMediaPackets;

int32_t ForwardErrorCorrection::Packet::AddRef() {
  return ++ref_count_;
}

int32_t ForwardErrorCorrection::Packet::Release() {
  int32_t ref_count;
  ref_count = --ref_count_;
  if (ref_count == 0)
    delete this;
  return ref_count;
}

// This comparator is used to compare std::unique_ptr's pointing to
// subclasses of SortablePackets. It needs to be parametric since
// the std::unique_ptr's are not covariant w.r.t. the types that
// they are pointing to.
template <typename S, typename T>
bool ForwardErrorCorrection::SortablePacket::LessThan::operator() (
    const S& first,
    const T& second) {
  return IsNewerSequenceNumber(second->seq_num, first->seq_num);
}

ForwardErrorCorrection::ReceivedPacket::ReceivedPacket() {}
ForwardErrorCorrection::ReceivedPacket::~ReceivedPacket() {}

ForwardErrorCorrection::RecoveredPacket::RecoveredPacket() {}
ForwardErrorCorrection::RecoveredPacket::~RecoveredPacket() {}

ForwardErrorCorrection::ForwardErrorCorrection()
    : generated_fec_packets_(kMaxMediaPackets), received_fec_packets_(),
      packet_mask_(), tmp_packet_mask_() {}
ForwardErrorCorrection::~ForwardErrorCorrection() {}

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
//
// Note that any potential RED headers are added/removed before calling
// GenerateFec() or DecodeFec().
int ForwardErrorCorrection::GenerateFec(const PacketList& media_packets,
                                        uint8_t protection_factor,
                                        int num_important_packets,
                                        bool use_unequal_protection,
                                        FecMaskType fec_mask_type,
                                        std::list<Packet*>* fec_packets) {
  const uint16_t num_media_packets = media_packets.size();
  // Sanity check arguments.
  RTC_DCHECK_GT(num_media_packets, 0);
  RTC_DCHECK_GE(num_important_packets, 0);
  RTC_DCHECK_LE(num_important_packets, num_media_packets);
  RTC_DCHECK(fec_packets->empty());

  if (num_media_packets > kMaxMediaPackets) {
    LOG(LS_WARNING) << "Can't protect " << num_media_packets
                    << " media packets per frame. Max is " << kMaxMediaPackets
                    << ".";
    return -1;
  }

  bool l_bit = (num_media_packets > 8 * kMaskSizeLBitClear);
  int num_mask_bytes = l_bit ? kMaskSizeLBitSet : kMaskSizeLBitClear;

  // Error check the media packets.
  for (const auto& media_packet : media_packets) {
    RTC_DCHECK(media_packet);
    if (media_packet->length < kRtpHeaderSize) {
      LOG(LS_WARNING) << "Media packet " << media_packet->length << " bytes "
                      << "is smaller than RTP header.";
      return -1;
    }
    // Ensure the FEC packets will fit in a typical MTU.
    if (media_packet->length + MaxPacketOverhead() + kTransportOverhead >
        IP_PACKET_SIZE) {
      LOG(LS_WARNING) << "Media packet " << media_packet->length << " bytes "
                      << "with overhead is larger than " << IP_PACKET_SIZE
                      << " bytes.";
    }
  }

  int num_fec_packets = NumFecPackets(num_media_packets, protection_factor);
  if (num_fec_packets == 0) {
    return 0;
  }

  // Prepare generated FEC packets by setting them to 0.
  for (int i = 0; i < num_fec_packets; ++i) {
    memset(generated_fec_packets_[i].data, 0, IP_PACKET_SIZE);
    // Use this as a marker for untouched packets.
    generated_fec_packets_[i].length = 0;
    fec_packets->push_back(&generated_fec_packets_[i]);
  }

  const internal::PacketMaskTable mask_table(fec_mask_type, num_media_packets);

  // -- Generate packet masks --
  memset(packet_mask_, 0, num_fec_packets * num_mask_bytes);
  internal::GeneratePacketMasks(num_media_packets, num_fec_packets,
                                num_important_packets, use_unequal_protection,
                                mask_table, packet_mask_);

  int num_mask_bits = InsertZerosInBitMasks(
      media_packets, packet_mask_, num_mask_bytes, num_fec_packets);

  if (num_mask_bits < 0) {
    return -1;
  }
  l_bit = (static_cast<size_t>(num_mask_bits) > 8 * kMaskSizeLBitClear);
  if (l_bit) {
    num_mask_bytes = kMaskSizeLBitSet;
  }

  GenerateFecBitStrings(media_packets, packet_mask_, num_fec_packets, l_bit);
  GenerateFecUlpHeaders(media_packets, packet_mask_, num_fec_packets, l_bit);

  return 0;
}

int ForwardErrorCorrection::NumFecPackets(int num_media_packets,
                                          int protection_factor) {
  // Result in Q0 with an unsigned round.
  int num_fec_packets = (num_media_packets * protection_factor + (1 << 7)) >> 8;
  // Generate at least one FEC packet if we need protection.
  if (protection_factor > 0 && num_fec_packets == 0) {
    num_fec_packets = 1;
  }
  RTC_DCHECK_LE(num_fec_packets, num_media_packets);
  return num_fec_packets;
}

void ForwardErrorCorrection::GenerateFecBitStrings(
    const PacketList& media_packets,
    uint8_t* packet_mask,
    int num_fec_packets,
    bool l_bit) {
  RTC_DCHECK(!media_packets.empty());
  uint8_t media_payload_length[2];
  const int num_mask_bytes = l_bit ? kMaskSizeLBitSet : kMaskSizeLBitClear;
  const uint16_t ulp_header_size =
      l_bit ? kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear;
  const uint16_t fec_rtp_offset =
      kFecHeaderSize + ulp_header_size - kRtpHeaderSize;

  for (int i = 0; i < num_fec_packets; ++i) {
    Packet* const fec_packet = &generated_fec_packets_[i];
    auto media_packets_it = media_packets.cbegin();
    uint32_t pkt_mask_idx = i * num_mask_bytes;
    uint32_t media_pkt_idx = 0;
    uint16_t fec_packet_length = 0;
    uint16_t prev_seq_num = ParseSequenceNumber((*media_packets_it)->data);
    while (media_packets_it != media_packets.end()) {
      // Each FEC packet has a multiple byte mask. Determine if this media
      // packet should be included in FEC packet i.
      if (packet_mask[pkt_mask_idx] & (1 << (7 - media_pkt_idx))) {
        Packet* media_packet = media_packets_it->get();

        // Assign network-ordered media payload length.
        ByteWriter<uint16_t>::WriteBigEndian(
            media_payload_length, media_packet->length - kRtpHeaderSize);

        fec_packet_length = media_packet->length + fec_rtp_offset;
        // On the first protected packet, we don't need to XOR.
        if (fec_packet->length == 0) {
          // Copy the first 2 bytes of the RTP header. Note that the E and L
          // bits are overwritten in GenerateFecUlpHeaders.
          memcpy(&fec_packet->data[0], &media_packet->data[0], 2);
          // Copy the 5th to 8th bytes of the RTP header (timestamp).
          memcpy(&fec_packet->data[4], &media_packet->data[4], 4);
          // Copy network-ordered payload size.
          memcpy(&fec_packet->data[8], media_payload_length, 2);

          // Copy RTP payload, leaving room for the ULP header.
          memcpy(&fec_packet->data[kFecHeaderSize + ulp_header_size],
                 &media_packet->data[kRtpHeaderSize],
                 media_packet->length - kRtpHeaderSize);
        } else {
          // XOR with the first 2 bytes of the RTP header.
          fec_packet->data[0] ^= media_packet->data[0];
          fec_packet->data[1] ^= media_packet->data[1];

          // XOR with the 5th to 8th bytes of the RTP header.
          for (uint32_t j = 4; j < 8; ++j) {
            fec_packet->data[j] ^= media_packet->data[j];
          }

          // XOR with the network-ordered payload size.
          fec_packet->data[8] ^= media_payload_length[0];
          fec_packet->data[9] ^= media_payload_length[1];

          // XOR with RTP payload, leaving room for the ULP header.
          for (int32_t j = kFecHeaderSize + ulp_header_size;
               j < fec_packet_length; j++) {
            fec_packet->data[j] ^= media_packet->data[j - fec_rtp_offset];
          }
        }
        if (fec_packet_length > fec_packet->length) {
          fec_packet->length = fec_packet_length;
        }
      }
      media_packets_it++;
      if (media_packets_it != media_packets.end()) {
        uint16_t seq_num = ParseSequenceNumber((*media_packets_it)->data);
        media_pkt_idx += static_cast<uint16_t>(seq_num - prev_seq_num);
        prev_seq_num = seq_num;
      }
      pkt_mask_idx += media_pkt_idx / 8;
      media_pkt_idx %= 8;
    }
    RTC_DCHECK_GT(fec_packet->length, 0u)
        << "Packet mask is wrong or poorly designed.";
  }
}

int ForwardErrorCorrection::InsertZerosInBitMasks(
    const PacketList& media_packets,
    uint8_t* packet_mask,
    int num_mask_bytes,
    int num_fec_packets) {
  if (media_packets.size() <= 1) {
    return media_packets.size();
  }
  int last_seq_num = ParseSequenceNumber(media_packets.back()->data);
  int first_seq_num = ParseSequenceNumber(media_packets.front()->data);
  int total_missing_seq_nums =
      static_cast<uint16_t>(last_seq_num - first_seq_num) -
      media_packets.size() + 1;
  if (total_missing_seq_nums == 0) {
    // All sequence numbers are covered by the packet mask. No zero insertion
    // required.
    return media_packets.size();
  }
  // We can only protect 8 * kMaskSizeLBitSet packets.
  if (total_missing_seq_nums + media_packets.size() > 8 * kMaskSizeLBitSet)
    return -1;
  // Allocate the new mask.
  int new_mask_bytes = kMaskSizeLBitClear;
  if (media_packets.size() +
      total_missing_seq_nums > 8 * kMaskSizeLBitClear) {
    new_mask_bytes = kMaskSizeLBitSet;
  }
  memset(tmp_packet_mask_, 0, num_fec_packets * kMaskSizeLBitSet);

  auto media_packets_it = media_packets.cbegin();
  uint16_t prev_seq_num = first_seq_num;
  ++media_packets_it;

  // Insert the first column.
  internal::CopyColumn(tmp_packet_mask_, new_mask_bytes, packet_mask_,
                       num_mask_bytes, num_fec_packets, 0, 0);
  size_t new_bit_index = 1;
  size_t old_bit_index = 1;
  // Insert zeros in the bit mask for every hole in the sequence.
  while (media_packets_it != media_packets.end()) {
    if (new_bit_index == 8 * kMaskSizeLBitSet) {
      // We can only cover up to 48 packets.
      break;
    }
    uint16_t seq_num = ParseSequenceNumber((*media_packets_it)->data);
    const int num_zeros_to_insert =
        static_cast<uint16_t>(seq_num - prev_seq_num - 1);
    if (num_zeros_to_insert > 0) {
      internal::InsertZeroColumns(num_zeros_to_insert, tmp_packet_mask_,
                                  new_mask_bytes, num_fec_packets,
                                  new_bit_index);
    }
    new_bit_index += num_zeros_to_insert;
    internal::CopyColumn(tmp_packet_mask_, new_mask_bytes, packet_mask_,
                         num_mask_bytes, num_fec_packets, new_bit_index,
                         old_bit_index);
    ++new_bit_index;
    ++old_bit_index;
    prev_seq_num = seq_num;
    ++media_packets_it;
  }
  if (new_bit_index % 8 != 0) {
    // We didn't fill the last byte. Shift bits to correct position.
    for (uint16_t row = 0; row < num_fec_packets; ++row) {
      int new_byte_index = row * new_mask_bytes + new_bit_index / 8;
      tmp_packet_mask_[new_byte_index] <<= (7 - (new_bit_index % 8));
    }
  }
  // Replace the old mask with the new.
  memcpy(packet_mask, tmp_packet_mask_, kMaskSizeLBitSet * num_fec_packets);
  return new_bit_index;
}

void ForwardErrorCorrection::GenerateFecUlpHeaders(
    const PacketList& media_packets,
    uint8_t* packet_mask,
    int num_fec_packets,
    bool l_bit) {
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
  int num_mask_bytes = l_bit ? kMaskSizeLBitSet : kMaskSizeLBitClear;
  const uint16_t ulp_header_size =
      l_bit ? kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear;

  RTC_DCHECK(!media_packets.empty());
  Packet* first_media_packet = media_packets.front().get();
  RTC_DCHECK(first_media_packet);
  uint16_t seq_num = ParseSequenceNumber(first_media_packet->data);
  for (int i = 0; i < num_fec_packets; ++i) {
    Packet* const fec_packet = &generated_fec_packets_[i];
    // -- FEC header --
    fec_packet->data[0] &= 0x7f;  // Set E to zero.
    if (l_bit == 0) {
      fec_packet->data[0] &= 0xbf;  // Clear the L bit.
    } else {
      fec_packet->data[0] |= 0x40;  // Set the L bit.
    }
    // Sequence number from first media packet used as SN base.
    // We use the same sequence number base for every FEC packet,
    // but that's not required in general.
    ByteWriter<uint16_t>::WriteBigEndian(&fec_packet->data[2], seq_num);

    // -- ULP header --
    // Copy the payload size to the protection length field.
    // (We protect the entire packet.)
    ByteWriter<uint16_t>::WriteBigEndian(
        &fec_packet->data[10],
        fec_packet->length - kFecHeaderSize - ulp_header_size);

    // Copy the packet mask.
    memcpy(&fec_packet->data[12], &packet_mask[i * num_mask_bytes],
           num_mask_bytes);
  }
}

void ForwardErrorCorrection::ResetState(
    RecoveredPacketList* recovered_packets) {
  // Free the memory for any existing recovered packets, if the caller hasn't.
  recovered_packets->clear();
  received_fec_packets_.clear();
}

void ForwardErrorCorrection::InsertMediaPacket(
    ReceivedPacket* received_packet,
    RecoveredPacketList* recovered_packets) {

  // Search for duplicate packets.
  for (const auto& recovered_packet : *recovered_packets) {
    if (received_packet->seq_num == recovered_packet->seq_num) {
      // Duplicate packet, no need to add to list.
      // Delete duplicate media packet data.
      received_packet->pkt = nullptr;
      return;
    }
  }

  std::unique_ptr<RecoveredPacket> recovered_packet(new RecoveredPacket());
  // This "recovered packet" was not recovered using parity packets.
  recovered_packet->was_recovered = false;
  // This media packet has already been passed on.
  recovered_packet->returned = true;
  recovered_packet->seq_num = received_packet->seq_num;
  recovered_packet->pkt = received_packet->pkt;
  recovered_packet->pkt->length = received_packet->pkt->length;

  RecoveredPacket* recovered_packet_ptr = recovered_packet.get();
  // TODO(holmer): Consider replacing this with a binary search for the right
  // position, and then just insert the new packet. Would get rid of the sort.
  recovered_packets->push_back(std::move(recovered_packet));
  recovered_packets->sort(SortablePacket::LessThan());
  UpdateCoveringFecPackets(recovered_packet_ptr);
}

void ForwardErrorCorrection::UpdateCoveringFecPackets(RecoveredPacket* packet) {
  for (auto& fec_packet : received_fec_packets_) {
    // Is this FEC packet protecting the media packet |packet|?
    auto protected_it = std::lower_bound(fec_packet->protected_packets.begin(),
                                         fec_packet->protected_packets.end(),
                                         packet,
                                         SortablePacket::LessThan());
    if (protected_it != fec_packet->protected_packets.end() &&
        (*protected_it)->seq_num == packet->seq_num) {
      // Found an FEC packet which is protecting |packet|.
      (*protected_it)->pkt = packet->pkt;
    }
  }
}

void ForwardErrorCorrection::InsertFecPacket(
    ReceivedPacket* received_packet,
    const RecoveredPacketList* recovered_packets) {
  // Check for duplicate.
  for (const auto& existing_fec_packet : received_fec_packets_) {
    if (received_packet->seq_num == existing_fec_packet->seq_num) {
      // Delete duplicate FEC packet data.
      received_packet->pkt = nullptr;
      return;
    }
  }

  std::unique_ptr<ReceivedFecPacket> fec_packet(new ReceivedFecPacket());
  fec_packet->pkt = received_packet->pkt;
  fec_packet->seq_num = received_packet->seq_num;
  fec_packet->ssrc = received_packet->ssrc;

  const uint16_t seq_num_base =
      ByteReader<uint16_t>::ReadBigEndian(&fec_packet->pkt->data[2]);
  const uint16_t mask_size_bytes = (fec_packet->pkt->data[0] & 0x40)
                                      ? kMaskSizeLBitSet
                                      : kMaskSizeLBitClear;  // L bit set?

  // Parse erasure code mask from ULP header and represent as protected packets.
  for (uint16_t byte_idx = 0; byte_idx < mask_size_bytes; ++byte_idx) {
    uint8_t packet_mask = fec_packet->pkt->data[12 + byte_idx];
    for (uint16_t bit_idx = 0; bit_idx < 8; ++bit_idx) {
      if (packet_mask & (1 << (7 - bit_idx))) {
        std::unique_ptr<ProtectedPacket> protected_packet(
            new ProtectedPacket());
        // This wraps naturally with the sequence number.
        protected_packet->seq_num =
            static_cast<uint16_t>(seq_num_base + (byte_idx << 3) + bit_idx);
        protected_packet->pkt = nullptr;
        // Note that |protected_pkt_list| is sorted (according to sequence
        // number) by construction.
        fec_packet->protected_packets.push_back(std::move(protected_packet));
      }
    }
  }
  if (fec_packet->protected_packets.empty()) {
    // All-zero packet mask; we can discard this FEC packet.
    LOG(LS_WARNING) << "Received FEC packet has an all-zero packet mask.";
  } else {
    AssignRecoveredPackets(fec_packet.get(), recovered_packets);
    // TODO(holmer): Consider replacing this with a binary search for the right
    // position, and then just insert the new packet. Would get rid of the sort.
    //
    // For correct decoding, |fec_packet_list_| does not necessarily
    // need to be sorted by sequence number (see decoding algorithm in
    // AttemptRecover()), but by keeping it sorted we try to recover the
    // oldest lost packets first.
    received_fec_packets_.push_back(std::move(fec_packet));
    received_fec_packets_.sort(SortablePacket::LessThan());
    if (received_fec_packets_.size() > kMaxFecPackets) {
      received_fec_packets_.pop_front();
    }
    RTC_DCHECK_LE(received_fec_packets_.size(), kMaxFecPackets);
  }
}

void ForwardErrorCorrection::AssignRecoveredPackets(
    ReceivedFecPacket* fec_packet,
    const RecoveredPacketList* recovered_packets) {
  ProtectedPacketList* protected_packets = &fec_packet->protected_packets;
  std::vector<RecoveredPacket*> recovered_protected_packets;

  // Find intersection between the (sorted) containers |protected_packets|
  // and |recovered_packets|, i.e. all protected packets that have already
  // been recovered. Update the corresponding protected packets to point to
  // the recovered packets.
  auto it_p = protected_packets->cbegin();
  auto it_r = recovered_packets->cbegin();
  SortablePacket::LessThan less_than;
  while (it_p != protected_packets->end() && it_r != recovered_packets->end()) {
    if (less_than(*it_p, *it_r)) {
      ++it_p;
    } else if (less_than(*it_r, *it_p)) {
      ++it_r;
    } else {  // *it_p == *it_r.
      // This protected packet has already been recovered.
      (*it_p)->pkt = (*it_r)->pkt;
      ++it_p;
      ++it_r;
    }
  }
}

void ForwardErrorCorrection::InsertPackets(
    ReceivedPacketList* received_packets,
    RecoveredPacketList* recovered_packets) {
  while (!received_packets->empty()) {
    ReceivedPacket* received_packet = received_packets->front().get();

    // Check for discarding oldest FEC packet, to avoid wrong FEC decoding from
    // sequence number wrap-around. Detection of old FEC packet is based on
    // sequence number difference of received packet and oldest packet in FEC
    // packet list.
    // TODO(marpan/holmer): We should be able to improve detection/discarding of
    // old FEC packets based on timestamp information or better sequence number
    // thresholding (e.g., to distinguish between wrap-around and reordering).
    if (!received_fec_packets_.empty()) {
      uint16_t seq_num_diff =
          abs(static_cast<int>(received_packet->seq_num) -
              static_cast<int>(received_fec_packets_.front()->seq_num));
      if (seq_num_diff > 0x3fff) {
        received_fec_packets_.pop_front();
      }
    }

    if (received_packet->is_fec) {
      InsertFecPacket(received_packet, recovered_packets);
    } else {
      InsertMediaPacket(received_packet, recovered_packets);
    }
    // Delete the received packet "wrapper".
    received_packets->pop_front();
  }
  RTC_DCHECK(received_packets->empty());
  DiscardOldRecoveredPackets(recovered_packets);
}

bool ForwardErrorCorrection::StartPacketRecovery(
      const ReceivedFecPacket* fec_packet,
      RecoveredPacket* recovered_packet) {
  // This is the first packet which we try to recover with.
  const uint16_t ulp_header_size = fec_packet->pkt->data[0] & 0x40
                                       ? kUlpHeaderSizeLBitSet
                                       : kUlpHeaderSizeLBitClear;  // L bit set?
  if (fec_packet->pkt->length <
      static_cast<size_t>(kFecHeaderSize + ulp_header_size)) {
    LOG(LS_WARNING)
        << "Truncated FEC packet doesn't contain room for ULP header.";
    return false;
  }
  recovered_packet->pkt = new Packet();
  memset(recovered_packet->pkt->data, 0, IP_PACKET_SIZE);
  recovered_packet->returned = false;
  recovered_packet->was_recovered = true;
  uint16_t protection_length =
      ByteReader<uint16_t>::ReadBigEndian(&fec_packet->pkt->data[10]);
  if (protection_length >
      std::min(
          sizeof(recovered_packet->pkt->data) - kRtpHeaderSize,
          sizeof(fec_packet->pkt->data) - kFecHeaderSize - ulp_header_size)) {
    LOG(LS_WARNING) << "Incorrect FEC protection length, dropping.";
    return false;
  }
  // Copy FEC payload, skipping the ULP header.
  memcpy(&recovered_packet->pkt->data[kRtpHeaderSize],
         &fec_packet->pkt->data[kFecHeaderSize + ulp_header_size],
         protection_length);
  // Copy the length recovery field.
  memcpy(recovered_packet->length_recovery, &fec_packet->pkt->data[8], 2);
  // Copy the first 2 bytes of the FEC header.
  memcpy(recovered_packet->pkt->data, fec_packet->pkt->data, 2);
  // Copy the 5th to 8th bytes of the FEC header.
  memcpy(&recovered_packet->pkt->data[4], &fec_packet->pkt->data[4], 4);
  // Set the SSRC field.
  ByteWriter<uint32_t>::WriteBigEndian(&recovered_packet->pkt->data[8],
                                       fec_packet->ssrc);
  return true;
}

bool ForwardErrorCorrection::FinishPacketRecovery(
    RecoveredPacket* recovered_packet) {
  // Set the RTP version to 2.
  recovered_packet->pkt->data[0] |= 0x80;  // Set the 1st bit.
  recovered_packet->pkt->data[0] &= 0xbf;  // Clear the 2nd bit.

  // Set the SN field.
  ByteWriter<uint16_t>::WriteBigEndian(&recovered_packet->pkt->data[2],
                                       recovered_packet->seq_num);
  // Recover the packet length.
  recovered_packet->pkt->length =
      ByteReader<uint16_t>::ReadBigEndian(recovered_packet->length_recovery) +
      kRtpHeaderSize;
  if (recovered_packet->pkt->length >
      sizeof(recovered_packet->pkt->data) - kRtpHeaderSize) {
    return false;
  }

  return true;
}

void ForwardErrorCorrection::XorPackets(const Packet* src,
                                        RecoveredPacket* dst) {
  // XOR with the first 2 bytes of the RTP header.
  for (uint32_t i = 0; i < 2; ++i) {
    dst->pkt->data[i] ^= src->data[i];
  }
  // XOR with the 5th to 8th bytes of the RTP header.
  for (uint32_t i = 4; i < 8; ++i) {
    dst->pkt->data[i] ^= src->data[i];
  }
  // XOR with the network-ordered payload size.
  uint8_t media_payload_length[2];
  ByteWriter<uint16_t>::WriteBigEndian(media_payload_length,
                                       src->length - kRtpHeaderSize);
  dst->length_recovery[0] ^= media_payload_length[0];
  dst->length_recovery[1] ^= media_payload_length[1];

  // XOR with RTP payload.
  // TODO(marpan/ajm): Are we doing more XORs than required here?
  for (size_t i = kRtpHeaderSize; i < src->length; ++i) {
    dst->pkt->data[i] ^= src->data[i];
  }
}

bool ForwardErrorCorrection::RecoverPacket(
    const ReceivedFecPacket* fec_packet,
    RecoveredPacket* rec_packet_to_insert) {
  if (!StartPacketRecovery(fec_packet, rec_packet_to_insert))
    return false;
  for (const auto& protected_packet : fec_packet->protected_packets) {
    if (protected_packet->pkt == nullptr) {
      // This is the packet we're recovering.
      rec_packet_to_insert->seq_num = protected_packet->seq_num;
    } else {
      XorPackets(protected_packet->pkt, rec_packet_to_insert);
    }
  }
  if (!FinishPacketRecovery(rec_packet_to_insert))
    return false;
  return true;
}

void ForwardErrorCorrection::AttemptRecover(
    RecoveredPacketList* recovered_packets) {
  auto fec_packet_it = received_fec_packets_.begin();
  while (fec_packet_it != received_fec_packets_.end()) {
    // Search for each FEC packet's protected media packets.
    int packets_missing = NumCoveredPacketsMissing(fec_packet_it->get());

    // We can only recover one packet with an FEC packet.
    if (packets_missing == 1) {
      // Recovery possible.
      std::unique_ptr<RecoveredPacket> packet_to_insert(new RecoveredPacket());
      packet_to_insert->pkt = nullptr;
      if (!RecoverPacket(fec_packet_it->get(), packet_to_insert.get())) {
        // Can't recover using this packet, drop it.
        fec_packet_it = received_fec_packets_.erase(fec_packet_it);
        continue;
      }

      auto packet_to_insert_ptr = packet_to_insert.get();
      // Add recovered packet to the list of recovered packets and update any
      // FEC packets covering this packet with a pointer to the data.
      // TODO(holmer): Consider replacing this with a binary search for the
      // right position, and then just insert the new packet. Would get rid of
      // the sort.
      recovered_packets->push_back(std::move(packet_to_insert));
      recovered_packets->sort(SortablePacket::LessThan());
      UpdateCoveringFecPackets(packet_to_insert_ptr);
      DiscardOldRecoveredPackets(recovered_packets);
      fec_packet_it = received_fec_packets_.erase(fec_packet_it);

      // A packet has been recovered. We need to check the FEC list again, as
      // this may allow additional packets to be recovered.
      // Restart for first FEC packet.
      fec_packet_it = received_fec_packets_.begin();
    } else if (packets_missing == 0) {
      // Either all protected packets arrived or have been recovered. We can
      // discard this FEC packet.
      fec_packet_it = received_fec_packets_.erase(fec_packet_it);
    } else {
      fec_packet_it++;
    }
  }
}

int ForwardErrorCorrection::NumCoveredPacketsMissing(
    const ReceivedFecPacket* fec_packet) {
  int packets_missing = 0;
  for (const auto& protected_packet : fec_packet->protected_packets) {
    if (protected_packet->pkt == nullptr) {
      ++packets_missing;
      if (packets_missing > 1) {
        break;  // We can't recover more than one packet.
      }
    }
  }
  return packets_missing;
}

void ForwardErrorCorrection::DiscardOldRecoveredPackets(
    RecoveredPacketList* recovered_packets) {
  while (recovered_packets->size() > kMaxMediaPackets) {
    recovered_packets->pop_front();
  }
  RTC_DCHECK_LE(recovered_packets->size(), kMaxMediaPackets);
}

uint16_t ForwardErrorCorrection::ParseSequenceNumber(uint8_t* packet) {
  return (packet[2] << 8) + packet[3];
}

int ForwardErrorCorrection::DecodeFec(
    ReceivedPacketList* received_packets,
    RecoveredPacketList* recovered_packets) {
  // TODO(marpan/ajm): can we check for multiple ULP headers, and return an
  // error?
  if (recovered_packets->size() == kMaxMediaPackets) {
    const unsigned int seq_num_diff =
        abs(static_cast<int>(received_packets->front()->seq_num) -
            static_cast<int>(recovered_packets->back()->seq_num));
    if (seq_num_diff > kMaxMediaPackets) {
      // A big gap in sequence numbers. The old recovered packets
      // are now useless, so it's safe to do a reset.
      ResetState(recovered_packets);
    }
  }
  InsertPackets(received_packets, recovered_packets);
  AttemptRecover(recovered_packets);
  return 0;
}

size_t ForwardErrorCorrection::MaxPacketOverhead() const {
  return kFecHeaderSize + kUlpHeaderSizeLBitSet;
}
}  // namespace webrtc
