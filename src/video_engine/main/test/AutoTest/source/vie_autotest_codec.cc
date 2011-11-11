/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// vie_autotest_codec.cc
//

#include "vie_autotest.h"

#include "common_types.h"
#include "engine_configurations.h"
#include "vie_autotest_defines.h"
#include "tb_capture_device.h"
#include "vie_fake_camera.h"
#include "tb_I420_codec.h"
#include "tb_interfaces.h"
#include "tb_video_channel.h"
#include "vie_base.h"
#include "vie_capture.h"
#include "vie_codec.h"
#include "vie_network.h"
#include "vie_render.h"
#include "vie_rtp_rtcp.h"
#include "voe_base.h"

const int kDoNotForceResolution = 0;

class ViEAutotestCodecObserver: public webrtc::ViEEncoderObserver,
                                public webrtc::ViEDecoderObserver
{
public:
    int incomingCodecCalled;
    int incomingRatecalled;
    int outgoingRatecalled;

    unsigned char lastPayloadType;
    unsigned short lastWidth;
    unsigned short lastHeight;

    unsigned int lastOutgoingFramerate;
    unsigned int lastOutgoingBitrate;
    unsigned int lastIncomingFramerate;
    unsigned int lastIncomingBitrate;

    webrtc::VideoCodec incomingCodec;

    ViEAutotestCodecObserver()
    {
        incomingCodecCalled = 0;
        incomingRatecalled = 0;
        outgoingRatecalled = 0;
        lastPayloadType = 0;
        lastWidth = 0;
        lastHeight = 0;
        lastOutgoingFramerate = 0;
        lastOutgoingBitrate = 0;
        lastIncomingFramerate = 0;
        lastIncomingBitrate = 0;
        memset(&incomingCodec, 0, sizeof(incomingCodec));
    }
    virtual void IncomingCodecChanged(const int videoChannel,
                                      const webrtc::VideoCodec& videoCodec)
    {
        incomingCodecCalled++;
        lastPayloadType = videoCodec.plType;
        lastWidth = videoCodec.width;
        lastHeight = videoCodec.height;

        memcpy(&incomingCodec, &videoCodec, sizeof(videoCodec));
    }

    virtual void IncomingRate(const int videoChannel,
                              const unsigned int framerate,
                              const unsigned int bitrate)
    {
        incomingRatecalled++;
        lastIncomingFramerate += framerate;
        lastIncomingBitrate += bitrate;
    }

    virtual void OutgoingRate(const int videoChannel,
                              const unsigned int framerate,
                              const unsigned int bitrate)
    {
        outgoingRatecalled++;
        lastOutgoingFramerate += framerate;
        lastOutgoingBitrate += bitrate;
    }

    virtual void RequestNewKeyFrame(const int videoChannel)
    {
    }
};

class ViEAutoTestEffectFilter: public webrtc::ViEEffectFilter
{
public:
    int numFrames;
    ViEAutoTestEffectFilter()
    {
        numFrames = 0;
    }
    ~ViEAutoTestEffectFilter()
    {
    }

    virtual int Transform(int size, unsigned char* frameBuffer,
                          unsigned int timeStamp90KHz, unsigned int width,
                          unsigned int height)
    {
        numFrames++;
        return 0;
    }
};

// Helper functions

// Finds a codec in the codec list. Returns 0 on success, nonzero otherwise.
// The resulting codec is filled into result on success but is zeroed out
// on failure.
int FindSpecificCodec(webrtc::VideoCodecType of_type,
                      webrtc::ViECodec* codec_interface,
                      webrtc::VideoCodec* result) {

  memset(result, 1, sizeof(webrtc::VideoCodec));

  for (int i = 0; i < codec_interface->NumberOfCodecs(); i++) {
    webrtc::VideoCodec codec;
    if (codec_interface->GetCodec(i, codec) != 0) {
      return -1;
    }
    if (codec.codecType == of_type) {
      // Done
      *result = codec;
      return 0;
    }
  }
  // Didn't find it
  return -1;
}

