/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/video_coding/frame_object.h"
#include "modules/video_coding/packet_buffer.h"
#include "system_wrappers/include/clock.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {

void IgnoreResult(video_coding::PacketBuffer::InsertResult result) {}

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size > 200000) {
    return;
  }
  video_coding::PacketBuffer::Packet packet;
  SimulatedClock clock(0);
  video_coding::PacketBuffer packet_buffer(&clock, 8, 1024);
  test::FuzzDataHelper helper(rtc::ArrayView<const uint8_t>(data, size));

  while (helper.BytesLeft()) {
    // Complex types (e.g. non-POD-like types) can't be bit-wise fuzzed with
    // random data or it will put them in an invalid state. We therefore backup
    // their byte-patterns before the fuzzing and restore them after.
    uint8_t video_header_backup[sizeof(packet.video_header)];
    memcpy(&video_header_backup, &packet.video_header,
           sizeof(packet.video_header));
    uint8_t generic_descriptor_backup[sizeof(packet.generic_descriptor)];
    memcpy(&generic_descriptor_backup, &packet.generic_descriptor,
           sizeof(packet.generic_descriptor));
    uint8_t packet_info_backup[sizeof(packet.packet_info)];
    memcpy(&packet_info_backup, &packet.packet_info,
           sizeof(packet.packet_info));

    helper.CopyTo(&packet);

    memcpy(&packet.video_header, &video_header_backup,
           sizeof(packet.video_header));
    memcpy(&packet.generic_descriptor, &generic_descriptor_backup,
           sizeof(packet.generic_descriptor));
    memcpy(&packet.packet_info, &packet_info_backup,
           sizeof(packet.packet_info));

    // The packet buffer owns the payload of the packet.
    uint8_t payload_size;
    helper.CopyTo(&payload_size);
    packet.size_bytes = payload_size;
    packet.data = new uint8_t[payload_size];

    IgnoreResult(packet_buffer.InsertPacket(&packet));
  }
}

}  // namespace webrtc
