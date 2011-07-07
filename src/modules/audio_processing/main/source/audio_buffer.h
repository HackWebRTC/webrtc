/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_AUDIO_BUFFER_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_AUDIO_BUFFER_H_

#include "typedefs.h"


namespace webrtc {

struct AudioChannel;
struct SplitAudioChannel;
class AudioFrame;

class AudioBuffer {
 public:
  AudioBuffer(WebRtc_Word32 max_num_channels, WebRtc_Word32 samples_per_channel);
  virtual ~AudioBuffer();

  WebRtc_Word32 num_channels() const;
  WebRtc_Word32 samples_per_channel() const;
  WebRtc_Word32 samples_per_split_channel() const;

  WebRtc_Word16* data(WebRtc_Word32 channel) const;
  WebRtc_Word16* low_pass_split_data(WebRtc_Word32 channel) const;
  WebRtc_Word16* high_pass_split_data(WebRtc_Word32 channel) const;
  WebRtc_Word16* mixed_low_pass_data(WebRtc_Word32 channel) const;
  WebRtc_Word16* low_pass_reference(WebRtc_Word32 channel) const;

  WebRtc_Word32* analysis_filter_state1(WebRtc_Word32 channel) const;
  WebRtc_Word32* analysis_filter_state2(WebRtc_Word32 channel) const;
  WebRtc_Word32* synthesis_filter_state1(WebRtc_Word32 channel) const;
  WebRtc_Word32* synthesis_filter_state2(WebRtc_Word32 channel) const;

  void DeinterleaveFrom(AudioFrame* audioFrame);
  void InterleaveTo(AudioFrame* audioFrame) const;
  void Mix(WebRtc_Word32 num_mixed_channels);
  void CopyAndMixLowPass(WebRtc_Word32 num_mixed_channels);
  void CopyLowPassToReference();

 private:
  const WebRtc_Word32 max_num_channels_;
  WebRtc_Word32 num_channels_;
  WebRtc_Word32 num_mixed_channels_;
  WebRtc_Word32 num_mixed_low_pass_channels_;
  const WebRtc_Word32 samples_per_channel_;
  WebRtc_Word32 samples_per_split_channel_;
  bool reference_copied_;

  WebRtc_Word16* data_;
  // TODO(ajm): Prefer to make these vectors if permitted...
  AudioChannel* channels_;
  SplitAudioChannel* split_channels_;
  // TODO(ajm): improve this, we don't need the full 32 kHz space here.
  AudioChannel* mixed_low_pass_channels_;
  AudioChannel* low_pass_reference_channels_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_AUDIO_BUFFER_H_
