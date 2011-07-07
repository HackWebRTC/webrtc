/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "engine_configurations.h"
#if defined(_WIN32)
#include <conio.h>     // exists only on windows
#include <tchar.h>
#endif

#include "voe_standard_test.h"

#if defined (_ENABLE_VISUAL_LEAK_DETECTOR_) && defined(_DEBUG) && defined(_WIN32) && !defined(_INSTRUMENTATION_TESTING_)
#include "vld.h"
#endif

#ifdef MAC_IPHONE
#include "../../source/voice_engine_defines.h"  // defines build macros
#else
#include "../../source/voice_engine_defines.h"  // defines build macros
#endif

#include "thread_wrapper.h"
#include "critical_section_wrapper.h"
#include "event_wrapper.h"

#ifdef _TEST_NETEQ_STATS_
#include "../../interface/voe_neteq_stats.h" // Not available in delivery folder
#endif

#include "voe_extended_test.h"
#include "voe_stress_test.h"
#include "voe_unit_test.h"
#include "voe_cpu_test.h"

using namespace webrtc;

namespace voetest{

#ifdef MAC_IPHONE
// Defined in iPhone specific test file
int GetDocumentsDir(char* buf, int bufLen);
char* GetFilename(char* filename);
const char* GetFilename(const char* filename);
int GetResource(char* resource, char* dest, int destLen);
char* GetResource(char* resource);
const char* GetResource(const char* resource);
// #ifdef MAC_IPHONE
#elif defined(WEBRTC_ANDROID)
char filenameStr[2][256] = {0};
int currentStr = 0;

char* GetFilename(char* filename)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr], "/sdcard/%s", filename); 
    return filenameStr[currentStr];
}
const char* GetFilename(const char* filename)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr], "/sdcard/%s", filename); 
    return filenameStr[currentStr];
}
int GetResource(char* resource, char* dest, int destLen)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr], "/sdcard/%s", resource);
    strncpy(dest, filenameStr[currentStr], destLen-1);
    return 0;
}
char* GetResource(char* resource)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr], "/sdcard/%s", resource); 
    return filenameStr[currentStr];
}
const char* GetResource(const char* resource)
{ 
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr], "/sdcard/%s", resource); 
    return filenameStr[currentStr];
}
#else
char filenameStr[2][256] = {0};
int currentStr = 0;

char* GetFilename(char* filename)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr],
            "/tmp/%s",
            filename);
    return filenameStr[currentStr];
}
const char* GetFilename(const char* filename)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr],
            "/tmp/%s",
            filename);
    return filenameStr[currentStr];
}
int GetResource(char* resource, char* dest, int destLen)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr],
            "/tmp/%s",
            resource);
    strncpy(dest, filenameStr[currentStr], destLen-1);
    return 0;
}
char* GetResource(char* resource)
{
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr],
            "/tmp/%s",
            resource);
    return filenameStr[currentStr];
}
const char* GetResource(const char* resource)
{ 
    currentStr = !currentStr;
    sprintf(filenameStr[currentStr],
            "/tmp/%s",
            resource);
    return filenameStr[currentStr];
}
#endif

#if defined(MAC_IPHONE)
char micFile[256] = {0}; // Filename copied to buffer in code
#elif defined(WEBRTC_MAC) && !defined(WEBRTC_MAC_INTEL)
const char* micFile = "audio_long16bigendian.pcm";
#elif defined(ANDROID)
const char* micFile = "/sdcard/audio_long16.pcm";
#else
const char* micFile =
    "/tmp/audio_long16.pcm";
#endif

#if !defined(MAC_IPHONE)
const char* summaryFilename =
    "/tmp/VoiceEngineSummary.txt";
#endif
// For iPhone the summary filename is created in createSummary

int dummy = 0; // Dummy used in different functions to avoid warnings

MyRTPObserver::MyRTPObserver()
{
    Reset();
}

MyRTPObserver::~MyRTPObserver()
{
}

void MyRTPObserver::Reset()
{
    for (int i = 0; i < 2; i++)
    {
        _SSRC[i] = 0;
        _CSRC[i][0] = 0;
        _CSRC[i][1] = 0;
        _added[i][0] = false;
        _added[i][1] = false;
        _size[i] = 0;
    }
}

void MyRTPObserver::OnIncomingCSRCChanged(const int channel,
                                          const unsigned int CSRC,
                                          const bool added)
{
    char msg[128];
    sprintf(msg, "=> OnIncomingCSRCChanged(channel=%d, CSRC=%u, added=%d)\n",
            channel, CSRC, added);
    TEST_LOG("%s", msg);

    if (channel > 1)
        return; // not enough memory

    _CSRC[channel][_size[channel]] = CSRC;
    _added[channel][_size[channel]] = added;

    _size[channel]++;
    if (_size[channel] == 2)
        _size[channel] = 0;
}

void MyRTPObserver::OnIncomingSSRCChanged(const int channel,
                                          const unsigned int SSRC)
{
    char msg[128];
    sprintf(msg,
            "\n=> OnIncomingSSRCChanged(channel=%d, SSRC=%u)\n",
            channel, SSRC);
    TEST_LOG("%s", msg);

    _SSRC[channel] = SSRC; 
}

void MyDeadOrAlive::OnPeriodicDeadOrAlive(const int /*channel*/,
                                          const bool alive)
{
    if (alive)
    {
        TEST_LOG("ALIVE\n");
    }
    else
    {
        TEST_LOG("DEAD\n");
    }
    fflush(NULL);
}

#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
void MyMedia::Process(const int channel,
                      const ProcessingTypes type,
                      WebRtc_Word16 audio_10ms[],
                      const int length, 
                      const int samplingFreqHz,
                      const bool stereo)
{
    for(int i = 0; i < length; i++)
    {
        if (!stereo)
        {
            audio_10ms[i] = (WebRtc_Word16)(audio_10ms[i] *
                sin(2.0 * 3.14 * f * 400.0 / samplingFreqHz));
        }
        else
        {
            // interleaved stereo 
            audio_10ms[2 * i] = (WebRtc_Word16)(audio_10ms[2 * i] *
                sin(2.0 * 3.14 * f * 400.0 / samplingFreqHz));
            audio_10ms[2 * i + 1] = (WebRtc_Word16)(audio_10ms[2 * i + 1] *
                sin(2.0 * 3.14 * f * 400.0 / samplingFreqHz));
        }
        f++;
    }
}
#endif

MyMedia mobj;

my_transportation::my_transportation(VoENetwork* ptr) :
    myNetw(ptr),
    _thread(NULL),
    _lock(NULL),
    _event(NULL),
    _length(0),
    _channel(0),
    _delayIsEnabled(0),
    _delayTimeInMs(0)
{
    const char* threadName = "external_thread";
    _lock = CriticalSectionWrapper::CreateCriticalSection();
    _event = EventWrapper::Create();
    _thread = ThreadWrapper::CreateThread(Run,
                                          this,
                                          kHighPriority,
                                          threadName);
    if (_thread)
    {
        unsigned int id;
        _thread->Start(id);
    }
}

my_transportation::~my_transportation()
{
    if (_thread)
    {
        _thread->SetNotAlive();
        _event->Set();
        if (_thread->Stop())
        {
            delete _thread;
            _thread = NULL;
            delete _event; 
            _event = NULL;
            delete _lock; 
            _lock = NULL;
        }
    }
}

bool my_transportation::Run(void* ptr)
{
    return static_cast<my_transportation*>(ptr)->Process();
}

bool my_transportation::Process()
{
    switch(_event->Wait(500))
    {
    case kEventSignaled:
        _lock->Enter();
        myNetw->ReceivedRTPPacket( _channel, _packetBuffer, _length );
        _lock->Leave();
        return true;
    case kEventTimeout:
        return true; 
    case kEventError:
        break;
    }
    return true;
}

int my_transportation::SendPacket(int channel, const void *data, int len)
{
    _lock->Enter();
    if (len < 1612)
    {
        memcpy(_packetBuffer, (const unsigned char*)data, len);
        _length = len;
        _channel = channel;
    }
    _lock->Leave();
    _event->Set();  // triggers ReceivedRTPPacket() from worker thread
    return len;
}

int my_transportation::SendRTCPPacket(int channel,const void *data,int len)
{
    if (_delayIsEnabled) 
    {
        Sleep(_delayTimeInMs);
    }
    myNetw->ReceivedRTCPPacket(channel, data, len);
    return len;
}

void my_transportation::SetDelayStatus(bool enable, unsigned int delayInMs)
{
    _delayIsEnabled = enable;
    _delayTimeInMs = delayInMs;
}

ErrorObserver::ErrorObserver()
{
    code = -1;
}
void ErrorObserver::CallbackOnError(const int channel, const int errCode)
{
    code=errCode;
#ifndef _INSTRUMENTATION_TESTING_
    TEST_LOG("\n************************\n");
    TEST_LOG(" RUNTIME ERROR: %d \n", errCode);
    TEST_LOG("************************\n");
#endif
}

void MyTraceCallback::Print(const TraceLevel level, const char *traceString,
                            const int length)
{
    if (traceString)
    {
        char* tmp = new char[length];
        memcpy(tmp, traceString, length);
        TEST_LOG("%s", tmp);
        TEST_LOG("\n");
        delete [] tmp;
    }
}

void RtcpAppHandler::OnApplicationDataReceived(
    const int /*channel*/,
    const unsigned char subType,
    const unsigned int name,
    const unsigned char* data,
    const unsigned short dataLengthInBytes)
{
    _lengthBytes = dataLengthInBytes;
    memcpy(_data, &data[0], dataLengthInBytes);
    _subType = subType;
    _name = name;
}

void RtcpAppHandler::Reset()
{
    _lengthBytes = 0;
    memset(_data, 0, sizeof(_data));
    _subType = 0;
    _name = 0;
}

ErrorObserver obs;
RtcpAppHandler myRtcpAppHandler;
MyRTPObserver rtpObserver;

void my_encryption::encrypt(int ,
                            unsigned char * in_data,
                            unsigned char * out_data,
                            int bytes_in,
                            int * bytes_out){
    int i;
    for(i=0;i<bytes_in;i++)
        out_data[i]=~in_data[i];
    *bytes_out=bytes_in+2;  // length is increased by 2
}

void my_encryption::decrypt(int ,
                            unsigned char * in_data,
                            unsigned char * out_data,
                            int bytes_in,
                            int * bytes_out){
    int i;
    for(i=0;i<bytes_in;i++)
        out_data[i]=~in_data[i];
    *bytes_out=bytes_in-2;  // length is decreased by 2
}

void my_encryption::encrypt_rtcp(int ,
                                 unsigned char * in_data,
                                 unsigned char * out_data,
                                 int bytes_in,
                                 int * bytes_out)
{
    int i;
    for(i=0;i<bytes_in;i++)
        out_data[i]=~in_data[i];
    *bytes_out=bytes_in+2;
}

void my_encryption::decrypt_rtcp(int ,
                                 unsigned char * in_data,
                                 unsigned char * out_data,
                                 int bytes_in,
                                 int * bytes_out)
{
    int i;
    for(i=0;i<bytes_in;i++)
        out_data[i]=~in_data[i];
    *bytes_out=bytes_in+2;
}

void SubAPIManager::DisplayStatus() const
{
    TEST_LOG("Supported sub APIs:\n\n");
    if (_base) TEST_LOG("  Base\n");
    if (_callReport) TEST_LOG("  CallReport\n");
    if (_codec) TEST_LOG("  Codec\n");
    if (_dtmf) TEST_LOG("  Dtmf\n");
    if (_encryption) TEST_LOG("  Encryption\n");
    if (_externalMedia) TEST_LOG("  ExternalMedia\n");
    if (_file) TEST_LOG("  File\n");
    if (_hardware) TEST_LOG("  Hardware\n");
    if (_netEqStats) TEST_LOG("  NetEqStats\n");
    if (_network) TEST_LOG("  Network\n");
    if (_rtp_rtcp) TEST_LOG("  RTP_RTCP\n");
    if (_videoSync) TEST_LOG("  VideoSync\n");
    if (_volumeControl) TEST_LOG("  VolumeControl\n");
    if (_apm) TEST_LOG("  AudioProcessing\n");
    ANL();
    TEST_LOG("Excluded sub APIs:\n\n");
    if (!_base) TEST_LOG("  Base\n");
    if (!_callReport) TEST_LOG("  CallReport\n");
    if (!_codec) TEST_LOG("  Codec\n");
    if (!_dtmf) TEST_LOG("  Dtmf\n");
    if (!_encryption) TEST_LOG("  Encryption\n");
    if (!_externalMedia) TEST_LOG("  ExternamMedia\n");
    if (!_file) TEST_LOG("  File\n");
    if (!_hardware) TEST_LOG("  Hardware\n");
    if (!_netEqStats) TEST_LOG("  NetEqStats\n");
    if (!_network) TEST_LOG("  Network\n");
    if (!_rtp_rtcp) TEST_LOG("  RTP_RTCP\n");
    if (!_videoSync) TEST_LOG("  VideoSync\n");
    if (!_volumeControl) TEST_LOG("  VolumeControl\n");
    if (!_apm) TEST_LOG("  AudioProcessing\n");
    ANL();
}

bool SubAPIManager::GetExtendedMenuSelection(ExtendedSelection& sel)
{
    printf("------------------------------------------------\n");
    printf("Select extended test\n\n");
    printf(" (0)  None\n");
    printf("- - - - - - - - - - - - - - - - - - - - - - - - \n");
    printf(" (1)  Base");
    if (_base) printf("\n"); else printf(" (NA)\n");
    printf(" (2)  CallReport");
    if (_callReport) printf("\n"); else printf(" (NA)\n");
    printf(" (3)  Codec");
    if (_codec) printf("\n"); else printf(" (NA)\n");
    printf(" (4)  Dtmf");
    if (_dtmf) printf("\n"); else printf(" (NA)\n");
    printf(" (5)  Encryption");
    if (_encryption) printf("\n"); else printf(" (NA)\n");
    printf(" (6)  VoEExternalMedia");
    if (_externalMedia) printf("\n"); else printf(" (NA)\n");
    printf(" (7)  File");
    if (_file) printf("\n"); else printf(" (NA)\n");
    printf(" (8)  Hardware");
    if (_hardware) printf("\n"); else printf(" (NA)\n");
    printf(" (9) NetEqStats");
    if (_netEqStats) printf("\n"); else printf(" (NA)\n");
    printf(" (10) Network");
    if (_network) printf("\n"); else printf(" (NA)\n");
    printf(" (11) RTP_RTCP");
    if (_rtp_rtcp) printf("\n"); else printf(" (NA)\n");
    printf(" (12) VideoSync");
    if (_videoSync) printf("\n"); else printf(" (NA)\n");
    printf(" (13) VolumeControl");
    if (_volumeControl) printf("\n"); else printf(" (NA)\n");
    printf(" (14) AudioProcessing");
    if (_apm) printf("\n"); else printf(" (NA)\n");
    printf("\n: ");

    ExtendedSelection xsel(XSEL_Invalid);
    int selection(0);
    dummy = scanf("%d", &selection);

    switch (selection)
    {
    case 0:
        xsel = XSEL_None;
        break;
    case 1:
        if (_base) xsel = XSEL_Base;
        break;
    case 2:
        if (_callReport) xsel = XSEL_CallReport;
        break;
    case 3:
        if (_codec) xsel = XSEL_Codec;
        break;
    case 4:
        if (_dtmf) xsel = XSEL_DTMF;
        break;
    case 5:
        if (_encryption) xsel = XSEL_Encryption;
        break;
    case 6:
        if (_externalMedia) xsel = XSEL_ExternalMedia;
        break;
    case 7:
        if (_file) xsel = XSEL_File;
        break;
    case 8:
        if (_hardware) xsel = XSEL_Hardware;
        break;
    case 9:
        if (_netEqStats) xsel = XSEL_NetEqStats;
        break;
    case 10:
        if (_network) xsel = XSEL_Network;
        break;
    case 11:
        if (_rtp_rtcp) xsel = XSEL_RTP_RTCP;
        break;
    case 12:
        if (_videoSync) xsel = XSEL_VideoSync;
        break;
    case 13:
        if (_volumeControl) xsel = XSEL_VolumeControl;
        break;
    case 14:
        if (_apm) xsel = XSEL_AudioProcessing;
        break;
    default:
        xsel = XSEL_Invalid;
        break;
    }
    if (xsel == XSEL_Invalid)
        printf("Invalid selection!\n");

    sel = xsel;
    _xsel = xsel;

    return (xsel != XSEL_Invalid);
}

VoETestManager::VoETestManager() :
    ve(0),
    base(0),
    codec(0),
    volume(0),
    dtmf(0),
    rtp_rtcp(0),
    apm(0),
    netw(0),
    file(0),
    encrypt(0),
    hardware(0),
    xmedia(0),
    report(0),
    vsync(0),
    instanceCount(0)
{
    if (VoiceEngine::SetTraceFile(NULL) != -1)
    {
        // should not be possible to call a Trace method before the VoE is
        // created
        TEST_LOG("\nError at line: %i (VoiceEngine::SetTraceFile()"
            "should fail)!\n", __LINE__);
    }
#ifdef _TEST_NETEQ_STATS_
    neteqst = 0;
#endif
    ve = VoiceEngine::Create();
    instanceCount++;
};

VoETestManager::~VoETestManager()
{
}

void VoETestManager::GetInterfaces()
{
    if (ve)
    {
        base = VoEBase::GetInterface(ve);
        codec = VoECodec::GetInterface(ve);
        volume = VoEVolumeControl::GetInterface(ve);
        dtmf = VoEDtmf::GetInterface(ve);
        rtp_rtcp = VoERTP_RTCP::GetInterface(ve);
        apm = VoEAudioProcessing::GetInterface(ve);
        netw = VoENetwork::GetInterface(ve);
        file = VoEFile::GetInterface(ve);
#ifdef _TEST_VIDEO_SYNC_
        vsync = VoEVideoSync::GetInterface(ve);
#endif
        encrypt = VoEEncryption::GetInterface(ve);
        hardware = VoEHardware::GetInterface(ve);
        // Set the audio layer to use in all tests
        if (hardware)
        {
            int res = hardware->SetAudioDeviceLayer(TESTED_AUDIO_LAYER);
            if (res < 0)
            {
                printf("\nERROR: failed to set audio layer to use in "
                    "testing\n");
            }
            else
            {
                printf("\nAudio layer %d will be used in testing\n",
                       TESTED_AUDIO_LAYER);
            }
        }
#ifdef _TEST_XMEDIA_
        xmedia = VoEExternalMedia::GetInterface(ve);
#endif
#ifdef _TEST_CALL_REPORT_
        report = VoECallReport::GetInterface(ve);
#endif
#ifdef _TEST_NETEQ_STATS_
        neteqst = VoENetEqStats::GetInterface(ve);
#endif
    }
}

int VoETestManager::ReleaseInterfaces()
{
    int err(0), remInt(1), j(0);
    bool releaseOK(true);

    if (base) 
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = base->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d base interfaces"
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        // try to release one addition time (should fail)
        TEST_MUSTPASS(-1 != base->Release());
        err = base->LastError();
        // it is considered safe to delete even if Release has been called
        // too many times
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
    if (codec)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = codec->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d codec interfaces"
                " (should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != codec->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
    if (volume)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = volume->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d volume interfaces"
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != volume->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
    if (dtmf)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = dtmf->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d dtmf interfaces"
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != dtmf->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
    if (rtp_rtcp)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = rtp_rtcp->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d rtp/rtcp interfaces"
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != rtp_rtcp->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
    if (apm)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = apm->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d apm interfaces"
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != apm->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
    if (netw)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = netw->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d network interfaces"
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != netw->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
    if (file)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = file->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d file interfaces"
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != file->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
#ifdef _TEST_VIDEO_SYNC_
    if (vsync)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = vsync->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d video sync interfaces"
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != vsync->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
#endif
    if (encrypt)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = encrypt->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d encryption interfaces"
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != encrypt->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
    if (hardware)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = hardware->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d hardware interfaces"
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != hardware->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
#ifdef _TEST_XMEDIA_
    if (xmedia)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = xmedia->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d external media interfaces"
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != xmedia->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
#endif
#ifdef _TEST_CALL_REPORT_
    if (report)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = report->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d call report interfaces"
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != report->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
#endif
#ifdef _TEST_NETEQ_STATS_
    if (neteqst)
    {
        for (remInt=1,j=0; remInt>0; j++)
            TEST_MUSTPASS(-1 == (remInt = neteqst->Release()));
        if (j>1)
        {
            TEST_LOG("\n\n*** Error: released %d neteq stat interfaces "
                "(should only be 1) \n", j);
            releaseOK = false;
        }
        TEST_MUSTPASS(-1 != neteqst->Release());
        err = base->LastError();
        TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
    }
#endif
    if (false == VoiceEngine::Delete(ve))
    {
        TEST_LOG("\n\nVoiceEngine::Delete() failed. \n");
        releaseOK = false;
    }

    if (VoiceEngine::SetTraceFile(NULL) != -1)
    {
        TEST_LOG("\nError at line: %i (VoiceEngine::SetTraceFile()"
            "should fail)!\n", __LINE__);
    }
    
    return (releaseOK == true) ? 0 : -1;
}

