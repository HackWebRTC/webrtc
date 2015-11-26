/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_ACM2_CODEC_MANAGER_H_
#define WEBRTC_MODULES_AUDIO_CODING_ACM2_CODEC_MANAGER_H_

#include <map>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_coding/acm2/rent_a_codec.h"
#include "webrtc/modules/audio_coding/include/audio_coding_module_typedefs.h"
#include "webrtc/common_types.h"

namespace webrtc {

class AudioDecoder;
class AudioEncoder;

namespace acm2 {

class CodecManager final {
 public:
  CodecManager();
  ~CodecManager();

  int RegisterEncoder(const CodecInst& send_codec);

  void RegisterEncoder(AudioEncoder* external_speech_encoder);

  rtc::Optional<CodecInst> GetCodecInst() const;

  bool SetCopyRed(bool enable);

  int SetVAD(bool enable, ACMVADMode mode);

  void VAD(bool* dtx_enabled, bool* vad_enabled, ACMVADMode* mode) const;

  int SetCodecFEC(bool enable_codec_fec);

  // Returns a pointer to AudioDecoder of the given codec. For iSAC, encoding
  // and decoding have to be performed on a shared codec instance. By calling
  // this method, we get the codec instance that ACM owns.
  // If |codec| does not share an instance between encoder and decoder, returns
  // null.
  AudioDecoder* GetAudioDecoder(const CodecInst& codec);

  bool red_enabled() const { return codec_stack_params_.use_red; }

  bool codec_fec_enabled() const { return codec_stack_params_.use_codec_fec; }

  AudioEncoder* CurrentEncoder() { return rent_a_codec_.GetEncoderStack(); }
  const AudioEncoder* CurrentEncoder() const {
    return rent_a_codec_.GetEncoderStack();
  }

  bool CurrentEncoderIsOpus() const { return encoder_is_opus_; }

 private:
  rtc::ThreadChecker thread_checker_;
  CodecInst send_codec_inst_;
  RentACodec rent_a_codec_;
  RentACodec::StackParameters codec_stack_params_;

  bool encoder_is_opus_;

  RTC_DISALLOW_COPY_AND_ASSIGN(CodecManager);
};

}  // namespace acm2
}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_ACM2_CODEC_MANAGER_H_
