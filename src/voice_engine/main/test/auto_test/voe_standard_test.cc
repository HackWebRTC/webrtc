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

#if defined (_ENABLE_VISUAL_LEAK_DETECTOR_) && defined(_DEBUG) && \
    defined(_WIN32) && !defined(_INSTRUMENTATION_TESTING_)
#include "vld.h"
#endif

#ifdef MAC_IPHONE
#include "../../source/voice_engine_defines.h"  // defines build macros
#else
#include "../../source/voice_engine_defines.h"  // defines build macros
#endif

#include "automated_mode.h"
#include "critical_section_wrapper.h"
#include "event_wrapper.h"
#include "thread_wrapper.h"

#ifdef _TEST_NETEQ_STATS_
#include "../../interface/voe_neteq_stats.h" // Not available in delivery folder
#endif

#include "voe_extended_test.h"
#include "voe_stress_test.h"
#include "voe_unit_test.h"
#include "voe_cpu_test.h"

using namespace webrtc;

namespace voetest {

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
char filenameStr[2][256];
int currentStr = 0;

char* GetFilename(char* filename) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/sdcard/%s", filename);
  return filenameStr[currentStr];
}

const char* GetFilename(const char* filename) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/sdcard/%s", filename);
  return filenameStr[currentStr];
}

int GetResource(char* resource, char* dest, int destLen) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/sdcard/%s", resource);
  strncpy(dest, filenameStr[currentStr], destLen-1);
  return 0;
}

char* GetResource(char* resource) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/sdcard/%s", resource);
  return filenameStr[currentStr];
}

const char* GetResource(const char* resource) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/sdcard/%s", resource);
  return filenameStr[currentStr];
}

#else
char filenameStr[2][256];
int currentStr = 0;

char* GetFilename(char* filename) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/tmp/%s", filename);
  return filenameStr[currentStr];
}
const char* GetFilename(const char* filename) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/tmp/%s", filename);
  return filenameStr[currentStr];
}
int GetResource(char* resource, char* dest, int destLen) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/tmp/%s", resource);
  strncpy(dest, filenameStr[currentStr], destLen - 1);
  return 0;
}
char* GetResource(char* resource) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/tmp/%s", resource);
  return filenameStr[currentStr];
}
const char* GetResource(const char* resource) {
  currentStr = !currentStr;
  sprintf(filenameStr[currentStr], "/tmp/%s", resource);
  return filenameStr[currentStr];
}
#endif

#if !defined(MAC_IPHONE)
const char* summaryFilename = "/tmp/VoiceEngineSummary.txt";
#endif
// For iPhone the summary filename is created in createSummary

int dummy = 0;  // Dummy used in different functions to avoid warnings

TestRtpObserver::TestRtpObserver() {
  Reset();
}

TestRtpObserver::~TestRtpObserver() {
}

void TestRtpObserver::Reset() {
  for (int i = 0; i < 2; i++) {
    ssrc_[i] = 0;
    csrc_[i][0] = 0;
    csrc_[i][1] = 0;
    added_[i][0] = false;
    added_[i][1] = false;
    size_[i] = 0;
  }
}

void TestRtpObserver::OnIncomingCSRCChanged(const int channel,
                                            const unsigned int CSRC,
                                            const bool added) {
  char msg[128];
  sprintf(msg, "=> OnIncomingCSRCChanged(channel=%d, CSRC=%u, added=%d)\n",
          channel, CSRC, added);
  TEST_LOG("%s", msg);

  if (channel > 1)
    return;  // Not enough memory.

  csrc_[channel][size_[channel]] = CSRC;
  added_[channel][size_[channel]] = added;

  size_[channel]++;
  if (size_[channel] == 2)
    size_[channel] = 0;
}

void TestRtpObserver::OnIncomingSSRCChanged(const int channel,
                                            const unsigned int SSRC) {
  char msg[128];
  sprintf(msg, "\n=> OnIncomingSSRCChanged(channel=%d, SSRC=%u)\n", channel,
          SSRC);
  TEST_LOG("%s", msg);

  ssrc_[channel] = SSRC;
}

