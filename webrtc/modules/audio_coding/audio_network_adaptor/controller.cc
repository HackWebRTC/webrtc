/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/audio_network_adaptor/controller.h"

namespace webrtc {

Controller::NetworkMetrics::NetworkMetrics() = default;

Controller::NetworkMetrics::~NetworkMetrics() = default;

Controller::Constraints::Constraints() = default;

Controller::Constraints::~Constraints() = default;

Controller::Constraints::FrameLengthRange::FrameLengthRange(
    int min_frame_length_ms,
    int max_frame_length_ms)
    : min_frame_length_ms(min_frame_length_ms),
      max_frame_length_ms(max_frame_length_ms) {}

Controller::Constraints::FrameLengthRange::~FrameLengthRange() = default;

void Controller::SetConstraints(const Constraints& constraints) {}

}  // namespace webrtc
