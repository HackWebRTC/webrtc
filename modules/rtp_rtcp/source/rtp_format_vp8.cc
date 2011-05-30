/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file contains the implementation of the VP8 packetizer class.
 */

#include "rtp_format_vp8.h"

#include <cassert>  // assert
#include <string.h> // memcpy

namespace webrtc {

RTPFormatVP8::RTPFormatVP8(const WebRtc_UWord8* payload_data,
                           const WebRtc_UWord32 payload_size,
                           const RTPFragmentationHeader* fragmentation,
                           const VP8PacketizerMode mode)
:
    payload_data_(payload_data),
    payload_size_(payload_size),
    bytes_sent_(0),
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

bool RTPFormatVP8::NextPacket(const int max_payload_len, WebRtc_UWord8* buffer,
                              int* bytes_to_send)
{
    // Convenience variables
    const int num_fragments = frag_info_.fragmentationVectorSize;

    // Which fragment are we in?
    int frag_ix = 0;
    while ((frag_ix + 1 < num_fragments) &&
        (bytes_sent_ >= frag_info_.fragmentationOffset[frag_ix + 1]))
    {
        ++frag_ix;
    }

    // How much data to send in this packet?
    int send_bytes = 0;
    bool last_fragment = false;

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
                    while (send_bytes + vp8_header_bytes_
                        + frag_info_.fragmentationLength[frag_ix]
                        <= max_payload_len)
                    {
                        send_bytes += frag_info_.fragmentationLength[frag_ix];
                        ++frag_ix;
                    }

                    // This packet ends on a complete fragment.
                    last_fragment = true;
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
                - bytes_sent_ + frag_info_.fragmentationLength[frag_ix];
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
                last_fragment = true;
            }
            break;
        }

        case kSloppy:
        {
            // Send a full packet, or what is left of the payload.
            const int remaining_bytes = payload_size_ - bytes_sent_;

            if (remaining_bytes + vp8_header_bytes_ > max_payload_len)
            {
                send_bytes = max_payload_len - vp8_header_bytes_;
                last_fragment = false;
            }
            else
            {
                send_bytes = remaining_bytes;
                last_fragment = true;
            }
            break;
        }

        default:
            // Should not end up here
            assert(false);
    }

    // Write the payload header and the payload to buffer.
    *bytes_to_send = WriteHeaderAndPayload(send_bytes, last_fragment, buffer);

    // Anything left to send?
    if (bytes_sent_ < payload_size_)
        return false;
    else
        return true;
}

int RTPFormatVP8::WriteHeaderAndPayload(const int send_bytes,
                                        const bool last_fragment,
                                        WebRtc_UWord8* buffer)
{
    // Write the VP8 payload header.
    //  0 1 2 3 4 5 6 7
    // +-+-+-+-+-+-+-+-+
    // | RSV |I|N|FI |B|
    // +-+-+-+-+-+-+-+-+

    // PictureID always present in first packet
    const int pictureid_present = beginning_;
    // TODO(hlundin): must pipe this info from VP8 encoder
    const int nonref_frame = 0;

    buffer[0] = 0 | (pictureid_present << 4) // I
                | (nonref_frame << 3) // N
                | (!first_fragment_ << 2) | (!last_fragment << 1) // FI
                | (beginning_);

    // Copy the payload.
    assert(bytes_sent_ + send_bytes <= payload_size_);
    memcpy(&buffer[vp8_header_bytes_], &payload_data_[bytes_sent_], send_bytes);

    // Update state variables.
    beginning_ = false; // next packet cannot be first packet in frame
    // next packet starts new fragment if this ended one
    first_fragment_ = last_fragment;
    bytes_sent_ += send_bytes;

    // Return total length of written data.
    return send_bytes + vp8_header_bytes_;
}
} // namespace webrtc