void MyDeadOrAlive::OnPeriodicDeadOrAlive(const int /*channel*/,
                                          const bool alive) {
  if (alive) {
    TEST_LOG("ALIVE\n");
  } else {
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
                      const bool stereo) {
  for (int i = 0; i < length; i++) {
    if (!stereo) {
      audio_10ms[i] = (WebRtc_Word16) (audio_10ms[i] *
          sin(2.0 * 3.14 * f * 400.0 / samplingFreqHz));
    } else {
      // interleaved stereo
      audio_10ms[2 * i] = (WebRtc_Word16) (audio_10ms[2 * i] *
          sin(2.0 * 3.14 * f * 400.0 / samplingFreqHz));
      audio_10ms[2 * i + 1] = (WebRtc_Word16) (audio_10ms[2 * i + 1] *
          sin(2.0 * 3.14 * f * 400.0 / samplingFreqHz));
    }
    f++;
  }
}
#endif

MyMedia mobj;

FakeExternalTransport::FakeExternalTransport(VoENetwork* ptr)
    : my_network_(ptr),
      thread_(NULL),
      lock_(NULL),
      event_(NULL),
      length_(0),
      channel_(0),
      delay_is_enabled_(0),
      delay_time_in_ms_(0) {
  const char* threadName = "external_thread";
  lock_ = CriticalSectionWrapper::CreateCriticalSection();
  event_ = EventWrapper::Create();
  thread_ = ThreadWrapper::CreateThread(Run, this, kHighPriority, threadName);
  if (thread_) {
    unsigned int id;
    thread_->Start(id);
  }
}

FakeExternalTransport::~FakeExternalTransport() {
  if (thread_) {
    thread_->SetNotAlive();
    event_->Set();
    if (thread_->Stop()) {
      delete thread_;
      thread_ = NULL;
      delete event_;
      event_ = NULL;
      delete lock_;
      lock_ = NULL;
    }
  }
}

bool FakeExternalTransport::Run(void* ptr) {
  return static_cast<FakeExternalTransport*> (ptr)->Process();
}

bool FakeExternalTransport::Process() {
  switch (event_->Wait(500)) {
    case kEventSignaled:
      lock_->Enter();
      my_network_->ReceivedRTPPacket(channel_, packet_buffer_, length_);
      lock_->Leave();
      return true;
    case kEventTimeout:
      return true;
    case kEventError:
      break;
  }
  return true;
}

int FakeExternalTransport::SendPacket(int channel, const void *data, int len) {
  lock_->Enter();
  if (len < 1612) {
    memcpy(packet_buffer_, (const unsigned char*) data, len);
    length_ = len;
    channel_ = channel;
  }
  lock_->Leave();
  event_->Set(); // triggers ReceivedRTPPacket() from worker thread
  return len;
}

int FakeExternalTransport::SendRTCPPacket(int channel, const void *data, int len) {
  if (delay_is_enabled_) {
    Sleep(delay_time_in_ms_);
  }
  my_network_->ReceivedRTCPPacket(channel, data, len);
  return len;
}

void FakeExternalTransport::SetDelayStatus(bool enable, unsigned int delayInMs) {
  delay_is_enabled_ = enable;
  delay_time_in_ms_ = delayInMs;
}

ErrorObserver::ErrorObserver() {
  code = -1;
}
void ErrorObserver::CallbackOnError(const int channel, const int errCode) {
  code = errCode;
#ifndef _INSTRUMENTATION_TESTING_
  TEST_LOG("\n************************\n");
  TEST_LOG(" RUNTIME ERROR: %d \n", errCode);
  TEST_LOG("************************\n");
#endif
}

void MyTraceCallback::Print(const TraceLevel level,
                            const char *traceString,
                            const int length) {
  if (traceString) {
    char* tmp = new char[length];
    memcpy(tmp, traceString, length);
    TEST_LOG("%s", tmp);
    TEST_LOG("\n");
    delete[] tmp;
  }
}

void RtcpAppHandler::OnApplicationDataReceived(
    const int /*channel*/, const unsigned char sub_type,
    const unsigned int name, const unsigned char* data,
    const unsigned short length_in_bytes) {
  length_in_bytes_ = length_in_bytes;
  memcpy(data_, &data[0], length_in_bytes);
  sub_type_ = sub_type;
  name_ = name;
}

void RtcpAppHandler::Reset() {
  length_in_bytes_ = 0;
  memset(data_, 0, sizeof(data_));
  sub_type_ = 0;
  name_ = 0;
}

void my_encryption::encrypt(int, unsigned char * in_data,
                            unsigned char * out_data,
                            int bytes_in,
                            int * bytes_out) {
  int i;
  for (i = 0; i < bytes_in; i++)
    out_data[i] = ~in_data[i];
  *bytes_out = bytes_in + 2; // length is increased by 2
}

void my_encryption::decrypt(int, unsigned char * in_data,
                            unsigned char * out_data,
                            int bytes_in,
                            int * bytes_out) {
  int i;
  for (i = 0; i < bytes_in; i++)
    out_data[i] = ~in_data[i];
  *bytes_out = bytes_in - 2; // length is decreased by 2
}

void my_encryption::encrypt_rtcp(int,
                                 unsigned char * in_data,
                                 unsigned char * out_data,
                                 int bytes_in,
                                 int * bytes_out) {
  int i;
  for (i = 0; i < bytes_in; i++)
    out_data[i] = ~in_data[i];
  *bytes_out = bytes_in + 2;
}

void my_encryption::decrypt_rtcp(int, unsigned char * in_data,
                                 unsigned char * out_data,
                                 int bytes_in,
                                 int * bytes_out) {
  int i;
  for (i = 0; i < bytes_in; i++)
    out_data[i] = ~in_data[i];
  *bytes_out = bytes_in + 2;
}

void SubAPIManager::DisplayStatus() const {
  TEST_LOG("Supported sub APIs:\n\n");
  if (_base)
    TEST_LOG("  Base\n");
  if (_callReport)
    TEST_LOG("  CallReport\n");
  if (_codec)
    TEST_LOG("  Codec\n");
  if (_dtmf)
    TEST_LOG("  Dtmf\n");
  if (_encryption)
    TEST_LOG("  Encryption\n");
  if (_externalMedia)
    TEST_LOG("  ExternalMedia\n");
  if (_file)
    TEST_LOG("  File\n");
  if (_hardware)
    TEST_LOG("  Hardware\n");
  if (_netEqStats)
    TEST_LOG("  NetEqStats\n");
  if (_network)
    TEST_LOG("  Network\n");
  if (_rtp_rtcp)
    TEST_LOG("  RTP_RTCP\n");
  if (_videoSync)
    TEST_LOG("  VideoSync\n");
  if (_volumeControl)
    TEST_LOG("  VolumeControl\n");
  if (_apm)
    TEST_LOG("  AudioProcessing\n");
  ANL();
  TEST_LOG("Excluded sub APIs:\n\n");
  if (!_base)
    TEST_LOG("  Base\n");
  if (!_callReport)
    TEST_LOG("  CallReport\n");
  if (!_codec)
    TEST_LOG("  Codec\n");
  if (!_dtmf)
    TEST_LOG("  Dtmf\n");
  if (!_encryption)
    TEST_LOG("  Encryption\n");
  if (!_externalMedia)
    TEST_LOG("  ExternamMedia\n");
  if (!_file)
    TEST_LOG("  File\n");
  if (!_hardware)
    TEST_LOG("  Hardware\n");
  if (!_netEqStats)
    TEST_LOG("  NetEqStats\n");
  if (!_network)
    TEST_LOG("  Network\n");
  if (!_rtp_rtcp)
    TEST_LOG("  RTP_RTCP\n");
  if (!_videoSync)
    TEST_LOG("  VideoSync\n");
  if (!_volumeControl)
    TEST_LOG("  VolumeControl\n");
  if (!_apm)
    TEST_LOG("  AudioProcessing\n");
  ANL();
}

bool SubAPIManager::GetExtendedMenuSelection(ExtendedSelection& sel) {
  printf("------------------------------------------------\n");
  printf("Select extended test\n\n");
  printf(" (0)  None\n");
  printf("- - - - - - - - - - - - - - - - - - - - - - - - \n");
  printf(" (1)  Base");
  if (_base)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (2)  CallReport");
  if (_callReport)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (3)  Codec");
  if (_codec)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (4)  Dtmf");
  if (_dtmf)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (5)  Encryption");
  if (_encryption)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (6)  VoEExternalMedia");
  if (_externalMedia)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (7)  File");
  if (_file)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (8)  Mixing");
  if (_file)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (9)  Hardware");
  if (_hardware)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (10) NetEqStats");
  if (_netEqStats)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (11) Network");
  if (_network)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (12) RTP_RTCP");
  if (_rtp_rtcp)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (13) VideoSync");
  if (_videoSync)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (14) VolumeControl");
  if (_volumeControl)
    printf("\n");
  else
    printf(" (NA)\n");
  printf(" (15) AudioProcessing");
  if (_apm)
    printf("\n");
  else
    printf(" (NA)\n");
  printf("\n: ");

  ExtendedSelection xsel(XSEL_Invalid);
  int selection(0);
  dummy = scanf("%d", &selection);

  switch (selection) {
    case 0:
      xsel = XSEL_None;
      break;
    case 1:
      if (_base)
        xsel = XSEL_Base;
      break;
    case 2:
      if (_callReport)
        xsel = XSEL_CallReport;
      break;
    case 3:
      if (_codec)
        xsel = XSEL_Codec;
      break;
    case 4:
      if (_dtmf)
        xsel = XSEL_DTMF;
      break;
    case 5:
      if (_encryption)
        xsel = XSEL_Encryption;
      break;
    case 6:
      if (_externalMedia)
        xsel = XSEL_ExternalMedia;
      break;
    case 7:
      if (_file)
        xsel = XSEL_File;
      break;
    case 8:
      if (_file)
        xsel = XSEL_Mixing;
      break;
    case 9:
      if (_hardware)
        xsel = XSEL_Hardware;
      break;
    case 10:
      if (_netEqStats)
        xsel = XSEL_NetEqStats;
      break;
    case 11:
      if (_network)
        xsel = XSEL_Network;
      break;
    case 12:
      if (_rtp_rtcp)
        xsel = XSEL_RTP_RTCP;
      break;
    case 13:
      if (_videoSync)
        xsel = XSEL_VideoSync;
      break;
    case 14:
      if (_volumeControl)
        xsel = XSEL_VolumeControl;
      break;
    case 15:
      if (_apm)
        xsel = XSEL_AudioProcessing;
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

VoETestManager::VoETestManager()
    : initialized_(false),
      voice_engine_(NULL),
      voe_base_(0),
      voe_call_report_(0),
      voe_codec_(0),
      voe_dtmf_(0),
      voe_encrypt_(0),
      voe_xmedia_(0),
      voe_file_(0),
      voe_hardware_(0),
      voe_network_(0),
#ifdef _TEST_NETEQ_STATS_
      voe_neteq_stats_(NULL),
#endif
      voe_rtp_rtcp_(0),
      voe_vsync_(0),
      voe_volume_control_(0),
      voe_apm_(0)
{
}

VoETestManager::~VoETestManager() {
}

bool VoETestManager::Init() {
  if (initialized_)
    return true;

  if (VoiceEngine::SetTraceFile(NULL) != -1) {
    // should not be possible to call a Trace method before the VoE is
    // created
    TEST_LOG("\nError at line: %i (VoiceEngine::SetTraceFile()"
      "should fail)!\n", __LINE__);
    return false;
  }

  voice_engine_ = VoiceEngine::Create();
  if (!voice_engine_) {
    TEST_LOG("Failed to create VoiceEngine\n");
    return false;
  }

  return true;
}

void VoETestManager::GetInterfaces() {
  if (voice_engine_) {
    voe_base_ = VoEBase::GetInterface(voice_engine_);
    voe_codec_ = VoECodec::GetInterface(voice_engine_);
    voe_volume_control_ = VoEVolumeControl::GetInterface(voice_engine_);
    voe_dtmf_ = VoEDtmf::GetInterface(voice_engine_);
    voe_rtp_rtcp_ = VoERTP_RTCP::GetInterface(voice_engine_);
    voe_apm_ = VoEAudioProcessing::GetInterface(voice_engine_);
    voe_network_ = VoENetwork::GetInterface(voice_engine_);
    voe_file_ = VoEFile::GetInterface(voice_engine_);
#ifdef _TEST_VIDEO_SYNC_
    voe_vsync_ = VoEVideoSync::GetInterface(voice_engine_);
#endif
    voe_encrypt_ = VoEEncryption::GetInterface(voice_engine_);
    voe_hardware_ = VoEHardware::GetInterface(voice_engine_);
    // Set the audio layer to use in all tests
    if (voe_hardware_) {
      int res = voe_hardware_->SetAudioDeviceLayer(TESTED_AUDIO_LAYER);
      if (res < 0) {
        printf("\nERROR: failed to set audio layer to use in "
          "testing\n");
      } else {
        printf("\nAudio layer %d will be used in testing\n",
               TESTED_AUDIO_LAYER);
      }
    }
#ifdef _TEST_XMEDIA_
    voe_xmedia_ = VoEExternalMedia::GetInterface(voice_engine_);
#endif
#ifdef _TEST_CALL_REPORT_
    voe_call_report_ = VoECallReport::GetInterface(voice_engine_);
#endif
#ifdef _TEST_NETEQ_STATS_
    voe_neteq_stats_ = VoENetEqStats::GetInterface(voice_engine_);
#endif
  }
}

int VoETestManager::ReleaseInterfaces() {
  int err(0), remInt(1), j(0);
  bool releaseOK(true);

  if (voe_base_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_base_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d base interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    // try to release one addition time (should fail)
    TEST_MUSTPASS(-1 != voe_base_->Release());
    err = voe_base_->LastError();
    // it is considered safe to delete even if Release has been called
    // too many times
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_codec_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_codec_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d codec interfaces"
        " (should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_codec_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_volume_control_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_volume_control_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d volume interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_volume_control_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_dtmf_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_dtmf_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d dtmf interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_dtmf_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_rtp_rtcp_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_rtp_rtcp_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d rtp/rtcp interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_rtp_rtcp_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_apm_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_apm_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d apm interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_apm_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_network_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_network_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d network interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_network_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_file_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_file_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d file interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_file_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
#ifdef _TEST_VIDEO_SYNC_
  if (voe_vsync_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_vsync_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d video sync interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_vsync_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
#endif
  if (voe_encrypt_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_encrypt_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d encryption interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_encrypt_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
  if (voe_hardware_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_hardware_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d hardware interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_hardware_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
#ifdef _TEST_XMEDIA_
  if (voe_xmedia_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_xmedia_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d external media interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_xmedia_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
#endif
#ifdef _TEST_CALL_REPORT_
  if (voe_call_report_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_call_report_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d call report interfaces"
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_call_report_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
#endif
#ifdef _TEST_NETEQ_STATS_
  if (voe_neteq_stats_) {
    for (remInt = 1, j = 0; remInt > 0; j++)
      TEST_MUSTPASS(-1 == (remInt = voe_neteq_stats_->Release()));
    if (j > 1) {
      TEST_LOG("\n\n*** Error: released %d neteq stat interfaces "
        "(should only be 1) \n", j);
      releaseOK = false;
    }
    TEST_MUSTPASS(-1 != voe_neteq_stats_->Release());
    err = voe_base_->LastError();
    TEST_MUSTPASS(err != VE_INTERFACE_NOT_FOUND);
  }
#endif
  if (false == VoiceEngine::Delete(voice_engine_)) {
    TEST_LOG("\n\nVoiceEngine::Delete() failed. \n");
    releaseOK = false;
  }

  if (VoiceEngine::SetTraceFile(NULL) != -1) {
    TEST_LOG("\nError at line: %i (VoiceEngine::SetTraceFile()"
      "should fail)!\n", __LINE__);
  }

  return (releaseOK == true) ? 0 : -1;
}

int VoETestManager::SetUp(ErrorObserver* error_observer) {
  char char_buffer[1024];

  TEST_MUSTPASS(voe_base_->Init());

#if defined(WEBRTC_ANDROID)
  TEST_MUSTPASS(voe_hardware_->SetLoudspeakerStatus(false));
#endif

  TEST_MUSTPASS(voe_base_->RegisterVoiceEngineObserver(*error_observer));

  TEST_LOG("Get version \n");
  TEST_MUSTPASS(voe_base_->GetVersion(char_buffer));
  TEST_LOG("--------------------\n%s\n--------------------\n", char_buffer);

  TEST_LOG("Create channel \n");
  int nChannels = voe_base_->MaxNumOfChannels();
  TEST_MUSTPASS(!(nChannels > 0));
  TEST_LOG("Max number of channels = %d \n", nChannels);
  TEST_MUSTPASS(voe_base_->CreateChannel());

  return 0;
}

int VoETestManager::TestStartStreaming(FakeExternalTransport& channel0_transport) {
  TEST_LOG("\n\n+++ Starting streaming +++\n\n");

#ifdef WEBRTC_EXTERNAL_TRANSPORT
  TEST_LOG("Enabling external transport \n");
  TEST_MUSTPASS(voe_network_->RegisterExternalTransport(0, channel0_transport));
#else
  TEST_LOG("Setting send and receive parameters \n");
  TEST_MUSTPASS(voe_base_->SetSendDestination(0, 8000, "127.0.0.1"));
  // No IP specified => "0.0.0.0" will be stored.
  TEST_MUSTPASS(voe_base_->SetLocalReceiver(0,8000));

  CodecInst Jing_inst;
  Jing_inst.channels = 1;
  Jing_inst.pacsize = 160;
  Jing_inst.plfreq = 8000;
  Jing_inst.pltype = 0;
  Jing_inst.rate = 64000;
  strcpy(Jing_inst.plname, "PCMU");
  TEST_MUSTPASS(voe_codec_->SetSendCodec(0, Jing_inst));

  int port = -1;
  int src_port = -1;
  int rtcp_port = -1;
  char ip_address[64] = { 0 };
  strcpy(ip_address, "10.10.10.10");
  TEST_MUSTPASS(voe_base_->GetSendDestination(0, port, ip_address, src_port,
                                         rtcp_port));
  TEST_MUSTPASS(8000 != port);
  TEST_MUSTPASS(8000 != src_port);
  TEST_MUSTPASS(8001 != rtcp_port);
  TEST_MUSTPASS(_stricmp(ip_address, "127.0.0.1"));

  port = -1;
  rtcp_port = -1;
  TEST_MUSTPASS(voe_base_->GetLocalReceiver(0, port, rtcp_port, ip_address));
  TEST_MUSTPASS(8000 != port);
  TEST_MUSTPASS(8001 != rtcp_port);
  TEST_MUSTPASS(_stricmp(ip_address, "0.0.0.0"));
#endif
  return 0;
}

int VoETestManager::TestStartPlaying() {
  TEST_LOG("Start listening, playout and sending \n");
  TEST_MUSTPASS(voe_base_->StartReceive(0));
  TEST_MUSTPASS(voe_base_->StartPlayout(0));
  TEST_MUSTPASS(voe_base_->StartSend(0));

  // Run in full duplex.
  TEST_LOG("You should now hear yourself, running default codec (PCMU)\n");
  SLEEP(2000);

  if (voe_file_) {
    TEST_LOG("Start playing a file as microphone, so you don't need to"
      " speak all the time\n");
    TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(0,
            AudioFilename(),
            true,
            true));
    SLEEP(1000);
  }
  return 0;
}

int VoETestManager::DoStandardTest() {
  // Ensure we have all input files:
  TEST_MUSTPASS(!strcmp("", AudioFilename()));

  TEST_LOG("\n\n+++ Base tests +++\n\n");

  ErrorObserver error_observer;
  if (SetUp(&error_observer) != 0) return -1;

  voe_network_->SetSourceFilter(0, 0);

  FakeExternalTransport channel0_transport(voe_network_);
  if (TestStartStreaming(channel0_transport) != 0) return -1;
  if (TestStartPlaying() != 0) return -1;

#ifndef _TEST_BASE_
  TEST_LOG("\n\n+++ (Base) tests NOT ENABLED +++\n");
#endif // #ifdef _TEST_BASE_

#ifdef WEBRTC_CODEC_RED
  TEST_LOG("Enabling FEC \n");
  TEST_MUSTPASS(voe_rtp_rtcp_->SetFECStatus(0, true));
  SLEEP(2000);

  TEST_LOG("Disabling FEC\n");
  TEST_MUSTPASS(voe_rtp_rtcp_->SetFECStatus(0, false));
  SLEEP(2000);
#else
  TEST_LOG("Skipping FEC tests - WEBRTC_CODEC_RED not defined \n");
#endif // #ifdef WEBRTC_CODEC_RED

  ////////
  // Dtmf

#ifdef _TEST_DTMF_
  TEST_LOG("\n\n+++ Dtmf tests +++\n\n");

  TEST_LOG("Making sure Dtmf Feedback is enabled by default \n");
  bool dtmfFeedback = false, dtmfDirectFeedback = true;
  TEST_MUSTPASS(voe_dtmf_->GetDtmfFeedbackStatus(dtmfFeedback,
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
  TEST_MUSTPASS(voe_codec_->SetSendCodec(0, ci));

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
    TEST_MUSTPASS(voe_dtmf_->RegisterTelephoneEventDetection(0, detMethod, *d));
#else
  TEST_LOG("Skipping Dtmf detection tests - WEBRTC_DTMF_DETECTION not"
    " defined or _INSTRUMENTATION_TESTING_ defined \n");
#endif

  TEST_MUSTPASS(voe_dtmf_->SetDtmfFeedbackStatus(false));
  TEST_LOG("Sending in-band telephone events:");
  for (int i = 0; i < 16; i++) {
    TEST_LOG("\n  %d ", i);
    fflush(NULL);
    TEST_MUSTPASS(voe_dtmf_->SendTelephoneEvent(0, i, false, 160, 10));
    SLEEP(500);
  }
#ifdef WEBRTC_CODEC_AVT
  TEST_LOG("\nSending out-of-band telephone events:");
  for (int i = 0; i < 16; i++) {
    TEST_LOG("\n  %d ", i);
    fflush(NULL);
    TEST_MUSTPASS(voe_dtmf_->SendTelephoneEvent(0, i, true));
    SLEEP(500);
  }
  // Testing 2 non-Dtmf events
  int num = 32;
  TEST_LOG("\n  %d ", num);
  fflush(NULL);
  TEST_MUSTPASS(voe_dtmf_->SendTelephoneEvent(0, num, true));
  SLEEP(500);
  num = 110;
  TEST_LOG("\n  %d ", num);
  fflush(NULL);
  TEST_MUSTPASS(voe_dtmf_->SendTelephoneEvent(0, num, true));
  SLEEP(500);
  ANL();
#endif
#if (defined(WEBRTC_DTMF_DETECTION) && !defined(_INSTRUMENTATION_TESTING_))
  TEST_MUSTPASS(voe_dtmf_->DeRegisterTelephoneEventDetection(0));
  TEST_LOG("Detected %d events \n", d->counter);
  int expectedCount = 32; // For 0 == h
  if (1 == h) expectedCount = 18;
  if (2 == h) expectedCount = 50;
  TEST_MUSTPASS(d->counter != expectedCount);
  d->counter = 0;
} // for loop

TEST_LOG("Testing no detection after disabling:");
TEST_MUSTPASS(voe_dtmf_->DeRegisterTelephoneEventDetection(0));
TEST_LOG(" 0");
TEST_MUSTPASS(voe_dtmf_->SendTelephoneEvent(0, 0, false));
SLEEP(500);
TEST_LOG(" 1");
TEST_MUSTPASS(voe_dtmf_->SendTelephoneEvent(0, 1, true));
SLEEP(500);
TEST_LOG("\nDtmf tones sent: 2, detected: %d \n", d->counter);
TEST_MUSTPASS(0 != d->counter);
delete d;

TEST_MUSTPASS(voe_codec_->GetCodec(0, ci));
TEST_LOG("Back to first codec in list: %s\n", ci.plname);
TEST_MUSTPASS(voe_codec_->SetSendCodec(0, ci));
#endif

#ifndef MAC_IPHONE
#ifdef WEBRTC_CODEC_AVT
  TEST_LOG("Disabling Dtmf playout (no tone should be heard) \n");
  TEST_MUSTPASS(voe_dtmf_->SetDtmfPlayoutStatus(0, false));
  TEST_MUSTPASS(voe_dtmf_->SendTelephoneEvent(0, 0, true));
  SLEEP(500);

  TEST_LOG("Enabling Dtmf playout (tone should be heard) \n");
  TEST_MUSTPASS(voe_dtmf_->SetDtmfPlayoutStatus(0, true));
  TEST_MUSTPASS(voe_dtmf_->SendTelephoneEvent(0, 0, true));
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
  if (voe_codec_) {
    int nc = voe_codec_->NumOfCodecs();
    for (int i = 0; i < nc; i++) {
      TEST_MUSTPASS(voe_codec_->GetCodec(i, c2));
      if (!_stricmp("telephone-event", c2.plname)) {
        c2.pltype = 88; // use 88 instead of default 106
        TEST_MUSTPASS(voe_base_->StopSend(0));
        TEST_MUSTPASS(voe_base_->StopPlayout(0));
        TEST_MUSTPASS(voe_base_->StopReceive(0));
        TEST_MUSTPASS(voe_codec_->SetRecPayloadType(0, c2));
        TEST_MUSTPASS(voe_base_->StartReceive(0));
        TEST_MUSTPASS(voe_base_->StartPlayout(0));
        TEST_MUSTPASS(voe_base_->StartSend(0));
        TEST_LOG("Start playing a file as microphone again \n");
        TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(
                0, AudioFilename(), true, true));
        break;
      }
    }
  }

  SLEEP(500);

  // Next, we must modify the sending side as well
  TEST_MUSTPASS(voe_dtmf_->SetSendTelephoneEventPayloadType(0, c2.pltype));

  TEST_LOG("Outband Dtmf test with modified Dtmf payload:");
  for (int i = 0; i < 16; i++) {
    TEST_LOG(" %d", i);
    fflush(NULL);
    TEST_MUSTPASS(voe_dtmf_->SendTelephoneEvent(0, i, true));
    SLEEP(500);
  }
  ANL();
#endif
  TEST_MUSTPASS(voe_dtmf_->SetDtmfFeedbackStatus(true, false));
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
  TEST_MUSTPASS(voe_volume_control_->GetSpeakerVolume(vol));
  TEST_MUSTPASS(!(vol <= 255));
  TEST_LOG("Setting speaker volume to 0\n");
  TEST_MUSTPASS(voe_volume_control_->SetSpeakerVolume(0));
  SLEEP(1000);
  TEST_LOG("Setting speaker volume to 255\n");
  TEST_MUSTPASS(voe_volume_control_->SetSpeakerVolume(255));
  SLEEP(1000);
  TEST_LOG("Setting speaker volume back to saved value\n");
  TEST_MUSTPASS(voe_volume_control_->SetSpeakerVolume(vol));
  SLEEP(1000);
#endif // #if !defined(MAC_IPHONE)
  if (voe_file_) {
    TEST_LOG("==> Talk into the microphone \n");
    TEST_MUSTPASS(voe_file_->StopPlayingFileAsMicrophone(0));
    SLEEP(1000);
  }

#if (!defined(MAC_IPHONE) && !defined(WEBRTC_ANDROID))
  // Mic volume test
#if defined(_TEST_AUDIO_PROCESSING_) && defined(WEBRTC_VOICE_ENGINE_AGC)
  bool agcTemp(true);
  AgcModes agcModeTemp(kAgcAdaptiveAnalog);
  TEST_MUSTPASS(voe_apm_->GetAgcStatus(agcTemp, agcModeTemp)); // current state
  TEST_LOG("Turn off AGC\n");
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(false));
#endif
  TEST_LOG("Saving Mic volume\n");
  TEST_MUSTPASS(voe_volume_control_->GetMicVolume(vol));
  TEST_MUSTPASS(!(vol <= 255));
  TEST_LOG("Setting Mic volume to 0\n");
  TEST_MUSTPASS(voe_volume_control_->SetMicVolume(0));
  SLEEP(1000);
  TEST_LOG("Setting Mic volume to 255\n");
  TEST_MUSTPASS(voe_volume_control_->SetMicVolume(255));
  SLEEP(1000);
  TEST_LOG("Setting Mic volume back to saved value\n");
  TEST_MUSTPASS(voe_volume_control_->SetMicVolume(vol));
  SLEEP(1000);
#if defined(_TEST_AUDIO_PROCESSING_) && defined(WEBRTC_VOICE_ENGINE_AGC)
  TEST_LOG("Reset AGC to previous state\n");
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(agcTemp, agcModeTemp));
#endif
#endif // #if (!defined(MAC_IPHONE) && !defined(WEBRTC_ANDROID))
  // Input mute test
  TEST_LOG("Enabling input muting\n");
  bool mute = true;
  TEST_MUSTPASS(voe_volume_control_->GetInputMute(0, mute));
  TEST_MUSTPASS(mute);
  TEST_MUSTPASS(voe_volume_control_->SetInputMute(0, true));
  TEST_MUSTPASS(voe_volume_control_->GetInputMute(0, mute));
  TEST_MUSTPASS(!mute);
  SLEEP(1000);
  TEST_LOG("Disabling input muting\n");
  TEST_MUSTPASS(voe_volume_control_->SetInputMute(0, false));
  TEST_MUSTPASS(voe_volume_control_->GetInputMute(0, mute));
  TEST_MUSTPASS(mute);
  SLEEP(1000);

#if (!defined(MAC_IPHONE) && !defined(WEBRTC_ANDROID))
  // System output mute test
  TEST_LOG("Enabling system output muting\n");
  bool outputMute = true;
  TEST_MUSTPASS(voe_volume_control_->GetSystemOutputMute(outputMute));
  TEST_MUSTPASS(outputMute);
  TEST_MUSTPASS(voe_volume_control_->SetSystemOutputMute(true));
  TEST_MUSTPASS(voe_volume_control_->GetSystemOutputMute(outputMute));
  TEST_MUSTPASS(!outputMute);
  SLEEP(1000);
  TEST_LOG("Disabling system output muting\n");
  TEST_MUSTPASS(voe_volume_control_->SetSystemOutputMute(false));
  TEST_MUSTPASS(voe_volume_control_->GetSystemOutputMute(outputMute));
  TEST_MUSTPASS(outputMute);
  SLEEP(1000);

  // System Input mute test
  TEST_LOG("Enabling system input muting\n");
  bool inputMute = true;
  TEST_MUSTPASS(voe_volume_control_->GetSystemInputMute(inputMute));
  TEST_MUSTPASS(inputMute);
  TEST_MUSTPASS(voe_volume_control_->SetSystemInputMute(true));
  // This is needed to avoid error using pulse
  SLEEP(100);
  TEST_MUSTPASS(voe_volume_control_->GetSystemInputMute(inputMute));
  TEST_MUSTPASS(!inputMute);
  SLEEP(1000);
  TEST_LOG("Disabling system input muting\n");
  TEST_MUSTPASS(voe_volume_control_->SetSystemInputMute(false));
  // This is needed to avoid error using pulse
  SLEEP(100);
  TEST_MUSTPASS(voe_volume_control_->GetSystemInputMute(inputMute));
  TEST_MUSTPASS(inputMute);
  SLEEP(1000);
#endif // #if (!defined(MAC_IPHONE) && !defined(WEBRTC_ANDROID))
#if(!defined(MAC_IPHONE) && !defined(WEBRTC_ANDROID))
  // Test Input & Output levels
  TEST_LOG("Testing input & output levels for 10 seconds (dT=1 second)\n");
  TEST_LOG("Speak in microphone to vary the levels...\n");
  unsigned int inputLevel(0);
  unsigned int outputLevel(0);
  unsigned int inputLevelFullRange(0);
  unsigned int outputLevelFullRange(0);

  for (int t = 0; t < 5; t++) {
    SLEEP(1000);
    TEST_MUSTPASS(voe_volume_control_->GetSpeechInputLevel(inputLevel));
    TEST_MUSTPASS(voe_volume_control_->GetSpeechOutputLevel(0, outputLevel));
    TEST_MUSTPASS(voe_volume_control_->GetSpeechInputLevelFullRange(
            inputLevelFullRange));
    TEST_MUSTPASS(voe_volume_control_->GetSpeechOutputLevelFullRange(
            0, outputLevelFullRange));
    TEST_LOG("    warped levels (0-9)    : in=%5d, out=%5d\n",
             inputLevel, outputLevel);
    TEST_LOG("    linear levels (0-32768): in=%5d, out=%5d\n",
             inputLevelFullRange, outputLevelFullRange);
  }
#endif // #if (!defined(MAC_IPHONE) && !defined(WEBRTC_ANDROID))
  if (voe_file_) {
    TEST_LOG("==> Start playing a file as microphone again \n");
    TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(0,
            AudioFilename(),
            true,
            true));
    SLEEP(1000);
  }

#if !defined(MAC_IPHONE)
  // Channel scaling test
  TEST_LOG("Channel scaling\n");
  float scaling = -1.0;
  TEST_MUSTPASS(voe_volume_control_->GetChannelOutputVolumeScaling(0, scaling));
  TEST_MUSTPASS(1.0 != scaling);
  TEST_MUSTPASS(voe_volume_control_->SetChannelOutputVolumeScaling(0,
                                                                   (float)0.1));
  TEST_MUSTPASS(voe_volume_control_->GetChannelOutputVolumeScaling(0, scaling));
  TEST_MUSTPASS(!((scaling > 0.099) && (scaling < 0.101)));
  SLEEP(1000);
  TEST_MUSTPASS(voe_volume_control_->SetChannelOutputVolumeScaling(0,
                                                                   (float)1.0));
  TEST_MUSTPASS(voe_volume_control_->GetChannelOutputVolumeScaling(0, scaling));
  TEST_MUSTPASS(1.0 != scaling);
#endif // #if !defined(MAC_IPHONE)
#if !defined(MAC_IPHONE) && !defined(WEBRTC_ANDROID)
  // Channel panning test
  TEST_LOG("Channel panning\n");
  float left = -1.0, right = -1.0;
  TEST_MUSTPASS(voe_volume_control_->GetOutputVolumePan(0, left, right));
  TEST_MUSTPASS(!((left == 1.0) && (right == 1.0)));
  TEST_LOG("Panning to left\n");
  TEST_MUSTPASS(voe_volume_control_->SetOutputVolumePan(0, (float)0.8,
                                                        (float)0.1));
  TEST_MUSTPASS(voe_volume_control_->GetOutputVolumePan(0, left, right));
  TEST_MUSTPASS(!((left > 0.799) && (left < 0.801)));
  TEST_MUSTPASS(!((right > 0.099) && (right < 0.101)));
  SLEEP(1000);
  TEST_LOG("Back to center\n");
  TEST_MUSTPASS(voe_volume_control_->SetOutputVolumePan(0, (float)1.0,
                                                        (float)1.0));
  SLEEP(1000);
  left = -1.0;
  right = -1.0;
  TEST_MUSTPASS(voe_volume_control_->GetOutputVolumePan(0, left, right));
  TEST_MUSTPASS(!((left == 1.0) && (right == 1.0)));
  TEST_LOG("Panning channel to right\n");
  TEST_MUSTPASS(voe_volume_control_->SetOutputVolumePan(0, (float)0.1,
                                                        (float)0.8));
  SLEEP(100);
  TEST_MUSTPASS(voe_volume_control_->GetOutputVolumePan(0, left, right));
  TEST_MUSTPASS(!((left > 0.099) && (left < 0.101)));
  TEST_MUSTPASS(!((right > 0.799) && (right < 0.801)));
  SLEEP(1000);
  TEST_LOG("Channel back to center\n");
  TEST_MUSTPASS(voe_volume_control_->SetOutputVolumePan(0, (float)1.0,
                                                        (float)1.0));
  SLEEP(1000);
#else
  TEST_LOG("Skipping stereo tests\n");
#endif // #if !defined(MAC_IPHONE) && !defined(WEBRTC_ANDROID))
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
#if (defined(MAC_IPHONE) || defined(WEBRTC_ANDROID))
  TEST_LOG("Must be OFF by default\n");
  test = true;
  AgcModes agcMode = kAgcAdaptiveAnalog;
  TEST_MUSTPASS(voe_apm_->GetAgcStatus(test, agcMode));
  TEST_MUSTPASS(test);
  TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);
#else
  TEST_LOG("Must be ON by default\n");
  test = false;
  AgcModes agcMode = kAgcAdaptiveAnalog;
  TEST_MUSTPASS(voe_apm_->GetAgcStatus(test, agcMode));
  TEST_MUSTPASS(!test);
  TEST_MUSTPASS(kAgcAdaptiveAnalog != agcMode);

  TEST_LOG("Turn off AGC\n");
  // must set value in first call!
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(false, kAgcDefault));
  TEST_LOG("Should be OFF now\n");
  TEST_MUSTPASS(voe_apm_->GetAgcStatus(test, agcMode));
  TEST_MUSTPASS(test);
  TEST_MUSTPASS(kAgcAdaptiveAnalog != agcMode);
#endif // #if (defined(MAC_IPHONE) || defined(WEBRTC_ANDROID))
  TEST_LOG("Turn ON AGC\n");
#if (defined(MAC_IPHONE) || defined(WEBRTC_ANDROID))
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(true, kAgcAdaptiveDigital));
#else
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(true));
#endif
  TEST_LOG("Should be ON now\n");
  TEST_MUSTPASS(voe_apm_->GetAgcStatus(test, agcMode));
  TEST_MUSTPASS(!test);
#if (defined(MAC_IPHONE) || defined(WEBRTC_ANDROID))
  TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);
#else
  TEST_MUSTPASS(kAgcAdaptiveAnalog != agcMode);
#endif

#if (defined(MAC_IPHONE) || defined(WEBRTC_ANDROID))
  TEST_LOG("Testing Type settings\n");
  // Should fail
  TEST_MUSTPASS(!voe_apm_->SetAgcStatus(true, kAgcAdaptiveAnalog));
  // Should fail
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(true, kAgcFixedDigital));
  // Should fail
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(true, kAgcAdaptiveDigital));

  TEST_LOG("Turn off AGC\n");
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(false));
  TEST_LOG("Should be OFF now\n");
  TEST_MUSTPASS(voe_apm_->GetAgcStatus(test, agcMode));
  TEST_MUSTPASS(test);
  TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);
