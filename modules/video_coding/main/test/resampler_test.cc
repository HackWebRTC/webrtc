/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "ResamplerTest.h"
#include "video_coding.h"
#include "tick_time.h"
#include "../source/event.h"
#include "VCMSpatialResampler.h"

#include <iostream>
#include <sstream>

using namespace webrtc;

int ResamplerTest()
{
    VideoCodingModule* vcm = VideoCodingModule::Create(1);
    class ResamplerTest test(vcm);
    int ret = test.Perform();
    VideoCodingModule::Destroy(vcm);

    return ret;
}

ResamplerTest::ResamplerTest(VideoCodingModule* vcm):
_width(0),
_height(0),
_timeStamp(0),
_lengthSourceFrame(0),
_vcmMacrosTests(0),
_vcmMacrosErrors(0),
_vcm(vcm)
{
    //
}
ResamplerTest::~ResamplerTest()
{
    //
}
void
ResamplerTest::Setup()
{
    _inname= "../../../../../codecs_video/testFiles/foreman.yuv";
    _width = 352;
    _height = 288;
    _frameRate = 30;
    _lengthSourceFrame  = 3*_width*_height/2;
    _encodedName = "../ResamplerTest_encoded.yuv";

    if ((_sourceFile = fopen(_inname.c_str(), "rb")) == NULL)
    {
        printf("Cannot read file %s.\n", _inname.c_str());
        exit(1);
    }

    if ((_encodedFile = fopen(_encodedName.c_str(), "wb")) == NULL)
    {
        printf("Cannot write encoded file.\n");
        exit(1);
    }

    return;
}

WebRtc_Word32 ResamplerTest::Perform()
{
    // Make sure this test isn't executed without simulated clocks
#if !defined(TICK_TIME_DEBUG) || !defined(EVENT_DEBUG)
    return -1;
#endif

    // Setup test
    Setup();

    ResamplerStandAloneTest();

    ResamplerVCMTest();

    TearDown();
    return 0;
}

void
ResamplerTest::ResamplerVCMTest()
{
    // Create the input frame and read a frame from file
    VideoFrame sourceFrame;
    sourceFrame.VerifyAndAllocate(_lengthSourceFrame);
    fread(sourceFrame.Buffer(), 1, _lengthSourceFrame, _sourceFile);
    sourceFrame.SetLength(_lengthSourceFrame);
    sourceFrame.SetHeight(_height);
    sourceFrame.SetWidth(_width);

    TEST_EXIT_ON_FAIL(_vcm->InitializeReceiver() == VCM_OK);
    TEST_EXIT_ON_FAIL(_vcm->InitializeSender() == VCM_OK);

    TEST_EXIT_ON_FAIL(_vcm->EnableInputFrameInterpolation(true) == VCM_OK);

    TestSizeVCM(sourceFrame, 128, 80);       // Cut, decimation 1x, interpolate
    TestSizeVCM(sourceFrame, 352/2, 288/2);  // Even decimation
    TestSizeVCM(sourceFrame, 352, 288);      // No resampling
    TestSizeVCM(sourceFrame, 2*352, 2*288);  // Upsampling 2x
    TestSizeVCM(sourceFrame, 400, 256);      // Upsampling 1.5x and cut
    TestSizeVCM(sourceFrame, 960, 720);      // Upsampling 3.5x and cut

    TEST_EXIT_ON_FAIL(_vcm->EnableInputFrameInterpolation(false) == VCM_OK);

    TestSizeVCM(sourceFrame, 320, 240);      // Cropped
    TestSizeVCM(sourceFrame, 1280, 720);     // Padded
}

void
ResamplerTest::TestSizeVCM(VideoFrame& sourceFrame, WebRtc_UWord32 targetWidth, WebRtc_UWord32 targetHeight)
{
    assert(false);
    /*
    std::ostringstream filename;
    filename << "../VCM_Resampler_" << targetWidth << "x" << targetHeight << "_30Hz_P420.yuv";
    std::cout << "Watch " << filename.str() << " and verify that it is okay." << std::endl;
    FILE* decodedFile = fopen(filename.str().c_str(), "wb");

    _timeStamp += (WebRtc_UWord32)(9e4 / _frameRate);
    sourceFrame.SetTimeStamp(_timeStamp);

    VCMDecodeCompleteCallback decodeCallback(decodedFile);
    VCMEncodeCompleteCallback encodeCompleteCallback(_encodedFile);
    TEST_EXIT_ON_FAIL(_vcm->RegisterReceiveCallback(&decodeCallback) == VCM_OK);
    TEST_EXIT_ON_FAIL(_vcm->RegisterTransportCallback(&encodeCompleteCallback) == VCM_OK);
    encodeCompleteCallback.RegisterReceiverVCM(_vcm);
    encodeCompleteCallback.SetCodecType(webrtc::VideoCodecVP8);

    RegisterCodec(targetWidth, targetHeight);
    encodeCompleteCallback.SetFrameDimensions(targetWidth, targetHeight);
    TEST(_vcm->AddVideoFrame(sourceFrame) == VCM_OK);
    TEST(_vcm->Decode() == VCM_OK);

    fclose(decodedFile);
    */
}

