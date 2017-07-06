/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/include/audio_processing.h"
#include "webrtc/test/fuzzers/audio_processing_fuzzer.h"

#include "webrtc/rtc_base/optional.h"

namespace webrtc {

std::unique_ptr<AudioProcessing> CreateAPM(const uint8_t** data,
                                           size_t* remaining_size) {
  // Parse boolean values for optionally enabling different
  // configurable public components of APM.
  auto exp_agc = ParseBool(data, remaining_size);
  auto exp_ns = ParseBool(data, remaining_size);
  auto bf = ParseBool(data, remaining_size);
  auto ef = ParseBool(data, remaining_size);
  auto raf = ParseBool(data, remaining_size);
  auto da = ParseBool(data, remaining_size);
  auto ie = ParseBool(data, remaining_size);
  auto red = ParseBool(data, remaining_size);
  auto lc = ParseBool(data, remaining_size);
  auto hpf = ParseBool(data, remaining_size);
  auto aec3 = ParseBool(data, remaining_size);

  if (!(exp_agc && exp_ns && bf && ef && raf && da && ie && red && lc && hpf &&
        aec3)) {
    return nullptr;
  }

  // Components can be enabled through webrtc::Config and
  // webrtc::AudioProcessingConfig.
  Config config;

  config.Set<ExperimentalAgc>(new ExperimentalAgc(*exp_agc));
  config.Set<ExperimentalNs>(new ExperimentalNs(*exp_ns));
  if (*bf) {
    config.Set<Beamforming>(new Beamforming());
  }
  config.Set<ExtendedFilter>(new ExtendedFilter(*ef));
  config.Set<RefinedAdaptiveFilter>(new RefinedAdaptiveFilter(*raf));
  config.Set<DelayAgnostic>(new DelayAgnostic(*da));
  config.Set<Intelligibility>(new Intelligibility(*ie));

  std::unique_ptr<AudioProcessing> apm(AudioProcessing::Create(config));

  webrtc::AudioProcessing::Config apm_config;
  apm_config.residual_echo_detector.enabled = *red;
  apm_config.level_controller.enabled = *lc;
  apm_config.high_pass_filter.enabled = *hpf;
  apm_config.echo_canceller3.enabled = *aec3;

  apm->ApplyConfig(apm_config);

  return apm;
}

void FuzzOneInput(const uint8_t* data, size_t size) {
  auto apm = CreateAPM(&data, &size);
  FuzzAudioProcessing(data, size, std::move(apm));
}
}  // namespace webrtc