#else
  TEST_LOG("Testing Mode settings\n");
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(true, kAgcFixedDigital));
  TEST_MUSTPASS(voe_apm_->GetAgcStatus(test, agcMode));
  TEST_MUSTPASS(kAgcFixedDigital != agcMode);
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(true, kAgcAdaptiveDigital));
  TEST_MUSTPASS(voe_apm_->GetAgcStatus(test, agcMode));
  TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(true, kAgcAdaptiveAnalog));
  TEST_MUSTPASS(voe_apm_->GetAgcStatus(test, agcMode));
  TEST_MUSTPASS(kAgcAdaptiveAnalog != agcMode);
#endif // #if (defined(MAC_IPHONE) || defined(WEBRTC_ANDROID))
  TEST_LOG("rxAGC calls\n");
  // Note the following test is not tested in iphone, android and wince,
  // you may run into issue

  bool rxAGCTemp(false);
  AgcModes rxAGCModeTemp(kAgcAdaptiveAnalog);
  // Store current state
  TEST_MUSTPASS(voe_apm_->GetAgcStatus(rxAGCTemp, rxAGCModeTemp));
  TEST_LOG("Turn off near-end AGC\n");
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(false));

  TEST_LOG("rxAGC Must be OFF by default\n");
  test = true;
  AgcModes rxAGCMode = kAgcAdaptiveDigital;
  TEST_MUSTPASS(voe_apm_->GetRxAgcStatus(0, test, agcMode));
  TEST_MUSTPASS(test);
  TEST_MUSTPASS(kAgcAdaptiveDigital != rxAGCMode);

  TEST_LOG("Turn off rxAGC\n");
  // must set value in first call!
  TEST_MUSTPASS(voe_apm_->SetRxAgcStatus(0, false, kAgcDefault));
  TEST_LOG("Should be OFF now\n");
  TEST_MUSTPASS(voe_apm_->GetRxAgcStatus(0, test, agcMode));
  TEST_MUSTPASS(test);
  TEST_MUSTPASS(kAgcAdaptiveDigital != rxAGCMode);

  TEST_LOG("Turn ON AGC\n");
  TEST_MUSTPASS(voe_apm_->SetRxAgcStatus(0, true));
  TEST_LOG("Should be ON now\n");
  TEST_MUSTPASS(voe_apm_->GetRxAgcStatus(0, test, agcMode));
  TEST_MUSTPASS(!test);
  TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);

  TEST_LOG("Testing Type settings\n");
  // Should fail
  TEST_MUSTPASS(!voe_apm_->SetRxAgcStatus(0, true, kAgcAdaptiveAnalog));
  TEST_MUSTPASS(voe_apm_->SetRxAgcStatus(0, true, kAgcFixedDigital));
  TEST_MUSTPASS(voe_apm_->GetRxAgcStatus(0, test, agcMode));
  TEST_MUSTPASS(kAgcFixedDigital != agcMode);
  TEST_MUSTPASS(voe_apm_->SetRxAgcStatus(0, true, kAgcAdaptiveDigital));
  TEST_MUSTPASS(voe_apm_->GetRxAgcStatus(0, test, agcMode));
  TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);

  TEST_LOG("Turn off AGC\n");
  TEST_MUSTPASS(voe_apm_->SetRxAgcStatus(0, false));
  TEST_LOG("Should be OFF now\n");
  TEST_MUSTPASS(voe_apm_->GetRxAgcStatus(0, test, agcMode));
  TEST_MUSTPASS(test);
  TEST_MUSTPASS(kAgcAdaptiveDigital != agcMode);

  // recover the old AGC mode
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(rxAGCTemp, rxAGCModeTemp));