int VoETestManager::DoStandardTest()
{
#if (defined(_TEST_CODEC_) || defined(_TEST_FILE_))
    CodecInst cinst;
    memset(&cinst, 0, sizeof(cinst));
#endif
    char tmpStr[1024];
    bool enabled(false);

    TEST_LOG("\n\n+++ Base tests +++\n\n");

    // Test trace callbacks
    TEST_LOG("Enabling the trace callback => default trace messages "
        "shall be printed... \n\n");
    MyTraceCallback* callback = new MyTraceCallback();
    VoiceEngine::SetTraceCallback(callback);
    
    // Test the remaining trace APIs
    TEST_MUSTPASS(VoiceEngine::SetTraceFile(GetFilename("webrtc_voe_trace.txt"),
                                            true));
    TEST_MUSTPASS(VoiceEngine::SetTraceFile(NULL));
    TEST_MUSTPASS(VoiceEngine::SetTraceFile(GetFilename(
        "webrtc_voe_trace.txt")));

    VoiceEngine* extra = VoiceEngine::Create();
    instanceCount++;
    TEST_LOG("\nVerify that the VoE ID is now changed from 1 to 2\n\n");
    TEST_MUSTPASS(VoiceEngine::SetTraceFile(NULL));
    TEST_MUSTPASS(VoiceEngine::SetTraceFile(GetFilename(
        "webrtc_voe_trace.txt")));
    TEST_MUSTPASS(VoiceEngine::SetTraceFile(NULL));
    VoiceEngine::Delete(extra);
    SLEEP(10);
    TEST_LOG("\nVerify that the VoE ID is now changed back to 1\n");
    TEST_LOG("NOTE: Currently it will still be 2, this is OK\n\n");

    // The API below shall be the first line in the stored trace file
    // (verify after test)!
    TEST_MUSTPASS(VoiceEngine::SetTraceFile(GetFilename(
        "webrtc_voe_trace.txt")));
    VoiceEngine::SetTraceCallback(NULL);
    delete callback;
    TEST_LOG("\n...the trace callback is now disabled.\n\n");

    /////////////////////////////////////////////////
    // Hardware (test before VoE is intialized)
#ifdef _TEST_HARDWARE_
    // Set/GetAudioDeviceLayer
    TEST_LOG("Set/Get audio device layer\n");
    AudioLayers wantedLayer = TESTED_AUDIO_LAYER;
    AudioLayers givenLayer;
    TEST_MUSTPASS(hardware->SetAudioDeviceLayer(wantedLayer));
    TEST_MUSTPASS(hardware->GetAudioDeviceLayer(givenLayer));
    TEST_MUSTPASS(wantedLayer != givenLayer); // Should be same before init
#endif //_TEST_HARDWARE_

    TEST_LOG("Init \n");
#if defined BLACKFIN
    TEST_MUSTPASS(base->Init(0,LINUX_AUDIO_OSS));
#else
   TEST_MUSTPASS( base->Init());
#endif

#if defined(ANDROID)
    TEST_LOG("Setting loudspeaker status to false \n");
    TEST_MUSTPASS(hardware->SetLoudspeakerStatus(false));
#endif

#ifndef __INSURE__
    TEST_LOG("Enabling the observer \n");
    TEST_MUSTPASS(base->RegisterVoiceEngineObserver(obs));
#endif

    TEST_LOG("Get version \n");
    TEST_MUSTPASS(base->GetVersion(tmpStr));
    TEST_LOG("--------------------\n%s\n--------------------\n", tmpStr);

    TEST_LOG("Create channel \n");
    int nChannels = base->MaxNumOfChannels();
    TEST_MUSTPASS(!(nChannels > 0));
    TEST_LOG("Max number of channels = %d \n", nChannels);
    TEST_MUSTPASS(base->CreateChannel());

    /////////////////////////////////////////////////
    // RTP/RTCP (test before streaming is activated)
#ifdef _TEST_RTP_RTCP_
    TEST_LOG("\n\n+++ RTP/RTCP tests +++\n\n");

    TEST_LOG("Set/Get RTCP and CName \n");
    bool on;
    TEST_MUSTPASS(rtp_rtcp->GetRTCPStatus(0, on));   // should be on by default
    TEST_MUSTPASS(on != true);
    TEST_MUSTPASS(rtp_rtcp->SetRTCPStatus(0, false));
    TEST_MUSTPASS(rtp_rtcp->GetRTCPStatus(0, on));
    TEST_MUSTPASS(on != false);
    TEST_MUSTPASS(rtp_rtcp->SetRTCPStatus(0, true));
    TEST_MUSTPASS(rtp_rtcp->GetRTCPStatus(0, on));
    TEST_MUSTPASS(on != true);
    TEST_MUSTPASS(rtp_rtcp->SetRTCP_CNAME(0, "Niklas"));

    TEST_LOG("Set/Get RTP Keepalive\n");
    unsigned char pt; int dT;
    TEST_MUSTPASS(!rtp_rtcp->GetRTPKeepaliveStatus(-1, on, pt, dT));
    // should be off by default
    TEST_MUSTPASS(rtp_rtcp->GetRTPKeepaliveStatus(0, on, pt, dT));
    TEST_MUSTPASS(on != false); 
    TEST_MUSTPASS(pt != 255);
    TEST_MUSTPASS(dT != 0);

    // verify invalid input parameters
    TEST_MUSTPASS(!rtp_rtcp->SetRTPKeepaliveStatus(-1, true, 0, 15));
    TEST_MUSTPASS(!rtp_rtcp->SetRTPKeepaliveStatus(0, true, -1, 15));
    TEST_MUSTPASS(!rtp_rtcp->SetRTPKeepaliveStatus(0, true, 0, 61));
    // should still be off
    TEST_MUSTPASS(rtp_rtcp->GetRTPKeepaliveStatus(0, on, pt, dT));
    TEST_MUSTPASS(!rtp_rtcp->SetRTPKeepaliveStatus(0, true, 0));
    // should fail since default 0 is used bu PCMU
    TEST_MUSTPASS(on != false);
    // try valid settings
    TEST_MUSTPASS(rtp_rtcp->SetRTPKeepaliveStatus(0, true, 1));
    TEST_MUSTPASS(rtp_rtcp->SetRTPKeepaliveStatus(0, true, 1));
    // should be on now
    TEST_MUSTPASS(rtp_rtcp->GetRTPKeepaliveStatus(0, on, pt, dT));
    TEST_MUSTPASS(on != true); TEST_MUSTPASS(pt != 1); TEST_MUSTPASS(dT != 15);
    // Set the Keep alive payload to 60, and this payloadtype could not used
    // by the codecs
    TEST_MUSTPASS(rtp_rtcp->SetRTPKeepaliveStatus(0, true, 60, 3));
    TEST_MUSTPASS(rtp_rtcp->GetRTPKeepaliveStatus(0, on, pt, dT));
    TEST_MUSTPASS(on != true); TEST_MUSTPASS(pt != 60); TEST_MUSTPASS(dT != 3);
    TEST_MUSTPASS(rtp_rtcp->SetRTPKeepaliveStatus(0, false, 60));

    TEST_LOG("Set and get SSRC \n");
    TEST_MUSTPASS(rtp_rtcp->SetLocalSSRC(0, 1234));
    unsigned int sendSSRC = 0;
    TEST_MUSTPASS(rtp_rtcp->GetLocalSSRC(0, sendSSRC));
    TEST_MUSTPASS(1234 != sendSSRC);
#else
    TEST_LOG("\n\n+++ RTP/RTCP tests NOT ENABLED +++\n");
#endif

    /////////////////////////////////////////////////
    // Hardware (test before streaming is activated)
    // the test will select the device using 44100, which fails the call
#ifdef _TEST_HARDWARE_
    TEST_LOG("\n\n+++ Hardware tests +++\n\n");

    // Set/GetAudioDeviceLayer
    TEST_LOG("Set/Get audio device layer\n");
    TEST_MUSTPASS(-1 != hardware->SetAudioDeviceLayer(wantedLayer));
    TEST_MUSTPASS(VE_ALREADY_INITED != base->LastError());
    TEST_MUSTPASS(hardware->GetAudioDeviceLayer(givenLayer));
    switch (givenLayer)
    {
    case kAudioPlatformDefault:
        // already set above
        break;
    case kAudioWindowsCore:
        TEST_LOG("Running kAudioWindowsCore\n");
        break;
    case kAudioWindowsWave:
        TEST_LOG("Running kAudioWindowsWave\n");
        break;
    case kAudioLinuxAlsa:
        TEST_LOG("Running kAudioLinuxAlsa\n");
        break;
    case kAudioLinuxPulse:
        TEST_LOG("Running kAudioLinuxPulse\n");
        break;
    default:
        TEST_LOG("ERROR: Running unknown audio layer!!\n");
        return -1;
    }

    int loadPercent;
#if defined(_WIN32)
    TEST_LOG("CPU load \n");
    TEST_MUSTPASS(hardware->GetCPULoad(loadPercent));
    TEST_LOG("GetCPULoad => %d%%\n", loadPercent);
#else
    TEST_MUSTPASS(!hardware->GetCPULoad(loadPercent));
#endif
#if !defined(MAC_IPHONE) & !defined(ANDROID)
    TEST_MUSTPASS(hardware->GetSystemCPULoad(loadPercent));
    TEST_LOG("GetSystemCPULoad => %d%%\n", loadPercent);
#endif
    
#if !defined(MAC_IPHONE) && !defined(ANDROID)
    bool playAvail = false, recAvail = false;
    TEST_LOG("Get device status \n");
    TEST_MUSTPASS(hardware->GetPlayoutDeviceStatus(playAvail));
    TEST_MUSTPASS(hardware->GetRecordingDeviceStatus(recAvail));
    TEST_MUSTPASS(!(recAvail && playAvail));
#endif
    
    // Win, Mac and Linux sound device tests
#if (defined(WEBRTC_MAC) && !defined(MAC_IPHONE)) || defined(_WIN32) || (defined(WEBRTC_LINUX) && !defined(ANDROID))
    int idx, nRec = 0, nPlay = 0;
    char devName[128] = {0};
    char guidName[128] = {0};

    TEST_LOG("Printing names of default sound devices \n");
#if defined(_WIN32)
    TEST_MUSTPASS(hardware->GetRecordingDeviceName(-1, devName, guidName));
    TEST_LOG("Recording device= %s, guid=%s\n",devName,guidName);
    TEST_MUSTPASS(hardware->GetPlayoutDeviceName(-1, devName, guidName));
    TEST_LOG("Playout device= %s, guid=%s\n",devName,guidName);
#else
    TEST_MUSTPASS(hardware->GetRecordingDeviceName(0, devName, guidName));
    TEST_LOG("Recording device= %s\n",devName);
    TEST_MUSTPASS(hardware->GetPlayoutDeviceName(0, devName, guidName));
    TEST_LOG("Playout device= %s\n",devName);
#endif

    // Recording side
    TEST_MUSTPASS(hardware->GetNumOfRecordingDevices(nRec));
    TEST_LOG("GetNumOfRecordingDevices = %d\n", nRec);
    for (idx = 0; idx < nRec; idx++)
    {
        // extended Win32 enumeration tests => unique GUID outputs on Vista
        // and up
        // Win XP and below : devName is copied to guidName
        // Win Vista and up : devName is the friendly name and GUID is a uniqe
        // indentifier
        // Other            : guidName is left unchanged
        TEST_MUSTPASS(hardware->GetRecordingDeviceName(idx, devName, guidName));
#if defined(_WIN32)
        TEST_LOG("GetRecordingDeviceName(%d) => name=%s, guid=%s\n",
                 idx, devName, guidName);
#else
        TEST_LOG("GetRecordingDeviceName(%d) => name=%s\n", idx, devName);
#endif
        TEST_MUSTPASS(hardware->SetRecordingDevice(idx));
    }

    // Playout side
    TEST_MUSTPASS(hardware->GetNumOfPlayoutDevices(nPlay));
    TEST_LOG("GetNumDevsPlayout = %d\n", nPlay);
    for (idx = 0; idx < nPlay; idx++)
    {
        // extended Win32 enumeration tests => unique GUID outputs on Vista
        // and up
        // Win XP and below : devName is copied to guidName
        // Win Vista and up : devName is the friendly name and GUID is a
        // uniqe indentifier
        // Other            : guidName is left unchanged
        TEST_MUSTPASS(hardware->GetPlayoutDeviceName(idx, devName, guidName));
#if defined(_WIN32)
        TEST_LOG("GetPlayoutDeviceName(%d) => name=%s, guid=%s\n",
                 idx, devName, guidName);
#else
        TEST_LOG("GetPlayoutDeviceName(%d) => name=%s\n", idx, devName);
#endif
        TEST_MUSTPASS(hardware->SetPlayoutDevice(idx));
    }

#endif // #if (defined(WEBRTC_MAC) && !defined(MAC_IPHONE)) || (defined(_WI...&
    TEST_LOG("Setting default sound devices \n");
#ifdef _WIN32
    TEST_MUSTPASS(hardware->SetRecordingDevice(-1));
    TEST_MUSTPASS(hardware->SetPlayoutDevice(-1));
#else
#if !defined(MAC_IPHONE) && !defined(ANDROID)
    TEST_MUSTPASS(hardware->SetRecordingDevice(0));
    TEST_MUSTPASS(hardware->SetPlayoutDevice(0));
#endif
#endif
    
#ifdef MAC_IPHONE
    // Reset sound device
    TEST_LOG("Reset sound device \n");
    TEST_MUSTPASS(hardware->ResetAudioDevice());
#endif

#else
    TEST_LOG("\n\n+++ Hardware tests NOT ENABLED +++\n");
#endif  // #ifdef _TEST_HARDWARE_

    // This testing must be done before we start playing
#ifdef _TEST_CODEC_
    // Test that set and get payload type work
#if defined(WEBRTC_CODEC_ISAC)
    TEST_LOG("Getting payload type for iSAC\n");
    strcpy(cinst.plname,"niklas");
    cinst.channels=1;
    cinst.plfreq=16000;
    cinst.pacsize=480;
    // should fail since niklas is not a valid codec name
    TEST_MUSTPASS(!codec->GetRecPayloadType(0,cinst));
    strcpy(cinst.plname,"iSAC");                                
    TEST_MUSTPASS(codec->GetRecPayloadType(0,cinst));  // both iSAC
    strcpy(cinst.plname,"ISAC");                        // and ISAC should work
    TEST_MUSTPASS(codec->GetRecPayloadType(0,cinst));
    int orgPT=cinst.pltype;                      // default payload type is 103
    TEST_LOG("Setting payload type for iSAC to 127\n");
    cinst.pltype=123;
    TEST_MUSTPASS(codec->SetRecPayloadType(0,cinst));
    TEST_MUSTPASS(codec->GetRecPayloadType(0,cinst));
    TEST_MUSTPASS(!(cinst.pltype==123));
    TEST_LOG("Setting it back\n");
    cinst.pltype=orgPT;
    TEST_MUSTPASS(codec->SetRecPayloadType(0,cinst));
    TEST_MUSTPASS(codec->GetRecPayloadType(0,cinst));
    TEST_MUSTPASS(!(cinst.pltype==orgPT));
    cinst.pltype=123;
    cinst.plfreq=8000;
    cinst.pacsize=240;
    cinst.rate=13300;
#ifdef WEBRTC_CODEC_ILBC
    strcpy(cinst.plname,"iLBC");
    TEST_MUSTPASS(codec->GetRecPayloadType(0,cinst));
    orgPT=cinst.pltype;
    cinst.pltype=123;
    TEST_MUSTPASS(codec->SetRecPayloadType(0,cinst));
    TEST_MUSTPASS(codec->GetRecPayloadType(0,cinst));
    TEST_LOG("Setting it back\n");
    cinst.pltype=orgPT;
    TEST_MUSTPASS(codec->SetRecPayloadType(0,cinst));
    TEST_MUSTPASS(codec->GetRecPayloadType(0,cinst));
    TEST_MUSTPASS(!(cinst.pltype==orgPT));
#endif // #ifdef WEBRTC_CODEC_ILBC
#endif // #if defined(WEBRTC_CODEC_ISAC)
#endif // #ifdef _TEST_CODEC_

    ///////////////////////////////////////////////
    // Network (test before streaming is activated)

#ifdef _TEST_NETWORK_
    TEST_LOG("\n\n+++ Network tests +++\n\n");

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    int srcRtpPort = 0;
    int srcRtcpPort = 0;

    int filtPort = -1;
    int filtPortRTCP = -1;
    char srcIp[32] = "0.0.0.0";
    char filtIp[32] = "0.0.0.0";

    TEST_LOG("GetSourceInfo \n");
    srcRtpPort = 1234;
    srcRtcpPort = 1235;
    TEST_MUSTPASS(netw->GetSourceInfo(0, srcRtpPort, srcRtcpPort, srcIp));
    TEST_MUSTPASS(0 != srcRtpPort);
    TEST_MUSTPASS(0 != srcRtcpPort);
    TEST_MUSTPASS(_stricmp(srcIp, ""));

    TEST_LOG("GetSourceFilter \n");
    TEST_MUSTPASS(netw->GetSourceFilter(0, filtPort, filtPortRTCP, filtIp));
    TEST_MUSTPASS(0 != filtPort);
    TEST_MUSTPASS(0 != filtPortRTCP);
    TEST_MUSTPASS(_stricmp(filtIp, ""));

    TEST_LOG("SetSourceFilter \n");
    TEST_MUSTPASS(netw->SetSourceFilter(0, srcRtpPort));
#else
    TEST_LOG("Skipping network tests - WEBRTC_EXTERNAL_TRANSPORT is defined \n");
#endif // #ifndef WEBRTC_EXTERNAL_TRANSPORT
#else
    TEST_LOG("\n\n+++ Network tests NOT ENABLED +++\n");
#endif 

    ///////////////////
    // Start streaming

    TEST_LOG("\n\n+++ Starting streaming +++\n\n");

    my_transportation ch0transport(netw);

    // goto Exit;

#ifdef WEBRTC_EXTERNAL_TRANSPORT
    TEST_LOG("Enabling external transport \n");
    TEST_MUSTPASS(netw->RegisterExternalTransport(0, ch0transport));
#else
    TEST_LOG("Setting send and receive parameters \n");
    TEST_MUSTPASS(base->SetSendDestination(0, 8000, "127.0.0.1"));
    // no IP specified => "0.0.0.0" will be stored
    TEST_MUSTPASS(base->SetLocalReceiver(0,8000));

    CodecInst Jing_inst;
    Jing_inst.channels=1;
    Jing_inst.pacsize=160;
    Jing_inst.plfreq=8000;
    Jing_inst.pltype=0;
    Jing_inst.rate=64000;
    strcpy(Jing_inst.plname, "PCMU");
    TEST_MUSTPASS(codec->SetSendCodec(0, Jing_inst));

    int port = -1, srcPort = -1, rtcpPort = -1;
    char ipaddr[64] = {0};
    strcpy(ipaddr, "10.10.10.10");
    TEST_MUSTPASS(base->GetSendDestination(0, port, ipaddr, srcPort, rtcpPort));
    TEST_MUSTPASS(8000 != port);
    TEST_MUSTPASS(8000 != srcPort);
    TEST_MUSTPASS(8001 != rtcpPort);
    TEST_MUSTPASS(_stricmp(ipaddr, "127.0.0.1"));

    port = -1; rtcpPort = -1;
    TEST_MUSTPASS(base->GetLocalReceiver(0, port, rtcpPort, ipaddr));
    TEST_MUSTPASS(8000 != port);
    TEST_MUSTPASS(8001 != rtcpPort);
    TEST_MUSTPASS(_stricmp(ipaddr, "0.0.0.0"));
#endif

    TEST_LOG("Start listening, playout and sending \n");
    TEST_MUSTPASS(base->StartReceive(0));
    TEST_MUSTPASS(base->StartPlayout(0));
    TEST_MUSTPASS(base->StartSend(0));

    // <=== full duplex ===>

    TEST_LOG("You should now hear yourself, running default codec (PCMU)\n");
    SLEEP(2000);

    if (file)
    {
        TEST_LOG("Start playing a file as microphone, so you don't need to"
            " speak all the time\n");
        TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0,
                                                         micFile,
                                                         true,
                                                         true));
        SLEEP(1000);
    }

