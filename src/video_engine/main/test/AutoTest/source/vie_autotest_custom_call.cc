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
 *  vie_autotest_custom_call.cc
 *
 */

#include "vie_autotest_defines.h"
#include "vie_autotest.h"

#include <iostream>

#define VCM_RED_PAYLOAD_TYPE        96
#define VCM_ULPFEC_PAYLOAD_TYPE     97
#define DEFAULT_SEND_IP                                 "127.0.0.1"
#define DEFAULT_VIDEO_PORT                              11111
#define DEFAULT_VIDEO_CODEC                             "vp8"
#define DEFAULT_VIDEO_CODEC_WIDTH                       640
#define DEFAULT_VIDEO_CODEC_HEIGHT                      480
#define DEFAULT_VIDEO_CODEC_BITRATE                     100
#define DEFAULT_VIDEO_CODEC_MAX_BITRATE                 1000
#define DEFAULT_AUDIO_PORT                              11113
#define DEFAULT_AUDIO_CODEC                             "ISAC"
#define DEFAULT_INCOMING_FILE_NAME                      "IncomingFile.avi"
#define DEFAULT_OUTGOING_FILE_NAME                      "OutgoingFile.avi"
#define DEFAULT_VIDEO_CODEC_MAX_FRAMERATE               30

enum StatisticsType {
  kSendStatistic,
  kReceivedStatistic
};

class ViEAutotestFileObserver: public webrtc::ViEFileObserver {
 public:
  ViEAutotestFileObserver() {};
  ~ViEAutotestFileObserver() {};

  void PlayFileEnded(const WebRtc_Word32 fileId) {
    ViETest::Log("PlayFile ended");
  }
};

class ViEAutotestEncoderObserver: public webrtc::ViEEncoderObserver {
 public:
  ViEAutotestEncoderObserver() {};
  ~ViEAutotestEncoderObserver() {};

  void OutgoingRate(const int videoChannel,
                    const unsigned int framerate,
                    const unsigned int bitrate) {
    std::cout << "Send FR: " << framerate
              << " BR: " << bitrate << std::endl;
  }
};

class ViEAutotestDecoderObserver: public webrtc::ViEDecoderObserver {
 public:
  ViEAutotestDecoderObserver() {};
  ~ViEAutotestDecoderObserver() {};

  void IncomingRate(const int videoChannel,
                    const unsigned int framerate,
                    const unsigned int bitrate) {
    std::cout << "Received FR: " << framerate
              << " BR: " << bitrate << std::endl;
  }
  void IncomingCodecChanged(const int videoChannel,
                            const webrtc::VideoCodec& codec) {}
  void RequestNewKeyFrame(const int videoChannel) {
    std::cout << "Decoder requesting a new key frame." << std::endl;
  }
};

// general settings functions
bool GetVideoDevice(webrtc::ViEBase* ptrViEBase,
                    webrtc::ViECapture* ptrViECapture,
                    char* captureDeviceName, char* captureDeviceUniqueId);
bool GetIPAddress(char* IP);
#ifndef WEBRTC_ANDROID
bool ValidateIP(std::string iStr);
#endif
// Print Call information and statistics
void PrintCallInformation(char* IP, char* videoCaptureDeviceName,
                          char* videoCaptureUniqueId,
                          webrtc::VideoCodec videoCodec, int videoTxPort,
                          int videoRxPort, char* audioCaptureDeviceName,
                          char* audioPlaybackDeviceName,
                          webrtc::CodecInst audioCodec, int audioTxPort,
                          int audioRxPort);
void PrintRTCCPStatistics(webrtc::ViERTP_RTCP* ptrViERtpRtcp,
                          int videoChannel, StatisticsType statType);
void PrintRTPStatistics(webrtc::ViERTP_RTCP* ptrViERtpRtcp,
                        int videoChannel);
void PrintBandwidthUsage(webrtc::ViERTP_RTCP* ptrViERtpRtcp,
                         int videoChannel);
void PrintCodecStatistics(webrtc::ViECodec* ptrViECodec, int videoChannel,
                          StatisticsType statType);
void PrintGetDiscardedPackets(webrtc::ViECodec* ptrViECodec, int videoChannel);

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
bool GetVideoCodecMaxBitrate(webrtc::ViECodec* ptrViECodec,
                             webrtc::VideoCodec& videoCodec);
bool GetVideoCodecMaxFramerate(webrtc::ViECodec* ptrViECodec,
                               webrtc::VideoCodec& videoCodec);
bool SetVideoProtection(webrtc::ViECodec* ptrViECodec,
                        webrtc::VideoCodec& videoCodec,
                        webrtc::ViERTP_RTCP* ptrViERtpRtcp,
                        int videoChannel);

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