#else
  TEST_LOG("Skipping AGC tests - WEBRTC_VOICE_ENGINE_AGC not defined \n");
#endif  // #ifdef WEBRTC_VOICE_ENGINE_AGC
#ifdef WEBRTC_VOICE_ENGINE_ECHO
  TEST_LOG("EC calls\n");
  TEST_LOG("Must be OFF by default\n");
#if (defined(MAC_IPHONE) || defined(WEBRTC_ANDROID))
  const EcModes ecModeDefault = kEcAecm;
#else
  const EcModes ecModeDefault = kEcAec;
#endif
  test = true;
  EcModes ecMode = kEcAec;
  AecmModes aecmMode = kAecmSpeakerphone;
  bool enabledCNG(false);
  TEST_MUSTPASS(voe_apm_->GetEcStatus(test, ecMode));
  TEST_MUSTPASS(test);
  TEST_MUSTPASS(ecModeDefault != ecMode);
  TEST_MUSTPASS(voe_apm_->GetAecmMode(aecmMode, enabledCNG));
  TEST_LOG("default AECM: mode=%d CNG: mode=%d\n", aecmMode, enabledCNG);
  TEST_MUSTPASS(kAecmSpeakerphone != aecmMode);
  TEST_MUSTPASS(enabledCNG != true);
  TEST_MUSTPASS(voe_apm_->SetAecmMode(kAecmQuietEarpieceOrHeadset, false));
  TEST_MUSTPASS(voe_apm_->GetAecmMode(aecmMode, enabledCNG));
  TEST_LOG("change AECM to mode=%d CNG to false\n", aecmMode);
  TEST_MUSTPASS(aecmMode != kAecmQuietEarpieceOrHeadset);
  TEST_MUSTPASS(enabledCNG != false);

  TEST_LOG("Turn ON EC\n");
  TEST_MUSTPASS(voe_apm_->SetEcStatus(true, ecModeDefault));
  TEST_LOG("Should be ON now\n");
  TEST_MUSTPASS(voe_apm_->GetEcStatus(test, ecMode));
  TEST_MUSTPASS(!test);
  TEST_MUSTPASS(ecModeDefault != ecMode);

