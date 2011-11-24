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
 * This file contains the declaration of the VP8 packetizer class.
 * A packetizer object is created for each encoded video frame. The
 * constructor is called with the payload data and size,
 * together with the fragmentation information and a packetizer mode
 * of choice. Alternatively, if no fragmentation info is available, the
 * second constructor can be used with only payload data and size; in that
 * case the mode kSloppy is used.
 *
 * After creating the packetizer, the method NextPacket is called
 * repeatedly to get all packets for the frame. The method returns
 * false as long as there are more packets left to fetch.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_RTP_FORMAT_VP8_H_
#define WEBRTC_MODULES_RTP_RTCP_RTP_FORMAT_VP8_H_

#include "module_common_types.h"
#include "typedefs.h"

namespace webrtc
{

enum VP8PacketizerMode
{
    kStrict = 0, // split partitions if too large; never aggregate, balance size
    kAggregate,  // split partitions if too large; aggregate whole partitions
    kSloppy,     // split entire payload without considering partition limits
    kNumModes,
};

// Packetizer for VP8.
class RtpFormatVp8
{
public:
    // Initialize with payload from encoder and fragmentation info.
    // The payload_data must be exactly one encoded VP8 frame.
    RtpFormatVp8(const WebRtc_UWord8* payload_data,
                 WebRtc_UWord32 payload_size,
                 const RTPVideoHeaderVP8& hdr_info,
                 const RTPFragmentationHeader& fragmentation,
                 VP8PacketizerMode mode);

    // Initialize without fragmentation info. Mode kSloppy will be used.
    // The payload_data must be exactly one encoded VP8 frame.
    RtpFormatVp8(const WebRtc_UWord8* payload_data,
                 WebRtc_UWord32 payload_size,
                 const RTPVideoHeaderVP8& hdr_info);

    // Get the next payload with VP8 payload header.
    // max_payload_len limits the sum length of payload and VP8 payload header.
    // buffer is a pointer to where the output will be written.
    // bytes_to_send is an output variable that will contain number of bytes
    // written to buffer. Parameter last_packet is true for the last packet of
    // the frame, false otherwise (i.e., call the function again to get the
    // next packet). Returns the partition index from which the first payload
    // byte in the packet is taken, with the first partition having index 0;
    // returns negative on error.
    int NextPacket(int max_payload_len, WebRtc_UWord8* buffer,
                   int* bytes_to_send, bool* last_packet);

private:
    enum AggregationMode
    {
        kAggrNone = 0,   // no aggregation
        kAggrPartitions, // aggregate intact partitions
        kAggrFragments   // aggregate intact and fragmented partitions
    };

    static const AggregationMode aggr_modes_[kNumModes];
    static const bool balance_modes_[kNumModes];
    static const bool separate_first_modes_[kNumModes];
    static const int kXBit        = 0x80;
    static const int kNBit        = 0x20;
    static const int kSBit        = 0x10;
    static const int kPartIdField = 0x0F;
    static const int kKeyIdxField = 0x1F;
    static const int kIBit        = 0x80;
    static const int kLBit        = 0x40;
    static const int kTBit        = 0x20;
    static const int kKBit        = 0x10;

    // Calculate size of next chunk to send. Returns 0 if none can be sent.
    int CalcNextSize(int max_payload_len, int remaining_bytes,
                     bool split_payload) const;

    // Write the payload header and copy the payload to the buffer.
    // Will copy send_bytes bytes from the current position on the payload data.
    // last_fragment indicates that this packet ends with the last byte of a
    // partition.
    int WriteHeaderAndPayload(int send_bytes, WebRtc_UWord8* buffer,
                              int buffer_length);


    // Write the X field and the appropriate extension fields to buffer.
    // The function returns the extension length (including X field), or -1
    // on error.
    int WriteExtensionFields(WebRtc_UWord8* buffer, int buffer_length) const;

    // Set the I bit in the x_field, and write PictureID to the appropriate
    // position in buffer. The function returns 0 on success, -1 otherwise.
    int WritePictureIDFields(WebRtc_UWord8* x_field, WebRtc_UWord8* buffer,
                             int buffer_length, int* extension_length) const;

    // Set the L bit in the x_field, and write Tl0PicIdx to the appropriate
    // position in buffer. The function returns 0 on success, -1 otherwise.
    int WriteTl0PicIdxFields(WebRtc_UWord8* x_field, WebRtc_UWord8* buffer,
                             int buffer_length, int* extension_length) const;

    // Set the T and K bits in the x_field, and write TID and KeyIdx to the
    // appropriate position in buffer. The function returns 0 on success,
    // -1 otherwise.
    int WriteTIDAndKeyIdxFields(WebRtc_UWord8* x_field, WebRtc_UWord8* buffer,
                                int buffer_length, int* extension_length) const;

    // Write the PictureID from codec_specific_info_ to buffer. One or two
    // bytes are written, depending on magnitude of PictureID. The function
    // returns the number of bytes written.
    int WritePictureID(WebRtc_UWord8* buffer, int buffer_length) const;

    // Calculate and return length (octets) of the variable header fields in
    // the next header (i.e., header length in addition to vp8_header_bytes_).
    int PayloadDescriptorExtraLength() const;

    // Calculate and return length (octets) of PictureID field in the next
    // header. Can be 0, 1, or 2.
    int PictureIdLength() const;

    // Check whether each of the optional fields will be included in the header.
    bool XFieldPresent() const;
    bool TIDFieldPresent() const;
    bool KeyIdxFieldPresent() const;
    bool TL0PicIdxFieldPresent() const;
    bool PictureIdPresent() const { return (PictureIdLength() > 0); }

    const WebRtc_UWord8* payload_data_;
    const int payload_size_;
    RTPFragmentationHeader part_info_;
    int payload_bytes_sent_;
    int part_ix_;
    bool beginning_; // first partition in this frame
    bool first_fragment_; // first fragment of a partition
    const int vp8_fixed_payload_descriptor_bytes_; // length of VP8 payload
                                                   // descriptors's fixed part
    AggregationMode aggr_mode_;
    bool balance_;
    bool separate_first_;
    const RTPVideoHeaderVP8 hdr_info_;
    int first_partition_in_packet_;
};

}

#endif /* WEBRTC_MODULES_RTP_RTCP_RTP_FORMAT_VP8_H_ */
