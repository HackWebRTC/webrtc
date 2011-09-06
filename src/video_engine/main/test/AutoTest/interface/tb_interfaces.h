/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_INTERFACES_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_INTERFACES_H_

#include "vie_autotest_defines.h"

#include "common_types.h"
#include "vie_base.h"
#include "vie_capture.h"
#include "vie_codec.h"
#include "vie_image_process.h"
#include "vie_network.h"
#include "vie_render.h"
#include "vie_rtp_rtcp.h"
#include "vie_encryption.h"
#include "vie_defines.h"

class tbInterfaces
{
public:
    tbInterfaces(const char* testName, int& nrOfErrors);

    ~tbInterfaces(void);
    webrtc::VideoEngine* ptrViE;
    webrtc::ViEBase* ptrViEBase;
    webrtc::ViECapture* ptrViECapture;
    webrtc::ViERender* ptrViERender;
    webrtc::ViERTP_RTCP* ptrViERtpRtcp;
    webrtc::ViECodec* ptrViECodec;
    webrtc::ViENetwork* ptrViENetwork;
    webrtc::ViEImageProcess* ptrViEImageProcess;
    webrtc::ViEEncryption* ptrViEEncryption;

    int LastError()
    {
        return ptrViEBase->LastError();
    }

private:
    int& numberOfErrors;
};

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_TB_INTERFACES_H_
