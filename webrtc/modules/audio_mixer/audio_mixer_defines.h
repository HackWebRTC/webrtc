/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_DEFINES_H_
#define WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_DEFINES_H_

#include "webrtc/base/checks.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/typedefs.h"

namespace webrtc {
class NewMixHistory;

// A callback class that all mixer participants must inherit from/implement.
class MixerAudioSource {
 public:
  enum class AudioFrameInfo {
    kNormal,  // The samples in audio_frame are valid and should be used.
    kMuted,   // The samples in audio_frame should not be used, but should be
              // implicitly interpreted as zero. Other fields in audio_frame
              // may be read and should contain meaningful values.
    kError    // audio_frame will not be used.
  };

  struct AudioFrameWithMuted {
    AudioFrame* audio_frame;
    AudioFrameInfo audio_frame_info;
  };

  // The implementation of GetAudioFrameWithMuted should update
  // audio_frame with new audio every time it's called. Implementing
  // classes are allowed to return the same AudioFrame pointer on
  // different calls. The pointer must stay valid until the next
  // mixing call or until this audio source is disconnected from the
  // mixer.
  virtual AudioFrameWithMuted GetAudioFrameWithMuted(int32_t id,
                                                     int sample_rate_hz) = 0;

  // Returns true if the participant was mixed this mix iteration.
  bool IsMixed() const;

  NewMixHistory* mix_history_;

 protected:
  MixerAudioSource();
  virtual ~MixerAudioSource();
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_MIXER_AUDIO_MIXER_DEFINES_H_
