/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "TestStereo.h"

#include "common_types.h"
#include "audio_coding_module_typedefs.h"
#include "engine_configurations.h"
#include <iostream>
#include "utility.h"
#include <cassert>
#include "trace.h"


// Class for simulating packet handling
TestPackStereo::TestPackStereo():
_receiverACM(NULL),
_seqNo(0),
_timeStampDiff(0),
_lastInTimestamp(0),
_totalBytes(0),
_payloadSize(0),
_noChannels(1),
_codecType(0)
{
}
TestPackStereo::~TestPackStereo()
{
}

void 
TestPackStereo::RegisterReceiverACM(AudioCodingModule* acm)
{
    _receiverACM = acm;
    return;
}


WebRtc_Word32 
TestPackStereo::SendData(
        const FrameType       frameType,
        const WebRtc_UWord8   payloadType,
        const WebRtc_UWord32  timeStamp,
        const WebRtc_UWord8*  payloadData, 
        const WebRtc_UWord16  payloadSize,
        const RTPFragmentationHeader* fragmentation)
{
    WebRtcRTPHeader rtpInfo;
    WebRtc_Word32   status;
    WebRtc_UWord16  payloadDataSize = payloadSize;
    WebRtc_UWord8 payloadDataMaster[60 * 32 * 2 * 2]; 
    WebRtc_UWord8 payloadDataSlave[60 * 32 * 2 * 2]; 
    bool twoBytePerSample = false;
    bool oneBytePerSample = true;
    bool frameBased = false;

    rtpInfo.header.markerBit = false;
    rtpInfo.header.ssrc = 0;
    rtpInfo.header.sequenceNumber = _seqNo++;
    rtpInfo.header.payloadType = payloadType;
    rtpInfo.header.timestamp = timeStamp;
    if(frameType == kFrameEmpty)
    {
        // Skip this frame
        return 0;
    }    
    if(frameType != kAudioFrameCN)
    {
        rtpInfo.type.Audio.isCNG = false;

        // For stereo we need to call ACM with two incoming packets, one for each channel.
        // Different packet-splitting depending on codec.
        if (_codecType == 0) {
            // one byte per sample
            for (int i=0, j=0; i<payloadDataSize; i+=2, j++)
            {
                payloadDataMaster[j] = payloadData[i];
                payloadDataSlave[j] = payloadData[i+1];
            }
        } else if (_codecType == 1) {
            // two bytes per sample
            for (int i=0, j=0; i<payloadDataSize; i+=4, j+=2)
            {
                payloadDataMaster[j] = payloadData[i];
                payloadDataMaster[j+1] = payloadData[i+1];
                payloadDataSlave[j] = payloadData[i+2];
                payloadDataSlave[j+1] = payloadData[i+3];
            }
        } else if (_codecType == 2) {
            // frameBased
            memcpy(payloadDataMaster, &payloadData[0], payloadDataSize/2);
            memcpy(payloadDataSlave, &payloadData[payloadDataSize/2], payloadDataSize/2);
        } else if (_codecType == 3) {
            // four bits per sample
            for (int i=0, j=0; i<payloadDataSize; i+=2, j++)
            {
                payloadDataMaster[j] = (payloadData[i] & 0xF0) + (payloadData[i+1] >> 4);
                payloadDataSlave[j] = ((payloadData[i] & 0x0F) << 4) + (payloadData[i+1] & 0x0F);
            }
        }
    }
    else
    {
        // If CNG packet, send the same packet to both master and slave.
        rtpInfo.type.Audio.isCNG = true;
        memcpy(payloadDataMaster, payloadData, payloadSize);
        memcpy(payloadDataSlave, payloadData, payloadSize);
        payloadDataSize = payloadSize*2;
    }

    // Call ACM with two packets, one for each channel
    rtpInfo.type.Audio.channel = 1;
    status = _receiverACM->IncomingPacket((WebRtc_Word8*)payloadDataMaster, payloadDataSize/2, rtpInfo);
    rtpInfo.type.Audio.channel = 2;
    status = _receiverACM->IncomingPacket((WebRtc_Word8*)payloadDataSlave, payloadDataSize/2, rtpInfo);

    if (frameType != kAudioFrameCN) {
        _payloadSize = payloadDataSize;
    } else {
        _payloadSize = -1;
    }
    _timeStampDiff = timeStamp - _lastInTimestamp;
    _lastInTimestamp = timeStamp;
    _totalBytes += payloadDataSize;
    return status;
}

