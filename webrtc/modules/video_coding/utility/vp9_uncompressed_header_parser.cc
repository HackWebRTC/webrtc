/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/video_coding/utility/vp9_uncompressed_header_parser.h"

namespace webrtc {

namespace vp9 {
namespace {
const size_t kVp9MaxProfile = 4;
const size_t kVp9NumRefsPerFrame = 3;
const size_t kVp9MaxRefLFDeltas = 4;
const size_t kVp9MaxModeLFDeltas = 2;
}  // namespace

static uint8_t VP9ReadProfile(VP9BitReader* br) {
  uint8_t profile = 0;
  if (br->GetBit())
    profile |= 1;
  if (br->GetBit())
    profile |= 2;
  if (profile > 2 && br->GetBit())
    profile += 1;
  return profile;
}

static bool VP9ReadColorConfig(VP9BitReader* br, uint8_t profile) {
  if (profile == 2 || profile == 3) {
    // Bitdepth.
    br->GetBit();
  }

  uint8_t color_space = br->GetValue(3);
  // SRGB is 7.
  if (color_space != 7) {
    // YUV range flag.
    br->GetBit();
    if (profile == 1 || profile == 3) {
      // Subsampling x.
      br->GetBit();
      // Subsampling y.
      br->GetBit();
      // Reserved.
      if (br->GetBit()) {
        LOG(LS_WARNING) << "Failed to get QP. Reserved bit set.";
        return false;
      }
    }
  } else {
    if (profile == 1 || profile == 3) {
      // Reserved.
      if (br->GetBit()) {
        LOG(LS_WARNING) << "Failed to get QP. Reserved bit set.";
        return false;
      }
    } else {
      LOG(LS_WARNING) << "Failed to get QP. 4:4:4 color not supported in "
                         "profile 0 or 2.";
      return false;
    }
  }

  return true;
}

static void VP9ReadFrameSize(VP9BitReader* br) {
  // Frame width.
  br->GetValue(16);
  // Frame height.
  br->GetValue(16);
}

static void VP9ReadRenderSize(VP9BitReader* br) {
  // Scaling.
  if (br->GetBit()) {
    // Render width.
    br->GetValue(16);
    // Render height.
    br->GetValue(16);
  }
}

static void VP9ReadFrameSizeFromRefs(VP9BitReader* br) {
  int found_ref = 0;
  for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
    // Size in refs.
    found_ref = br->GetBit();
    if (found_ref)
      break;
  }

  if (!found_ref)
    VP9ReadFrameSize(br);

  VP9ReadRenderSize(br);
}

static void VP9ReadInterpolationFilter(VP9BitReader* br) {
  if (br->GetBit())
    return;

  br->GetValue(2);
}

static void VP9ReadLoopfilter(VP9BitReader* br) {
  // Filter level.
  br->GetValue(6);
  // Sharpness level.
  br->GetValue(3);
  uint32_t mode_ref_delta_enabled = br->GetBit();
  if (mode_ref_delta_enabled) {
    uint32_t mode_ref_delta_update = br->GetBit();
    if (mode_ref_delta_update) {
      for (size_t i = 0; i < kVp9MaxRefLFDeltas; i++) {
        if (br->GetBit())
          br->GetSignedValue(6);
      }
      for (size_t i = 0; i < kVp9MaxModeLFDeltas; i++) {
        if (br->GetBit())
          br->GetSignedValue(6);
      }
    }
  }
}

bool GetQp(const uint8_t* buf, size_t length, int* qp) {
  VP9BitReader br(buf, length);

  // Frame marker.
  if (br.GetValue(2) != 0x2) {
    LOG(LS_WARNING) << "Failed to get QP. Frame marker should be 2.";
    return false;
  }

  // Profile.
  uint8_t profile = VP9ReadProfile(&br);
  if (profile > kVp9MaxProfile) {
    LOG(LS_WARNING) << "Failed to get QP. Unsupported bitstream profile: "
                    << profile;
    return false;
  }

  // Show existing frame.
  if (br.GetBit())
    return false;

  // Frame type: KEY_FRAME(0), INTER_FRAME(1).
  uint8_t frame_type = br.GetBit();
  // Show frame.
  uint8_t show_frame = br.GetBit();
  // Error resilient.
  uint8_t error_resilient = br.GetBit();

  if (!frame_type) {
    // Sync code.
    uint32_t sync_code = br.GetValue(24);
    if (sync_code != 0x498342) {
      LOG(LS_WARNING) << "Failed to get QP. Invalid sync code.";
      return false;
    }

    if (!VP9ReadColorConfig(&br, profile))
      return false;

    VP9ReadFrameSize(&br);
    VP9ReadRenderSize(&br);
  } else {
    uint8_t intra_only = 0;
    if (!show_frame)
      intra_only = br.GetBit();

    if (!error_resilient)
      // Reset frame context.
      br.GetValue(2);

    if (intra_only) {
      // Sync code.
      if (br.GetValue(24) != 0x498342) {
        LOG(LS_WARNING) << "Failed to get QP. Invalid sync code.";
        return false;
      }
      if (profile > 0) {
        if (!VP9ReadColorConfig(&br, profile))
          return false;
      }
      // Refresh frame flags.
      br.GetValue(8);

      VP9ReadFrameSize(&br);
      VP9ReadRenderSize(&br);
    } else {
      // Refresh frame flags.
      br.GetValue(8);

      for (size_t i = 0; i < kVp9NumRefsPerFrame; i++) {
        // Ref frame index.
        br.GetValue(3);
        // Ref frame sign biases.
        br.GetBit();
      }

      VP9ReadFrameSizeFromRefs(&br);
      // Allow high precision mv.
      br.GetBit();
      // Interpolation filter.
      VP9ReadInterpolationFilter(&br);
    }
  }

  if (!error_resilient) {
    // Refresh frame context.
    br.GetBit();
    // Frame parallel decoding mode.
    br.GetBit();
  }

  // Frame context index.
  br.GetValue(2);

  VP9ReadLoopfilter(&br);

  // Base QP.
  const int base_q0 = br.GetValue(8);
  *qp = base_q0;
  return true;
}

}  // namespace vp9

}  // namespace webrtc
