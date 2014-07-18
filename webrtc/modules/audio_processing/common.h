/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_COMMON_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_COMMON_H_

#include <assert.h>
#include <string.h>

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"

namespace webrtc {

static inline int ChannelsFromLayout(AudioProcessing::ChannelLayout layout) {
  switch (layout) {
    case AudioProcessing::kMono:
    case AudioProcessing::kMonoAndKeyboard:
      return 1;
    case AudioProcessing::kStereo:
    case AudioProcessing::kStereoAndKeyboard:
      return 2;
  }
  assert(false);
  return -1;
}

// Helper to encapsulate a contiguous data buffer with access to a pointer
// array of the deinterleaved channels.
template <typename T>
class ChannelBuffer {
 public:
  ChannelBuffer(int samples_per_channel, int num_channels)
      : data_(new T[samples_per_channel * num_channels]),
        channels_(new T*[num_channels]),
        samples_per_channel_(samples_per_channel),
        num_channels_(num_channels) {
    memset(data_.get(), 0, sizeof(T) * samples_per_channel * num_channels);
    for (int i = 0; i < num_channels; ++i)
      channels_[i] = &data_[i * samples_per_channel];
  }
  ~ChannelBuffer() {}

  void CopyFrom(const void* channel_ptr, int i) {
    assert(i < num_channels_);
    memcpy(channels_[i], channel_ptr, samples_per_channel_ * sizeof(T));
  }

  T* data() { return data_.get(); }
  const T* channel(int i) const {
    assert(i >= 0 && i < num_channels_);
    return channels_[i];
  }
  T* channel(int i) {
    const ChannelBuffer<T>* t = this;
    return const_cast<T*>(t->channel(i));
  }
  T** channels() { return channels_.get(); }

  int samples_per_channel() { return samples_per_channel_; }
  int num_channels() { return num_channels_; }
  int length() { return samples_per_channel_ * num_channels_; }

 private:
  scoped_ptr<T[]> data_;
  scoped_ptr<T*[]> channels_;
  int samples_per_channel_;
  int num_channels_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_COMMON_H_
