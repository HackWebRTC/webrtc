/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ENCODEDECODETEST_H
#define ENCODEDECODETEST_H

#include "EncodeToFileTest.h"

#define MAX_INCOMING_PAYLOAD 8096
#include "audio_coding_module.h"

class Receiver
{
public:
    Receiver();
    void Setup(AudioCodingModule *acm, RTPStream *rtpStream);
    void Teardown();
    void Run();
    bool IncomingPacket();
    bool PlayoutData();    

    //for auto_test and logging
    WebRtc_UWord8             codeId;
    WebRtc_UWord8             testMode;

private:
    AudioCodingModule*    _acm;
    bool                  _rtpEOF;
    RTPStream*            _rtpStream;
    PCMFile               _pcmFile;
    WebRtc_Word16*        _playoutBuffer;
    WebRtc_UWord16        _playoutLengthSmpls;
    WebRtc_Word8          _incomingPayload[MAX_INCOMING_PAYLOAD];
    WebRtc_UWord16        _payloadSizeBytes;
    WebRtc_UWord16        _realPayloadSizeBytes;
    WebRtc_Word32         _frequency;
    bool                  _firstTime;
    WebRtcRTPHeader       _rtpInfo;
    WebRtc_UWord32        _nextTime;
};

class EncodeDecodeTest : public EncodeToFileTest
{
public:
    EncodeDecodeTest();
    EncodeDecodeTest(int testMode);
    virtual void Perform();
    WebRtc_UWord16            _playoutFreq;    
    WebRtc_UWord8             _testMode;
protected:
    Receiver    _receiver;    
};      



#endif

