/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_CODEC_OWNER_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_CODEC_OWNER_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"
#include "webrtc/modules/audio_coding/codecs/audio_decoder.h"
#include "webrtc/modules/audio_coding/main/include/audio_coding_module_typedefs.h"

namespace webrtc {
namespace acm2 {

class CodecOwner {
 public:
  CodecOwner();
  ~CodecOwner();

  void SetEncoders(AudioEncoder* external_speech_encoder,
                   int cng_payload_type,
                   ACMVADMode vad_mode,
                   int red_payload_type);

  void ChangeCngAndRed(int cng_payload_type,
                       ACMVADMode vad_mode,
                       int red_payload_type);

  AudioEncoder* Encoder();
  const AudioEncoder* Encoder() const;

 private:
  AudioEncoder* speech_encoder_;

  // |cng_encoder_| and |red_encoder_| are valid iff CNG or RED, respectively,
  // are active.
  rtc::scoped_ptr<AudioEncoder> cng_encoder_;
  rtc::scoped_ptr<AudioEncoder> red_encoder_;

  RTC_DISALLOW_COPY_AND_ASSIGN(CodecOwner);
};

}  // namespace acm2
}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_CODEC_OWNER_H_
