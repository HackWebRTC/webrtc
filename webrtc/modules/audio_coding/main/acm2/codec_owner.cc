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
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_coding/codecs/cng/include/audio_encoder_cng.h"
#include "webrtc/modules/audio_coding/codecs/g711/include/audio_encoder_pcm.h"
#include "webrtc/modules/audio_coding/codecs/g722/include/audio_encoder_g722.h"
#include "webrtc/modules/audio_coding/codecs/ilbc/interface/audio_encoder_ilbc.h"
#include "webrtc/modules/audio_coding/codecs/isac/fix/interface/audio_encoder_isacfix.h"
#include "webrtc/modules/audio_coding/codecs/isac/main/interface/audio_encoder_isac.h"
#include "webrtc/modules/audio_coding/codecs/opus/interface/audio_encoder_opus.h"
#include "webrtc/modules/audio_coding/codecs/pcm16b/include/audio_encoder_pcm16b.h"
#include "webrtc/modules/audio_coding/codecs/red/audio_encoder_copy_red.h"

namespace webrtc {
namespace acm2 {

namespace {
bool IsIsac(const CodecInst& codec) {
  return
#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
      !STR_CASE_CMP(codec.plname, "isac") ||
#endif
      false;
}

bool IsOpus(const CodecInst& codec) {
  return
#ifdef WEBRTC_CODEC_OPUS
      !STR_CASE_CMP(codec.plname, "opus") ||
#endif
      false;
}

bool IsPcmU(const CodecInst& codec) {
  return !STR_CASE_CMP(codec.plname, "pcmu");
}

bool IsPcmA(const CodecInst& codec) {
  return !STR_CASE_CMP(codec.plname, "pcma");
}

bool IsPcm16B(const CodecInst& codec) {
  return
#ifdef WEBRTC_CODEC_PCM16
      !STR_CASE_CMP(codec.plname, "l16") ||
#endif
      false;
}

bool IsIlbc(const CodecInst& codec) {
  return
#ifdef WEBRTC_CODEC_ILBC
      !STR_CASE_CMP(codec.plname, "ilbc") ||
#endif
      false;
}

bool IsG722(const CodecInst& codec) {
  return
#ifdef WEBRTC_CODEC_G722
      !STR_CASE_CMP(codec.plname, "g722") ||
#endif
      false;
}
}  // namespace

CodecOwner::CodecOwner() : isac_is_encoder_(false) {
}

CodecOwner::~CodecOwner() = default;

namespace {
AudioEncoderDecoderMutableIsac* CreateIsacCodec(const CodecInst& speech_inst) {
#if defined(WEBRTC_CODEC_ISACFX)
  return new AudioEncoderDecoderMutableIsacFix(speech_inst);
#elif defined(WEBRTC_CODEC_ISAC)
  return new AudioEncoderDecoderMutableIsacFloat(speech_inst);
#else
  FATAL() << "iSAC is not supported.";
  return nullptr;
#endif
}

AudioEncoder* CreateSpeechEncoder(
    const CodecInst& speech_inst,
    rtc::scoped_ptr<AudioEncoderMutable>* speech_encoder,
    rtc::scoped_ptr<AudioEncoderDecoderMutableIsac>* isac_codec,
    bool* isac_is_encoder) {
  if (IsIsac(speech_inst)) {
    if (*isac_codec) {
      (*isac_codec)->UpdateSettings(speech_inst);
    } else {
      isac_codec->reset(CreateIsacCodec(speech_inst));
    }
    *isac_is_encoder = true;
    speech_encoder->reset();
    return isac_codec->get();
  }
  if (IsOpus(speech_inst)) {
    speech_encoder->reset(new AudioEncoderMutableOpus(speech_inst));
  } else if (IsPcmU(speech_inst)) {
    speech_encoder->reset(new AudioEncoderMutablePcmU(speech_inst));
  } else if (IsPcmA(speech_inst)) {
    speech_encoder->reset(new AudioEncoderMutablePcmA(speech_inst));
  } else if (IsPcm16B(speech_inst)) {
    speech_encoder->reset(new AudioEncoderMutablePcm16B(speech_inst));
  } else if (IsIlbc(speech_inst)) {
    speech_encoder->reset(new AudioEncoderMutableIlbc(speech_inst));
  } else if (IsG722(speech_inst)) {
    speech_encoder->reset(new AudioEncoderMutableG722(speech_inst));
  } else {
    FATAL();
  }
  *isac_is_encoder = false;
  return speech_encoder->get();
}

AudioEncoder* CreateRedEncoder(int red_payload_type,
                               AudioEncoder* encoder,
                               rtc::scoped_ptr<AudioEncoder>* red_encoder) {
  if (red_payload_type == -1) {
    red_encoder->reset();
    return encoder;
  }
  AudioEncoderCopyRed::Config config;
  config.payload_type = red_payload_type;
  config.speech_encoder = encoder;
  red_encoder->reset(new AudioEncoderCopyRed(config));
  return red_encoder->get();
}

AudioEncoder* CreateCngEncoder(int cng_payload_type,
                               ACMVADMode vad_mode,
                               AudioEncoder* encoder,
                               rtc::scoped_ptr<AudioEncoder>* cng_encoder) {
  if (cng_payload_type == -1) {
    cng_encoder->reset();
    return encoder;
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
  return cng_encoder->get();
}
}  // namespace

void CodecOwner::SetEncoders(const CodecInst& speech_inst,
                             int cng_payload_type,
                             ACMVADMode vad_mode,
                             int red_payload_type) {
  AudioEncoder* encoder = CreateSpeechEncoder(speech_inst, &speech_encoder_,
                                              &isac_codec_, &isac_is_encoder_);
  encoder = CreateRedEncoder(red_payload_type, encoder, &red_encoder_);
  encoder =
      CreateCngEncoder(cng_payload_type, vad_mode, encoder, &cng_encoder_);
  DCHECK(!speech_encoder_ || !isac_is_encoder_);
  DCHECK(!isac_is_encoder_ || isac_codec_);
}

AudioDecoder* CodecOwner::GetIsacDecoder() {
  if (!isac_codec_) {
    DCHECK(!isac_is_encoder_);
    // None of the parameter values in |speech_inst| matter when the codec is
    // used only as a decoder.
    CodecInst speech_inst;
    speech_inst.plfreq = 16000;
    speech_inst.rate = -1;
    speech_inst.pacsize = 480;
    isac_codec_.reset(CreateIsacCodec(speech_inst));
  }
  return isac_codec_.get();
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
  return SpeechEncoder();
}

AudioEncoderMutable* CodecOwner::SpeechEncoder() {
  const auto& const_this = *this;
  return const_cast<AudioEncoderMutable*>(const_this.SpeechEncoder());
}

const AudioEncoderMutable* CodecOwner::SpeechEncoder() const {
  DCHECK(!speech_encoder_ || !isac_is_encoder_);
  DCHECK(!isac_is_encoder_ || isac_codec_);
  if (speech_encoder_)
    return speech_encoder_.get();
  return isac_is_encoder_ ? isac_codec_.get() : nullptr;
}

}  // namespace acm2
}  // namespace webrtc
