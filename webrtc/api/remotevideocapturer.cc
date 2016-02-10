/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/remotevideocapturer.h"

#include "webrtc/base/logging.h"
#include "webrtc/media/base/videoframe.h"

namespace webrtc {

RemoteVideoCapturer::RemoteVideoCapturer() {}

RemoteVideoCapturer::~RemoteVideoCapturer() {}

cricket::CaptureState RemoteVideoCapturer::Start(
    const cricket::VideoFormat& capture_format) {
  if (capture_state() == cricket::CS_RUNNING) {
    LOG(LS_WARNING)
        << "RemoteVideoCapturer::Start called when it's already started.";
    return capture_state();
  }

  LOG(LS_INFO) << "RemoteVideoCapturer::Start";
  SetCaptureFormat(&capture_format);
  return cricket::CS_RUNNING;
}

void RemoteVideoCapturer::Stop() {
  if (capture_state() == cricket::CS_STOPPED) {
    LOG(LS_WARNING)
        << "RemoteVideoCapturer::Stop called when it's already stopped.";
    return;
  }

  LOG(LS_INFO) << "RemoteVideoCapturer::Stop";
  SetCaptureFormat(NULL);
  SetCaptureState(cricket::CS_STOPPED);
}

bool RemoteVideoCapturer::IsRunning() {
  return capture_state() == cricket::CS_RUNNING;
}

bool RemoteVideoCapturer::GetPreferredFourccs(std::vector<uint32_t>* fourccs) {
  if (!fourccs)
    return false;
  fourccs->push_back(cricket::FOURCC_I420);
  return true;
}

bool RemoteVideoCapturer::GetBestCaptureFormat(
    const cricket::VideoFormat& desired, cricket::VideoFormat* best_format) {
  if (!best_format) {
    return false;
  }

  // RemoteVideoCapturer does not support capability enumeration.
  // Use the desired format as the best format.
  best_format->width = desired.width;
  best_format->height = desired.height;
  best_format->fourcc = cricket::FOURCC_I420;
  best_format->interval = desired.interval;
  return true;
}

bool RemoteVideoCapturer::IsScreencast() const {
  // TODO(ronghuawu): what about remote screencast stream.
  return false;
}

}  // namespace webrtc
