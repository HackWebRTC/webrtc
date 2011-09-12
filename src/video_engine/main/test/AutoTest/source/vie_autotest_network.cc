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
// vie_autotest_network.cc
//

#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "engine_configurations.h"

#include "tb_capture_device.h"
#include "tb_external_transport.h"
#include "tb_interfaces.h"
#include "tb_video_channel.h"

#if defined(_WIN32)
#include <qos.h>
#elif defined(WEBRTC_MAC_INTEL)

#endif

class ViEAutoTestNetworkObserver: public webrtc::ViENetworkObserver
{
public:
    ViEAutoTestNetworkObserver()
    {
    }
    ~ViEAutoTestNetworkObserver()
    {
    }
    virtual void OnPeriodicDeadOrAlive(const int videoChannel, const bool alive)
    {
    }
    virtual void PacketTimeout(const int videoChannel,
                               const webrtc::ViEPacketTimeout timeout)
    {
    }
};

int ViEAutoTest::ViENetworkStandardTest()
{
    int error = 0;
    int numberOfErrors = 0;

    tbInterfaces ViE("ViENetworkStandardTest", numberOfErrors); // Create VIE
    tbCaptureDevice tbCapture(ViE, numberOfErrors);
    error = ViE.ptrViERender->AddRenderer(tbCapture.captureId, _window1, 0,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->StartRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    {
        // Create a video channel
        tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);
        tbCapture.ConnectTo(tbChannel.videoChannel);

        error = ViE.ptrViERender->AddRenderer(tbChannel.videoChannel, _window2,
                                              1, 0.0, 0.0, 1.0, 1.0);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERender->StartRender(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        //***************************************************************
        //	Engine ready. Begin testing class
        //***************************************************************


        //
        // Transport
        //
        tbExternalTransport testTransport(*ViE.ptrViENetwork);
        error = ViE.ptrViENetwork->RegisterSendTransport(tbChannel.videoChannel,
                                                         testTransport);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViEBase->StartReceive(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error= ViE.ptrViERtpRtcp->SetKeyFrameRequestMethod(
            tbChannel.videoChannel, webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0,
                                                     "ERROR: %s at line %d",
                                                     __FUNCTION__, __LINE__);

        ViETest::Log("Call started using external transport, video should "
            "see video in both windows\n");
        AutoTestSleep(KAutoTestSleepTimeMs);

        error = ViE.ptrViEBase->StopReceive(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StopSend(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->DeregisterSendTransport(
            tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        char myIpAddress[64];
        memset(myIpAddress, 0, 64);
        unsigned short rtpPort = 1234;
        memcpy(myIpAddress, "127.0.0.1", sizeof("127.0.0.1"));
        error = ViE.ptrViENetwork->SetLocalReceiver(tbChannel.videoChannel,
                                                    rtpPort, rtpPort + 1,
                                                    myIpAddress);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendDestination(tbChannel.videoChannel,
                                                      myIpAddress, rtpPort,
                                                      rtpPort + 1, rtpPort);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StartReceive(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViETest::Log("Changed to WebRTC SocketTransport, you should still see "
                     "video in both windows\n");
        AutoTestSleep(KAutoTestSleepTimeMs);

        error = ViE.ptrViENetwork->SetSourceFilter(tbChannel.videoChannel,
                                                   rtpPort + 10, rtpPort + 11,
                                                   myIpAddress);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        ViETest::Log("Added UDP port filter for incorrect ports, you should "
            "not see video in Window2");
        AutoTestSleep(2000);
        error = ViE.ptrViENetwork->SetSourceFilter(tbChannel.videoChannel,
                                                   rtpPort, rtpPort + 1,
                                                   "123.1.1.0");
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        ViETest::Log("Added IP filter for incorrect IP address, you should not "
                     "see video in Window2");
        AutoTestSleep(2000);
        error = ViE.ptrViENetwork->SetSourceFilter(tbChannel.videoChannel,
                                                   rtpPort, rtpPort + 1,
                                                   myIpAddress);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        ViETest::Log("Added IP filter for this computer, you should see video "
                     "in Window2 again\n");
        AutoTestSleep(KAutoTestSleepTimeMs);

        tbCapture.Disconnect(tbChannel.videoChannel);
    }

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViENetwork Standard Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViENetwork Standard Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViENetworkExtendedTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViENetwork Extended Test\n");

    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************

    int numberOfErrors = ViENetworkStandardTest();

    int error = 0;

    tbInterfaces ViE("ViENetworkExtendedTest", numberOfErrors); // Create VIE
    tbCaptureDevice tbCapture(ViE, numberOfErrors);
    error = ViE.ptrViERender->AddRenderer(tbCapture.captureId, _window1, 0,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->StartRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    {
        //
        // ToS
        //
        // Create a video channel
        tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);
        tbCapture.ConnectTo(tbChannel.videoChannel);
        const char* remoteIp = "192.168.200.1";
        int DSCP = 0;
        bool useSetSockOpt = false;

        webrtc::VideoCodec videoCodec;
        error = ViE.ptrViECodec->GetSendCodec(tbChannel.videoChannel,
                                              videoCodec);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        videoCodec.maxFramerate = 5;
        error = ViE.ptrViECodec->SetSendCodec(tbChannel.videoChannel,
                                              videoCodec);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        //***************************************************************
        //	Engine ready. Begin testing class
        //***************************************************************

        char myIpAddress[64];
        memset(myIpAddress, 0, 64);
        unsigned short rtpPort = 9000;
        error = ViE.ptrViENetwork->GetLocalIP(myIpAddress, false);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetLocalReceiver(tbChannel.videoChannel,
                                                    rtpPort, rtpPort + 1,
                                                    myIpAddress);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendDestination(tbChannel.videoChannel,
                                                      remoteIp, rtpPort,
                                                      rtpPort + 1, rtpPort);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // ToS
        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, 2);
        if (error != 0)
        {
            ViETest::Log("ViESetSendToS error!.");
            ViETest::Log("You must be admin to run these tests.");
            ViETest::Log("On Win7 and late Vista, you need to right click the "
                         "exe and choose");
            ViETest::Log("\"Run as administrator\"\n");
            getchar();
        }
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendToS(tbChannel.videoChannel, DSCP,
                                              useSetSockOpt); // No ToS set
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViEBase->StartReceive(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViETest::Log("Use Wireshark to capture the outgoing video stream and "
                     "verify ToS settings\n");
        ViETest::Log(" DSCP set to 0x%x\n", DSCP);
        AutoTestSleep(1000);

        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, 63);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendToS(tbChannel.videoChannel, DSCP,
                                              useSetSockOpt); // No ToS set
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        ViETest::Log(" DSCP set to 0x%x\n", DSCP);
        AutoTestSleep(1000);

        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, 0);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, 2, true);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendToS(tbChannel.videoChannel, DSCP,
                                              useSetSockOpt); // No ToS set
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        ViETest::Log(" DSCP set to 0x%x\n", DSCP);
        AutoTestSleep(1000);

        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, 63, true);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendToS(tbChannel.videoChannel, DSCP,
                                              useSetSockOpt); // No ToS set
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        ViETest::Log(" DSCP set to 0x%x\n", DSCP);
        AutoTestSleep(1000);

        tbCapture.Disconnect(tbChannel.videoChannel);
    }

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViENetwork Extended Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViENetwork Extended Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViENetworkAPITest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViENetwork API Test\n");

    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************


    int error = 0;
    int numberOfErrors = 0;

    tbInterfaces ViE("ViENetworkAPITest", numberOfErrors); // Create VIE
    {
        // Create a video channel
        tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);

        //***************************************************************
        //	Engine ready. Begin testing class
        //***************************************************************

        //
        // External transport
        //
        tbExternalTransport testTransport(*ViE.ptrViENetwork);
        error = ViE.ptrViENetwork->RegisterSendTransport(tbChannel.videoChannel,
                                                         testTransport);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->RegisterSendTransport(tbChannel.videoChannel,
                                                         testTransport);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        unsigned char packet[1500];
        packet[0] = 0x80; // V=2, P=0, X=0, CC=0
        packet[1] = 0x78; // M=0, PT = 120 (VP8)
        error = ViE.ptrViENetwork->ReceivedRTPPacket(tbChannel.videoChannel,
                                                     packet, 1500);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->ReceivedRTCPPacket(tbChannel.videoChannel,
                                                      packet, 1500);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StartReceive(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->ReceivedRTPPacket(tbChannel.videoChannel,
                                                     packet, 1500);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->ReceivedRTCPPacket(tbChannel.videoChannel,
                                                      packet, 1500);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->ReceivedRTPPacket(tbChannel.videoChannel,
                                                     packet, 11);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->ReceivedRTPPacket(tbChannel.videoChannel,
                                                     packet, 11);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->ReceivedRTPPacket(tbChannel.videoChannel,
                                                     packet, 3000);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->ReceivedRTPPacket(tbChannel.videoChannel,
                                                     packet, 3000);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StopReceive(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->DeregisterSendTransport(
            tbChannel.videoChannel); // Sending
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StopSend(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->DeregisterSendTransport(
            tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->DeregisterSendTransport(
            tbChannel.videoChannel); // Already deregistered
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        //
        // Local receiver
        //
        // TODO (perkj) change when B 4239431 is fixed.
        /*error = ViE.ptrViENetwork->SetLocalReceiver(tbChannel.videoChannel,
                                                    1234, 1234, "127.0.0.1");
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);*/
        error = ViE.ptrViENetwork->SetLocalReceiver(tbChannel.videoChannel,
                                                    1234, 1235, "127.0.0.1");
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetLocalReceiver(tbChannel.videoChannel,
                                                    1234, 1235, "127.0.0.1");
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetLocalReceiver(tbChannel.videoChannel,
                                                    1236, 1237, "127.0.0.1");
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        unsigned short rtpPort = 0;
        unsigned short rtcpPort = 0;
        char ipAddress[64];
        memset(ipAddress, 0, 64);
        error = ViE.ptrViENetwork->GetLocalReceiver(tbChannel.videoChannel,
                                                    rtpPort, rtcpPort,
                                                    ipAddress);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StartReceive(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetLocalReceiver(tbChannel.videoChannel,
                                                    1234, 1235, "127.0.0.1");
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetLocalReceiver(tbChannel.videoChannel,
                                                    rtpPort, rtcpPort,
                                                    ipAddress);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StopReceive(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        //
        // Send destination
        //
        error = ViE.ptrViENetwork->SetSendDestination(tbChannel.videoChannel,
                                                      "127.0.0.1", 1234, 1235,
                                                      1234, 1235);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendDestination(tbChannel.videoChannel,
                                                      "127.0.0.1", 1236, 1237,
                                                      1234, 1235);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        unsigned short sourceRtpPort = 0;
        unsigned short sourceRtcpPort = 0;
        error = ViE.ptrViENetwork->GetSendDestination(tbChannel.videoChannel,
                                                      ipAddress, rtpPort,
                                                      rtcpPort, sourceRtpPort,
                                                      sourceRtcpPort);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Not allowed while sending
        error = ViE.ptrViENetwork->SetSendDestination(tbChannel.videoChannel,
                                                      "127.0.0.1", 1234, 1235,
                                                      1234, 1235);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(ViE.ptrViEBase->LastError()
            == kViENetworkAlreadySending, "ERROR: %s at line %d", __FUNCTION__,
                                             __LINE__);

        error = ViE.ptrViEBase->StopSend(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViENetwork->SetSendDestination(tbChannel.videoChannel,
                                                      "127.0.0.1", 1234, 1235,
                                                      1234, 1235);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViENetwork->GetSendDestination(tbChannel.videoChannel,
                                                      ipAddress, rtpPort,
                                                      rtcpPort, sourceRtpPort,
                                                      sourceRtcpPort);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViEBase->StopSend(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        //
        // Address information
        //

        // GetSourceInfo: Tested in functional test
        error = ViE.ptrViENetwork->GetLocalIP(ipAddress, false);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        // TODO: IPv6

        //
        // Filter
        //
        error = ViE.ptrViENetwork->GetSourceFilter(tbChannel.videoChannel,
                                                   rtpPort, rtcpPort, ipAddress);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSourceFilter(tbChannel.videoChannel,
                                                   1234, 1235, "10.10.10.10");
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSourceFilter(tbChannel.videoChannel,
                                                   1236, 1237, "127.0.0.1");
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSourceFilter(tbChannel.videoChannel,
                                                   rtpPort, rtcpPort, ipAddress);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSourceFilter(tbChannel.videoChannel, 0,
                                                   0, NULL);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSourceFilter(tbChannel.videoChannel,
                                                   rtpPort, rtcpPort, ipAddress);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }
    {
        tbVideoChannel tbChannel(ViE, numberOfErrors); // Create a video channel
        error = ViE.ptrViENetwork->SetLocalReceiver(tbChannel.videoChannel,
                                                    1234);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        int DSCP = 0;
        bool useSetSockOpt = false;
        // SetSockOpt should work without a locally bind socket
        error = ViE.ptrViENetwork->GetSendToS(tbChannel.videoChannel, DSCP,
                                              useSetSockOpt); // No ToS set
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(DSCP == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        // Invalid input
        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, -1, true);        
		numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        // Invalid input
        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, 64, true);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        // Valid
        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, 20, true);

        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendToS(tbChannel.videoChannel, DSCP,
                                              useSetSockOpt);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError((DSCP == 20 && useSetSockOpt
            == true), "ERROR: %s at line %d", __FUNCTION__, __LINE__);
        // Disable
        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, 0, true);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendToS(tbChannel.videoChannel, DSCP,
                                              useSetSockOpt);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(DSCP == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        char myIpAddress[64];
        memset(myIpAddress, 0, 64);
        // Get local ip to be able to set ToS withtou setSockOpt
        error = ViE.ptrViENetwork->GetLocalIP(myIpAddress, false);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetLocalReceiver(tbChannel.videoChannel,
                                                    1234, 1235, myIpAddress);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        // Invalid input
        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, -1,
                                              false);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, 64,
                                              false); // Invalid input
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendToS(tbChannel.videoChannel, DSCP,
                                              useSetSockOpt); // No ToS set
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(DSCP == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, 20,
                                              false); // Valid
        if (error != 0)
        {
            ViETest::Log("ViESetSendToS error!.");
            ViETest::Log("You must be admin to run these tests.");
            ViETest::Log("On Win7 and late Vista, you need to right click the "
                         "exe and choose");
            ViETest::Log("\"Run as administrator\"\n");
            getchar();
        }
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendToS(tbChannel.videoChannel, DSCP,
                                              useSetSockOpt);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
#ifdef _WIN32
        numberOfErrors += ViETest::TestError((DSCP == 20
                                             && useSetSockOpt == false),
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
#else // useSetSockOpt is true on Linux and Mac
        numberOfErrors += ViETest::TestError((DSCP == 20
                                             && useSetSockOpt == true),
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
#endif
        error = ViE.ptrViENetwork->SetSendToS(tbChannel.videoChannel, 0, false);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendToS(tbChannel.videoChannel, DSCP,
                                              useSetSockOpt);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(DSCP == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }
    {
        // From qos.h. (*) -> supported by ViE
        //
        //  #define SERVICETYPE_NOTRAFFIC               0x00000000
        //  #define SERVICETYPE_BESTEFFORT              0x00000001 (*)
        //  #define SERVICETYPE_CONTROLLEDLOAD          0x00000002 (*)
        //  #define SERVICETYPE_GUARANTEED              0x00000003 (*)
        //  #define SERVICETYPE_NETWORK_UNAVAILABLE     0x00000004
        //  #define SERVICETYPE_GENERAL_INFORMATION     0x00000005
        //  #define SERVICETYPE_NOCHANGE                0x00000006
        //  #define SERVICETYPE_NONCONFORMING           0x00000009
        //  #define SERVICETYPE_NETWORK_CONTROL         0x0000000A
        //  #define SERVICETYPE_QUALITATIVE             0x0000000D (*)
        //
        //  #define SERVICE_BESTEFFORT                  0x80010000
        //  #define SERVICE_CONTROLLEDLOAD              0x80020000
        //  #define SERVICE_GUARANTEED                  0x80040000
        //  #define SERVICE_QUALITATIVE                 0x80200000

        tbVideoChannel tbChannel(ViE, numberOfErrors); // Create a video channel


#if defined(_WIN32)
        // No socket
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_BESTEFFORT);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViENetwork->SetLocalReceiver(tbChannel.videoChannel,
                                                    1234);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        // Sender not initialized
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_BESTEFFORT);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendDestination(tbChannel.videoChannel,
                                                      "127.0.0.1", 12345);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Try to set all non-supported service types
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_NOTRAFFIC);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_NETWORK_UNAVAILABLE);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_GENERAL_INFORMATION);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_NOCHANGE);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_NONCONFORMING);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_NOTRAFFIC);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_NETWORK_CONTROL);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICE_BESTEFFORT);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICE_CONTROLLEDLOAD);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICE_GUARANTEED);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICE_QUALITATIVE);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Loop through valid service settings
        bool enabled = false;
        int serviceType = 0;
        int overrideDSCP = 0;

        error = ViE.ptrViENetwork->GetSendGQoS(tbChannel.videoChannel, enabled,
                                               serviceType, overrideDSCP);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(enabled == false,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_BESTEFFORT);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendGQoS(tbChannel.videoChannel, enabled,
                                               serviceType, overrideDSCP);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError((enabled == true && serviceType
            == SERVICETYPE_BESTEFFORT && overrideDSCP == false),
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_CONTROLLEDLOAD);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendGQoS(tbChannel.videoChannel, enabled,
                                               serviceType, overrideDSCP);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError((enabled == true && serviceType
            == SERVICETYPE_CONTROLLEDLOAD && overrideDSCP == false),
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_GUARANTEED);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendGQoS(tbChannel.videoChannel, enabled,
                                               serviceType, overrideDSCP);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(
            (enabled == true
             && serviceType == SERVICETYPE_GUARANTEED
             && overrideDSCP == false),
             "ERROR: %s at line %d", __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, true,
                                               SERVICETYPE_QUALITATIVE);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendGQoS(tbChannel.videoChannel, enabled,
                                               serviceType, overrideDSCP);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError((enabled == true && serviceType
            == SERVICETYPE_QUALITATIVE && overrideDSCP == false),
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetSendGQoS(tbChannel.videoChannel, false,
                                               SERVICETYPE_QUALITATIVE);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->GetSendGQoS(tbChannel.videoChannel, enabled,
                                               serviceType, overrideDSCP);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError(enabled == false,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
#endif
    }
    {
        //
        // MTU and packet burst
        //
        // Create a video channel
        tbVideoChannel tbChannel(ViE, numberOfErrors);
        // Invalid input
        error = ViE.ptrViENetwork->SetMTU(tbChannel.videoChannel, 1600);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        // Invalid input
        error = ViE.ptrViENetwork->SetMTU(tbChannel.videoChannel, 800);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        //
        // Observer and timeout
        //
        ViEAutoTestNetworkObserver vieTestObserver;
        error = ViE.ptrViENetwork->RegisterObserver(tbChannel.videoChannel,
                                                    vieTestObserver);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->RegisterObserver(tbChannel.videoChannel,
                                                    vieTestObserver);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetPeriodicDeadOrAliveStatus(
            tbChannel.videoChannel, true); // No observer
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->DeregisterObserver(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViENetwork->DeregisterObserver(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViENetwork->SetPeriodicDeadOrAliveStatus(
            tbChannel.videoChannel, true); // No observer
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        // Packet timout notification
        error = ViE.ptrViENetwork->SetPacketTimeoutNotification(
            tbChannel.videoChannel, true, 10);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }
#if 0
    virtual int SendUDPPacket(const int videoChannel, const void* data,
                              const unsigned int length, int& transmittedBytes,
                              bool useRtcpSocket = false) = 0;
#endif

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************


    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViENetwork API Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViENetwork API Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}