#ifdef _TEST_BASE_
    TEST_LOG("Put channel on hold => should *not* hear audio \n");
    // HOLD_SEND_AND_PLAY is the default mode
    TEST_MUSTPASS(base->SetOnHoldStatus(0, true));
    SLEEP(2000);
    TEST_LOG("Remove on hold => should hear audio again \n");
    TEST_MUSTPASS(base->SetOnHoldStatus(0, false));
    SLEEP(2000);
    TEST_LOG("Put sending on hold => should *not* hear audio \n");
    TEST_MUSTPASS(base->SetOnHoldStatus(0, true, kHoldSendOnly));
    SLEEP(2000);
    if (file)
    {
        TEST_LOG("Start playing a file locally => "
            "you should now hear this file being played out \n");
        TEST_MUSTPASS(file->StartPlayingFileLocally(0, micFile, true));
        SLEEP(2000);
    }
    TEST_LOG("Put playing on hold => should *not* hear audio \n");
    TEST_MUSTPASS(base->SetOnHoldStatus(0, true, kHoldPlayOnly));
    SLEEP(2000);
    TEST_LOG("Remove on hold => should hear audio again \n");
    if (file)
    {
        TEST_MUSTPASS(file->StopPlayingFileLocally(0));
    }
    TEST_MUSTPASS(base->SetOnHoldStatus(0, false));
    SLEEP(2000);

    NetEqModes mode;
    TEST_MUSTPASS(base->GetNetEQPlayoutMode(0, mode));
    TEST_MUSTPASS(mode != kNetEqDefault);
    TEST_LOG("NetEQ DEFAULT playout mode enabled => should hear OK audio \n");
    TEST_MUSTPASS(base->SetNetEQPlayoutMode(0, kNetEqDefault));
    SLEEP(3000);
    TEST_LOG("NetEQ STREAMING playout mode enabled => should hear OK audio \n");
    TEST_MUSTPASS(base->SetNetEQPlayoutMode(0, kNetEqStreaming));
    SLEEP(3000);
    TEST_LOG("NetEQ FAX playout mode enabled => should hear OK audio \n");
    TEST_MUSTPASS(base->SetNetEQPlayoutMode(0, kNetEqFax));
    SLEEP(3000);
    TEST_LOG("NetEQ default mode is restored \n");
    TEST_MUSTPASS(base->SetNetEQPlayoutMode(0, kNetEqDefault));
    TEST_MUSTPASS(base->GetNetEQPlayoutMode(0, mode));
    TEST_MUSTPASS(mode != kNetEqDefault);
    TEST_MUSTPASS(base->GetNetEQPlayoutMode(0, mode));
    TEST_MUSTPASS(mode != kNetEqDefault);
    TEST_LOG("NetEQ DEFAULT playout mode enabled => should hear OK audio \n");
    TEST_MUSTPASS(base->SetNetEQPlayoutMode(0, kNetEqDefault));
    SLEEP(3000);
    TEST_LOG("NetEQ STREAMING playout mode enabled => should hear OK audio \n");
    TEST_MUSTPASS(base->SetNetEQPlayoutMode(0, kNetEqStreaming));
    SLEEP(3000);
    TEST_LOG("NetEQ FAX playout mode enabled => should hear OK audio \n");
    TEST_MUSTPASS(base->SetNetEQPlayoutMode(0, kNetEqFax));
    SLEEP(3000);
    TEST_LOG("NetEQ default mode is restored \n");
    TEST_MUSTPASS(base->SetNetEQPlayoutMode(0, kNetEqDefault));
    TEST_MUSTPASS(base->GetNetEQPlayoutMode(0, mode));
    TEST_MUSTPASS(mode != kNetEqDefault);
    TEST_MUSTPASS(base->GetNetEQPlayoutMode(0, mode));
    TEST_MUSTPASS(mode != kNetEqDefault);
    TEST_LOG("NetEQ DEFAULT playout mode enabled => should hear OK audio \n");
    TEST_MUSTPASS(base->SetNetEQPlayoutMode(0, kNetEqDefault));
    SLEEP(3000);
    TEST_LOG("NetEQ STREAMING playout mode enabled => should hear OK audio \n");
    TEST_MUSTPASS(base->SetNetEQPlayoutMode(0, kNetEqStreaming));
    SLEEP(3000);
    TEST_LOG("NetEQ FAX playout mode enabled => should hear OK audio \n");
    TEST_MUSTPASS(base->SetNetEQPlayoutMode(0, kNetEqFax));
    SLEEP(3000);
    TEST_LOG("NetEQ default mode is restored \n");
    TEST_MUSTPASS(base->SetNetEQPlayoutMode(0, kNetEqDefault));
    TEST_MUSTPASS(base->GetNetEQPlayoutMode(0, mode));
    TEST_MUSTPASS(mode != kNetEqDefault);

    TEST_LOG("Scan all possible NetEQ BGN modes\n");  // skip listening test
    enum NetEqBgnModes bgnMode;
    TEST_MUSTPASS(base->GetNetEQBGNMode(0, bgnMode));
    TEST_MUSTPASS(bgnMode != kBgnOn);
    TEST_MUSTPASS(base->SetNetEQBGNMode(0, kBgnOn));
    TEST_MUSTPASS(base->GetNetEQBGNMode(0, bgnMode));
    TEST_MUSTPASS(bgnMode != kBgnOn);
    TEST_MUSTPASS(base->SetNetEQBGNMode(0, kBgnFade));
    TEST_MUSTPASS(base->GetNetEQBGNMode(0, bgnMode));
    TEST_MUSTPASS(bgnMode != kBgnFade);
    TEST_MUSTPASS(base->SetNetEQBGNMode(0, kBgnOff));
    TEST_MUSTPASS(base->GetNetEQBGNMode(0, bgnMode));
    TEST_MUSTPASS(bgnMode != kBgnOff);
#else
    TEST_LOG("Skipping on hold and NetEQ playout tests -"
        "Base tests are not enabled \n");
#endif // #ifdef _TEST_BASE_

    /////////
    // Codec

#ifdef _TEST_CODEC_
    TEST_LOG("\n\n+++ Codec tests +++\n\n");

    TEST_LOG("Checking default codec\n");
    TEST_MUSTPASS(codec->GetSendCodec(0, cinst));
    TEST_MUSTPASS(cinst.channels != 1);
    TEST_MUSTPASS(cinst.pacsize != 160);
    TEST_MUSTPASS(cinst.plfreq != 8000);
    TEST_MUSTPASS(cinst.pltype != 0);
    TEST_MUSTPASS(cinst.rate != 64000);
    TEST_MUSTPASS(strcmp("PCMU", cinst.plname) != 0);

    TEST_LOG("Looping through all codecs and packet sizes\n");
    TEST_LOG("NOTE: For swb codecs, ensure that you speak in the mic\n");
    int nCodecs = codec->NumOfCodecs();
    for (int index = 0; index < nCodecs; index++)
    {
        TEST_MUSTPASS(codec->GetCodec(index, cinst));

        if (!((!_stricmp("CN", cinst.plname)) ||
            (!_stricmp("telephone-event", cinst.plname) ||
            (!_stricmp("red",cinst.plname)))))
        {
            // If no default payload type is defined, we use 127 and also set
          // receive payload type
            if (-1 == cinst.pltype)
            {
                cinst.pltype = 127;
                TEST_MUSTPASS(base->StopPlayout(0));
                TEST_MUSTPASS(base->StopReceive(0));
                TEST_MUSTPASS(codec->SetRecPayloadType(0, cinst));
                TEST_MUSTPASS(base->StartReceive(0));
                TEST_MUSTPASS(base->StartPlayout(0));
            }
            TEST_LOG("%s (pt=%d): default(%d) ", cinst.plname, cinst.pltype,
                     cinst.pacsize);
            TEST_MUSTPASS(codec->SetSendCodec(0, cinst));
            SLEEP(CODEC_TEST_TIME);
            // Packet sizes
            if (!_stricmp("g7221", cinst.plname))   // special case for G.722.1
            {
                // Test 16 and 32 kHz
                for (int freq = 16000; freq <= 32000; freq += 16000)
                {
                    cinst.plfreq = freq;
                    // Test 16/24/32 and 24/32/48 kbit respectively
                    int rate = (16000 == freq ? 16000 : 24000);
                    int maxRate = (16000 == freq ? 32000 : 40000);
                    // In fact 48, see below
                    for (; rate <= maxRate; rate += 8000)
                    {
                        rate = (40000 == rate ? 48000 : rate); // 40 -> 48
                        cinst.rate = rate;
                        // Test packet sizes
                        TEST_LOG("\n%s (pt=%d, fs=%d, rate=%d): ",
                                 cinst.plname, cinst.pltype,
                                 cinst.plfreq, cinst.rate);
                        for (int pacsize = 80; pacsize < 1000; pacsize += 80)
                        {
                            // Set codec, and receive payload type
                            cinst.pacsize = pacsize;
                            if (-1 != codec->SetSendCodec(0, cinst))
                            {
                                TEST_MUSTPASS(base->StopPlayout(0));
                                TEST_MUSTPASS(base->StopReceive(0));
                                TEST_MUSTPASS(codec->SetRecPayloadType(0,
                                                                       cinst));
                                TEST_MUSTPASS(base->StartReceive(0));
                                TEST_MUSTPASS(base->StartPlayout(0));
                                TEST_LOG("%d ", pacsize);
                                fflush(NULL);
                                SLEEP(2*CODEC_TEST_TIME);
                            }
                        }
                    }
                }
            }
            else
            {
                for (int pacsize = 80; pacsize < 1000; pacsize += 80)
                {
                    // Set codec
                    // from VoE 4.0, we need the specify the right rate
                    if (!_stricmp("ilbc", cinst.plname))
                    {

                        if((pacsize == 160) || (pacsize == 320))
                        {
                            cinst.rate = 15200;
                        }
                        else
                        {
                            cinst.rate = 13300;
                        }
                    }
                    cinst.pacsize = pacsize;
                    if (-1 != codec->SetSendCodec(0, cinst))
                    {
                        TEST_LOG("%d ", pacsize);
                        fflush(NULL);
                        SLEEP(CODEC_TEST_TIME);
                    }
                }
            }
            TEST_LOG("\n");
        }
    }

    TEST_MUSTPASS(codec->GetCodec(0, cinst));
    TEST_LOG("Setting codec to first in list: %s \n", cinst.plname);
    TEST_MUSTPASS(codec->SetSendCodec(0, cinst));

    TEST_LOG("Voice Activity Detection calls\n");
    TEST_LOG("Must be OFF by default\n");
    bool VADtest = true;
    VadModes vadMode = kVadAggressiveHigh;
    bool disabledDTX = true;
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(VADtest);
    TEST_MUSTPASS(kVadConventional != vadMode);
    TEST_MUSTPASS(!disabledDTX);
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(VADtest);
    TEST_MUSTPASS(kVadConventional != vadMode);
    TEST_MUSTPASS(!disabledDTX);
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(VADtest);
    TEST_MUSTPASS(kVadConventional != vadMode);
    TEST_MUSTPASS(!disabledDTX);

    TEST_LOG("Turn ON VAD\n");
    TEST_MUSTPASS(codec->SetVADStatus(0, true));
    TEST_LOG("Should be ON now\n");
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(!VADtest);
    TEST_MUSTPASS(kVadConventional != vadMode);
    TEST_MUSTPASS(disabledDTX);
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(!VADtest);
    TEST_MUSTPASS(kVadConventional != vadMode);
    TEST_MUSTPASS(disabledDTX);
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(!VADtest);
    TEST_MUSTPASS(kVadConventional != vadMode);
    TEST_MUSTPASS(disabledDTX);

    TEST_LOG("Testing Type settings\n");
    TEST_MUSTPASS(codec->SetVADStatus(0, true, kVadAggressiveLow));
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(kVadAggressiveLow != vadMode);
    TEST_MUSTPASS(codec->SetVADStatus(0, true, kVadAggressiveMid));
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(kVadAggressiveMid != vadMode);
    TEST_MUSTPASS(codec->SetVADStatus(0, true, kVadAggressiveMid));
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(kVadAggressiveMid != vadMode);
    TEST_MUSTPASS(codec->SetVADStatus(0, true, kVadAggressiveMid));
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(kVadAggressiveMid != vadMode);
    TEST_MUSTPASS(codec->SetVADStatus(0, true, kVadAggressiveHigh, true));
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(kVadAggressiveHigh != vadMode);
    TEST_MUSTPASS(codec->SetVADStatus(0, true, kVadAggressiveHigh, true));
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(kVadAggressiveHigh != vadMode);
    TEST_MUSTPASS(codec->SetVADStatus(0, true, kVadAggressiveHigh, true));
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(kVadAggressiveHigh != vadMode);
    TEST_MUSTPASS(!disabledDTX);
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(kVadAggressiveHigh != vadMode);
    TEST_MUSTPASS(!disabledDTX);
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(kVadAggressiveHigh != vadMode);
    TEST_MUSTPASS(!disabledDTX);
    TEST_MUSTPASS(codec->SetVADStatus(0, true, kVadConventional));
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(kVadConventional != vadMode);
    TEST_MUSTPASS(disabledDTX);
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(kVadConventional != vadMode);
    TEST_MUSTPASS(disabledDTX);
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(kVadConventional != vadMode);
    TEST_MUSTPASS(disabledDTX);

    // VAD is always on when DTX is on, so we need to turn off DTX too
    TEST_LOG("Turn OFF VAD\n");
    TEST_MUSTPASS(codec->SetVADStatus(0, false, kVadConventional, true));
    TEST_LOG("Should be OFF now\n");
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(VADtest);
    TEST_MUSTPASS(kVadConventional != vadMode);
    TEST_MUSTPASS(!disabledDTX);
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(VADtest);
    TEST_MUSTPASS(kVadConventional != vadMode);
    TEST_MUSTPASS(!disabledDTX);
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(VADtest);
    TEST_MUSTPASS(kVadConventional != vadMode);
    TEST_MUSTPASS(!disabledDTX);

#if defined(WEBRTC_CODEC_ISAC)
    TEST_LOG("Test extended iSAC APIs\n");
    TEST_LOG("Start by selecting iSAC 30ms adaptive mode\n");
    strcpy(cinst.plname,"isac");
    cinst.pltype=103;
    cinst.plfreq=16000;
    cinst.channels=1;
    cinst.rate=-1;  // adaptive rate
    cinst.pacsize=480;
    TEST_LOG("  testing SetISACInitTargetRate:\n");
    TEST_MUSTPASS(codec->SetSendCodec(0, cinst));
    TEST_MUSTPASS(!codec->SetISACInitTargetRate(0, 5000));
    TEST_MUSTPASS(!codec->SetISACInitTargetRate(0, 33000));
    TEST_MUSTPASS(codec->SetISACInitTargetRate(0, 32000));
    TEST_LOG("Speak and ensure that iSAC sounds OK (target = 32kbps)...\n");
    SLEEP(3000);
    TEST_MUSTPASS(codec->SetISACInitTargetRate(0, 10000));
    TEST_LOG("Speak and ensure that iSAC sounds OK (target = 10kbps)...\n");
    SLEEP(3000);
    TEST_MUSTPASS(codec->SetISACInitTargetRate(0, 10000, true));
    TEST_MUSTPASS(codec->SetISACInitTargetRate(0, 10000, false));
    TEST_MUSTPASS(codec->SetISACInitTargetRate(0, 0));
    TEST_LOG("Speak and ensure that iSAC sounds OK (target = default)...\n");
    SLEEP(3000);

    TEST_LOG("  testing SetISACMaxPayloadSize:\n");
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(!codec->SetISACMaxPayloadSize(0, 50));
    TEST_MUSTPASS(!codec->SetISACMaxPayloadSize(0, 650));
    TEST_MUSTPASS(codec->SetISACMaxPayloadSize(0, 120));
    TEST_MUSTPASS(base->StartSend(0));
    TEST_LOG("Speak and ensure that iSAC sounds OK"
        "(max payload size = 100 bytes)...\n");
    SLEEP(3000);
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(codec->SetISACMaxPayloadSize(0, 400));
    TEST_MUSTPASS(base->StartSend(0));

    TEST_LOG("  testing SetISACMaxRate:\n");
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(!codec->SetISACMaxRate(0, 31900));
    TEST_MUSTPASS(!codec->SetISACMaxRate(0, 53500));
    TEST_MUSTPASS(codec->SetISACMaxRate(0, 32000));
    TEST_MUSTPASS(base->StartSend(0));
    TEST_LOG("Speak and ensure that iSAC sounds OK (max rate = 32 kbps)...\n");
    SLEEP(3000);
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(codec->SetISACMaxRate(0, 53400)); // restore no limitation
    TEST_MUSTPASS(base->StartSend(0));
    if (file)
    {
        TEST_LOG("==> Start playing a file as microphone again \n");
        TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0,
                                                         micFile,
                                                         true,
                                                         true));
    }
#else
    TEST_LOG("Skipping extended iSAC API tests - "
        "WEBRTC_CODEC_ISAC not defined\n");
#endif // #if defined(WEBRTC_CODEC_ISAC)

    // Tests on AMR setencformat and setdecformat
    // These should fail
    TEST_MUSTPASS(!codec->SetAMREncFormat(0, kRfc3267BwEfficient));
    TEST_MUSTPASS(!codec->SetAMRDecFormat(0, kRfc3267BwEfficient));
    TEST_MUSTPASS(!codec->SetAMREncFormat(0, kRfc3267OctetAligned));
    TEST_MUSTPASS(!codec->SetAMRDecFormat(0, kRfc3267OctetAligned));
    TEST_MUSTPASS(!codec->SetAMREncFormat(0, kRfc3267FileStorage));
    TEST_MUSTPASS(!codec->SetAMRDecFormat(0, kRfc3267FileStorage));

    // Tests on AMRWB setencformat and setdecformat
    // These should fail
    TEST_MUSTPASS(!codec->SetAMRWbEncFormat(0, kRfc3267BwEfficient));
    TEST_MUSTPASS(!codec->SetAMRWbDecFormat(0, kRfc3267BwEfficient));
    TEST_MUSTPASS(!codec->SetAMRWbEncFormat(0, kRfc3267OctetAligned));
    TEST_MUSTPASS(!codec->SetAMRWbDecFormat(0, kRfc3267OctetAligned));
    TEST_MUSTPASS(!codec->SetAMRWbEncFormat(0, kRfc3267FileStorage));
    TEST_MUSTPASS(!codec->SetAMRWbDecFormat(0, kRfc3267FileStorage));

    TEST_LOG("Turn on VAD,G711 and set packet size to 30 ms:\n");
    strcpy(cinst.plname,"pcmu");
    cinst.pacsize=160;
    cinst.pltype=0;
    cinst.plfreq=8000;
    cinst.channels=1;
    cinst.rate=64000;
    TEST_MUSTPASS(codec->SetSendCodec(0, cinst));
    // The test here is confusing, what are we expecting? VADtest = false? 
    TEST_MUSTPASS(codec->GetVADStatus(0, VADtest, vadMode, disabledDTX));
    TEST_MUSTPASS(VADtest);
    TEST_MUSTPASS(codec->SetVADStatus(0, false, vadMode, true));

    // Set back to preferred codec
    TEST_MUSTPASS(codec->GetCodec(0, cinst));
    TEST_MUSTPASS(codec->SetSendCodec(0, cinst));

