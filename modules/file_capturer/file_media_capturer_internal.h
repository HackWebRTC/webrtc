/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source tree.
 */

#ifndef MODULES_FILE_CAPTURER_FILE_MEDIA_CAPTURER_INTERNAL_H_
#define MODULES_FILE_CAPTURER_FILE_MEDIA_CAPTURER_INTERNAL_H_

#include <cstddef>

namespace webrtc::file_capturer_internal {

inline constexpr size_t kMaximumPacketBytes = 64 * 1024 * 1024;

inline bool ExceedsPacketBudget(size_t current_bytes, size_t packet_bytes) {
  return current_bytes > kMaximumPacketBytes ||
         packet_bytes > kMaximumPacketBytes - current_bytes;
}

}  // namespace webrtc::file_capturer_internal

#endif  // MODULES_FILE_CAPTURER_FILE_MEDIA_CAPTURER_INTERNAL_H_
