/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOE_STANDARD_TEST_H
#define WEBRTC_VOICE_ENGINE_VOE_STANDARD_TEST_H

#include <string>

#include "voe_test_defines.h"
#include "voe_test_interface.h"

#include "voe_errors.h"
#include "voe_base.h"
#include "voe_file.h"
#include "voe_dtmf.h"
#include "voe_rtp_rtcp.h"
#include "voe_audio_processing.h"
#ifdef WEBRTC_VOICE_ENGINE_CALL_REPORT_API
#include "voe_call_report.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_CODEC_API
#include "voe_codec.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API
#include "voe_encryption.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
#include "voe_external_media.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_HARDWARE_API
#include "voe_hardware.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETWORK_API
#include "voe_network.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
#include "voe_video_sync.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API
#include "voe_volume_control.h"
#endif

#ifdef _TEST_NETEQ_STATS_
namespace webrtc
{
class CriticalSectionWrapper;
class ThreadWrapper;
class VoENetEqStats;
}
#endif

#if defined(WEBRTC_ANDROID)
extern char mobileLogMsg[640];
#endif

namespace voetest
{

void createSummary(VoiceEngine* ve);
void prepareDelivery();

class MyRTPObserver: public VoERTPObserver
{
public:
    MyRTPObserver();
    ~MyRTPObserver();
    virtual void OnIncomingCSRCChanged(const int channel,
                                       const unsigned int CSRC,
                                       const bool added);
    virtual void OnIncomingSSRCChanged(const int channel,
                                       const unsigned int SSRC);
    void Reset();
public:
    unsigned int _SSRC[2];
    unsigned int _CSRC[2][2]; // stores 2 SSRCs for each channel
    bool _added[2][2];
    int _size[2];
};

class MyTraceCallback: public TraceCallback
{
public:
    void Print(const TraceLevel level, const char *traceString,
               const int length);
};

class MyDeadOrAlive: public VoEConnectionObserver
{
public:
    void OnPeriodicDeadOrAlive(const int channel, const bool alive);
};

class ErrorObserver: public VoiceEngineObserver
{
public:
    ErrorObserver();
    void CallbackOnError(const int channel, const int errCode);
public:
    int code;
};

class RtcpAppHandler: public VoERTCPObserver
{
public:
    void OnApplicationDataReceived(const int channel,
                                   const unsigned char subType,
                                   const unsigned int name,
                                   const unsigned char* data,
                                   const unsigned short dataLengthInBytes);
    void Reset();
    ~RtcpAppHandler()
    {
    };
    unsigned short _lengthBytes;
    unsigned char _data[256];
    unsigned char _subType;
    unsigned int _name;
};

class DtmfCallback: public VoETelephoneEventObserver
{
public:
    int counter;
    DtmfCallback()
    {
        counter = 0;
    }
    virtual void OnReceivedTelephoneEventInband(int channel,
                                                int eventCode,
                                                bool endOfEvent)
    {
        char msg[128];
        if (endOfEvent)
            sprintf(msg, "(event=%d, [END])", eventCode);
        else
            sprintf(msg, "(event=%d, [START])", eventCode);
        TEST_LOG("%s", msg);
        if (!endOfEvent)
            counter++; // cound start of event only
        fflush(NULL);
    }

    virtual void OnReceivedTelephoneEventOutOfBand(
        int channel,
        int eventCode,
        bool endOfEvent)
    {
        char msg[128];
        if (endOfEvent)
            sprintf(msg, "(event=%d, [END])", eventCode);
        else
            sprintf(msg, "(event=%d, [START])", eventCode);
        TEST_LOG("%s", msg);
        if (!endOfEvent)
            counter++; // cound start of event only
        fflush(NULL);
    }
};

class my_encryption: public Encryption
{
    void encrypt(int channel_no, unsigned char * in_data,
                 unsigned char * out_data, int bytes_in, int * bytes_out);
    void decrypt(int channel_no, unsigned char * in_data,
                 unsigned char * out_data, int bytes_in, int * bytes_out);
    void encrypt_rtcp(int channel_no, unsigned char * in_data,
                      unsigned char * out_data, int bytes_in, int * bytes_out);
    void decrypt_rtcp(int channel_no, unsigned char * in_data,
                      unsigned char * out_data, int bytes_in, int * bytes_out);
};

class RxCallback: public VoERxVadCallback
{
public:
    RxCallback() :
        _vadDecision(-1)
    {
    };

