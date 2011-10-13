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

// Define how the VP8PacketizerModes are implemented.
// Modes are: kStrict, kAggregate, kSloppy.
const RtpFormatVp8::AggregationMode RtpFormatVp8::aggr_modes_[kNumModes] =
    { kAggrNone, kAggrPartitions, kAggrFragments };
const bool RtpFormatVp8::balance_modes_[kNumModes] =
    { true, true, false };
const bool RtpFormatVp8::separate_first_modes_[kNumModes] =
    { true, false, false };

RtpFormatVp8::RtpFormatVp8(const WebRtc_UWord8* payload_data,
                           WebRtc_UWord32 payload_size,
                           const RTPVideoHeaderVP8& hdr_info,
                           const RTPFragmentationHeader& fragmentation,
                           VP8PacketizerMode mode)
    : payload_data_(payload_data),
      payload_size_(static_cast<int>(payload_size)),
      payload_bytes_sent_(0),
      part_ix_(0),
      beginning_(true),
      first_fragment_(true),
      vp8_fixed_payload_descriptor_bytes_(1),
      aggr_mode_(aggr_modes_[mode]),
      balance_(balance_modes_[mode]),
      separate_first_(separate_first_modes_[mode]),
      hdr_info_(hdr_info),
      first_partition_in_packet_(0)
{
    part_info_ = fragmentation;
}

RtpFormatVp8::RtpFormatVp8(const WebRtc_UWord8* payload_data,
                           WebRtc_UWord32 payload_size,
                           const RTPVideoHeaderVP8& hdr_info)
    : payload_data_(payload_data),
      payload_size_(static_cast<int>(payload_size)),
      part_info_(),
      payload_bytes_sent_(0),
      part_ix_(0),
      beginning_(true),
      first_fragment_(true),
      vp8_fixed_payload_descriptor_bytes_(1),
      aggr_mode_(aggr_modes_[kSloppy]),
      balance_(balance_modes_[kSloppy]),
      separate_first_(separate_first_modes_[kSloppy]),
      hdr_info_(hdr_info),
      first_partition_in_packet_(0)
{
    part_info_.VerifyAndAllocateFragmentationHeader(1);
    part_info_.fragmentationLength[0] = payload_size;
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
        int num_frags = remaining_bytes / max_payload_len + 1;
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
    if (max_payload_len < vp8_fixed_payload_descriptor_bytes_
            + PayloadDescriptorExtraLength() + 1)
    {
        // The provided payload length is not long enough for the payload
        // descriptor and one payload byte. Return an error.
        return -1;
    }
    const int num_partitions = part_info_.fragmentationVectorSize;
    int send_bytes = 0; // How much data to send in this packet.
    bool split_payload = true; // Splitting of partitions is initially allowed.
    int remaining_in_partition = part_info_.fragmentationOffset[part_ix_] -
        payload_bytes_sent_ + part_info_.fragmentationLength[part_ix_] +
        PayloadDescriptorExtraLength();
    int rem_payload_len = max_payload_len - vp8_fixed_payload_descriptor_bytes_;
    first_partition_in_packet_ = part_ix_;
    if (first_partition_in_packet_ > 8) return -1;

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

    send_bytes -= PayloadDescriptorExtraLength(); // Remove extra length again.
    assert(send_bytes > 0);
    // Write the payload header and the payload to buffer.
    *bytes_to_send = WriteHeaderAndPayload(send_bytes, buffer, max_payload_len);
    if (*bytes_to_send < 0)
    {
        return -1;
    }

    beginning_ = false; // Next packet cannot be first packet in frame.
    // Next packet starts new fragment if this ended one.
    first_fragment_ = (remaining_in_partition == 0);
    *last_packet = (payload_bytes_sent_ >= payload_size_);
    assert(!*last_packet || (payload_bytes_sent_ == payload_size_));
    return first_partition_in_packet_;
}

int RtpFormatVp8::WriteHeaderAndPayload(int payload_bytes,
                                        WebRtc_UWord8* buffer,
                                        int buffer_length)
{
    // Write the VP8 payload descriptor.
    //     0
    //     0 1 2 3 4 5 6 7 8
    //    +-+-+-+-+-+-+-+-+-+
    //    |X| |N|S| PART_ID |
    //    +-+-+-+-+-+-+-+-+-+
    // X: |I|L|T|           | (mandatory if any of the below are used)
    //    +-+-+-+-+-+-+-+-+-+
    // I: |PictureID (8/16b)| (optional)
    //    +-+-+-+-+-+-+-+-+-+
    // L: |   TL0PIC_IDX    | (optional)
    //    +-+-+-+-+-+-+-+-+-+
    // T: | TID |           | (optional)
    //    +-+-+-+-+-+-+-+-+-+

    assert(payload_bytes > 0);
    assert(payload_bytes_sent_ + payload_bytes <= payload_size_);
    assert(vp8_fixed_payload_descriptor_bytes_ + PayloadDescriptorExtraLength()
            + payload_bytes <= buffer_length);

    buffer[0] = 0;
    if (XFieldPresent())        buffer[0] |= kXBit;
    if (hdr_info_.nonReference) buffer[0] |= kNBit;
    if (first_fragment_)        buffer[0] |= kSBit;
    buffer[0] |= (first_partition_in_packet_ & kPartIdField);

    const int extension_length = WriteExtensionFields(buffer, buffer_length);

    memcpy(&buffer[vp8_fixed_payload_descriptor_bytes_ + extension_length],
        &payload_data_[payload_bytes_sent_], payload_bytes);

    payload_bytes_sent_ += payload_bytes;

    // Return total length of written data.
    return payload_bytes + vp8_fixed_payload_descriptor_bytes_
            + extension_length;
}

