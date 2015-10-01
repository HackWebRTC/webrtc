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
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"

#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
#include "webrtc/modules/audio_coding/codecs/isac/locked_bandwidth_info.h"
#else
// Dummy implementation, for when we don't have iSAC.
namespace webrtc {
class LockedIsacBandwidthInfo {};
}
#endif

namespace webrtc {
namespace acm2 {

class CodecOwner {
 public:
  CodecOwner();
  ~CodecOwner();

  // Start using the specified encoder. Returns false on error.
  // TODO(kwiberg): Don't handle errors here (bug 5033)
  bool SetEncoders(const CodecInst& speech_inst,
                   int cng_payload_type,
                   ACMVADMode vad_mode,
                   int red_payload_type) WARN_UNUSED_RESULT;

  void SetEncoders(AudioEncoder* external_speech_encoder,
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

 private:
  AudioEncoder* SpeechEncoder();
  const AudioEncoder* SpeechEncoder() const;

  // At most one of these is non-null:
  rtc::scoped_ptr<AudioEncoder> speech_encoder_;
  AudioEncoder* external_speech_encoder_;

  // If we've created an iSAC decoder because someone called GetIsacDecoder,
  // store it here.
  rtc::scoped_ptr<AudioDecoder> isac_decoder_;

  // iSAC bandwidth estimation info, for use with iSAC encoders and decoders.
  LockedIsacBandwidthInfo isac_bandwidth_info_;

  // |cng_encoder_| and |red_encoder_| are valid iff CNG or RED, respectively,
  // are active.
  rtc::scoped_ptr<AudioEncoder> cng_encoder_;
  rtc::scoped_ptr<AudioEncoder> red_encoder_;

  RTC_DISALLOW_COPY_AND_ASSIGN(CodecOwner);
};

}  // namespace acm2
}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_CODEC_OWNER_H_
