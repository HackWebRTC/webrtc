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
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_RTP_FORMAT_VP8_H_
#define WEBRTC_MODULES_RTP_RTCP_RTP_FORMAT_VP8_H_

#include "module_common_types.h"
#include "typedefs.h"

namespace webrtc
{

// VP8 packetization modes.
enum VP8PacketizerMode
{
    kStrict = 0, // split partitions if too large; never aggregate partitions
    kAggregate, // split partitions if too large; aggregate partitions/fragments
    kSloppy, // split entire payload without considering partition boundaries
};

// Packetizer for VP8.
class RTPFormatVP8
{
public:
    // Constructor.
    // Initialize with payload from encoder and fragmentation info.
    // If fragmentation info is NULL, mode will be forced to kSloppy.
    RTPFormatVP8(const WebRtc_UWord8* payload_data,
                 const WebRtc_UWord32 payload_size,
                 const RTPFragmentationHeader* fragmentation,
                 const VP8PacketizerMode mode = kStrict);

    // Get the next payload with VP8 payload header.
    // max_payload_len limits the sum length of payload and VP8 payload header.
    // buffer is a pointer to where the output will be written.
    // bytes_to_send is an output variable that will contain number of bytes
    // written to buffer.
    bool NextPacket(const int max_payload_len, WebRtc_UWord8* buffer,
                    int* bytes_to_send);

private:
    // Write the payload header and copy the payload to the buffer.
    // Will copy send_bytes bytes from the current position on the payload data.
    // last_fragment indicates that this packet ends with the last byte of a
    // partition.
    int WriteHeaderAndPayload(const int send_bytes, const bool last_fragment,
                              WebRtc_UWord8* buffer);

    const WebRtc_UWord8* payload_data_;
    const WebRtc_UWord32 payload_size_;
    RTPFragmentationHeader frag_info_;
    int bytes_sent_;
    VP8PacketizerMode mode_;
    bool beginning_; // first partition in this frame
    bool first_fragment_; // first fragment of a partition
    const int vp8_header_bytes_; // length of VP8 payload header
};

}

#endif /* WEBRTC_MODULES_RTP_RTCP_RTP_FORMAT_VP8_H_ */
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
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_RTP_FORMAT_VP8_H_
#define WEBRTC_MODULES_RTP_RTCP_RTP_FORMAT_VP8_H_

#include "module_common_types.h"
#include "typedefs.h"

namespace webrtc
{

// VP8 packetization modes.
enum VP8PacketizerMode
{
    kStrict = 0, // split partitions if too large; never aggregate partitions
    kAggregate, // split partitions if too large; aggregate partitions/fragments
    kSloppy, // split entire payload without considering partition boundaries
};

// Packetizer for VP8.
class RTPFormatVP8
{
public:
    // Constructor.
    // Initialize with payload from encoder and fragmentation info.
    // If fragmentation info is NULL, mode will be forced to kSloppy.
    RTPFormatVP8(const WebRtc_UWord8* payload_data,
                 const WebRtc_UWord32 payload_size,
                 const RTPFragmentationHeader* fragmentation,
                 const VP8PacketizerMode mode = kStrict);

    // Get the next payload with VP8 payload header.
    // max_payload_len limits the sum length of payload and VP8 payload header.
    // buffer is a pointer to where the output will be written.
    // bytes_to_send is an output variable that will contain number of bytes
    // written to buffer.
    bool NextPacket(const int max_payload_len, WebRtc_UWord8* buffer,
                    int* bytes_to_send);

private:
    // Write the payload header and copy the payload to the buffer.
    // Will copy send_bytes bytes from the current position on the payload data.
    // last_fragment indicates that this packet ends with the last byte of a
    // partition.
    int WriteHeaderAndPayload(const int send_bytes, const bool last_fragment,
                              WebRtc_UWord8* buffer);

    const WebRtc_UWord8* payload_data_;
    const WebRtc_UWord32 payload_size_;
    RTPFragmentationHeader frag_info_;
    int bytes_sent_;
    VP8PacketizerMode mode_;
    bool beginning_; // first partition in this frame
    bool first_fragment_; // first fragment of a partition
    const int vp8_header_bytes_; // length of VP8 payload header
};

}

#endif /* WEBRTC_MODULES_RTP_RTCP_RTP_FORMAT_VP8_H_ */
