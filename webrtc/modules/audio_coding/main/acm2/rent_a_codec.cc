/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/acm2/rent_a_codec.h"

#include "webrtc/base/logging.h"
#include "webrtc/modules/audio_coding/codecs/g711/include/audio_encoder_pcm.h"
#ifdef WEBRTC_CODEC_G722
#include "webrtc/modules/audio_coding/codecs/g722/include/audio_encoder_g722.h"
#endif
#ifdef WEBRTC_CODEC_ILBC
#include "webrtc/modules/audio_coding/codecs/ilbc/include/audio_encoder_ilbc.h"
#endif
#ifdef WEBRTC_CODEC_ISACFX
#include "webrtc/modules/audio_coding/codecs/isac/fix/include/audio_decoder_isacfix.h"
#include "webrtc/modules/audio_coding/codecs/isac/fix/include/audio_encoder_isacfix.h"
#endif
#ifdef WEBRTC_CODEC_ISAC
#include "webrtc/modules/audio_coding/codecs/isac/main/include/audio_decoder_isac.h"
#include "webrtc/modules/audio_coding/codecs/isac/main/include/audio_encoder_isac.h"
#endif
#ifdef WEBRTC_CODEC_OPUS
#include "webrtc/modules/audio_coding/codecs/opus/include/audio_encoder_opus.h"
#endif
#include "webrtc/modules/audio_coding/codecs/pcm16b/include/audio_encoder_pcm16b.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_common_defs.h"

namespace webrtc {
namespace acm2 {

rtc::Maybe<RentACodec::CodecId> RentACodec::CodecIdByParams(
    const char* payload_name,
    int sampling_freq_hz,
    int channels) {
  return CodecIdFromIndex(
      ACMCodecDB::CodecId(payload_name, sampling_freq_hz, channels));
}

rtc::Maybe<CodecInst> RentACodec::CodecInstById(CodecId codec_id) {
  rtc::Maybe<int> mi = CodecIndexFromId(codec_id);
  return mi ? rtc::Maybe<CodecInst>(Database()[*mi]) : rtc::Maybe<CodecInst>();
}

rtc::Maybe<RentACodec::CodecId> RentACodec::CodecIdByInst(
    const CodecInst& codec_inst) {
  return CodecIdFromIndex(ACMCodecDB::CodecNumber(codec_inst));
}

rtc::Maybe<CodecInst> RentACodec::CodecInstByParams(const char* payload_name,
                                                    int sampling_freq_hz,
                                                    int channels) {
  rtc::Maybe<CodecId> codec_id =
      CodecIdByParams(payload_name, sampling_freq_hz, channels);
  if (!codec_id)
    return rtc::Maybe<CodecInst>();
  rtc::Maybe<CodecInst> ci = CodecInstById(*codec_id);
  RTC_DCHECK(ci);

  // Keep the number of channels from the function call. For most codecs it
  // will be the same value as in default codec settings, but not for all.
  ci->channels = channels;

  return ci;
}

bool RentACodec::IsCodecValid(const CodecInst& codec_inst) {
  return ACMCodecDB::CodecNumber(codec_inst) >= 0;
}

rtc::Maybe<bool> RentACodec::IsSupportedNumChannels(CodecId codec_id,
                                                    int num_channels) {
  auto i = CodecIndexFromId(codec_id);
  return i ? rtc::Maybe<bool>(ACMCodecDB::codec_settings_[*i].channel_support >=
                              num_channels)
           : rtc::Maybe<bool>();
}

rtc::ArrayView<const CodecInst> RentACodec::Database() {
  return rtc::ArrayView<const CodecInst>(ACMCodecDB::database_,
                                         NumberOfCodecs());
}

rtc::Maybe<NetEqDecoder> RentACodec::NetEqDecoderFromCodecId(CodecId codec_id,
                                                             int num_channels) {
  rtc::Maybe<int> i = CodecIndexFromId(codec_id);
  if (!i)
    return rtc::Maybe<NetEqDecoder>();
  const NetEqDecoder ned = ACMCodecDB::neteq_decoders_[*i];
  return rtc::Maybe<NetEqDecoder>(
      (ned == NetEqDecoder::kDecoderOpus && num_channels == 2)
          ? NetEqDecoder::kDecoderOpus_2ch
          : ned);
}

namespace {

// Returns a new speech encoder, or null on error.
// TODO(kwiberg): Don't handle errors here (bug 5033)
rtc::scoped_ptr<AudioEncoder> CreateEncoder(
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

}  // namespace

RentACodec::RentACodec() = default;
RentACodec::~RentACodec() = default;

AudioEncoder* RentACodec::RentEncoder(const CodecInst& codec_inst) {
  rtc::scoped_ptr<AudioEncoder> enc =
      CreateEncoder(codec_inst, &isac_bandwidth_info_);
  if (!enc)
    return nullptr;
  encoder_ = enc.Pass();
  return encoder_.get();
}

AudioDecoder* RentACodec::RentIsacDecoder() {
  if (!isac_decoder_)
    isac_decoder_ = CreateIsacDecoder(&isac_bandwidth_info_);
  return isac_decoder_.get();
}

}  // namespace acm2
}  // namespace webrtc