int ViEAutoTest::ViECustomCall()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" Enter values to use custom settings\n");

    int error = 0;
    int numberOfErrors = 0;
    std::string str;

    // VoE
    webrtc::VoiceEngine* ptrVE = webrtc::VoiceEngine::Create();
    numberOfErrors += ViETest::TestError(ptrVE != NULL, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    webrtc::VoEBase* ptrVEBase = webrtc::VoEBase::GetInterface(ptrVE);
    numberOfErrors += ViETest::TestError(ptrVEBase != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrVEBase->Init();
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    webrtc::VoECodec* ptrVECodec = webrtc::VoECodec::GetInterface(ptrVE);
    numberOfErrors += ViETest::TestError(ptrVECodec != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    webrtc::VoEHardware* ptrVEHardware =
      webrtc::VoEHardware::GetInterface(ptrVE);
    numberOfErrors += ViETest::TestError(ptrVEHardware != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    webrtc::VoEAudioProcessing* ptrVEAPM =
      webrtc::VoEAudioProcessing::GetInterface(ptrVE);
    numberOfErrors += ViETest::TestError(ptrVEAPM != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    // ViE
    webrtc::VideoEngine* ptrViE = webrtc::VideoEngine::Create();
    numberOfErrors += ViETest::TestError(ptrViE != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    webrtc::ViEBase* ptrViEBase = webrtc::ViEBase::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViEBase != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ptrViEBase->Init();
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    webrtc::ViECapture* ptrViECapture = 
      webrtc::ViECapture::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViECapture != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    webrtc::ViERender* ptrViERender = webrtc::ViERender::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViERender != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    webrtc::ViECodec* ptrViECodec = webrtc::ViECodec::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViECodec != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    webrtc::ViENetwork* ptrViENetwork =
      webrtc::ViENetwork::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViENetwork != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    webrtc::ViEFile* ptrViEFile = webrtc::ViEFile::GetInterface(ptrViE);
    numberOfErrors += ViETest::TestError(ptrViEFile != NULL,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    bool startCall = false;
    const unsigned int kMaxIPLength = 16;
    char ipAddress[kMaxIPLength] = "";
    const unsigned int KMaxUniqueIdLength = 256;
    char uniqueId[KMaxUniqueIdLength] = "";
    char deviceName[KMaxUniqueIdLength] = "";
    int videoTxPort = 0;
    int videoRxPort = 0;
    int videoChannel = -1;
    webrtc::VideoCodec videoCodec;
    char audioCaptureDeviceName[KMaxUniqueIdLength] = "";
    char audioPlaybackDeviceName[KMaxUniqueIdLength] = "";
    int audioCaptureDeviceIndex = -1;
    int audioPlaybackDeviceIndex = -1;
    int audioTxPort = 0;
    int audioRxPort = 0;
    webrtc::CodecInst audioCodec;
    int audioChannel = -1;
    int protectionMethod = 0;
    // TODO (amyfong):  Change the observers to pointers, use NULL checks to 
    // toggle between registered or deregistered
    bool isEncoderObserverRegistered = false;
    bool isDecoderObserverRegistered = false;

    while(1)
    {
        // IP
        memset(ipAddress, 0, kMaxIPLength);
        GetIPAddress(ipAddress);

        // video devices
        memset(deviceName, 0, KMaxUniqueIdLength);
        memset(uniqueId, 0, KMaxUniqueIdLength);
        GetVideoDevice(ptrViEBase, ptrViECapture, deviceName, uniqueId);

        // video ports
        videoTxPort = 0;
        videoRxPort = 0;
        GetVideoPorts(&videoTxPort, &videoRxPort);

        // video codecs
        memset((void*)&videoCodec, 0, sizeof(videoCodec));
        GetVideoCodecType(ptrViECodec, videoCodec);
        GetVideoCodecSize(ptrViECodec, videoCodec);
        GetVideoCodecBitrate(ptrViECodec, videoCodec);
        GetVideoCodecMaxBitrate(ptrViECodec, videoCodec);
        GetVideoCodecMaxFramerate(ptrViECodec, videoCodec);

        // Choose video protection mode
        std::cout << "Available Video Protection Method" << std::endl;
        std::cout << "0. None" << std::endl;
        std::cout << "1. FEC" << std::endl;
        std::cout << "2. NACK" << std::endl;
        std::cout << "3. NACK+FEC" << std::endl;
        std::cout << "Enter Video Protection Method: ";

        std::string method;
        std::getline(std::cin, method);
        protectionMethod = atoi(method.c_str());

        // audio devices
        memset(audioCaptureDeviceName, 0, KMaxUniqueIdLength);
        memset(audioPlaybackDeviceName, 0, KMaxUniqueIdLength);
        GetAudioDevices(ptrVEBase, ptrVEHardware, audioCaptureDeviceName,
                        audioCaptureDeviceIndex, audioPlaybackDeviceName,
                        audioPlaybackDeviceIndex);

        // audio port
        audioTxPort = 0;
        audioRxPort = 0;
        GetAudioPorts(&audioTxPort, &audioRxPort);

        // audio codec
        memset((void*)&audioCodec, 0, sizeof(audioCodec));
        GetAudioCodec(ptrVECodec, audioCodec);

        // start the call now
        PrintCallInformation(ipAddress, deviceName, uniqueId, videoCodec,
                             videoTxPort, videoRxPort, audioCaptureDeviceName,
                             audioPlaybackDeviceName, audioCodec, audioTxPort,
                             audioRxPort);

        std::cout << std::endl;
        std::cout << "1. Start the call" << std::endl;
        std::cout << "2. Reconfigure call settings" << std::endl;
        std::cout << "3. Go back to main menu" << std::endl;
        std::cout << "What do you want to do? Press enter for default "
                  << "(Start the call): ";

        std::getline(std::cin, str);
        int selection = 0;
        selection = atoi(str.c_str());

        if(selection == 0 || selection == 1)
        {
            startCall = true;
            break;
        }
        else if(selection == 2)
        {
            continue;
        }
        else if(selection == 3)
        {
            startCall = false;
            break;
        }
        else
        {
            // invalid selection
            std::cout << "ERROR: Code=" << error <<
                      " Invalid selection" << std::endl;
            continue;
        }
    }

    //***************************************************************
    // Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************
    if(startCall == true)
    {
        // Configure Audio first
        audioChannel = ptrVEBase->CreateChannel();
        error = ptrVEBase->SetSendDestination(audioChannel, audioTxPort,
                                              ipAddress);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEBase->SetLocalReceiver(audioChannel, audioRxPort);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEHardware->SetRecordingDevice(audioCaptureDeviceIndex);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEHardware->SetPlayoutDevice(audioPlaybackDeviceIndex);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVECodec->SetSendCodec(audioChannel, audioCodec);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEAPM->SetAgcStatus(true, webrtc::kAgcDefault);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEAPM->SetNsStatus(true, webrtc::kNsHighSuppression);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        // Configure Video now
        error = ptrViE->SetTraceFilter(webrtc::kTraceAll);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViE->SetTraceFile("ViECustomCall_trace.txt");
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->SetVoiceEngine(ptrVE);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->CreateChannel(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->ConnectAudioChannel(videoChannel, audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        int captureId = 0;
        error = ptrViECapture->AllocateCaptureDevice(uniqueId,
                                                     KMaxUniqueIdLength,
                                                     captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->ConnectCaptureDevice(captureId, videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->StartCapture(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        webrtc::ViERTP_RTCP* ptrViERtpRtcp =
          webrtc::ViERTP_RTCP::GetInterface(ptrViE);
        numberOfErrors += ViETest::TestError(ptrViE != NULL,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetRTCPStatus(videoChannel,
                                             webrtc::kRtcpCompound_RFC4585);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERtpRtcp->SetKeyFrameRequestMethod(videoChannel,
                                 webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        // Set all video protection to false initially
        // shouldn't be nessecary as ViE protection modes are all off
        // initially.  
        // TODO(amyfong):  remove the set to false and use i
        // SetVideoProtection instead
        error = ptrViERtpRtcp->SetHybridNACKFECStatus(videoChannel, false,
                                                      VCM_RED_PAYLOAD_TYPE,
                                                      VCM_ULPFEC_PAYLOAD_TYPE);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ptrViERtpRtcp->SetFECStatus(videoChannel, false,
                                            VCM_RED_PAYLOAD_TYPE,
                                            VCM_ULPFEC_PAYLOAD_TYPE);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ptrViERtpRtcp->SetNACKStatus(videoChannel, false);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        // Set video protection for FEC,  NACK or Hybrid
        // TODO(amyfong):  Use SetVideoProtection instead, need to 
        // move the set protection method after the first SetReceiveCodec
        // also should change and use videoSendCodec & videoReceiveCodec
        // instead of just videoCodec.  Helps check what exactly the call
        // setup is onces the call is up and running 
        switch (protectionMethod)
        {
            case 0: // None
              // No protection selected, all protection already at false
              break;
            case 1: // FEC only
              error = ptrViERtpRtcp->SetFECStatus(videoChannel, true,
                                                  VCM_RED_PAYLOAD_TYPE,
                                                  VCM_ULPFEC_PAYLOAD_TYPE);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
              break;
            case 2: // NACK only
              error = ptrViERtpRtcp->SetNACKStatus(videoChannel, true);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
              break;
            case 3: // Hybrid NACK and FEC
              error = ptrViERtpRtcp->SetHybridNACKFECStatus(
                  videoChannel, true, VCM_RED_PAYLOAD_TYPE,
                  VCM_ULPFEC_PAYLOAD_TYPE);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
              break;
        }

        error = ptrViERtpRtcp->SetTMMBRStatus(videoChannel, true);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->AddRenderer(captureId, _window1, 0, 0.0, 0.0,
                                          1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->AddRenderer(videoChannel, _window2, 1, 0.0, 0.0,
                                          1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViENetwork->SetSendDestination(videoChannel, ipAddress,
                                                  videoTxPort);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViENetwork->SetLocalReceiver(videoChannel, videoRxPort);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECodec->SetSendCodec(videoChannel, videoCodec);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Set receive codecs for FEC and hybrid NACK/FEC
        // TODO(amyfong):  Use SetVideoProtection instead, need to 
        // move the set protection method after the first SetReceiveCodec
        // also should change and use videoSendCodec & videoReceiveCodec
        // instead of just videoCodec.  Helps check what exactly the call
        // setup is onces the call is up and running 

        if (protectionMethod == 1 || protectionMethod == 3)
        {
            // RED
            error = ptrViECodec->GetCodec(ptrViECodec->NumberOfCodecs() - 2,
                                          videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // ULPFEC
            error = ptrViECodec->GetCodec(ptrViECodec->NumberOfCodecs() - 1,
                                          videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
        }

        // **** start the engines
        // VE first
        error = ptrVEBase->StartReceive(audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEBase->StartPlayout(audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEBase->StartSend(audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // ViE next
        error = ptrViEBase->StartSend(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StartReceive(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StartRender(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StartRender(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        ViEAutotestFileObserver fileObserver;
        int fileId;
        //  Codec Observers
        // TODO (amyfong):  Change the observers to pointers, use NULL checks
        // to toggle between registered or deregistered

        ViEAutotestEncoderObserver codecEncoderObserver;
        ViEAutotestDecoderObserver codecDecoderObserver;

        //***************************************************************
        //  Engine ready. Wait for input
        //***************************************************************

        // Call started
        std::cout << std::endl;
        std::cout << "Custom call started" << std::endl;
        std::cout << std::endl << std::endl;

        // Modify call or stop call

        std::cout << "Custom call in progress, would you like do?" << std::endl;
        std::cout << "  0. Stop the call" << std::endl;
        std::cout << "  1. Modify the call" << std::endl;
        std::cout << "What do you want to do? "
                  << "Press enter for default (Stop the call): ";

        std::getline(std::cin, str);
        int selection = 0;
        selection = atoi(str.c_str());

        // keep on modifying the call until user selects finish modify call
        bool modify_call = false;

        while (selection == 1) {

          std::cout << "Modify Custom Call" << std::endl;
          std::cout << "  0. Finished modifying custom call" << std::endl;
          std::cout << "  1. Change Video Codec" << std::endl;
          std::cout << "  2. Change Video Size by Common Resolutions"
                    << std::endl;
          std::cout << "  3. Change Video Size by Width & Height" << std::endl;
          std::cout << "  4. Change Video Capture Device" << std::endl;
          std::cout << "  5. Record Incoming Call" << std::endl;
          std::cout << "  6. Record Outgoing Call" << std::endl;
          std::cout << "  7. Play File on Video Channel"
                    << "(Assumes you recorded incoming & outgoing call)" 
                    << std::endl;
          std::cout << "  8. Change Video Protection Method" << std::endl;
          std::cout << "  9. Toggle Encoder Observer" << std::endl;
          std::cout << " 10. Toggle Decoder Observer" << std::endl;
          std::cout << " 11. Print Call Information" << std::endl;
          std::cout << " 12. Print Call Statistics" << std::endl;
          std::cout << "What do you want to do? ";
          std::cout << "Press enter for default "
                    << "(Finished modifying custom call): ";

          std::getline(std::cin, str);
          int modify_selection = 0;
          int file_selection = 0;

          modify_selection = atoi(str.c_str());

          switch (modify_selection) {
            case 0:
              std::cout << "Finished modifying custom call." << std::endl;
              modify_call = false;
              break;
            case 1:
              // Change video Codec 
              GetVideoCodecType(ptrViECodec, videoCodec);
              GetVideoCodecSize(ptrViECodec, videoCodec);
              GetVideoCodecBitrate(ptrViECodec, videoCodec);
              GetVideoCodecMaxBitrate(ptrViECodec, videoCodec);
              GetVideoCodecMaxFramerate(ptrViECodec, videoCodec);
              PrintCallInformation(ipAddress, deviceName,
                                   uniqueId, videoCodec,
                                   videoTxPort, videoRxPort,
                                   audioCaptureDeviceName,
                                   audioPlaybackDeviceName, audioCodec,
                                   audioTxPort, audioRxPort);
              error = ptrViECodec->SetSendCodec(videoChannel, videoCodec);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
              error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
              modify_call = true;
              break;
            case 2:
              // Change Video codec size by common resolution
              GetVideoCodecResolution(ptrViECodec, videoCodec);
              PrintCallInformation(ipAddress, deviceName,
                                   uniqueId, videoCodec,
                                   videoTxPort, videoRxPort,
                                   audioCaptureDeviceName,
                                   audioPlaybackDeviceName, audioCodec,
                                   audioTxPort, audioRxPort);
              error = ptrViECodec->SetSendCodec(videoChannel, videoCodec);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                    __FUNCTION__, __LINE__);

              error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
              modify_call = true;
              break;
            case 3:
              // Change Video codec by size height and width
              GetVideoCodecSize(ptrViECodec, videoCodec);
              PrintCallInformation(ipAddress, deviceName,
                                   uniqueId, videoCodec,
                                   videoTxPort, videoRxPort,
                                   audioCaptureDeviceName,
                                   audioPlaybackDeviceName, audioCodec,
                                   audioTxPort,  audioRxPort);
              error = ptrViECodec->SetSendCodec(videoChannel, videoCodec);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
              error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
              numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
              modify_call = true;
              break;
            case 4:
              error = ptrViERender->StopRender(captureId);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                    __FUNCTION__, __LINE__);
              error = ptrViERender->RemoveRenderer(captureId);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                  __FUNCTION__, __LINE__);
              error = ptrViECapture->StopCapture(captureId);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                    __FUNCTION__, __LINE__);
              error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                    __FUNCTION__, __LINE__);
              error = ptrViECapture->ReleaseCaptureDevice(captureId);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                    __FUNCTION__, __LINE__);
              memset(deviceName, 0, KMaxUniqueIdLength);
              memset(uniqueId, 0, KMaxUniqueIdLength);
              GetVideoDevice(ptrViEBase, ptrViECapture, deviceName, uniqueId);
              captureId = 0;
              error = ptrViECapture->AllocateCaptureDevice(uniqueId,
                                                           KMaxUniqueIdLength,
                                                           captureId);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                    __FUNCTION__, __LINE__);
              error = ptrViECapture->ConnectCaptureDevice(
                  captureId, videoChannel);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                    __FUNCTION__, __LINE__);
              error = ptrViECapture->StartCapture(captureId);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                    __FUNCTION__, __LINE__);
              error = ptrViERender->AddRenderer(
                  captureId, _window1, 0, 0.0, 0.0, 1.0, 1.0);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
              error = ptrViERender->StartRender(captureId);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR: %s at line %d",
                                                   __FUNCTION__, __LINE__);
              modify_call = true;
              break;
            case 5:
              // Record the incoming call
              std::cout << "Start Recording Incoming Video " 
                        << DEFAULT_INCOMING_FILE_NAME <<  std::endl;
              error = ptrViEFile->StartRecordIncomingVideo(
                  videoChannel, DEFAULT_INCOMING_FILE_NAME,
                  webrtc::NO_AUDIO, audioCodec, videoCodec);
              std::cout << "Press enter to stop...";
              std::getline(std::cin, str);
              error = ptrViEFile->StopRecordIncomingVideo(videoChannel);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR:%d %s at line %d",
                                                   ptrViEBase->LastError(),
                                                   __FUNCTION__, __LINE__);
              modify_call = true;
              break;
            case 6:
              // Record the outgoing call
              std::cout << "Start Recording Outgoing Video " 
                        << DEFAULT_OUTGOING_FILE_NAME <<  std::endl;
              error = ptrViEFile->StartRecordOutgoingVideo(
                  videoChannel, DEFAULT_OUTGOING_FILE_NAME,
                  webrtc::NO_AUDIO, audioCodec, videoCodec);
              std::cout << "Press enter to stop...";
              std::getline(std::cin, str);
              error = ptrViEFile->StopRecordOutgoingVideo(videoChannel);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR:%d %s at line %d",
                                                   ptrViEBase->LastError(),
                                                   __FUNCTION__, __LINE__);
              modify_call = true;
              break;
            case 7:
              // Send the file on the videoChannel
              file_selection = 0;
              std::cout << "Available files to play" << std::endl;
              std::cout << "  0. " << DEFAULT_INCOMING_FILE_NAME <<  std::endl;
              std::cout << "  1. " << DEFAULT_OUTGOING_FILE_NAME <<  std::endl;
              std::cout << "Press enter for default (" 
                        << DEFAULT_INCOMING_FILE_NAME << "): ";
              std::getline(std::cin, str);
              file_selection = atoi(str.c_str());
              // Disconnect the camera first
              error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR:%d %s at line %d",
                                                   ptrViEBase->LastError(),
                                                   __FUNCTION__, __LINE__);
              if (file_selection == 1) 
                error = ptrViEFile->StartPlayFile(DEFAULT_OUTGOING_FILE_NAME,
                                                  fileId, true);
              else
                error = ptrViEFile->StartPlayFile(DEFAULT_INCOMING_FILE_NAME,
                                                  fileId, true);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR:%d %s at line %d",
                                                   ptrViEBase->LastError(),
                                                   __FUNCTION__, __LINE__);
              ViETest::Log("Registering file observer");
              error = ptrViEFile->RegisterObserver(fileId, fileObserver);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR:%d %s at line %d",
                                                    ptrViEBase->LastError(),
                                                    __FUNCTION__, __LINE__);
              std::cout << std::endl;
              std::cout << "Start sending the file that is played in a loop " 
                        << std::endl;
              error = ptrViEFile->SendFileOnChannel(fileId, videoChannel);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR:%d %s at line %d",
                                                   ptrViEBase->LastError(),
                                                   __FUNCTION__, __LINE__);
              std::cout << "Press enter to stop...";
              std::getline(std::cin, str);
              ViETest::Log("Stopped sending video on channel");
              error = ptrViEFile->StopSendFileOnChannel(videoChannel);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR:%d %s at line %d",
                                                   ptrViEBase->LastError(),
                                                   __FUNCTION__, __LINE__);
              ViETest::Log("Stop playing the file.");
              error = ptrViEFile->StopPlayFile(fileId);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR:%d %s at line %d",
                                                   ptrViEBase->LastError(),
                                                   __FUNCTION__, __LINE__);
              error = ptrViECapture->ConnectCaptureDevice(captureId,
                                                          videoChannel);
              numberOfErrors += ViETest::TestError(error == 0,
                                                   "ERROR:%d %s at line %d",
                                                   ptrViEBase->LastError(),
                                                   __FUNCTION__, __LINE__);
              error = ptrViEFile->DeregisterObserver(fileId, fileObserver);
              numberOfErrors += ViETest::TestError(error == -1,
                                                   "ERROR:%d %s at line %d",
                                                   ptrViEBase->LastError(),
                                                   __FUNCTION__, __LINE__);
              modify_call = true;
              break;
            case 8:
              // Change the Video Protection
              SetVideoProtection(ptrViECodec, videoCodec, ptrViERtpRtcp,
                                 videoChannel);
              PrintCallInformation(ipAddress, deviceName,
                                   uniqueId, videoCodec,
                                   videoTxPort, videoRxPort,
                                   audioCaptureDeviceName,
                                   audioPlaybackDeviceName,
                                   audioCodec, audioTxPort,
                                   audioRxPort);

              modify_call = true;
              break;  
            case 9:
              // Toggle Encoder Observer
              if (!isEncoderObserverRegistered) {
                std::cout << "Registering Encoder Observer" << std::endl;
                error = ptrViECodec->RegisterEncoderObserver(videoChannel,
                    codecEncoderObserver);
                numberOfErrors += ViETest::TestError(error == 0,
                                                     "ERROR: %s at line %d",
                                                     __FUNCTION__, __LINE__);
              } else {
                std::cout << "Deregistering Encoder Observer" << std::endl;
                error = ptrViECodec->DeregisterEncoderObserver(videoChannel);
                numberOfErrors += ViETest::TestError(error == 0,
                                                     "ERROR: %s at line %d",
                                                     __FUNCTION__, __LINE__);
                isEncoderObserverRegistered = false;
              }
              isEncoderObserverRegistered = !isEncoderObserverRegistered;
              modify_call = true;
              break;
            case 10:
              // Toggle Decoder Observer
              if (!isDecoderObserverRegistered) {
                std::cout << "Registering Decoder Observer" << std::endl;
                error = ptrViECodec->RegisterDecoderObserver(videoChannel,
                    codecDecoderObserver);
                numberOfErrors += ViETest::TestError(error == 0,
                                                     "ERROR: %s at line %d",
                                                     __FUNCTION__, __LINE__);
              } else {
                std::cout << "Deregistering Decoder Observer" << std::endl;
                error = ptrViECodec->DeregisterDecoderObserver(videoChannel);
                numberOfErrors += ViETest::TestError(error == 0,
                                                     "ERROR: %s at line %d",
                                                     __FUNCTION__, __LINE__);
              }
              isDecoderObserverRegistered = !isDecoderObserverRegistered;
              modify_call = true;
              break;
            case 11:
              // Print Call information
              PrintCallInformation(ipAddress, deviceName,
                                   uniqueId, videoCodec,
                                   videoTxPort, videoRxPort,
                                   audioCaptureDeviceName,
                                   audioPlaybackDeviceName,
                                   audioCodec, audioTxPort,
                                   audioRxPort);
              modify_call = true;
              break;
            case 12:
              // Print Call statistics
              PrintRTCCPStatistics(ptrViERtpRtcp, videoChannel,
                                   kSendStatistic);
              PrintRTCCPStatistics(ptrViERtpRtcp, videoChannel,
                                   kReceivedStatistic);
              PrintRTPStatistics(ptrViERtpRtcp, videoChannel);
              PrintBandwidthUsage(ptrViERtpRtcp, videoChannel);
              PrintCodecStatistics(ptrViECodec, videoChannel,
                                   kSendStatistic);
              PrintCodecStatistics(ptrViECodec, videoChannel,
                                   kReceivedStatistic);
              PrintGetDiscardedPackets(ptrViECodec, videoChannel);
              modify_call = true;
              break;
            default:
              // invalid selection, shows options menu again 
              std::cout << "Invalid selection. Select Again." << std::endl;
              break;
          }
          // modify_call is false if user does not select one of the
          // modify options
          if (modify_call == false) {
            selection = 0;
          }
        }
        // Stop the Call
        std::cout << "Press enter to stop...";
        std::getline(std::cin, str);
        //***************************************************************
        //  Testing finished. Tear down Video Engine
        //***************************************************************
        // audio engine first
        error = ptrVEBase->StopReceive(audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEBase->StopPlayout(audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrVEBase->DeleteChannel(audioChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        // now do video
        error = ptrViEBase->DisconnectAudioChannel(videoChannel);

        error = ptrViEBase->StopReceive(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->StopSend(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StopRender(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->StopRender(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->RemoveRenderer(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViERender->RemoveRenderer(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->StopCapture(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->DisconnectCaptureDevice(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViECapture->ReleaseCaptureDevice(captureId);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ptrViEBase->DeleteChannel(videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        int remainingInterfaces = 0;
        remainingInterfaces = ptrViEFile->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        remainingInterfaces = ptrViECodec->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        remainingInterfaces = ptrViECapture->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        remainingInterfaces = ptrViERtpRtcp->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        remainingInterfaces = ptrViERender->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        remainingInterfaces = ptrViENetwork->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        remainingInterfaces = ptrViEBase->Release();
        numberOfErrors += ViETest::TestError(remainingInterfaces == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        bool deleted = webrtc::VideoEngine::Delete(ptrViE);
        numberOfErrors += ViETest::TestError(deleted == true,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViETest::Log(" ");
        ViETest::Log(" ViE Autotest Custom Call Started");
        ViETest::Log("========================================");
        ViETest::Log(" ");
    }
    return numberOfErrors;
}

bool GetVideoDevice(webrtc::ViEBase* ptrViEBase,
                    webrtc::ViECapture* ptrViECapture,
                    char* captureDeviceName,
                    char* captureDeviceUniqueId) {
    int error = 0;
    int numberOfErrors = 0;
    int captureDeviceIndex = 0;
    std::string str;

    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 256;
    char deviceName[KMaxDeviceNameLength];
    char uniqueId[KMaxUniqueIdLength];

    while(1)
    {
        memset(deviceName, 0, KMaxDeviceNameLength);
        memset(uniqueId, 0, KMaxUniqueIdLength);

        std::cout << std::endl;
        std::cout << "Available video capture devices:" << std::endl;
        int captureIdx = 0;
        for (captureIdx = 0;
             captureIdx < ptrViECapture->NumberOfCaptureDevices(); captureIdx++)
        {
            memset(deviceName, 0, KMaxDeviceNameLength);
            memset(uniqueId, 0, KMaxUniqueIdLength);

            error = ptrViECapture->GetCaptureDevice(captureIdx, deviceName,
                                                    KMaxDeviceNameLength,
                                                    uniqueId,
                                                    KMaxUniqueIdLength);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            std::cout << "   " << captureIdx+1 << ". " << deviceName
                      << "/" << uniqueId
                      << std::endl;
        }
        //  Get the devName of the default (or first) camera for display
        error = ptrViECapture->GetCaptureDevice(0, deviceName,
                                                KMaxDeviceNameLength,
                                                uniqueId,
                                                KMaxUniqueIdLength);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                              __FUNCTION__, __LINE__);

        std::cout << "Choose a video capture device. Press enter for default ("
                  << deviceName << "/" << uniqueId << "): ";
        std::getline(std::cin, str);
        captureDeviceIndex = atoi(str.c_str());

        if(captureDeviceIndex == 0)
        {
            // use default (or first) camera
            error = ptrViECapture->GetCaptureDevice(0, deviceName,
                                                    KMaxDeviceNameLength,
                                                    uniqueId,
                                                    KMaxUniqueIdLength);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            strcpy(captureDeviceUniqueId, uniqueId);
            strcpy(captureDeviceName, deviceName);
            return true;
        }
        else if(captureDeviceIndex < 0
                || (captureDeviceIndex >
                    (int)ptrViECapture->NumberOfCaptureDevices()))
        {
            // invalid selection
            continue;
        }
        else
        {
            error = ptrViECapture->GetCaptureDevice(captureDeviceIndex - 1,
                                                    deviceName,
                                                    KMaxDeviceNameLength,
                                                    uniqueId,
                                                    KMaxUniqueIdLength);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            strcpy(captureDeviceUniqueId, uniqueId);
            strcpy(captureDeviceName, deviceName);
            return true;
        }
    }
}

bool GetAudioDevices(webrtc::VoEBase* ptrVEBase,
                     webrtc::VoEHardware* ptrVEHardware,
                     char* recordingDeviceName,
                     int& recordingDeviceIndex,
                     char* playbackDeviceName,
                     int& playbackDeviceIndex)
{
    int error = 0;
    int numberOfErrors = 0;
    std::string str;

    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 128;
    char recordingDeviceUniqueName[KMaxDeviceNameLength];
    char playbackDeviceUniqueName[KMaxUniqueIdLength];

    int numberOfRecordingDevices = -1;
    error = ptrVEHardware->GetNumOfRecordingDevices(numberOfRecordingDevices);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    while(1)
    {
        recordingDeviceIndex = -1;
        std::cout << std::endl;
        std::cout << "Available audio capture devices:" << std::endl;
        int captureIdx = 0;

        for (captureIdx = 0; captureIdx < numberOfRecordingDevices;
             captureIdx++)
        {
            memset(recordingDeviceName, 0, KMaxDeviceNameLength);
            memset(recordingDeviceUniqueName, 0, KMaxDeviceNameLength);
            error = ptrVEHardware->GetRecordingDeviceName(
                captureIdx, recordingDeviceName, recordingDeviceUniqueName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            std::cout << "   " << captureIdx+1 << ". " << recordingDeviceName
                      << std::endl;
        }

        std::cout << "Choose an audio capture device. Press enter for default("
                  << recordingDeviceName << "): ";
        std::getline(std::cin, str);
        int captureDeviceIndex = atoi(str.c_str());

        if(captureDeviceIndex == 0)
        {
            // use default (or first) camera
            recordingDeviceIndex = 0;
            error = ptrVEHardware->GetRecordingDeviceName(
                recordingDeviceIndex, recordingDeviceName,
                recordingDeviceUniqueName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            break;
        }
        else if(captureDeviceIndex < 0
                || captureDeviceIndex > numberOfRecordingDevices)
        {
            // invalid selection
            continue;
        }
        else
        {
            recordingDeviceIndex = captureDeviceIndex - 1;
            error = ptrVEHardware->GetRecordingDeviceName(
                recordingDeviceIndex, recordingDeviceName,
                recordingDeviceUniqueName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            break;
        }
    }

    int numberOfPlaybackDevices = -1;
    error = ptrVEHardware->GetNumOfPlayoutDevices(numberOfPlaybackDevices);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    while(1)
    {
        playbackDeviceIndex = -1;
        std::cout << std::endl;
        std::cout << "Available audio playout devices:" << std::endl;
        int captureIdx = 0;

        for (captureIdx = 0; captureIdx < numberOfPlaybackDevices;
             captureIdx++)
        {
            memset(playbackDeviceName, 0, KMaxDeviceNameLength);
            memset(playbackDeviceUniqueName, 0, KMaxDeviceNameLength);
            error = ptrVEHardware->GetPlayoutDeviceName(
                captureIdx, playbackDeviceName, playbackDeviceUniqueName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            std::cout << "   " << captureIdx+1 << ". " << playbackDeviceName
                      << std::endl;
        }

        std::cout <<
                  "Choose an audio playback device. Press enter for default ("
                  << playbackDeviceName << "): ";
        std::getline(std::cin, str);
        int captureDeviceIndex = atoi(str.c_str());

        if(captureDeviceIndex == 0)
        {
            // use default (or first) camera
            playbackDeviceIndex = 0;
            error = ptrVEHardware->GetPlayoutDeviceName(
                playbackDeviceIndex, playbackDeviceName,
                playbackDeviceUniqueName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            return true;
        }
        else if(captureDeviceIndex < 0
                || captureDeviceIndex > numberOfPlaybackDevices)
        {
            // invalid selection
            continue;
        }
        else
        {
            playbackDeviceIndex = captureDeviceIndex - 1;
            error = ptrVEHardware->GetPlayoutDeviceName(
                playbackDeviceIndex, playbackDeviceName,
                playbackDeviceUniqueName);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            return true;
        }
    }
}

// general settings functions
bool GetIPAddress(char* iIP) {
    char oIP[16] = DEFAULT_SEND_IP;
    std::string str;

    while(1)
    {
        std::cout << std::endl;
        std::cout << "Enter destination IP. Press enter for default ("
                  << oIP << "): ";
        std::getline(std::cin, str);

        if(str.compare("") == 0)
        {
            // use default value;
            strcpy(iIP, oIP);
            return true;
        }

        if(ValidateIP(str) == false)
        {
            std::cout << "Invalid entry. Try again." << std::endl;
            continue;
        }

        // done. Copy std::string to c_string and return
        strcpy(iIP, str.c_str());
        return true;
    }
    assert(false);
    return false;
}

bool ValidateIP(std::string iStr) {
    if(0 == iStr.compare(""))
    {
        return false;
    }
    return true;
}

// video settings functions
bool GetVideoPorts(int* txPort, int* rxPort) {
    std::string str;
    int port = 0;

    // set to default values
    *txPort = DEFAULT_VIDEO_PORT;
    *rxPort = DEFAULT_VIDEO_PORT;

    while(1)
    {
        std::cout << "Enter video send port. Press enter for default ("
                  << *txPort << "):  ";
        std::getline(std::cin, str);
        port = atoi(str.c_str());

        if(port == 0)
        {
            // default value
            break;
        }
        else
        {
            // user selection
            if(port <= 0 || port > 63556)
            {
                // invalid selection
                continue;
            }
            else
            {
                *txPort = port;
                break; // move on to rxport
            }
        }
    }

    while(1)
    {
        std::cout << "Enter video receive port. Press enter for default ("
                  << *rxPort << "):  ";
        std::getline(std::cin, str);
        port = atoi(str.c_str());

        if(port == 0)
        {
            // default value
            return true;
        }
        else
        {
            // user selection
            if(port <= 0 || port > 63556)
            {
                // invalid selection
                continue;
            }
            else
            {
              *rxPort = port;
              return true;
            }
      }
  }
  assert(false);
  return false;
}

// audio settings functions
bool GetAudioPorts(int* txPort, int* rxPort) {
    int port = 0;
    std::string str;

    // set to default values
    *txPort = DEFAULT_AUDIO_PORT;
    *rxPort = DEFAULT_AUDIO_PORT;

    while(1)
    {
        std::cout << "Enter audio send port. Press enter for default ("
                  << *txPort << "):  ";
        std::getline(std::cin, str);
        port = atoi(str.c_str());

        if(port == 0)
        {
            // default value
            break;
        }
        else
        {
            // user selection
            if(port <= 0 || port > 63556)
            {
                // invalid selection
                continue;
            }
            else
            {
                *txPort = port;
                break; // move on to rxport
            }
        }
    }

    while(1)
    {
        std::cout << "Enter audio receive port. Press enter for default ("
                  << *rxPort << "):  ";
        std::getline(std::cin, str);
        port = atoi(str.c_str());

        if(port == 0)
        {
            // default value
            return true;
        }
        else
        {
            // user selection
            if(port <= 0 || port > 63556)
            {
                // invalid selection
                continue;
            }
            else
            {
                *rxPort = port;
                return true;
            }
        }
    }
    assert(false);
    return false;
}

bool GetAudioCodec(webrtc::VoECodec* ptrVeCodec,
                   webrtc::CodecInst& audioCodec) {
    int error = 0;
    int numberOfErrors = 0;
    int codecSelection = 0;
    std::string str;
    memset(&audioCodec, 0, sizeof(webrtc::CodecInst));

    while(1)
    {
        std::cout << std::endl;
        std::cout << "Available audio codecs:" << std::endl;
        int codecIdx = 0;
        int defaultCodecIdx = 0;
        for (codecIdx = 0; codecIdx < ptrVeCodec->NumOfCodecs(); codecIdx++)
        {
            error = ptrVeCodec->GetCodec(codecIdx, audioCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);

            // test for default codec index
            if(strcmp(audioCodec.plname, DEFAULT_AUDIO_CODEC) == 0)
            {
              defaultCodecIdx = codecIdx;
            }
            std::cout << "   " << codecIdx+1 << ". " << audioCodec.plname
                      << std::endl;
        }
        std::cout << std::endl;
        std::cout << "Choose audio codec. Press enter for default ("
                  << DEFAULT_AUDIO_CODEC << "):  ";
        std::getline(std::cin, str);
        codecSelection = atoi(str.c_str());

        if(codecSelection == 0)
        {
            // use default
            error = ptrVeCodec->GetCodec(defaultCodecIdx, audioCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            return true;
        }
        else
        {
            // user selection
            codecSelection = atoi(str.c_str())-1;
            error = ptrVeCodec->GetCodec(codecSelection, audioCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            if(error != 0)
            {
                std::cout << "ERROR: Code = " << error << " Invalid selection"
                          << std::endl;
                continue;
            }
            return true;
        }
    }
    assert(false);
    return false;
}

void PrintCallInformation(char* IP, char* videoCaptureDeviceName,
                          char* videoCaptureUniqueId,
                          webrtc::VideoCodec videoCodec,
                          int videoTxPort, int videoRxPort,
                          char* audioCaptureDeviceName,
                          char* audioPlaybackDeviceName,
                          webrtc::CodecInst audioCodec,
                          int audioTxPort, int audioRxPort) {
    std::string str;

    std::cout << "************************************************"
              << std::endl;
    std::cout << "The call has the following settings: " << std::endl;
    std::cout << "\tIP: " << IP << std::endl;
    std::cout << "\tVideo Capture Device: " << videoCaptureDeviceName
              << std::endl;
    std::cout << "\t\tName: " << videoCaptureDeviceName << std::endl;
    std::cout << "\t\tUniqueId: " << videoCaptureUniqueId << std::endl;
    std::cout << "\tVideo Codec: " << std::endl;
    std::cout << "\t\tplName: " << videoCodec.plName << std::endl;
    std::cout << "\t\tplType: " << (int)videoCodec.plType << std::endl;
    std::cout << "\t\twidth: " << videoCodec.width << std::endl;
    std::cout << "\t\theight: " << videoCodec.height << std::endl;
    std::cout << "\t\tstartBitrate: " << videoCodec.startBitrate << std::endl;
    std::cout << "\t\tmaxBitrate: " << videoCodec.maxBitrate << std::endl;
    std::cout << "\t\tmaxFramerate: " << (int)videoCodec.maxFramerate
                                      << std::endl;
    std::cout << "\t Video Tx Port: " << videoTxPort << std::endl;
    std::cout << "\t Video Rx Port: " << videoRxPort << std::endl;
    std::cout << "\tAudio Capture Device: " << audioCaptureDeviceName
              << std::endl;
    std::cout << "\tAudio Playback Device: " << audioPlaybackDeviceName
              << std::endl;
    std::cout << "\tAudio Codec: " << std::endl;
    std::cout << "\t\tplname: " << audioCodec.plname << std::endl;
    std::cout << "\t\tpltype: " << (int)audioCodec.pltype << std::endl;
    std::cout << "\t Audio Tx Port: " << audioTxPort << std::endl;
    std::cout << "\t Audio Rx Port: " << audioRxPort << std::endl;
    std::cout << "************************************************"
              << std::endl;
}
// TODO(amyfong):  Change the GetVideo*  to SetVideo* where applicable
bool GetVideoCodecType(webrtc::ViECodec* ptrViECodec,
                       webrtc::VideoCodec& videoCodec) {
  int error = 0;
  int numberOfErrors = 0;
  int codecSelection = 0;
  std::string str;
  memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));

  bool exitLoop=false;
  while(!exitLoop) {
    std::cout << std::endl;
    std::cout << "Available video codecs:" << std::endl;
    int codecIdx = 0;
    int defaultCodecIdx = 0;
    for (codecIdx = 0; codecIdx < ptrViECodec->NumberOfCodecs(); codecIdx++) {
      error = ptrViECodec->GetCodec(codecIdx, videoCodec);
      numberOfErrors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                            __FUNCTION__, __LINE__);

      // test for default codec index
      if(strcmp(videoCodec.plName, DEFAULT_VIDEO_CODEC) == 0) {
        defaultCodecIdx = codecIdx;
      }
      std::cout << "   " << codecIdx+1 << ". " << videoCodec.plName                    
                << std::endl;
    }
    std::cout << std::endl;
    std::cout << "Choose video codec. Press enter for default ("
              << DEFAULT_VIDEO_CODEC << "):  ";
    std::getline(std::cin, str);
    codecSelection = atoi(str.c_str());
    if(codecSelection == 0) {
      // use default
      error = ptrViECodec->GetCodec(defaultCodecIdx, videoCodec);
      numberOfErrors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                            __FUNCTION__, __LINE__);
      exitLoop=true;
    }
    else {
    // user selection
      codecSelection = atoi(str.c_str())-1;
      error = ptrViECodec->GetCodec(codecSelection, videoCodec);
      numberOfErrors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                            __FUNCTION__, __LINE__);
      if(error != 0) {
        std::cout << "ERROR: Code=" << error << " Invalid selection"
                  << std::endl;
        continue;
      }
      exitLoop=true;
    }
  }
  if (videoCodec.codecType == webrtc::kVideoCodecI420) {
    videoCodec.width = 176;
    videoCodec.height = 144;
  }
  return true;
}

bool GetVideoCodecResolution(webrtc::ViECodec* ptrViECodec,
                             webrtc::VideoCodec& videoCodec) {
  std::string str;
  int sizeOption = 5;

  if (videoCodec.codecType == webrtc::kVideoCodecVP8) {
    std::cout << std::endl;
    std::cout << "Available Common Resolutions : " << std::endl;
    std::cout << "  1. SQCIF (128X96) " << std::endl;
    std::cout << "  2. QQVGA (160X120) " << std::endl;
    std::cout << "  3. QCIF (176X144) " << std::endl;
    std::cout << "  4. CIF  (352X288) " << std::endl;
    std::cout << "  5. VGA  (640X480) " << std::endl;
    std::cout << "  6. WVGA (800x480) " << std::endl;
    std::cout << "  7. 4CIF (704X576) " << std::endl;
    std::cout << "  8. SVGA (800X600) " << std::endl;
    std::cout << "  9. HD   (1280X720) " << std::endl;
    std::cout << " 10. XGA  (1024x768) " << std::endl;
    std::cout << "Enter frame size option: " << std::endl;

    std::getline(std::cin, str);
    sizeOption = atoi(str.c_str());
   
   switch (sizeOption) {
     case 1:
       videoCodec.width = 128;
       videoCodec.height = 96;
       break;
     case 2:
       videoCodec.width = 160;
       videoCodec.height = 120;
       break;
     case 3:
       videoCodec.width = 176;
       videoCodec.height = 144;
       break;
     case 4:
       videoCodec.width = 352;
       videoCodec.height = 288;
       break;
     case 5:
       videoCodec.width = 640;
       videoCodec.height = 480;
       break;
     case 6:
       videoCodec.width = 800;
       videoCodec.height = 480;
       break;
     case 7:
       videoCodec.width = 704;
       videoCodec.height = 576;
       break;
     case 8:
       videoCodec.width = 800;
       videoCodec.height = 600;
       break;
     case 9:
       videoCodec.width = 1280;
       videoCodec.height = 720;
       break;
     case 10:
       videoCodec.width = 1024;
       videoCodec.height = 768;
       break;
    }
  }  
  else {
      std::cout << "Can Only change codec size if it's VP8" << std::endl;
  }
  return true;
}

bool GetVideoCodecSize(webrtc::ViECodec* ptrViECodec,
                       webrtc::VideoCodec& videoCodec) {
  if (videoCodec.codecType == webrtc::kVideoCodecVP8) {
    std::string str;
    videoCodec.width = DEFAULT_VIDEO_CODEC_WIDTH;
    videoCodec.height = DEFAULT_VIDEO_CODEC_HEIGHT;
    std::cout << "Choose video width. Press enter for default ("
              << DEFAULT_VIDEO_CODEC_WIDTH << "):  ";
    std::getline(std::cin, str);
    int sizeSelection = atoi(str.c_str());
    if(sizeSelection!=0) {
      videoCodec.width=sizeSelection;
    }
    std::cout << "Choose video height. Press enter for default ("
              << DEFAULT_VIDEO_CODEC_HEIGHT << "):  ";
    std::getline(std::cin, str);
    sizeSelection = atoi(str.c_str());
    if(sizeSelection!=0) {
      videoCodec.height=sizeSelection;
    }
  }
  else {
    std::cout << "Can Only change codec size if it's VP8" << std::endl;
  }
  return true;
}

bool GetVideoCodecBitrate(webrtc::ViECodec* ptrViECodec,
                          webrtc::VideoCodec& videoCodec) {
    std::string str;
    std::cout << std::endl;
    std::cout << "Choose start rate (in kbps). Press enter for default ("
              << DEFAULT_VIDEO_CODEC_BITRATE << "):  ";
    std::getline(std::cin, str);
    int startRate = atoi(str.c_str());
    videoCodec.startBitrate = DEFAULT_VIDEO_CODEC_BITRATE;
    if (startRate != 0) {
        videoCodec.startBitrate = startRate;
    }
    return true;
}

bool GetVideoCodecMaxBitrate(webrtc::ViECodec* ptrViECodec,
                             webrtc::VideoCodec& videoCodec) {
    std::string str;
    std::cout << std::endl;
    std::cout << "Choose max bitrate (in kbps). Press enter for default ("
              << DEFAULT_VIDEO_CODEC_MAX_BITRATE << "):  ";
    std::getline(std::cin, str);
    int maxRate = atoi(str.c_str());
    videoCodec.maxBitrate = DEFAULT_VIDEO_CODEC_MAX_BITRATE;
    if (maxRate != 0) {
        videoCodec.maxBitrate = maxRate;
    }
    return true;
}

bool GetVideoCodecMaxFramerate(webrtc::ViECodec* ptrViECodec,
                               webrtc::VideoCodec& videoCodec) {
    std::string str;
    std::cout << std::endl;
    std::cout << "Choose max framerate (in fps). Press enter for default ("
              << DEFAULT_VIDEO_CODEC_MAX_FRAMERATE << "):  ";
    std::getline(std::cin, str);
    char maxFrameRate = atoi(str.c_str());
    videoCodec.maxFramerate = DEFAULT_VIDEO_CODEC_MAX_FRAMERATE;
    if (maxFrameRate != 0) {
        videoCodec.maxFramerate = maxFrameRate;
    }
    return true;
}

bool SetVideoProtection(webrtc::ViECodec* ptrViECodec,
                        webrtc::VideoCodec& videoCodec,
                        webrtc::ViERTP_RTCP* ptrViERtpRtcp,
                        int videoChannel) {
  int error = 0;
  int numberOfErrors = 0;
  int protectionMethod = 0;

  std::cout << "Available Video Protection Method" << std::endl;
  std::cout << "  0. None" << std::endl;
  std::cout << "  1. FEC" << std::endl;
  std::cout << "  2. NACK" << std::endl;
  std::cout << "  3. NACK+FEC" << std::endl;
  std::cout << "Enter Video Protection Method: ";

  std::string method;
  std::getline(std::cin, method);
  protectionMethod = atoi(method.c_str());
  // Set all video protection to false initially
  error = ptrViERtpRtcp->SetHybridNACKFECStatus(videoChannel, false,
                                                VCM_RED_PAYLOAD_TYPE,
                                                VCM_ULPFEC_PAYLOAD_TYPE);
  numberOfErrors += ViETest::TestError(error == 0,
                                       "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERtpRtcp->SetFECStatus(videoChannel, false,
                                      VCM_RED_PAYLOAD_TYPE,
                                      VCM_ULPFEC_PAYLOAD_TYPE);
  numberOfErrors += ViETest::TestError(error == 0,
                                       "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  error = ptrViERtpRtcp->SetNACKStatus(videoChannel, false);
  numberOfErrors += ViETest::TestError(error == 0,
                                       "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);

  // Set video protection for FEC, NACK or Hybrid
  switch (protectionMethod) {
    case 0: // None
      // No protection selected, all protection already at false
      break;
    case 1: // FEC only
      error = ptrViERtpRtcp->SetFECStatus(videoChannel, true,
                                          VCM_RED_PAYLOAD_TYPE,
                                          VCM_ULPFEC_PAYLOAD_TYPE);
      numberOfErrors += ViETest::TestError(error == 0,
                                          "ERROR: %s at line %d",
                                          __FUNCTION__, __LINE__);
      break;
    case 2: // NACK only
      error = ptrViERtpRtcp->SetNACKStatus(videoChannel, true);
      numberOfErrors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
      break;
    case 3: // Hybrid NACK and FEC
      error = ptrViERtpRtcp->SetHybridNACKFECStatus(videoChannel, true,
                                                    VCM_RED_PAYLOAD_TYPE,
                                                    VCM_ULPFEC_PAYLOAD_TYPE);
      numberOfErrors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
      break;
  }
  // Set receive codecs for FEC and hybrid NACK/FEC
  if (protectionMethod == 1 || protectionMethod == 3) {
    // RED
    error = ptrViECodec->GetCodec(ptrViECodec->NumberOfCodecs() - 2,
                                  videoCodec);
    numberOfErrors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
    numberOfErrors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    // ULPFEC
    error = ptrViECodec->GetCodec(ptrViECodec->NumberOfCodecs() - 1,
                                  videoCodec);
    numberOfErrors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ptrViECodec->SetReceiveCodec(videoChannel, videoCodec);
    numberOfErrors += ViETest::TestError(error == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
  }
  return true;
}

void PrintRTCCPStatistics(webrtc::ViERTP_RTCP* ptrViERtpRtcp,
                          int videoChannel, StatisticsType statType) {
  int error = 0;
  int numberOfErrors =0;
  unsigned short fractionLost = 0;
  unsigned int cumulativeLost = 0;
  unsigned int extendedMax = 0;
  unsigned int jitter = 0;
  int rttMS = 0;

  switch (statType) {
    case kReceivedStatistic:
      std::cout << "RTCP Received statistics"
                << std::endl;
      // Get and print the Received RTCP Statistics
      error = ptrViERtpRtcp->GetReceivedRTCPStatistics(videoChannel,
                                                       fractionLost,
                                                       cumulativeLost,
                                                       extendedMax,
                                                       jitter, rttMS);
      numberOfErrors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
      break;
    case kSendStatistic:
      std::cout << "RTCP Sent statistics"
                << std::endl;
      // Get and print the Sent RTCP Statistics
      error = ptrViERtpRtcp->GetSentRTCPStatistics(videoChannel, fractionLost,
                                                   cumulativeLost, extendedMax,
                                                   jitter, rttMS);
      numberOfErrors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
      break;
    default:
      std::cout << "Invalid RTCP Statistics selected" << std::endl;
      break;
  }
  std::cout << "\tRTCP fraction of lost packets: "
            << fractionLost << std::endl;
  std::cout << "\tRTCP cumulative number of lost packets: "
            << cumulativeLost << std::endl;
  std::cout << "\tRTCP max received sequence number "
            << extendedMax << std::endl;
  std::cout << "\tRTCP jitter: "
            << jitter << std::endl;
  std::cout << "\tRTCP round trip (ms): "
            << rttMS<< std::endl;
}

void PrintRTPStatistics(webrtc::ViERTP_RTCP* ptrViERtpRtcp,
                        int videoChannel) {
  int error = 0;
  int numberOfErrors =0;
  unsigned int bytesSent = 0;
  unsigned int packetsSent= 0;
  unsigned int bytesReceived = 0;
  unsigned int packetsReceived = 0;

  std::cout << "RTP statistics"
            << std::endl;

  // Get and print the RTP Statistics
  error = ptrViERtpRtcp->GetRTPStatistics(videoChannel, bytesSent, packetsSent,
                                          bytesReceived, packetsReceived);
  numberOfErrors += ViETest::TestError(error == 0,
                                       "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  std::cout << "\tRTP bytes sent: "
            << bytesSent << std::endl;
  std::cout << "\tRTP packets sent: "
            << packetsSent << std::endl;
  std::cout << "\tRTP bytes received: "
            << bytesReceived << std::endl;
  std::cout << "\tRTP packets received: "
            << packetsReceived << std::endl;
}

void PrintBandwidthUsage(webrtc::ViERTP_RTCP* ptrViERtpRtcp,
                         int videoChannel) {
  int error = 0;
  int numberOfErrors = 0;
  unsigned int totalBitrateSent = 0;
  unsigned int videoBitrateSent = 0;
  unsigned int fecBitrateSent = 0;
  unsigned int nackBitrateSent = 0;
  double percentageFEC = 0;
  double percentageNACK = 0;

  std::cout << "Bandwidth Usage"
            << std::endl;

  // Get and print Bandwidth usage
  error = ptrViERtpRtcp->GetBandwidthUsage(videoChannel, totalBitrateSent,
                                           videoBitrateSent, fecBitrateSent,
                                           nackBitrateSent);
  numberOfErrors += ViETest::TestError(error == 0,
                                       "ERROR: %s at line %d",
                                       __FUNCTION__, __LINE__);
  std::cout << "\tTotal bitrate sent (Kbit/s): "
            << totalBitrateSent << std::endl;
  std::cout << "\tVideo bitrate sent (Kbit/s): "
            << videoBitrateSent << std::endl;
  std::cout << "\tFEC bitrate sent (Kbit/s): "
            << fecBitrateSent << std::endl;
  percentageFEC = ((double)fecBitrateSent/(double)totalBitrateSent) * 100;
  std::cout << "\tPercentage FEC bitrate sent from total bitrate: "
            << percentageFEC << std::endl;
  std::cout << "\tNACK bitrate sent (Kbit/s): "
            << nackBitrateSent << std::endl;
  percentageNACK = ((double)nackBitrateSent/(double)totalBitrateSent) * 100;
  std::cout << "\tPercentage NACK bitrate sent from total bitrate: "
            << percentageNACK << std::endl;
}

void PrintCodecStatistics(webrtc::ViECodec* ptrViECodec, int videoChannel,
                          StatisticsType statType) {
  int error = 0;
  int numberOfErrors = 0;
  unsigned int keyFrames = 0;
  unsigned int deltaFrames = 0;
  switch(statType) {
    case kReceivedStatistic:
      std::cout << "Codec Receive statistics"
                << std::endl;
      // Get and print the Receive Codec Statistics
      error = ptrViECodec->GetReceiveCodecStastistics(videoChannel, keyFrames,
                                                     deltaFrames);
      numberOfErrors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
      break;
    case kSendStatistic:
      std::cout << "Codec Send statistics"
                << std::endl;
      // Get and print the Send Codec Statistics
      error = ptrViECodec->GetSendCodecStastistics(videoChannel, keyFrames,
                                                  deltaFrames);
      numberOfErrors += ViETest::TestError(error == 0,
                                           "ERROR: %s at line %d",
                                           __FUNCTION__, __LINE__);
      break;
    default:
      std::cout << "Invalid Codec Statistics selected" << std::endl;
      break;
  }
  std::cout << "\tNumber of encoded key frames: "
            << keyFrames << std::endl;
  std::cout << "\tNumber of encoded delta frames: "
            << deltaFrames << std::endl;
}

void PrintGetDiscardedPackets(webrtc::ViECodec* ptrViECodec, int videoChannel) {
  std::cout << "Discarded Packets"
            << std::endl;
  int discardedPackets = 0;
  discardedPackets = ptrViECodec->GetDiscardedPackets(videoChannel);
  std::cout << "\tNumber of discarded packets: "
            << discardedPackets << std::endl;
}