#else
    TEST_LOG("\n\n+++ Codec tests NOT ENABLED +++\n");
#endif // #ifdef _TEST_CODEC_

    /////////////////////////
    // Start another channel

#if defined(_TEST_RTP_RTCP_)
    TEST_LOG("\n\n+++ Preparing another channel for"
        " RTP/RTCP tests +++ \n\n");

    TEST_LOG("Create one more channel and start it up\n");
    TEST_MUSTPASS(!(1==base->CreateChannel()));
#ifdef WEBRTC_EXTERNAL_TRANSPORT
    my_transportation ch1transport(netw);
    TEST_MUSTPASS(netw->RegisterExternalTransport(1, ch1transport));
#else
    TEST_MUSTPASS(base->SetSendDestination(1, 8002, "127.0.0.1"));
    TEST_MUSTPASS(base->SetLocalReceiver(1, 8002));
#endif
    TEST_MUSTPASS(base->StartReceive(1));
    TEST_MUSTPASS(base->StartPlayout(1));
    TEST_MUSTPASS(rtp_rtcp->SetLocalSSRC(1, 5678)); // ensures SSSR_ch1 = 5678
    TEST_MUSTPASS(base->StartSend(1));
    SLEEP(2000);
#else
    TEST_LOG("\n\n+++ Preparing another channel NOT NEEDED +++ \n");
#endif // defined(_TEST_RTP_RTCP_)

    /////////////////
    // Conferencing

#ifndef _TEST_BASE_
    
    TEST_LOG("\n\n+++ (Base) tests NOT ENABLED +++\n");
#endif // #ifdef _TEST_BASE_

    ////////////////////////////////////////////////
    // RTP/RTCP (test after streaming is activated)

#if (defined(_TEST_RTP_RTCP_) && defined(_TEST_BASE_))

    TEST_LOG("\n\n+++ More RTP/RTCP tests +++\n\n");

    SLEEP(8000);

    TEST_LOG("Check that we have gotten RTCP packet, and collected CName\n");
    TEST_MUSTPASS(rtp_rtcp->GetRemoteRTCP_CNAME(0, tmpStr));
    TEST_LOG("default cname is %s", tmpStr);
    TEST_MUSTPASS(_stricmp("Niklas", tmpStr));

    TEST_LOG("Check that we have received the right SSRC\n");
    unsigned int ssrc1;
    TEST_MUSTPASS(rtp_rtcp->GetLocalSSRC(0, ssrc1));
    TEST_LOG("SSRC chan 0 = %lu \n", (long unsigned int) ssrc1);
    TEST_MUSTPASS(rtp_rtcp->GetRemoteSSRC(0, ssrc1));
    // the originally set 1234 should be maintained
    TEST_MUSTPASS(1234 != ssrc1);

    

    // RTCP APP tests
    TEST_LOG("Check RTCP APP send/receive \n");
    TEST_MUSTPASS(rtp_rtcp->RegisterRTCPObserver(0, myRtcpAppHandler));
    SLEEP(100);
    // send RTCP APP packet (fill up data message to multiple of 32 bits)
    const char* data = "application-dependent data------"; // multiple of 32byte
    unsigned short lenBytes(static_cast<unsigned short>(strlen(data)));
    unsigned int name = static_cast<unsigned int>(0x41424344); // 'ABCD';
    unsigned char subType = 1;
    TEST_MUSTPASS(rtp_rtcp->SendApplicationDefinedRTCPPacket(0,
                                                             subType,
                                                             name,
                                                             data,
                                                             lenBytes));
    TEST_LOG("Waiting for RTCP APP callback...\n");
    SLEEP(8000);    // ensures that RTCP is scheduled
    TEST_MUSTPASS(strlen(data) != myRtcpAppHandler._lengthBytes);
    TEST_MUSTPASS(memcmp(data, myRtcpAppHandler._data, lenBytes));
    TEST_MUSTPASS(myRtcpAppHandler._name != name);
    TEST_MUSTPASS(myRtcpAppHandler._subType != subType);
    TEST_LOG("=> application-dependent data of size %d bytes was received\n",
             lenBytes);
    // disable the callback and verify that no callback is received this time
    myRtcpAppHandler.Reset();
    TEST_MUSTPASS(rtp_rtcp->DeRegisterRTCPObserver(0));

    TEST_MUSTPASS(rtp_rtcp->SendApplicationDefinedRTCPPacket(0,
                                                             subType,
                                                             name,
                                                             data,
                                                             lenBytes));
    TEST_LOG("RTCP APP callback should not be received since the observer "
        "is disabled...\n");
    SLEEP(5000);    // ensures that RTCP is scheduled
    TEST_MUSTPASS(myRtcpAppHandler._name != 0);
    TEST_MUSTPASS(myRtcpAppHandler._subType != 0);





#if !defined(WEBRTC_EXTERNAL_TRANSPORT)
    printf("Tesing InsertExtraRTPPacket\n");

    const char payloadData[8] = {'A','B','C','D','E','F','G','H'};

    // fail tests
    // invalid channel
    TEST_MUSTPASS(-1 != rtp_rtcp->InsertExtraRTPPacket(-1,
                                                       0,
                                                       false,
                                                       payloadData,
                                                       8));
    // invalid payload type
    TEST_MUSTPASS(-1 != rtp_rtcp->InsertExtraRTPPacket(0,
                                                       -1,
                                                       false,
                                                       payloadData,
                                                       8));
    // invalid payload type
    TEST_MUSTPASS(-1 != rtp_rtcp->InsertExtraRTPPacket(0,
                                                       128,
                                                       false,
                                                       payloadData,
                                                       8));
    // invalid pointer
    TEST_MUSTPASS(-1 != rtp_rtcp->InsertExtraRTPPacket(0,
                                                       99,
                                                       false,
                                                       NULL,
                                                       8));
    // invalid size
    TEST_MUSTPASS(-1 != rtp_rtcp->InsertExtraRTPPacket(0,
                                                       99,
                                                       false,
                                                       payloadData,
                                                       1500 - 28 + 1));

    // transmit some extra RTP packets
    for (int pt = 0; pt < 128; pt++)
    {
        TEST_MUSTPASS(rtp_rtcp->InsertExtraRTPPacket(0,
                                                     pt,
                                                     false,
                                                     payloadData,
                                                     8));
        TEST_MUSTPASS(rtp_rtcp->InsertExtraRTPPacket(0,
                                                     pt,
                                                     true,
                                                     payloadData,
                                                     8));
    }
#else
    printf("Skipping InsertExtraRTPPacket tests -"
        " WEBRTC_EXTERNAL_TRANSPORT is defined \n");
#endif

    TEST_LOG("Enable the RTP observer\n");
    TEST_MUSTPASS(rtp_rtcp->RegisterRTPObserver(0, rtpObserver));
    TEST_MUSTPASS(rtp_rtcp->RegisterRTPObserver(1, rtpObserver));
    rtpObserver.Reset();

    // Create two RTP-dump files (3 seconds long).
    // Verify using rtpplay or NetEqRTPplay when test is done.
    TEST_LOG("Creating two RTP-dump files...\n");
    TEST_MUSTPASS(rtp_rtcp->StartRTPDump(0,
                                         GetFilename("dump_in_3sec.rtp"),
                                         kRtpIncoming));
    MARK();
    TEST_MUSTPASS(rtp_rtcp->StartRTPDump(0,
                                         GetFilename("dump_out_3sec.rtp"),
                                         kRtpOutgoing));
    MARK();
    SLEEP(3000);
    TEST_MUSTPASS(rtp_rtcp->StopRTPDump(0, kRtpIncoming));
    MARK();
    TEST_MUSTPASS(rtp_rtcp->StopRTPDump(0, kRtpOutgoing));
    MARK();

    rtpObserver.Reset();

    TEST_LOG("Verify the OnIncomingSSRCChanged callback\n");
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(rtp_rtcp->SetLocalSSRC(0, 7777));
    TEST_MUSTPASS(base->StartSend(0));
    SLEEP(500);
    TEST_MUSTPASS(rtpObserver._SSRC[0] != 7777);
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(rtp_rtcp->SetLocalSSRC(0, 1234));
    TEST_MUSTPASS(base->StartSend(0));
    SLEEP(500);
    TEST_MUSTPASS(rtpObserver._SSRC[0] != 1234);
    rtpObserver.Reset();
    if (file)
    {
        TEST_LOG("Start playing a file as microphone again...\n");
        TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0,
                                                         micFile,
                                                         true,
                                                         true));
    }

#ifdef WEBRTC_CODEC_RED
    TEST_LOG("Enabling FEC \n");
    TEST_MUSTPASS(rtp_rtcp->SetFECStatus(0, true));
    SLEEP(2000);

    TEST_LOG("Disabling FEC\n");
    TEST_MUSTPASS(rtp_rtcp->SetFECStatus(0, false));
    SLEEP(2000);
#else
    TEST_LOG("Skipping FEC tests - WEBRTC_CODEC_RED not defined \n");
#endif // #ifdef WEBRTC_CODEC_RED
#else
    TEST_LOG("\n\n+++ More RTP/RTCP tests NOT ENABLED +++\n");
#endif // #ifdef _TEST_RTP_RTCP_

    /////////////////////////
    // Delete extra channel

#if defined(_TEST_RTP_RTCP_)
    TEST_LOG("\n\n+++ Delete extra channel +++ \n\n");

    TEST_LOG("Delete channel 1, stopping everything\n");
    TEST_MUSTPASS(base->DeleteChannel(1));
#else
    TEST_LOG("\n\n+++ Delete extra channel NOT NEEDED +++ \n");
#endif // #if defined(WEBRTC_VOICE_ENGINE_CONFERENCING) && (define......

    /////////////////////////////////////////////////
    // Hardware (test after streaming is activated)

#ifdef _TEST_HARDWARE_
    TEST_LOG("\n\n+++ More hardware tests +++\n\n");


#if !defined(MAC_IPHONE) && !defined(ANDROID)
#ifdef _WIN32
    // should works also while already recording
    TEST_MUSTPASS(hardware->SetRecordingDevice(-1));
    // should works also while already playing
    TEST_MUSTPASS(hardware->SetPlayoutDevice(-1));
#else
    TEST_MUSTPASS(hardware->SetRecordingDevice(0));
    TEST_MUSTPASS(hardware->SetPlayoutDevice(0));
#endif
    TEST_MUSTPASS(hardware->GetRecordingDeviceName(0, devName, guidName));
    TEST_MUSTPASS(hardware->GetPlayoutDeviceName(0, devName, guidName));

    TEST_MUSTPASS(hardware->GetNumOfRecordingDevices(nRec));
    TEST_MUSTPASS(hardware->GetNumOfPlayoutDevices(nPlay));
#endif
    
    int load = -1;
    
#if defined(_WIN32)
    TEST_MUSTPASS(hardware->GetCPULoad(load));
    TEST_MUSTPASS(load == -1);
    TEST_LOG("VE CPU load     = %d\n", load);
#else
    TEST_MUSTPASS(!hardware->GetCPULoad(load));
#endif

#if !defined(WEBRTC_MAC) && !defined(ANDROID)
    // Not supported on Mac yet
    load = -1;
    TEST_MUSTPASS(hardware->GetSystemCPULoad(load));
    TEST_MUSTPASS(load == -1);
    TEST_LOG("System CPU load = %d\n", load);
#endif
    
#ifdef MAC_IPHONE
    // Reset sound device
    TEST_LOG("Reset sound device \n");
    TEST_MUSTPASS(hardware->ResetAudioDevice());
    SLEEP(2000);
#endif // #ifdef MAC_IPHONE

#else
    TEST_LOG("\n\n+++ More hardware tests NOT ENABLED +++\n");
#endif

    ////////
    // Dtmf

#ifdef _TEST_DTMF_
    TEST_LOG("\n\n+++ Dtmf tests +++\n\n");

    TEST_LOG("Making sure Dtmf Feedback is enabled by default \n");
    bool dtmfFeedback = false, dtmfDirectFeedback = true;
    TEST_MUSTPASS(dtmf->GetDtmfFeedbackStatus(dtmfFeedback,
                                              dtmfDirectFeedback));
    TEST_MUSTPASS(!dtmfFeedback);
    TEST_MUSTPASS(dtmfDirectFeedback);

    // Add support when new 4.0 API is complete
#if (defined(WEBRTC_DTMF_DETECTION) && !defined(_INSTRUMENTATION_TESTING_))
    DtmfCallback *d = new DtmfCallback();

    // Set codec to PCMU to make sure tones are not distorted
    TEST_LOG("Setting codec to PCMU\n");
    CodecInst ci;
    ci.channels = 1;
    ci.pacsize = 160;
    ci.plfreq = 8000;
    ci.pltype = 0;
    ci.rate = 64000;
    strcpy(ci.plname, "PCMU");
    TEST_MUSTPASS(codec->SetSendCodec(0, ci));

    // Loop the different detections methods
    TelephoneEventDetectionMethods detMethod = kInBand;
    for (int h=0; h<3; ++h)
    {
        if (0 == h)
        {
            TEST_LOG("Testing telephone-event (Dtmf) detection"
                " using in-band method \n");
            TEST_LOG("  In-band events should be detected \n");
            TEST_LOG("  Out-of-band Dtmf events (0-15) should be"
                " detected \n");
            TEST_LOG("  Out-of-band non-Dtmf events (>15) should NOT be"
                " detected \n");
            detMethod = kInBand;
        }
        if (1 == h)
        {
            TEST_LOG("Testing telephone-event (Dtmf) detection using"
                " out-of-band method\n");
            TEST_LOG("  In-band events should NOT be detected \n");
            TEST_LOG("  Out-of-band events should be detected \n");
            detMethod = kOutOfBand;
        }
        if (2 == h)
        {
            TEST_LOG("Testing telephone-event (Dtmf) detection using both"
                " in-band and out-of-band methods\n");
            TEST_LOG("  In-band events should be detected \n");
            TEST_LOG("  Out-of-band Dtmf events (0-15) should be detected"
                " TWICE \n");
            TEST_LOG("  Out-of-band non-Dtmf events (>15) should be detected"
                " ONCE \n");
            detMethod = kInAndOutOfBand;
        }
        TEST_MUSTPASS(dtmf->RegisterTelephoneEventDetection(0, detMethod, *d));
#else
        TEST_LOG("Skipping Dtmf detection tests - WEBRTC_DTMF_DETECTION not"
            " defined or _INSTRUMENTATION_TESTING_ defined \n");
#endif

        TEST_MUSTPASS(dtmf->SetDtmfFeedbackStatus(false));
        TEST_LOG("Sending in-band telephone events:");
        for(int i = 0; i < 16; i++)
        {
            TEST_LOG("\n  %d ", i); fflush(NULL);
            TEST_MUSTPASS(dtmf->SendTelephoneEvent(0, i, false, 160, 10));
            SLEEP(500);
        }
#ifdef WEBRTC_CODEC_AVT
        TEST_LOG("\nSending out-of-band telephone events:");
        for(int i = 0; i < 16; i++)
        {
            TEST_LOG("\n  %d ", i); fflush(NULL);
            TEST_MUSTPASS(dtmf->SendTelephoneEvent(0, i, true));
            SLEEP(500);
        }
        // Testing 2 non-Dtmf events
        int num = 32;
        TEST_LOG("\n  %d ", num); fflush(NULL);
        TEST_MUSTPASS(dtmf->SendTelephoneEvent(0, num, true));
        SLEEP(500);
        num = 110;
        TEST_LOG("\n  %d ", num); fflush(NULL);
        TEST_MUSTPASS(dtmf->SendTelephoneEvent(0, num, true));
        SLEEP(500);
        ANL();
#endif
#if (defined(WEBRTC_DTMF_DETECTION) && !defined(_INSTRUMENTATION_TESTING_))
        TEST_MUSTPASS(dtmf->DeRegisterTelephoneEventDetection(0));
        TEST_LOG("Detected %d events \n", d->counter);
        int expectedCount = 32; // For 0 == h
        if (1 == h) expectedCount = 18;
        if (2 == h) expectedCount = 50;
        TEST_MUSTPASS(d->counter != expectedCount);
        d->counter = 0;
    } // for loop

    TEST_LOG("Testing no detection after disabling:");
    TEST_MUSTPASS(dtmf->DeRegisterTelephoneEventDetection(0));
    TEST_LOG(" 0");
    TEST_MUSTPASS(dtmf->SendTelephoneEvent(0, 0, false));
    SLEEP(500);
    TEST_LOG(" 1");
    TEST_MUSTPASS(dtmf->SendTelephoneEvent(0, 1, true));
    SLEEP(500);
    TEST_LOG("\nDtmf tones sent: 2, detected: %d \n", d->counter);
    TEST_MUSTPASS(0 != d->counter);
    delete d;

    TEST_MUSTPASS(codec->GetCodec(0, ci));
    TEST_LOG("Back to first codec in list: %s\n", ci.plname);
    TEST_MUSTPASS(codec->SetSendCodec(0, ci));
#endif


#ifndef MAC_IPHONE
#ifdef WEBRTC_CODEC_AVT
    TEST_LOG("Disabling Dtmf playout (no tone should be heard) \n");
    TEST_MUSTPASS(dtmf->SetDtmfPlayoutStatus(0, false));
    TEST_MUSTPASS(dtmf->SendTelephoneEvent(0, 0, true));
    SLEEP(500);

    TEST_LOG("Enabling Dtmf playout (tone should be heard) \n");
    TEST_MUSTPASS(dtmf->SetDtmfPlayoutStatus(0, true));
    TEST_MUSTPASS(dtmf->SendTelephoneEvent(0, 0, true));
    SLEEP(500);
#endif
#endif
    
    TEST_LOG("Playing Dtmf tone locally \n");
///    TEST_MUSTPASS(dtmf->PlayDtmfTone(0, 300, 15));
    SLEEP(500);
#ifdef WEBRTC_CODEC_AVT
    CodecInst c2;

    TEST_LOG("Changing Dtmf payload type \n");

    // Start by modifying the receiving side
    if (codec)
    {
        int nc = codec->NumOfCodecs();
        for(int i = 0; i < nc; i++)
        {
            TEST_MUSTPASS(codec->GetCodec(i, c2));
            if(!_stricmp("telephone-event", c2.plname))
            {
                c2.pltype = 88;    // use 88 instead of default 106
                TEST_MUSTPASS(base->StopSend(0));
                TEST_MUSTPASS(base->StopPlayout(0));
                TEST_MUSTPASS(base->StopReceive(0));
                TEST_MUSTPASS(codec->SetRecPayloadType(0, c2));
                TEST_MUSTPASS(base->StartReceive(0));
                TEST_MUSTPASS(base->StartPlayout(0));
                TEST_MUSTPASS(base->StartSend(0));
                TEST_LOG("Start playing a file as microphone again \n");
                TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0,
                                                                 micFile,
                                                                 true,
                                                                 true));
                break;
            }
        }
    }

    SLEEP(500);

    // Next, we must modify the sending side as well
    TEST_MUSTPASS(dtmf->SetSendTelephoneEventPayloadType(0, c2.pltype));

    TEST_LOG("Outband Dtmf test with modified Dtmf payload:");
    for(int i = 0; i < 16; i++)
    {
        TEST_LOG(" %d", i);
        fflush(NULL);
        TEST_MUSTPASS(dtmf->SendTelephoneEvent(0, i, true));
        SLEEP(500);
    }
    ANL();
#endif
    TEST_MUSTPASS(dtmf->SetDtmfFeedbackStatus(true, false));
#else
    TEST_LOG("\n\n+++ Dtmf tests NOT ENABLED +++\n");
#endif  // #ifdef _TEST_DTMF_

    //////////
    // Volume

#ifdef _TEST_VOLUME_
    TEST_LOG("\n\n+++ Volume tests +++\n\n");

