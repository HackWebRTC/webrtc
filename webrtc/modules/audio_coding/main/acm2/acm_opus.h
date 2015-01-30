/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_OPUS_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_OPUS_H_

#include "webrtc/common_audio/resampler/include/resampler.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_generic_codec.h"

struct WebRtcOpusEncInst;
struct WebRtcOpusDecInst;

namespace webrtc {

namespace acm2 {

class ACMOpus : public ACMGenericCodec {
 public:
  ACMOpus(int16_t codec_id, bool enable_red);
  ~ACMOpus();

  ACMGenericCodec* CreateInstance(void);

  int16_t InternalEncode(uint8_t* bitstream,
                         int16_t* bitstream_len_byte) override
      EXCLUSIVE_LOCKS_REQUIRED(codec_wrapper_lock_);

  int16_t InitEncoderSafe(WebRtcACMCodecParams* codec_params,
                          bool force_initialization) override
      EXCLUSIVE_LOCKS_REQUIRED(codec_wrapper_lock_);

  int16_t InternalInitEncoder(WebRtcACMCodecParams *codec_params);

  int SetFEC(bool enable_fec) override;

  int SetOpusApplication(OpusApplicationMode mode) override;

  int SetPacketLossRate(int loss_rate) override;

  int SetOpusMaxPlaybackRate(int frequency_hz) override;

 protected:
  void DestructEncoderSafe();

  int16_t InternalCreateEncoder();

  int16_t SetBitRateSafe(const int32_t rate) override
      EXCLUSIVE_LOCKS_REQUIRED(codec_wrapper_lock_);

  WebRtcOpusEncInst* encoder_inst_ptr_;
  uint16_t sample_freq_;
  int32_t bitrate_;
  int channels_;

  int packet_loss_rate_;

  OpusApplicationMode application_;
};

}  // namespace acm2

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_OPUS_H_
