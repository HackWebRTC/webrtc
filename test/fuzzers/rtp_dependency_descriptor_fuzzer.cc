/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "api/array_view.h"
#include "common_video/generic_frame_descriptor/generic_frame_info.h"
#include "modules/rtp_rtcp/source/rtp_dependency_descriptor_extension.h"
#include "rtc_base/checks.h"
#include "test/fuzzers/fuzz_data_helper.h"

namespace webrtc {
namespace {

bool AreSame(const DependencyDescriptor& lhs, const DependencyDescriptor& rhs) {
  return lhs.first_packet_in_frame == rhs.first_packet_in_frame &&
         lhs.last_packet_in_frame == rhs.last_packet_in_frame &&
         (lhs.attached_structure != nullptr) ==
             (rhs.attached_structure != nullptr) &&
         lhs.frame_number == rhs.frame_number &&
         lhs.resolution == rhs.resolution &&
         lhs.frame_dependencies == rhs.frame_dependencies;
}

}  // namespace

void FuzzOneInput(const uint8_t* data, size_t size) {
  FrameDependencyStructure structure1;
  // nullptr during 1st while loop, after that should point to structure1.
  const FrameDependencyStructure* structure1_ptr = nullptr;
  std::unique_ptr<const FrameDependencyStructure> structure2;

  test::FuzzDataHelper fuzz_data(rtc::MakeArrayView(data, size));
  while (fuzz_data.CanReadBytes(1)) {
    // Treat next byte as size of the next extension. That aligns how
    // two-byte rtp header extension sizes are written.
    size_t next_size = fuzz_data.Read<uint8_t>();
    auto raw =
        fuzz_data.ReadByteArray(std::min(next_size, fuzz_data.BytesLeft()));

    // Read the random input.
    DependencyDescriptor descriptor1;
    if (!RtpDependencyDescriptorExtension::Parse(raw, structure1_ptr,
                                                 &descriptor1)) {
      // Ignore invalid buffer and move on.
      continue;
    }
    if (descriptor1.attached_structure) {
      structure1 = *descriptor1.attached_structure;
      structure1_ptr = &structure1;
    }
    RTC_CHECK(structure1_ptr);

    // Write parsed descriptor back into raw buffer.
    size_t value_size =
        RtpDependencyDescriptorExtension::ValueSize(structure1, descriptor1);
    // Check |writer| use minimal number of bytes to pack the descriptor by
    // checking it doesn't use more than reader consumed.
    RTC_CHECK_LE(value_size, raw.size());
    uint8_t some_memory[256];
    // That should be true because value_size <= next_size < 256
    RTC_CHECK_LT(value_size, 256);
    rtc::ArrayView<uint8_t> write_buffer(some_memory, value_size);
    RTC_CHECK(RtpDependencyDescriptorExtension::Write(write_buffer, structure1,
                                                      descriptor1));

    // Parse what Write assembled.
    // Unlike random input that should always succeed.
    DependencyDescriptor descriptor2;
    RTC_CHECK(RtpDependencyDescriptorExtension::Parse(
        write_buffer, structure2.get(), &descriptor2));
    RTC_CHECK(AreSame(descriptor1, descriptor2));

    if (descriptor2.attached_structure) {
      structure2 = std::move(descriptor2.attached_structure);
    }
  }
}

}  // namespace webrtc
