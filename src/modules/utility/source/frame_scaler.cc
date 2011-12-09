/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifdef WEBRTC_MODULE_UTILITY_VIDEO

#include "frame_scaler.h"

#include "common_video/libyuv/include/scaler.h"
#include "trace.h"

namespace webrtc {
FrameScaler::FrameScaler()
    : _scaler(new Scaler()),
      _scalerBuffer(),
      _outWidth(0),
      _outHeight(0),
      _inWidth(0),
      _inHeight(0) {}

FrameScaler::~FrameScaler( ) {}

WebRtc_Word32 FrameScaler::ResizeFrameIfNeeded(VideoFrame& videoFrame,
                                               WebRtc_UWord32 outWidth,
                                               WebRtc_UWord32 outHeight) {
  if ( videoFrame.Length( ) == 0) {
    return -1;
  }

  if ((videoFrame.Width() != outWidth) || (videoFrame.Height() != outHeight)) {
    _scaler->Set(videoFrame.Width(), videoFrame.Height(),
                 outWidth, outHeight,
                 kI420, kI420, kScaleBox);

    int reqSize = CalcBufferSize(kI420, _outWidth, _outHeight);
    _scalerBuffer.VerifyAndAllocate(reqSize);
    int ret = _scaler->Scale(videoFrame.Buffer(),
                             _scalerBuffer.Buffer(),
                             reqSize);
    if (ret < 0)
      return ret;
    videoFrame.VerifyAndAllocate(reqSize);
    videoFrame.CopyFrame(videoFrame.Length(), _scalerBuffer.Buffer());
    videoFrame.SetWidth(_outWidth);
    videoFrame.SetHeight(_outHeight);
  }
  return 0;
}
}  // namespace webrtc

#endif  // WEBRTC_MODULE_UTILITY_VIDEO
