/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_processing/aec3/echo_remover.h"

#include <algorithm>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/optional.h"
#include "webrtc/modules/audio_processing/aec3/aec3_constants.h"

namespace webrtc {

namespace {
class EchoRemoverImpl final : public EchoRemover {
 public:
  explicit EchoRemoverImpl(int sample_rate_hz);
  ~EchoRemoverImpl() override;

  void ProcessBlock(const rtc::Optional<size_t>& echo_path_delay_samples,
                    const EchoPathVariability& echo_path_variability,
                    bool capture_signal_saturation,
                    const std::vector<std::vector<float>>& render,
                    std::vector<std::vector<float>>* capture) override;

  void UpdateEchoLeakageStatus(bool leakage_detected) override;

 private:
  const int sample_rate_hz_;

  RTC_DISALLOW_COPY_AND_ASSIGN(EchoRemoverImpl);
};

// TODO(peah): Add functionality.
EchoRemoverImpl::EchoRemoverImpl(int sample_rate_hz)
    : sample_rate_hz_(sample_rate_hz) {
  RTC_DCHECK(sample_rate_hz == 8000 || sample_rate_hz == 16000 ||
             sample_rate_hz == 32000 || sample_rate_hz == 48000);
}

EchoRemoverImpl::~EchoRemoverImpl() = default;

// TODO(peah): Add functionality.
void EchoRemoverImpl::ProcessBlock(
    const rtc::Optional<size_t>& echo_path_delay_samples,
    const EchoPathVariability& echo_path_variability,
    bool capture_signal_saturation,
    const std::vector<std::vector<float>>& render,
    std::vector<std::vector<float>>* capture) {
  RTC_DCHECK(capture);
  RTC_DCHECK_EQ(render.size(), NumBandsForRate(sample_rate_hz_));
  RTC_DCHECK_EQ(capture->size(), NumBandsForRate(sample_rate_hz_));
  RTC_DCHECK_EQ(render[0].size(), kBlockSize);
  RTC_DCHECK_EQ((*capture)[0].size(), kBlockSize);
}

// TODO(peah): Add functionality.
void EchoRemoverImpl::UpdateEchoLeakageStatus(bool leakage_detected) {}

}  // namespace

EchoRemover* EchoRemover::Create(int sample_rate_hz) {
  return new EchoRemoverImpl(sample_rate_hz);
}

}  // namespace webrtc
