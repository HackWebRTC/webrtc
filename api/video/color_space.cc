/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/video/color_space.h"

namespace webrtc {

ColorSpace::ColorSpace() = default;
ColorSpace::ColorSpace(const ColorSpace& other) = default;
ColorSpace::ColorSpace(ColorSpace&& other) = default;
ColorSpace& ColorSpace::operator=(const ColorSpace& other) = default;

ColorSpace::ColorSpace(PrimaryID primaries,
                       TransferID transfer,
                       MatrixID matrix,
                       RangeID range)
    : ColorSpace(primaries, transfer, matrix, range, nullptr) {}

ColorSpace::ColorSpace(PrimaryID primaries,
                       TransferID transfer,
                       MatrixID matrix,
                       RangeID range,
                       const HdrMetadata* hdr_metadata)
    : primaries_(primaries),
      transfer_(transfer),
      matrix_(matrix),
      range_(range),
      hdr_metadata_(hdr_metadata ? absl::make_optional(*hdr_metadata)
                                 : absl::nullopt) {}

ColorSpace::PrimaryID ColorSpace::primaries() const {
  return primaries_;
}

ColorSpace::TransferID ColorSpace::transfer() const {
  return transfer_;
}

ColorSpace::MatrixID ColorSpace::matrix() const {
  return matrix_;
}

ColorSpace::RangeID ColorSpace::range() const {
  return range_;
}

const HdrMetadata* ColorSpace::hdr_metadata() const {
  return hdr_metadata_ ? &*hdr_metadata_ : nullptr;
}

}  // namespace webrtc
