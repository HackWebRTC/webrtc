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
#include <string.h> // memcpy

namespace webrtc {

RtpFormatVp8::RtpFormatVp8(const WebRtc_UWord8* payload_data,
                           WebRtc_UWord32 payload_size,
                           const RTPFragmentationHeader* fragmentation,
                           VP8PacketizerMode mode)
    : payload_data_(payload_data),
      payload_size_(payload_size),
      payload_bytes_sent_(0),
      mode_(mode),
      beginning_(true),
      first_fragment_(true),
      vp8_header_bytes_(1)
{
    if (fragmentation == NULL)
    {
        // Cannot do kStrict or kAggregate without fragmentation info.
        // Change to kSloppy.
        mode_ = kSloppy;
    }
    else
    {
        frag_info_ = *fragmentation;
    }
}

RtpFormatVp8::RtpFormatVp8(const WebRtc_UWord8* payload_data,
                           WebRtc_UWord32 payload_size)
    : payload_data_(payload_data),
      payload_size_(payload_size),
      frag_info_(),
      payload_bytes_sent_(0),
      mode_(kSloppy),
      beginning_(true),
      first_fragment_(true),
      vp8_header_bytes_(1)
{}

int RtpFormatVp8::GetFragIdx()
{
    // Which fragment are we in?
    int frag_ix = 0;
    while ((frag_ix + 1 < frag_info_.fragmentationVectorSize) &&
        (payload_bytes_sent_ >= frag_info_.fragmentationOffset[frag_ix + 1]))
    {
        ++frag_ix;
    }
    return frag_ix;
}

int RtpFormatVp8::NextPacket(int max_payload_len, WebRtc_UWord8* buffer,
                             int* bytes_to_send, bool* last_packet)
{
    // Convenience variables
    const int num_fragments = frag_info_.fragmentationVectorSize;
    int frag_ix = GetFragIdx(); //TODO (hlundin): Store frag_ix as a member?
    int send_bytes = 0; // How much data to send in this packet.
    bool end_of_fragment = false;

    switch (mode_)
    {
        case kAggregate:
        {
            // Check if we are at the beginning of a new partition.
            if (first_fragment_)
            {
                // Check if this fragment fits in one packet.
                if (frag_info_.fragmentationLength[frag_ix] + vp8_header_bytes_
                    <= max_payload_len)
                {
                    // Pack as many whole partitions we can into this packet;
                    // don't fragment.
                    while ((frag_ix < num_fragments) &&
                        (send_bytes + vp8_header_bytes_
                        + frag_info_.fragmentationLength[frag_ix]
                        <= max_payload_len))
                    {
                        send_bytes += frag_info_.fragmentationLength[frag_ix];
                        ++frag_ix;
                    }

                    // This packet ends on a complete fragment.
                    end_of_fragment = true;
                    break; // Jump out of case statement.
                }
            }

            // Either we are not starting this packet with a new partition,
            // or the partition is too large for a packet.
            // Move on to "case kStrict".
            // NOTE: break intentionally omitted!
        }

        case kStrict: // Can also continue to here from kAggregate.
        {
            // Find out how much is left to send in the current partition.
            const int remaining_bytes = frag_info_.fragmentationOffset[frag_ix]
                - payload_bytes_sent_ + frag_info_.fragmentationLength[frag_ix];
            assert(remaining_bytes > 0);
            assert(remaining_bytes <= frag_info_.fragmentationLength[frag_ix]);

            if (remaining_bytes + vp8_header_bytes_ > max_payload_len)
            {
                // send one full packet
                send_bytes = max_payload_len - vp8_header_bytes_;
            }
            else
            {
                // last packet from this partition
                send_bytes = remaining_bytes;
                end_of_fragment = true;
            }
            break;
        }

        case kSloppy:
        {
            // Send a full packet, or what is left of the payload.
            const int remaining_bytes = payload_size_ - payload_bytes_sent_;

            if (remaining_bytes + vp8_header_bytes_ > max_payload_len)
            {
                send_bytes = max_payload_len - vp8_header_bytes_;
                end_of_fragment = false;
            }
            else
            {
                send_bytes = remaining_bytes;
                end_of_fragment = true;
            }
            break;
        }

        default:
            // Should not end up here
            assert(false);
            return -1;
    }

    // Write the payload header and the payload to buffer.
    *bytes_to_send = WriteHeaderAndPayload(send_bytes, end_of_fragment, buffer);
    if (*bytes_to_send < 0)
    {
        return -1;
    }

    *last_packet = payload_bytes_sent_ >= payload_size_;
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
