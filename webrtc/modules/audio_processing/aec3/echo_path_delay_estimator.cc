/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_processing/aec3/echo_path_delay_estimator.h"

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/aec3/aec3_constants.h"
#include "webrtc/modules/audio_processing/logging/apm_data_dumper.h"

namespace webrtc {

// TODO(peah): Add functionality.
EchoPathDelayEstimator::EchoPathDelayEstimator(ApmDataDumper* data_dumper,
                                               int sample_rate_hz) {
  RTC_DCHECK(data_dumper);
  RTC_DCHECK(sample_rate_hz == 8000 || sample_rate_hz == 16000 ||
             sample_rate_hz == 32000 || sample_rate_hz == 48000);
}

EchoPathDelayEstimator::~EchoPathDelayEstimator() = default;

// TODO(peah): Add functionality.
rtc::Optional<size_t> EchoPathDelayEstimator::EstimateDelay(
    rtc::ArrayView<const float> render,
    rtc::ArrayView<const float> capture) {
  RTC_DCHECK_EQ(render.size(), kBlockSize);
  RTC_DCHECK_EQ(capture.size(), kBlockSize);
  return rtc::Optional<size_t>();
}

}  // namespace webrtc