#if !defined(MAC_IPHONE)
    // Speaker volume test
    unsigned int vol = 1000;
    TEST_LOG("Saving Speaker volume\n");
    TEST_MUSTPASS(volume->GetSpeakerVolume(vol));
    TEST_MUSTPASS(!(vol <= 255));
    TEST_LOG("Setting speaker volume to 0\n");
    TEST_MUSTPASS(volume->SetSpeakerVolume(0));
    SLEEP(1000);
    TEST_LOG("Setting speaker volume to 255\n");
    TEST_MUSTPASS(volume->SetSpeakerVolume(255));
    SLEEP(1000);
    TEST_LOG("Setting speaker volume back to saved value\n");
    TEST_MUSTPASS(volume->SetSpeakerVolume(vol));
    SLEEP(1000);
#endif // #if !defined(MAC_IPHONE)

    if (file)
    {
        TEST_LOG("==> Talk into the microphone \n");
        TEST_MUSTPASS(file->StopPlayingFileAsMicrophone(0));
        SLEEP(1000);
    }

#if (!defined(MAC_IPHONE) && !defined(ANDROID))
    // Mic volume test
#if defined(_TEST_AUDIO_PROCESSING_) && defined(WEBRTC_VOICE_ENGINE_AGC)
    bool agcTemp(true);
    AgcModes agcModeTemp(kAgcAdaptiveAnalog);
    TEST_MUSTPASS(apm->GetAgcStatus(agcTemp, agcModeTemp)); // current state
    TEST_LOG("Turn off AGC\n");
    TEST_MUSTPASS(apm->SetAgcStatus(false));
#endif
    TEST_LOG("Saving Mic volume\n");
    TEST_MUSTPASS(volume->GetMicVolume(vol));
    TEST_MUSTPASS(!(vol <= 255));
    TEST_LOG("Setting Mic volume to 0\n");
    TEST_MUSTPASS(volume->SetMicVolume(0));
    SLEEP(1000);
    TEST_LOG("Setting Mic volume to 255\n");
    TEST_MUSTPASS(volume->SetMicVolume(255));
    SLEEP(1000);
    TEST_LOG("Setting Mic volume back to saved value\n");
    TEST_MUSTPASS(volume->SetMicVolume(vol));
    SLEEP(1000);
#if defined(_TEST_AUDIO_PROCESSING_) && defined(WEBRTC_VOICE_ENGINE_AGC)
    TEST_LOG("Reset AGC to previous state\n");
    TEST_MUSTPASS(apm->SetAgcStatus(agcTemp, agcModeTemp));
#endif
#endif // #if (!defined(MAC_IPHONE) && !defined(ANDROID))

    // Input mute test
    TEST_LOG("Enabling input muting\n");
    bool mute = true;
    TEST_MUSTPASS(volume->GetInputMute(0, mute));
    TEST_MUSTPASS(mute);
    TEST_MUSTPASS(volume->SetInputMute(0, true));
    TEST_MUSTPASS(volume->GetInputMute(0, mute));
    TEST_MUSTPASS(!mute);
    SLEEP(1000);
    TEST_LOG("Disabling input muting\n");
    TEST_MUSTPASS(volume->SetInputMute(0, false));
    TEST_MUSTPASS(volume->GetInputMute(0, mute));
    TEST_MUSTPASS(mute);
    SLEEP(1000);

#if (!defined(MAC_IPHONE) && !defined(ANDROID))
    // System output mute test
    TEST_LOG("Enabling system output muting\n");
    bool outputMute = true;
    TEST_MUSTPASS(volume->GetSystemOutputMute(outputMute));
    TEST_MUSTPASS(outputMute);
    TEST_MUSTPASS(volume->SetSystemOutputMute(true));
    TEST_MUSTPASS(volume->GetSystemOutputMute(outputMute));
    TEST_MUSTPASS(!outputMute);
    SLEEP(1000);
    TEST_LOG("Disabling system output muting\n");
    TEST_MUSTPASS(volume->SetSystemOutputMute(false));
    TEST_MUSTPASS(volume->GetSystemOutputMute(outputMute));
    TEST_MUSTPASS(outputMute);
    SLEEP(1000);

    // System Input mute test
    TEST_LOG("Enabling system input muting\n");
    bool inputMute = true;
    TEST_MUSTPASS(volume->GetSystemInputMute(inputMute));
    TEST_MUSTPASS(inputMute);
    TEST_MUSTPASS(volume->SetSystemInputMute(true));
    // This is needed to avoid error using pulse
    SLEEP(100);
    TEST_MUSTPASS(volume->GetSystemInputMute(inputMute));
    TEST_MUSTPASS(!inputMute);
    SLEEP(1000);
    TEST_LOG("Disabling system input muting\n");
    TEST_MUSTPASS(volume->SetSystemInputMute(false));
    // This is needed to avoid error using pulse
    SLEEP(100);
    TEST_MUSTPASS(volume->GetSystemInputMute(inputMute));
    TEST_MUSTPASS(inputMute);
    SLEEP(1000);
#endif // #if (!defined(MAC_IPHONE) && !defined(ANDROID))

#if(!defined(MAC_IPHONE) && !defined(ANDROID))
    // Test Input & Output levels
    TEST_LOG("Testing input & output levels for 10 seconds (dT=1 second)\n");
    TEST_LOG("Speak in microphone to vary the levels...\n");
    unsigned int inputLevel(0);
    unsigned int outputLevel(0);
    unsigned int inputLevelFullRange(0);
    unsigned int outputLevelFullRange(0);

    for (int t = 0; t < 5; t++)
    {
        SLEEP(1000);
        TEST_MUSTPASS(volume->GetSpeechInputLevel(inputLevel));
        TEST_MUSTPASS(volume->GetSpeechOutputLevel(0, outputLevel));
        TEST_MUSTPASS(volume->GetSpeechInputLevelFullRange(
            inputLevelFullRange));
        TEST_MUSTPASS(volume->GetSpeechOutputLevelFullRange(
            0, outputLevelFullRange));
        TEST_LOG("    warped levels (0-9)    : in=%5d, out=%5d\n",
                 inputLevel, outputLevel);
        TEST_LOG("    linear levels (0-32768): in=%5d, out=%5d\n",
                 inputLevelFullRange, outputLevelFullRange);
    }
#endif // #if (!defined(MAC_IPHONE) && !defined(ANDROID))

    if (file)
    {
        TEST_LOG("==> Start playing a file as microphone again \n");
        TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0,
                                                         micFile,
                                                         true,
                                                         true));
        SLEEP(1000);
    }

#if !defined(MAC_IPHONE)
    // Channel scaling test
    TEST_LOG("Channel scaling\n");
    float scaling = -1.0;
    TEST_MUSTPASS(volume->GetChannelOutputVolumeScaling(0, scaling));
    TEST_MUSTPASS(1.0 != scaling);
    TEST_MUSTPASS(volume->SetChannelOutputVolumeScaling(0, (float)0.1));
    TEST_MUSTPASS(volume->GetChannelOutputVolumeScaling(0, scaling));
    TEST_MUSTPASS(!((scaling > 0.099) && (scaling < 0.101)));
    SLEEP(1000);
    TEST_MUSTPASS(volume->SetChannelOutputVolumeScaling(0, (float)1.0));
    TEST_MUSTPASS(volume->GetChannelOutputVolumeScaling(0, scaling));
    TEST_MUSTPASS(1.0 != scaling);
#endif // #if !defined(MAC_IPHONE)

#if !defined(MAC_IPHONE) && !defined(ANDROID)
    // Channel panning test
    TEST_LOG("Channel panning\n");
    float left = -1.0, right = -1.0;
    TEST_MUSTPASS(volume->GetOutputVolumePan(0, left, right));
    TEST_MUSTPASS(!((left == 1.0) && (right == 1.0)));
    TEST_LOG("Panning to left\n");
    TEST_MUSTPASS(volume->SetOutputVolumePan(0, (float)0.8, (float)0.1));
    TEST_MUSTPASS(volume->GetOutputVolumePan(0, left, right));
    TEST_MUSTPASS(!((left > 0.799) && (left < 0.801)));
    TEST_MUSTPASS(!((right > 0.099) && (right < 0.101)));
    SLEEP(1000);
    TEST_LOG("Back to center\n");
    TEST_MUSTPASS(volume->SetOutputVolumePan(0, (float)1.0, (float)1.0));
    SLEEP(1000);
    left = -1.0; right = -1.0;
    TEST_MUSTPASS(volume->GetOutputVolumePan(0, left, right));
    TEST_MUSTPASS(!((left == 1.0) && (right == 1.0)));
    TEST_LOG("Panning channel to right\n");
    TEST_MUSTPASS(volume->SetOutputVolumePan(0, (float)0.1, (float)0.8));
    SLEEP(100);
    TEST_MUSTPASS(volume->GetOutputVolumePan(0, left, right));
    TEST_MUSTPASS(!((left > 0.099) && (left < 0.101)));
    TEST_MUSTPASS(!((right > 0.799) && (right < 0.801)));
    SLEEP(1000);
    TEST_LOG("Channel back to center\n");
    TEST_MUSTPASS(volume->SetOutputVolumePan(0, (float)1.0, (float)1.0));
    SLEEP(1000);
#else
    TEST_LOG("Skipping stereo tests\n");
#endif // #if !defined(MAC_IPHONE) && !defined(ANDROID))

#else
    TEST_LOG("\n\n+++ Volume tests NOT ENABLED +++\n");
#endif // #ifdef _TEST_VOLUME_

    ///////
    // AudioProcessing

#ifdef _TEST_AUDIO_PROCESSING_
    TEST_LOG("\n\n+++ AudioProcessing tests +++\n\n");
#ifdef WEBRTC_VOICE_ENGINE_AGC
    bool test;
    TEST_LOG("AGC calls\n");
#if (defined(MAC_IPHONE) || defined(ANDROID))
    TEST_LOG("Must be OFF by default\n");
    test = true;
    AgcModes agcMode = kAgcAdaptiveAnalog;
    TEST_MUSTPASS(apm->GetAgcStatus(test, agcMode));
    TEST_MUSTPASS(test);
    TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);
#else
    TEST_LOG("Must be ON by default\n");
    test = false;
    AgcModes agcMode = kAgcAdaptiveAnalog;
    TEST_MUSTPASS(apm->GetAgcStatus(test, agcMode));
    TEST_MUSTPASS(!test);
    TEST_MUSTPASS(kAgcAdaptiveAnalog != agcMode);

    TEST_LOG("Turn off AGC\n");
    // must set value in first call!
    TEST_MUSTPASS(apm->SetAgcStatus(false, kAgcDefault));
    TEST_LOG("Should be OFF now\n");
    TEST_MUSTPASS(apm->GetAgcStatus(test, agcMode));
    TEST_MUSTPASS(test);
    TEST_MUSTPASS(kAgcAdaptiveAnalog != agcMode);
#endif // #if (defined(MAC_IPHONE) || defined(ANDROID))

    TEST_LOG("Turn ON AGC\n");
#if (defined(MAC_IPHONE) || defined(ANDROID))
    TEST_MUSTPASS(apm->SetAgcStatus(true, kAgcAdaptiveDigital));
#else
    TEST_MUSTPASS(apm->SetAgcStatus(true));
#endif
    TEST_LOG("Should be ON now\n");
    TEST_MUSTPASS(apm->GetAgcStatus(test, agcMode));
    TEST_MUSTPASS(!test);
#if (defined(MAC_IPHONE) || defined(ANDROID))
    TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);
#else
    TEST_MUSTPASS(kAgcAdaptiveAnalog != agcMode);
#endif

#if (defined(MAC_IPHONE) || defined(ANDROID))
    TEST_LOG("Testing Type settings\n");
    // Should fail
    TEST_MUSTPASS(!apm->SetAgcStatus(true, kAgcAdaptiveAnalog));
    // Should fail
    TEST_MUSTPASS(apm->SetAgcStatus(true, kAgcFixedDigital));
    // Should fail
    TEST_MUSTPASS(apm->SetAgcStatus(true, kAgcAdaptiveDigital));

    TEST_LOG("Turn off AGC\n");
    TEST_MUSTPASS(apm->SetAgcStatus(false));
    TEST_LOG("Should be OFF now\n");
    TEST_MUSTPASS(apm->GetAgcStatus(test, agcMode));
    TEST_MUSTPASS(test);
    TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);
#else
    TEST_LOG("Testing Mode settings\n");
    TEST_MUSTPASS(apm->SetAgcStatus(true, kAgcFixedDigital));
    TEST_MUSTPASS(apm->GetAgcStatus(test, agcMode));
    TEST_MUSTPASS(kAgcFixedDigital != agcMode);
    TEST_MUSTPASS(apm->SetAgcStatus(true, kAgcAdaptiveDigital));
    TEST_MUSTPASS(apm->GetAgcStatus(test, agcMode));
    TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);
    TEST_MUSTPASS(apm->SetAgcStatus(true, kAgcAdaptiveAnalog));
    TEST_MUSTPASS(apm->GetAgcStatus(test, agcMode));
    TEST_MUSTPASS(kAgcAdaptiveAnalog != agcMode);
#endif // #if (defined(MAC_IPHONE) || defined(ANDROID))

    TEST_LOG("rxAGC calls\n");
    // Note the following test is not tested in iphone, android and wince,
    // you may run into issue

    bool rxAGCTemp(false);
    AgcModes rxAGCModeTemp(kAgcAdaptiveAnalog);
    // Store current state
    TEST_MUSTPASS(apm->GetAgcStatus(rxAGCTemp, rxAGCModeTemp));
    TEST_LOG("Turn off near-end AGC\n");
    TEST_MUSTPASS(apm->SetAgcStatus(false));

    TEST_LOG("rxAGC Must be OFF by default\n");
    test = true;
    AgcModes rxAGCMode = kAgcAdaptiveDigital;
    TEST_MUSTPASS(apm->GetRxAgcStatus(0, test, agcMode));
    TEST_MUSTPASS(test);
    TEST_MUSTPASS(kAgcAdaptiveDigital != rxAGCMode);

    TEST_LOG("Turn off rxAGC\n");
    // must set value in first call!
    TEST_MUSTPASS(apm->SetRxAgcStatus(0, false, kAgcDefault));
    TEST_LOG("Should be OFF now\n");
    TEST_MUSTPASS(apm->GetRxAgcStatus(0, test, agcMode));
    TEST_MUSTPASS(test);
    TEST_MUSTPASS(kAgcAdaptiveDigital != rxAGCMode);

    TEST_LOG("Turn ON AGC\n");
    TEST_MUSTPASS(apm->SetRxAgcStatus(0, true));
    TEST_LOG("Should be ON now\n");
    TEST_MUSTPASS(apm->GetRxAgcStatus(0, test, agcMode));
    TEST_MUSTPASS(!test);
    TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);

    TEST_LOG("Testing Type settings\n");
    // Should fail
    TEST_MUSTPASS(!apm->SetRxAgcStatus(0, true, kAgcAdaptiveAnalog));
    TEST_MUSTPASS(apm->SetRxAgcStatus(0, true, kAgcFixedDigital));
    TEST_MUSTPASS(apm->GetRxAgcStatus(0, test, agcMode));
    TEST_MUSTPASS(kAgcFixedDigital != agcMode);
    TEST_MUSTPASS(apm->SetRxAgcStatus(0, true, kAgcAdaptiveDigital));
    TEST_MUSTPASS(apm->GetRxAgcStatus(0, test, agcMode));
    TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);

    TEST_LOG("Turn off AGC\n");
    TEST_MUSTPASS(apm->SetRxAgcStatus(0, false));
    TEST_LOG("Should be OFF now\n");
    TEST_MUSTPASS(apm->GetRxAgcStatus(0, test, agcMode));
    TEST_MUSTPASS(test);
    TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);

    // recover the old AGC mode
    TEST_MUSTPASS(apm->SetAgcStatus(rxAGCTemp, rxAGCModeTemp));

#else
    TEST_LOG("Skipping AGC tests - WEBRTC_VOICE_ENGINE_AGC not defined \n");
#endif  // #ifdef WEBRTC_VOICE_ENGINE_AGC

#ifdef WEBRTC_VOICE_ENGINE_ECHO
    TEST_LOG("EC calls\n");
    TEST_LOG("Must be OFF by default\n");
#if (defined(MAC_IPHONE) || defined(ANDROID))
    const EcModes ecModeDefault = kEcAecm;
#else
    const EcModes ecModeDefault = kEcAec;
#endif
    test = true;
    EcModes ecMode = kEcAec;
    AecmModes aecmMode = kAecmSpeakerphone;
    bool enabledCNG(false);
    TEST_MUSTPASS(apm->GetEcStatus(test, ecMode));
    TEST_MUSTPASS(test);
    TEST_MUSTPASS(ecModeDefault != ecMode);
    TEST_MUSTPASS(apm->GetAecmMode(aecmMode, enabledCNG));
    TEST_LOG("default AECM: mode=%d CNG: mode=%d\n",aecmMode, enabledCNG);
    TEST_MUSTPASS(kAecmSpeakerphone != aecmMode);
    TEST_MUSTPASS(enabledCNG != true);
    TEST_MUSTPASS(apm->SetAecmMode(kAecmQuietEarpieceOrHeadset, false));
    TEST_MUSTPASS(apm->GetAecmMode(aecmMode, enabledCNG));
    TEST_LOG("change AECM to mode=%d CNG to false\n",aecmMode);
    TEST_MUSTPASS(aecmMode != kAecmQuietEarpieceOrHeadset);
    TEST_MUSTPASS(enabledCNG != false);

    TEST_LOG("Turn ON EC\n");
    TEST_MUSTPASS(apm->SetEcStatus(true, ecModeDefault));
    TEST_LOG("Should be ON now\n");
    TEST_MUSTPASS(apm->GetEcStatus(test, ecMode));
    TEST_MUSTPASS(!test);
    TEST_MUSTPASS(ecModeDefault != ecMode);

#if (!defined(MAC_IPHONE) && !defined(ANDROID))
    TEST_MUSTPASS(apm->SetEcStatus(true, kEcAec));
    TEST_MUSTPASS(apm->GetEcStatus(test, ecMode));
    TEST_MUSTPASS(kEcAec != ecMode);

    TEST_MUSTPASS(apm->SetEcStatus(true, kEcConference));
    TEST_MUSTPASS(apm->GetEcStatus(test, ecMode));
    TEST_MUSTPASS(kEcAec != ecMode);


    // the samplefreq for AudioProcessing is 32k, so it wont work to
    // activate AECM
    TEST_MUSTPASS(apm->SetEcStatus(true, kEcAecm));
    TEST_MUSTPASS(apm->GetEcStatus(test, ecMode));
    TEST_MUSTPASS(kEcAecm != ecMode);
#endif

    // set kEcAecm mode
    TEST_LOG("Testing AECM Mode settings\n");
    TEST_MUSTPASS(apm->SetEcStatus(true, kEcAecm));
    TEST_MUSTPASS(apm->GetEcStatus(test, ecMode));
    TEST_LOG("EC: enabled=%d, ECmode=%d\n", test, ecMode);
    TEST_MUSTPASS(test != true);
    TEST_MUSTPASS(ecMode != kEcAecm);

    // AECM mode, get and set 
    TEST_MUSTPASS(apm->GetAecmMode(aecmMode, enabledCNG));
    TEST_MUSTPASS(aecmMode != kAecmQuietEarpieceOrHeadset);
    TEST_MUSTPASS(enabledCNG != false);
    TEST_MUSTPASS(apm->SetAecmMode(kAecmEarpiece, true));
    TEST_MUSTPASS(apm->GetAecmMode(aecmMode, enabledCNG));
    TEST_LOG("AECM: mode=%d CNG: mode=%d\n",aecmMode, enabledCNG);
    TEST_MUSTPASS(aecmMode != kAecmEarpiece);
    TEST_MUSTPASS(enabledCNG != true);
    TEST_MUSTPASS(apm->SetAecmMode(kAecmEarpiece, false));
    TEST_MUSTPASS(apm->GetAecmMode(aecmMode, enabledCNG));
    TEST_LOG("AECM: mode=%d CNG: mode=%d\n",aecmMode, enabledCNG);
    TEST_MUSTPASS(aecmMode != kAecmEarpiece);
    TEST_MUSTPASS(enabledCNG != false);
    TEST_MUSTPASS(apm->SetAecmMode(kAecmLoudEarpiece, true));
    TEST_MUSTPASS(apm->GetAecmMode(aecmMode, enabledCNG));
    TEST_LOG("AECM: mode=%d CNG: mode=%d\n",aecmMode, enabledCNG);
    TEST_MUSTPASS(aecmMode != kAecmLoudEarpiece);
    TEST_MUSTPASS(enabledCNG != true);
    TEST_MUSTPASS(apm->SetAecmMode(kAecmSpeakerphone, false));
    TEST_MUSTPASS(apm->GetAecmMode(aecmMode, enabledCNG));
    TEST_LOG("AECM: mode=%d CNG: mode=%d\n",aecmMode, enabledCNG);
    TEST_MUSTPASS(aecmMode != kAecmSpeakerphone);
    TEST_MUSTPASS(enabledCNG != false);
    TEST_MUSTPASS(apm->SetAecmMode(kAecmLoudSpeakerphone, true));
    TEST_MUSTPASS(apm->GetAecmMode(aecmMode, enabledCNG));
    TEST_LOG("AECM: mode=%d CNG: mode=%d\n",aecmMode, enabledCNG);
    TEST_MUSTPASS(aecmMode != kAecmLoudSpeakerphone);
    TEST_MUSTPASS(enabledCNG != true);

    TEST_LOG("Turn OFF AEC\n");
    TEST_MUSTPASS(apm->SetEcStatus(false));
    TEST_LOG("Should be OFF now\n");
    TEST_MUSTPASS(apm->GetEcStatus(test, ecMode));
    TEST_MUSTPASS(test);
