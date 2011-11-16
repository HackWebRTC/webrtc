/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "Renderer.h"

#define nil NULL
#define NO false

namespace webrtc {

jobject Renderer::g_renderWindow = NULL;

int WebRtcCreateWindow(void** os_specific_handle, int /*winNum*/, 
                       int /*width*/, int /*height*/) {
  // jobject is a pointer type, hence a pointer to it is a
  // pointer-to-pointer, which makes it castable from void**.
  jobject* window = (jobject*)os_specific_handle;
  *window = Renderer::g_renderWindow;
  return 0;
}

void SetWindowPos(void ** /*os_specific_handle*/, int /*x*/, int /*y*/, 
                  int /*width*/, int /*height*/, bool /*onTop*/) {
  // Do nothing.
}

void Renderer::SetRenderWindow(jobject renderWindow) {
  __android_log_print(ANDROID_LOG_DEBUG, 
                      "VideoCaptureModule -testAPI", 
                      "Renderer::SetRenderWindow");
  g_renderWindow=renderWindow;
}

} // namespace webrtc