    virtual void OnRxVad(int, int vadDecision)
    {
        char msg[128];
        sprintf(msg, "RX VAD detected decision %d \n", vadDecision);
        TEST_LOG("%s", msg);
        _vadDecision = vadDecision;
    }

    int _vadDecision;
};

#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
class MyMedia: public VoEMediaProcess
{
public:
    virtual void Process(const int channel, const ProcessingTypes type,
                         WebRtc_Word16 audio_10ms[], const int length,
                         const int samplingFreqHz, const bool stereo);
private:
    int f;
};
#endif

class SubAPIManager
{
public:
    SubAPIManager() :
        _base(true),
        _callReport(false),
        _codec(false),
        _dtmf(false),
        _encryption(false),
        _externalMedia(false),
        _file(false),
        _hardware(false),
        _netEqStats(false),
        _network(false),
        _rtp_rtcp(false),
        _videoSync(false),
        _volumeControl(false),
        _apm(false),
        _xsel(XSEL_Invalid)
    {
#ifdef WEBRTC_VOICE_ENGINE_CALL_REPORT_API
        _callReport = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_CODEC_API
        _codec = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_DTMF_API
        _dtmf = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API
        _encryption = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
        _externalMedia = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_FILE_API
        _file = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_HARDWARE_API
        _hardware = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
        _netEqStats = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETWORK_API
        _network = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_RTP_RTCP_API
        _rtp_rtcp = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
        _videoSync = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API
        _volumeControl = true;
#endif
#ifdef WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API
        _apm = true;
#endif
    };

    void DisplayStatus() const;
    bool GetExtendedMenuSelection(ExtendedSelection& sel);

private:
    bool _base, _callReport, _codec, _dtmf, _encryption;
    bool _externalMedia, _file, _hardware;
    bool _netEqStats, _network, _rtp_rtcp, _videoSync, _volumeControl, _apm;
    ExtendedSelection _xsel;
};

class VoETestManager
{
public:
    VoETestManager();
    ~VoETestManager();

    // Must be called after construction.
    bool Init();

    void GetInterfaces();
    int ReleaseInterfaces();
    int DoStandardTest();

    const char* AudioFilename() const
    {
        return audioFilename_.c_str();
    }

    VoiceEngine* VoiceEnginePtr() const
    {
        return ve;
    };
    VoEBase* BasePtr() const
    {
        return base;
    };
    VoECodec* CodecPtr() const
    {
        return codec;
    };
    VoEVolumeControl* VolumeControlPtr() const
    {
        return volume;
    };
    VoEDtmf* DtmfPtr() const
    {
        return dtmf;
    };
    VoERTP_RTCP* RTP_RTCPPtr() const
    {
        return rtp_rtcp;
    };
    VoEAudioProcessing* APMPtr() const
    {
        return apm;
    };
    VoENetwork* NetworkPtr() const
    {
        return netw;
    };
    VoEFile* FilePtr() const
    {
        return file;
    };
    VoEHardware* HardwarePtr() const
    {
        return hardware;
    };
    VoEVideoSync* VideoSyncPtr() const
    {
        return vsync;
    };
    VoEEncryption* EncryptionPtr() const
    {
        return encrypt;
    };
    VoEExternalMedia* ExternalMediaPtr() const
    {
        return xmedia;
    };
    VoECallReport* CallReportPtr() const
    {
        return report;
    };
#ifdef _TEST_NETEQ_STATS_
    VoENetEqStats* NetEqStatsPtr() const
    {
        return neteqst;
    };
#endif

private:
    bool initialized_;
    VoiceEngine* ve;
    VoEBase* base;
    VoECallReport* report;
    VoECodec* codec;
    VoEDtmf* dtmf;
    VoEEncryption* encrypt;
    VoEExternalMedia* xmedia;
    VoEFile* file;
    VoEHardware* hardware;
#ifdef _TEST_NETEQ_STATS_
    VoENetEqStats* neteqst;
#endif
    VoENetwork* netw;
    VoERTP_RTCP* rtp_rtcp;
    VoEVideoSync* vsync;
    VoEVolumeControl* volume;
    VoEAudioProcessing* apm;
    int instanceCount;
    std::string resourcePath_;
    std::string audioFilename_;
};

} // namespace voetest

#endif // WEBRTC_VOICE_ENGINE_VOE_STANDARD_TEST_H
