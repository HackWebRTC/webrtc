/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_OPUS_H_
#define API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_OPUS_H_

#include "modules/audio_coding/codecs/opus/audio_encoder_opus.h"

namespace webrtc {

// Opus encoder API for use as a template parameter to
// CreateAudioEncoderFactory<...>().
//
// NOTE: At the moment, this struct actually resides in another file. This is a
// temporary backwards compatibility hack; see
// https://bugs.chromium.org/p/webrtc/issues/detail?id=7847
//
// NOTE: This struct is still under development and may change without notice.
/*
struct AudioEncoderOpus {
  static rtc::Optional<AudioEncoderOpusConfig> SdpToConfig(
      const SdpAudioFormat& audio_format);
  static void AppendSupportedEncoders(std::vector<AudioCodecSpec>* specs);
  static AudioCodecInfo QueryAudioEncoder(const AudioEncoderOpusConfig& config);
  static std::unique_ptr<AudioEncoder> MakeAudioEncoder(
      const AudioEncoderOpusConfig&,
      int payload_type);
};
*/

}  // namespace webrtc

#endif  // API_AUDIO_CODECS_OPUS_AUDIO_ENCODER_OPUS_H_
