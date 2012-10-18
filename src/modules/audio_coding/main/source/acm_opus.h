/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_OPUS_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_OPUS_H_

#include "acm_generic_codec.h"
#include "opus_interface.h"
#include "resampler.h"

namespace webrtc {

class ACMOpus : public ACMGenericCodec {
 public:
  ACMOpus(int16_t codecID);
  ~ACMOpus();

  ACMGenericCodec* CreateInstance(void);

  int16_t InternalEncode(uint8_t* bitstream, int16_t* bitStreamLenByte);

  int16_t InternalInitEncoder(WebRtcACMCodecParams *codecParams);

  int16_t InternalInitDecoder(WebRtcACMCodecParams *codecParams);

 protected:
  int16_t DecodeSafe(uint8_t* bitStream, int16_t bitStreamLenByte,
                     int16_t* audio, int16_t* audioSamples, int8_t* speechType);

  int32_t CodecDef(WebRtcNetEQ_CodecDef& codecDef, const CodecInst& codecInst);

  void DestructEncoderSafe();

  void DestructDecoderSafe();

  int16_t InternalCreateEncoder();

  int16_t InternalCreateDecoder();

  void InternalDestructEncoderInst(void* ptrInst);

  int16_t SetBitRateSafe(const int32_t rate);

  OpusEncInst* _encoderInstPtr;
  OpusDecInst* _decoderInstPtr;
  uint16_t _sampleFreq;
  uint16_t _bitrate;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_OPUS_H_
