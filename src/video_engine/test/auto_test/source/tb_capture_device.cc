/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "tb_capture_device.h"

TbCaptureDevice::TbCaptureDevice(TbInterfaces& Engine) :
    captureId(-1),
    ViE(Engine),
    vcpm_(NULL)
{
    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 256;
    char deviceName[KMaxDeviceNameLength];
    memset(deviceName, 0, KMaxDeviceNameLength);
    char uniqueId[KMaxUniqueIdLength];
    memset(uniqueId, 0, KMaxUniqueIdLength);

    bool captureDeviceSet = false;

    webrtc::VideoCaptureModule::DeviceInfo* devInfo =
        webrtc::VideoCaptureFactory::CreateDeviceInfo(0);
    for (size_t captureIdx = 0;
        captureIdx < devInfo->NumberOfDevices();
        captureIdx++)
    {
        EXPECT_EQ(0, devInfo->GetDeviceName(captureIdx, deviceName,
                                            KMaxDeviceNameLength, uniqueId,
                                            KMaxUniqueIdLength));

        vcpm_ = webrtc::VideoCaptureFactory::Create(
            captureIdx, uniqueId);
        if (vcpm_ == NULL)  // Failed to open this device. Try next.
        {
            continue;
        }
        vcpm_->AddRef();

        int error = ViE.capture->AllocateCaptureDevice(*vcpm_, captureId);
        if (error == 0)
        {
            ViETest::Log("Using capture device: %s, captureId: %d", deviceName,
                         captureId);
            captureDeviceSet = true;
            break;
        }
    }
    delete devInfo;
    EXPECT_TRUE(captureDeviceSet);
    if (!captureDeviceSet) {
        return;
    }

    ViETest::Log("Starting capture device %s with captureId %d\n", deviceName,
                 captureId);
    EXPECT_EQ(0, ViE.capture->StartCapture(captureId));
}

TbCaptureDevice::~TbCaptureDevice(void)
{
    ViETest::Log("Stopping capture device with id %d\n", captureId);
    EXPECT_EQ(0, ViE.capture->StopCapture(captureId));
    EXPECT_EQ(0, ViE.capture->ReleaseCaptureDevice(captureId));
    vcpm_->Release();
}

void TbCaptureDevice::ConnectTo(int videoChannel)
{
    EXPECT_EQ(0, ViE.capture->ConnectCaptureDevice(captureId, videoChannel));
}

void TbCaptureDevice::Disconnect(int videoChannel)
{
    EXPECT_EQ(0, ViE.capture->DisconnectCaptureDevice(videoChannel));
}