#if (!defined(MAC_IPHONE) && !defined(WEBRTC_ANDROID))
  TEST_MUSTPASS(voe_apm_->SetEcStatus(true, kEcAec));
  TEST_MUSTPASS(voe_apm_->GetEcStatus(test, ecMode));
  TEST_MUSTPASS(kEcAec != ecMode);

  TEST_MUSTPASS(voe_apm_->SetEcStatus(true, kEcConference));
  TEST_MUSTPASS(voe_apm_->GetEcStatus(test, ecMode));
  TEST_MUSTPASS(kEcAec != ecMode);

  // the samplefreq for AudioProcessing is 32k, so it wont work to
  // activate AECM
  TEST_MUSTPASS(voe_apm_->SetEcStatus(true, kEcAecm));
  TEST_MUSTPASS(voe_apm_->GetEcStatus(test, ecMode));
  TEST_MUSTPASS(kEcAecm != ecMode);
#endif

  // set kEcAecm mode
  TEST_LOG("Testing AECM Mode settings\n");
  TEST_MUSTPASS(voe_apm_->SetEcStatus(true, kEcAecm));
  TEST_MUSTPASS(voe_apm_->GetEcStatus(test, ecMode));
  TEST_LOG("EC: enabled=%d, ECmode=%d\n", test, ecMode);
  TEST_MUSTPASS(test != true);
  TEST_MUSTPASS(ecMode != kEcAecm);

  // AECM mode, get and set
  TEST_MUSTPASS(voe_apm_->GetAecmMode(aecmMode, enabledCNG));
  TEST_MUSTPASS(aecmMode != kAecmQuietEarpieceOrHeadset);
  TEST_MUSTPASS(enabledCNG != false);
  TEST_MUSTPASS(voe_apm_->SetAecmMode(kAecmEarpiece, true));
  TEST_MUSTPASS(voe_apm_->GetAecmMode(aecmMode, enabledCNG));
  TEST_LOG("AECM: mode=%d CNG: mode=%d\n", aecmMode, enabledCNG);
  TEST_MUSTPASS(aecmMode != kAecmEarpiece);
  TEST_MUSTPASS(enabledCNG != true);
  TEST_MUSTPASS(voe_apm_->SetAecmMode(kAecmEarpiece, false));
  TEST_MUSTPASS(voe_apm_->GetAecmMode(aecmMode, enabledCNG));
  TEST_LOG("AECM: mode=%d CNG: mode=%d\n", aecmMode, enabledCNG);
  TEST_MUSTPASS(aecmMode != kAecmEarpiece);
  TEST_MUSTPASS(enabledCNG != false);
  TEST_MUSTPASS(voe_apm_->SetAecmMode(kAecmLoudEarpiece, true));
  TEST_MUSTPASS(voe_apm_->GetAecmMode(aecmMode, enabledCNG));
  TEST_LOG("AECM: mode=%d CNG: mode=%d\n", aecmMode, enabledCNG);
  TEST_MUSTPASS(aecmMode != kAecmLoudEarpiece);
  TEST_MUSTPASS(enabledCNG != true);
  TEST_MUSTPASS(voe_apm_->SetAecmMode(kAecmSpeakerphone, false));
  TEST_MUSTPASS(voe_apm_->GetAecmMode(aecmMode, enabledCNG));
  TEST_LOG("AECM: mode=%d CNG: mode=%d\n", aecmMode, enabledCNG);
  TEST_MUSTPASS(aecmMode != kAecmSpeakerphone);
  TEST_MUSTPASS(enabledCNG != false);
  TEST_MUSTPASS(voe_apm_->SetAecmMode(kAecmLoudSpeakerphone, true));
  TEST_MUSTPASS(voe_apm_->GetAecmMode(aecmMode, enabledCNG));
  TEST_LOG("AECM: mode=%d CNG: mode=%d\n", aecmMode, enabledCNG);
  TEST_MUSTPASS(aecmMode != kAecmLoudSpeakerphone);
  TEST_MUSTPASS(enabledCNG != true);

  TEST_LOG("Turn OFF AEC\n");
  TEST_MUSTPASS(voe_apm_->SetEcStatus(false));
  TEST_LOG("Should be OFF now\n");
  TEST_MUSTPASS(voe_apm_->GetEcStatus(test, ecMode));
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
  TEST_MUSTPASS(voe_apm_->GetNsStatus(test, nsMode));
  TEST_MUSTPASS(test);
  TEST_MUSTPASS(nsModeDefault != nsMode);

  TEST_LOG("Turn ON NS\n");
  TEST_MUSTPASS(voe_apm_->SetNsStatus(true));
  TEST_LOG("Should be ON now\n");
  TEST_MUSTPASS(voe_apm_->GetNsStatus(test, nsMode));
  TEST_MUSTPASS(!test);
  TEST_MUSTPASS(nsModeDefault != nsMode);

  TEST_LOG("Testing Mode settings\n");
  TEST_MUSTPASS(voe_apm_->SetNsStatus(true, kNsLowSuppression));
  TEST_MUSTPASS(voe_apm_->GetNsStatus(test, nsMode));
  TEST_MUSTPASS(kNsLowSuppression != nsMode);
  TEST_MUSTPASS(voe_apm_->SetNsStatus(true, kNsModerateSuppression));
  TEST_MUSTPASS(voe_apm_->GetNsStatus(test, nsMode));
  TEST_MUSTPASS(kNsModerateSuppression != nsMode);
  TEST_MUSTPASS(voe_apm_->SetNsStatus(true, kNsHighSuppression));
  TEST_MUSTPASS(voe_apm_->GetNsStatus(test, nsMode));
  TEST_MUSTPASS(kNsHighSuppression != nsMode);
  TEST_MUSTPASS(voe_apm_->SetNsStatus(true, kNsVeryHighSuppression));
  TEST_MUSTPASS(voe_apm_->GetNsStatus(test, nsMode));
  TEST_MUSTPASS(kNsVeryHighSuppression != nsMode);
  TEST_MUSTPASS(voe_apm_->SetNsStatus(true, kNsConference));
  TEST_MUSTPASS(voe_apm_->GetNsStatus(test, nsMode));
  TEST_MUSTPASS(kNsHighSuppression != nsMode);
  TEST_MUSTPASS(voe_apm_->SetNsStatus(true, kNsDefault));
  TEST_MUSTPASS(voe_apm_->GetNsStatus(test, nsMode));
  TEST_MUSTPASS(nsModeDefault != nsMode);

  TEST_LOG("Turn OFF NS\n");
  TEST_MUSTPASS(voe_apm_->SetNsStatus(false));
  TEST_LOG("Should be OFF now\n");
  TEST_MUSTPASS(voe_apm_->GetNsStatus(test, nsMode));
  TEST_MUSTPASS(test);

  TEST_LOG("rxNS calls\n");
  TEST_LOG("rxNS Must be OFF by default\n");

  TEST_MUSTPASS(voe_apm_->GetRxNsStatus(0, test, nsMode));
  TEST_MUSTPASS(test);
  TEST_MUSTPASS(nsModeDefault != nsMode);

  TEST_LOG("Turn ON rxNS\n");
  TEST_MUSTPASS(voe_apm_->SetRxNsStatus(0, true));
  TEST_LOG("Should be ON now\n");
  TEST_MUSTPASS(voe_apm_->GetRxNsStatus(0, test, nsMode));
  TEST_MUSTPASS(!test);
  TEST_MUSTPASS(nsModeDefault != nsMode);

  TEST_LOG("Testing Mode settings\n");
  TEST_MUSTPASS(voe_apm_->SetRxNsStatus(0, true, kNsLowSuppression));
  TEST_MUSTPASS(voe_apm_->GetRxNsStatus(0, test, nsMode));
  TEST_MUSTPASS(kNsLowSuppression != nsMode);
  TEST_MUSTPASS(voe_apm_->SetRxNsStatus(0, true, kNsModerateSuppression));
  TEST_MUSTPASS(voe_apm_->GetRxNsStatus(0, test, nsMode));
  TEST_MUSTPASS(kNsModerateSuppression != nsMode);
  TEST_MUSTPASS(voe_apm_->SetRxNsStatus(0, true, kNsHighSuppression));
  TEST_MUSTPASS(voe_apm_->GetRxNsStatus(0, test, nsMode));
  TEST_MUSTPASS(kNsHighSuppression != nsMode);
  TEST_MUSTPASS(voe_apm_->SetRxNsStatus(0, true, kNsVeryHighSuppression));
  TEST_MUSTPASS(voe_apm_->GetRxNsStatus(0, test, nsMode));
  TEST_MUSTPASS(kNsVeryHighSuppression != nsMode);
  TEST_MUSTPASS(voe_apm_->SetRxNsStatus(0, true, kNsConference));
  TEST_MUSTPASS(voe_apm_->GetRxNsStatus(0, test, nsMode));
  TEST_MUSTPASS(kNsHighSuppression != nsMode);
  TEST_MUSTPASS(voe_apm_->SetRxNsStatus(0, true, kNsDefault));
  TEST_MUSTPASS(voe_apm_->GetRxNsStatus(0, test, nsMode));
  TEST_MUSTPASS(nsModeDefault != nsMode);

  TEST_LOG("Turn OFF NS\n");
  TEST_MUSTPASS(voe_apm_->SetRxNsStatus(0, false));
  TEST_LOG("Should be OFF now\n");
  TEST_MUSTPASS(voe_apm_->GetRxNsStatus(0, test, nsMode));
  TEST_MUSTPASS(test);

#else
  TEST_LOG("Skipping NS tests - WEBRTC_VOICE_ENGINE_NR not defined \n");
#endif  // #ifdef WEBRTC_VOICE_ENGINE_NR
#if (!defined(MAC_IPHONE) && !defined(WEBRTC_ANDROID) && \
      defined(WEBRTC_VOICE_ENGINE_NR))
#ifdef WEBRTC_VOICE_ENGINE_ECHO
  bool enabled = false;
  TEST_LOG("EC Metrics calls\n");
  TEST_MUSTPASS(voe_apm_->GetEcMetricsStatus(enabled)); // check default
  TEST_MUSTPASS(enabled != false);
  TEST_MUSTPASS(voe_apm_->SetEcMetricsStatus(true)); // enable EC metrics
  // must enable AEC to get valid echo metrics
  TEST_MUSTPASS(voe_apm_->SetEcStatus(true, kEcAec));
  TEST_MUSTPASS(voe_apm_->GetEcMetricsStatus(enabled));
  TEST_MUSTPASS(enabled != true);

  TEST_LOG("Speak into microphone and check metrics for 10 seconds...\n");
  int ERL, ERLE, RERL, A_NLP;
  int delay_median = 0;
  int delay_std = 0;
  for (int t = 0; t < 5; t++) {
    SLEEP(2000);
    TEST_MUSTPASS(voe_apm_->GetEchoMetrics(ERL, ERLE, RERL, A_NLP));
    TEST_MUSTPASS(voe_apm_->GetEcDelayMetrics(delay_median, delay_std));
    TEST_LOG("    Echo  : ERL=%5d, ERLE=%5d, RERL=%5d, A_NLP=%5d [dB], "
      " delay median=%3d, delay std=%3d [ms]\n", ERL, ERLE, RERL, A_NLP,
      delay_median, delay_std);
  }
  TEST_MUSTPASS(voe_apm_->SetEcMetricsStatus(false)); // disable echo metrics
#else
  TEST_LOG("Skipping Echo Control metrics tests -"
      " WEBRTC_VOICE_ENGINE_ECHO not defined \n");
#endif  // #ifdef WEBRTC_VOICE_ENGINE_ECHO
#else
  TEST_LOG("Skipping apm metrics tests - MAC_IPHONE/WEBRTC_ANDROID defined \n");