// Sets the video codec's resolution info to something suitable based on each
// codec's quirks, except if the forced* variables are != kDoNotForceResolution.
void SetSuitableResolution(webrtc::VideoCodec* video_codec,
                           int forced_codec_width,
                           int forced_codec_height) {
  if (forced_codec_width != kDoNotForceResolution &&
      forced_codec_height != kDoNotForceResolution) {
    video_codec->width = forced_codec_width;
    video_codec->height = forced_codec_height;
  } else if (video_codec->codecType == webrtc::kVideoCodecI420) {
    // I420 is very bandwidth heavy, so limit it here
    video_codec->width = 176;
    video_codec->height = 144;
  } else if (video_codec->codecType != webrtc::kVideoCodecH263) {
    // Otherwise go with 640x480, except for H263 which can do whatever
    // it pleases.
    video_codec->width = 640;
    video_codec->height = 480;
  }
}

// Tests that a codec actually renders frames by registering a basic
// render effect filter on the codec and then running it. This test is
// quite lenient on the number of frames that get rendered, so it should not
// be seen as a end-user-visible quality measure - it is more a sanity check
// that the codec at least gets some frames through.
void TestCodecImageProcess(webrtc::VideoCodec video_codec,
                           webrtc::ViECodec* codec_interface,
                           int video_channel,
                           int* number_of_errors,
                           webrtc::ViEImageProcess* image_process) {

  int error = codec_interface->SetSendCodec(video_channel, video_codec);
  *number_of_errors += ViETest::TestError(error == 0,
                                          "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  ViEAutoTestEffectFilter frame_counter;
  error = image_process->RegisterRenderEffectFilter(video_channel,
                                                    frame_counter);
  *number_of_errors += ViETest::TestError(error == 0,
                                          "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  AutoTestSleep (KAutoTestSleepTimeMs);

  int max_number_of_rendered_frames = video_codec.maxFramerate *
      KAutoTestSleepTimeMs / 1000;

  if (video_codec.codecType == webrtc::kVideoCodecI420) {
    // Due to that I420 needs a huge bandwidth, rate control can set
    // frame rate very low. This happen since we use the same channel
    // as we just tested with vp8.
    *number_of_errors += ViETest::TestError(frame_counter.numFrames > 0,
                                            "ERROR: %s at line %d",
                                            __FUNCTION__, __LINE__);
  } else {
#ifdef WEBRTC_ANDROID
    // Special case to get the autotest to pass on some slow devices
    *number_of_errors +=
        ViETest::TestError(frameCounter.numFrames
                           > max_number_of_rendered_frames / 6,
                           "ERROR: %s at line %d",
                           __FUNCTION__, __LINE__);
#else
    *number_of_errors += ViETest::TestError(frame_counter.numFrames
                                            > max_number_of_rendered_frames / 4,
                                            "ERROR: %s at line %d",
                                            __FUNCTION__, __LINE__);
#endif
  }
  error = image_process->DeregisterRenderEffectFilter(video_channel);
  *number_of_errors += ViETest::TestError(error == 0,
                                          "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
}

void SetSendCodec(webrtc::VideoCodecType of_type,
                  webrtc::ViECodec* codec_interface,
                  int video_channel,
                  int* number_of_errors,
                  int forced_codec_width,
                  int forced_codec_height) {
  webrtc::VideoCodec codec;
  int error = FindSpecificCodec(of_type, codec_interface, &codec);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);

  SetSuitableResolution(&codec, forced_codec_width, forced_codec_height);

  error = codec_interface->SetSendCodec(video_channel, codec);
  *number_of_errors += ViETest::TestError(error == 0,
                                          "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
}

// Test switching from i420 to VP8 as send codec and make sure that
// the codec observer gets called after the switch.
void TestCodecCallbacks(webrtc::ViEBase *& base_interface,
                        webrtc::ViECodec *codec_interface,
                        int video_channel,
                        int* number_of_errors,
                        int forced_codec_width,
                        int forced_codec_height) {

  // Set I420 as send codec so we don't make any assumptions about what
  // we currently have as send codec:
  SetSendCodec(webrtc::kVideoCodecI420, codec_interface, video_channel,
               number_of_errors, forced_codec_width, forced_codec_height);

  // Register the observer:
  ViEAutotestCodecObserver codec_observer;
  int error = codec_interface->RegisterEncoderObserver(video_channel,
                                                       codec_observer);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  error = codec_interface->RegisterDecoderObserver(video_channel,
                                                   codec_observer);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);

  // Make the switch
  ViETest::Log("Testing codec callbacks...");

  SetSendCodec(webrtc::kVideoCodecVP8, codec_interface, video_channel,
               number_of_errors, forced_codec_width, forced_codec_height);

  AutoTestSleep (KAutoTestSleepTimeMs);

  // Verify that we got the right codec
  *number_of_errors += ViETest::TestError(
      codec_observer.incomingCodec.codecType == webrtc::kVideoCodecVP8,
      "ERROR: %s at line %d", __FUNCTION__, __LINE__);

  // Clean up
  error = codec_interface->DeregisterEncoderObserver(video_channel);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  error = codec_interface->DeregisterDecoderObserver(video_channel);
  *number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
  *number_of_errors += ViETest::TestError(
      codec_observer.incomingCodecCalled > 0,
      "ERROR: %s at line %d", __FUNCTION__, __LINE__);
  *number_of_errors += ViETest::TestError(
      codec_observer.incomingRatecalled > 0,
      "ERROR: %s at line %d", __FUNCTION__, __LINE__);
  *number_of_errors += ViETest::TestError(
      codec_observer.outgoingRatecalled > 0,
      "ERROR: %s at line %d", __FUNCTION__, __LINE__);
}

void ViEAutoTest::ViEAutomatedCodecStandardTest(
    const std::string& i420_video_file,
    int width,
    int height,
    ViEToFileRenderer* local_file_renderer,
    ViEToFileRenderer* remote_file_renderer) {
  int ignored = 0;

  tbInterfaces interfaces = tbInterfaces("ViECodecAutomatedStandardTest",
                                         ignored);

  ViEFakeCamera fake_camera(interfaces.ptrViECapture);
  if (!fake_camera.StartCameraInNewThread(i420_video_file, width, height)) {
    // No point in continuing if we have no proper video source
    ViETest::TestError(false, "ERROR: %s at line %d: "
                       "Could not open input video %s: aborting test...",
                       __FUNCTION__, __LINE__, i420_video_file.c_str());
    return;
  }

  // Force the codec resolution to what our input video is so we can make
  // comparisons later. Our comparison algorithms wouldn't like scaling.
  RunCodecTestInternal(interfaces, ignored, fake_camera.capture_id(),
                       width, height, local_file_renderer,
                       remote_file_renderer);

  fake_camera.StopCamera();
}

int ViEAutoTest::ViECodecStandardTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViECodec Standard Test\n");

    int number_of_errors = 0;

    tbInterfaces interfaces = tbInterfaces("ViECodecStandardTest",
                                           number_of_errors);

    tbCaptureDevice capture_device =
        tbCaptureDevice(interfaces, number_of_errors);
    RunCodecTestInternal(interfaces, number_of_errors, capture_device.captureId,
                         kDoNotForceResolution, kDoNotForceResolution,
                         NULL, NULL);

    if (number_of_errors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViECodec Standard Test FAILED!");
        ViETest::Log(" Number of errors: %d", number_of_errors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return number_of_errors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViECodec Standard Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViECodecExtendedTest()
{
    int error = 0;
    int numberOfErrors = 0;

    {
        ViETest::Log(" ");
        ViETest::Log("========================================");
        ViETest::Log(" ViECodec Extended Test\n");

        numberOfErrors = ViECodecAPITest();
        numberOfErrors += ViECodecStandardTest();
        numberOfErrors += ViECodecExternalCodecTest();

        tbInterfaces interfaces = tbInterfaces("ViECodecExtendedTest",
                                               numberOfErrors);
        webrtc::ViEBase* ptrViEBase = interfaces.ptrViEBase;
        webrtc::ViECapture* ptrViECapture = interfaces.ptrViECapture;
        webrtc::ViERender* ptrViERender = interfaces.ptrViERender;
        webrtc::ViECodec* ptrViECodec = interfaces.ptrViECodec;
        webrtc::ViERTP_RTCP* ptrViERtpRtcp = interfaces.ptrViERtpRtcp;
        webrtc::ViENetwork* ptrViENetwork = interfaces.ptrViENetwork;

        tbCaptureDevice captureDevice = tbCaptureDevice(interfaces,
                                                        numberOfErrors);
        int captureId = captureDevice.captureId;

        int videoChannel = -1;
        error = ptrViEBase->CreateChannel(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->ConnectCaptureDevice(captureId, videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetRTCPStatus(videoChannel,
                                             webrtc::kRtcpCompound_RFC4585);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetKeyFrameRequestMethod(
            videoChannel, webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetTMMBRStatus(videoChannel, true);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->AddRenderer(captureId, _window1, 0, 0.0, 0.0,
                                          1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->AddRenderer(videoChannel, _window2, 1, 0.0, 0.0,
                                          1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StartRender(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StartRender(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        webrtc::VideoCodec videoCodec;
        memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
        for (int idx = 0; idx < ptrViECodec->NumberOfCodecs(); idx++)
        {
            error = ptrViECodec->GetCodec(idx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            if (videoCodec.codecType != webrtc::kVideoCodecH263
                && videoCodec.codecType != webrtc::kVideoCodecI420)
            {
                videoCodec.width = 640;
                videoCodec.height = 480;
            }
            error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
        }

        const char* ipAddress = "127.0.0.1";
        const unsigned short rtpPort = 6000;
        error = ptrViENetwork->SetLocalReceiver(videoChannel, rtpPort);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StartReceive(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViENetwork->SetSendDestination(videoChannel, ipAddress,
                                                  rtpPort);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StartSend(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        //
        // Codec specific tests
        //
        memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
        error = ptrViEBase->StopSend(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViEAutotestCodecObserver codecObserver;
        error = ptrViECodec->RegisterEncoderObserver(videoChannel,
                                                     codecObserver);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ptrViECodec->RegisterDecoderObserver(videoChannel,
                                                     codecObserver);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        //***************************************************************
        //	Testing finished. Tear down Video Engine
        //***************************************************************

        error = ptrViEBase->StopReceive(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StopSend(videoChannel); // Already stopped
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StopRender(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->RemoveRenderer(captureId);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->RemoveRenderer(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->DeleteChannel(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }

    //
    // Default channel
    //
    {
        // Create VIE
        tbInterfaces ViE("ViECodecExtendedTest2", numberOfErrors);
        // Create a capture device
        tbCaptureDevice tbCapture(ViE, numberOfErrors);

        // Create channel 1
        int videoChannel1 = -1;
        error = ViE.ptrViEBase->CreateChannel(videoChannel1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        unsigned short rtpPort1 = 12000;
        error = ViE.ptrViENetwork->SetLocalReceiver(videoChannel1, rtpPort1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendDestination(videoChannel1,
                                                      "127.0.0.1", rtpPort1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        tbCapture.ConnectTo(videoChannel1);

        error = ViE.ptrViERtpRtcp->SetKeyFrameRequestMethod(
            videoChannel1, webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViERender->AddRenderer(videoChannel1, _window1, 0, 0.0,
                                              0.0, 1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERender->StartRender(videoChannel1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViEAutotestCodecObserver codecObserver1;
        error = ViE.ptrViECodec->RegisterEncoderObserver(videoChannel1,
                                                         codecObserver1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViECodec->RegisterDecoderObserver(videoChannel1,
                                                         codecObserver1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Set Send codec
        unsigned short codecWidth = 176;
        unsigned short codecHeight = 144;
        bool codecSet = false;
        webrtc::VideoCodec videoCodec;
        for (int idx = 0; idx < ViE.ptrViECodec->NumberOfCodecs(); idx++)
        {
            error = ViE.ptrViECodec->GetCodec(idx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            error = ViE.ptrViECodec->SetReceiveCodec(videoChannel1, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            if (videoCodec.codecType == webrtc::kVideoCodecVP8)
            {
                videoCodec.width = codecWidth;
                videoCodec.height = codecHeight;
                videoCodec.startBitrate = 200;
                videoCodec.maxBitrate = 300;
                error
                    = ViE.ptrViECodec->SetSendCodec(videoChannel1, videoCodec);
                numberOfErrors += ViETest::TestError(error == 0,
                                                     "ERROR: %s at line %d",
                                                     __FUNCTION__, __LINE__);
                codecSet = true;
                break;
            }
        }
        numberOfErrors += ViETest::TestError(codecSet, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViEBase->StartSend(videoChannel1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StartReceive(videoChannel1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Create channel 2, based on channel 1
        int videoChannel2 = -1;
        error = ViE.ptrViEBase->CreateChannel(videoChannel2, videoChannel1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(videoChannel1 != videoChannel2,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViERtpRtcp->SetKeyFrameRequestMethod(
            videoChannel2, webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Prepare receive codecs
        for (int idx = 0; idx < ViE.ptrViECodec->NumberOfCodecs(); idx++)
        {
            error = ViE.ptrViECodec->GetCodec(idx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            error = ViE.ptrViECodec->SetReceiveCodec(videoChannel2, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
        }

        ViEAutotestCodecObserver codecObserver2;
        error = ViE.ptrViECodec->RegisterDecoderObserver(videoChannel2,
                                                         codecObserver2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViERender->AddRenderer(videoChannel2, _window2, 0, 0.0,
                                              0.0, 1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERender->StartRender(videoChannel2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        unsigned short rtpPort2 = 13000;
        error = ViE.ptrViENetwork->SetLocalReceiver(videoChannel2, rtpPort2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendDestination(videoChannel2,
                                                      "127.0.0.1", rtpPort2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViEBase->StartReceive(videoChannel2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StartSend(videoChannel2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViETest::Log("\nTest using one encoder on several channels");
        ViETest::Log(
                     "Channel 1 is rendered in Window1, channel 2 in Window 2."
                     "\nSending VP8 on both channels");

        AutoTestSleep(KAutoTestSleepTimeMs);

        // Check that we received H.263 on both channels
        numberOfErrors += ViETest::TestError(
            codecObserver1.incomingCodec.codecType == webrtc::kVideoCodecVP8
            && codecObserver1.incomingCodec.width == 176,
            "ERROR: %s at line %d", __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(
            codecObserver2.incomingCodec.codecType ==
                webrtc::kVideoCodecVP8
            && codecObserver2.incomingCodec.width == 176,
            "ERROR: %s at line %d", __FUNCTION__, __LINE__);

        // Delete the first channel and keep the second
        error = ViE.ptrViEBase->DeleteChannel(videoChannel1);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViETest::Log("Channel 1 deleted, you should only see video in Window "
                     "2");

        AutoTestSleep(KAutoTestSleepTimeMs);

        // Create another channel
        int videoChannel3 = -1;
        error = ViE.ptrViEBase->CreateChannel(videoChannel3, videoChannel2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(videoChannel3 != videoChannel2,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViERtpRtcp->SetKeyFrameRequestMethod(
            videoChannel3, webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Prepare receive codecs
        for (int idx = 0; idx < ViE.ptrViECodec->NumberOfCodecs(); idx++)
        {
            error = ViE.ptrViECodec->GetCodec(idx, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            error = ViE.ptrViECodec->SetReceiveCodec(videoChannel3, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
        }

        ViEAutotestCodecObserver codecObserver3;
        error = ViE.ptrViECodec->RegisterDecoderObserver(videoChannel3,
                                                         codecObserver3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViERender->AddRenderer(videoChannel3, _window1, 0, 0.0,
                                              0.0, 1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERender->StartRender(videoChannel3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        unsigned short rtpPort3 = 14000;
        error = ViE.ptrViENetwork->SetLocalReceiver(videoChannel3, rtpPort3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendDestination(videoChannel3,
                                                      "127.0.0.1", rtpPort3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViEBase->StartReceive(videoChannel3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StartSend(videoChannel3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViEBase->DeleteChannel(videoChannel2);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViETest::Log("A third channel created and rendered in Window 1,\n"
            "channel 2 is deleted and you should only see video in Window 1");

        AutoTestSleep(KAutoTestSleepTimeMs);

        error = ViE.ptrViEBase->DeleteChannel(videoChannel3);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }

    // SetKeyFrameRequestCallbackStatus
    // Check callback

    // SetPacketLossBitrateAdaptationStatus
    // Check bitrate changes/doesn't change

    // GetAvailableBandwidth

    // SendKeyFrame

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViECodec Extended Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViECodec Extended Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViECodecAPITest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViECodec API Test\n");

    // ***************************************************************
    // Begin create/initialize WebRTC Video Engine for testing
    // ***************************************************************

    int error = 0;
    int numberOfErrors = 0;

    webrtc::VideoEngine* ptrViE = NULL;
    ptrViE = webrtc::VideoEngine::Create();
    numberOfErrors += ViETest::TestError(ptrViE != NULL, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    webrtc::ViEBase* ptrViEBase = webrtc::ViEBase::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViEBase != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrViEBase->Init();
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    int videoChannel = -1;
    error = ptrViEBase->CreateChannel(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    webrtc::ViECodec* ptrViECodec = webrtc::ViECodec::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViECodec != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************

    //
    // SendCodec
    //
    webrtc::VideoCodec videoCodec;
    memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));

    const int numberOfCodecs = ptrViECodec->NumberOfCodecs();
    numberOfErrors += ViETest::TestError(numberOfCodecs > 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    SetSendCodec(webrtc::kVideoCodecVP8, ptrViECodec, videoChannel,
                 &numberOfErrors, kDoNotForceResolution, kDoNotForceResolution);

    memset(&videoCodec, 0, sizeof(videoCodec));
    error = ptrViECodec->GetSendCodec(videoChannel, videoCodec);
    assert(videoCodec.codecType == webrtc::kVideoCodecVP8);

    SetSendCodec(webrtc::kVideoCodecI420, ptrViECodec, videoChannel,
                 &numberOfErrors, kDoNotForceResolution, kDoNotForceResolution);
    memset(&videoCodec, 0, sizeof(videoCodec));
    error = ptrViECodec->GetSendCodec(videoChannel, videoCodec);
    assert(videoCodec.codecType == webrtc::kVideoCodecI420);

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************

    error = ptrViEBase->DeleteChannel(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    int remainingInterfaces = 0;
    remainingInterfaces = ptrViECodec->Release();
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

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViECodec API Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViECodec API Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
#include "vie_external_codec.h"
#endif
int ViEAutoTest::ViECodecExternalCodecTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEExternalCodec Test\n");

    // ***************************************************************
    // Begin create/initialize WebRTC Video Engine for testing
    // ***************************************************************


    // ***************************************************************
    // Engine ready. Begin testing class
    // ***************************************************************

#ifdef WEBRTC_VIDEO_ENGINE_EXTERNAL_CODEC_API
    int numberOfErrors=0;
    {
        int error=0;
        tbInterfaces ViE("ViEExternalCodec", numberOfErrors);
        tbCaptureDevice captureDevice(ViE, numberOfErrors);
        tbVideoChannel channel(ViE, numberOfErrors, webrtc::kVideoCodecI420,
                               352,288,30,(352*288*3*8*30)/(2*1000));

        captureDevice.ConnectTo(channel.videoChannel);

        error = ViE.ptrViERender->AddRenderer(channel.videoChannel, _window1, 0,
                                              0.0, 0.0, 1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERender->StartRender(channel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        channel.StartReceive();
        channel.StartSend();

        ViETest::Log("Using internal I420 codec");
        AutoTestSleep(KAutoTestSleepTimeMs/2);

        ViEExternalCodec* ptrViEExtCodec =
            ViEExternalCodec::GetInterface(ViE.ptrViE);
        numberOfErrors += ViETest::TestError(ptrViEExtCodec != NULL,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        webrtc::VideoCodec codecStruct;

        error=ViE.ptrViECodec->GetSendCodec(channel.videoChannel,codecStruct);
        numberOfErrors += ViETest::TestError(ptrViEExtCodec != NULL,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Use external encoder instead
        {
            tbI420Encoder extEncoder;

            // Test to register on wrong channel
            error = ptrViEExtCodec->RegisterExternalSendCodec(
                channel.videoChannel+5,codecStruct.plType,&extEncoder);
            numberOfErrors += ViETest::TestError(error == -1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                ViE.LastError() == kViECodecInvalidArgument,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);

            error = ptrViEExtCodec->RegisterExternalSendCodec(
                channel.videoChannel,codecStruct.plType,&extEncoder);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // Use new external encoder
            error = ViE.ptrViECodec->SetSendCodec(channel.videoChannel,
                                                  codecStruct);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            tbI420Decoder extDecoder;
            error = ptrViEExtCodec->RegisterExternalReceiveCodec(
                channel.videoChannel,codecStruct.plType,&extDecoder);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            error = ViE.ptrViECodec->SetReceiveCodec(channel.videoChannel,
                                                     codecStruct);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            ViETest::Log("Using external I420 codec");
            AutoTestSleep(KAutoTestSleepTimeMs);

            // Test to deregister on wrong channel
            error = ptrViEExtCodec->DeRegisterExternalSendCodec(
                channel.videoChannel+5,codecStruct.plType);
            numberOfErrors += ViETest::TestError(error == -1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                ViE.LastError() == kViECodecInvalidArgument,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);

            // Test to deregister wrong payload type.
            error = ptrViEExtCodec->DeRegisterExternalSendCodec(
                channel.videoChannel,codecStruct.plType-1);
            numberOfErrors += ViETest::TestError(error == -1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // Deregister external send codec
            error = ptrViEExtCodec->DeRegisterExternalSendCodec(
                channel.videoChannel,codecStruct.plType);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            error = ptrViEExtCodec->DeRegisterExternalReceiveCodec(
                channel.videoChannel,codecStruct.plType);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // Verify that the encoder and decoder has been used
            tbI420Encoder::FunctionCalls encodeCalls =
                extEncoder.GetFunctionCalls();
            numberOfErrors += ViETest::TestError(encodeCalls.InitEncode == 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.Release == 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.Encode > 30,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                encodeCalls.RegisterEncodeCompleteCallback ==1,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.SetRates > 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.SetPacketLoss > 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            tbI420Decoder::FunctionCalls decodeCalls =
                extDecoder.GetFunctionCalls();
            numberOfErrors += ViETest::TestError(decodeCalls.InitDecode == 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(decodeCalls.Release == 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(decodeCalls.Decode > 30,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                decodeCalls.RegisterDecodeCompleteCallback ==1,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);

            ViETest::Log("Changing payload type Using external I420 codec");

            codecStruct.plType=codecStruct.plType-1;
            error = ptrViEExtCodec->RegisterExternalReceiveCodec(
                channel.videoChannel, codecStruct.plType, &extDecoder);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            error = ViE.ptrViECodec->SetReceiveCodec(channel.videoChannel,
                                                     codecStruct);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            error = ptrViEExtCodec->RegisterExternalSendCodec(
                channel.videoChannel, codecStruct.plType, &extEncoder);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // Use new external encoder
            error = ViE.ptrViECodec->SetSendCodec(channel.videoChannel,
                                                  codecStruct);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            AutoTestSleep(KAutoTestSleepTimeMs/2);

            //***************************************************************
            //	Testing finished. Tear down Video Engine
            //***************************************************************


            error = ptrViEExtCodec->DeRegisterExternalSendCodec(
                channel.videoChannel,codecStruct.plType);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            error = ptrViEExtCodec->DeRegisterExternalReceiveCodec(
                channel.videoChannel,codecStruct.plType);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // Verify that the encoder and decoder has been used
            encodeCalls = extEncoder.GetFunctionCalls();
            numberOfErrors += ViETest::TestError(encodeCalls.InitEncode == 2,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.Release == 2,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.Encode > 30,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                encodeCalls.RegisterEncodeCompleteCallback == 2,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.SetRates > 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(encodeCalls.SetPacketLoss > 1,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            decodeCalls = extDecoder.GetFunctionCalls();
            numberOfErrors += ViETest::TestError(decodeCalls.InitDecode == 2,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(decodeCalls.Release == 2,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(decodeCalls.Decode > 30,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                decodeCalls.RegisterDecodeCompleteCallback == 2,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);

            int remainingInterfaces = ptrViEExtCodec->Release();
            numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
        } // tbI420Encoder and extDecoder goes out of scope

        ViETest::Log("Using internal I420 codec");
        AutoTestSleep(KAutoTestSleepTimeMs/2);

    }
    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViEExternalCodec Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViEExternalCodec Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;

#else
    ViETest::Log(" ViEExternalCodec not enabled\n");
    return 0;
#endif
}

void ViEAutoTest::RunCodecTestInternal(
    const tbInterfaces& interfaces,
    int & number_of_errors,
    int capture_id,
    int forced_codec_width,
    int forced_codec_height,
    ViEToFileRenderer* local_file_renderer,
    ViEToFileRenderer* remote_file_renderer) {

  webrtc::VideoEngine *video_engine_interface = interfaces.ptrViE;
  webrtc::ViEBase *base_interface = interfaces.ptrViEBase;
  webrtc::ViECapture *capture_interface = interfaces.ptrViECapture;
  webrtc::ViERender *render_interface = interfaces.ptrViERender;
  webrtc::ViECodec *codec_interface = interfaces.ptrViECodec;
  webrtc::ViERTP_RTCP *rtcp_interface = interfaces.ptrViERtpRtcp;
  webrtc::ViENetwork *network_interface = interfaces.ptrViENetwork;
  int video_channel = -1;

  int error = base_interface->CreateChannel(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = capture_interface->ConnectCaptureDevice(capture_id, video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = rtcp_interface->SetRTCPStatus(video_channel,
                                        webrtc::kRtcpCompound_RFC4585);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = rtcp_interface->
      SetKeyFrameRequestMethod(video_channel,
                               webrtc::kViEKeyFrameRequestPliRtcp);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = rtcp_interface->SetTMMBRStatus(video_channel, true);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  if (local_file_renderer && remote_file_renderer) {
    RenderToFile(render_interface, capture_id, local_file_renderer);
    RenderToFile(render_interface, video_channel, remote_file_renderer);
  } else {
    RenderInWindow(render_interface, &number_of_errors,
                   capture_id, _window1, 0);
    RenderInWindow(render_interface, &number_of_errors,
                   video_channel, _window2, 1);
  }

  // ***************************************************************
  // Engine ready. Begin testing class
  // ***************************************************************
  webrtc::VideoCodec video_codec;
  webrtc::VideoCodec vp8_codec;
  memset(&video_codec, 0, sizeof (webrtc::VideoCodec));
  memset(&vp8_codec,   0, sizeof (webrtc::VideoCodec));

  // Set up all receive codecs. This sets up a mapping in the codec interface
  // which makes it able to recognize all receive codecs based on payload type.
  for (int idx = 0; idx < codec_interface->NumberOfCodecs(); idx++) {
    error = codec_interface->GetCodec(idx, video_codec);
    number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
    SetSuitableResolution(&video_codec,
                          forced_codec_width,
                          forced_codec_height);

    error = codec_interface->SetReceiveCodec(video_channel, video_codec);
    number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
  }
  const char *ip_address = "127.0.0.1";
  const unsigned short rtp_port = 6000;
  error = network_interface->SetLocalReceiver(video_channel, rtp_port);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = base_interface->StartReceive(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = network_interface->SetSendDestination(video_channel, ip_address,
                                                rtp_port);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = base_interface->StartSend(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  // Run all found codecs
  webrtc::ViEImageProcess *image_process =
      webrtc::ViEImageProcess::GetInterface(video_engine_interface);
  number_of_errors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

  ViETest::Log("Loop through all codecs for %d seconds",
               KAutoTestSleepTimeMs / 1000);
  for (int i = 0; i < codec_interface->NumberOfCodecs(); i++) {
    error = codec_interface->GetCodec(i, video_codec);
    number_of_errors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);

    if (video_codec.codecType == webrtc::kVideoCodecMPEG4 ||
        video_codec.codecType == webrtc::kVideoCodecRED ||
        video_codec.codecType == webrtc::kVideoCodecULPFEC) {
      ViETest::Log("\t %d. %s not tested", i, video_codec.plName);
    } else {
      ViETest::Log("\t %d. %s", i, video_codec.plName);
      SetSuitableResolution(&video_codec,
                            forced_codec_width, forced_codec_height);
      TestCodecImageProcess(video_codec, codec_interface, video_channel,
                            &number_of_errors, image_process);
    }
  }
  image_process->Release();

  TestCodecCallbacks(base_interface, codec_interface, video_channel,
                     &number_of_errors, forced_codec_width,
                     forced_codec_height);

  ViETest::Log("Done!");

  // ***************************************************************
  // Testing finished. Tear down Video Engine
  // ***************************************************************
  error = base_interface->StopSend(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = base_interface->StopReceive(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = render_interface->StopRender(capture_id);
    number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
  error = render_interface->StopRender(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = render_interface->RemoveRenderer(capture_id);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = render_interface->RemoveRenderer(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = capture_interface->DisconnectCaptureDevice(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  error = base_interface->DeleteChannel(video_channel);
  number_of_errors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
}
