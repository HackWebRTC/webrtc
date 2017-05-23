/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/include/audio_processing.h"

#include "webrtc/base/checks.h"
#include "webrtc/modules/audio_processing/include/aec_dump.h"
// TODO(aleloi): remove AecDump header usage when internal projcets
// have updated. See https://bugs.webrtc.org/7404.

namespace webrtc {

Beamforming::Beamforming()
    : enabled(false),
      array_geometry(),
      target_direction(
          SphericalPointf(static_cast<float>(M_PI) / 2.f, 0.f, 1.f)) {}
Beamforming::Beamforming(bool enabled, const std::vector<Point>& array_geometry)
    : Beamforming(enabled,
                  array_geometry,
                  SphericalPointf(static_cast<float>(M_PI) / 2.f, 0.f, 1.f)) {}

Beamforming::Beamforming(bool enabled,
                         const std::vector<Point>& array_geometry,
                         SphericalPointf target_direction)
    : enabled(enabled),
      array_geometry(array_geometry),
      target_direction(target_direction) {}

Beamforming::~Beamforming() {}

// TODO(aleloi): make pure virtual when internal projects have
// updated. See https://bugs.webrtc.org/7404
void AudioProcessing::AttachAecDump(std::unique_ptr<AecDump> aec_dump) {
  RTC_NOTREACHED();
}

// If no AecDump is attached, this has no effect. If an AecDump is
// attached, it's destructor is called. The d-tor may block until
// all pending logging tasks are completed.
//
// TODO(aleloi): make pure virtual when internal projects have
// updated. See https://bugs.webrtc.org/7404
void AudioProcessing::DetachAecDump() {
  RTC_NOTREACHED();
}

}  // namespace webrtc