int RtpFormatVp8::WriteExtensionFields(WebRtc_UWord8* buffer, int buffer_length)
const
{
    int extension_length = 0;
    if (XFieldPresent())
    {
        WebRtc_UWord8* x_field = buffer + vp8_fixed_payload_descriptor_bytes_;
        *x_field = 0;
        extension_length = 1; // One octet for the X field.
        if (PictureIdPresent())
        {
            if (WritePictureIDFields(x_field, buffer, buffer_length,
                    &extension_length) < 0)
            {
                return -1;
            }
        }
        if (TL0PicIdxFieldPresent())
        {
            if (WriteTl0PicIdxFields(x_field, buffer, buffer_length,
                    &extension_length) < 0)
            {
                return -1;
            }
        }
        if (TIDFieldPresent())
        {
            if (WriteTIDFields(x_field, buffer, buffer_length,
                    &extension_length) < 0)
            {
                return -1;
            }
        }
        assert(extension_length == PayloadDescriptorExtraLength());
    }
    return extension_length;
}


int RtpFormatVp8::WritePictureIDFields(WebRtc_UWord8* x_field,
                                       WebRtc_UWord8* buffer,
                                       int buffer_length,
                                       int* extension_length) const
{
    *x_field |= kIBit;
    const int pic_id_length = WritePictureID(
            buffer + vp8_fixed_payload_descriptor_bytes_ + *extension_length,
            buffer_length - vp8_fixed_payload_descriptor_bytes_
                    - *extension_length);
    if (pic_id_length < 0) return -1;
    *extension_length += pic_id_length;
    return 0;
}

int RtpFormatVp8::WritePictureID(WebRtc_UWord8* buffer, int buffer_length) const
{
    const WebRtc_UWord16 pic_id =
        static_cast<WebRtc_UWord16> (hdr_info_.pictureId);
    int picture_id_len = PictureIdLength();
    if (picture_id_len > buffer_length) return -1;
    if (picture_id_len == 2)
    {
        buffer[0] = 0x80 | ((pic_id >> 8) & 0x7F);
        buffer[1] = pic_id & 0xFF;
    }
    else if (picture_id_len == 1)
    {
        buffer[0] = pic_id & 0x7F;
    }
    return picture_id_len;
}

int RtpFormatVp8::WriteTl0PicIdxFields(WebRtc_UWord8* x_field,
                                       WebRtc_UWord8* buffer,
                                       int buffer_length,
                                       int* extension_length) const
{
    if (buffer_length < vp8_fixed_payload_descriptor_bytes_ + *extension_length
            + 1)
    {
        return -1;
    }
    *x_field |= kLBit;
    buffer[vp8_fixed_payload_descriptor_bytes_
           + *extension_length] = hdr_info_.tl0PicIdx;
    ++*extension_length;
    return 0;
}

int RtpFormatVp8::WriteTIDFields(WebRtc_UWord8* x_field,
                                 WebRtc_UWord8* buffer,
                                 int buffer_length,
                                 int* extension_length) const
{
    if (buffer_length < vp8_fixed_payload_descriptor_bytes_ + *extension_length
            + 1)
    {
        return -1;
    }
    *x_field |= kTBit;
    buffer[vp8_fixed_payload_descriptor_bytes_ + *extension_length]
        = hdr_info_.temporalIdx << 5;
    ++*extension_length;
    return 0;
}

int RtpFormatVp8::PayloadDescriptorExtraLength() const
{
    int length_bytes = 0;
    if (beginning_)
    {
        length_bytes = PictureIdLength();
    }
    if (TL0PicIdxFieldPresent()) ++length_bytes;
    if (TIDFieldPresent())       ++length_bytes;
    if (length_bytes > 0)        ++length_bytes; // Include the extension field.
    return length_bytes;
}

int RtpFormatVp8::PictureIdLength() const
{
    if (!beginning_ || hdr_info_.pictureId == kNoPictureId)
    {
        return 0;
    }
    if (hdr_info_.pictureId <= 0x7F)
    {
        return 1;
    }
    return 2;
}

bool RtpFormatVp8::XFieldPresent() const
{
    return (TIDFieldPresent() || TL0PicIdxFieldPresent() || PictureIdPresent());
}

bool RtpFormatVp8::TIDFieldPresent() const
{
    return (hdr_info_.temporalIdx != kNoTemporalIdx);
}

bool RtpFormatVp8::TL0PicIdxFieldPresent() const
{
    return (hdr_info_.tl0PicIdx != kNoTl0PicIdx);
}
} // namespace webrtc
