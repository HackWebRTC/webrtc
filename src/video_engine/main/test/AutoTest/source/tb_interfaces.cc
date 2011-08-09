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

tbInterfaces::tbInterfaces(const char* testName, int& nrOfErrors) :
    numberOfErrors(nrOfErrors)
{
    char traceFile[256] = "";
    char traceFileEnc[256] = "";

#ifdef WEBRTC_ANDROID
    strcat(traceFile,"/sdcard/");
#endif
    strcat(traceFile, testName);
    strcat(traceFileEnc, traceFile);
    strcat(traceFileEnc, "_encrypted");

    ViETest::Log("Creating ViE Interfaces for test %s\n", testName);

    ptrViE = webrtc::VideoEngine::Create();
    numberOfErrors += ViETest::TestError(ptrViE != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    int error = ptrViE->SetTraceFile(traceFile);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ptrViE->SetTraceFilter(webrtc::kTraceAll);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ptrViEBase = webrtc::ViEBase::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViEBase != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrViEBase->Init();
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ptrViECapture = webrtc::ViECapture::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViECapture != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    ptrViERtpRtcp = webrtc::ViERTP_RTCP::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViERtpRtcp != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    ptrViERender = webrtc::ViERender::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViERender != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    ptrViECodec = webrtc::ViECodec::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViECodec != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    ptrViENetwork = webrtc::ViENetwork::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViENetwork != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    ptrViEImageProcess = webrtc::ViEImageProcess::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViEImageProcess != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    ptrViEEncryption = webrtc::ViEEncryption::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViEEncryption != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);
}

tbInterfaces::~tbInterfaces(void)
{
    int numberOfErrors = 0;
    int remainingInterfaces = 0;

    remainingInterfaces = ptrViEEncryption->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViEImageProcess->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViECodec->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViECapture->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViERender->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViERtpRtcp->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViENetwork->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    remainingInterfaces = ptrViEBase->Release();
    numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    bool deleted = webrtc::VideoEngine::Delete(ptrViE);
    numberOfErrors += ViETest::TestError(deleted == true,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

}
