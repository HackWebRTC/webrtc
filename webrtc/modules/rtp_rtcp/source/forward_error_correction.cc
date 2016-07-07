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
#include <memory>

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

// Used to link media packets to their protecting FEC packets.
//
// TODO(holmer): Refactor into a proper class.
class ProtectedPacket : public ForwardErrorCorrection::SortablePacket {
 public:
  rtc::scoped_refptr<ForwardErrorCorrection::Packet> pkt;
};

typedef std::list<ProtectedPacket*> ProtectedPacketList;

//
// Used for internal storage of FEC packets in a list.
//
// TODO(holmer): Refactor into a proper class.
class FecPacket : public ForwardErrorCorrection::SortablePacket {
 public:
  ProtectedPacketList protected_pkt_list;
  uint32_t ssrc;  // SSRC of the current frame.
  rtc::scoped_refptr<ForwardErrorCorrection::Packet> pkt;
};

bool ForwardErrorCorrection::SortablePacket::LessThan(
    const SortablePacket* first,
    const SortablePacket* second) {
  return IsNewerSequenceNumber(second->seq_num, first->seq_num);
}

ForwardErrorCorrection::ReceivedPacket::ReceivedPacket() {}
ForwardErrorCorrection::ReceivedPacket::~ReceivedPacket() {}

ForwardErrorCorrection::RecoveredPacket::RecoveredPacket() {}
ForwardErrorCorrection::RecoveredPacket::~RecoveredPacket() {}

ForwardErrorCorrection::ForwardErrorCorrection()
    : generated_fec_packets_(kMaxMediaPackets), fec_packet_list_(),
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
int ForwardErrorCorrection::GenerateFec(const PacketList& media_packet_list,
                                        uint8_t protection_factor,
                                        int num_important_packets,
                                        bool use_unequal_protection,
                                        FecMaskType fec_mask_type,
                                        PacketList* fec_packet_list) {
  const uint16_t num_media_packets = media_packet_list.size();
  // Sanity check arguments.
  RTC_DCHECK_GT(num_media_packets, 0);
  RTC_DCHECK_GE(num_important_packets, 0);
  RTC_DCHECK_LE(num_important_packets, num_media_packets);
  RTC_DCHECK(fec_packet_list->empty());

  if (num_media_packets > kMaxMediaPackets) {
    LOG(LS_WARNING) << "Can't protect " << num_media_packets
                    << " media packets per frame. Max is " << kMaxMediaPackets;
    return -1;
  }

  bool l_bit = (num_media_packets > 8 * kMaskSizeLBitClear);
  int num_mask_bytes = l_bit ? kMaskSizeLBitSet : kMaskSizeLBitClear;

  // Do some error checking on the media packets.
  for (Packet* media_packet : media_packet_list) {
    RTC_DCHECK(media_packet);

    if (media_packet->length < kRtpHeaderSize) {
      LOG(LS_WARNING) << "Media packet " << media_packet->length << " bytes "
                      << "is smaller than RTP header.";
      return -1;
    }

    // Ensure our FEC packets will fit in a typical MTU.
    if (media_packet->length + PacketOverhead() + kTransportOverhead >
        IP_PACKET_SIZE) {
      LOG(LS_WARNING) << "Media packet " << media_packet->length << " bytes "
                      << "with overhead is larger than " << IP_PACKET_SIZE;
    }
  }

  int num_fec_packets =
      GetNumberOfFecPackets(num_media_packets, protection_factor);
  if (num_fec_packets == 0) {
    return 0;
  }

  // Prepare FEC packets by setting them to 0.
  for (int i = 0; i < num_fec_packets; ++i) {
    memset(generated_fec_packets_[i].data, 0, IP_PACKET_SIZE);
    generated_fec_packets_[i].length = 0;  // Use this as a marker for untouched
                                           // packets.
    fec_packet_list->push_back(&generated_fec_packets_[i]);
  }

  const internal::PacketMaskTable mask_table(fec_mask_type, num_media_packets);

  // -- Generate packet masks --
  memset(packet_mask_, 0, num_fec_packets * num_mask_bytes);
  internal::GeneratePacketMasks(num_media_packets, num_fec_packets,
                                num_important_packets, use_unequal_protection,
                                mask_table, packet_mask_);

  int num_mask_bits = InsertZerosInBitMasks(
      media_packet_list, packet_mask_, num_mask_bytes, num_fec_packets);

  if (num_mask_bits < 0) {
    return -1;
  }
  l_bit = (static_cast<size_t>(num_mask_bits) > 8 * kMaskSizeLBitClear);
  if (l_bit) {
    num_mask_bytes = kMaskSizeLBitSet;
  }

  GenerateFecBitStrings(media_packet_list, packet_mask_,
                        num_fec_packets, l_bit);
  GenerateFecUlpHeaders(media_packet_list, packet_mask_,
                        num_fec_packets, l_bit);

  return 0;
}