WebRtc_UWord16
TestPackStereo::GetPayloadSize()
{
    return _payloadSize;
}


WebRtc_UWord32
TestPackStereo::GetTimeStampDiff()
{
    return _timeStampDiff;
}

void 
TestPackStereo::ResetPayloadSize()
{
    _payloadSize = 0;
}

void 
TestPackStereo::SetCodecType(int codecType)
{
    _codecType = codecType;
}

TestStereo::TestStereo(int testMode):
_acmA(NULL),
_acmB(NULL),
_channelA2B(NULL),
_testCntr(0),
_packSizeSamp(0),
_packSizeBytes(0),
_counter(0)
{
    // testMode = 0 for silent test (auto test)
    _testMode = testMode;
}

using namespace std;
TestStereo::~TestStereo()
{
    if(_acmA != NULL)
    {
        AudioCodingModule::Destroy(_acmA);
        _acmA = NULL;
    }
    if(_acmB != NULL)
    {
        AudioCodingModule::Destroy(_acmB);
        _acmB = NULL;
    }
    if(_channelA2B != NULL)
    {
        delete _channelA2B;
        _channelA2B = NULL;
    }
}

void TestStereo::Perform()
{
     char fileName[500];
     WebRtc_UWord16 frequencyHz;

     if(_testMode == 0)
      {
          printf("Running Stereo Test");
          WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceAudioCoding, -1,
                       "---------- TestStereo ----------");
      }

     strcpy(fileName, "./modules/audio_coding/main/test/teststereo32kHz.pcm");
     frequencyHz = 32000;

    _inFileA.Open(fileName, frequencyHz, "rb");
    _inFileA.ReadStereo(true);

    _acmA = AudioCodingModule::Create(0);
    _acmB = AudioCodingModule::Create(1);

    _acmA->InitializeReceiver();
    _acmB->InitializeReceiver();

    WebRtc_UWord8 numEncoders = _acmA->NumberOfCodecs();
    CodecInst myCodecParam;
 
    for(WebRtc_UWord8 n = 0; n < numEncoders; n++)
    {
        _acmB->Codec(n, myCodecParam);
        if(!strcmp(myCodecParam.plname, "L16") || 
            !strcmp(myCodecParam.plname, "PCMA")|| 
            !strcmp(myCodecParam.plname, "PCMU")|| 
            !strcmp(myCodecParam.plname, "G722"))
        {
            myCodecParam.channels=2;
            _acmB->RegisterReceiveCodec(myCodecParam);
        }
    }

    // Create and connect the channel
    _channelA2B = new TestPackStereo;    
    _acmA->RegisterTransportCallback(_channelA2B);
    _channelA2B->RegisterReceiverACM(_acmB);
    
    // All codecs are tested for all allowed sampling frequencies, rates and packet sizes
#ifdef WEBRTC_CODEC_G722
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _channelA2B->SetCodecType(3);
    _testCntr++;
    OpenOutFile(_testCntr);
    char codecG722[] = "G722";
    RegisterSendCodec('A', codecG722, 16000, 64000, 160);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG722, 16000, 64000, 320);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG722, 16000, 64000, 480);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG722, 16000, 64000, 640);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG722, 16000, 64000, 800);
    Run(_channelA2B);
    RegisterSendCodec('A', codecG722, 16000, 64000, 960);
    Run(_channelA2B);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecG722, 16000, 64000, 320);
    Run(_channelA2B);
    _acmA->SetVAD(false, false, VADNormal);
    _outFileB.Close();
