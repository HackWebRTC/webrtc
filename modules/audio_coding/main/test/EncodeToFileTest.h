/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ENCODETOFILETEST_H
#define ENCODETOFILETEST_H

#include "ACMTest.h"
#include "audio_coding_module.h"
#include "typedefs.h"
#include "RTPFile.h"
#include "PCMFile.h"
#include <stdio.h>

using namespace webrtc;

// TestPacketization callback which writes the encoded payloads to file
class TestPacketization : public AudioPacketizationCallback
{
public:
    TestPacketization(RTPStream *rtpStream, WebRtc_UWord16 frequency);
    ~TestPacketization();
    virtual WebRtc_Word32 SendData(const FrameType frameType,
        const WebRtc_UWord8 payloadType,
        const WebRtc_UWord32 timeStamp,
        const WebRtc_UWord8* payloadData, 
        const WebRtc_UWord16 payloadSize,
        const RTPFragmentationHeader* fragmentation);

private:
    static void MakeRTPheader(WebRtc_UWord8* rtpHeader, 
                              WebRtc_UWord8 payloadType, WebRtc_Word16 seqNo,
                              WebRtc_UWord32 timeStamp, WebRtc_UWord32 ssrc);
    RTPStream*      _rtpStream;
    WebRtc_Word32    _frequency;
    WebRtc_Word16     _seqNo;
};

class Sender
{
public:
    Sender();
    void Setup(AudioCodingModule *acm, RTPStream *rtpStream);
    void Teardown();
    void Run();
    bool Add10MsData();
    bool Process();

    //for auto_test and logging
    WebRtc_UWord8             testMode;
    WebRtc_UWord8             codeId;

private:
    AudioCodingModule*  _acm;
    PCMFile             _pcmFile;
    //WebRtc_Word16*    _payloadData;
    AudioFrame          _audioFrame;
    WebRtc_UWord16      _payloadSize;
    WebRtc_UWord32      _timeStamp;
    TestPacketization*  _packetization;
};

// Test class
class EncodeToFileTest : public ACMTest
{
public:
    EncodeToFileTest();
    virtual void Perform(int fileType, int codeId, int* codePars, int testMode);
protected:
    Sender _sender;
};

#endif
