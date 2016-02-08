/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MEDIA_BASE_FAKECAPTUREMANAGER_H_
#define WEBRTC_MEDIA_BASE_FAKECAPTUREMANAGER_H_

#include "webrtc/media/base/capturemanager.h"

namespace cricket {

class FakeCaptureManager : public CaptureManager {
 public:
  FakeCaptureManager() {}
  ~FakeCaptureManager() {}

  void AddVideoSink(VideoCapturer* video_capturer,
                    rtc::VideoSinkInterface<VideoFrame>* sink) override {}
  void RemoveVideoSink(VideoCapturer* video_capturer,
                       rtc::VideoSinkInterface<VideoFrame>* sink) override {}
};

}  // namespace cricket

#endif  // WEBRTC_MEDIA_BASE_FAKECAPTUREMANAGER_H_
