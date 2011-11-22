/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "tb_video_channel.h"

tbVideoChannel::tbVideoChannel(TbInterfaces& Engine, int& nrOfErrors,
                               webrtc::VideoCodecType sendCodec, int width,
                               int height, int frameRate, int startBitrate) :
    videoChannel(-1), numberOfErrors(nrOfErrors), ViE(Engine)
{
    int error;
    error = ViE.base->CreateChannel(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    webrtc::VideoCodec videoCodec;
    memset(&videoCodec, 0, sizeof(webrtc::VideoCodec));
    bool sendCodecSet = false;
    for (int idx = 0; idx < ViE.codec->NumberOfCodecs(); idx++)
    {
        error = ViE.codec->GetCodec(idx, videoCodec);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
        videoCodec.width = width;
        videoCodec.height = height;
        videoCodec.maxFramerate = frameRate;

        if (videoCodec.codecType == sendCodec && sendCodecSet == false)
        {
            if(videoCodec.codecType != webrtc::kVideoCodecI420 )
            {
                videoCodec.startBitrate = startBitrate;
                videoCodec.maxBitrate = startBitrate * 3;
            }
            error = ViE.codec->SetSendCodec(videoChannel, videoCodec);
            numberOfErrors += ViETest::TestError(error == 0,
                                                 "ERROR: %s at line %d",
                                                 __FUNCTION__, __LINE__);
            sendCodecSet = true;
        }
        if (videoCodec.codecType == webrtc::kVideoCodecVP8)
        {
            videoCodec.width = 352;
            videoCodec.height = 288;
        }
        error = ViE.codec->SetReceiveCodec(videoChannel, videoCodec);
        numberOfErrors += ViETest::TestError(error == 0,
                                             "ERROR: %s at line %d",
                                             __FUNCTION__, __LINE__);
    }
    numberOfErrors += ViETest::TestError(sendCodecSet == true,
                                         "ERROR: %s at line %d", __FUNCTION__,
                                         __LINE__);

}

tbVideoChannel::~tbVideoChannel(void)
{
    int error;
    error = ViE.base->DeleteChannel(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
}

void tbVideoChannel::StartSend(const unsigned short rtpPort /*= 11000*/,
                               const char* ipAddress /*= "127.0.0.1"*/)
{
    int error;
    error = ViE.network->SetSendDestination(videoChannel, ipAddress,
                                                  rtpPort);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.base->StartSend(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
}

void tbVideoChannel::SetFrameSettings(int width, int height, int frameRate)
{
    int error;
    webrtc::VideoCodec videoCodec;
    error = ViE.codec->GetSendCodec(videoChannel, videoCodec);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    videoCodec.width = width;
    videoCodec.height = height;
    videoCodec.maxFramerate = frameRate;

    error = ViE.codec->SetSendCodec(videoChannel, videoCodec);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.codec->SetReceiveCodec(videoChannel, videoCodec);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

}
void tbVideoChannel::StopSend()
{
    int error;
    error = ViE.base->StopSend(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
}

void tbVideoChannel::StartReceive(const unsigned short rtpPort /*= 11000*/)
{
    int error;

    error = ViE.network->SetLocalReceiver(videoChannel, rtpPort);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    error = ViE.base->StartReceive(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
}

void tbVideoChannel::StopReceive()
{
    int error;
    error = ViE.base->StopReceive(videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
}
