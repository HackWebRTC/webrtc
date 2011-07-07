/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "EncodeToFileTest.h"
#include "audio_coding_module.h"
#include "common_types.h"

#ifdef WIN32
#   include <Winsock2.h>
#else
#   include <arpa/inet.h>
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TestPacketization::TestPacketization(RTPStream *rtpStream, WebRtc_UWord16 frequency)
:
_frequency(frequency),
_seqNo(0)
{
    _rtpStream = rtpStream;
}

TestPacketization::~TestPacketization()
{
}

WebRtc_Word32 TestPacketization::SendData(
    const FrameType       /* frameType */,
    const WebRtc_UWord8   payloadType,
    const WebRtc_UWord32  timeStamp,
    const WebRtc_UWord8*  payloadData, 
    const WebRtc_UWord16  payloadSize,
    const RTPFragmentationHeader* /* fragmentation */)
{
    _rtpStream->Write(payloadType, timeStamp, _seqNo++, payloadData, payloadSize, _frequency);
    //delete [] payloadData;
    return 1;
}

Sender::Sender()
:
_acm(NULL),
//_payloadData(NULL),
_payloadSize(0),
_timeStamp(0)
{
}

void Sender::Setup(AudioCodingModule *acm, RTPStream *rtpStream)
{
    acm->InitializeSender();
    struct CodecInst sendCodec;
    int noOfCodecs = acm->NumberOfCodecs();
    int codecNo;
    
    if (testMode == 1)
    {
        //set the codec, input file, and parameters for the current test    
        codecNo = codeId;
        //use same input file for now
        char fileName[] = "./modules/audio_coding/main/test/testfile32kHz.pcm";
        _pcmFile.Open(fileName, 32000, "rb");
    }
    else if (testMode == 0)
    {
        //set the codec, input file, and parameters for the current test    
        codecNo = codeId;
        acm->Codec(codecNo, sendCodec);
        //use same input file for now
        char fileName[] = "./modules/audio_coding/main/test/testfile32kHz.pcm";
        _pcmFile.Open(fileName, 32000, "rb");
    }
    else
    {
        printf("List of supported codec.\n");
        for(int n = 0; n < noOfCodecs; n++)
        {
            acm->Codec(n, sendCodec);
            printf("%d %s\n", n, sendCodec.plname);
        }
        printf("Choose your codec:");
    
        scanf("%d", &codecNo);
        char fileName[] = "./modules/audio_coding/main/test/testfile32kHz.pcm";
        _pcmFile.Open(fileName, 32000, "rb");
    }

    acm->Codec(codecNo, sendCodec);
    acm->RegisterSendCodec(sendCodec);
    _packetization = new TestPacketization(rtpStream, sendCodec.plfreq);
    if(acm->RegisterTransportCallback(_packetization) < 0)
    {
        printf("Registering Transport Callback failed, for run: codecId: %d: --\n",
                codeId);
    }

    _acm = acm;
}

void Sender::Teardown()
{
    _pcmFile.Close();
    delete _packetization;
}

bool Sender::Add10MsData()
{
    if (!_pcmFile.EndOfFile())
    {
        _pcmFile.Read10MsData(_audioFrame);
        WebRtc_Word32 ok = _acm->Add10MsData(_audioFrame);
        if (ok != 0)
        {
            printf("Error calling Add10MsData: for run: codecId: %d\n",
                codeId);
            exit(1);
        }
        //_audioFrame._timeStamp += _pcmFile.PayloadLength10Ms();
        return true;
    }
    return false;
}

bool Sender::Process()
{
    WebRtc_Word32 ok = _acm->Process();
    if (ok < 0)
    {
        printf("Error calling Add10MsData: for run: codecId: %d\n",
                codeId);
        exit(1);
    }
    return true;
}

void Sender::Run()
{
    while (true)
    {
        if (!Add10MsData())
        {
            break;
        }
        if (!Process()) // This could be done in a processing thread
        {
            break;
        }
    }
}

EncodeToFileTest::EncodeToFileTest()
{
}


void EncodeToFileTest::Perform(int fileType, int codeId, int* codePars, int testMode)
{
    AudioCodingModule *acm = AudioCodingModule::Create(0);
    RTPFile rtpFile;
    char fileName[] = "outFile.rtp";
    rtpFile.Open(fileName, "wb+");
    rtpFile.WriteHeader();

    //for auto_test and logging
    _sender.testMode = testMode;
    _sender.codeId = codeId;

    _sender.Setup(acm, &rtpFile);
    struct CodecInst sendCodecInst;
    if(acm->SendCodec(sendCodecInst) >= 0)
    {
        _sender.Run();
    }
    _sender.Teardown();
    rtpFile.Close();
    AudioCodingModule::Destroy(acm);
}