#else
    TEST_LOG("Skipping echo cancellation tests -"
        " WEBRTC_VOICE_ENGINE_ECHO not defined \n");
#endif  // #ifdef WEBRTC_VOICE_ENGINE_ECHO

#ifdef WEBRTC_VOICE_ENGINE_NR
    TEST_LOG("NS calls\n");
    TEST_LOG("Must be OFF by default\n");

    NsModes nsModeDefault = kNsModerateSuppression;

    test = true;
    NsModes nsMode = kNsVeryHighSuppression;
    TEST_MUSTPASS(apm->GetNsStatus(test, nsMode));
    TEST_MUSTPASS(test);
    TEST_MUSTPASS(nsModeDefault != nsMode);

    TEST_LOG("Turn ON NS\n");
    TEST_MUSTPASS(apm->SetNsStatus(true));
    TEST_LOG("Should be ON now\n");
    TEST_MUSTPASS(apm->GetNsStatus(test, nsMode));
    TEST_MUSTPASS(!test);
    TEST_MUSTPASS(nsModeDefault != nsMode);

    TEST_LOG("Testing Mode settings\n");
    TEST_MUSTPASS(apm->SetNsStatus(true, kNsLowSuppression));
    TEST_MUSTPASS(apm->GetNsStatus(test, nsMode));
    TEST_MUSTPASS(kNsLowSuppression != nsMode);
    TEST_MUSTPASS(apm->SetNsStatus(true, kNsModerateSuppression));
    TEST_MUSTPASS(apm->GetNsStatus(test, nsMode));
    TEST_MUSTPASS(kNsModerateSuppression != nsMode);
    TEST_MUSTPASS(apm->SetNsStatus(true, kNsHighSuppression));
    TEST_MUSTPASS(apm->GetNsStatus(test, nsMode));
    TEST_MUSTPASS(kNsHighSuppression != nsMode);
    TEST_MUSTPASS(apm->SetNsStatus(true, kNsVeryHighSuppression));
    TEST_MUSTPASS(apm->GetNsStatus(test, nsMode));
    TEST_MUSTPASS(kNsVeryHighSuppression != nsMode);
    TEST_MUSTPASS(apm->SetNsStatus(true, kNsConference));
    TEST_MUSTPASS(apm->GetNsStatus(test, nsMode));
    TEST_MUSTPASS(kNsHighSuppression != nsMode);
    TEST_MUSTPASS(apm->SetNsStatus(true, kNsDefault));
    TEST_MUSTPASS(apm->GetNsStatus(test, nsMode));
    TEST_MUSTPASS(nsModeDefault != nsMode);

    TEST_LOG("Turn OFF NS\n");
    TEST_MUSTPASS(apm->SetNsStatus(false));
    TEST_LOG("Should be OFF now\n");
    TEST_MUSTPASS(apm->GetNsStatus(test, nsMode));
    TEST_MUSTPASS(test);


    TEST_LOG("rxNS calls\n");
    TEST_LOG("rxNS Must be OFF by default\n");

    TEST_MUSTPASS(apm->GetRxNsStatus(0, test, nsMode));
    TEST_MUSTPASS(test);
    TEST_MUSTPASS(nsModeDefault != nsMode);

    TEST_LOG("Turn ON rxNS\n");
    TEST_MUSTPASS(apm->SetRxNsStatus(0, true));
    TEST_LOG("Should be ON now\n");
    TEST_MUSTPASS(apm->GetRxNsStatus(0, test, nsMode));
    TEST_MUSTPASS(!test);
    TEST_MUSTPASS(nsModeDefault != nsMode);

    TEST_LOG("Testing Mode settings\n");
    TEST_MUSTPASS(apm->SetRxNsStatus(0, true, kNsLowSuppression));
    TEST_MUSTPASS(apm->GetRxNsStatus(0, test, nsMode));
    TEST_MUSTPASS(kNsLowSuppression != nsMode);
    TEST_MUSTPASS(apm->SetRxNsStatus(0, true, kNsModerateSuppression));
    TEST_MUSTPASS(apm->GetRxNsStatus(0, test, nsMode));
    TEST_MUSTPASS(kNsModerateSuppression != nsMode);
    TEST_MUSTPASS(apm->SetRxNsStatus(0, true, kNsHighSuppression));
    TEST_MUSTPASS(apm->GetRxNsStatus(0, test, nsMode));
    TEST_MUSTPASS(kNsHighSuppression != nsMode);
    TEST_MUSTPASS(apm->SetRxNsStatus(0, true, kNsVeryHighSuppression));
    TEST_MUSTPASS(apm->GetRxNsStatus(0, test, nsMode));
    TEST_MUSTPASS(kNsVeryHighSuppression != nsMode);
    TEST_MUSTPASS(apm->SetRxNsStatus(0, true, kNsConference));
    TEST_MUSTPASS(apm->GetRxNsStatus(0, test, nsMode));
    TEST_MUSTPASS(kNsHighSuppression != nsMode);
    TEST_MUSTPASS(apm->SetRxNsStatus(0, true, kNsDefault));
    TEST_MUSTPASS(apm->GetRxNsStatus(0, test, nsMode));
    TEST_MUSTPASS(nsModeDefault != nsMode);

    TEST_LOG("Turn OFF NS\n");
    TEST_MUSTPASS(apm->SetRxNsStatus(0, false));
    TEST_LOG("Should be OFF now\n");
    TEST_MUSTPASS(apm->GetRxNsStatus(0, test, nsMode));
    TEST_MUSTPASS(test);

#else
    TEST_LOG("Skipping NS tests - WEBRTC_VOICE_ENGINE_NR not defined \n");
#endif  // #ifdef WEBRTC_VOICE_ENGINE_NR

    // TODO(xians), enable the metrics test when APM is ready
    /*
#if (!defined(MAC_IPHONE) && !defined(ANDROID) && defined(WEBRTC_VOICE_ENGINE_NR))
    TEST_LOG("Speech, Noise and Echo Metric calls\n");
    TEST_MUSTPASS(apm->GetMetricsStatus(enabled));   // check default
    TEST_MUSTPASS(enabled != false);
    TEST_MUSTPASS(apm->SetMetricsStatus(true));      // enable metrics
#ifdef WEBRTC_VOICE_ENGINE_ECHO
    // must enable AEC to get valid echo metrics
    TEST_MUSTPASS(apm->SetEcStatus(true, kEcAec));
#endif
    TEST_MUSTPASS(apm->GetMetricsStatus(enabled));
    TEST_MUSTPASS(enabled != true);

    TEST_LOG("Speak into microphone and check metrics for 10 seconds...\n");
    int speech_tx, speech_rx;
    int noise_tx, noise_rx;
#ifdef WEBRTC_VOICE_ENGINE_ECHO
    int ERLE, ERL, RERL, A_NLP;
#endif
    for (int t = 0; t < 5; t++)
    {
        SLEEP(2000);
        TEST_MUSTPASS(apm->GetSpeechMetrics(speech_tx, speech_rx));
        TEST_LOG("    Speech: Tx=%5d, Rx=%5d [dBm0]\n", speech_tx, speech_rx);
        TEST_MUSTPASS(apm->GetNoiseMetrics(noise_tx, noise_rx));
        TEST_LOG("    Noise : Tx=%5d, Rx=%5d [dBm0]\n", noise_tx, noise_rx);
#ifdef WEBRTC_VOICE_ENGINE_ECHO
        TEST_MUSTPASS(apm->GetEchoMetrics(ERL, ERLE, RERL, A_NLP));
        TEST_LOG("    Echo  : ERL=%5d, ERLE=%5d, RERL=%5d, A_NLP=%5d [dB]\n",
                 ERL, ERLE, RERL, A_NLP);
#endif
    }
    TEST_MUSTPASS(apm->SetMetricsStatus(false));     // disable metrics
#else
    TEST_LOG("Skipping apm metrics tests - MAC_IPHONE/ANDROID defined \n");
#endif // #if (!defined(MAC_IPHONE) && !d...
*/
    // VAD/DTX indication
    TEST_LOG("Get voice activity indication \n");
    if (codec)
    {
        bool v = true, dummy2;
        VadModes dummy1;
        TEST_MUSTPASS(codec->GetVADStatus(0, v, dummy1, dummy2));
        TEST_MUSTPASS(v); // Make sure VAD is disabled
    }
    TEST_MUSTPASS(1 != apm->VoiceActivityIndicator(0));
    if (codec && volume)
    {
        TEST_LOG ("RX VAD detections may vary depending on current signal"
            " and mic input \n");
#if !defined(ANDROID) && !defined(MAC_IPHONE)
        RxCallback rxc;
        TEST_MUSTPASS(apm->RegisterRxVadObserver(0, rxc));
#endif
        TEST_MUSTPASS(codec->SetVADStatus(0, true));
        TEST_MUSTPASS(volume->SetInputMute(0, true));
        if (file)
        {
            TEST_MUSTPASS(file->StopPlayingFileAsMicrophone(0));
        }
        SLEEP(500); // After sleeping we should have detected silence
        TEST_MUSTPASS(0 != apm->VoiceActivityIndicator(0));
#if !defined(ANDROID) && !defined(MAC_IPHONE)
        TEST_MUSTPASS(0 != rxc._vadDecision);
#endif
        if (file)
        {
            TEST_LOG("Start playing a file as microphone again \n");
            TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0,
                                                             micFile,
                                                             true,
                                                             true));
        }
        else
        {
            TEST_LOG("==> Make sure you talk into the microphone \n");
        }
        TEST_MUSTPASS(codec->SetVADStatus(0, false));
        TEST_MUSTPASS(volume->SetInputMute(0, false));
        SLEEP(500); // Sleep time selected by looking in mic play file, after
                    // sleep we should have detected voice
        TEST_MUSTPASS(1 != apm->VoiceActivityIndicator(0));
#if !defined(ANDROID) && !defined(MAC_IPHONE)
        TEST_MUSTPASS(1 != rxc._vadDecision);
        TEST_LOG("Disabling RX VAD detection, make sure you see no "
            "detections\n");
        TEST_MUSTPASS(apm->DeRegisterRxVadObserver(0));
        SLEEP(2000);
#endif
    }
    else
    {
        TEST_LOG("Skipping voice activity indicator tests - codec and"
            " volume APIs not available \n");
    }

#else
    TEST_LOG("\n\n+++ AudioProcessing tests NOT ENABLED +++\n");
#endif  // #ifdef _TEST_AUDIO_PROCESSING_

    ////////
    // File

#ifdef _TEST_FILE_
    TEST_LOG("\n\n+++ File tests +++\n\n");

    // test of UTF8 using swedish letters åäö

    char fileName[64];
    fileName[0] = (char)0xc3;
    fileName[1] = (char)0xa5;
    fileName[2] = (char)0xc3;
    fileName[3] = (char)0xa4;
    fileName[4] = (char)0xc3;
    fileName[5] = (char)0xb6;
    fileName[6] = '.';
    fileName[7] = 'p';
    fileName[8] = 'c';
    fileName[9] = 'm';
    fileName[10] = 0;

    // test of UTF8 using japanese Hirigana "ぁあ"letter small A and letter A
/*    fileName[0] = (char)0xe3;
    fileName[1] = (char)0x81;
    fileName[2] = (char)0x81;
    fileName[3] = (char)0xe3;
    fileName[4] = (char)0x81;
    fileName[5] = (char)0x82;
    fileName[6] = '.';
    fileName[7] = 'p';
    fileName[8] = 'c';
    fileName[9] = 'm';
    fileName[10] = 0;
*/

    // Part of the cyrillic alpabet
    // Ф    Х   Ѡ   Ц   Ч   Ш   Щ   Ъ   ЪІ  Ь   Ѣ

    const char* recName = GetFilename(fileName);
    // Generated with   
#if _WIN32
/*   char tempFileNameUTF8[200];
     int err = WideCharToMultiByte(CP_UTF8,0,L"åäö", -1, tempFileNameUTF8,
        sizeof(tempFileNameUTF8), NULL, NULL);
*/
#endif

    //Stop the current file
    TEST_LOG("Stop playing file as microphone \n");
    TEST_MUSTPASS(file->StopPlayingFileAsMicrophone(0));
    TEST_LOG("==> Talk into the microphone \n");
    SLEEP(1000);
    TEST_LOG("Record mic for 3 seconds in PCM format\n");
    TEST_MUSTPASS(file->StartRecordingMicrophone(recName));
    SLEEP(3000);
    TEST_MUSTPASS(file->StopRecordingMicrophone());
    TEST_LOG("Play out the recorded file...\n");
    TEST_MUSTPASS(file->StartPlayingFileLocally(0, recName));
    SLEEP(2000);
#ifndef _INSTRUMENTATION_TESTING_
    TEST_LOG("After 2 seconds we should still be playing\n");
    TEST_MUSTPASS(!file->IsPlayingFileLocally(0));
#endif
    TEST_LOG("Set scaling\n"); 
    TEST_MUSTPASS(file->ScaleLocalFilePlayout(0,(float)0.11));
    SLEEP(1100);
    TEST_LOG("After 3.1 seconds we should NOT be playing\n");
    TEST_MUSTPASS(file->IsPlayingFileLocally(0));

    CodecInst codec;
    TEST_LOG("Record speaker for 3 seconds to wav file\n");
    memset(&codec, 0, sizeof(CodecInst));
    strcpy(codec.plname,"pcmu");
    codec.plfreq=8000;
    codec.channels=1;
    codec.pacsize=160;
    codec.pltype=0;
    codec.rate=64000;
    TEST_MUSTPASS(file->StartRecordingPlayout(0,recName,&codec));
    SLEEP(3000);
    TEST_MUSTPASS(file->StopRecordingPlayout(0));

    TEST_LOG("Play file as mic, looping for 3 seconds\n");
    TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0,
                                                     recName,
                                                     1,
                                                     0,
                                                     kFileFormatWavFile));
    SLEEP(3000);
    TEST_LOG("After 3 seconds we should still be playing\n");
    TEST_MUSTPASS(!file->IsPlayingFileAsMicrophone(0));
    SLEEP(600);
    TEST_LOG("After 3.6 seconds we should still be playing\n");
    TEST_MUSTPASS(!file->IsPlayingFileAsMicrophone(0));

    TEST_LOG("Set scaling\n");
    TEST_MUSTPASS(file->ScaleFileAsMicrophonePlayout(0,(float)0.11));
    SLEEP(200);

    TEST_LOG("Stop playing file as microphone\n");
    TEST_MUSTPASS(file->StopPlayingFileAsMicrophone(0));

    TEST_LOG("==> Start playing a file as microphone again \n");
    TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0, micFile, true , true));
#else
    TEST_LOG("\n\n+++ File tests NOT ENABLED +++\n");
#endif  // #ifdef _TEST_FILE_

#ifdef _XTENDED_TEST_FILE_
    // Create unique trace files for this test
    TEST_MUSTPASS(base->SetTraceFileName(GetFilename("VoEFile_trace.txt")));
    TEST_MUSTPASS(base->SetDebugTraceFileName(GetFilename(
        "VoEFile_trace_debug.txt")));
    // turn off default AGC during these tests
    TEST_MUSTPASS(apm->SetAgcStatus(false));
    int res = xtend.TestFile(file);
#ifndef MAC_IPHONE
    TEST_MUSTPASS(apm->SetAgcStatus(true)); // restore AGC state
#endif
    TEST_MUSTPASS(base->Terminate());
    return res;
#endif

    ////////////
    // Network

#ifdef _TEST_NETWORK_
    TEST_LOG("\n\n+++ Network tests +++\n\n");

#ifndef WEBRTC_EXTERNAL_TRANSPORT
    int sourceRtpPort = 1234;
    int sourceRtcpPort = 1235;

    int filterPort = -1;
    int filterPortRTCP = -1;
    char sourceIp[32] = "127.0.0.1";
    char filterIp[64] = {0};

    SLEEP(200); // Make sure we have received packets
    
    TEST_MUSTPASS(netw->GetSourceInfo(0,
                                      sourceRtpPort,
                                      sourceRtcpPort,
                                      sourceIp));

    TEST_LOG("sourceIp = %s, sourceRtpPort = %d, sourceRtcpPort = %d\n",
             sourceIp, sourceRtpPort, sourceRtcpPort);
    TEST_MUSTPASS(8000 != sourceRtpPort);
    TEST_MUSTPASS(8001 != sourceRtcpPort);

    TEST_MUSTPASS(netw->GetSourceFilter(0,
                                        filterPort,
                                        filterPortRTCP,
                                        filterIp));
    TEST_MUSTPASS(0 != filterPort);
    TEST_MUSTPASS(0 != filterPortRTCP);
    TEST_MUSTPASS(_stricmp(filterIp, ""));

    TEST_LOG("Set filter port to %d => should hear audio\n", sourceRtpPort);
    TEST_MUSTPASS(netw->SetSourceFilter(0,
                                        sourceRtpPort,
                                        sourceRtcpPort,
                                        "0.0.0.0"));
    TEST_MUSTPASS(netw->GetSourceFilter(0,
                                        filterPort,
                                        filterPortRTCP,
                                        filterIp));
    TEST_MUSTPASS(sourceRtpPort != filterPort);
    TEST_MUSTPASS(sourceRtcpPort != filterPortRTCP);
    TEST_MUSTPASS(_stricmp(filterIp, "0.0.0.0"));
    SLEEP(1000);
    TEST_LOG("Set filter port to %d => should *not* hear audio\n",
             sourceRtpPort+10);
    TEST_MUSTPASS(netw->SetSourceFilter(0, sourceRtpPort+10));
    TEST_MUSTPASS(netw->GetSourceFilter(0,
                                        filterPort,
                                        filterPortRTCP,
                                        filterIp));
    TEST_MUSTPASS(sourceRtpPort+10 != filterPort);
    SLEEP(1000);
    TEST_LOG("Disable port filter => should hear audio again\n");
    TEST_MUSTPASS(netw->SetSourceFilter(0, 0));
    SLEEP(1000);

    if(rtp_rtcp)
    {
        TEST_MUSTPASS(rtp_rtcp->SetRTCP_CNAME(0, "Tomas"));
    }
  
    TEST_LOG("Set filter IP to %s => should hear audio\n", sourceIp);
    TEST_MUSTPASS(netw->SetSourceFilter(0, 0, sourceRtcpPort+10, sourceIp));
    TEST_MUSTPASS(netw->GetSourceFilter(0,
                                        filterPort,
                                        filterPortRTCP,
                                        filterIp));
    TEST_MUSTPASS(_stricmp(filterIp, sourceIp));
    SLEEP(1000);
    TEST_LOG("Set filter IP to 10.10.10.10 => should *not* hear audio\n");
    TEST_MUSTPASS(netw->SetSourceFilter(0, 0, sourceRtcpPort+10,
                                        "10.10.10.10"));
    TEST_MUSTPASS(netw->GetSourceFilter(0, filterPort, filterPort, filterIp));
    TEST_MUSTPASS(_stricmp(filterIp, "10.10.10.10"));
    SLEEP(1000);
    TEST_LOG("Disable IP filter => should hear audio again\n");
    TEST_MUSTPASS(netw->SetSourceFilter(0, 0, sourceRtcpPort+10, "0.0.0.0"));
    SLEEP(1000);
    TEST_LOG("Set filter IP to 10.10.10.10 => should *not* hear audio\n");
    TEST_MUSTPASS(netw->SetSourceFilter(0, 0, sourceRtcpPort+10,
                                        "10.10.10.10"));
    SLEEP(1000);

    if(rtp_rtcp)
    {
        char tmpStr[64];
        SLEEP(2000);
        TEST_LOG("Checking RTCP port filter with CNAME...\n");
        TEST_MUSTPASS(rtp_rtcp->GetRemoteRTCP_CNAME(0, tmpStr));
        TEST_MUSTPASS(!_stricmp("Tomas", tmpStr));
        TEST_MUSTPASS(rtp_rtcp->SetRTCP_CNAME(0, "Niklas"));
    }
    else
    {
        TEST_LOG("Skipping RTCP port filter test since there is no RTP/RTCP "
            "interface!\n");
    }

    TEST_LOG("Disable IP filter => should hear audio again\n");
    TEST_MUSTPASS(netw->SetSourceFilter(0, 0, 0, NULL));
    TEST_MUSTPASS(netw->GetSourceFilter(0, filterPort, filterPortRTCP,
                                        filterIp));
    TEST_MUSTPASS(_stricmp(filterIp, ""));
    SLEEP(1000);

    TEST_LOG("Wait 2 seconds for packet timeout...\n");
    TEST_LOG("You should see runtime error %d\n", VE_RECEIVE_PACKET_TIMEOUT);
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(netw->SetPacketTimeoutNotification(0, true, 2));
    SLEEP(3000);

 #if !defined(_INSTRUMENTATION_TESTING_)
    TEST_LOG("obs.code is %d\n", obs.code);
    TEST_MUSTPASS(obs.code != VE_RECEIVE_PACKET_TIMEOUT);
 #endif
    obs.code=-1;
    TEST_MUSTPASS(base->StartSend(0));
    if (file)
    {
        TEST_LOG("Start playing a file as microphone again \n");
        TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0,
                                                         micFile,
                                                         true,
                                                         true));
    }
    TEST_LOG("You should see runtime error %d\n", VE_PACKET_RECEIPT_RESTARTED);
    SLEEP(1000);
 #if !defined(_INSTRUMENTATION_TESTING_)
    TEST_MUSTPASS(obs.code != VE_PACKET_RECEIPT_RESTARTED);
 #endif

 #if !defined(_INSTRUMENTATION_TESTING_)
    TEST_LOG("Disabling observer, no runtime error should be seen...\n");
    TEST_MUSTPASS(base->DeRegisterVoiceEngineObserver());
    obs.code = -1;
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(netw->SetPacketTimeoutNotification(0, true, 2));
    SLEEP(2500);
    TEST_MUSTPASS(obs.code != -1);
    // disable notifications to avoid additional 8082 callbacks
    TEST_MUSTPASS(netw->SetPacketTimeoutNotification(0, false, 2));
    TEST_MUSTPASS(base->StartSend(0));
    if (file)
    {
        TEST_LOG("Start playing a file as microphone again \n");
        TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0,
                                                         micFile,
                                                         true,
                                                         true));
    }
    SLEEP(1000);
