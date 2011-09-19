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

#include "module_common_types.h"
#include "typedefs.h"

namespace webrtc {

struct AudioChannel;
struct SplitAudioChannel;

class AudioBuffer {
 public:
  AudioBuffer(int max_num_channels, int samples_per_channel);
  virtual ~AudioBuffer();

  int num_channels() const;
  int samples_per_channel() const;
  int samples_per_split_channel() const;

  WebRtc_Word16* data(int channel) const;
  WebRtc_Word16* low_pass_split_data(int channel) const;
  WebRtc_Word16* high_pass_split_data(int channel) const;
  WebRtc_Word16* mixed_low_pass_data(int channel) const;
  WebRtc_Word16* low_pass_reference(int channel) const;

  WebRtc_Word32* analysis_filter_state1(int channel) const;
  WebRtc_Word32* analysis_filter_state2(int channel) const;
  WebRtc_Word32* synthesis_filter_state1(int channel) const;
  WebRtc_Word32* synthesis_filter_state2(int channel) const;

  void set_activity(AudioFrame::VADActivity activity);
  AudioFrame::VADActivity activity();

  void DeinterleaveFrom(AudioFrame* audioFrame);
  void InterleaveTo(AudioFrame* audioFrame) const;
  void Mix(int num_mixed_channels);
  void CopyAndMixLowPass(int num_mixed_channels);
  void CopyLowPassToReference();

 private:
  const int max_num_channels_;
  int num_channels_;
  int num_mixed_channels_;
  int num_mixed_low_pass_channels_;
  const int samples_per_channel_;
  int samples_per_split_channel_;
  bool reference_copied_;
  AudioFrame::VADActivity activity_;

  WebRtc_Word16* data_;
  // TODO(andrew): use vectors here.
  AudioChannel* channels_;
  SplitAudioChannel* split_channels_;
  // TODO(andrew): improve this, we don't need the full 32 kHz space here.
  AudioChannel* mixed_low_pass_channels_;
  AudioChannel* low_pass_reference_channels_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_AUDIO_BUFFER_H_
