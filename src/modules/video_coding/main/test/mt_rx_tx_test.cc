/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*************************************************
 *
 * Testing multi thread - receive and send sides
 *
 **************************************************/

#include "receiver_tests.h" // shared RTP state and receive side threads
#include "video_coding.h"
#include "rtp_rtcp.h"
#include "thread_wrapper.h"
#include "../source/event.h"
#include "test_util.h" // send side callback
#include "media_opt_test.h"

#include <string.h>

using namespace webrtc;

bool
MainSenderThread(void* obj)
{
    SendSharedState* state = static_cast<SendSharedState*>(obj);
    EventWrapper& waitEvent = *EventWrapper::Create();
    // preparing a frame for encoding
    VideoFrame sourceFrame;
    WebRtc_Word32 width = state->_args.width;
    WebRtc_Word32 height = state->_args.height;
    float frameRate = state->_args.frameRate;
    WebRtc_Word32 lengthSourceFrame  = 3*width*height/2;
    sourceFrame.VerifyAndAllocate(lengthSourceFrame);
    WebRtc_UWord8* tmpBuffer = new WebRtc_UWord8[lengthSourceFrame];

    if (state->_sourceFile == NULL)
    {
        state->_sourceFile = fopen(state->_args.inputFile.c_str(), "rb");
        if (state->_sourceFile == NULL)
        {
            printf ("Error when opening file \n");
            delete &waitEvent;
            delete tmpBuffer;
            return false;
        }
    }
    if (feof(state->_sourceFile) == 0)
    {
        fread(tmpBuffer, 1, lengthSourceFrame,state->_sourceFile);
        state->_frameCnt++;
        sourceFrame.CopyFrame(lengthSourceFrame, tmpBuffer);
        sourceFrame.SetHeight(height);
        sourceFrame.SetWidth(width);
        state->_timestamp += (WebRtc_UWord32)(9e4 / frameRate);
        sourceFrame.SetTimeStamp(state->_timestamp);

        WebRtc_Word32 ret = state->_vcm.AddVideoFrame(sourceFrame);
        if (ret < 0)
        {
            printf("Add Frame error: %d\n", ret);
            delete &waitEvent;
            delete tmpBuffer;
            return false;
        }
        waitEvent.Wait(33);
    }

    delete &waitEvent;
    delete tmpBuffer;

    return true;
}

bool
IntSenderThread(void* obj)
{
    SendSharedState* state = static_cast<SendSharedState*>(obj);
    state->_vcm.SetChannelParameters(1000,30,0);

    return true;
}