#endif // #if (!defined(MAC_IPHONE) && !d...
  // VAD/DTX indication
  TEST_LOG("Get voice activity indication \n");
  if (voe_codec_) {
    bool v = true, dummy2;
    VadModes dummy1;
    TEST_MUSTPASS(voe_codec_->GetVADStatus(0, v, dummy1, dummy2));
    TEST_MUSTPASS(v); // Make sure VAD is disabled
  }
  TEST_MUSTPASS(1 != voe_apm_->VoiceActivityIndicator(0));
  if (voe_codec_ && voe_volume_control_) {
    TEST_LOG("RX VAD detections may vary depending on current signal"
      " and mic input \n");
#if !defined(WEBRTC_ANDROID) && !defined(MAC_IPHONE)
    RxCallback rxc;
    TEST_MUSTPASS(voe_apm_->RegisterRxVadObserver(0, rxc));
#endif
    TEST_MUSTPASS(voe_codec_->SetVADStatus(0, true));
    TEST_MUSTPASS(voe_volume_control_->SetInputMute(0, true));
    if (voe_file_) {
      TEST_MUSTPASS(voe_file_->StopPlayingFileAsMicrophone(0));
    }
    SLEEP(500); // After sleeping we should have detected silence
    TEST_MUSTPASS(0 != voe_apm_->VoiceActivityIndicator(0));
#if !defined(WEBRTC_ANDROID) && !defined(MAC_IPHONE)
    TEST_MUSTPASS(0 != rxc._vadDecision);
#endif
    if (voe_file_) {
      TEST_LOG("Start playing a file as microphone again \n");
      TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(
              0, AudioFilename(), true, true));
    } else {
      TEST_LOG("==> Make sure you talk into the microphone \n");
    }
    TEST_MUSTPASS(voe_codec_->SetVADStatus(0, false));
    TEST_MUSTPASS(voe_volume_control_->SetInputMute(0, false));
    SLEEP(500); // Sleep time selected by looking in mic play file, after
    // sleep we should have detected voice
    TEST_MUSTPASS(1 != voe_apm_->VoiceActivityIndicator(0));
#if !defined(WEBRTC_ANDROID) && !defined(MAC_IPHONE)
    TEST_MUSTPASS(1 != rxc._vadDecision);
    TEST_LOG("Disabling RX VAD detection, make sure you see no "
      "detections\n");
    TEST_MUSTPASS(voe_apm_->DeRegisterRxVadObserver(0));
    SLEEP(2000);
#endif
  } else {
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
  fileName[0] = (char) 0xc3;
  fileName[1] = (char) 0xa5;
  fileName[2] = (char) 0xc3;
  fileName[3] = (char) 0xa4;
  fileName[4] = (char) 0xc3;
  fileName[5] = (char) 0xb6;
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
  TEST_MUSTPASS(voe_file_->StopPlayingFileAsMicrophone(0));
  TEST_LOG("==> Talk into the microphone \n");
  SLEEP(1000);
  TEST_LOG("Record mic for 3 seconds in PCM format\n");
  TEST_MUSTPASS(voe_file_->StartRecordingMicrophone(recName));
  SLEEP(3000);
  TEST_MUSTPASS(voe_file_->StopRecordingMicrophone());
  TEST_LOG("Play out the recorded file...\n");
  TEST_MUSTPASS(voe_file_->StartPlayingFileLocally(0, recName));
  SLEEP(2000);
#ifndef _INSTRUMENTATION_TESTING_
  TEST_LOG("After 2 seconds we should still be playing\n");
  TEST_MUSTPASS(!voe_file_->IsPlayingFileLocally(0));
#endif
  TEST_LOG("Set scaling\n");
  TEST_MUSTPASS(voe_file_->ScaleLocalFilePlayout(0,(float)0.11));
  SLEEP(1100);
  TEST_LOG("After 3.1 seconds we should NOT be playing\n");
  TEST_MUSTPASS(voe_file_->IsPlayingFileLocally(0));

  CodecInst codec;
  TEST_LOG("Record speaker for 3 seconds to wav file\n");
  memset(&codec, 0, sizeof(CodecInst));
  strcpy(codec.plname, "pcmu");
  codec.plfreq = 8000;
  codec.channels = 1;
  codec.pacsize = 160;
  codec.pltype = 0;
  codec.rate = 64000;
  TEST_MUSTPASS(voe_file_->StartRecordingPlayout(0,recName,&codec));
  SLEEP(3000);
  TEST_MUSTPASS(voe_file_->StopRecordingPlayout(0));

  TEST_LOG("Play file as mic, looping for 3 seconds\n");
  TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(0,
          recName,
          1,
          0,
          kFileFormatWavFile));
  SLEEP(3000);
  TEST_LOG("After 3 seconds we should still be playing\n");
  TEST_MUSTPASS(!voe_file_->IsPlayingFileAsMicrophone(0));
  SLEEP(600);
  TEST_LOG("After 3.6 seconds we should still be playing\n");
  TEST_MUSTPASS(!voe_file_->IsPlayingFileAsMicrophone(0));

  TEST_LOG("Set scaling\n");
  TEST_MUSTPASS(voe_file_->ScaleFileAsMicrophonePlayout(0,(float)0.11));
  SLEEP(200);

  TEST_LOG("Stop playing file as microphone\n");
  TEST_MUSTPASS(voe_file_->StopPlayingFileAsMicrophone(0));

  TEST_LOG("==> Start playing a file as microphone again \n");
  TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(0, AudioFilename(),
          true, true));
#else
  TEST_LOG("\n\n+++ File tests NOT ENABLED +++\n");
#endif  // #ifdef _TEST_FILE_
#ifdef _XTENDED_TEST_FILE_
  // Create unique trace files for this test
  TEST_MUSTPASS(voe_base_->SetTraceFileName(GetFilename("VoEFile_trace.txt")));
  TEST_MUSTPASS(voe_base_->SetDebugTraceFileName(GetFilename(
              "VoEFile_trace_debug.txt")));
  // turn off default AGC during these tests
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(false));
  int res = xtend.TestFile(voe_file_);
#ifndef MAC_IPHONE
  TEST_MUSTPASS(voe_apm_->SetAgcStatus(true)); // restore AGC state
#endif
  TEST_MUSTPASS(voe_base_->Terminate());
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
  char filterIp[64] = { 0 };

  SLEEP(200); // Make sure we have received packets

  TEST_MUSTPASS(voe_network_->GetSourceInfo(0,
          sourceRtpPort,
          sourceRtcpPort,
          sourceIp));

  TEST_LOG("sourceIp = %s, sourceRtpPort = %d, sourceRtcpPort = %d\n",
           sourceIp, sourceRtpPort, sourceRtcpPort);
  TEST_MUSTPASS(8000 != sourceRtpPort);
  TEST_MUSTPASS(8001 != sourceRtcpPort);

  TEST_MUSTPASS(voe_network_->GetSourceFilter(0,
          filterPort,
          filterPortRTCP,
          filterIp));
  TEST_MUSTPASS(0 != filterPort);
  TEST_MUSTPASS(0 != filterPortRTCP);
  TEST_MUSTPASS(_stricmp(filterIp, ""));

  TEST_LOG("Set filter port to %d => should hear audio\n", sourceRtpPort);
  TEST_MUSTPASS(voe_network_->SetSourceFilter(0,
          sourceRtpPort,
          sourceRtcpPort,
          "0.0.0.0"));
  TEST_MUSTPASS(voe_network_->GetSourceFilter(0,
          filterPort,
          filterPortRTCP,
          filterIp));
  TEST_MUSTPASS(sourceRtpPort != filterPort);
  TEST_MUSTPASS(sourceRtcpPort != filterPortRTCP);
  TEST_MUSTPASS(_stricmp(filterIp, "0.0.0.0"));
  SLEEP(1000);
  TEST_LOG("Set filter port to %d => should *not* hear audio\n",
           sourceRtpPort + 10);
  TEST_MUSTPASS(voe_network_->SetSourceFilter(0, sourceRtpPort+10));
  TEST_MUSTPASS(voe_network_->GetSourceFilter(0,
          filterPort,
          filterPortRTCP,
          filterIp));
  TEST_MUSTPASS(sourceRtpPort+10 != filterPort);
  SLEEP(1000);
  TEST_LOG("Disable port filter => should hear audio again\n");
  TEST_MUSTPASS(voe_network_->SetSourceFilter(0, 0));
  SLEEP(1000);

  if (voe_rtp_rtcp_) {
    TEST_MUSTPASS(voe_rtp_rtcp_->SetRTCP_CNAME(0, "Tomas"));
  }

  TEST_LOG("Set filter IP to %s => should hear audio\n", sourceIp);
  TEST_MUSTPASS(voe_network_->SetSourceFilter(0, 0, sourceRtcpPort+10,
                                              sourceIp));
  TEST_MUSTPASS(voe_network_->GetSourceFilter(0,
          filterPort,
          filterPortRTCP,
          filterIp));
  TEST_MUSTPASS(_stricmp(filterIp, sourceIp));
  SLEEP(1000);
  TEST_LOG("Set filter IP to 10.10.10.10 => should *not* hear audio\n");
  TEST_MUSTPASS(voe_network_->SetSourceFilter(0, 0, sourceRtcpPort+10,
          "10.10.10.10"));
  TEST_MUSTPASS(voe_network_->GetSourceFilter(0, filterPort, filterPort,
                                              filterIp));
  TEST_MUSTPASS(_stricmp(filterIp, "10.10.10.10"));
  SLEEP(1000);
  TEST_LOG("Disable IP filter => should hear audio again\n");
  TEST_MUSTPASS(voe_network_->SetSourceFilter(0, 0, sourceRtcpPort+10,
                                              "0.0.0.0"));
  SLEEP(1000);
  TEST_LOG("Set filter IP to 10.10.10.10 => should *not* hear audio\n");
  TEST_MUSTPASS(voe_network_->SetSourceFilter(0, 0, sourceRtcpPort+10,
          "10.10.10.10"));
  SLEEP(1000);

  if (voe_rtp_rtcp_) {
    char tmpStr[64];
    SLEEP(2000);
    TEST_LOG("Checking RTCP port filter with CNAME...\n");
    TEST_MUSTPASS(voe_rtp_rtcp_->GetRemoteRTCP_CNAME(0, tmpStr));
    TEST_MUSTPASS(!_stricmp("Tomas", tmpStr));
    TEST_MUSTPASS(voe_rtp_rtcp_->SetRTCP_CNAME(0, "Niklas"));
  } else {
    TEST_LOG("Skipping RTCP port filter test since there is no RTP/RTCP "
      "interface!\n");
  }

  TEST_LOG("Disable IP filter => should hear audio again\n");
  TEST_MUSTPASS(voe_network_->SetSourceFilter(0, 0, 0, NULL));
  TEST_MUSTPASS(voe_network_->GetSourceFilter(0, filterPort, filterPortRTCP,
          filterIp));
  TEST_MUSTPASS(_stricmp(filterIp, ""));
  SLEEP(1000);

  TEST_LOG("Wait 2 seconds for packet timeout...\n");
  TEST_LOG("You should see runtime error %d\n", VE_RECEIVE_PACKET_TIMEOUT);
  TEST_MUSTPASS(voe_base_->StopSend(0));
  TEST_MUSTPASS(voe_network_->SetPacketTimeoutNotification(0, true, 2));
  SLEEP(3000);

#if !defined(_INSTRUMENTATION_TESTING_)
  TEST_LOG("error_observer.code is %d\n", error_observer.code);
  TEST_MUSTPASS(error_observer.code != VE_RECEIVE_PACKET_TIMEOUT);
#endif
  error_observer.code = -1;
  TEST_MUSTPASS(voe_base_->StartSend(0));
  if (voe_file_) {
    TEST_LOG("Start playing a file as microphone again \n");
    TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(0,
            AudioFilename(),
            true,
            true));
  }
  TEST_LOG("You should see runtime error %d\n", VE_PACKET_RECEIPT_RESTARTED);
  SLEEP(1000);
#if !defined(_INSTRUMENTATION_TESTING_)
  TEST_MUSTPASS(error_observer.code != VE_PACKET_RECEIPT_RESTARTED);
#endif