#endif
#ifdef WEBRTC_CODEC_PCM16
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _channelA2B->SetCodecType(1);
    _testCntr++; 
    OpenOutFile(_testCntr);
    char codecL16[] = "L16";
    RegisterSendCodec('A', codecL16, 8000, 128000, 80);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 8000, 128000, 160);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 8000, 128000, 240);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 8000, 128000, 320);
    Run(_channelA2B);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecL16, 8000, 128000, 80);
    Run(_channelA2B);
    _acmA->SetVAD(false, false, VADNormal);
    _outFileB.Close();
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;  
    OpenOutFile(_testCntr);
    RegisterSendCodec('A', codecL16, 16000, 256000, 160);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 16000, 256000, 320);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 16000, 256000, 480);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 16000, 256000, 640);
    Run(_channelA2B);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecL16, 16000, 256000, 160);
    Run(_channelA2B);
    _acmA->SetVAD(false, false, VADNormal);
    _outFileB.Close();
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++; 
    OpenOutFile(_testCntr);
    RegisterSendCodec('A', codecL16, 32000, 512000, 320);
    Run(_channelA2B);
    RegisterSendCodec('A', codecL16, 32000, 512000, 640);
    Run(_channelA2B);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecL16, 32000, 512000, 320);
    Run(_channelA2B);
    _acmA->SetVAD(false, false, VADNormal);
    _outFileB.Close();
#endif
#define PCMA_AND_PCMU
#ifdef PCMA_AND_PCMU
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _channelA2B->SetCodecType(0);
    _testCntr++; 
    OpenOutFile(_testCntr);
    char codecPCMA[] = "PCMA";
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 80);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 160);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 240);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 320);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 400);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 480);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecPCMA, 8000, 64000, 80);
    Run(_channelA2B);
    _acmA->SetVAD(false, false, VADNormal);
    _outFileB.Close();
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
    _testCntr++;
    OpenOutFile(_testCntr);
    Run(_channelA2B);
    char codecPCMU[] = "PCMU";
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 80);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 160);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 240);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 320);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 400);
    Run(_channelA2B);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 480);
    _acmA->SetVAD(true, true, VADNormal);
    RegisterSendCodec('A', codecPCMU, 8000, 64000, 80);
    Run(_channelA2B);
    _acmA->SetVAD(false, false, VADNormal);
    Run(_channelA2B);
    _outFileB.Close();
    if(_testMode != 0) {
        printf("=======================================================================\n");
    } else {
        printf(".");
    }
#endif

    /* Print out which codecs were tested, and which were not, in the run */

    if(_testMode != 0) {
        printf("The following codecs was INCLUDED in the test:\n");
#ifdef WEBRTC_CODEC_G722
        printf("   G.722\n");
#endif
#ifdef WEBRTC_CODEC_PCM16
        printf("   PCM16\n");
#endif
        printf("   G.711\n");

        printf("\nTo complete the test, listen to the %d number of output files.\n", _testCntr);
    } else {
        printf("Done!\n");
    }
}

