/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_CODEC_MANAGER_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_CODEC_MANAGER_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_coding/main/acm2/codec_owner.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/common_types.h"

namespace webrtc {

class AudioDecoder;
class AudioEncoder;
class AudioEncoderMutable;

namespace acm2 {

class CodecManager final {
 public:
  CodecManager();
  ~CodecManager();

  int RegisterEncoder(const CodecInst& send_codec);

  void RegisterEncoder(AudioEncoderMutable* external_speech_encoder);

  int GetCodecInst(CodecInst* current_codec) const;

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

  bool stereo_send() const { return stereo_send_; }

  bool red_enabled() const { return red_enabled_; }

  bool codec_fec_enabled() const { return codec_fec_enabled_; }

  AudioEncoderMutable* CurrentSpeechEncoder() {
    return codec_owner_.SpeechEncoder();
  }
  AudioEncoder* CurrentEncoder() { return codec_owner_.Encoder(); }
  const AudioEncoder* CurrentEncoder() const { return codec_owner_.Encoder(); }

 private:
  int CngPayloadType(int sample_rate_hz) const;

  int RedPayloadType(int sample_rate_hz) const;

  rtc::ThreadChecker thread_checker_;
  uint8_t cng_nb_pltype_;
  uint8_t cng_wb_pltype_;
  uint8_t cng_swb_pltype_;
  uint8_t cng_fb_pltype_;
  uint8_t red_nb_pltype_;
  bool stereo_send_;
  bool dtx_enabled_;
  ACMVADMode vad_mode_;
  CodecInst send_codec_inst_;
  bool red_enabled_;
  bool codec_fec_enabled_;
  CodecOwner codec_owner_;

  DISALLOW_COPY_AND_ASSIGN(CodecManager);
};

}  // namespace acm2
}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_CODEC_MANAGER_H_
