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
    kStrict = 0, // split partitions if too large; never aggregate partitions
    kAggregate, // split partitions if too large; aggregate whole partitions
    kSloppy, // split entire payload without considering partition boundaries
};

// Packetizer for VP8.
class RtpFormatVp8
{
public:
    // Initialize with payload from encoder and fragmentation info.
    // The payload_data must be exactly one encoded VP8 frame.
    RtpFormatVp8(const WebRtc_UWord8* payload_data,
                 WebRtc_UWord32 payload_size,
                 const RTPFragmentationHeader* fragmentation,
                 VP8PacketizerMode mode);

    // Initialize without fragmentation info. Mode kSloppy will be used.
    // The payload_data must be exactly one encoded VP8 frame.
    RtpFormatVp8(const WebRtc_UWord8* payload_data,
                 WebRtc_UWord32 payload_size);

    // Get the next payload with VP8 payload header.
    // max_payload_len limits the sum length of payload and VP8 payload header.
    // buffer is a pointer to where the output will be written.
    // bytes_to_send is an output variable that will contain number of bytes
    // written to buffer.
    // Returns true for the last packet of the frame, false otherwise (i.e.,
    // call the function again to get the next packet).
    int NextPacket(int max_payload_len, WebRtc_UWord8* buffer,
                   int* bytes_to_send, bool* last_packet);

private:
    // Determine from which fragment the next byte to send will be taken.
    int GetFragIdx();

    // Write the payload header and copy the payload to the buffer.
    // Will copy send_bytes bytes from the current position on the payload data.
    // last_fragment indicates that this packet ends with the last byte of a
    // partition.
    int WriteHeaderAndPayload(int send_bytes, bool end_of_fragment,
                              WebRtc_UWord8* buffer);

    const WebRtc_UWord8* payload_data_;
    const WebRtc_UWord32 payload_size_;
    RTPFragmentationHeader frag_info_;
    int payload_bytes_sent_;
    VP8PacketizerMode mode_;
    bool beginning_; // first partition in this frame
    bool first_fragment_; // first fragment of a partition
    const int vp8_header_bytes_; // length of VP8 payload header
};

}

#endif /* WEBRTC_MODULES_RTP_RTCP_RTP_FORMAT_VP8_H_ */