int ForwardErrorCorrection::GetNumberOfFecPackets(int num_media_packets,
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
    const PacketList& media_packet_list,
    uint8_t* packet_mask,
    int num_fec_packets,
    bool l_bit) {
  RTC_DCHECK(!media_packet_list.empty());
  uint8_t media_payload_length[2];
  const int num_mask_bytes = l_bit ? kMaskSizeLBitSet : kMaskSizeLBitClear;
  const uint16_t ulp_header_size =
      l_bit ? kUlpHeaderSizeLBitSet : kUlpHeaderSizeLBitClear;
  const uint16_t fec_rtp_offset =
      kFecHeaderSize + ulp_header_size - kRtpHeaderSize;

  for (int i = 0; i < num_fec_packets; ++i) {
    Packet* const fec_packet = &generated_fec_packets_[i];
    auto media_list_it = media_packet_list.cbegin();
    uint32_t pkt_mask_idx = i * num_mask_bytes;
    uint32_t media_pkt_idx = 0;
    uint16_t fec_packet_length = 0;
    uint16_t prev_seq_num = ParseSequenceNumber((*media_list_it)->data);
    while (media_list_it != media_packet_list.end()) {
      // Each FEC packet has a multiple byte mask. Determine if this media
      // packet should be included in FEC packet i.
      if (packet_mask[pkt_mask_idx] & (1 << (7 - media_pkt_idx))) {
        Packet* media_packet = *media_list_it;

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
      media_list_it++;
      if (media_list_it != media_packet_list.end()) {
        uint16_t seq_num = ParseSequenceNumber((*media_list_it)->data);
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
  if (media_packets.size() + total_missing_seq_nums > 8 * kMaskSizeLBitClear) {
    new_mask_bytes = kMaskSizeLBitSet;
  }
  memset(tmp_packet_mask_, 0, num_fec_packets * kMaskSizeLBitSet);

  auto it = media_packets.cbegin();
  uint16_t prev_seq_num = first_seq_num;
  ++it;

  // Insert the first column.
  CopyColumn(tmp_packet_mask_, new_mask_bytes, packet_mask, num_mask_bytes,
             num_fec_packets, 0, 0);
  int new_bit_index = 1;
  int old_bit_index = 1;
  // Insert zeros in the bit mask for every hole in the sequence.
  for (; it != media_packets.end(); ++it) {
    if (new_bit_index == 8 * kMaskSizeLBitSet) {
      // We can only cover up to 48 packets.
      break;
    }
    uint16_t seq_num = ParseSequenceNumber((*it)->data);
    const int zeros_to_insert =
        static_cast<uint16_t>(seq_num - prev_seq_num - 1);
    if (zeros_to_insert > 0) {
      InsertZeroColumns(zeros_to_insert, tmp_packet_mask_, new_mask_bytes,
                        num_fec_packets, new_bit_index);
    }
    new_bit_index += zeros_to_insert;
    CopyColumn(tmp_packet_mask_, new_mask_bytes, packet_mask, num_mask_bytes,
               num_fec_packets, new_bit_index, old_bit_index);
    ++new_bit_index;
    ++old_bit_index;
    prev_seq_num = seq_num;
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

void ForwardErrorCorrection::InsertZeroColumns(int num_zeros,
                                               uint8_t* new_mask,
                                               int new_mask_bytes,
                                               int num_fec_packets,
                                               int new_bit_index) {
  for (uint16_t row = 0; row < num_fec_packets; ++row) {
    const int new_byte_index = row * new_mask_bytes + new_bit_index / 8;
    const int max_shifts = (7 - (new_bit_index % 8));
    new_mask[new_byte_index] <<= std::min(num_zeros, max_shifts);
  }
}

void ForwardErrorCorrection::CopyColumn(uint8_t* new_mask,
                                        int new_mask_bytes,
                                        uint8_t* old_mask,
                                        int old_mask_bytes,
                                        int num_fec_packets,
                                        int new_bit_index,
                                        int old_bit_index) {
  // Copy column from the old mask to the beginning of the new mask and shift it
  // out from the old mask.
  for (uint16_t row = 0; row < num_fec_packets; ++row) {
    int new_byte_index = row * new_mask_bytes + new_bit_index / 8;
    int old_byte_index = row * old_mask_bytes + old_bit_index / 8;
    new_mask[new_byte_index] |= ((old_mask[old_byte_index] & 0x80) >> 7);
    if (new_bit_index % 8 != 7) {
      new_mask[new_byte_index] <<= 1;
    }
    old_mask[old_byte_index] <<= 1;
  }
}

void ForwardErrorCorrection::GenerateFecUlpHeaders(
    const PacketList& media_packet_list,
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

  RTC_DCHECK(!media_packet_list.empty());
  Packet* first_media_packet = media_packet_list.front();
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
    RecoveredPacketList* recovered_packet_list) {
  // Free the memory for any existing recovered packets, if the user hasn't.
  while (!recovered_packet_list->empty()) {
    delete recovered_packet_list->front();
    recovered_packet_list->pop_front();
  }
  RTC_DCHECK(recovered_packet_list->empty());

  // Free the FEC packet list.
  while (!fec_packet_list_.empty()) {
    FecPacket* fec_packet = fec_packet_list_.front();
    auto protected_packet_list_it = fec_packet->protected_pkt_list.begin();
    while (protected_packet_list_it != fec_packet->protected_pkt_list.end()) {
      delete *protected_packet_list_it;
      protected_packet_list_it =
          fec_packet->protected_pkt_list.erase(protected_packet_list_it);
    }
    RTC_DCHECK(fec_packet->protected_pkt_list.empty());
    delete fec_packet;
    fec_packet_list_.pop_front();
  }
  RTC_DCHECK(fec_packet_list_.empty());
}

void ForwardErrorCorrection::InsertMediaPacket(
    ReceivedPacket* rx_packet,
    RecoveredPacketList* recovered_packet_list) {
  auto recovered_packet_list_it = recovered_packet_list->cbegin();

  // Search for duplicate packets.
  while (recovered_packet_list_it != recovered_packet_list->end()) {
    if (rx_packet->seq_num == (*recovered_packet_list_it)->seq_num) {
      // Duplicate packet, no need to add to list.
      // Delete duplicate media packet data.
      rx_packet->pkt = nullptr;
      return;
    }
    ++recovered_packet_list_it;
  }
  RecoveredPacket* recovered_packet_to_insert = new RecoveredPacket();
  recovered_packet_to_insert->was_recovered = false;
  // Inserted Media packet is already sent to VCM.
  recovered_packet_to_insert->returned = true;
  recovered_packet_to_insert->seq_num = rx_packet->seq_num;
  recovered_packet_to_insert->pkt = rx_packet->pkt;
  recovered_packet_to_insert->pkt->length = rx_packet->pkt->length;

  // TODO(holmer): Consider replacing this with a binary search for the right
  // position, and then just insert the new packet. Would get rid of the sort.
  recovered_packet_list->push_back(recovered_packet_to_insert);
  recovered_packet_list->sort(SortablePacket::LessThan);
  UpdateCoveringFecPackets(recovered_packet_to_insert);
}

void ForwardErrorCorrection::UpdateCoveringFecPackets(RecoveredPacket* packet) {
  for (auto* fec_packet : fec_packet_list_) {
    // Is this FEC packet protecting the media packet |packet|?
    auto protected_it = std::lower_bound(fec_packet->protected_pkt_list.begin(),
                                         fec_packet->protected_pkt_list.end(),
                                         packet,
                                         SortablePacket::LessThan);
    if (protected_it != fec_packet->protected_pkt_list.end() &&
        (*protected_it)->seq_num == packet->seq_num) {
      // Found an FEC packet which is protecting |packet|.
      (*protected_it)->pkt = packet->pkt;
    }
  }
}

void ForwardErrorCorrection::InsertFecPacket(
    ReceivedPacket* rx_packet,
    const RecoveredPacketList* recovered_packet_list) {
  // Check for duplicate.
  for (auto* fec_packet : fec_packet_list_) {
    if (rx_packet->seq_num == fec_packet->seq_num) {
      // Delete duplicate FEC packet data.
      rx_packet->pkt = nullptr;
      return;
    }
  }
  FecPacket* fec_packet = new FecPacket();
  fec_packet->pkt = rx_packet->pkt;
  fec_packet->seq_num = rx_packet->seq_num;
  fec_packet->ssrc = rx_packet->ssrc;

  const uint16_t seq_num_base =
      ByteReader<uint16_t>::ReadBigEndian(&fec_packet->pkt->data[2]);
  const uint16_t maskSizeBytes = (fec_packet->pkt->data[0] & 0x40)
                                     ? kMaskSizeLBitSet
                                     : kMaskSizeLBitClear;  // L bit set?

  for (uint16_t byte_idx = 0; byte_idx < maskSizeBytes; ++byte_idx) {
    uint8_t packet_mask = fec_packet->pkt->data[12 + byte_idx];
    for (uint16_t bit_idx = 0; bit_idx < 8; ++bit_idx) {
      if (packet_mask & (1 << (7 - bit_idx))) {
        ProtectedPacket* protected_packet = new ProtectedPacket();
        fec_packet->protected_pkt_list.push_back(protected_packet);
        // This wraps naturally with the sequence number.
        protected_packet->seq_num =
            static_cast<uint16_t>(seq_num_base + (byte_idx << 3) + bit_idx);
        protected_packet->pkt = nullptr;
      }
    }
  }
  if (fec_packet->protected_pkt_list.empty()) {
    // All-zero packet mask; we can discard this FEC packet.
    LOG(LS_WARNING) << "FEC packet has an all-zero packet mask.";
    delete fec_packet;
  } else {
    AssignRecoveredPackets(fec_packet, recovered_packet_list);
    // TODO(holmer): Consider replacing this with a binary search for the right
    // position, and then just insert the new packet. Would get rid of the sort.
    fec_packet_list_.push_back(fec_packet);
    fec_packet_list_.sort(SortablePacket::LessThan);
    if (fec_packet_list_.size() > kMaxFecPackets) {
      DiscardFecPacket(fec_packet_list_.front());
      fec_packet_list_.pop_front();
    }
    RTC_DCHECK_LE(fec_packet_list_.size(), kMaxFecPackets);
  }
}

void ForwardErrorCorrection::AssignRecoveredPackets(
    FecPacket* fec_packet,
    const RecoveredPacketList* recovered_packets) {
  // Search for missing packets which have arrived or have been recovered by
  // another FEC packet.
  ProtectedPacketList* not_recovered = &fec_packet->protected_pkt_list;
  RecoveredPacketList already_recovered;
  std::set_intersection(
      recovered_packets->cbegin(), recovered_packets->cend(),
      not_recovered->cbegin(), not_recovered->cend(),
      std::inserter(already_recovered, already_recovered.end()),
      SortablePacket::LessThan);
  // Set the FEC pointers to all recovered packets so that we don't have to
  // search for them when we are doing recovery.
  auto not_recovered_it = not_recovered->cbegin();
  for (auto it = already_recovered.cbegin();
       it != already_recovered.end(); ++it) {
    // Search for the next recovered packet in |not_recovered|.
    while ((*not_recovered_it)->seq_num != (*it)->seq_num)
      ++not_recovered_it;
    (*not_recovered_it)->pkt = (*it)->pkt;
  }
}

void ForwardErrorCorrection::InsertPackets(
    ReceivedPacketList* received_packet_list,
    RecoveredPacketList* recovered_packet_list) {
  while (!received_packet_list->empty()) {
    ReceivedPacket* rx_packet = received_packet_list->front();

    // Check for discarding oldest FEC packet, to avoid wrong FEC decoding from
    // sequence number wrap-around. Detection of old FEC packet is based on
    // sequence number difference of received packet and oldest packet in FEC
    // packet list.
    // TODO(marpan/holmer): We should be able to improve detection/discarding of
    // old FEC packets based on timestamp information or better sequence number
    // thresholding (e.g., to distinguish between wrap-around and reordering).
    if (!fec_packet_list_.empty()) {
      uint16_t seq_num_diff =
          abs(static_cast<int>(rx_packet->seq_num) -
              static_cast<int>(fec_packet_list_.front()->seq_num));
      if (seq_num_diff > 0x3fff) {
        DiscardFecPacket(fec_packet_list_.front());
        fec_packet_list_.pop_front();
      }
    }

    if (rx_packet->is_fec) {
      InsertFecPacket(rx_packet, recovered_packet_list);
    } else {
      // Insert packet at the end of |recoveredPacketList|.
      InsertMediaPacket(rx_packet, recovered_packet_list);
    }
    // Delete the received packet "wrapper", but not the packet data.
    delete rx_packet;
    received_packet_list->pop_front();
  }
  RTC_DCHECK(received_packet_list->empty());
  DiscardOldPackets(recovered_packet_list);
}

bool ForwardErrorCorrection::StartPacketRecovery(const FecPacket* fec_packet,
                                                 RecoveredPacket* recovered) {
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
  recovered->pkt = new Packet();
  memset(recovered->pkt->data, 0, IP_PACKET_SIZE);
  recovered->returned = false;
  recovered->was_recovered = true;
  uint16_t protection_length =
      ByteReader<uint16_t>::ReadBigEndian(&fec_packet->pkt->data[10]);
  if (protection_length >
      std::min(
          sizeof(recovered->pkt->data) - kRtpHeaderSize,
          sizeof(fec_packet->pkt->data) - kFecHeaderSize - ulp_header_size)) {
    LOG(LS_WARNING) << "Incorrect FEC protection length, dropping.";
    return false;
  }
  // Copy FEC payload, skipping the ULP header.
  memcpy(&recovered->pkt->data[kRtpHeaderSize],
         &fec_packet->pkt->data[kFecHeaderSize + ulp_header_size],
         protection_length);
  // Copy the length recovery field.
  memcpy(recovered->length_recovery, &fec_packet->pkt->data[8], 2);
  // Copy the first 2 bytes of the FEC header.
  memcpy(recovered->pkt->data, fec_packet->pkt->data, 2);
  // Copy the 5th to 8th bytes of the FEC header.
  memcpy(&recovered->pkt->data[4], &fec_packet->pkt->data[4], 4);
  // Set the SSRC field.
  ByteWriter<uint32_t>::WriteBigEndian(&recovered->pkt->data[8],
                                       fec_packet->ssrc);
  return true;
}

bool ForwardErrorCorrection::FinishPacketRecovery(RecoveredPacket* recovered) {
  // Set the RTP version to 2.
  recovered->pkt->data[0] |= 0x80;  // Set the 1st bit.
  recovered->pkt->data[0] &= 0xbf;  // Clear the 2nd bit.

  // Set the SN field.
  ByteWriter<uint16_t>::WriteBigEndian(&recovered->pkt->data[2],
                                       recovered->seq_num);
  // Recover the packet length.
  recovered->pkt->length =
      ByteReader<uint16_t>::ReadBigEndian(recovered->length_recovery) +
      kRtpHeaderSize;
  if (recovered->pkt->length > sizeof(recovered->pkt->data) - kRtpHeaderSize)
    return false;

  return true;
}

void ForwardErrorCorrection::XorPackets(const Packet* src_packet,
                                        RecoveredPacket* dst_packet) {
  // XOR with the first 2 bytes of the RTP header.
  for (uint32_t i = 0; i < 2; ++i) {
    dst_packet->pkt->data[i] ^= src_packet->data[i];
  }
  // XOR with the 5th to 8th bytes of the RTP header.
  for (uint32_t i = 4; i < 8; ++i) {
    dst_packet->pkt->data[i] ^= src_packet->data[i];
  }
  // XOR with the network-ordered payload size.
  uint8_t media_payload_length[2];
  ByteWriter<uint16_t>::WriteBigEndian(media_payload_length,
                                       src_packet->length - kRtpHeaderSize);
  dst_packet->length_recovery[0] ^= media_payload_length[0];
  dst_packet->length_recovery[1] ^= media_payload_length[1];

  // XOR with RTP payload.
  // TODO(marpan/ajm): Are we doing more XORs than required here?
  for (size_t i = kRtpHeaderSize; i < src_packet->length; ++i) {
    dst_packet->pkt->data[i] ^= src_packet->data[i];
  }
}

bool ForwardErrorCorrection::RecoverPacket(
    const FecPacket* fec_packet,
    RecoveredPacket* rec_packet_to_insert) {
  if (!StartPacketRecovery(fec_packet, rec_packet_to_insert))
    return false;
  for (const auto* protected_packet : fec_packet->protected_pkt_list) {
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
    RecoveredPacketList* recovered_packet_list) {
  auto fec_packet_list_it = fec_packet_list_.begin();
  while (fec_packet_list_it != fec_packet_list_.end()) {
    // Search for each FEC packet's protected media packets.
    int packets_missing = NumCoveredPacketsMissing(*fec_packet_list_it);

    // We can only recover one packet with an FEC packet.
    if (packets_missing == 1) {
      // Recovery possible.
      RecoveredPacket* packet_to_insert = new RecoveredPacket();
      packet_to_insert->pkt = nullptr;
      if (!RecoverPacket(*fec_packet_list_it, packet_to_insert)) {
        // Can't recover using this packet, drop it.
        DiscardFecPacket(*fec_packet_list_it);
        fec_packet_list_it = fec_packet_list_.erase(fec_packet_list_it);
        delete packet_to_insert;
        continue;
      }

      // Add recovered packet to the list of recovered packets and update any
      // FEC packets covering this packet with a pointer to the data.
      // TODO(holmer): Consider replacing this with a binary search for the
      // right position, and then just insert the new packet. Would get rid of
      // the sort.
      recovered_packet_list->push_back(packet_to_insert);
      recovered_packet_list->sort(SortablePacket::LessThan);
      UpdateCoveringFecPackets(packet_to_insert);
      DiscardOldPackets(recovered_packet_list);
      DiscardFecPacket(*fec_packet_list_it);
      fec_packet_list_it = fec_packet_list_.erase(fec_packet_list_it);

      // A packet has been recovered. We need to check the FEC list again, as
      // this may allow additional packets to be recovered.
      // Restart for first FEC packet.
      fec_packet_list_it = fec_packet_list_.begin();
    } else if (packets_missing == 0) {
      // Either all protected packets arrived or have been recovered. We can
      // discard this FEC packet.
      DiscardFecPacket(*fec_packet_list_it);
      fec_packet_list_it = fec_packet_list_.erase(fec_packet_list_it);
    } else {
      fec_packet_list_it++;
    }
  }
}

int ForwardErrorCorrection::NumCoveredPacketsMissing(
    const FecPacket* fec_packet) {
  int packets_missing = 0;
  for (const auto* protected_packet : fec_packet->protected_pkt_list) {
    if (protected_packet->pkt == nullptr) {
      ++packets_missing;
      if (packets_missing > 1) {
        break;  // We can't recover more than one packet.
      }
    }
  }
  return packets_missing;
}

void ForwardErrorCorrection::DiscardFecPacket(FecPacket* fec_packet) {
  while (!fec_packet->protected_pkt_list.empty()) {
    delete fec_packet->protected_pkt_list.front();
    fec_packet->protected_pkt_list.pop_front();
  }
  RTC_DCHECK(fec_packet->protected_pkt_list.empty());
  delete fec_packet;
}

void ForwardErrorCorrection::DiscardOldPackets(
    RecoveredPacketList* recovered_packet_list) {
  while (recovered_packet_list->size() > kMaxMediaPackets) {
    ForwardErrorCorrection::RecoveredPacket* packet =
        recovered_packet_list->front();
    delete packet;
    recovered_packet_list->pop_front();
  }
  RTC_DCHECK(recovered_packet_list->size() <= kMaxMediaPackets);
}

uint16_t ForwardErrorCorrection::ParseSequenceNumber(uint8_t* packet) {
  return (packet[2] << 8) + packet[3];
}

int ForwardErrorCorrection::DecodeFec(
    ReceivedPacketList* received_packet_list,
    RecoveredPacketList* recovered_packet_list) {
  // TODO(marpan/ajm): can we check for multiple ULP headers, and return an
  // error?
  if (recovered_packet_list->size() == kMaxMediaPackets) {
    const unsigned int seq_num_diff =
        abs(static_cast<int>(received_packet_list->front()->seq_num) -
            static_cast<int>(recovered_packet_list->back()->seq_num));
    if (seq_num_diff > kMaxMediaPackets) {
      // A big gap in sequence numbers. The old recovered packets
      // are now useless, so it's safe to do a reset.
      ResetState(recovered_packet_list);
    }
  }
  InsertPackets(received_packet_list, recovered_packet_list);
  AttemptRecover(recovered_packet_list);
  return 0;
}

size_t ForwardErrorCorrection::PacketOverhead() {
  return kFecHeaderSize + kUlpHeaderSizeLBitSet;
}
}  // namespace webrtc
