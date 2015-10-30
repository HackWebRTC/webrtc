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

}  // namespace acm2
}  // namespace webrtc
