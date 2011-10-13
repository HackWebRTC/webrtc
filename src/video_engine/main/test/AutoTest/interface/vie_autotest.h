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
// vie_autotest.h
//

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_H_

#include "common_types.h"

#include "voe_base.h"
#include "voe_codec.h"
#include "voe_hardware.h"
#include "voe_audio_processing.h"

#include "vie_base.h"
#include "vie_capture.h"
#include "vie_codec.h"
#include "vie_file.h"
#include "vie_network.h"
#include "vie_render.h"
#include "vie_rtp_rtcp.h"
#include "vie_defines.h"
#include "vie_errors.h"
#include "video_render_defines.h"

#ifndef WEBRTC_ANDROID
#include <string>
#endif

class ViEAutoTest
{
public:
    ViEAutoTest(void* window1, void* window2);
    ~ViEAutoTest();

    int ViEStandardTest();
    int ViEExtendedTest();
    int ViEAPITest();
    int ViELoopbackCall();
    int ViESimulcastCall();

    // custom call and helper functions
    int ViECustomCall();

    // general settings functions
    bool GetVideoDevice(webrtc::ViEBase* ptrViEBase,
                        webrtc::ViECapture* ptrViECapture,
                        char* captureDeviceName, char* captureDeviceUniqueId);
    bool GetIPAddress(char* IP);
#ifndef WEBRTC_ANDROID
    bool ValidateIP(std::string iStr);
#endif
    void PrintCallInformation(char* IP, char* videoCaptureDeviceName,
                              char* videoCaptureUniqueId,
                              webrtc::VideoCodec videoCodec, int videoTxPort,
                              int videoRxPort, char* audioCaptureDeviceName,
                              char* audioPlaybackDeviceName,
                              webrtc::CodecInst audioCodec, int audioTxPort,
                              int audioRxPort);

    // video settings functions
    bool GetVideoPorts(int* txPort, int* rxPort);
    bool GetVideoCodecType(webrtc::ViECodec* ptrViECodec,
                           webrtc::VideoCodec& videoCodec);
    bool GetVideoCodecResolution(webrtc::ViECodec* ptrViECodec,
                           webrtc::VideoCodec& videoCodec);
    bool GetVideoCodecSize(webrtc::ViECodec* ptrViECodec,
                           webrtc::VideoCodec& videoCodec);
    bool GetVideoCodecBitrate(webrtc::ViECodec* ptrViECodec,
                           webrtc::VideoCodec& videoCodec);

    // audio settings functions
    bool GetAudioDevices(webrtc::VoEBase* ptrVEBase,
                         webrtc::VoEHardware* ptrVEHardware,
                         char* recordingDeviceName, int& recordingDeviceIndex,
                         char* playbackDeviceName, int& playbackDeviceIndex);
    bool GetAudioDevices(webrtc::VoEBase* ptrVEBase,
                         webrtc::VoEHardware* ptrVEHardware,
                         int& recordingDeviceIndex, int& playbackDeviceIndex);
    bool GetAudioPorts(int* txPort, int* rxPort);
    bool GetAudioCodec(webrtc::VoECodec* ptrVeCodec,
                       webrtc::CodecInst& audioCodec);

    // vie_autotest_base.cc
    int ViEBaseStandardTest();
    int ViEBaseExtendedTest();
    int ViEBaseAPITest();

    // vie_autotest_capture.cc
    int ViECaptureStandardTest();
    int ViECaptureExtendedTest();
    int ViECaptureAPITest();
    int ViECaptureExternalCaptureTest();

    // vie_autotest_codec.cc
    int ViECodecStandardTest();
    int ViECodecExtendedTest();
    int ViECodecExternalCodecTest();
    int ViECodecAPITest();

    // vie_autotest_encryption.cc
    int ViEEncryptionStandardTest();
    int ViEEncryptionExtendedTest();
    int ViEEncryptionAPITest();

    // vie_autotest_file.ccs
    int ViEFileStandardTest();
    int ViEFileExtendedTest();
    int ViEFileAPITest();

    // vie_autotest_image_process.cc
    int ViEImageProcessStandardTest();
    int ViEImageProcessExtendedTest();
    int ViEImageProcessAPITest();

    // vie_autotest_network.cc
    int ViENetworkStandardTest();
    int ViENetworkExtendedTest();
    int ViENetworkAPITest();

    // vie_autotest_render.cc
    int ViERenderStandardTest();
    int ViERenderExtendedTest();
    int ViERenderAPITest();

    // vie_autotest_rtp_rtcp.cc
    int ViERtpRtcpStandardTest();
    int ViERtpRtcpExtendedTest();
    int ViERtpRtcpAPITest();

private:
    void PrintAudioCodec(const webrtc::CodecInst audioCodec);
    void PrintVideoCodec(const webrtc::VideoCodec videoCodec);

    void* _window1;
    void* _window2;

    webrtc::VideoRenderType _renderType;
    webrtc::VideoRender* _vrm1;
    webrtc::VideoRender* _vrm2;
};

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_H_
