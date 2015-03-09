/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/buffer.h"

namespace rtc {

Buffer::Buffer() {
  Construct(NULL, 0, 0);
}

Buffer::Buffer(const void* data, size_t length) {
  Construct(data, length, length);
}

Buffer::Buffer(const void* data, size_t length, size_t capacity) {
  Construct(data, length, capacity);
}

Buffer::Buffer(const Buffer& buf) {
  Construct(buf.data(), buf.length(), buf.length());
}

Buffer::~Buffer() = default;

};  // namespace rtc
