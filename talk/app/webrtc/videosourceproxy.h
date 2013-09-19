/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#ifndef TALK_APP_WEBRTC_VIDEOSOURCEPROXY_H_
#define TALK_APP_WEBRTC_VIDEOSOURCEPROXY_H_

#include "talk/app/webrtc/proxy.h"
#include "talk/app/webrtc/videosourceinterface.h"

namespace webrtc {

// VideoSourceProxy makes sure the real VideoSourceInterface implementation is
// destroyed on the signaling thread and marshals all method calls to the
// signaling thread.
BEGIN_PROXY_MAP(VideoSource)
  PROXY_CONSTMETHOD0(SourceState, state)
  PROXY_METHOD0(cricket::VideoCapturer*, GetVideoCapturer)
  PROXY_METHOD1(void, AddSink, cricket::VideoRenderer*)
  PROXY_METHOD1(void, RemoveSink, cricket::VideoRenderer*)
  PROXY_CONSTMETHOD0(const cricket::VideoOptions*, options)
  PROXY_METHOD0(cricket::VideoRenderer*, FrameInput)
  PROXY_METHOD1(void, RegisterObserver, ObserverInterface*)
  PROXY_METHOD1(void, UnregisterObserver, ObserverInterface*)
END_PROXY()

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_VIDEOSOURCEPROXY_H_
