/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_RENT_A_CODEC_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_RENT_A_CODEC_H_

#include <stddef.h>

#include "webrtc/base/array_view.h"
#include "webrtc/base/maybe.h"
#include "webrtc/typedefs.h"

namespace webrtc {

struct CodecInst;

namespace acm2 {

class RentACodec {
 public:
  enum class CodecId {
#if defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX)
    kISAC,
#endif
#ifdef WEBRTC_CODEC_ISAC
    kISACSWB,
#endif
    // Mono
    kPCM16B,
    kPCM16Bwb,
    kPCM16Bswb32kHz,
    // Stereo
    kPCM16B_2ch,
    kPCM16Bwb_2ch,
    kPCM16Bswb32kHz_2ch,
    // Mono
    kPCMU,
    kPCMA,
    // Stereo
    kPCMU_2ch,
    kPCMA_2ch,
#ifdef WEBRTC_CODEC_ILBC
    kILBC,
#endif
#ifdef WEBRTC_CODEC_G722
    kG722,      // Mono
    kG722_2ch,  // Stereo
#endif
#ifdef WEBRTC_CODEC_OPUS
    kOpus,  // Mono and stereo
#endif
    kCNNB,
    kCNWB,
    kCNSWB,
#ifdef ENABLE_48000_HZ
    kCNFB,
#endif
    kAVT,
#ifdef WEBRTC_CODEC_RED
    kRED,
#endif
    kNumCodecs,  // Implementation detail. Don't use.

// Set unsupported codecs to -1.
#if !defined(WEBRTC_CODEC_ISAC) && !defined(WEBRTC_CODEC_ISACFX)
    kISAC = -1,
#endif
#ifndef WEBRTC_CODEC_ISAC
    kISACSWB = -1,
#endif
    // 48 kHz not supported, always set to -1.
    kPCM16Bswb48kHz = -1,
#ifndef WEBRTC_CODEC_ILBC
    kILBC = -1,
#endif
#ifndef WEBRTC_CODEC_G722
    kG722 = -1,      // Mono
    kG722_2ch = -1,  // Stereo
#endif
#ifndef WEBRTC_CODEC_OPUS
    kOpus = -1,  // Mono and stereo
#endif
#ifndef WEBRTC_CODEC_RED
    kRED = -1,
#endif
#ifndef ENABLE_48000_HZ
    kCNFB = -1,
#endif

    kNone = -1
  };

  static inline size_t NumberOfCodecs() {
    return static_cast<size_t>(CodecId::kNumCodecs);
  }

  static inline rtc::Maybe<int> CodecIndexFromId(CodecId codec_id) {
    const int i = static_cast<int>(codec_id);
    return i < static_cast<int>(NumberOfCodecs()) ? i : rtc::Maybe<int>();
  }

  static inline rtc::Maybe<CodecId> CodecIdFromIndex(int codec_index) {
    return static_cast<size_t>(codec_index) < NumberOfCodecs()
               ? static_cast<RentACodec::CodecId>(codec_index)
               : rtc::Maybe<RentACodec::CodecId>();
  }

  static rtc::Maybe<CodecId> CodecIdByParams(const char* payload_name,
                                             int sampling_freq_hz,
                                             int channels);
  static rtc::Maybe<CodecInst> CodecInstById(CodecId codec_id);
  static rtc::Maybe<CodecInst> CodecInstByParams(const char* payload_name,
                                                 int sampling_freq_hz,
                                                 int channels);
  static bool IsCodecValid(const CodecInst& codec_inst);
  static rtc::ArrayView<const CodecInst> Database();
};

}  // namespace acm2
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_RENT_A_CODEC_H_
