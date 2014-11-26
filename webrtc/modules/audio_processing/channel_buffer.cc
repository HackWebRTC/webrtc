/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/channel_buffer.h"

namespace webrtc {

IFChannelBuffer::IFChannelBuffer(int samples_per_channel, int num_channels)
    : ivalid_(true),
      ibuf_(samples_per_channel, num_channels),
      fvalid_(true),
      fbuf_(samples_per_channel, num_channels) {}

ChannelBuffer<int16_t>* IFChannelBuffer::ibuf() {
  RefreshI();
  fvalid_ = false;
  return &ibuf_;
}

ChannelBuffer<float>* IFChannelBuffer::fbuf() {
  RefreshF();
  ivalid_ = false;
  return &fbuf_;
}

const ChannelBuffer<int16_t>* IFChannelBuffer::ibuf_const() const {
  RefreshI();
  return &ibuf_;
}

const ChannelBuffer<float>* IFChannelBuffer::fbuf_const() const {
  RefreshF();
  return &fbuf_;
}

void IFChannelBuffer::RefreshF() const {
  if (!fvalid_) {
    assert(ivalid_);
    const int16_t* const int_data = ibuf_.data();
    float* const float_data = fbuf_.data();
    const int length = fbuf_.length();
    for (int i = 0; i < length; ++i)
      float_data[i] = int_data[i];
    fvalid_ = true;
  }
}

void IFChannelBuffer::RefreshI() const {
  if (!ivalid_) {
    assert(fvalid_);
    FloatS16ToS16(fbuf_.data(), ibuf_.length(), ibuf_.data());
    ivalid_ = true;
  }
}

}  // namespace webrtc