#if !defined(_INSTRUMENTATION_TESTING_)
  TEST_LOG("Disabling observer, no runtime error should be seen...\n");
  TEST_MUSTPASS(voe_base_->DeRegisterVoiceEngineObserver());
  error_observer.code = -1;
  TEST_MUSTPASS(voe_base_->StopSend(0));
  TEST_MUSTPASS(voe_network_->SetPacketTimeoutNotification(0, true, 2));
  SLEEP(2500);
  TEST_MUSTPASS(error_observer.code != -1);
  // disable notifications to avoid additional 8082 callbacks
  TEST_MUSTPASS(voe_network_->SetPacketTimeoutNotification(0, false, 2));
  TEST_MUSTPASS(voe_base_->StartSend(0));
  if (voe_file_) {
    TEST_LOG("Start playing a file as microphone again \n");
    TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(0,
            AudioFilename(),
            true,
            true));
  }
  SLEEP(1000);
  ///    TEST_MUSTPASS(obs.code != -1);
  TEST_LOG("Enabling observer again\n");
  TEST_MUSTPASS(voe_base_->RegisterVoiceEngineObserver(error_observer));
#endif

  TEST_LOG("Enable dead-or-alive callbacks for 4 seconds (dT=1sec)...\n");
  TEST_LOG("You should see ALIVE messages\n");

  MyDeadOrAlive dead_or_alive_observer;
  TEST_MUSTPASS(voe_network_->RegisterDeadOrAliveObserver(
      0, dead_or_alive_observer));
  TEST_MUSTPASS(voe_network_->SetPeriodicDeadOrAliveStatus(0, true, 1));
  SLEEP(4000);

  // stop sending and flush dead-or-alive states
  if (voe_rtp_rtcp_) {
    TEST_MUSTPASS(voe_rtp_rtcp_->SetRTCPStatus(0, false));
  }
  TEST_MUSTPASS(voe_base_->StopSend(0));
  SLEEP(500);

  TEST_LOG("Disable sending for 4 seconds (dT=1sec)...\n");
  TEST_LOG("You should see DEAD messages (one ALIVE message might"
    " sneak in if you are unlucky)\n");
  SLEEP(4000);
  TEST_LOG("Disable dead-or-alive callbacks.\n");
  TEST_MUSTPASS(voe_network_->SetPeriodicDeadOrAliveStatus(0, false));

  TEST_LOG("Enabling external transport\n");
  TEST_MUSTPASS(voe_base_->StopReceive(0));

  // recreate the channel to ensure that we can switch from transport
  // to external transport
  TEST_MUSTPASS(voe_base_->DeleteChannel(0));
  TEST_MUSTPASS(voe_base_->CreateChannel());

  TEST_MUSTPASS(voe_network_->RegisterExternalTransport(0, channel0_transport));

  TEST_MUSTPASS(voe_base_->StartReceive(0));
  TEST_MUSTPASS(voe_base_->StartSend(0));
  TEST_MUSTPASS(voe_base_->StartPlayout(0));
  if (voe_file_) {
    TEST_LOG("Start playing a file as microphone again using"
      " external transport\n");
    TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(0,
            AudioFilename(),
            true,
            true));
  }
  SLEEP(4000);

  TEST_LOG("Disabling external transport\n");
  TEST_MUSTPASS(voe_base_->StopSend(0));
  TEST_MUSTPASS(voe_base_->StopPlayout(0));
  TEST_MUSTPASS(voe_base_->StopReceive(0));

  TEST_MUSTPASS(voe_network_->DeRegisterExternalTransport(0));

  TEST_MUSTPASS(voe_base_->SetSendDestination(0, 8000, "127.0.0.1"));
  TEST_MUSTPASS(voe_base_->SetLocalReceiver(0, 8000));

  TEST_MUSTPASS(voe_base_->StartReceive(0));
  TEST_MUSTPASS(voe_base_->StartSend(0));
  TEST_MUSTPASS(voe_base_->StartPlayout(0));
  if (voe_file_) {
    TEST_LOG("Start playing a file as microphone again using transport\n");
    TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(0,
            AudioFilename(),
            true,
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
  TEST(ResetCallReportStatistics);ANL();
  TEST_MUSTPASS(!voe_call_report_->ResetCallReportStatistics(-2));
  TEST_MUSTPASS(!voe_call_report_->ResetCallReportStatistics(1));
  TEST_MUSTPASS(voe_call_report_->ResetCallReportStatistics(0));
  TEST_MUSTPASS(voe_call_report_->ResetCallReportStatistics(-1));

  bool onOff;
  TEST_MUSTPASS(voe_apm_->GetEcMetricsStatus(onOff));
  TEST_MUSTPASS(onOff != false);
  TEST_MUSTPASS(voe_apm_->SetEcMetricsStatus(true));
  SLEEP(3000);
  EchoStatistics echo;
  TEST(GetEchoMetricSummary);ANL();
  // all outputs will be -100 in loopback (skip further tests)
  TEST_MUSTPASS(voe_call_report_->GetEchoMetricSummary(echo));

  StatVal delays;
  TEST(GetRoundTripTimeSummary);ANL();
  voe_rtp_rtcp_->SetRTCPStatus(0, false);
  // All values should be -1 since RTCP is off
  TEST_MUSTPASS(voe_call_report_->GetRoundTripTimeSummary(0, delays));
  TEST_MUSTPASS(delays.min != -1);
  TEST_MUSTPASS(delays.max != -1);
  TEST_MUSTPASS(delays.average != -1);
  voe_rtp_rtcp_->SetRTCPStatus(0, true);
  SLEEP(5000); // gives time for RTCP
  TEST_MUSTPASS(voe_call_report_->GetRoundTripTimeSummary(0, delays));
  TEST_MUSTPASS(delays.min == -1);
  TEST_MUSTPASS(delays.max == -1);
  TEST_MUSTPASS(delays.average == -1);
  voe_rtp_rtcp_->SetRTCPStatus(0, false);

  int nDead = 0;
  int nAlive = 0;
  // -1 will be returned since dead-or-alive is not active
  TEST(GetDeadOrAliveSummary);ANL();
  TEST_MUSTPASS(voe_call_report_->GetDeadOrAliveSummary(0, nDead,
                                                        nAlive) != -1);
  // we don't need these callbacks any longer
  TEST_MUSTPASS(voe_network_->DeRegisterDeadOrAliveObserver(0));
  TEST_MUSTPASS(voe_network_->SetPeriodicDeadOrAliveStatus(0, true, 1));
  SLEEP(2000);
  // All results should be >= 0 since dead-or-alive is active
  TEST_MUSTPASS(voe_call_report_->GetDeadOrAliveSummary(0, nDead, nAlive));
  TEST_MUSTPASS(nDead == -1);TEST_MUSTPASS(nAlive == -1)
  TEST_MUSTPASS(voe_network_->SetPeriodicDeadOrAliveStatus(0, false));

  TEST(WriteReportToFile);ANL();
  TEST_MUSTPASS(!voe_call_report_->WriteReportToFile(NULL));
  TEST_MUSTPASS(voe_call_report_->WriteReportToFile("call_report.txt"));
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
  TEST_MUSTPASS(voe_vsync_->GetPlayoutTimestamp(0, val));
  TEST_LOG("Playout timestamp = %lu\n", (long unsigned int) val);

  TEST_LOG("Init timestamp and sequence number manually\n");
  TEST_MUSTPASS(!voe_vsync_->SetInitTimestamp(0, 12345));
  TEST_MUSTPASS(!voe_vsync_->SetInitSequenceNumber(0, 123));
  TEST_MUSTPASS(voe_base_->StopSend(0));
  TEST_MUSTPASS(voe_vsync_->SetInitTimestamp(0, 12345));
  TEST_MUSTPASS(voe_vsync_->SetInitSequenceNumber(0, 123));
  TEST_MUSTPASS(voe_base_->StartSend(0));
  if (voe_file_) {
    TEST_LOG("Start playing a file as microphone again \n");
    TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(0,
            AudioFilename(),
            true,
            true));
  }
  SLEEP(3000);

  TEST_LOG("Check delay estimates during 15 seconds, verify that "
    "they stabilize during this time\n");
  int valInt = -1;
  for (int i = 0; i < 15; i++) {
    TEST_MUSTPASS(voe_vsync_->GetDelayEstimate(0, valInt));
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
  TEST_MUSTPASS(voe_vsync_->SetMinimumPlayoutDelay(0, 500));
  for (int i = 0; i < 15; i++) {
    TEST_MUSTPASS(voe_vsync_->GetDelayEstimate(0, valInt));
    TEST_LOG("Delay estimate = %d ms\n", valInt);
    TEST_MUSTPASS(valInt <= 45);
    SLEEP(1000);
  }

  TEST_LOG("Setting NetEQ min delay to 0 milliseconds and repeat"
    " the test above\n");
  TEST_MUSTPASS(voe_vsync_->SetMinimumPlayoutDelay(0, 0));
  for (int i = 0; i < 15; i++) {
    TEST_MUSTPASS(voe_vsync_->GetDelayEstimate(0, valInt));
    TEST_LOG("Delay estimate = %d ms\n", valInt);
    TEST_MUSTPASS(valInt <= 45);
    SLEEP(1000);
  }

#if (defined (_WIN32) || (defined(WEBRTC_LINUX)) && !defined(WEBRTC_ANDROID))
  valInt = -1;
  TEST_MUSTPASS(voe_vsync_->GetPlayoutBufferSize(valInt));
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
  TEST_MUSTPASS(voe_encrypt_->EnableSRTPSend(0,
          kCipherAes128CounterMode,
          30,
          kAuthHmacSha1,
          20, 4, kEncryptionAndAuthentication, encrKey));
  TEST_MUSTPASS(voe_encrypt_->EnableSRTPReceive(0,
          kCipherAes128CounterMode,
          30,
          kAuthHmacSha1,
          20, 4, kEncryptionAndAuthentication, encrKey));
  SLEEP(2000);

  TEST_LOG("Disabling decryption, you should hear nothing or garbage\n");
  TEST_MUSTPASS(voe_encrypt_->DisableSRTPReceive(0));
  SLEEP(2000);

  TEST_LOG("Enable decryption again, you should hear the voice again\n");
  TEST_MUSTPASS(voe_encrypt_->EnableSRTPReceive(0,
          kCipherAes128CounterMode,
          30,
          kAuthHmacSha1,
          20, 4, kEncryptionAndAuthentication, encrKey));
  SLEEP(2000);

  TEST_LOG("Disabling encryption and enabling decryption, you should"
      " hear nothing\n");
  TEST_MUSTPASS(voe_encrypt_->DisableSRTPSend(0));
  SLEEP(2000);

  TEST_LOG("Back to normal\n");
  // both SRTP sides are now inactive
  TEST_MUSTPASS(voe_encrypt_->DisableSRTPReceive(0));
  SLEEP(2000);

  TEST_LOG("Enable SRTP and SRTCP encryption and decryption,"
      " you should still hear the voice\n");
  TEST_MUSTPASS(voe_encrypt_->EnableSRTPSend(0,
          kCipherAes128CounterMode,
          30,
          kAuthHmacSha1,
          20, 4, kEncryptionAndAuthentication, encrKey, true));
  TEST_MUSTPASS(voe_encrypt_->EnableSRTPReceive(0,
          kCipherAes128CounterMode,
          30,
          kAuthHmacSha1,
          20, 4, kEncryptionAndAuthentication, encrKey, true));
  SLEEP(2000);

  TEST_LOG("Back to normal\n");
  TEST_MUSTPASS(voe_encrypt_->DisableSRTPSend(0));
  // both SRTP sides are now inactive
  TEST_MUSTPASS(voe_encrypt_->DisableSRTPReceive(0));
  SLEEP(2000);

#else
  TEST_LOG("Skipping SRTP tests - WEBRTC_SRTP not defined \n");
#endif // #ifdef WEBRTC_SRTP
  TEST_LOG("\nExternal encryption tests:\n");
  my_encryption * encObj = new my_encryption;
  TEST_MUSTPASS(voe_encrypt_->RegisterExternalEncryption(0, *encObj));
  TEST_LOG("Encryption enabled but you should still hear the voice\n");
  SLEEP(2000);
  TEST_LOG("Removing encryption object and deleting it\n");
  TEST_MUSTPASS(voe_encrypt_->DeRegisterExternalEncryption(0));
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
  TEST_MUSTPASS(voe_file_->StopPlayingFileAsMicrophone(0));

  TEST_LOG("Enabling external playout\n");
  TEST_MUSTPASS(voe_base_->StopSend(0));
  TEST_MUSTPASS(voe_base_->StopPlayout(0));
  TEST_MUSTPASS(voe_xmedia_->SetExternalPlayoutStatus(true));
  TEST_MUSTPASS(voe_base_->StartPlayout(0));
  TEST_MUSTPASS(voe_base_->StartSend(0));

  TEST_LOG("Writing 2 secs of play data to vector\n");
  int getLen;
  WebRtc_Word16 speechData[32000];
  for (int i = 0; i < 200; i++) {
    TEST_MUSTPASS(voe_xmedia_->ExternalPlayoutGetData(speechData+i*160,
            16000,
            100,
            getLen));
    TEST_MUSTPASS(160 != getLen);
    SLEEP(10);
  }

  TEST_LOG("Disabling external playout\n");
  TEST_MUSTPASS(voe_base_->StopSend(0));
  TEST_MUSTPASS(voe_base_->StopPlayout(0));
  TEST_MUSTPASS(voe_xmedia_->SetExternalPlayoutStatus(false));
  TEST_MUSTPASS(voe_base_->StartPlayout(0));

  TEST_LOG("Enabling external recording\n");
  TEST_MUSTPASS(voe_xmedia_->SetExternalRecordingStatus(true));
  TEST_MUSTPASS(voe_base_->StartSend(0));

  TEST_LOG("Inserting record data from vector\n");
  for (int i = 0; i < 200; i++) {
    TEST_MUSTPASS(voe_xmedia_->ExternalRecordingInsertData(speechData+i*160,
            160,
            16000,
            20));
    SLEEP(10);
  }

  TEST_LOG("Disabling external recording\n");
  TEST_MUSTPASS(voe_base_->StopSend(0));
  TEST_MUSTPASS(voe_xmedia_->SetExternalRecordingStatus(false));
  TEST_MUSTPASS(voe_base_->StartSend(0));

  TEST_LOG("==> Start playing a file as microphone again \n");
  TEST_MUSTPASS(voe_file_->StartPlayingFileAsMicrophone(0, AudioFilename(),
          true, true));
#else
  TEST_LOG("Skipping external rec and playout tests - \
             WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT not defined \n");
#endif // WEBRTC_VOE_EXTERNAL_REC_AND_PLAYOUT
  TEST_LOG("Enabling playout external media processing => "
    "played audio should now be affected \n");
  TEST_MUSTPASS(voe_xmedia_->RegisterExternalMediaProcessing(
          -1, kPlaybackAllChannelsMixed, mobj));
  SLEEP(2000);
  TEST_LOG("Back to normal again \n");
  TEST_MUSTPASS(voe_xmedia_->DeRegisterExternalMediaProcessing(
          -1, kPlaybackAllChannelsMixed));
  SLEEP(2000);
  // Note that we must do per channel here because PlayFileAsMicrophone
  // is only done on ch 0.
  TEST_LOG("Enabling recording external media processing => "
    "played audio should now be affected \n");
  TEST_MUSTPASS(voe_xmedia_->RegisterExternalMediaProcessing(
          0, kRecordingPerChannel, mobj));
  SLEEP(2000);
  TEST_LOG("Back to normal again \n");
  TEST_MUSTPASS(voe_xmedia_->DeRegisterExternalMediaProcessing(
          0, kRecordingPerChannel));
  SLEEP(2000);
  TEST_LOG("Enabling recording external media processing => "
    "speak and make sure that voice is affected \n");
  TEST_MUSTPASS(voe_xmedia_->RegisterExternalMediaProcessing(
          -1, kRecordingAllChannelsMixed, mobj));
  SLEEP(2000);
  TEST_LOG("Back to normal again \n");
  TEST_MUSTPASS(voe_xmedia_->DeRegisterExternalMediaProcessing(
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
  TEST_MUSTPASS(voe_neteq_stats_->GetNetworkStatistics(0, nStats));
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
  TEST_LOG("    jitterPeaksFound          = %i \n",
           nStats.jitterPeaksFound);
  TEST_LOG("    clockDriftPPM             = %i \n",
           nStats.clockDriftPPM);
  TEST_LOG("    meanWaitingTimeMs         = %i \n",
           nStats.meanWaitingTimeMs);
  TEST_LOG("    medianWaitingTimeMs       = %i \n",
           nStats.medianWaitingTimeMs);
  TEST_LOG("    minWaitingTimeMs          = %i \n",
           nStats.minWaitingTimeMs);
  TEST_LOG("    maxWaitingTimeMs          = %i \n",
           nStats.maxWaitingTimeMs);
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
  TEST_MUSTPASS(voe_base_->StopPlayout(0));
  TEST_MUSTPASS(voe_base_->StopSend(0));
  TEST_MUSTPASS(voe_base_->StopReceive(0));

  // Exit:
  TEST_LOG("Delete channel and terminate VE \n");
  TEST_MUSTPASS(voe_base_->DeleteChannel(0));
  TEST_MUSTPASS(voe_base_->Terminate());

  return 0;
}

int runAutoTest(TestType testType, ExtendedSelection extendedSel) {
  SubAPIManager apiMgr;
  apiMgr.DisplayStatus();

  ////////////////////////////////////
  // Create VoiceEngine and sub API:s

  voetest::VoETestManager tm;
  if (!tm.Init()) {
    return -1;
  }
  tm.GetInterfaces();

  //////////////////////
  // Run standard tests

  int mainRet(-1);
  if (testType == Standard) {
    mainRet = tm.DoStandardTest();

    ////////////////////////////////
    // Create configuration summary
    TEST_LOG("\n\n+++ Creating configuration summary file +++\n");
    createSummary(tm.VoiceEnginePtr());
  } else if (testType == Extended) {
    VoEExtendedTest xtend(tm);

    mainRet = 0;
    while (extendedSel != XSEL_None) {
      if (extendedSel == XSEL_Base || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestBase()) == -1)
          break;
        xtend.TestPassed("Base");
      }
      if (extendedSel == XSEL_CallReport || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestCallReport()) == -1)
          break;
        xtend.TestPassed("CallReport");
      }
      if (extendedSel == XSEL_Codec || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestCodec()) == -1)
          break;
        xtend.TestPassed("Codec");
      }
      if (extendedSel == XSEL_DTMF || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestDtmf()) == -1)
          break;
        xtend.TestPassed("Dtmf");
      }
      if (extendedSel == XSEL_Encryption || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestEncryption()) == -1)
          break;
        xtend.TestPassed("Encryption");
      }
      if (extendedSel == XSEL_ExternalMedia || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestExternalMedia()) == -1)
          break;
        xtend.TestPassed("ExternalMedia");
      }
      if (extendedSel == XSEL_File || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestFile()) == -1)
          break;
        xtend.TestPassed("File");
      }
      if (extendedSel == XSEL_Mixing || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestMixing()) == -1)
          break;
        xtend.TestPassed("Mixing");
      }
      if (extendedSel == XSEL_Hardware || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestHardware()) == -1)
          break;
        xtend.TestPassed("Hardware");
      }
      if (extendedSel == XSEL_NetEqStats || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestNetEqStats()) == -1)
          break;
        xtend.TestPassed("NetEqStats");
      }
      if (extendedSel == XSEL_Network || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestNetwork()) == -1)
          break;
        xtend.TestPassed("Network");
      }
      if (extendedSel == XSEL_RTP_RTCP || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestRTP_RTCP()) == -1)
          break;
        xtend.TestPassed("RTP_RTCP");
      }
      if (extendedSel == XSEL_VideoSync || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestVideoSync()) == -1)
          break;
        xtend.TestPassed("VideoSync");
      }
      if (extendedSel == XSEL_VolumeControl || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestVolumeControl()) == -1)
          break;
        xtend.TestPassed("VolumeControl");
      }
      if (extendedSel == XSEL_AudioProcessing || extendedSel == XSEL_All) {
        if ((mainRet = xtend.TestAPM()) == -1)
          break;
        xtend.TestPassed("AudioProcessing");
      }
      apiMgr.GetExtendedMenuSelection(extendedSel);
    } // while (extendedSel != XSEL_None)
  } else if (testType == Stress) {
    VoEStressTest stressTest(tm);
    mainRet = stressTest.DoTest();
  } else if (testType == Unit) {
    VoEUnitTest unitTest(tm);
    mainRet = unitTest.DoTest();
  } else if (testType == CPU) {
    VoECpuTest cpuTest(tm);
    mainRet = cpuTest.DoTest();
  } else {
    // Should never end up here
    TEST_LOG("INVALID SELECTION \n");
  }

  //////////////////
  // Release/Delete

  int releaseOK = tm.ReleaseInterfaces();

  if ((0 == mainRet) && (releaseOK != -1)) {
    TEST_LOG("\n\n*** All tests passed *** \n\n");
  } else {
    TEST_LOG("\n\n*** Test failed! *** \n");
  }

  return 0;
}

