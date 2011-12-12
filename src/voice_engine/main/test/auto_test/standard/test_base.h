/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_H_
#define SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_H_

#include <assert.h>

#include "common_types.h"
#include "voe_audio_processing.h"
#include "voe_base.h"
#include "voe_call_report.h"
#include "voe_codec.h"
#include "voe_dtmf.h"
#include "voe_encryption.h"
#include "voe_errors.h"
#include "voe_external_media.h"
#include "voe_file.h"
#include "voe_hardware.h"
#include "voe_neteq_stats.h"
#include "voe_network.h"
#include "voe_rtp_rtcp.h"
#include "voe_test_defines.h"
#include "voe_video_sync.h"
#include "voe_volume_control.h"

// TODO(qhogpat): Remove these undefs once the clashing macros are gone.
#undef TEST
#undef ASSERT_TRUE
#undef ASSERT_FALSE
#include "gtest/gtest.h"
#include "gmock/gmock.h"

// This convenience class sets up all the VoE interfaces automatically for
// use by testing subclasses. It allocates each interface and releases it once
// which means that if a tests allocates additional interfaces from the voice
// engine and forgets to release it, this test will fail in the destructor.
class TestBase : public testing::Test {
 public:
  // The interface fetching is done in the constructor and not SetUp() since
  // this relieves our subclasses from calling SetUp in the superclass if they
  // choose to override SetUp() themselves. This is fine as googletest will
  // construct new test objects for each method.
  TestBase() {
    voice_engine_ = webrtc::VoiceEngine::Create();
    EXPECT_TRUE(voice_engine_ != NULL);

    voe_base_ = webrtc::VoEBase::GetInterface(voice_engine_);
    voe_codec_ = webrtc::VoECodec::GetInterface(voice_engine_);
    voe_volume_control_ = webrtc::VoEVolumeControl::GetInterface(voice_engine_);
    voe_dtmf_ = webrtc::VoEDtmf::GetInterface(voice_engine_);
    voe_rtp_rtcp_ = webrtc::VoERTP_RTCP::GetInterface(voice_engine_);
    voe_apm_ = webrtc::VoEAudioProcessing::GetInterface(voice_engine_);
    voe_network_ = webrtc::VoENetwork::GetInterface(voice_engine_);
    voe_file_ = webrtc::VoEFile::GetInterface(voice_engine_);
    voe_vsync_ = webrtc::VoEVideoSync::GetInterface(voice_engine_);
    voe_encrypt_ = webrtc::VoEEncryption::GetInterface(voice_engine_);
    voe_hardware_ = webrtc::VoEHardware::GetInterface(voice_engine_);
    voe_xmedia_ = webrtc::VoEExternalMedia::GetInterface(voice_engine_);
    voe_call_report_ = webrtc::VoECallReport::GetInterface(voice_engine_);
    voe_neteq_stats_ = webrtc::VoENetEqStats::GetInterface(voice_engine_);
  }

  virtual ~TestBase() {
    EXPECT_EQ(0, voe_base_->Release());
    EXPECT_EQ(0, voe_codec_->Release());
    EXPECT_EQ(0, voe_volume_control_->Release());
    EXPECT_EQ(0, voe_dtmf_->Release());
    EXPECT_EQ(0, voe_rtp_rtcp_->Release());
    EXPECT_EQ(0, voe_apm_->Release());
    EXPECT_EQ(0, voe_network_->Release());
    EXPECT_EQ(0, voe_file_->Release());
    EXPECT_EQ(0, voe_vsync_->Release());
    EXPECT_EQ(0, voe_encrypt_->Release());
    EXPECT_EQ(0, voe_hardware_->Release());
    EXPECT_EQ(0, voe_xmedia_->Release());
    EXPECT_EQ(0, voe_call_report_->Release());
    EXPECT_EQ(0, voe_neteq_stats_->Release());

    EXPECT_TRUE(webrtc::VoiceEngine::Delete(voice_engine_));
  }

 protected:
  webrtc::VoiceEngine*        voice_engine_;
  webrtc::VoEBase*            voe_base_;
  webrtc::VoECodec*           voe_codec_;
  webrtc::VoEVolumeControl*   voe_volume_control_;
  webrtc::VoEDtmf*            voe_dtmf_;
  webrtc::VoERTP_RTCP*        voe_rtp_rtcp_;
  webrtc::VoEAudioProcessing* voe_apm_;
  webrtc::VoENetwork*         voe_network_;
  webrtc::VoEFile*            voe_file_;
  webrtc::VoEVideoSync*       voe_vsync_;
  webrtc::VoEEncryption*      voe_encrypt_;
  webrtc::VoEHardware*        voe_hardware_;
  webrtc::VoEExternalMedia*   voe_xmedia_;
  webrtc::VoECallReport*      voe_call_report_;
  webrtc::VoENetEqStats*      voe_neteq_stats_;
};

#endif  // SRC_VOICE_ENGINE_MAIN_TEST_AUTO_TEST_STANDARD_TEST_BASE_H_
