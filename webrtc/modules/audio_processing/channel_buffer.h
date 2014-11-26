/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_CHANNEL_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_CHANNEL_BUFFER_H_

#include "webrtc/common_audio/include/audio_util.h"
#include "webrtc/modules/audio_processing/common.h"

namespace webrtc {

// One int16_t and one float ChannelBuffer that are kept in sync. The sync is
// broken when someone requests write access to either ChannelBuffer, and
// reestablished when someone requests the outdated ChannelBuffer. It is
// therefore safe to use the return value of ibuf_const() and fbuf_const()
// until the next call to ibuf() or fbuf(), and the return value of ibuf() and
// fbuf() until the next call to any of the other functions.
class IFChannelBuffer {
 public:
  IFChannelBuffer(int samples_per_channel, int num_channels);

  ChannelBuffer<int16_t>* ibuf();
  ChannelBuffer<float>* fbuf();
  const ChannelBuffer<int16_t>* ibuf_const() const;
  const ChannelBuffer<float>* fbuf_const() const;

  int num_channels() const { return ibuf_.num_channels(); }
  int samples_per_channel() const { return ibuf_.samples_per_channel(); }

 private:
  void RefreshF() const;
  void RefreshI() const;

  mutable bool ivalid_;
  mutable ChannelBuffer<int16_t> ibuf_;
  mutable bool fvalid_;
  mutable ChannelBuffer<float> fbuf_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_CHANNEL_BUFFER_H_