void createSummary(VoiceEngine* ve) {
  int len;
  char str[256];

#ifdef MAC_IPHONE
  char summaryFilename[256];
  GetDocumentsDir(summaryFilename, 256);
  strcat(summaryFilename, "/summary.txt");
#endif

  VoEBase* voe_base_ = VoEBase::GetInterface(ve);
  FILE* stream = fopen(summaryFilename, "wt");

  sprintf(str, "WebRTc VoiceEngine ");
#if defined(_WIN32)
  strcat(str, "Win");
#elif defined(WEBRTC_LINUX) && defined(WEBRTC_TARGET_PC) && \
     !defined(WEBRTC_ANDROID)
  strcat(str, "Linux");
#elif defined(WEBRTC_MAC) && !defined(MAC_IPHONE)
  strcat(str, "Mac");
#elif defined(WEBRTC_ANDROID)
  strcat(str, "Android");
#elif defined(MAC_IPHONE)
  strcat(str, "iPhone");
#endif
  // Add for other platforms as needed

  fprintf(stream, "%s\n", str);
  len = (int) strlen(str);
  for (int i = 0; i < len; i++) {
    fprintf(stream, "=");
  }
  fprintf(stream, "\n\n");

  char version[1024];
  char veVersion[24];
  voe_base_->GetVersion(version);
  // find first NL <=> end of VoiceEngine version string
  int pos = (int) strcspn(version, "\n");
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
  voe_base_->Release();
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
      VoEBase* voe_base_);
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
    VoEBase* voe_base_)
:
_stopped(false),
_base(voe_base_)
{
  //Thread Creation
  _myThread = ThreadWrapper::CreateThread(StartSend, this, kLowPriority);
  unsigned int id = 0;
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
  _myThread->SetNotAlive(); //Ensures this function is called only once.
  _base->StartSend(0);
  return true;
}

void ThreadTest::Stop()
{
  _stopped = true;
}

//  Use the following to invoke ThreatTest from the main function.
//  ThreadTest* threadtest = new ThreadTest(voe_base_);
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
  voe_base_->StartSend(0);

  //  TEST_MUSTPASS(voe_base_->StartSend(0));
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

} // namespace voetest

int RunInManualMode(int argc, char** argv) {
  using namespace voetest;

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

  switch (selection) {
    case 0:
      return 0;
    case 1:
      testType = Standard;
      break;
    case 2:
      testType = Extended;
      while (!apiMgr.GetExtendedMenuSelection(extendedSel))
        continue;
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

  if (testType == Standard) {
    TEST_LOG("\n\n+++ Running gtest-rewritten standard tests first +++\n\n");

    // Run the automated tests too in standard mode since we are gradually
    // rewriting the standard test to be automated. Running this will give
    // the standard suite the same completeness.
    RunInAutomatedMode(argc, argv);
  }

  // Function that can be called from other entry functions.
  return runAutoTest(testType, extendedSel);
}

// ----------------------------------------------------------------------------
//                                       main
// ----------------------------------------------------------------------------

#if !defined(MAC_IPHONE)
int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) == "--automated") {
    // This function is defined in automated_mode.cc to avoid macro clashes
    // with googletest (for instance the ASSERT_TRUE macro).
    return RunInAutomatedMode(argc, argv);
  }

  return RunInManualMode(argc, argv);
}
#endif //#if !defined(MAC_IPHONE)
