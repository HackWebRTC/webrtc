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
#include "webrtc/modules/audio_coding/codecs/g711/include/audio_encoder_pcm.h"
#ifdef WEBRTC_CODEC_G722
#include "webrtc/modules/audio_coding/codecs/g722/include/audio_encoder_g722.h"
#endif
#ifdef WEBRTC_CODEC_ILBC
#include "webrtc/modules/audio_coding/codecs/ilbc/interface/audio_encoder_ilbc.h"
#endif
#ifdef WEBRTC_CODEC_ISACFX
#include "webrtc/modules/audio_coding/codecs/isac/fix/interface/audio_decoder_isacfix.h"
#include "webrtc/modules/audio_coding/codecs/isac/fix/interface/audio_encoder_isacfix.h"
#endif
#ifdef WEBRTC_CODEC_ISAC
#include "webrtc/modules/audio_coding/codecs/isac/main/interface/audio_decoder_isac.h"
#include "webrtc/modules/audio_coding/codecs/isac/main/interface/audio_encoder_isac.h"
#endif
#ifdef WEBRTC_CODEC_OPUS
#include "webrtc/modules/audio_coding/codecs/opus/interface/audio_encoder_opus.h"
#endif
#include "webrtc/modules/audio_coding/codecs/pcm16b/include/audio_encoder_pcm16b.h"
#ifdef WEBRTC_CODEC_RED
#include "webrtc/modules/audio_coding/codecs/red/audio_encoder_copy_red.h"
#endif

namespace webrtc {
namespace acm2 {

CodecOwner::CodecOwner() : external_speech_encoder_(nullptr) {
}

CodecOwner::~CodecOwner() = default;

namespace {

rtc::scoped_ptr<AudioDecoder> CreateIsacDecoder(
    LockedIsacBandwidthInfo* bwinfo) {
#if defined(WEBRTC_CODEC_ISACFX)
  return rtc_make_scoped_ptr(new AudioDecoderIsacFix(bwinfo));
#elif defined(WEBRTC_CODEC_ISAC)
  return rtc_make_scoped_ptr(new AudioDecoderIsac(bwinfo));
#else
  FATAL() << "iSAC is not supported.";
  return rtc::scoped_ptr<AudioDecoder>();
#endif
}

// Returns a new speech encoder, or null on error.
// TODO(kwiberg): Don't handle errors here (bug 5033)
rtc::scoped_ptr<AudioEncoder> CreateSpeechEncoder(
    const CodecInst& speech_inst,
    LockedIsacBandwidthInfo* bwinfo) {
#if defined(WEBRTC_CODEC_ISACFX)
  if (STR_CASE_CMP(speech_inst.plname, "isac") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderIsacFix(speech_inst, bwinfo));
#endif
#if defined(WEBRTC_CODEC_ISAC)
  if (STR_CASE_CMP(speech_inst.plname, "isac") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderIsac(speech_inst, bwinfo));
#endif
#ifdef WEBRTC_CODEC_OPUS
  if (STR_CASE_CMP(speech_inst.plname, "opus") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderOpus(speech_inst));
#endif
  if (STR_CASE_CMP(speech_inst.plname, "pcmu") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderPcmU(speech_inst));
  if (STR_CASE_CMP(speech_inst.plname, "pcma") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderPcmA(speech_inst));
  if (STR_CASE_CMP(speech_inst.plname, "l16") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderPcm16B(speech_inst));
#ifdef WEBRTC_CODEC_ILBC
  if (STR_CASE_CMP(speech_inst.plname, "ilbc") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderIlbc(speech_inst));
#endif
#ifdef WEBRTC_CODEC_G722
  if (STR_CASE_CMP(speech_inst.plname, "g722") == 0)
    return rtc_make_scoped_ptr(new AudioEncoderG722(speech_inst));
#endif
  LOG_F(LS_ERROR) << "Could not create encoder of type " << speech_inst.plname;
  return rtc::scoped_ptr<AudioEncoder>();
}

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

bool CodecOwner::SetEncoders(const CodecInst& speech_inst,
                             int cng_payload_type,
                             ACMVADMode vad_mode,
                             int red_payload_type) {
  speech_encoder_ = CreateSpeechEncoder(speech_inst, &isac_bandwidth_info_);
  if (!speech_encoder_)
    return false;
  external_speech_encoder_ = nullptr;
  ChangeCngAndRed(cng_payload_type, vad_mode, red_payload_type);
  return true;
}

void CodecOwner::SetEncoders(AudioEncoder* external_speech_encoder,
                             int cng_payload_type,
                             ACMVADMode vad_mode,
                             int red_payload_type) {
  external_speech_encoder_ = external_speech_encoder;
  speech_encoder_.reset();
  ChangeCngAndRed(cng_payload_type, vad_mode, red_payload_type);
}

void CodecOwner::ChangeCngAndRed(int cng_payload_type,
                                 ACMVADMode vad_mode,
                                 int red_payload_type) {
  AudioEncoder* speech_encoder = SpeechEncoder();
  if (cng_payload_type != -1 || red_payload_type != -1) {
    // The RED and CNG encoders need to be in sync with the speech encoder, so
    // reset the latter to ensure its buffer is empty.
    speech_encoder->Reset();
  }
  AudioEncoder* encoder =
      CreateRedEncoder(red_payload_type, speech_encoder, &red_encoder_);
  CreateCngEncoder(cng_payload_type, vad_mode, encoder, &cng_encoder_);
  RTC_DCHECK_EQ(!!speech_encoder_ + !!external_speech_encoder_, 1);
}

AudioDecoder* CodecOwner::GetIsacDecoder() {
  if (!isac_decoder_)
    isac_decoder_ = CreateIsacDecoder(&isac_bandwidth_info_);
  return isac_decoder_.get();
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

AudioEncoder* CodecOwner::SpeechEncoder() {
  const auto* const_this = this;
  return const_cast<AudioEncoder*>(const_this->SpeechEncoder());
}

const AudioEncoder* CodecOwner::SpeechEncoder() const {
  RTC_DCHECK(!speech_encoder_ || !external_speech_encoder_);
  return external_speech_encoder_ ? external_speech_encoder_
                                  : speech_encoder_.get();
}

}  // namespace acm2
}  // namespace webrtc