// Register Codec to use in the test
//
// Input:   side            - which ACM to use, 'A' or 'B'
//          codecName       - name to use when register the codec
//          samplingFreqHz  - sampling frequency in Herz
//          rate            - bitrate in bytes
//          packSize        - packet size in samples
//          extraByte       - if extra bytes needed compared to the bitrate 
//                            used when registering, can be an internal header
//                            set to -1 if the codec is a variable rate codec
WebRtc_Word16 TestStereo::RegisterSendCodec(char side, 
                                          char* codecName, 
                                          WebRtc_Word32 samplingFreqHz,
                                          int rate,
                                          int packSize)
{
    if(_testMode != 0) {
        // Print out codec and settings
        printf("codec: %s Freq: %d Rate: %d PackSize: %d", codecName, samplingFreqHz, rate, packSize);
    }

    // Store packetsize in samples, used to validate the recieved packet
    _packSizeSamp = packSize;

    // Store the expected packet size in bytes, used to validate the recieved packet
    // Add 0.875 to always round up to a whole byte
    _packSizeBytes = (WebRtc_UWord16)((float)(packSize*rate)/(float)(samplingFreqHz*8)+0.875);

    // Set pointer to the ACM where to register the codec
    AudioCodingModule* myACM;
    switch(side)
    {
    case 'A':
        {
            myACM = _acmA;
            break;
        }
    case 'B':
        {
            myACM = _acmB;
            break;
        }
    default:
        return -1;
    }

    if(myACM == NULL)
    {
        assert(false);
        return -1;
    }
    CodecInst myCodecParam;

    // Get all codec paramters before registering
    CHECK_ERROR(AudioCodingModule::Codec(codecName, myCodecParam, samplingFreqHz));
    myCodecParam.rate = rate;
    myCodecParam.pacsize = packSize;
    myCodecParam.channels = 2;
    CHECK_ERROR(myACM->RegisterSendCodec(myCodecParam));

    // initialization was succesful
    return 0;
}

void TestStereo::Run(TestPackStereo* channel)
{
    AudioFrame audioFrame;

    WebRtc_UWord16 SamplesIn10MsecA = _inFileA.PayloadLength10Ms();
    WebRtc_UWord32 timestampA = 1;
    WebRtc_Word32 outFreqHzB = _outFileB.SamplingFrequency();
    WebRtc_UWord16 recSize;
    WebRtc_UWord32 timeStampDiff;
    channel->ResetPayloadSize();
    int errorCount = 0;

    // Only run 1 second for each test case
    while((_counter<1000)&& (!_inFileA.EndOfFile()))
    {
        // Add 10 msec to ACM
         _inFileA.Read10MsData(audioFrame);
        CHECK_ERROR(_acmA->Add10MsData(audioFrame));

        // Run sender side of ACM
        CHECK_ERROR(_acmA->Process());

        // Verify that the received packet size matches the settings
        recSize = channel->GetPayloadSize();
        if ((0<recSize) & (recSize<65535)) {
            if ((recSize != _packSizeBytes*2) && (_packSizeBytes < 65535)) {
                errorCount++;
            }

            // Verify that the timestamp is updated with expected length
            timeStampDiff = channel->GetTimeStampDiff();
            if ((_counter > 10) && (timeStampDiff != _packSizeSamp)) {
                errorCount++;
            }
        }

        // Run received side of ACM
        CHECK_ERROR(_acmB->PlayoutData10Ms(outFreqHzB, audioFrame));

        // Write output speech to file
        _outFileB.Write10MsData(audioFrame._payloadData, audioFrame._payloadDataLengthInSamples*audioFrame._audioChannel);
    }

    if (errorCount) 
    {
        printf(" - test FAILED\n");
    } 
    else if(_testMode != 0)
    {
        printf(" - test PASSED\n");
    }

    // Reset _counter
    if (_counter == 1000) {
        _counter = 0;
    }
    if (_inFileA.EndOfFile()) {
        _inFileA.Rewind();
    }
}

void TestStereo::OpenOutFile(WebRtc_Word16 testNumber)
{
    char fileName[500] = "./modules/audio_coding/main/test/res_tests/teststereo_out_";
    char cntrStr[10];

    sprintf(cntrStr, "%02d.pcm", testNumber);
    strcat(fileName, cntrStr);

    _outFileB.Open(fileName, 32000, "wb");
}

void TestStereo::DisplaySendReceiveCodec()
{
    CodecInst myCodecParam;
    _acmA->SendCodec(myCodecParam);
    if(_testMode != 0) {
        printf("%s -> ", myCodecParam.plname);
    }
    _acmB->ReceiveCodec(myCodecParam);
    if(_testMode != 0) {
        printf("%s\n", myCodecParam.plname);
    }
}

