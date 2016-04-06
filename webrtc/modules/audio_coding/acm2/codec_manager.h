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
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_coding/acm2/rent_a_codec.h"
#include "webrtc/modules/audio_coding/include/audio_coding_module.h"
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

  // Parses the given specification. On success, returns true and updates the
  // stored CodecInst and stack parameters; on error, returns false.
  bool RegisterEncoder(const CodecInst& send_codec);

  static CodecInst ForgeCodecInst(const AudioEncoder* external_speech_encoder);

  const CodecInst* GetCodecInst() const {
    return send_codec_inst_ ? &*send_codec_inst_ : nullptr;
  }

  void UnsetCodecInst() { send_codec_inst_ = rtc::Optional<CodecInst>(); }

  const RentACodec::StackParameters* GetStackParams() const {
    return &codec_stack_params_;
  }
  RentACodec::StackParameters* GetStackParams() { return &codec_stack_params_; }

  bool SetCopyRed(bool enable);

  bool SetVAD(bool enable, ACMVADMode mode);

  bool SetCodecFEC(bool enable_codec_fec);

  // Uses the provided Rent-A-Codec to create a new encoder stack, if we have a
  // complete specification; if so, it is then passed to set_encoder. On error,
  // returns false.
  bool MakeEncoder(RentACodec* rac, AudioCodingModule* acm) {
    RTC_DCHECK(rac);
    RTC_DCHECK(acm);
    if (!codec_stack_params_.speech_encoder && send_codec_inst_) {
      // We have no speech encoder, but we have a specification for making one.
      auto enc = rac->RentEncoder(*send_codec_inst_);
      if (!enc)
        return false;
      codec_stack_params_.speech_encoder = std::move(enc);
    }
    auto stack = rac->RentEncoderStack(&codec_stack_params_);
    if (stack) {
      // Give new encoder stack to the ACM.
      acm->SetEncoder(std::move(stack));
    } else {
      // The specification was good but incomplete, so we have no encoder stack
      // to give to the ACM.
    }
    return true;
  }

 private:
  rtc::ThreadChecker thread_checker_;
  rtc::Optional<CodecInst> send_codec_inst_;
  RentACodec::StackParameters codec_stack_params_;

  RTC_DISALLOW_COPY_AND_ASSIGN(CodecManager);
};

}  // namespace acm2
}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_ACM2_CODEC_MANAGER_H_
