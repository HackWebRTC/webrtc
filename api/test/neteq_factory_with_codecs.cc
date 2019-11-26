/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/neteq_factory_with_codecs.h"

#include "api/audio_codecs/audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/neteq/default_neteq_controller_factory.h"
#include "api/neteq/neteq_controller_factory.h"
#include "api/neteq/neteq_factory.h"
#include "modules/audio_coding/neteq/decision_logic.h"
#include "modules/audio_coding/neteq/neteq_impl.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
namespace {

class NetEqFactoryWithCodecs final : public NetEqFactory {
 public:
  std::unique_ptr<NetEq> CreateNetEq(const NetEq::Config& config,
                                     Clock* clock) const override {
    return std::make_unique<NetEqImpl>(
        config, NetEqImpl::Dependencies(config, clock, decoder_factory_,
                                        *controller_factory_));
  }
  std::unique_ptr<NetEq> CreateNetEq(
      const NetEq::Config& config,
      const rtc::scoped_refptr<AudioDecoderFactory>& decoder_factory,
      Clock* clock) const override {
    return std::make_unique<NetEqImpl>(
        config, NetEqImpl::Dependencies(config, clock, decoder_factory,
                                        *controller_factory_));
  }

 private:
  const rtc::scoped_refptr<AudioDecoderFactory> decoder_factory_ =
      CreateBuiltinAudioDecoderFactory();
  const std::unique_ptr<NetEqControllerFactory> controller_factory_ =
      std::make_unique<DefaultNetEqControllerFactory>();
};

}  // namespace

std::unique_ptr<NetEqFactory> CreateNetEqFactoryWithCodecs() {
  return std::make_unique<NetEqFactoryWithCodecs>();
}

}  // namespace webrtc
