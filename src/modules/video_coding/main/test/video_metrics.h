/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_VIDEO_METRICS_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_VIDEO_METRICS_H_


// PSNR & SSIM calculations
WebRtc_Word32
PsnrFromFiles(const WebRtc_Word8 *refFileName,
        const WebRtc_Word8 *testFileName, WebRtc_Word32 width,
        WebRtc_Word32 height, double *YPsnrPtr);

WebRtc_Word32
SsimFromFiles(const WebRtc_Word8 *refFileName,
        const WebRtc_Word8 *testFileName, WebRtc_Word32 width,
        WebRtc_Word32 height, double *YSsimPtr);


#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_VIDEO_METRICS_H_
