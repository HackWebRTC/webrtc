/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * vie_autotest_capture.cc
 */

#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "engine_configurations.h"

#include "common_types.h"
#include "voe_base.h"
#include "vie_base.h"
#include "vie_capture.h"
#include "vie_codec.h"
#include "vie_network.h"
#include "vie_render.h"
#include "vie_rtp_rtcp.h"
#include "tick_util.h"

#include "tb_interfaces.h"
#include "tb_video_channel.h"
#include "video_capture.h"

class CaptureObserver: public ViECaptureObserver
{
public:
    CaptureObserver() :
        _brightness(Normal), _alarm(AlarmCleared), _frameRate(0)
    {}

    virtual void BrightnessAlarm(const int captureId,
                                 const Brightness brightness)
    {
        _brightness = brightness;
        switch (brightness)
        {
            case Normal:
                ViETest::Log("  BrightnessAlarm Normal");
                break;
            case Bright:
                ViETest::Log("  BrightnessAlarm Bright");
                break;
            case Dark:
                ViETest::Log("  BrightnessAlarm Dark");
                break;
            default:
                assert(!"Unknown brightness alarm");
        }
    }

    virtual void CapturedFrameRate(const int captureId,
                                   const unsigned char frameRate)
    {
        ViETest::Log("  CapturedFrameRate %u", frameRate);
        _frameRate = frameRate;
    }

    virtual void NoPictureAlarm(const int captureId, const CaptureAlarm alarm)
    {
        _alarm = alarm;
        if (alarm == AlarmRaised)
        {
            ViETest::Log("NoPictureAlarm CARaised.");
        }
        else
        {
            ViETest::Log("NoPictureAlarm CACleared.");
        }
    }

    Brightness _brightness;
    CaptureAlarm _alarm;
    unsigned char _frameRate;
};

class CaptureEffectFilter: public ViEEffectFilter
{
public:
    CaptureEffectFilter(int reqWidth, int reqHeight, int& numberOfErrors) :
        _numberOfCapturedFrames(0),
        _reqWidth(reqWidth),
        _reqHeight(reqHeight),
        _numberOfErrors(numberOfErrors)
    {
    }
    // Implements ViEEffectFilter
    virtual int Transform(int size, unsigned char* frameBuffer,
                          unsigned int timeStamp90KHz, unsigned int width,
                          unsigned int height)
    {
        _numberOfErrors += ViETest::TestError(
            frameBuffer != 0
            && width == _reqWidth
            && height == _reqHeight,
            "ERROR: %s at line %d", __FUNCTION__, __LINE__);
        ++_numberOfCapturedFrames;
        return 0;
    }

    int _numberOfCapturedFrames;

protected:
    int _reqWidth;
    int _reqHeight;
    int& _numberOfErrors;
};

