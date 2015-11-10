/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/acm2/codec_owner.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_coding/codecs/cng/include/audio_encoder_cng.h"
#ifdef WEBRTC_CODEC_RED
#include "webrtc/modules/audio_coding/codecs/red/audio_encoder_copy_red.h"
#endif

namespace webrtc {
namespace acm2 {

CodecOwner::CodecOwner() : speech_encoder_(nullptr) {
}

CodecOwner::~CodecOwner() = default;

namespace {

AudioEncoder* CreateRedEncoder(int red_payload_type,
                               AudioEncoder* encoder,
                               rtc::scoped_ptr<AudioEncoder>* red_encoder) {
#ifdef WEBRTC_CODEC_RED
  if (red_payload_type != -1) {
    AudioEncoderCopyRed::Config config;
    config.payload_type = red_payload_type;
    config.speech_encoder = encoder;
    red_encoder->reset(new AudioEncoderCopyRed(config));
    return red_encoder->get();
  }
#endif

  red_encoder->reset();
  return encoder;
}

void CreateCngEncoder(int cng_payload_type,
                      ACMVADMode vad_mode,
                      AudioEncoder* encoder,
                      rtc::scoped_ptr<AudioEncoder>* cng_encoder) {
  if (cng_payload_type == -1) {
    cng_encoder->reset();
    return;
  }
  AudioEncoderCng::Config config;
  config.num_channels = encoder->NumChannels();
  config.payload_type = cng_payload_type;
  config.speech_encoder = encoder;
  switch (vad_mode) {
    case VADNormal:
      config.vad_mode = Vad::kVadNormal;
      break;
    case VADLowBitrate:
      config.vad_mode = Vad::kVadLowBitrate;
      break;
    case VADAggr:
      config.vad_mode = Vad::kVadAggressive;
      break;
    case VADVeryAggr:
      config.vad_mode = Vad::kVadVeryAggressive;
      break;
    default:
      FATAL();
  }
  cng_encoder->reset(new AudioEncoderCng(config));
}
}  // namespace

void CodecOwner::SetEncoders(AudioEncoder* external_speech_encoder,
                             int cng_payload_type,
                             ACMVADMode vad_mode,
                             int red_payload_type) {
  speech_encoder_ = external_speech_encoder;
  ChangeCngAndRed(cng_payload_type, vad_mode, red_payload_type);
}

void CodecOwner::ChangeCngAndRed(int cng_payload_type,
                                 ACMVADMode vad_mode,
                                 int red_payload_type) {
  RTC_DCHECK(speech_encoder_);
  if (cng_payload_type != -1 || red_payload_type != -1) {
    // The RED and CNG encoders need to be in sync with the speech encoder, so
    // reset the latter to ensure its buffer is empty.
    speech_encoder_->Reset();
  }
  AudioEncoder* encoder = CreateRedEncoder(
      red_payload_type, speech_encoder_, &red_encoder_);
  CreateCngEncoder(cng_payload_type, vad_mode, encoder, &cng_encoder_);
}

AudioEncoder* CodecOwner::Encoder() {
  const auto& const_this = *this;
  return const_cast<AudioEncoder*>(const_this.Encoder());
}

const AudioEncoder* CodecOwner::Encoder() const {
  if (cng_encoder_)
    return cng_encoder_.get();
  if (red_encoder_)
    return red_encoder_.get();
  return speech_encoder_;
}

}  // namespace acm2
}  // namespace webrtc
