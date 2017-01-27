/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_DELAY_ESTIMATOR_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_DELAY_ESTIMATOR_H_

#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/optional.h"

namespace webrtc {

class ApmDataDumper;

class EchoPathDelayEstimator {
 public:
  EchoPathDelayEstimator(ApmDataDumper* data_dumper, int sample_rate_hz);
  ~EchoPathDelayEstimator();
  rtc::Optional<size_t> EstimateDelay(rtc::ArrayView<const float> render,
                                      rtc::ArrayView<const float> capture);

 private:
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(EchoPathDelayEstimator);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_AEC3_ECHO_PATH_DELAY_ESTIMATOR_H_
