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

namespace webrtc {

int WebRtcCreateWindow(void** os_specific_handle, int winNum, 
                       int width, int height) {
  return 0;
}

void SetWindowPos(void** os_specific_handle, int x, int y, 
                  int width, int height, bool onTop) {
  // Do nothing.
}

} // namespace webrtc