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
// vie_autotest_rtp_rtcp.cc
//
#include <iostream>

#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "engine_configurations.h"

#include "tb_capture_device.h"
#include "tb_external_transport.h"
#include "tb_interfaces.h"
#include "tb_video_channel.h"

class ViERtpObserver: public webrtc::ViERTPObserver
{
public:
    ViERtpObserver()
    {
    }
    virtual ~ViERtpObserver()
    {
    }

    virtual void IncomingSSRCChanged(const int videoChannel,
                                     const unsigned int SSRC)
    {
    }
    virtual void IncomingCSRCChanged(const int videoChannel,
                                     const unsigned int CSRC, const bool added)
    {
    }
};

class ViERtcpObserver: public webrtc::ViERTCPObserver
{
public:
    int _channel;
    unsigned char _subType;
    unsigned int _name;
    char* _data;
    unsigned short _dataLength;

    ViERtcpObserver() :
        _channel(-1),
        _subType(0),
        _name(-1),
        _data(NULL),
        _dataLength(0)
    {
    }
    ~ViERtcpObserver()
    {
        if (_data)
        {
            delete[] _data;
        }
    }
    virtual void OnApplicationDataReceived(
        const int videoChannel, const unsigned char subType,
        const unsigned int name, const char* data,
        const unsigned short dataLengthInBytes)
    {
        _channel = videoChannel;
        _subType = subType;
        _name = name;
        if (dataLengthInBytes > _dataLength)
        {
            delete[] _data;
            _data = NULL;
        }
        if (_data == NULL)
        {
            _data = new char[dataLengthInBytes];
        }
        memcpy(_data, data, dataLengthInBytes);
        _dataLength = dataLengthInBytes;
    }
};

