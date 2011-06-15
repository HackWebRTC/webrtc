/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtp_format_vp8.h"

#include <cassert>  // assert
#include <math.h>   // ceil
#include <string.h> // memcpy

namespace webrtc {

// Define how the VP8PacketizerModes are implemented.
// Modes are: kStrict, kAggregate, kSloppy.
const RtpFormatVp8::AggregationMode RtpFormatVp8::aggr_modes_[kNumModes] =
    { kAggrNone, kAggrPartitions, kAggrFragments };
const bool RtpFormatVp8::bal_modes_[kNumModes] =
    { true, false, false };
const bool RtpFormatVp8::sep_first_modes_[kNumModes] =
    { true, false, false };

RtpFormatVp8::RtpFormatVp8(const WebRtc_UWord8* payload_data,
                           WebRtc_UWord32 payload_size,
                           const RTPFragmentationHeader& fragmentation,
                           VP8PacketizerMode mode)
    : payload_data_(payload_data),
      payload_size_(payload_size),
      payload_bytes_sent_(0),
      part_ix_(0),
      beginning_(true),
      first_fragment_(true),
      vp8_header_bytes_(1),
      aggr_mode_(aggr_modes_[mode]),
      balance_(bal_modes_[mode]),
      separate_first_(sep_first_modes_[mode])
{
    part_info_ = fragmentation;
}

RtpFormatVp8::RtpFormatVp8(const WebRtc_UWord8* payload_data,
                           WebRtc_UWord32 payload_size)
    : payload_data_(payload_data),
      payload_size_(payload_size),
      part_info_(),
      payload_bytes_sent_(0),
      part_ix_(0),
      beginning_(true),
      first_fragment_(true),
      vp8_header_bytes_(1),
      aggr_mode_(aggr_modes_[kSloppy]),
      balance_(bal_modes_[kSloppy]),
      separate_first_(sep_first_modes_[kSloppy])
{
    part_info_.VerifyAndAllocateFragmentationHeader(1);
    part_info_.fragmentationLength[0] = payload_size_;
    part_info_.fragmentationOffset[0] = 0;
}

int RtpFormatVp8::CalcNextSize(int max_payload_len, int remaining_bytes,
                               bool split_payload) const
{
    if (max_payload_len == 0 || remaining_bytes == 0)
    {
        return 0;
    }
    if (!split_payload)
    {
        return max_payload_len >= remaining_bytes ? remaining_bytes : 0;
    }

    if (balance_)
    {
        // Balance payload sizes to produce (almost) equal size
        // fragments.
        // Number of fragments for remaining_bytes:
        int num_frags = ceil(
            static_cast<double>(remaining_bytes) / max_payload_len);
        // Number of bytes in this fragment:
        return static_cast<int>(static_cast<double>(remaining_bytes)
            / num_frags + 0.5);
    }
    else
    {
        return max_payload_len >= remaining_bytes ? remaining_bytes
            : max_payload_len;
    }
}

int RtpFormatVp8::NextPacket(int max_payload_len, WebRtc_UWord8* buffer,
                             int* bytes_to_send, bool* last_packet)
{
    const int num_partitions = part_info_.fragmentationVectorSize;
    int send_bytes = 0; // How much data to send in this packet.
    bool split_payload = true; // Splitting of partitions is initially allowed.
    int remaining_in_partition = part_info_.fragmentationOffset[part_ix_]
        - payload_bytes_sent_ + part_info_.fragmentationLength[part_ix_];
    int rem_payload_len = max_payload_len - vp8_header_bytes_;

    while (int next_size = CalcNextSize(rem_payload_len, remaining_in_partition,
        split_payload))
    {
        send_bytes += next_size;
        rem_payload_len -= next_size;
        remaining_in_partition -= next_size;

        if (remaining_in_partition == 0 && !(beginning_ && separate_first_))
        {
            // Advance to next partition?
            // Check that there are more partitions; verify that we are either
            // allowed to aggregate fragments, or that we are allowed to
            // aggregate intact partitions and that we started this packet
            // with an intact partition (indicated by first_fragment_ == true).
            if (part_ix_ + 1 < num_partitions &&
                ((aggr_mode_ == kAggrFragments) ||
                 (aggr_mode_ == kAggrPartitions && first_fragment_)))
            {
                remaining_in_partition
                    = part_info_.fragmentationLength[++part_ix_];
                // Disallow splitting unless kAggrFragments. In kAggrPartitions,
                // we can only aggregate intact partitions.
                split_payload = (aggr_mode_ == kAggrFragments);
            }
        }
        else if (balance_ && remaining_in_partition > 0)
        {
            break;
        }
    }
    if (remaining_in_partition == 0)
    {
        ++part_ix_; // Advance to next partition.
    }

    const bool end_of_fragment = (remaining_in_partition == 0);
    // Write the payload header and the payload to buffer.
    *bytes_to_send = WriteHeaderAndPayload(send_bytes, end_of_fragment, buffer);
    if (*bytes_to_send < 0)
    {
        return -1;
    }

    *last_packet = (payload_bytes_sent_ >= payload_size_);
    assert(!*last_packet || (payload_bytes_sent_ == payload_size_));
    return 0;
}

int RtpFormatVp8::WriteHeaderAndPayload(int send_bytes,
                                        bool end_of_fragment,
                                        WebRtc_UWord8* buffer)
{
    // Write the VP8 payload header.
    //  0 1 2 3 4 5 6 7
    // +-+-+-+-+-+-+-+-+
    // | RSV |I|N|FI |B|
    // +-+-+-+-+-+-+-+-+

    if (send_bytes < 0)
    {
        return -1;
    }
    if (payload_bytes_sent_ + send_bytes > payload_size_)
    {
        return -1;
    }

    // PictureID always present in first packet
    const int picture_id_present = beginning_;
    // TODO(hlundin): must pipe this info from VP8 encoder
    const int kNonrefFrame = 0;

    buffer[0] = 0;
    if (picture_id_present) buffer[0] |= (0x01 << 4); // I
    if (kNonrefFrame)       buffer[0] |= (0x01 << 3); // N
    if (!first_fragment_)   buffer[0] |= (0x01 << 2); // FI
    if (!end_of_fragment)   buffer[0] |= (0x01 << 1); // FI
    if (beginning_)         buffer[0] |= 0x01; // B

    memcpy(&buffer[vp8_header_bytes_], &payload_data_[payload_bytes_sent_],
           send_bytes);

    beginning_ = false; // next packet cannot be first packet in frame
    // next packet starts new fragment if this ended one
    first_fragment_ = end_of_fragment;
    payload_bytes_sent_ += send_bytes;

    // Return total length of written data.
    return send_bytes + vp8_header_bytes_;
}
} // namespace webrtc
