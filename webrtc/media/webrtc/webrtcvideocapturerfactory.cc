/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/media/webrtc/webrtcvideocapturer.h"
#include "webrtc/media/webrtc/webrtcvideocapturerfactory.h"

namespace cricket {

VideoCapturer* WebRtcVideoDeviceCapturerFactory::Create(const Device& device) {
#ifdef HAVE_WEBRTC_VIDEO
  rtc::scoped_ptr<WebRtcVideoCapturer> capturer(
      new WebRtcVideoCapturer());
  if (!capturer->Init(device)) {
    return nullptr;
  }
  return capturer.release();
#else
  return nullptr;
#endif
}

}  // namespace cricket