int ViEAutoTest::ViERtpRtcpStandardTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViERTP_RTCP Standard Test\n");

    //***************************************************************
    //  Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************

    int error = 0;
    int numberOfErrors = 0;

    // Create VIE
    tbInterfaces ViE("ViERtpRtcpStandardTest", numberOfErrors);
    // Create a video channel
    tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);

    // Create a capture device
    tbCaptureDevice tbCapture(ViE, numberOfErrors);
    tbCapture.ConnectTo(tbChannel.videoChannel);

    ViETest::Log("\n");
    tbExternalTransport myTransport(*(ViE.ptrViENetwork));

    error = ViE.ptrViENetwork->RegisterSendTransport(tbChannel.videoChannel,
                                                     myTransport);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //***************************************************************
    //  Engine ready. Begin testing class
    //***************************************************************

    unsigned short startSequenceNumber = 12345;
    ViETest::Log("Set start sequence number: %u", startSequenceNumber);
    error = ViE.ptrViERtpRtcp->SetStartSequenceNumber(tbChannel.videoChannel,
                                                      startSequenceNumber);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    myTransport.EnableSequenceNumberCheck();

    error = ViE.ptrViEBase->StartReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    AutoTestSleep(2000);

    unsigned short receivedSequenceNumber =
        myTransport.GetFirstSequenceNumber();
    ViETest::Log("First received sequence number: %u\n",
                 receivedSequenceNumber);
    numberOfErrors += ViETest::TestError(
        receivedSequenceNumber == startSequenceNumber, "ERROR: %s at line %d",
        __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StopSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //
    // RTCP CName
    //
    ViETest::Log("Testing CName\n");
    const char* sendCName = "ViEAutoTestCName\0";
    error = ViE.ptrViERtpRtcp->SetRTCPCName(tbChannel.videoChannel, sendCName);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    char returnCName[webrtc::ViERTP_RTCP::KMaxRTCPCNameLength];
    memset(returnCName, 0, webrtc::ViERTP_RTCP::KMaxRTCPCNameLength);
    error = ViE.ptrViERtpRtcp->GetRTCPCName(tbChannel.videoChannel,
                                            returnCName);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError((strcmp(sendCName, returnCName) == 0),
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    AutoTestSleep(1000);

    char remoteCName[webrtc::ViERTP_RTCP::KMaxRTCPCNameLength];
    memset(remoteCName, 0, webrtc::ViERTP_RTCP::KMaxRTCPCNameLength);
    error = ViE.ptrViERtpRtcp->GetRemoteRTCPCName(tbChannel.videoChannel,
                                                  remoteCName);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError((strcmp(sendCName, remoteCName) == 0),
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    //
    //  Statistics
    //
    // Stop and restart to clear stats
    ViETest::Log("Testing statistics\n");
    error = ViE.ptrViEBase->StopReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StopSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    myTransport.ClearStats();
    int rate = 20;
    myTransport.SetPacketLoss(rate);

    // Start send to verify sending stats

    error = ViE.ptrViERtpRtcp->SetStartSequenceNumber(tbChannel.videoChannel,
                                                      startSequenceNumber);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StartReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    AutoTestSleep(KAutoTestSleepTimeMs);

    unsigned short sentFractionsLost = 0;
    unsigned int sentCumulativeLost = 0;
    unsigned int sentExtendedMax = 0;
    unsigned int sentJitter = 0;
    int sentRttMs = 0;
    unsigned short recFractionsLost = 0;
    unsigned int recCumulativeLost = 0;
    unsigned int recExtendedMax = 0;
    unsigned int recJitter = 0;
    int recRttMs = 0;

    unsigned int sentTotalBitrate = 0;
    unsigned int sentVideoBitrate = 0;
    unsigned int sentFecBitrate = 0;
    unsigned int sentNackBitrate = 0;

    error = ViE.ptrViERtpRtcp->GetBandwidthUsage(tbChannel.videoChannel,
                                                 sentTotalBitrate,
                                                 sentVideoBitrate,
                                                 sentFecBitrate,
                                                 sentNackBitrate);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    numberOfErrors += ViETest::TestError(sentTotalBitrate > 0 &&
                                         sentFecBitrate == 0 &&
                                         sentNackBitrate == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StopReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    AutoTestSleep(2000);

    error = ViE.ptrViERtpRtcp->GetSentRTCPStatistics(tbChannel.videoChannel,
                                                     sentFractionsLost,
                                                     sentCumulativeLost,
                                                     sentExtendedMax,
                                                     sentJitter, sentRttMs);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError((sentCumulativeLost > 0
        && sentExtendedMax > startSequenceNumber && sentJitter > 0 && sentRttMs
        > 0), "ERROR: %s at line %d", __FUNCTION__, __LINE__);

    error = ViE.ptrViERtpRtcp->GetReceivedRTCPStatistics(tbChannel.videoChannel,
                                                         recFractionsLost,
                                                         recCumulativeLost,
                                                         recExtendedMax,
                                                         recJitter, recRttMs);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(
        (recCumulativeLost > 0
            && recExtendedMax > startSequenceNumber
            && recJitter > 0
            && recRttMs > 0),
            "ERROR: %s at line %d", __FUNCTION__, __LINE__);
    // Check that rec stats extended max is greater than what we've sent.
    numberOfErrors += ViETest::TestError(recExtendedMax >= sentExtendedMax,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ViE.ptrViEBase->StopSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //
    // Test bandwidth statistics with NACK and FEC separately
    //

    myTransport.ClearStats();
    myTransport.SetPacketLoss(rate);

    error = ViE.ptrViERtpRtcp->SetFECStatus(tbChannel.videoChannel,
                                            true, 96, 97);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StartReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViERtpRtcp->GetBandwidthUsage(tbChannel.videoChannel,
                                                 sentTotalBitrate,
                                                 sentVideoBitrate,
                                                 sentFecBitrate,
                                                 sentNackBitrate);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    numberOfErrors += ViETest::TestError(sentTotalBitrate > 0 &&
                                         sentFecBitrate > 0 &&
                                         sentNackBitrate == 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StopSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERtpRtcp->SetFECStatus(tbChannel.videoChannel,
                                            false, 96, 97);
    error = ViE.ptrViERtpRtcp->SetNACKStatus(tbChannel.videoChannel, true);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViERtpRtcp->GetBandwidthUsage(tbChannel.videoChannel,
                                                 sentTotalBitrate,
                                                 sentVideoBitrate,
                                                 sentFecBitrate,
                                                 sentNackBitrate);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    numberOfErrors += ViETest::TestError(sentTotalBitrate > 0 &&
                                         sentFecBitrate == 0 &&
                                         sentNackBitrate > 0,
                                         "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);


    error = ViE.ptrViEBase->StopReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERtpRtcp->SetNACKStatus(tbChannel.videoChannel, false);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //
    // Keepalive
    //
    ViETest::Log("Testing RTP keep alive...\n");
    error = ViE.ptrViEBase->StopSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEBase->StartReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    myTransport.SetPacketLoss(0);
    myTransport.ClearStats();

    const char keepAlivePT = 109;
    unsigned int deltaTimeSeconds = 2;
    error = ViE.ptrViERtpRtcp->SetRTPKeepAliveStatus(tbChannel.videoChannel,
                                                     true, keepAlivePT,
                                                     deltaTimeSeconds);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViERtpRtcp->SetRTPKeepAliveStatus(tbChannel.videoChannel,
                                                     false, keepAlivePT,
                                                     deltaTimeSeconds);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    WebRtc_Word32 numRtpPackets = 0;
    WebRtc_Word32 numDroppedPackets = 0;
    WebRtc_Word32 numRtcpPackets = 0;
    myTransport.GetStats(numRtpPackets, numDroppedPackets, numRtcpPackets);
    WebRtc_Word32 expectedPackets = KAutoTestSleepTimeMs / (1000 *
        static_cast<WebRtc_Word32>(deltaTimeSeconds));
    numberOfErrors += ViETest::TestError(numRtpPackets == expectedPackets,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    // Test to set SSRC
    unsigned int setSSRC = 0x01234567;
    ViETest::Log("Set SSRC %u", setSSRC);
    error = ViE.ptrViERtpRtcp->SetLocalSSRC(tbChannel.videoChannel, setSSRC);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StartReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    myTransport.EnableSSRCCheck();

    AutoTestSleep(2000);
    unsigned int receivedSSRC = myTransport.ReceivedSSRC();
    ViETest::Log("Received SSRC %u\n", receivedSSRC);
    numberOfErrors += ViETest::TestError(setSSRC == receivedSSRC,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    unsigned int localSSRC = 0;
    error = ViE.ptrViERtpRtcp->GetLocalSSRC(tbChannel.videoChannel, localSSRC);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(localSSRC == setSSRC,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    unsigned int remoteSSRC = 0;
    error
        = ViE.ptrViERtpRtcp->GetRemoteSSRC(tbChannel.videoChannel, remoteSSRC);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(remoteSSRC == setSSRC,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    error = ViE.ptrViEBase->StopSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);


    ViETest::Log("Testing RTP dump...\n");

#ifdef WEBRTC_ANDROID
    const char* inDumpName = "/sdcard/IncomingRTPDump.rtp";
    const char* outDumpName = "/sdcard/OutgoingRTPDump.rtp";
#else
    const char* inDumpName = "IncomingRTPDump.rtp";
    const char* outDumpName = "OutgoingRTPDump.rtp";
#endif

    error = ViE.ptrViERtpRtcp->StartRTPDump(tbChannel.videoChannel, inDumpName,
                                            webrtc::kRtpIncoming);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERtpRtcp->StartRTPDump(tbChannel.videoChannel,
                                            outDumpName, webrtc::kRtpOutgoing);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    AutoTestSleep(KAutoTestSleepTimeMs);

    error = ViE.ptrViEBase->StopSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    AutoTestSleep(1000);

    error = ViE.ptrViERtpRtcp->StopRTPDump(tbChannel.videoChannel,
                                           webrtc::kRtpIncoming);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERtpRtcp->StopRTPDump(tbChannel.videoChannel,
                                           webrtc::kRtpOutgoing);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Make sure data was actuall saved to the file and we stored the same
    // amount of data in both files
    FILE* inDump = fopen(inDumpName, "r");
    fseek(inDump, 0L, SEEK_END);
    long inEndPos = ftell(inDump);
    fclose(inDump);
    FILE* outDump = fopen(outDumpName, "r");
    fseek(outDump, 0L, SEEK_END);
    long outEndPos = ftell(outDump);
    fclose(outDump);

    numberOfErrors += ViETest::TestError((inEndPos > 0
                                          && inEndPos < outEndPos + 100),
                                          "ERROR: %s at line %d", __FUNCTION__,
                                          __LINE__);

    // Deregister external transport
    error = ViE.ptrViENetwork->DeregisterSendTransport(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //***************************************************************
    //  Testing finished. Tear down Video Engine
    //***************************************************************


    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViERTP_RTCP Standard Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViERTP_RTCP Standard Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViERtpRtcpExtendedTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViERTP_RTCP Extended Test\n");

    //***************************************************************
    //  Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************


    int error = 0;
    int numberOfErrors = 0;

    numberOfErrors = ViERtpRtcpStandardTest();

    // Create VIE
    tbInterfaces ViE("ViERtpRtcpStandardTest", numberOfErrors);
    // Create a video channel
    tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);
    // Create a capture device
    tbCaptureDevice tbCapture(ViE, numberOfErrors);
    tbCapture.ConnectTo(tbChannel.videoChannel);

    //tbChannel.StartReceive(rtpPort);
    //tbChannel.StartSend(rtpPort);
    tbExternalTransport myTransport(*(ViE.ptrViENetwork));

    error = ViE.ptrViENetwork->RegisterSendTransport(tbChannel.videoChannel,
                                                     myTransport);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StartReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //***************************************************************
    //  Engine ready. Begin testing class
    //***************************************************************


    //
    // Application specific RTCP
    //
    //

    ViERtcpObserver rtcpObserver;
    error = ViE.ptrViERtpRtcp->RegisterRTCPObserver(tbChannel.videoChannel,
                                                    rtcpObserver);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    unsigned char subType = 3;
    unsigned int name = static_cast<unsigned int> (0x41424344); // 'ABCD';
    const char* data = "ViEAutoTest Data of length 32 --";
    const unsigned short numBytes = 32;

    error = ViE.ptrViERtpRtcp->SendApplicationDefinedRTCPPacket(
        tbChannel.videoChannel, subType, name, data, numBytes);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    ViETest::Log("Sending RTCP application data...\n");
    AutoTestSleep(KAutoTestSleepTimeMs);
    numberOfErrors += ViETest::TestError(
        (subType = rtcpObserver._subType
         && !strncmp(data, rtcpObserver._data, 32)
         && name == rtcpObserver._name
         && numBytes == rtcpObserver._dataLength),
         "ERROR: %s at line %d", __FUNCTION__, __LINE__);
    ViETest::Log("\t RTCP application data received\n");

    //***************************************************************
    //  Testing finished. Tear down Video Engine
    //***************************************************************


    error = ViE.ptrViEBase->StopReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEBase->StopSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViENetwork->DeregisterSendTransport(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViERTP_RTCP Extended Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViERTP_RTCP Extended Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViERtpRtcpAPITest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViERTP_RTCP API Test\n");

    //***************************************************************
    //  Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************


    int error = 0;
    int numberOfErrors = 0;

    // Create VIE
    tbInterfaces ViE("ViERtpRtcpAPITest", numberOfErrors);
    // Create a video channel
    tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);
    // Create a capture device
    tbCaptureDevice tbCapture(ViE, numberOfErrors);
    tbCapture.ConnectTo(tbChannel.videoChannel);

    //***************************************************************
    //  Engine ready. Begin testing class
    //***************************************************************


    //
    // Check different RTCP modes
    //
    webrtc::ViERTCPMode rtcpMode = webrtc::kRtcpNone;
    error = ViE.ptrViERtpRtcp->GetRTCPStatus(tbChannel.videoChannel, rtcpMode);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(
        rtcpMode == webrtc::kRtcpCompound_RFC4585,
        "ERROR: %s at line %d", __FUNCTION__, __LINE__);
    error = ViE.ptrViERtpRtcp->SetRTCPStatus(tbChannel.videoChannel,
                                             webrtc::kRtcpCompound_RFC4585);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERtpRtcp->GetRTCPStatus(tbChannel.videoChannel, rtcpMode);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(
        rtcpMode == webrtc::kRtcpCompound_RFC4585,
        "ERROR: %s at line %d", __FUNCTION__, __LINE__);
    error = ViE.ptrViERtpRtcp->SetRTCPStatus(tbChannel.videoChannel,
                                             webrtc::kRtcpNonCompound_RFC5506);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERtpRtcp->GetRTCPStatus(tbChannel.videoChannel, rtcpMode);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(
        rtcpMode == webrtc::kRtcpNonCompound_RFC5506, "ERROR: %s at line %d",
        __FUNCTION__, __LINE__);
    error = ViE.ptrViERtpRtcp->SetRTCPStatus(tbChannel.videoChannel,
                                             webrtc::kRtcpNone);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERtpRtcp->GetRTCPStatus(tbChannel.videoChannel, rtcpMode);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError(rtcpMode == webrtc::kRtcpNone,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);
    error = ViE.ptrViERtpRtcp->SetRTCPStatus(tbChannel.videoChannel,
                                             webrtc::kRtcpCompound_RFC4585);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //
    // CName is testedn in SimpleTest
    // Start sequence number is tested in SimplTEst
    //
    const char* testCName = "ViEAutotestCName";
    error = ViE.ptrViERtpRtcp->SetRTCPCName(tbChannel.videoChannel, testCName);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    char returnCName[256];
    memset(returnCName, 0, 256);
    error
        = ViE.ptrViERtpRtcp->GetRTCPCName(tbChannel.videoChannel, returnCName);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    numberOfErrors += ViETest::TestError((strcmp(testCName, returnCName) == 0),
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

    //
    // SSRC
    //
    error = ViE.ptrViERtpRtcp->SetLocalSSRC(tbChannel.videoChannel, 0x01234567);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERtpRtcp->SetLocalSSRC(tbChannel.videoChannel, 0x76543210);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    unsigned int ssrc = 0;
    error = ViE.ptrViERtpRtcp->GetLocalSSRC(tbChannel.videoChannel, ssrc);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.ptrViERtpRtcp->SetStartSequenceNumber(tbChannel.videoChannel,
                                                      1000);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    tbChannel.StartSend();
    error = ViE.ptrViERtpRtcp->SetStartSequenceNumber(tbChannel.videoChannel,
                                                      12345);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    tbChannel.StopSend();

    //
    // Start sequence number
    //
    error = ViE.ptrViERtpRtcp->SetStartSequenceNumber(tbChannel.videoChannel,
                                                      12345);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERtpRtcp->SetStartSequenceNumber(tbChannel.videoChannel,
                                                      1000);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    tbChannel.StartSend();
    error = ViE.ptrViERtpRtcp->SetStartSequenceNumber(tbChannel.videoChannel,
                                                      12345);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    tbChannel.StopSend();

    //
    // Application specific RTCP
    //
    {
        unsigned char subType = 3;
        unsigned int name = static_cast<unsigned int> (0x41424344); // 'ABCD';
        const char* data = "ViEAutoTest Data of length 32 --";
        const unsigned short numBytes = 32;

        tbChannel.StartSend();
        error = ViE.ptrViERtpRtcp->SendApplicationDefinedRTCPPacket(
            tbChannel.videoChannel, subType, name, data, numBytes);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->SendApplicationDefinedRTCPPacket(
            tbChannel.videoChannel, subType, name, NULL, numBytes);
        // NULL input
        numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->SendApplicationDefinedRTCPPacket(
            tbChannel.videoChannel, subType, name, data, numBytes - 1);
        // incorrect length
        numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->GetRTCPStatus(tbChannel.videoChannel,
                                                 rtcpMode);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->SendApplicationDefinedRTCPPacket(
            tbChannel.videoChannel, subType, name, data, numBytes);
        // RTCP off
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->SetRTCPStatus(tbChannel.videoChannel,
                                                 webrtc::kRtcpCompound_RFC4585);
        numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        tbChannel.StopSend();
        error = ViE.ptrViERtpRtcp->SendApplicationDefinedRTCPPacket(
            tbChannel.videoChannel, subType, name, data, numBytes);
        // Not sending
        numberOfErrors += ViETest::TestError(error != 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }

    //
    // Statistics
    //
    // Tested in SimpleTest(), we'll get errors if we haven't received a RTCP
    // packet.

    //
    // RTP Keepalive
    //
    {
        char setPT = 123;
        unsigned int setDeltaTime = 10;
        bool enabled = false;
        char getPT = 0;
        unsigned int getDeltaTime = 0;
        error = ViE.ptrViERtpRtcp->SetRTPKeepAliveStatus(tbChannel.videoChannel,
                                                         true, 119);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->SetRTPKeepAliveStatus(tbChannel.videoChannel,
                                                         true, setPT,
                                                         setDeltaTime);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->SetRTPKeepAliveStatus(tbChannel.videoChannel,
                                                         false, setPT,
                                                         setDeltaTime);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->SetRTPKeepAliveStatus(tbChannel.videoChannel,
                                                         true, setPT,
                                                         setDeltaTime);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViERtpRtcp->GetRTPKeepAliveStatus(tbChannel.videoChannel,
                                                         enabled, getPT,
                                                         getDeltaTime);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        numberOfErrors += ViETest::TestError((enabled == true && setPT == getPT
                                              && setDeltaTime == getDeltaTime),
                                              "ERROR: %s at line %d",
                                              __FUNCTION__, __LINE__);

        error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        error = ViE.ptrViERtpRtcp->SetRTPKeepAliveStatus(tbChannel.videoChannel,
                                                         true, setPT,
                                                         setDeltaTime);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        tbChannel.StopSend();
        error = ViE.ptrViERtpRtcp->SetRTPKeepAliveStatus(tbChannel.videoChannel,
                                                         enabled, getPT, 0);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->SetRTPKeepAliveStatus(tbChannel.videoChannel,
                                                         enabled, getPT, 61);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }
    //
    // RTP Dump
    //
    {
#ifdef WEBRTC_ANDROID
        const char* dumpName = "/sdcard/DumpFileName.rtp";
#else
        const char* dumpName = "DumpFileName.rtp";
#endif
        error = ViE.ptrViERtpRtcp->StartRTPDump(tbChannel.videoChannel,
                                                dumpName, webrtc::kRtpIncoming);

        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->StopRTPDump(tbChannel.videoChannel,
                                               webrtc::kRtpIncoming);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->StopRTPDump(tbChannel.videoChannel,
                                               webrtc::kRtpIncoming);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->StartRTPDump(tbChannel.videoChannel,
                                                dumpName, webrtc::kRtpOutgoing);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->StopRTPDump(tbChannel.videoChannel,
                                               webrtc::kRtpOutgoing);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->StopRTPDump(tbChannel.videoChannel,
                                               webrtc::kRtpOutgoing);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->StartRTPDump(tbChannel.videoChannel,
                                                dumpName,
                                                (webrtc::RTPDirections) 3);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }
    //
    // RTP/RTCP Observers
    //
    {
        ViERtpObserver rtpObserver;
        error = ViE.ptrViERtpRtcp->RegisterRTPObserver(tbChannel.videoChannel,
                                                       rtpObserver);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->RegisterRTPObserver(tbChannel.videoChannel,
                                                       rtpObserver);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->DeregisterRTPObserver(
            tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->DeregisterRTPObserver(
            tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);

        ViERtcpObserver rtcpObserver;
        error = ViE.ptrViERtpRtcp->RegisterRTCPObserver(tbChannel.videoChannel,
                                                        rtcpObserver);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->RegisterRTCPObserver(tbChannel.videoChannel,
                                                        rtcpObserver);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->DeregisterRTCPObserver(
            tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->DeregisterRTCPObserver(
            tbChannel.videoChannel);
        numberOfErrors += ViETest::TestError(error == -1,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }
    //
    // PLI
    //
    {
        error = ViE.ptrViERtpRtcp->SetKeyFrameRequestMethod(
            tbChannel.videoChannel, webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->SetKeyFrameRequestMethod(
            tbChannel.videoChannel, webrtc::kViEKeyFrameRequestPliRtcp);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->SetKeyFrameRequestMethod(
            tbChannel.videoChannel, webrtc::kViEKeyFrameRequestNone);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        error = ViE.ptrViERtpRtcp->SetKeyFrameRequestMethod(
            tbChannel.videoChannel, webrtc::kViEKeyFrameRequestNone);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }
    //
    // NACK
    //
    {
        error = ViE.ptrViERtpRtcp->SetNACKStatus(tbChannel.videoChannel, true);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }

    //***************************************************************
    //  Testing finished. Tear down Video Engine
    //***************************************************************

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViERTP_RTCP API Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViERTP_RTCP API Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}