int ViEAutoTest::ViECaptureStandardTest()
{
    int numberOfErrors = 0;
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViECapture StandardTest Test\n");

    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************


    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************


    int error = 0;
    tbInterfaces ViE("WebRTCViECapture_Standard", numberOfErrors);

    VideoCaptureModule::DeviceInfo* devInfo =
        VideoCaptureModule::CreateDeviceInfo(0);

    int numberOfCaptureDevices = devInfo->NumberOfDevices();
    ViETest::Log("Number of capture devices %d", numberOfCaptureDevices);
    numberOfErrors += ViETest::TestError(numberOfCaptureDevices > 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    int captureDeviceId[10];
    VideoCaptureModule* vcpms[10];
    memset(vcpms, 0, sizeof(vcpms));

    //Check capabilities
    for (int deviceIndex = 0;
         deviceIndex < numberOfCaptureDevices;
         ++deviceIndex)
    {
        WebRtc_UWord8 deviceName[128];
        WebRtc_UWord8 deviceUniqueName[512];

        error = devInfo->GetDeviceName(deviceIndex, deviceName,
                                       sizeof(deviceName), deviceUniqueName,
                                       sizeof(deviceUniqueName));
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        ViETest::Log("Found capture device %s\nUnique name %s", deviceName,
                     deviceUniqueName);

// not supported on MAC (is part of capture capabilites
#if !defined(WEBRTC_LINUX) && !defined(WEBRTC_MAC_INTEL)
        error = ViE.ptrViECapture->ShowCaptureSettingsDialogBox(
            (char*) deviceUniqueName,
            (unsigned int) (strlen((char*) deviceUniqueName)),
            "WebRTCViECapture StandardTest");
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
#endif

#if !defined(WEBRTC_MAC_INTEL) // these functions will return -1
        unsigned int numberOfCapabilities =
            devInfo->NumberOfCapabilities(deviceUniqueName);
        numberOfErrors += ViETest::TestError(numberOfCapabilities > 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        for (unsigned int capIndex = 0;
             capIndex < numberOfCapabilities;
             ++capIndex)
        {
            VideoCaptureCapability capability;
            error = devInfo->GetCapability(deviceUniqueName, capIndex,
                                           capability);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            ViETest::Log("Capture capability %d (of %u)", capIndex + 1,
                         numberOfCapabilities);
            ViETest::Log("witdh %d, height %d, frame rate %d",
                         capability.width, capability.height, capability.maxFPS);
            ViETest::Log("expected delay %d, color type %d, encoding %d",
                         capability.expectedCaptureDelay, capability.rawType,
                         capability.codecType);
            numberOfErrors += ViETest::TestError(
                capability.width > 0
                && capability.height > 0
                && capability.maxFPS >= 0
                && capability.expectedCaptureDelay > 0,
                "ERROR: %s at line %d", __FUNCTION__, __LINE__);
        }
#endif
    }
#if !defined(WEBRTC_MAC_INTEL)
    // "capture capability functios" are not supported on WEBRTC_MAC_INTEL
    //Check allocation. Try to allocate them all after each other.

    for (int deviceIndex = 0;
         deviceIndex < numberOfCaptureDevices;
         ++deviceIndex)
    {
        WebRtc_UWord8 deviceName[128];
        WebRtc_UWord8 deviceUniqueName[512];

        error = devInfo->GetDeviceName(deviceIndex, deviceName,
                                       sizeof(deviceName), deviceUniqueName,
                                       sizeof(deviceUniqueName));
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        VideoCaptureModule* vcpm = VideoCaptureModule::Create(deviceIndex,
                                                              deviceUniqueName);
        numberOfErrors += ViETest::TestError(vcpm != NULL,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        vcpms[deviceIndex] = vcpm;

        error = ViE.ptrViECapture->AllocateCaptureDevice(
                                            *vcpm, captureDeviceId[deviceIndex]);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        VideoCaptureCapability capability;
        error = devInfo->GetCapability(deviceUniqueName, 0, capability);

        // Test that the camera select the closest capability to the selected
        // widht and height.
        CaptureEffectFilter filter(capability.width, capability.height,
                                   numberOfErrors);
        error = ViE.ptrViEImageProcess->RegisterCaptureEffectFilter(
                                        captureDeviceId[deviceIndex], filter);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViETest::Log("Testing Device %s capability width %d  height %d",
                     deviceUniqueName, capability.width, capability.height);
        capability.height = capability.height - 2;
        capability.width = capability.width - 2;

        CaptureCapability vieCapability;
        vieCapability.width = capability.width;
        vieCapability.height = capability.height;
        vieCapability.codecType = capability.codecType;
        vieCapability.maxFPS = capability.maxFPS;
        vieCapability.rawType = capability.rawType;

        error = ViE.ptrViECapture->StartCapture(captureDeviceId[deviceIndex],
                                                vieCapability);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        TickTime startTime = TickTime::Now();

        while (filter._numberOfCapturedFrames < 10
               && (TickTime::Now() - startTime).Milliseconds() < 10000)
        {
            AutoTestSleep(100);
        }
        numberOfErrors += ViETest::TestError(filter._numberOfCapturedFrames
            >= 10, "ERROR: %s at line %d", __FUNCTION__, __LINE__);
        error = ViE.ptrViEImageProcess->DeregisterCaptureEffectFilter(
            captureDeviceId[deviceIndex]);

#ifdef WEBRTC_ANDROID // Can only allocate one camera at the time on Android
        error = ViE.ptrViECapture->StopCapture(captureDeviceId[deviceIndex]);
        numberOfErrors += ViETest::TestError(error==0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViECapture->ReleaseCaptureDevice(
            captureDeviceId[deviceIndex]);
        numberOfErrors += ViETest::TestError(error==0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
#endif
    }

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************


    // stop all started capture devices
    for (int deviceIndex = 0; deviceIndex < numberOfCaptureDevices; ++deviceIndex)
    {
        error = ViE.ptrViECapture->StopCapture(captureDeviceId[deviceIndex]);
#ifdef WEBRTC_ANDROID
        // Camera already stoped on Android since we can only allocate one
        // camera at the time.
        numberOfErrors += ViETest::TestError(error==-1, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
#else
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
#endif

        error = ViE.ptrViECapture->ReleaseCaptureDevice(
            captureDeviceId[deviceIndex]);
#ifdef WEBRTC_ANDROID
        // Camera already stoped on Android since we can only allocate one
        // camera at the time
        numberOfErrors += ViETest::TestError(error==-1, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
#else
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
#endif
        VideoCaptureModule::Destroy(vcpms[deviceIndex]);
    }
#endif
    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViECapture Standard Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }
    VideoCaptureModule::DestroyDeviceInfo(devInfo);

    ViETest::Log(" ");
    ViETest::Log(" ViECapture Standard Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");

    return 0;
}

int ViEAutoTest::ViECaptureExtendedTest()
{

    // Test
    int numberOfErrors = 0;
    numberOfErrors += ViECaptureStandardTest();
    numberOfErrors += ViECaptureAPITest();
    numberOfErrors += ViECaptureExternalCaptureTest();

    return 0;
}

int ViEAutoTest::ViECaptureAPITest()
{
    int numberOfErrors = 0;
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViECapture API Test\n");

    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************


    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************

    int error = 0;
    tbInterfaces ViE("WebRTCViECapture_API", numberOfErrors);

    ViE.ptrViECapture->NumberOfCaptureDevices();

    WebRtc_UWord8 deviceName[128];
    WebRtc_UWord8 deviceUniqueName[512];
    int captureId = 0;
    int dummy = 0;

    VideoCaptureModule::DeviceInfo* devInfo =
        VideoCaptureModule::CreateDeviceInfo(0);
    numberOfErrors += ViETest::TestError(devInfo != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    // Get the first capture device
    error = devInfo->GetDeviceName(0, deviceName, sizeof(deviceName),
                                   deviceUniqueName, sizeof(deviceUniqueName));
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    VideoCaptureModule* vcpm = VideoCaptureModule::Create(0, deviceUniqueName);
    numberOfErrors += ViETest::TestError(vcpm != NULL, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Allocate capture device
    error = ViE.ptrViECapture->AllocateCaptureDevice(*vcpm, captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Start the capture device
    error = ViE.ptrViECapture->StartCapture(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Start again. Should fail
    error = ViE.ptrViECapture->StartCapture(captureId);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(ViE.LastError()
        == kViECaptureDeviceAlreadyStarted, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Start invalid capture device
    error = ViE.ptrViECapture->StartCapture(captureId + 1);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(ViE.LastError()
        == kViECaptureDeviceDoesnNotExist, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Stop invalide capture device
    error = ViE.ptrViECapture->StopCapture(captureId + 1);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(ViE.LastError()
        == kViECaptureDeviceDoesnNotExist, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Stop the capture device
    error = ViE.ptrViECapture->StopCapture(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Stop the capture device again
    error = ViE.ptrViECapture->StopCapture(captureId);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(ViE.LastError()
        == kViECaptureDeviceNotStarted, "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    // Connect to invalid channel
    error = ViE.ptrViECapture->ConnectCaptureDevice(captureId, 0);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(ViE.LastError()
        == kViECaptureDeviceInvalidChannelId, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    tbVideoChannel channel(ViE, numberOfErrors);

    // Connect invalid captureId
    error = ViE.ptrViECapture->ConnectCaptureDevice(captureId + 1,
                                                    channel.videoChannel);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(ViE.LastError()
                                         == kViECaptureDeviceDoesnNotExist,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    // Connect the capture device to the channel
    error = ViE.ptrViECapture->ConnectCaptureDevice(captureId,
                                                    channel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Connect the channel again
    error = ViE.ptrViECapture->ConnectCaptureDevice(captureId,
                                                    channel.videoChannel);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(ViE.LastError()
        == kViECaptureDeviceAlreadyConnected, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Start the capture device
    error = ViE.ptrViECapture->StartCapture(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Release invalid capture device    
    error = ViE.ptrViECapture->ReleaseCaptureDevice(captureId + 1);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(ViE.LastError()
        == kViECaptureDeviceDoesnNotExist, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Release the capture device
    error = ViE.ptrViECapture->ReleaseCaptureDevice(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Release the capture device again
    error = ViE.ptrViECapture->ReleaseCaptureDevice(captureId);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(ViE.LastError()
        == kViECaptureDeviceDoesnNotExist, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Test GetOrientation
    VideoCaptureRotation orientation;
    WebRtc_UWord8 dummy_name[5];
    error = devInfo->GetOrientation(dummy_name, orientation);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //Test SetRotation
    error = ViE.ptrViECapture->SetRotateCapturedFrames(captureId,
                                                       RotateCapturedFrame_90);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(ViE.LastError()
        == kViECaptureDeviceDoesnNotExist, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Allocate capture device
    error = ViE.ptrViECapture->AllocateCaptureDevice(*vcpm, captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViECapture->SetRotateCapturedFrames(captureId,
                                                       RotateCapturedFrame_0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViECapture->SetRotateCapturedFrames(captureId,
                                                       RotateCapturedFrame_90);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViECapture->SetRotateCapturedFrames(captureId,
                                                       RotateCapturedFrame_180);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViECapture->SetRotateCapturedFrames(captureId,
                                                       RotateCapturedFrame_270);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Release the capture device
    error = ViE.ptrViECapture->ReleaseCaptureDevice(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    VideoCaptureModule::DestroyDeviceInfo(devInfo);
    VideoCaptureModule::Destroy(vcpm);

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************


    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR WebRTCViECapture API Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" WebRTCViECapture API Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");

    return numberOfErrors;
}

int ViEAutoTest::ViECaptureExternalCaptureTest()
{
    int numberOfErrors = 0;
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" WebRTCViECapture External Capture Test\n");

    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************


    int error = 0;
    tbInterfaces ViE("WebRTCViECapture_ExternalCapture", numberOfErrors);
    tbVideoChannel channel(ViE, numberOfErrors);
    channel.StartReceive();
    channel.StartSend();

    VideoCaptureExternal* externalCapture;
    int captureId = 0;

    // Allocate the external capture device
    VideoCaptureModule* vcpm = VideoCaptureModule::Create(0, externalCapture);
    numberOfErrors += ViETest::TestError(vcpm != NULL, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViECapture->AllocateCaptureDevice(*vcpm, captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(externalCapture != 0,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    // Connect the capture device to the channel
    error = ViE.ptrViECapture->ConnectCaptureDevice(captureId,
                                                    channel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Render the local capture
    error = ViE.ptrViERender->AddRenderer(captureId, _window1, 1, 0.0, 0.0,
                                          1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Render the remote capture
    error = ViE.ptrViERender->AddRenderer(channel.videoChannel, _window2, 1,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->StartRender(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERender->StartRender(channel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Register observer
    CaptureObserver observer;
    error = ViE.ptrViECapture->RegisterObserver(captureId, observer);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Enable brighness alarm
    error = ViE.ptrViECapture->EnableBrightnessAlarm(captureId, true);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    CaptureEffectFilter effectFilter(176, 144, numberOfErrors);
    error = ViE.ptrViEImageProcess->RegisterCaptureEffectFilter(captureId,
                                                                effectFilter);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Call started

    ViETest::Log("You should see local preview from external capture\n"
                 "in window 1 and the remote video in window 2.\n");

    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************

    const unsigned int videoFrameLength = (176 * 144 * 3) / 2;
    unsigned char* videoFrame = new unsigned char[videoFrameLength];
    memset(videoFrame, 128, 176 * 144);

    // TODO: Find a file to use for testing.
    // FILE* foreman = OpenTestFile("akiyo_qcif.yuv");
    // if (foreman == NULL)
    // {
    //     ViETest::Log("Failed to open file akiyo_qcif.yuv");
    // }

    int frameCount = 0;
    VideoCaptureCapability capability;
    capability.width = 176;
    capability.height = 144;
    capability.rawType = kVideoI420;

    ViETest::Log("Testing external capturing and frame rate callbacks.");
    // TODO: Change when using a real file!
    // while (fread(videoFrame, videoFrameLength, 1, foreman) == 1)
    while (frameCount < 120)
    {

        externalCapture->IncomingFrame(
            videoFrame, videoFrameLength, capability,
            TickTime::Now().MillisecondTimestamp());
        AutoTestSleep(33);

        if (effectFilter._numberOfCapturedFrames > 2)
        {
            // make sure brigthness or no picture alarm has not been triggered
            numberOfErrors += ViETest::TestError(observer._brightness == Normal,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            numberOfErrors += ViETest::TestError(
                observer._alarm == AlarmCleared, "ERROR: %s at line %d",
                __FUNCTION__, __LINE__);
        }
        frameCount++;
    }

    // Test brightness alarm
    // Test bright image
    for (int i = 0; i < 176 * 144; ++i)
    {
        if (videoFrame[i] <= 155)
            videoFrame[i] = videoFrame[i] + 100;
        else
            videoFrame[i] = 255;
    }
    ViETest::Log("Testing Brighness alarm");
    for (int frame = 0; frame < 30; ++frame)
    {
        externalCapture->IncomingFrame(
            videoFrame, videoFrameLength, capability,
            TickTime::Now().MillisecondTimestamp());
        AutoTestSleep(33);
    }
    numberOfErrors += ViETest::TestError(observer._brightness == Bright,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    // Test Dark image
    for (int i = 0; i < 176 * 144; ++i)
    {
        videoFrame[i] = videoFrame[i] > 200 ? videoFrame[i] - 200 : 0;
    }
    for (int frame = 0; frame < 30; ++frame)
    {
        externalCapture->IncomingFrame(
            videoFrame, videoFrameLength, capability,
            TickTime::Now().MillisecondTimestamp());
        AutoTestSleep(33);
    }
    numberOfErrors += ViETest::TestError(observer._brightness == Dark,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    // Test that frames were played
    numberOfErrors += ViETest::TestError(
        effectFilter._numberOfCapturedFrames > 150,
        "ERROR: %s at line %d", __FUNCTION__, __LINE__);

    //  Test frame rate callback
    numberOfErrors += ViETest::TestError(observer._frameRate >= 29
                                         && observer._frameRate <= 30,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    // Test no picture alarm
    ViETest::Log("Testing NoPictureAlarm.");
    AutoTestSleep(1050);
    numberOfErrors += ViETest::TestError(observer._alarm == AlarmRaised,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);
    for (int frame = 0; frame < 10; ++frame)
    {
        externalCapture->IncomingFrame(
            videoFrame, videoFrameLength, capability,
            TickTime::Now().MillisecondTimestamp());
        AutoTestSleep(33);
    }
    numberOfErrors += ViETest::TestError(observer._alarm == AlarmCleared,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    delete videoFrame;

    // Release the capture device
    error = ViE.ptrViECapture->ReleaseCaptureDevice(captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Release the capture device again
    error = ViE.ptrViECapture->ReleaseCaptureDevice(captureId);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(
        ViE.LastError() == kViECaptureDeviceDoesnNotExist,
        "ERROR: %s at line %d", __FUNCTION__, __LINE__);

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR WebRTCViECapture External Capture Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" WebRTCViECapture External Capture Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");

    return numberOfErrors;
}