///    TEST_MUSTPASS(obs.code != -1);
    TEST_LOG("Enabling observer again\n");
    TEST_MUSTPASS(base->RegisterVoiceEngineObserver(obs));
 #endif

    TEST_LOG("Enable dead-or-alive callbacks for 4 seconds (dT=1sec)...\n");
    TEST_LOG("You should see ALIVE messages\n");

    MyDeadOrAlive obs;
    TEST_MUSTPASS(netw->RegisterDeadOrAliveObserver(0, obs));
    TEST_MUSTPASS(netw->SetPeriodicDeadOrAliveStatus(0, true, 1));
    SLEEP(4000);

    // stop sending and flush dead-or-alive states
    if (rtp_rtcp)
    {
        TEST_MUSTPASS(rtp_rtcp->SetRTCPStatus(0, false));
    }
    TEST_MUSTPASS(base->StopSend(0));
    SLEEP(500);

    TEST_LOG("Disable sending for 4 seconds (dT=1sec)...\n");
    TEST_LOG("You should see DEAD messages (one ALIVE message might"
        " sneak in if you are unlucky)\n");
    SLEEP(4000);
    TEST_LOG("Disable dead-or-alive callbacks.\n");
    TEST_MUSTPASS(netw->SetPeriodicDeadOrAliveStatus(0, false));

    TEST_LOG("Enabling external transport\n");
    TEST_MUSTPASS(base->StopReceive(0));

    // recreate the channel to ensure that we can switch from transport
    // to external transport
    TEST_MUSTPASS(base->DeleteChannel(0));
    TEST_MUSTPASS(base->CreateChannel());
 
    TEST_MUSTPASS(netw->RegisterExternalTransport(0, ch0transport));

    TEST_MUSTPASS(base->StartReceive(0));
    TEST_MUSTPASS(base->StartSend(0));
    TEST_MUSTPASS(base->StartPlayout(0));
    if (file)
    {
        TEST_LOG("Start playing a file as microphone again using"
            " external transport\n");
        TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0,
                                                         micFile,
                                                         true,
                                                         true));
    }
    SLEEP(4000);

    TEST_LOG("Disabling external transport\n");
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(base->StopPlayout(0));
    TEST_MUSTPASS(base->StopReceive(0));

    TEST_MUSTPASS(netw->DeRegisterExternalTransport(0));

    TEST_MUSTPASS(base->SetSendDestination(0, 8000, "127.0.0.1"));
    TEST_MUSTPASS(base->SetLocalReceiver(0, 8000));

    TEST_MUSTPASS(base->StartReceive(0));
    TEST_MUSTPASS(base->StartSend(0));
    TEST_MUSTPASS(base->StartPlayout(0));
    if (file)
    {
        TEST_LOG("Start playing a file as microphone again using transport\n");
        TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0, micFile, true,
                                                         true));
    }
    SLEEP(2000);
#else
    TEST_LOG("Skipping network tests - "
        "WEBRTC_EXTERNAL_TRANSPORT is defined \n");
#endif // #ifndef WEBRTC_EXTERNAL_TRANSPORT
#else
    TEST_LOG("\n\n+++ Network tests NOT ENABLED +++\n");
#endif  // #ifdef _TEST_NETWORK_

    ///////////////
    // CallReport

#ifdef _TEST_CALL_REPORT_
    TEST_LOG("\n\n+++ CallReport tests +++\n\n");
#if (defined(WEBRTC_VOICE_ENGINE_ECHO) && defined(WEBRTC_VOICE_ENGINE_NR))
    // TODO(xians), enale the tests when APM is ready
    /*
    TEST(ResetCallReportStatistics);ANL();
    TEST_MUSTPASS(!report->ResetCallReportStatistics(-2));
    TEST_MUSTPASS(!report->ResetCallReportStatistics(1));
    TEST_MUSTPASS(report->ResetCallReportStatistics(0));
    TEST_MUSTPASS(report->ResetCallReportStatistics(-1));

    bool onOff;
    LevelStatistics stats;
    TEST_MUSTPASS(apm->GetMetricsStatus(onOff));
    TEST_MUSTPASS(onOff != false);
    // All values should be -100 dBm0 when metrics are disabled
    TEST(GetSpeechAndNoiseSummary);ANL();
    TEST_MUSTPASS(report->GetSpeechAndNoiseSummary(stats));
    TEST_MUSTPASS(stats.noise_rx.min != -100);
    TEST_MUSTPASS(stats.noise_rx.max != -100);
    TEST_MUSTPASS(stats.noise_rx.average != -100);
    TEST_MUSTPASS(stats.noise_tx.min != -100);
    TEST_MUSTPASS(stats.noise_tx.max != -100);
    TEST_MUSTPASS(stats.noise_tx.average != -100);
    TEST_MUSTPASS(stats.speech_rx.min != -100);
    TEST_MUSTPASS(stats.speech_rx.max != -100);
    TEST_MUSTPASS(stats.speech_rx.average != -100);
    TEST_MUSTPASS(stats.speech_tx.min != -100);
    TEST_MUSTPASS(stats.speech_tx.max != -100);
    TEST_MUSTPASS(stats.speech_tx.average != -100);
    TEST_MUSTPASS(apm->SetMetricsStatus(true));
    SLEEP(3000);
    // All values should *not* be -100 dBm0 when metrics are enabled
    // (check Rx side only since user might be silent)
    TEST_MUSTPASS(report->GetSpeechAndNoiseSummary(stats));
    TEST_MUSTPASS(stats.noise_rx.min == -100);
    TEST_MUSTPASS(stats.noise_rx.max == -100);
    TEST_MUSTPASS(stats.noise_rx.average == -100);
    TEST_MUSTPASS(stats.speech_rx.min == -100);
    TEST_MUSTPASS(stats.speech_rx.max == -100);
    TEST_MUSTPASS(stats.speech_rx.average == -100);

    EchoStatistics echo;
    TEST(GetEchoMetricSummary);ANL();
    // all outputs will be -100 in loopback (skip further tests)
    TEST_MUSTPASS(report->GetEchoMetricSummary(echo));

    StatVal delays;
    TEST(GetRoundTripTimeSummary);ANL();
    rtp_rtcp->SetRTCPStatus(0, false);
    // All values should be -1 since RTCP is off
    TEST_MUSTPASS(report->GetRoundTripTimeSummary(0, delays));
    TEST_MUSTPASS(delays.min != -1);
    TEST_MUSTPASS(delays.max != -1);
    TEST_MUSTPASS(delays.max != -1);
    rtp_rtcp->SetRTCPStatus(0, true);
    SLEEP(5000); // gives time for RTCP
    TEST_MUSTPASS(report->GetRoundTripTimeSummary(0, delays));
    TEST_MUSTPASS(delays.min == -1);
    TEST_MUSTPASS(delays.max == -1);
    TEST_MUSTPASS(delays.max == -1);
    rtp_rtcp->SetRTCPStatus(0, false);

    int nDead;
    int nAlive;
    // -1 will be returned since dead-or-alive is not active
    TEST(GetDeadOrAliveSummary);ANL();
    TEST_MUSTPASS(report->GetDeadOrAliveSummary(0, nDead, nAlive) != -1);
    // we don't need these callbacks any longer
    TEST_MUSTPASS(netw->DeRegisterDeadOrAliveObserver(0));
    TEST_MUSTPASS(netw->SetPeriodicDeadOrAliveStatus(0, true, 1));
    SLEEP(2000);
    // All results should be >= 0 since dead-or-alive is active
    TEST_MUSTPASS(report->GetDeadOrAliveSummary(0, nDead, nAlive));
    TEST_MUSTPASS(nDead == -1);TEST_MUSTPASS(nAlive == -1)
    TEST_MUSTPASS(netw->SetPeriodicDeadOrAliveStatus(0, false));

    TEST(WriteReportToFile);ANL();
    TEST_MUSTPASS(!report->WriteReportToFile(NULL));
    TEST_MUSTPASS(report->WriteReportToFile("call_report.txt"));
    */
#else
    TEST_LOG("Skipping CallReport tests since both EC and NS are required\n");
#endif
#else
    TEST_LOG("\n\n+++ CallReport tests NOT ENABLED +++\n");
#endif // #ifdef _TEST_CALL_REPORT_

    //////////////
    // Video Sync

#ifdef _TEST_VIDEO_SYNC_
    TEST_LOG("\n\n+++ Video sync tests +++\n\n");

    unsigned int val;
    TEST_MUSTPASS(vsync->GetPlayoutTimestamp(0, val));
    TEST_LOG("Playout timestamp = %lu\n", (long unsigned int)val);

    TEST_LOG("Init timestamp and sequence number manually\n");
    TEST_MUSTPASS(!vsync->SetInitTimestamp(0, 12345));
    TEST_MUSTPASS(!vsync->SetInitSequenceNumber(0, 123));
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(vsync->SetInitTimestamp(0, 12345));
    TEST_MUSTPASS(vsync->SetInitSequenceNumber(0, 123));
    TEST_MUSTPASS(base->StartSend(0));
    if (file)
    {
        TEST_LOG("Start playing a file as microphone again \n");
        TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0,
                                                         micFile,
                                                         true,
                                                         true));
    }
    SLEEP(3000);

    TEST_LOG("Check delay estimates during 15 seconds, verify that "
        "they stabilize during this time\n");
    int valInt = -1;
    for (int i = 0; i < 15; i++)
    {
        TEST_MUSTPASS(vsync->GetDelayEstimate(0, valInt));
        TEST_LOG("Delay estimate = %d ms\n", valInt);
#if defined(MAC_IPHONE)
        TEST_MUSTPASS(valInt <= 30);    
#else
        TEST_MUSTPASS(valInt <= 45); // 45=20+25 => can't be this low
#endif
        SLEEP(1000);
    }

    TEST_LOG("Setting NetEQ min delay to 500 milliseconds and repeat "
        "the test above\n");
    TEST_MUSTPASS(vsync->SetMinimumPlayoutDelay(0, 500));
    for (int i = 0; i < 15; i++)
    {
        TEST_MUSTPASS(vsync->GetDelayEstimate(0, valInt));
        TEST_LOG("Delay estimate = %d ms\n", valInt);
        TEST_MUSTPASS(valInt <= 45);
        SLEEP(1000);
    }

    TEST_LOG("Setting NetEQ min delay to 0 milliseconds and repeat"
        " the test above\n");
    TEST_MUSTPASS(vsync->SetMinimumPlayoutDelay(0, 0));
    for (int i = 0; i < 15; i++)
    {
        TEST_MUSTPASS(vsync->GetDelayEstimate(0, valInt));
        TEST_LOG("Delay estimate = %d ms\n", valInt);
        TEST_MUSTPASS(valInt <= 45);
        SLEEP(1000);
    }

#if (defined (_WIN32) || (defined(WEBRTC_LINUX)) && !defined(ANDROID))
    valInt = -1;
    TEST_MUSTPASS(vsync->GetPlayoutBufferSize(valInt));
    TEST_LOG("Soundcard buffer size = %d ms\n", valInt);
#endif
#else
    TEST_LOG("\n\n+++ Video sync tests NOT ENABLED +++\n");
#endif  // #ifdef _TEST_VIDEO_SYNC_

    //////////////
    // Encryption

#ifdef _TEST_ENCRYPT_
    TEST_LOG("\n\n+++ Encryption tests +++\n\n");

#ifdef WEBRTC_SRTP
    TEST_LOG("SRTP tests:\n");

    unsigned char encrKey[30] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 0,
        1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0};
  
    TEST_LOG("Enable SRTP encryption and decryption, you should still hear"
        " the voice\n");
    TEST_MUSTPASS(encrypt->EnableSRTPSend(0,
                                          kCipherAes128CounterMode,
                                          30,
                                          kAuthHmacSha1,
        20, 4, kEncryptionAndAuthentication, encrKey));
    TEST_MUSTPASS(encrypt->EnableSRTPReceive(0,
                                             kCipherAes128CounterMode,
                                             30,
                                             kAuthHmacSha1,
        20, 4, kEncryptionAndAuthentication, encrKey));
    SLEEP(2000);

    TEST_LOG("Disabling decryption, you should hear nothing or garbage\n");     
    TEST_MUSTPASS(encrypt->DisableSRTPReceive(0));
    SLEEP(2000);

    TEST_LOG("Enable decryption again, you should hear the voice again\n");     
    TEST_MUSTPASS(encrypt->EnableSRTPReceive(0,
                                             kCipherAes128CounterMode,
                                             30,
                                             kAuthHmacSha1,
        20, 4, kEncryptionAndAuthentication, encrKey));
    SLEEP(2000);

    TEST_LOG("Disabling encryption and enabling decryption, you should"
        " hear nothing\n");
    TEST_MUSTPASS(encrypt->DisableSRTPSend(0));
    SLEEP(2000);

    TEST_LOG("Back to normal\n");
    // both SRTP sides are now inactive
    TEST_MUSTPASS(encrypt->DisableSRTPReceive(0));
    SLEEP(2000);

    TEST_LOG("Enable SRTP and SRTCP encryption and decryption,"
        " you should still hear the voice\n");
    TEST_MUSTPASS(encrypt->EnableSRTPSend(0,
                                          kCipherAes128CounterMode,
                                          30,
                                          kAuthHmacSha1,
        20, 4, kEncryptionAndAuthentication, encrKey, true));
    TEST_MUSTPASS(encrypt->EnableSRTPReceive(0,
                                             kCipherAes128CounterMode,
                                             30,
                                             kAuthHmacSha1,
        20, 4, kEncryptionAndAuthentication, encrKey, true));
    SLEEP(2000);

    TEST_LOG("Back to normal\n");
    TEST_MUSTPASS(encrypt->DisableSRTPSend(0));
    // both SRTP sides are now inactive
    TEST_MUSTPASS(encrypt->DisableSRTPReceive(0));
    SLEEP(2000);

#else
    TEST_LOG("Skipping SRTP tests - WEBRTC_SRTP not defined \n");
#endif // #ifdef WEBRTC_SRTP

    TEST_LOG("\nExternal encryption tests:\n");
    my_encryption * encObj = new my_encryption;
    TEST_MUSTPASS(encrypt->RegisterExternalEncryption(0, *encObj));
    TEST_LOG("Encryption enabled but you should still hear the voice\n");
    SLEEP(2000);
    TEST_LOG("Removing encryption object and deleting it\n");
    TEST_MUSTPASS(encrypt->DeRegisterExternalEncryption(0));
    delete encObj;
    SLEEP(2000);
#else
    TEST_LOG("\n\n+++ Encryption tests NOT ENABLED +++\n");
#endif // #ifdef _TEST_ENCRYPT_

    //////////////////
    // External media

#ifdef _TEST_XMEDIA_
    TEST_LOG("\n\n+++ External media tests +++\n\n");

#ifdef WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
    TEST_LOG("Stop playing file as microphone \n");
    TEST_LOG("==> Talk into the microphone \n");
    TEST_MUSTPASS(file->StopPlayingFileAsMicrophone(0));

    TEST_LOG("Enabling external playout\n");
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(base->StopPlayout(0));
    TEST_MUSTPASS(xmedia->SetExternalPlayoutStatus(true));
    TEST_MUSTPASS(base->StartPlayout(0));
    TEST_MUSTPASS(base->StartSend(0));

    TEST_LOG("Writing 2 secs of play data to vector\n");
    int getLen;
    WebRtc_Word16 speechData[32000];
    for (int i = 0; i < 200; i++)
    {
        TEST_MUSTPASS(xmedia->ExternalPlayoutGetData(speechData+i*160, 
                                                     16000, 
                                                     100, 
                                                     getLen));
        TEST_MUSTPASS(160 != getLen);
        SLEEP(10);
    }

    TEST_LOG("Disabling external playout\n");
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(base->StopPlayout(0));
    TEST_MUSTPASS(xmedia->SetExternalPlayoutStatus(false));
    TEST_MUSTPASS(base->StartPlayout(0));

    TEST_LOG("Enabling external recording\n");
    TEST_MUSTPASS(xmedia->SetExternalRecordingStatus(true));
    TEST_MUSTPASS(base->StartSend(0));

    TEST_LOG("Inserting record data from vector\n");
    for (int i = 0; i < 200; i++)
    {
        TEST_MUSTPASS(xmedia->ExternalRecordingInsertData(speechData+i*160, 
                                                          160, 
                                                          16000, 
                                                          20));
        SLEEP(10);
    }

    TEST_LOG("Disabling external recording\n");
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(xmedia->SetExternalRecordingStatus(false));
    TEST_MUSTPASS(base->StartSend(0));

    TEST_LOG("==> Start playing a file as microphone again \n");
    TEST_MUSTPASS(file->StartPlayingFileAsMicrophone(0, micFile, true , true));
#else
    TEST_LOG("Skipping external rec and playout tests - \
             WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT not defined \n");
