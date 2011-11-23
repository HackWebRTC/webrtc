/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "EncodeDecodeTest.h"

#include <stdlib.h>
#include <string.h>

#include "common_types.h"
#include "gtest/gtest.h"
#include "trace.h"
#include "utility.h"

Receiver::Receiver()
:
_playoutLengthSmpls(WEBRTC_10MS_PCM_AUDIO),
_payloadSizeBytes(MAX_INCOMING_PAYLOAD)
{
}

void Receiver::Setup(AudioCodingModule *acm, RTPStream *rtpStream)
{
    struct CodecInst recvCodec;
    int noOfCodecs;
    acm->InitializeReceiver();

    noOfCodecs = acm->NumberOfCodecs();
    for (int i=0; i < noOfCodecs; i++)
    {
        acm->Codec((WebRtc_UWord8)i, recvCodec);      
        if (acm->RegisterReceiveCodec(recvCodec) != 0)
        {
            printf("Unable to register codec: for run: codecId: %d\n", codeId);
            exit(1);
        }
    }
     
    char filename[128];
    _rtpStream = rtpStream;
    int playSampFreq;

    if (testMode == 1)
    {
      playSampFreq=recvCodec.plfreq;
      //output file for current run
      sprintf(filename,"./src/modules/audio_coding/main/test/out%dFile.pcm",codeId);
      _pcmFile.Open(filename, recvCodec.plfreq, "wb+");
    }
    else if (testMode == 0)
    {
        playSampFreq=32000;
      //output file for current run
      sprintf(filename,"./src/modules/audio_coding/main/test/encodeDecode_out%d.pcm",codeId);
      _pcmFile.Open(filename, 32000/*recvCodec.plfreq*/, "wb+");
    }
    else
    {
        printf("\nValid output frequencies:\n");
        printf("8000\n16000\n32000\n-1, which means output freq equal to received signal freq");
        printf("\n\nChoose output sampling frequency: ");
        ASSERT_GT(scanf("%d", &playSampFreq), 0);
        char fileName[] = "./src/modules/audio_coding/main/test/outFile.pcm";
        _pcmFile.Open(fileName, 32000, "wb+");
    }
     
    _realPayloadSizeBytes = 0;
    _playoutBuffer = new WebRtc_Word16[WEBRTC_10MS_PCM_AUDIO];
    _frequency = playSampFreq;
    _acm = acm;
    _firstTime = true;
}

void Receiver::Teardown()
{
    delete [] _playoutBuffer;
    _pcmFile.Close();
    if (testMode > 1) Trace::ReturnTrace();
}

bool Receiver::IncomingPacket()
{
    if (!_rtpStream->EndOfFile())
    {
        if (_firstTime)
        {
            _firstTime = false;
            _realPayloadSizeBytes = _rtpStream->Read(&_rtpInfo, _incomingPayload, _payloadSizeBytes, &_nextTime);
            if (_realPayloadSizeBytes == 0 && _rtpStream->EndOfFile())
            {
                _firstTime = true;
                return true;
            }
        }
        
       WebRtc_Word32 ok = _acm->IncomingPacket(_incomingPayload, _realPayloadSizeBytes, _rtpInfo);
        if (ok != 0)
        {
            printf("Error when inserting packet to ACM, for run: codecId: %d\n", codeId);
            exit(1);
        }
        _realPayloadSizeBytes = _rtpStream->Read(&_rtpInfo, _incomingPayload, _payloadSizeBytes, &_nextTime);
        if (_realPayloadSizeBytes == 0 && _rtpStream->EndOfFile())
        {
            _firstTime = true;
        }
    }
    return true;
}

bool Receiver::PlayoutData()
{
    AudioFrame audioFrame;

    if (_acm->PlayoutData10Ms(_frequency, audioFrame) != 0)
    {
        printf("Error when calling PlayoutData10Ms, for run: codecId: %d\n", codeId);
        exit(1);
    }
    if (_playoutLengthSmpls == 0)
    {
        return false;
    }
    _pcmFile.Write10MsData(audioFrame._payloadData, audioFrame._payloadDataLengthInSamples);
    return true;
}

void Receiver::Run()
{
    WebRtc_UWord8 counter500Ms = 50;
    
    WebRtc_UWord32 clock = 0;

    while (counter500Ms > 0)
    {
        if (clock == 0 || clock >= _nextTime)
        {
            IncomingPacket();
            if (clock == 0)
            {
                clock = _nextTime;
            }
        }
        if ((clock % 10) == 0)
        {
            if (!PlayoutData())
            {
                clock++;
                continue;
            }
        }
        if (_rtpStream->EndOfFile())
        {
            counter500Ms--;
        }
        clock++;
    }
}

