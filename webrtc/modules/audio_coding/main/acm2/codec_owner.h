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
#include "webrtc/modules/audio_coding/codecs/isac/main/interface/audio_encoder_isac.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"

namespace webrtc {

class AudioDecoder;

namespace acm2 {

class CodecOwner {
 public:
  CodecOwner();
  ~CodecOwner();

  void SetEncoders(const CodecInst& speech_inst,
                   int cng_payload_type,
                   ACMVADMode vad_mode,
                   int red_payload_type);

  void SetEncoders(AudioEncoderMutable* external_speech_encoder,
                   int cng_payload_type,
                   ACMVADMode vad_mode,
                   int red_payload_type);

  void ChangeCngAndRed(int cng_payload_type,
                       ACMVADMode vad_mode,
                       int red_payload_type);

  // Returns a pointer to an iSAC decoder owned by the CodecOwner. The decoder
  // will live as long as the CodecOwner exists.
  AudioDecoder* GetIsacDecoder();

  AudioEncoder* Encoder();
  const AudioEncoder* Encoder() const;
  AudioEncoderMutable* SpeechEncoder();
  const AudioEncoderMutable* SpeechEncoder() const;

 private:
  // There are three main cases for the state of the encoder members below:
  // 1. An external encoder is used. |external_speech_encoder_| points to it.
  //    |speech_encoder_| is null, and |isac_is_encoder_| is false.
  // 2. The internal iSAC codec is used as encoder. |isac_codec_| points to it
  //    and |isac_is_encoder_| is true. |external_speech_encoder_| and
  //    |speech_encoder_| are null.
  // 3. Another internal encoder is used. |speech_encoder_| points to it.
  //    |external_speech_encoder_| is null, and |isac_is_encoder_| is false.
  // In addition to case 2, |isac_codec_| is valid when GetIsacDecoder has been
  // called.
  rtc::scoped_ptr<AudioEncoderMutable> speech_encoder_;
  rtc::scoped_ptr<AudioEncoderDecoderMutableIsac> isac_codec_;
  bool isac_is_encoder_;
  AudioEncoderMutable* external_speech_encoder_;

  // |cng_encoder_| and |red_encoder_| are valid iff CNG or RED, respectively,
  // are active.
  rtc::scoped_ptr<AudioEncoder> cng_encoder_;
  rtc::scoped_ptr<AudioEncoder> red_encoder_;

  DISALLOW_COPY_AND_ASSIGN(CodecOwner);
};

}  // namespace acm2
}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_CODEC_OWNER_H_