#endif // WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT

    TEST_LOG("Enabling playout external media processing => "
             "played audio should now be affected \n");
    TEST_MUSTPASS(xmedia->RegisterExternalMediaProcessing(
        -1, kPlaybackAllChannelsMixed, mobj));
    SLEEP(2000);
    TEST_LOG("Back to normal again \n");
    TEST_MUSTPASS(xmedia->DeRegisterExternalMediaProcessing(
        -1, kPlaybackAllChannelsMixed));
    SLEEP(2000);
    // Note that we must do per channel here because PlayFileAsMicrophone
    // is only done on ch 0.
    TEST_LOG("Enabling recording external media processing => "
             "played audio should now be affected \n");
    TEST_MUSTPASS(xmedia->RegisterExternalMediaProcessing(
        0, kRecordingPerChannel, mobj));
    SLEEP(2000);
    TEST_LOG("Back to normal again \n");
    TEST_MUSTPASS(xmedia->DeRegisterExternalMediaProcessing(
        0, kRecordingPerChannel));
    SLEEP(2000);
    TEST_LOG("Enabling recording external media processing => "
             "speak and make sure that voice is affected \n");
    TEST_MUSTPASS(xmedia->RegisterExternalMediaProcessing(
        -1, kRecordingAllChannelsMixed, mobj));
    SLEEP(2000);
    TEST_LOG("Back to normal again \n");
    TEST_MUSTPASS(xmedia->DeRegisterExternalMediaProcessing(
        -1, kRecordingAllChannelsMixed));
    SLEEP(2000);
#else
    TEST_LOG("\n\n+++ External media tests NOT ENABLED +++\n");
#endif // #ifdef _TEST_XMEDIA_

    /////////////////////
    // NetEQ statistics

#ifdef _TEST_NETEQ_STATS_
    TEST_LOG("\n\n+++ NetEQ statistics tests +++\n\n");

#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
    NetworkStatistics nStats;
    TEST_MUSTPASS(neteqst->GetNetworkStatistics(0, nStats));
    TEST_LOG("\nNetwork statistics: \n");
    TEST_LOG("    currentAccelerateRate     = %hu \n",
             nStats.currentAccelerateRate);
    TEST_LOG("    currentBufferSize         = %hu \n",
             nStats.currentBufferSize);
    TEST_LOG("    currentDiscardRate        = %hu \n",
             nStats.currentDiscardRate);
    TEST_LOG("    currentExpandRate         = %hu \n",
             nStats.currentExpandRate);
    TEST_LOG("    currentPacketLossRate     = %hu \n",
             nStats.currentPacketLossRate);
    TEST_LOG("    currentPreemptiveRate     = %hu \n",
             nStats.currentPreemptiveRate);
    TEST_LOG("    preferredBufferSize       = %hu \n",
             nStats.preferredBufferSize);

    JitterStatistics jStats;
    TEST_MUSTPASS(neteqst->GetJitterStatistics(0, jStats));
    TEST_LOG("\nJitter statistics: \n");
    TEST_LOG("    jbMinSize                 = %u \n",
             jStats.jbMinSize);
    TEST_LOG("    jbMaxSize                 = %u \n",
             jStats.jbMaxSize);
    TEST_LOG("    jbAvgSize                 = %u \n",
             jStats.jbAvgSize);
    TEST_LOG("    jbChangeCount             = %u \n",
             jStats.jbChangeCount);
    TEST_LOG("    lateLossMs                = %u \n",
             jStats.lateLossMs);
    TEST_LOG("    accelerateMs              = %u \n",
             jStats.accelerateMs);
    TEST_LOG("    flushedMs                 = %u \n",
             jStats.flushedMs);
    TEST_LOG("    generatedSilentMs         = %u \n",
             jStats.generatedSilentMs);
    TEST_LOG("    interpolatedVoiceMs       = %u \n",
             jStats.interpolatedVoiceMs);
    TEST_LOG("    interpolatedSilentMs      = %u \n",
             jStats.interpolatedSilentMs);
    TEST_LOG("    countExpandMoreThan120ms  = %u \n",
             jStats.countExpandMoreThan120ms);
    TEST_LOG("    countExpandMoreThan250ms  = %u \n",
             jStats.countExpandMoreThan250ms);
    TEST_LOG("    countExpandMoreThan500ms  = %u \n",
             jStats.countExpandMoreThan500ms);
    TEST_LOG("    countExpandMoreThan2000ms = %u \n",
             jStats.countExpandMoreThan2000ms);
    TEST_LOG("    longestExpandDurationMs   = %u \n",
             jStats.longestExpandDurationMs);
    TEST_LOG("    countIAT500ms             = %u \n",
             jStats.countIAT500ms);
    TEST_LOG("    countIAT1000ms            = %u \n",
             jStats.countIAT1000ms);
    TEST_LOG("    countIAT2000ms            = %u \n",
             jStats.countIAT2000ms);
    TEST_LOG("    longestIATms              = %u \n",
             jStats.longestIATms);
    TEST_LOG("    minPacketDelayMs          = %u \n",
             jStats.minPacketDelayMs);
    TEST_LOG("    maxPacketDelayMs          = %u \n",
             jStats.maxPacketDelayMs);
    TEST_LOG("    avgPacketDelayMs          = %u \n",
             jStats.avgPacketDelayMs);

    unsigned short preferredBufferSize;
    TEST_MUSTPASS(neteqst->GetPreferredBufferSize(0, preferredBufferSize));
    TEST_MUSTPASS(preferredBufferSize != nStats.preferredBufferSize);

    TEST_MUSTPASS(neteqst->ResetJitterStatistics(0));
#else
    TEST_LOG("Skipping NetEQ statistics tests - "
        "WEBRTC_VOICE_ENGINE_NETEQ_STATS_API not defined \n");
#endif // #ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
#else
    TEST_LOG("\n\n+++ NetEQ statistics tests NOT ENABLED +++\n");
#endif // #ifdef _TEST_NETEQ_STATS_

 
    //////////////////
    // Stop streaming

    TEST_LOG("\n\n+++ Stop streaming +++\n\n");

    TEST_LOG("Stop playout, sending and listening \n");
    TEST_MUSTPASS(base->StopPlayout(0));
    TEST_MUSTPASS(base->StopSend(0));
    TEST_MUSTPASS(base->StopReceive(0));

// Exit:

    TEST_LOG("Delete channel and terminate VE \n");
    TEST_MUSTPASS(base->DeleteChannel(0));
    TEST_MUSTPASS(base->Terminate());

    return 0;
}

int runAutoTest(TestType testType, ExtendedSelection extendedSel)
{
    SubAPIManager apiMgr;
    apiMgr.DisplayStatus();

#ifdef MAC_IPHONE
    // Write mic file path to buffer
    TEST_LOG("Get mic file path \n");
    if (0 != GetResource("audio_long16.pcm", micFile, 256))
    {
        TEST_LOG("Failed get mic file path! \n");
    }
#endif

    ////////////////////////////////////
    // Create VoiceEngine and sub API:s

    voetest::VoETestManager tm;
    tm.GetInterfaces();

    //////////////////////
    // Run standard tests

    int mainRet(-1);
    if (testType == Standard)
    {
        mainRet = tm.DoStandardTest();

        ////////////////////////////////
        // Create configuration summary

        TEST_LOG("\n\n+++ Creating configuration summary file +++\n");
        createSummary(tm.VoiceEnginePtr());
    }
    else if (testType == Extended)
    {
        VoEExtendedTest xtend(tm);

        mainRet = 0;
        while (extendedSel != XSEL_None)
        {
            if (extendedSel == XSEL_Base || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestBase()) == -1)
                    break;
                xtend.TestPassed("Base");
            }
            if (extendedSel == XSEL_CallReport || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestCallReport()) == -1)
                    break;
                xtend.TestPassed("CallReport");
            }
            if (extendedSel == XSEL_Codec || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestCodec()) == -1)
                    break;
                xtend.TestPassed("Codec");
            }
            if (extendedSel == XSEL_DTMF || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestDtmf()) == -1)
                    break;
                xtend.TestPassed("Dtmf");
            }
            if (extendedSel == XSEL_Encryption || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestEncryption()) == -1)
                    break;
                xtend.TestPassed("Encryption");
            }
            if (extendedSel == XSEL_ExternalMedia || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestExternalMedia()) == -1)
                    break;
                xtend.TestPassed("ExternalMedia");
            }
            if (extendedSel == XSEL_File || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestFile()) == -1)
                    break;
                xtend.TestPassed("File");
            }
            if (extendedSel == XSEL_Hardware || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestHardware()) == -1)
                    break;
                xtend.TestPassed("Hardware");
            }
            if (extendedSel == XSEL_NetEqStats || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestNetEqStats()) == -1)
                    break;
                xtend.TestPassed("NetEqStats");
            }
            if (extendedSel == XSEL_Network || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestNetwork()) == -1)
                    break;
                xtend.TestPassed("Network");
            }
            if (extendedSel == XSEL_RTP_RTCP || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestRTP_RTCP()) == -1)
                    break;
                xtend.TestPassed("RTP_RTCP");
            }
            if (extendedSel == XSEL_VideoSync || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestVideoSync()) == -1)
                    break;
                xtend.TestPassed("VideoSync");
            }
            if (extendedSel == XSEL_VolumeControl || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestVolumeControl()) == -1)
                    break;
                xtend.TestPassed("VolumeControl");
            }
            if (extendedSel == XSEL_AudioProcessing || extendedSel == XSEL_All)
            {
                if ((mainRet = xtend.TestAPM()) == -1)
                    break;
                xtend.TestPassed("AudioProcessing");
            }
            apiMgr.GetExtendedMenuSelection(extendedSel);
        }  // while (extendedSel != XSEL_None)
    }
    else if (testType == Stress)
    {
        VoEStressTest stressTest(tm);
        mainRet = stressTest.DoTest();
    }
    else if (testType == Unit)
    {
        VoEUnitTest unitTest(tm);
        mainRet = unitTest.DoTest();
    }
    else if (testType == CPU)
    {
        VoECpuTest cpuTest(tm);
        mainRet = cpuTest.DoTest();
    }
    else
    {
        // Should never end up here
        TEST_LOG("INVALID SELECTION \n");
    }


    //////////////////
    // Release/Delete

    int releaseOK = tm.ReleaseInterfaces();

    if ((0 == mainRet) && (releaseOK != -1))
    {
        TEST_LOG("\n\n*** All tests passed *** \n\n");
    }
    else
    {
        TEST_LOG("\n\n*** Test failed! *** \n");
    }

    return 0;
}

void createSummary(VoiceEngine* ve)
{
    int len;
    char str[256];

#ifdef MAC_IPHONE
    char summaryFilename[256];
    GetDocumentsDir(summaryFilename, 256);
    strcat(summaryFilename, "/summary.txt");
#endif

    VoEBase* base = VoEBase::GetInterface(ve);
    FILE* stream = fopen(summaryFilename, "wt");

    sprintf(str, "WebRTc VoiceEngine ");
#if defined(_WIN32)
    strcat(str, "Win");
#elif defined(WEBRTC_LINUX) && defined(WEBRTC_TARGET_PC) && !defined(ANDROID)
    strcat(str, "Linux");
#elif defined(WEBRTC_MAC) && !defined(MAC_IPHONE)
    strcat(str, "Mac");
#elif defined(ANDROID)
    strcat(str, "Android");
#elif defined(MAC_IPHONE)
    strcat(str, "iPhone");
#endif
    // Add for other platforms as needed

    fprintf(stream, "%s\n", str);
    len = (int)strlen(str);
    for (int i=0; i<len; i++)
    {
        fprintf(stream, "=");
    }
    fprintf(stream, "\n\n");

    char version[1024];
    char veVersion[24];
    base->GetVersion(version);
    // find first NL <=> end of VoiceEngine version string
    int pos = (int)strcspn(version, "\n");
    strncpy(veVersion, version, pos);
    veVersion[pos] = '\0';
    sprintf(str, "Version:                    %s\n", veVersion);
    fprintf(stream, "%s\n", str);

    sprintf(str, "Build date & time:          %s\n", BUILDDATE " " BUILDTIME);
    fprintf(stream, "%s\n", str);

    strcpy(str, "G.711 A-law");
    fprintf(stream, "\nSupported codecs:           %s\n", str); 
    strcpy(str, "                            G.711 mu-law");
    fprintf(stream, "%s\n", str);
#ifdef WEBRTC_CODEC_EG711
    strcpy(str, "                            Enhanced G.711 A-law");
    fprintf(stream, "%s\n", str);
    strcpy(str, "                            Enhanced G.711 mu-law");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_IPCMWB
    strcpy(str, "                            iPCM-wb");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_ILBC
    strcpy(str, "                            iLBC");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_ISAC
    strcpy(str, "                            iSAC");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_ISACLC
    strcpy(str, "                            iSAC-LC");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G722
    strcpy(str, "                            G.722");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G722_1
    strcpy(str, "                            G.722.1");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G722_1C
    strcpy(str, "                            G.722.1C");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G723
    strcpy(str, "                            G.723");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G726
    strcpy(str, "                            G.726");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G729
    strcpy(str, "                            G.729");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_G729_1
    strcpy(str, "                            G.729.1");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_GSMFR
    strcpy(str, "                            GSM-FR");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_GSMAMR
    strcpy(str, "                            AMR");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_GSMAMRWB
    strcpy(str, "                            AMR-WB");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_GSMEFR
    strcpy(str, "                            GSM-EFR");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_SPEEX
    strcpy(str, "                            Speex");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_SILK
    strcpy(str, "                            Silk");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_CODEC_PCM16
    strcpy(str, "                            L16");
    fprintf(stream, "%s\n", str);
#endif
#ifdef NETEQFIX_VOXWARE_SC3
    strcpy(str, "                            Voxware SC3");
    fprintf(stream, "%s\n", str);
#endif
    // Always included
    strcpy(str, "                            AVT (RFC2833)");
    fprintf(stream, "%s\n", str);
#ifdef WEBRTC_CODEC_RED
    strcpy(str, "                            RED (forward error correction)");
    fprintf(stream, "%s\n", str);
#endif

    fprintf(stream, "\nEcho Control:               ");
#ifdef WEBRTC_VOICE_ENGINE_ECHO
    fprintf(stream, "Yes\n");
#else
    fprintf(stream, "No\n");
#endif

    fprintf(stream, "\nAutomatic Gain Control:     ");
#ifdef WEBRTC_VOICE_ENGINE_AGC
    fprintf(stream, "Yes\n");
#else
    fprintf(stream, "No\n");
#endif

    fprintf(stream, "\nNoise Reduction:            ");
#ifdef WEBRTC_VOICE_ENGINE_NR
    fprintf(stream, "Yes\n");
#else
    fprintf(stream, "No\n");
#endif

    fprintf(stream, "\nSRTP:                       ");
#ifdef WEBRTC_SRTP
    fprintf(stream, "Yes\n");
#else
    fprintf(stream, "No\n");
#endif

    fprintf(stream, "\nExternal transport only:    ");
#ifdef WEBRTC_EXTERNAL_TRANSPORT
    fprintf(stream, "Yes\n");
#else
    fprintf(stream, "No\n");
#endif

    fprintf(stream, "\nTelephone event detection:  ");
#ifdef WEBRTC_DTMF_DETECTION
    fprintf(stream, "Yes\n");
#else
    fprintf(stream, "No\n");
#endif

    strcpy(str, "VoEBase");
    fprintf(stream, "\nSupported sub-APIs:         %s\n", str); 
#ifdef WEBRTC_VOICE_ENGINE_CODEC_API
    strcpy(str, "                            VoECodec");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_DTMF_API
    strcpy(str, "                            VoEDtmf");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_FILE_API
    strcpy(str, "                            VoEFile");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_HARDWARE_API
    strcpy(str, "                            VoEHardware");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETWORK_API
    strcpy(str, "                            VoENetwork");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_RTP_RTCP_API
    strcpy(str, "                            VoERTP_RTCP");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API
    strcpy(str, "                            VoEVolumeControl");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API
    strcpy(str, "                            VoEAudioProcessing");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
    strcpy(str, "                            VoeExternalMedia");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
    strcpy(str, "                            VoENetEqStats");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API
    strcpy(str, "                            VoEEncryption");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_CALL_REPORT_API
    strcpy(str, "                            VoECallReport");
    fprintf(stream, "%s\n", str);
#endif
#ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
    strcpy(str, "                            VoEVideoSync");
    fprintf(stream, "%s\n", str);
#endif

    fclose(stream);
    base->Release();
}

/*********Knowledge Base******************/

//An example for creating threads and calling VE API's from that thread.
// Using thread.  A generic API/Class for all platforms.
#ifdef THEADTEST // find first NL <=> end of VoiceEngine version string
//Definition of Thread Class
class ThreadTest 
{
public:
    ThreadTest(
        VoEBase* base);
    ~ThreadTest() 
    {
        delete _myThread;
    }
    void Stop();
private:
    static bool StartSend(
        void* obj);
    bool StartSend();

    ThreadWrapper* _myThread;
    VoEBase* _base;

    bool _stopped;
};

//Main function from where StartSend is invoked as a seperate thread.
ThreadTest::ThreadTest(
    VoEBase* base)
    :
    _stopped(false),
    _base(base)
{
    //Thread Creation
    _myThread = ThreadWrapper::CreateThread(StartSend, this, kLowPriority);
    unsigned int id  = 0;
    //Starting the thread
    _myThread->Start(id);
}

//Calls the StartSend.  This is to avoid the static declaration issue.
bool
ThreadTest::StartSend(
    void* obj)
{
    return ((ThreadTest*)obj)->StartSend(); 
}


bool
ThreadTest::StartSend()
{
    _myThread->SetNotAlive();  //Ensures this function is called only once.
    _base->StartSend(0);
    return true;
}

void ThreadTest::Stop()
{
    _stopped = true;
}

//  Use the following to invoke ThreatTest from the main function.
//  ThreadTest* threadtest = new ThreadTest(base);
#endif

// An example to create a thread and call VE API's call from that thread.
// Specific to Windows Platform
#ifdef THREAD_TEST_WINDOWS
//Thread Declaration.  Need to be added in the class controlling/dictating
// the main code.
/**
private:
    static unsigned int WINAPI StartSend(void* obj);
    unsigned int WINAPI StartSend();
**/

//Thread Definition
unsigned int WINAPI mainTest::StartSend(void *obj)
{
    return ((mainTest*)obj)->StartSend();   
}
unsigned int WINAPI mainTest::StartSend()
{
    //base
    base->StartSend(0);

    //  TEST_MUSTPASS(base->StartSend(0));
    TEST_LOG("hi hi hi");
    return 0;
}

//Thread invoking.  From the main code
/*****
    unsigned int threadID=0;
    if ((HANDLE)_beginthreadex(NULL,
                               0,
                               StartSend,
                               (void*)this,
                               0,
                                &threadID) == NULL)
        return false;
****/

#endif

}  // namespace voetest



// ----------------------------------------------------------------------------
//                                       main
// ----------------------------------------------------------------------------

using namespace voetest;

#if !defined(MAC_IPHONE)
int main(int , char** )
{
    SubAPIManager apiMgr;
    apiMgr.DisplayStatus();

    printf("----------------------------\n");
    printf("Select type of test\n\n");
    printf(" (0)  Quit\n");
    printf(" (1)  Standard test\n");
    printf(" (2)  Extended test(s)...\n");
    printf(" (3)  Stress test(s)...\n");
    printf(" (4)  Unit test(s)...\n");
    printf(" (5)  CPU & memory reference test [Windows]...\n");
    printf("\n: ");

    int selection(0);

    dummy = scanf("%d", &selection);

    ExtendedSelection extendedSel(XSEL_Invalid);

    enum TestType testType(Invalid);

    switch (selection)
    {
    case 0:
        return 0;
    case 1:
        testType = Standard;
        break;
    case 2:
        testType = Extended;
        while (!apiMgr.GetExtendedMenuSelection(extendedSel))
            ;
        break;
    case 3:
        testType = Stress;
        break;
    case 4:
        testType = Unit;
        break;
    case 5:
        testType = CPU;
        break;
    default:
        TEST_LOG("Invalid selection!\n");
        return 0;
    }

    // function that can be called
    // from other entry functions
    int retVal = runAutoTest(testType, extendedSel);

    return retVal;
}
#endif //#if !defined(MAC_IPHONE)
