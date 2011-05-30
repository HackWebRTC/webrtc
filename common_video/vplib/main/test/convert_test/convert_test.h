/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_COMMON_VIDEO_VPLIB_TEST_CONVERT_TEST_H
#define WEBRTC_COMMON_VIDEO_VPLIB_TEST_CONVERT_TEST_H

#include "vplib.h"


void ToFile(WebRtc_UWord8 *buf, WebRtc_Word32 length, WebRtc_Word32 num);
WebRtc_Word32
PSNRfromFiles(const WebRtc_Word8 *refFileName, const WebRtc_Word8 *testFileName, WebRtc_Word32 width, 
              WebRtc_Word32 height, WebRtc_Word32 numberOfFrames, double *YPSNRptr);

#endif
