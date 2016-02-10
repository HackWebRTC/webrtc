/*
 *  Copyright 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_API_VIDEOSOURCEPROXY_H_
#define WEBRTC_API_VIDEOSOURCEPROXY_H_

#include "webrtc/api/proxy.h"
#include "webrtc/api/videosourceinterface.h"

namespace webrtc {

// VideoSourceProxy makes sure the real VideoSourceInterface implementation is
// destroyed on the signaling thread and marshals all method calls to the
// signaling thread.
BEGIN_PROXY_MAP(VideoSource)
  PROXY_CONSTMETHOD0(SourceState, state)
  PROXY_CONSTMETHOD0(bool, remote)
  PROXY_METHOD0(cricket::VideoCapturer*, GetVideoCapturer)
  PROXY_METHOD0(void, Stop)
  PROXY_METHOD0(void, Restart)
  PROXY_METHOD1(void, AddSink, rtc::VideoSinkInterface<cricket::VideoFrame>*)
  PROXY_METHOD1(void, RemoveSink, rtc::VideoSinkInterface<cricket::VideoFrame>*)
  PROXY_CONSTMETHOD0(const cricket::VideoOptions*, options)
  PROXY_METHOD1(void, RegisterObserver, ObserverInterface*)
  PROXY_METHOD1(void, UnregisterObserver, ObserverInterface*)
END_PROXY()

}  // namespace webrtc

#endif  // WEBRTC_API_VIDEOSOURCEPROXY_H_
