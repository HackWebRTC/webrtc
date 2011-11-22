/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "tb_interfaces.h"

TbInterfaces::TbInterfaces(const char* testName, int& nrOfErrors) :
    numberOfErrors(nrOfErrors)
{
    char traceFile[256] = "";

#ifdef WEBRTC_ANDROID
    strcat(traceFile,"/sdcard/");
#endif
    strcat(traceFile, testName);
    strcat(traceFile, "_trace.txt");

    ViETest::Log("Creating ViE Interfaces for test %s\n", testName);

    video_engine = webrtc::VideoEngine::Create();
    numberOfErrors += ViETest::TestError(video_engine != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    int error = video_engine->SetTraceFile(traceFile);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = video_engine->SetTraceFilter(webrtc::kTraceAll);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    base = webrtc::ViEBase::GetInterface(video_engine);
    numberOfErrors += ViETest::TestError(base != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = base->Init();
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    capture = webrtc::ViECapture::GetInterface(video_engine);
    numberOfErrors += ViETest::TestError(capture != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    rtp_rtcp = webrtc::ViERTP_RTCP::GetInterface(video_engine);
    numberOfErrors += ViETest::TestError(rtp_rtcp != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    render = webrtc::ViERender::GetInterface(video_engine);
    numberOfErrors += ViETest::TestError(render != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    codec = webrtc::ViECodec::GetInterface(video_engine);
    numberOfErrors += ViETest::TestError(codec != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    network = webrtc::ViENetwork::GetInterface(video_engine);
    numberOfErrors += ViETest::TestError(network != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    image_process = webrtc::ViEImageProcess::GetInterface(video_engine);
    numberOfErrors += ViETest::TestError(image_process != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    encryption = webrtc::ViEEncryption::GetInterface(video_engine);
    numberOfErrors += ViETest::TestError(encryption != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);
}

TbInterfaces::~TbInterfaces(void)
{
    int numberOfErrors = 0;
    int remainingInterfaces = 0;

    remainingInterfaces = encryption->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = image_process->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = codec->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = capture->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = render->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = rtp_rtcp->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = network->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = base->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    bool deleted = webrtc::VideoEngine::Delete(video_engine);
    numberOfErrors += ViETest::TestError(deleted == true,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

}
