/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/codecs/builtin_audio_decoder_factory.h"

#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/audio_coding/codecs/cng/webrtc_cng.h"
#include "webrtc/modules/audio_coding/codecs/g711/audio_decoder_pcm.h"
#ifdef WEBRTC_CODEC_G722
#include "webrtc/modules/audio_coding/codecs/g722/audio_decoder_g722.h"
#endif
#ifdef WEBRTC_CODEC_ILBC
#include "webrtc/modules/audio_coding/codecs/ilbc/audio_decoder_ilbc.h"
#endif
#ifdef WEBRTC_CODEC_ISACFX
#include "webrtc/modules/audio_coding/codecs/isac/fix/include/audio_decoder_isacfix.h"
#endif
#ifdef WEBRTC_CODEC_ISAC
#include "webrtc/modules/audio_coding/codecs/isac/main/include/audio_decoder_isac.h"
#endif
#ifdef WEBRTC_CODEC_OPUS
#include "webrtc/modules/audio_coding/codecs/opus/audio_decoder_opus.h"
#endif
#include "webrtc/modules/audio_coding/codecs/pcm16b/audio_decoder_pcm16b.h"

namespace webrtc {

namespace {

struct NamedDecoderConstructor {
  const char* name;
  std::unique_ptr<AudioDecoder> (*constructor)(int clockrate_hz,
                                               int num_channels);
};

std::unique_ptr<AudioDecoder> Unique(AudioDecoder* d) {
  return std::unique_ptr<AudioDecoder>(d);
}

// TODO(kwiberg): These factory functions should probably be moved to each
// decoder.
NamedDecoderConstructor decoder_constructors[] = {
    {"pcmu",
     [](int clockrate_hz, int num_channels) {
       return clockrate_hz == 8000 && num_channels >= 1
                  ? Unique(new AudioDecoderPcmU(num_channels))
                  : nullptr;
     }},
    {"pcma",
     [](int clockrate_hz, int num_channels) {
       return clockrate_hz == 8000 && num_channels >= 1
                  ? Unique(new AudioDecoderPcmA(num_channels))
                  : nullptr;
     }},
#ifdef WEBRTC_CODEC_ILBC
    {"ilbc",
     [](int clockrate_hz, int num_channels) {
       return clockrate_hz == 8000 && num_channels == 1
                  ? Unique(new AudioDecoderIlbc)
                  : nullptr;
     }},
#endif
#if defined(WEBRTC_CODEC_ISACFX)
    {"isac",
     [](int clockrate_hz, int num_channels) {
       return clockrate_hz == 16000 && num_channels == 1
                  ? Unique(new AudioDecoderIsacFix)
                  : nullptr;
     }},
#elif defined(WEBRTC_CODEC_ISAC)
    {"isac",
     [](int clockrate_hz, int num_channels) {
       return (clockrate_hz == 16000 || clockrate_hz == 32000) &&
                      num_channels == 1
                  ? Unique(new AudioDecoderIsac)
                  : nullptr;
     }},
#endif
    {"l16",
     [](int clockrate_hz, int num_channels) {
       return num_channels >= 1 ? Unique(new AudioDecoderPcm16B(num_channels))
                                : nullptr;
     }},
#ifdef WEBRTC_CODEC_G722
    {"g722",
     [](int clockrate_hz, int num_channels) {
       if (clockrate_hz == 8000) {
         if (num_channels == 1)
           return Unique(new AudioDecoderG722);
         if (num_channels == 2)
           return Unique(new AudioDecoderG722Stereo);
       }
       return Unique(nullptr);
     }},
#endif
#ifdef WEBRTC_CODEC_OPUS
    {"opus",
     [](int clockrate_hz, int num_channels) {
       return clockrate_hz == 48000 && (num_channels == 1 || num_channels == 2)
                  ? Unique(new AudioDecoderOpus(num_channels))
                  : nullptr;
     }},
#endif
};

class BuiltinAudioDecoderFactory : public AudioDecoderFactory {
 public:
  std::vector<SdpAudioFormat> GetSupportedFormats() override {
    FATAL() << "Not implemented yet!";
  }

  std::unique_ptr<AudioDecoder> MakeAudioDecoder(
      const SdpAudioFormat& format) override {
    for (const auto& dc : decoder_constructors) {
      if (STR_CASE_CMP(format.name.c_str(), dc.name) == 0) {
        return std::unique_ptr<AudioDecoder>(
            dc.constructor(format.clockrate_hz, format.num_channels));
      }
    }
    return nullptr;
  }
};

}  // namespace

std::unique_ptr<AudioDecoderFactory> CreateBuiltinAudioDecoderFactory() {
  return std::unique_ptr<AudioDecoderFactory>(new BuiltinAudioDecoderFactory);
}

}  // namespace webrtc
