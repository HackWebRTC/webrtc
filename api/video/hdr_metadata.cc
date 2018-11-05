/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/hdr_metadata.h"

namespace webrtc {

HdrMasteringMetadata::Chromaticity::Chromaticity() = default;
HdrMasteringMetadata::Chromaticity::Chromaticity(const Chromaticity& rhs) =
    default;
HdrMasteringMetadata::Chromaticity::Chromaticity(Chromaticity&& rhs) = default;
HdrMasteringMetadata::Chromaticity& HdrMasteringMetadata::Chromaticity::
operator=(const Chromaticity& rhs) = default;

HdrMasteringMetadata::HdrMasteringMetadata() = default;
HdrMasteringMetadata::HdrMasteringMetadata(const HdrMasteringMetadata& rhs) =
    default;
HdrMasteringMetadata::HdrMasteringMetadata(HdrMasteringMetadata&& rhs) =
    default;
HdrMasteringMetadata& HdrMasteringMetadata::operator=(
    const HdrMasteringMetadata& rhs) = default;

HdrMetadata::HdrMetadata() = default;
HdrMetadata::HdrMetadata(const HdrMetadata& rhs) = default;
HdrMetadata::HdrMetadata(HdrMetadata&& rhs) = default;
HdrMetadata& HdrMetadata::operator=(const HdrMetadata& rhs) = default;

}  // namespace webrtc
