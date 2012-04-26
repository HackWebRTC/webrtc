/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
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
  bool releaseOK(true);

  if (voe_base_) {
    voe_base_->Release();
    voe_base_ = NULL;
  }
  if (voe_codec_) {
    voe_codec_->Release();
    voe_codec_ = NULL;
  }
  if (voe_volume_control_) {
    voe_volume_control_->Release();
    voe_volume_control_ = NULL;
  }
  if (voe_dtmf_) {
    voe_dtmf_->Release();
    voe_dtmf_ = NULL;
  }
  if (voe_rtp_rtcp_) {
    voe_rtp_rtcp_->Release();
    voe_rtp_rtcp_ = NULL;
  }
  if (voe_apm_) {
    voe_apm_->Release();
    voe_apm_ = NULL;
  }
  if (voe_network_) {
    voe_network_->Release();
    voe_network_ = NULL;
  }
  if (voe_file_) {
    voe_file_->Release();
    voe_file_ = NULL;
  }
#ifdef _TEST_VIDEO_SYNC_
  if (voe_vsync_) {
    voe_vsync_->Release();
    voe_vsync_ = NULL;
  }
#endif
  if (voe_encrypt_) {
    voe_encrypt_->Release();
    voe_encrypt_ = NULL;
  }
  if (voe_hardware_) {
    voe_hardware_->Release();
    voe_hardware_ = NULL;
  }
#ifdef _TEST_XMEDIA_
  if (voe_xmedia_) {
    voe_xmedia_->Release();
    voe_xmedia_ = NULL;
  }
#endif
#ifdef _TEST_CALL_REPORT_
  if (voe_call_report_) {
    voe_call_report_->Release();
    voe_call_report_ = NULL;
  }
#endif
#ifdef _TEST_NETEQ_STATS_
  if (voe_neteq_stats_) {
    voe_neteq_stats_->Release();
    voe_neteq_stats_ = NULL;
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

int run_auto_test(TestType test_type, ExtendedSelection ext_selection) {
  assert(test_type != Standard);

  SubAPIManager api_manager;
  api_manager.DisplayStatus();

  ////////////////////////////////////
  // Create VoiceEngine and sub API:s

  voetest::VoETestManager test_manager;
  if (!test_manager.Init()) {
    return -1;
  }
  test_manager.GetInterfaces();

  int result(-1);
  if (test_type == Extended) {
    VoEExtendedTest xtend(test_manager);

    result = 0;
    while (ext_selection != XSEL_None) {
      if (ext_selection == XSEL_Base || ext_selection == XSEL_All) {
        if ((result = xtend.TestBase()) == -1)
          break;
        xtend.TestPassed("Base");
      }
      if (ext_selection == XSEL_CallReport || ext_selection == XSEL_All) {
        if ((result = xtend.TestCallReport()) == -1)
          break;
        xtend.TestPassed("CallReport");
      }
      if (ext_selection == XSEL_Codec || ext_selection == XSEL_All) {
        if ((result = xtend.TestCodec()) == -1)
          break;
        xtend.TestPassed("Codec");
      }
      if (ext_selection == XSEL_DTMF || ext_selection == XSEL_All) {
        if ((result = xtend.TestDtmf()) == -1)
          break;
        xtend.TestPassed("Dtmf");
      }
      if (ext_selection == XSEL_Encryption || ext_selection == XSEL_All) {
        if ((result = xtend.TestEncryption()) == -1)
          break;
        xtend.TestPassed("Encryption");
      }
      if (ext_selection == XSEL_ExternalMedia || ext_selection == XSEL_All) {
        if ((result = xtend.TestExternalMedia()) == -1)
          break;
        xtend.TestPassed("ExternalMedia");
      }
      if (ext_selection == XSEL_File || ext_selection == XSEL_All) {
        if ((result = xtend.TestFile()) == -1)
          break;
        xtend.TestPassed("File");
      }
      if (ext_selection == XSEL_Mixing || ext_selection == XSEL_All) {
        if ((result = xtend.TestMixing()) == -1)
          break;
        xtend.TestPassed("Mixing");
      }
      if (ext_selection == XSEL_Hardware || ext_selection == XSEL_All) {
        if ((result = xtend.TestHardware()) == -1)
          break;
        xtend.TestPassed("Hardware");
      }
      if (ext_selection == XSEL_NetEqStats || ext_selection == XSEL_All) {
        if ((result = xtend.TestNetEqStats()) == -1)
          break;
        xtend.TestPassed("NetEqStats");
      }
      if (ext_selection == XSEL_Network || ext_selection == XSEL_All) {
        if ((result = xtend.TestNetwork()) == -1)
          break;
        xtend.TestPassed("Network");
      }
      if (ext_selection == XSEL_RTP_RTCP || ext_selection == XSEL_All) {
        if ((result = xtend.TestRTP_RTCP()) == -1)
          break;
        xtend.TestPassed("RTP_RTCP");
      }
      if (ext_selection == XSEL_VideoSync || ext_selection == XSEL_All) {
        if ((result = xtend.TestVideoSync()) == -1)
          break;
        xtend.TestPassed("VideoSync");
      }
      if (ext_selection == XSEL_VolumeControl || ext_selection == XSEL_All) {
        if ((result = xtend.TestVolumeControl()) == -1)
          break;
        xtend.TestPassed("VolumeControl");
      }
      if (ext_selection == XSEL_AudioProcessing || ext_selection == XSEL_All) {
        if ((result = xtend.TestAPM()) == -1)
          break;
        xtend.TestPassed("AudioProcessing");
      }
      api_manager.GetExtendedMenuSelection(ext_selection);
    } // while (extendedSel != XSEL_None)
  } else if (test_type == Stress) {
    VoEStressTest stressTest(test_manager);
    result = stressTest.DoTest();
  } else if (test_type == Unit) {
    VoEUnitTest unitTest(test_manager);
    result = unitTest.DoTest();
  } else if (test_type == CPU) {
    VoECpuTest cpuTest(test_manager);
    result = cpuTest.DoTest();
  } else {
    // Should never end up here
    assert(false);
  }

  //////////////////
  // Release/Delete

  int release_ok = test_manager.ReleaseInterfaces();

  if ((0 == result) && (release_ok != -1)) {
    TEST_LOG("\n\n*** All tests passed *** \n\n");
  } else {
    TEST_LOG("\n\n*** Test failed! *** \n");
  }

  return 0;
}
} // namespace voetest

int RunInManualMode(int argc, char** argv) {
  using namespace voetest;

  SubAPIManager api_manager;
  api_manager.DisplayStatus();

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

  ExtendedSelection ext_selection = XSEL_Invalid;
  TestType test_type = Invalid;

  switch (selection) {
    case 0:
      return 0;
    case 1:
      test_type = Standard;
      break;
    case 2:
      test_type = Extended;
      while (!api_manager.GetExtendedMenuSelection(ext_selection))
        continue;
      break;
    case 3:
      test_type = Stress;
      break;
    case 4:
      test_type = Unit;
      break;
    case 5:
      test_type = CPU;
      break;
    default:
      TEST_LOG("Invalid selection!\n");
      return 0;
  }

  if (test_type == Standard) {
    TEST_LOG("\n\n+++ Running standard tests +++\n\n");

    // Currently, all googletest-rewritten tests are in the "automated" suite.
    return RunInAutomatedMode(argc, argv);
  }

  // Function that can be called from other entry functions.
  return run_auto_test(test_type, ext_selection);
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
