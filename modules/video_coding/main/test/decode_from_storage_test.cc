/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "receiver_tests.h"
#include "video_coding.h"
#include "rtp_rtcp.h"
#include "trace.h"
#include "tick_time.h"
#include "../source/event.h"
#include "test_macros.h"
#include "rtp_player.h"

using namespace webrtc;

class FrameStorageCallback : public VCMFrameStorageCallback
{
public:
    FrameStorageCallback(VideoCodingModule* vcm) : _vcm(vcm) {}

    WebRtc_Word32 StoreReceivedFrame(const EncodedVideoData& frameToStore)
    {
        _vcm->DecodeFromStorage(frameToStore);
        return VCM_OK;
    }

private:
    VideoCodingModule* _vcm;
};

int DecodeFromStorageTest(CmdArgs& args)
{
    // Make sure this test isn't executed without simulated clocks
#if !defined(TICK_TIME_DEBUG) || !defined(EVENT_DEBUG)
    return -1;
#endif
    // BEGIN Settings

    bool protectionEnabled = false;
    VCMVideoProtection protectionMethod = kProtectionNack;
    WebRtc_UWord32 rttMS = 100;
    float lossRate = 0.00f;
    bool reordering = false;
    WebRtc_UWord32 renderDelayMs = 0;
    WebRtc_UWord32 minPlayoutDelayMs = 0;
    const WebRtc_Word64 MAX_RUNTIME_MS = -1;
    std::string rtpFilename = args.inputFile;
    std::string outFilename = args.outputFile;
    if (outFilename == "")
        outFilename = "DecodeFromStorage.yuv";

    FrameReceiveCallback receiveCallback(outFilename.c_str());

    // END Settings

    Trace::CreateTrace();
    Trace::SetTraceFile("decodeFromStorageTestTrace.txt");
    Trace::SetLevelFilter(webrtc::kTraceAll);


    VideoCodingModule* vcm = VideoCodingModule::Create(1);
    VideoCodingModule* vcmPlayback = VideoCodingModule::Create(2);
    FrameStorageCallback storageCallback(vcmPlayback);
    RtpDataCallback dataCallback(vcm);
    WebRtc_Word32 ret = vcm->InitializeReceiver();
    if (ret < 0)
    {
        return -1;
    }
    ret = vcmPlayback->InitializeReceiver();
    if (ret < 0)
    {
        return -1;
    }
    vcm->RegisterFrameStorageCallback(&storageCallback);
    vcmPlayback->RegisterReceiveCallback(&receiveCallback);
    RTPPlayer rtpStream(rtpFilename.c_str(), &dataCallback);
    ListWrapper payloadTypes;
    payloadTypes.PushFront(new PayloadCodecTuple(VCM_VP8_PAYLOAD_TYPE, "VP8", kVideoCodecVP8));

    // Register receive codecs in VCM
    ListItem* item = payloadTypes.First();
    while (item != NULL)
    {
        PayloadCodecTuple* payloadType = static_cast<PayloadCodecTuple*>(item->GetItem());
        if (payloadType != NULL)
        {
            VideoCodec codec;
            memset(&codec, 0, sizeof(codec));
            strncpy(codec.plName, payloadType->name.c_str(), payloadType->name.length());
            codec.plName[payloadType->name.length()] = '\0';
            codec.plType = payloadType->payloadType;
            codec.codecType = payloadType->codecType;
            if (vcm->RegisterReceiveCodec(&codec, 1) < 0)
            {
                return -1;
            }
            if (vcmPlayback->RegisterReceiveCodec(&codec, 1) < 0)
            {
                return -1;
            }
        }
        item = payloadTypes.Next(item);
    }
    if (rtpStream.Initialize(payloadTypes) < 0)
    {
        return -1;
    }
    bool nackEnabled = protectionEnabled && (protectionMethod == kProtectionNack ||
                                            protectionMethod == kProtectionDualDecoder);
    rtpStream.SimulatePacketLoss(lossRate, nackEnabled, rttMS);
    rtpStream.SetReordering(reordering);
    vcm->SetChannelParameters(0, 0, rttMS);
    vcm->SetVideoProtection(protectionMethod, protectionEnabled);
    vcm->SetRenderDelay(renderDelayMs);
    vcm->SetMinimumPlayoutDelay(minPlayoutDelayMs);

    ret = 0;

    // RTP stream main loop
    while ((ret = rtpStream.NextPacket(VCMTickTime::MillisecondTimestamp())) == 0)
    {
        if (VCMTickTime::MillisecondTimestamp() % 5 == 0)
        {
            ret = vcm->Decode();
            if (ret < 0)
            {
                return -1;
            }
        }
        if (vcm->TimeUntilNextProcess() <= 0)
        {
            vcm->Process();
        }
        if (MAX_RUNTIME_MS > -1 && VCMTickTime::MillisecondTimestamp() >= MAX_RUNTIME_MS)
        {
            break;
        }
        VCMTickTime::IncrementDebugClock();
    }

    switch (ret)
    {
    case 1:
        printf("Success\n");
        break;
    case -1:
        printf("Failed\n");
        break;
    case 0:
        printf("Timeout\n");
        break;
    }

    rtpStream.Print();

    // Tear down
    item = payloadTypes.First();
    while (item != NULL)
    {
        PayloadCodecTuple* payloadType = static_cast<PayloadCodecTuple*>(item->GetItem());
        if (payloadType != NULL)
        {
            delete payloadType;
        }
        ListItem* itemToRemove = item;
        item = payloadTypes.Next(item);
        payloadTypes.Erase(itemToRemove);
    }
    VideoCodingModule::Destroy(vcm);
    vcm = NULL;
    VideoCodingModule::Destroy(vcmPlayback);
    vcmPlayback = NULL;
    Trace::ReturnTrace();

    return 0;
}
