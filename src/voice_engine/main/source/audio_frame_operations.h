/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_AUDIO_FRAME_OPERATIONS_H_
#define WEBRTC_VOICE_ENGINE_AUDIO_FRAME_OPERATIONS_H_

#include "typedefs.h"

namespace webrtc {

class AudioFrame;

// TODO(andrew): consolidate this with utility.h and audio_frame_manipulator.h.
// Change reference parameters to pointers. Move out of VoE to a common place.
// Consider using a namespace rather than class.
class AudioFrameOperations {
 public:
  static int MonoToStereo(AudioFrame& frame);

  static int StereoToMono(AudioFrame& frame);

  // Swap the left and right channels of |frame|. Fails silently if |frame| is
  // not stereo.
  static void SwapStereoChannels(AudioFrame* frame);

  static void Mute(AudioFrame& frame);

  static int Scale(float left, float right, AudioFrame& frame);

  static int ScaleWithSat(float scale, AudioFrame& frame);
};

}  //  namespace webrtc

#endif  // #ifndef WEBRTC_VOICE_ENGINE_AUDIO_FRAME_OPERATIONS_H_
