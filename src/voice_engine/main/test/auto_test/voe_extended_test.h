/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_EXTENDED_TEST_H
#define WEBRTC_VOICE_ENGINE_VOE_EXTENDED_TEST_H

#include "voe_standard_test.h"

namespace voetest {

class VoETestManager;

// ----------------------------------------------------------------------------
//	Transport
// ----------------------------------------------------------------------------

class ExtendedTestTransport : public Transport
{
public:
    ExtendedTestTransport(VoENetwork* ptr);
    ~ExtendedTestTransport();
    VoENetwork* myNetw;
protected:
    virtual int SendPacket(int channel,const void *data,int len);
    virtual int SendRTCPPacket(int channel, const void *data, int len);
private:
    static bool Run(void* ptr);
    bool Process();
private:
    ThreadWrapper* _thread;
    CriticalSectionWrapper* _lock;
    EventWrapper* _event;
private:
    unsigned char _packetBuffer[1612];
    int _length;
    int _channel;
};

class XTransport : public Transport
{
public:
    XTransport(VoENetwork* netw, VoEFile* file);
    VoENetwork* _netw;
    VoEFile* _file;
public:
    virtual int SendPacket(int channel, const void *data, int len);
    virtual int SendRTCPPacket(int channel, const void *data, int len);
};

class XRTPObserver : public VoERTPObserver
{
public:
    XRTPObserver();
    ~XRTPObserver();
    virtual void OnIncomingCSRCChanged(const int channel,
                                       const unsigned int CSRC,
                                       const bool added);
	virtual void OnIncomingSSRCChanged(const int channel,
	                                   const unsigned int SSRC);
public:
    unsigned int _SSRC;
};

// ----------------------------------------------------------------------------
//	VoEExtendedTest
// ----------------------------------------------------------------------------

class VoEExtendedTest : public VoiceEngineObserver,
                        public VoEConnectionObserver
{
public:
    VoEExtendedTest(VoETestManager& mgr);
    ~VoEExtendedTest();
    int PrepareTest(const char* str) const;
    int TestPassed(const char* str) const;
    int TestBase();
    int TestCallReport();
    int TestCodec();
    int TestDtmf();
    int TestEncryption();
    int TestExternalMedia();
    int TestFile();
    int TestMixing();
    int TestHardware();
    int TestNetEqStats();
    int TestNetwork();
    int TestRTP_RTCP();
    int TestVideoSync();
    int TestVolumeControl();
    int TestAPM();
public:
    int ErrorCode() const
    {
        return _errCode;
    }
    ;
    void ClearErrorCode()
    {
        _errCode = 0;
    }
    ;
protected:
    // from VoiceEngineObserver
    void CallbackOnError(const int errCode, const int channel);
    void CallbackOnTrace(const TraceLevel level,
                         const char* message,
                         const int length);
protected:
    // from VoEConnectionObserver
    void OnPeriodicDeadOrAlive(const int channel, const bool alive);
private:
    void Play(int channel,
              unsigned int timeMillisec,
              bool addFileAsMicrophone = false,
              bool addTimeMarker = false);
    void Sleep(unsigned int timeMillisec, bool addMarker = false);
    void StartMedia(int channel,
                    int rtpPort,
                    bool listen,
                    bool playout,
                    bool send);
    void StopMedia(int channel);
private:
    VoETestManager& _mgr;
private:
    int _errCode;
    bool _alive;
    bool _listening[32];
    bool _playing[32];
    bool _sending[32];
};

} //  namespace voetest
#endif // WEBRTC_VOICE_ENGINE_VOE_EXTENDED_TEST_H