void
ResamplerTest::RegisterCodec(WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    // Register codecs
    assert(false);
    /*
    VideoCodec codec;
    VideoCodingModule::Codec(webrtc::kVideoCodecVP8, &codec);
    codec.width = static_cast<WebRtc_Word16>(width);
    codec.height = static_cast<WebRtc_Word16>(height);
    TEST(_vcm->RegisterSendCodec(&codec, 1, 1440) == VCM_OK);
    TEST(_vcm->RegisterReceiveCodec(&codec, 1) == VCM_OK);
    TEST(_vcm->SetChannelParameters(2000, 0, 0) == VCM_OK);
    */
}

WebRtc_Word32
ResamplerTest::ResamplerStandAloneTest()
{
    // Create the input frame and read a frame from file
    VideoFrame sourceFrame;
    sourceFrame.VerifyAndAllocate(_lengthSourceFrame);
    fread(sourceFrame.Buffer(), 1, _lengthSourceFrame, _sourceFile);
    sourceFrame.SetLength(_lengthSourceFrame);
    sourceFrame.SetHeight(_height);
    sourceFrame.SetWidth(_width);

    TestSize(sourceFrame, 100, 50);         // Cut, decimation 1x, interpolate
    TestSize(sourceFrame, 352/2, 288/2);    // Even decimation
    TestSize(sourceFrame, 352, 288);        // No resampling
    TestSize(sourceFrame, 2*352, 2*288);    // Even upsampling
    TestSize(sourceFrame, 400, 256);        // Upsampling 1.5x and cut
    TestSize(sourceFrame, 960, 720);        // Upsampling 3.5x and cut
    TestSize(sourceFrame, 1280, 720);       // Upsampling 4x and cut

    sourceFrame.Free();
    return 0;
}

void
ResamplerTest::TestSize(VideoFrame& sourceFrame, WebRtc_UWord32 targetWidth, WebRtc_UWord32 targetHeight)
{
    VCMSimpleSpatialResampler resampler;
    VideoFrame outFrame;
    std::ostringstream filename;
    filename << "../Resampler_" << targetWidth << "x" << targetHeight << "_30Hz_P420.yuv";
    std::cout << "Watch " << filename.str() << " and verify that it is okay." << std::endl;
    FILE* standAloneFile = fopen(filename.str().c_str(), "wb");
    //resampler.EnableUpSampling(true);
    resampler.EnableInterpolation(true);
    TEST(resampler.SetTargetFrameSize(targetWidth, targetHeight) == VCM_OK);
    TEST(resampler.ResampleFrame(sourceFrame, outFrame) == VCM_OK);
    TEST(outFrame.Buffer() != NULL);
    TEST(outFrame.Length() == (targetWidth * targetHeight * 3 / 2));

    // Write to file for visual inspection
    fwrite(outFrame.Buffer(), 1, outFrame.Length(), standAloneFile);

    outFrame.Free();
    fclose(standAloneFile);
}

void
ResamplerTest::Print()
{
    printf("\nVCM Resampler Test: \n\n%i tests completed\n", _vcmMacrosTests);
    if (_vcmMacrosErrors > 0)
    {
        printf("%i FAILED\n\n", _vcmMacrosErrors);
    }
    else
    {
        printf("ALL PASSED\n\n");
    }
}

void
ResamplerTest::TearDown()
{
    fclose(_sourceFile);
    fclose(_encodedFile);
    return;
}

void
ResamplerTest::IncrementDebugClock(float frameRate)
{
    for (int t= 0; t < 1000/frameRate; t++)
    {
        VCMTickTime::IncrementDebugClock();
    }
    return;
}