int MTRxTxTest(CmdArgs& args)
{
    /* TEST SETTINGS */
    std::string   inname = args.inputFile;
    std::string outname;
    if (args.outputFile == "")
        outname = "../MTRxTxTest_decoded.yuv";
    else
        outname = args.outputFile;

    WebRtc_UWord16  width = args.width;
    WebRtc_UWord16  height = args.height;
    WebRtc_UWord32  lengthSourceFrame  = 3*width*height/2;

    float         frameRate = args.frameRate;
    float         bitRate = args.bitRate;
    WebRtc_Word32   numberOfCores = 1;

    // error resilience/network
    // Nack support is currently not implemented in this test.
    bool          nackEnabled = false;
    bool          fecEnabled = false;
    WebRtc_UWord8   rttMS = 20;
    float         lossRate = 0.0*255; // no packet loss
    WebRtc_UWord32  renderDelayMs = 0;
    WebRtc_UWord32  minPlayoutDelayMs = 0;
    WebRtc_UWord8   deltaFECRate = 0;
    WebRtc_UWord8   keyFECRate = 0;

    /* TEST SET-UP */

    // Set up trace
    Trace::CreateTrace();
    Trace::SetTraceFile("MTRxTxTestTrace.txt");
    Trace::SetLevelFilter(webrtc::kTraceAll);

    FILE* sourceFile;
    FILE* decodedFile;

    if ((sourceFile = fopen(inname.c_str(), "rb")) == NULL)
    {
        printf("Cannot read file %s.\n", inname.c_str());
        return -1;
    }

    if ((decodedFile = fopen(outname.c_str(), "wb")) == NULL)
    {
        printf("Cannot read file %s.\n", outname.c_str());
        return -1;
    }

    //RTP
    RtpRtcp* rtp = RtpRtcp::CreateRtpRtcp(1, false);
    if (rtp->InitReceiver() < 0)
    {
        return -1;
    }
    if (rtp->InitSender() < 0)
    {
        return -1;
    }
    // registering codecs for the RTP module
    TEST(rtp->RegisterReceivePayload("ULPFEC", VCM_ULPFEC_PAYLOAD_TYPE) == 0);
    TEST(rtp->RegisterReceivePayload("RED", VCM_RED_PAYLOAD_TYPE) == 0);
    TEST(rtp->RegisterReceivePayload(args.codecName.c_str(), VCM_VP8_PAYLOAD_TYPE) == 0);

    // inform RTP Module of error resilience features
    TEST(rtp->SetGenericFECStatus(fecEnabled, VCM_RED_PAYLOAD_TYPE, VCM_ULPFEC_PAYLOAD_TYPE) == 0);

    TEST(rtp->RegisterSendPayload(args.codecName.c_str(), VCM_VP8_PAYLOAD_TYPE, 90000, 1, 10000) == 0);

    //VCM
    VideoCodingModule* vcm = VideoCodingModule::Create(1);
    if (vcm->InitializeReceiver() < 0)
    {
        return -1;
    }
    if (vcm->InitializeSender())
    {
        return -1;
    }
    // registering codecs for the VCM module
    VideoCodec sendCodec;
    vcm->InitializeSender();
    WebRtc_Word32 numberOfCodecs = vcm->NumberOfCodecs();
    if (numberOfCodecs < 1)
    {
        return -1;
    }

    if (vcm->Codec(args.codecType, &sendCodec) != 0)
    {
        // desired codec unavailable
        printf("Codec not registered\n");
        return -1;
    }
    // register codec
    sendCodec.startBitrate = (int) bitRate;
    sendCodec.height = height;
    sendCodec.width = width;
    sendCodec.maxFramerate = (WebRtc_UWord8)frameRate;
    vcm->RegisterSendCodec(&sendCodec, numberOfCores, 1440);
    vcm->RegisterReceiveCodec(&sendCodec, numberOfCores); // same settings for encode and decode

    vcm->SetRenderDelay(renderDelayMs);
    vcm->SetMinimumPlayoutDelay(minPlayoutDelayMs);

    // Callback Settings

    PacketRequester packetRequester(*rtp);
    vcm->RegisterPacketRequestCallback(&packetRequester);

    VCMRTPEncodeCompleteCallback* encodeCompleteCallback = new VCMRTPEncodeCompleteCallback(rtp);
    vcm->RegisterTransportCallback(encodeCompleteCallback);
    encodeCompleteCallback->SetCodecType(ConvertCodecType(args.codecName.c_str()));
    encodeCompleteCallback->SetFrameDimensions(width, height);
    // frame ready to be sent to network
    RTPSendCompleteCallback* outgoingTransport = new RTPSendCompleteCallback(rtp, "dump.rtp");
    rtp->RegisterSendTransport(outgoingTransport);
    // FrameReceiveCallback
    VCMDecodeCompleteCallback receiveCallback(decodedFile);
    RtpDataCallback dataCallback(vcm);
    rtp->RegisterIncomingDataCallback(&dataCallback);
    vcm->RegisterReceiveCallback(&receiveCallback);

    VCMTestProtectionCallback protectionCallback;
    vcm->RegisterProtectionCallback(&protectionCallback);

    outgoingTransport->SetLossPct(lossRate);
    // Nack support is currently not implemented in this test
    assert(nackEnabled == false);
    vcm->SetVideoProtection(kProtectionNack, nackEnabled);
    vcm->SetVideoProtection(kProtectionFEC, fecEnabled);

    // inform RTP Module of error resilience features
    rtp->SetFECCodeRate(protectionCallback.FECKeyRate(),
                        protectionCallback.FECDeltaRate());
    rtp->SetNACKStatus(protectionCallback.NACKMethod());

    vcm->SetChannelParameters((WebRtc_UWord32) bitRate,
                              (WebRtc_UWord8) lossRate, rttMS);

    SharedRTPState mtState(*vcm, *rtp); // receive side
    SendSharedState mtSendState(*vcm, *rtp, args); // send side

    /*START TEST*/

    // Create and start all threads
    // send side threads
    ThreadWrapper* mainSenderThread = ThreadWrapper::CreateThread(MainSenderThread,
            &mtSendState, kNormalPriority, "MainSenderThread");
    ThreadWrapper* intSenderThread = ThreadWrapper::CreateThread(IntSenderThread,
            &mtSendState, kNormalPriority, "IntThread");

    if (MainSenderThread != NULL)
    {
        unsigned int tid;
        mainSenderThread->Start(tid);
    }
    else
    {
        printf("Unable to start main sender thread\n");
        return -1;
    }

    if (IntSenderThread != NULL)
    {
        unsigned int tid;
        intSenderThread->Start(tid);
    }
    else
    {
        printf("Unable to start sender interference thread\n");
        return -1;
    }

    // Receive side threads
    ThreadWrapper* processingThread = ThreadWrapper::CreateThread(ProcessingThread,
            &mtState, kNormalPriority, "ProcessingThread");
    ThreadWrapper* decodeThread = ThreadWrapper::CreateThread(DecodeThread,
            &mtState, kNormalPriority, "DecodeThread");

    if (processingThread != NULL)
    {
        unsigned int tid;
        processingThread->Start(tid);
    }
    else
    {
        printf("Unable to start processing thread\n");
        return -1;
    }

    if (decodeThread != NULL)
    {
        unsigned int tid;
        decodeThread->Start(tid);
    }
    else
    {
        printf("Unable to start decode thread\n");
        return -1;
    }

    EventWrapper& waitEvent = *EventWrapper::Create();

    // Decode for 10 seconds and then tear down and exit.
    waitEvent.Wait(30000);

    // Tear down

    while (!mainSenderThread->Stop())
    {
        ;
    }

    while (!intSenderThread->Stop())
    {
        ;
    }


    while (!processingThread->Stop())
    {
        ;
    }

    while (!decodeThread->Stop())
    {
        ;
    }

    delete &waitEvent;
    delete mainSenderThread;
    delete intSenderThread;
    delete processingThread;
    delete decodeThread;
    delete encodeCompleteCallback;
    delete outgoingTransport;
    VideoCodingModule::Destroy(vcm);
    RtpRtcp::DestroyRtpRtcp(rtp);
    rtp = NULL;
    vcm = NULL;
    Trace::ReturnTrace();
    fclose(decodedFile);
    printf("Multi-Thread test Done: View output file \n");
    return 0;

}

