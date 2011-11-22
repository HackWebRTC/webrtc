/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_CAPTURE_DEVICE_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_CAPTURE_DEVICE_H_

#include "tb_interfaces.h"
#include "video_capture_factory.h"

class TbCaptureDevice
{
public:
    TbCaptureDevice(TbInterfaces& Engine, int& nrOfErrors);
    ~TbCaptureDevice(void);

    int captureId;
    void ConnectTo(int videoChannel);
    void Disconnect(int videoChannel);
private:
    int& numberOfErrors;
    TbInterfaces& ViE;
    webrtc::VideoCaptureModule* vcpm_;
};

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_CAPTURE_DEVICE_H_