EncodeDecodeTest::EncodeDecodeTest()
{
    _testMode = 2;
    Trace::CreateTrace();
    Trace::SetTraceFile("acm_encdec_test.txt");
}

EncodeDecodeTest::EncodeDecodeTest(int testMode)
{
    //testMode == 0 for autotest
    //testMode == 1 for testing all codecs/parameters
    //testMode > 1 for specific user-input test (as it was used before)
   _testMode = testMode;
   if(_testMode != 0)
   {
       Trace::CreateTrace();
       Trace::SetTraceFile("acm_encdec_test.txt");
   }
}
void EncodeDecodeTest::Perform()
{

    if(_testMode == 0)
    {
        printf("Running Encode/Decode Test");
        WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceAudioCoding, -1, "---------- EncodeDecodeTest ----------");
    }

    int numCodecs = 1;
    int codePars[3]; //freq, pacsize, rate     
    int playoutFreq[3]; //8, 16, 32k   

    int numPars[52]; //number of codec parameters sets (rate,freq,pacsize)to test, for a given codec                    
        
    codePars[0]=0;
    codePars[1]=0;
    codePars[2]=0;

    if (_testMode == 1)
    {
        AudioCodingModule *acmTmp = AudioCodingModule::Create(0);
        struct CodecInst sendCodecTmp;
        numCodecs = acmTmp->NumberOfCodecs();
        printf("List of supported codec.\n");
        for(int n = 0; n < numCodecs; n++)
        {
            acmTmp->Codec(n, sendCodecTmp);
            if (STR_CASE_CMP(sendCodecTmp.plname, "telephone-event") == 0) {
                numPars[n] = 0;
            } else if (STR_CASE_CMP(sendCodecTmp.plname, "cn") == 0) {
                numPars[n] = 0;
            } else if (STR_CASE_CMP(sendCodecTmp.plname, "red") == 0) {
                numPars[n] = 0;
            } else {
                numPars[n] = 1;
                printf("%d %s\n", n, sendCodecTmp.plname);
            }
        }
        AudioCodingModule::Destroy(acmTmp);
        playoutFreq[1]=16000;
    }
    else if (_testMode == 0)
    {
        AudioCodingModule *acmTmp = AudioCodingModule::Create(0);
        numCodecs = acmTmp->NumberOfCodecs();
        AudioCodingModule::Destroy(acmTmp);
        struct CodecInst dummyCodec;

        //chose range of testing for codecs/parameters
        for(int i = 0 ; i < numCodecs ; i++)
        {
            numPars[i] = 1;
            acmTmp->Codec(i, dummyCodec);
            if (STR_CASE_CMP(dummyCodec.plname, "telephone-event") == 0)
            {
                numPars[i] = 0;
            } else if (STR_CASE_CMP(dummyCodec.plname, "cn") == 0) {
                numPars[i] = 0;
            } else if (STR_CASE_CMP(dummyCodec.plname, "red") == 0) {
                numPars[i] = 0;
            }
        }
        playoutFreq[1] = 16000;
    }
    else 
    {
        numCodecs = 1;
        numPars[0] = 1;
        playoutFreq[1]=16000;
    }

    _receiver.testMode = _testMode;

     //loop over all codecs:
     for(int codeId=0;codeId<numCodecs;codeId++)
     {
         //only encode using real encoders, not telephone-event anc cn
         for(int loopPars=1;loopPars<=numPars[codeId];loopPars++)
         {
             if (_testMode == 1)
             {
                 printf("\n");
                 printf("***FOR RUN: codeId: %d\n",codeId);
                 printf("\n");
             }
             else if (_testMode == 0)
             {
                 printf(".");
             }

             EncodeToFileTest::Perform(1, codeId, codePars, _testMode);

             AudioCodingModule *acm = AudioCodingModule::Create(10);
             RTPFile rtpFile;
             char fileName[] = "outFile.rtp";
             rtpFile.Open(fileName, "rb");

             _receiver.codeId = codeId;

             rtpFile.ReadHeader();
             _receiver.Setup(acm, &rtpFile);
             _receiver.Run();
             _receiver.Teardown();
             rtpFile.Close();
             AudioCodingModule::Destroy(acm);

             if (_testMode == 1)
             {
                 printf("***COMPLETED RUN FOR: codecID: %d ***\n",
                     codeId);
             }
        }
    }
    if (_testMode == 0)
    {
        printf("Done!\n");
    }
    if (_testMode == 1) Trace::ReturnTrace();
}

