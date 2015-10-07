/*
 * libjingle
 * Copyright 2013 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/app/webrtc/remotevideocapturer.h"

#include "talk/media/base/videoframe.h"
#include "webrtc/base/logging.h"

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
