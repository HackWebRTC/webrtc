/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_UTILITY_VP9_UNCOMPRESSED_HEADER_PARSER_H_
#define WEBRTC_MODULES_VIDEO_CODING_UTILITY_VP9_UNCOMPRESSED_HEADER_PARSER_H_

#include <stddef.h>
#include <stdint.h>

#include "webrtc/base/bitbuffer.h"
#include "webrtc/base/logging.h"

namespace webrtc {

namespace vp9 {

class VP9BitReader : public ::rtc::BitBuffer {
 public:
  VP9BitReader(const uint8_t* buffer, size_t length_)
      : BitBuffer(buffer, length_) {}

  uint32_t GetBit() {
    uint32_t bit = 0;
    if (ReadBits(&bit, 1))
      return bit;

    LOG(LS_WARNING) << "Failed to get bit. Reached EOF.";
    return 0;
  }

  uint32_t GetValue(int bits) {
    uint32_t value = 0;
    if (ReadBits(&value, bits))
      return value;

    LOG(LS_WARNING) << "Failed to get bit. Reached EOF.";
    return 0;
  }

  int32_t GetSignedValue(int bits) {
    const int32_t value = static_cast<int>(GetValue(bits));
    return GetBit() ? -value : value;
  }
};

// Gets the QP, QP range: [0, 255].
// Returns true on success, false otherwise.
bool GetQp(const uint8_t* buf, size_t length, int* qp);

}  // namespace vp9

}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_UTILITY_VP9_UNCOMPRESSED_HEADER_PARSER_H_
