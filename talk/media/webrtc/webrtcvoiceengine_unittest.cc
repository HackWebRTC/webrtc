/*
 * libjingle
 * Copyright 2008 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "webrtc/base/byteorder.h"
#include "webrtc/base/gunit.h"
#include "webrtc/call.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/base/fakenetworkinterface.h"
#include "talk/media/base/fakertp.h"
#include "talk/media/webrtc/fakewebrtccall.h"
#include "talk/media/webrtc/fakewebrtcvoiceengine.h"
#include "talk/media/webrtc/webrtcvoiceengine.h"
#include "webrtc/p2p/base/faketransportcontroller.h"
#include "talk/session/media/channel.h"

using cricket::kRtpAudioLevelHeaderExtension;
using cricket::kRtpAbsoluteSenderTimeHeaderExtension;

namespace {

const cricket::AudioCodec kPcmuCodec(0, "PCMU", 8000, 64000, 1, 0);
const cricket::AudioCodec kIsacCodec(103, "ISAC", 16000, 32000, 1, 0);
const cricket::AudioCodec kOpusCodec(111, "opus", 48000, 64000, 2, 0);
const cricket::AudioCodec kG722CodecVoE(9, "G722", 16000, 64000, 1, 0);
const cricket::AudioCodec kG722CodecSdp(9, "G722", 8000, 64000, 1, 0);
const cricket::AudioCodec kRedCodec(117, "red", 8000, 0, 1, 0);
const cricket::AudioCodec kCn8000Codec(13, "CN", 8000, 0, 1, 0);
const cricket::AudioCodec kCn16000Codec(105, "CN", 16000, 0, 1, 0);
const cricket::AudioCodec kTelephoneEventCodec(106, "telephone-event", 8000, 0,
                                               1, 0);
const cricket::AudioCodec* const kAudioCodecs[] = {
    &kPcmuCodec, &kIsacCodec, &kOpusCodec, &kG722CodecVoE, &kRedCodec,
    &kCn8000Codec, &kCn16000Codec, &kTelephoneEventCodec,
};
const uint32_t kSsrc1 = 0x99;
const uint32_t kSsrc2 = 0x98;
const uint32_t kSsrcs4[] = { 1, 2, 3, 4 };

class FakeVoEWrapper : public cricket::VoEWrapper {
 public:
  explicit FakeVoEWrapper(cricket::FakeWebRtcVoiceEngine* engine)
      : cricket::VoEWrapper(engine,  // processing
                            engine,  // base
                            engine,  // codec
                            engine,  // dtmf
                            engine,  // hw
                            engine,  // network
                            engine,  // rtp
                            engine) {  // volume
  }
};

class FakeVoETraceWrapper : public cricket::VoETraceWrapper {
 public:
  int SetTraceFilter(const unsigned int filter) override {
    filter_ = filter;
    return 0;
  }
  int SetTraceFile(const char* fileNameUTF8) override { return 0; }
  int SetTraceCallback(webrtc::TraceCallback* callback) override { return 0; }
  unsigned int filter_;
};
}  // namespace

class WebRtcVoiceEngineTestFake : public testing::Test {
 public:
  WebRtcVoiceEngineTestFake()
      : call_(webrtc::Call::Config()),
        voe_(kAudioCodecs, ARRAY_SIZE(kAudioCodecs)),
        trace_wrapper_(new FakeVoETraceWrapper()),
        engine_(new FakeVoEWrapper(&voe_), trace_wrapper_),
        channel_(nullptr) {
    send_parameters_.codecs.push_back(kPcmuCodec);
    recv_parameters_.codecs.push_back(kPcmuCodec);
    options_adjust_agc_.adjust_agc_delta = rtc::Maybe<int>(-10);
  }
  bool SetupEngine() {
    if (!engine_.Init(rtc::Thread::Current())) {
      return false;
    }
    channel_ = engine_.CreateChannel(&call_, cricket::AudioOptions());
    return (channel_ != nullptr);
  }
  bool SetupEngineWithRecvStream() {
    if (!SetupEngine()) {
      return false;
    }
    return channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kSsrc1));
  }
  bool SetupEngineWithSendStream() {
    if (!SetupEngine()) {
      return false;
    }
    return channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrc1));
  }
  void SetupForMultiSendStream() {
    EXPECT_TRUE(SetupEngineWithSendStream());
    // Remove stream added in Setup.
    int default_channel_num = voe_.GetLastChannel();
    EXPECT_EQ(kSsrc1, voe_.GetLocalSSRC(default_channel_num));
    EXPECT_TRUE(channel_->RemoveSendStream(kSsrc1));

    // Verify the channel does not exist.
    EXPECT_EQ(-1, voe_.GetChannelFromLocalSsrc(kSsrc1));
  }
  void DeliverPacket(const void* data, int len) {
    rtc::Buffer packet(reinterpret_cast<const uint8_t*>(data), len);
    channel_->OnPacketReceived(&packet, rtc::PacketTime());
  }
  void TearDown() override {
    delete channel_;
    engine_.Terminate();
  }

  void TestInsertDtmf(uint32_t ssrc, bool caller) {
    EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
    channel_ = engine_.CreateChannel(&call_, cricket::AudioOptions());
    EXPECT_TRUE(channel_ != nullptr);
    if (caller) {
      // If this is a caller, local description will be applied and add the
      // send stream.
      EXPECT_TRUE(channel_->AddSendStream(
          cricket::StreamParams::CreateLegacy(kSsrc1)));
    }

    // Test we can only InsertDtmf when the other side supports telephone-event.
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
    EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
    EXPECT_FALSE(channel_->CanInsertDtmf());
    EXPECT_FALSE(channel_->InsertDtmf(ssrc, 1, 111, cricket::DF_SEND));
    send_parameters_.codecs.push_back(kTelephoneEventCodec);
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
    EXPECT_TRUE(channel_->CanInsertDtmf());

    if (!caller) {
      // If this is callee, there's no active send channel yet.
      EXPECT_FALSE(channel_->InsertDtmf(ssrc, 2, 123, cricket::DF_SEND));
      EXPECT_TRUE(channel_->AddSendStream(
          cricket::StreamParams::CreateLegacy(kSsrc1)));
    }

    // Check we fail if the ssrc is invalid.
    EXPECT_FALSE(channel_->InsertDtmf(-1, 1, 111, cricket::DF_SEND));

    // Test send
    int channel_id = voe_.GetLastChannel();
    EXPECT_FALSE(voe_.WasSendTelephoneEventCalled(channel_id, 2, 123));
    EXPECT_TRUE(channel_->InsertDtmf(ssrc, 2, 123, cricket::DF_SEND));
    EXPECT_TRUE(voe_.WasSendTelephoneEventCalled(channel_id, 2, 123));

    // Test play
    EXPECT_FALSE(voe_.WasPlayDtmfToneCalled(3, 134));
    EXPECT_TRUE(channel_->InsertDtmf(ssrc, 3, 134, cricket::DF_PLAY));
    EXPECT_TRUE(voe_.WasPlayDtmfToneCalled(3, 134));

    // Test send and play
    EXPECT_FALSE(voe_.WasSendTelephoneEventCalled(channel_id, 4, 145));
    EXPECT_FALSE(voe_.WasPlayDtmfToneCalled(4, 145));
    EXPECT_TRUE(channel_->InsertDtmf(ssrc, 4, 145,
                                     cricket::DF_PLAY | cricket::DF_SEND));
    EXPECT_TRUE(voe_.WasSendTelephoneEventCalled(channel_id, 4, 145));
    EXPECT_TRUE(voe_.WasPlayDtmfToneCalled(4, 145));
  }

  // Test that send bandwidth is set correctly.
  // |codec| is the codec under test.
  // |max_bitrate| is a parameter to set to SetMaxSendBandwidth().
  // |expected_result| is the expected result from SetMaxSendBandwidth().
  // |expected_bitrate| is the expected audio bitrate afterward.
  void TestSendBandwidth(const cricket::AudioCodec& codec,
                         int max_bitrate,
                         bool expected_result,
                         int expected_bitrate) {
    cricket::AudioSendParameters parameters;
    parameters.codecs.push_back(codec);
    parameters.max_bandwidth_bps = max_bitrate;
    EXPECT_EQ(expected_result, channel_->SetSendParameters(parameters));

    int channel_num = voe_.GetLastChannel();
    webrtc::CodecInst temp_codec;
    EXPECT_FALSE(voe_.GetSendCodec(channel_num, temp_codec));
    EXPECT_EQ(expected_bitrate, temp_codec.rate);
  }

  void TestSetSendRtpHeaderExtensions(const std::string& ext) {
    EXPECT_TRUE(SetupEngineWithSendStream());
    int channel_num = voe_.GetLastChannel();

    // Ensure extensions are off by default.
    EXPECT_EQ(-1, voe_.GetSendRtpExtensionId(channel_num, ext));

    // Ensure unknown extensions won't cause an error.
    send_parameters_.extensions.push_back(cricket::RtpHeaderExtension(
        "urn:ietf:params:unknownextention", 1));
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
    EXPECT_EQ(-1, voe_.GetSendRtpExtensionId(channel_num, ext));

    // Ensure extensions stay off with an empty list of headers.
    send_parameters_.extensions.clear();
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
    EXPECT_EQ(-1, voe_.GetSendRtpExtensionId(channel_num, ext));

    // Ensure extension is set properly.
    const int id = 1;
    send_parameters_.extensions.push_back(cricket::RtpHeaderExtension(ext, id));
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
    EXPECT_EQ(id, voe_.GetSendRtpExtensionId(channel_num, ext));

    // Ensure extension is set properly on new channels.
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrc2)));
    int new_channel_num = voe_.GetLastChannel();
    EXPECT_NE(channel_num, new_channel_num);
    EXPECT_EQ(id, voe_.GetSendRtpExtensionId(new_channel_num, ext));

    // Ensure all extensions go back off with an empty list.
    send_parameters_.codecs.push_back(kPcmuCodec);
    send_parameters_.extensions.clear();
    EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
    EXPECT_EQ(-1, voe_.GetSendRtpExtensionId(channel_num, ext));
    EXPECT_EQ(-1, voe_.GetSendRtpExtensionId(new_channel_num, ext));
  }

  void TestSetRecvRtpHeaderExtensions(const std::string& ext) {
    EXPECT_TRUE(SetupEngineWithRecvStream());
    int channel_num = voe_.GetLastChannel();

    // Ensure extensions are off by default.
    EXPECT_EQ(-1, voe_.GetReceiveRtpExtensionId(channel_num, ext));

    cricket::AudioRecvParameters parameters;
    // Ensure unknown extensions won't cause an error.
    parameters.extensions.push_back(cricket::RtpHeaderExtension(
        "urn:ietf:params:unknownextention", 1));
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));
    EXPECT_EQ(-1, voe_.GetReceiveRtpExtensionId(channel_num, ext));

    // Ensure extensions stay off with an empty list of headers.
    parameters.extensions.clear();
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));
    EXPECT_EQ(-1, voe_.GetReceiveRtpExtensionId(channel_num, ext));

    // Ensure extension is set properly.
    const int id = 2;
    parameters.extensions.push_back(cricket::RtpHeaderExtension(ext, id));
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));
    EXPECT_EQ(id, voe_.GetReceiveRtpExtensionId(channel_num, ext));

    // Ensure extension is set properly on new channel.
    // The first stream to occupy the default channel.
    EXPECT_TRUE(channel_->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kSsrc2)));
    int new_channel_num = voe_.GetLastChannel();
    EXPECT_NE(channel_num, new_channel_num);
    EXPECT_EQ(id, voe_.GetReceiveRtpExtensionId(new_channel_num, ext));

    // Ensure all extensions go back off with an empty list.
    parameters.extensions.clear();
    EXPECT_TRUE(channel_->SetRecvParameters(parameters));
    EXPECT_EQ(-1, voe_.GetReceiveRtpExtensionId(channel_num, ext));
    EXPECT_EQ(-1, voe_.GetReceiveRtpExtensionId(new_channel_num, ext));
  }

  webrtc::AudioSendStream::Stats GetAudioSendStreamStats() const {
    webrtc::AudioSendStream::Stats stats;
    stats.local_ssrc = 12;
    stats.bytes_sent = 345;
    stats.packets_sent = 678;
    stats.packets_lost = 9012;
    stats.fraction_lost = 34.56f;
    stats.codec_name = "codec_name_send";
    stats.ext_seqnum = 789;
    stats.jitter_ms = 12;
    stats.rtt_ms = 345;
    stats.audio_level = 678;
    stats.aec_quality_min = 9.01f;
    stats.echo_delay_median_ms = 234;
    stats.echo_delay_std_ms = 567;
    stats.echo_return_loss = 890;
    stats.echo_return_loss_enhancement = 1234;
    stats.typing_noise_detected = true;
    return stats;
  }
  void SetAudioSendStreamStats() {
    for (auto* s : call_.GetAudioSendStreams()) {
      s->SetStats(GetAudioSendStreamStats());
    }
  }
  void VerifyVoiceSenderInfo(const cricket::VoiceSenderInfo& info,
                             bool is_sending) {
    const auto stats = GetAudioSendStreamStats();
    EXPECT_EQ(info.ssrc(), stats.local_ssrc);
    EXPECT_EQ(info.bytes_sent, stats.bytes_sent);
    EXPECT_EQ(info.packets_sent, stats.packets_sent);
    EXPECT_EQ(info.packets_lost, stats.packets_lost);
    EXPECT_EQ(info.fraction_lost, stats.fraction_lost);
    EXPECT_EQ(info.codec_name, stats.codec_name);
    EXPECT_EQ(info.ext_seqnum, stats.ext_seqnum);
    EXPECT_EQ(info.jitter_ms, stats.jitter_ms);
    EXPECT_EQ(info.rtt_ms, stats.rtt_ms);
    EXPECT_EQ(info.audio_level, stats.audio_level);
    EXPECT_EQ(info.aec_quality_min, stats.aec_quality_min);
    EXPECT_EQ(info.echo_delay_median_ms, stats.echo_delay_median_ms);
    EXPECT_EQ(info.echo_delay_std_ms, stats.echo_delay_std_ms);
    EXPECT_EQ(info.echo_return_loss, stats.echo_return_loss);
    EXPECT_EQ(info.echo_return_loss_enhancement,
              stats.echo_return_loss_enhancement);
    EXPECT_EQ(info.typing_noise_detected,
              stats.typing_noise_detected && is_sending);
  }

  webrtc::AudioReceiveStream::Stats GetAudioReceiveStreamStats() const {
    webrtc::AudioReceiveStream::Stats stats;
    stats.remote_ssrc = 123;
    stats.bytes_rcvd = 456;
    stats.packets_rcvd = 768;
    stats.packets_lost = 101;
    stats.fraction_lost = 23.45f;
    stats.codec_name = "codec_name_recv";
    stats.ext_seqnum = 678;
    stats.jitter_ms = 901;
    stats.jitter_buffer_ms = 234;
    stats.jitter_buffer_preferred_ms = 567;
    stats.delay_estimate_ms = 890;
    stats.audio_level = 1234;
    stats.expand_rate = 5.67f;
    stats.speech_expand_rate = 8.90f;
    stats.secondary_decoded_rate = 1.23f;
    stats.accelerate_rate = 4.56f;
    stats.preemptive_expand_rate = 7.89f;
    stats.decoding_calls_to_silence_generator = 12;
    stats.decoding_calls_to_neteq = 345;
    stats.decoding_normal = 67890;
    stats.decoding_plc = 1234;
    stats.decoding_cng = 5678;
    stats.decoding_plc_cng = 9012;
    stats.capture_start_ntp_time_ms = 3456;
    return stats;
  }
  void SetAudioReceiveStreamStats() {
    for (auto* s : call_.GetAudioReceiveStreams()) {
      s->SetStats(GetAudioReceiveStreamStats());
    }
  }
  void VerifyVoiceReceiverInfo(const cricket::VoiceReceiverInfo& info) {
    const auto stats = GetAudioReceiveStreamStats();
    EXPECT_EQ(info.ssrc(), stats.remote_ssrc);
    EXPECT_EQ(info.bytes_rcvd, stats.bytes_rcvd);
    EXPECT_EQ(info.packets_rcvd, stats.packets_rcvd);
    EXPECT_EQ(info.packets_lost, stats.packets_lost);
    EXPECT_EQ(info.fraction_lost, stats.fraction_lost);
    EXPECT_EQ(info.codec_name, stats.codec_name);
    EXPECT_EQ(info.ext_seqnum, stats.ext_seqnum);
    EXPECT_EQ(info.jitter_ms, stats.jitter_ms);
    EXPECT_EQ(info.jitter_buffer_ms, stats.jitter_buffer_ms);
    EXPECT_EQ(info.jitter_buffer_preferred_ms,
              stats.jitter_buffer_preferred_ms);
    EXPECT_EQ(info.delay_estimate_ms, stats.delay_estimate_ms);
    EXPECT_EQ(info.audio_level, stats.audio_level);
    EXPECT_EQ(info.expand_rate, stats.expand_rate);
    EXPECT_EQ(info.speech_expand_rate, stats.speech_expand_rate);
    EXPECT_EQ(info.secondary_decoded_rate, stats.secondary_decoded_rate);
    EXPECT_EQ(info.accelerate_rate, stats.accelerate_rate);
    EXPECT_EQ(info.preemptive_expand_rate, stats.preemptive_expand_rate);
    EXPECT_EQ(info.decoding_calls_to_silence_generator,
              stats.decoding_calls_to_silence_generator);
    EXPECT_EQ(info.decoding_calls_to_neteq, stats.decoding_calls_to_neteq);
    EXPECT_EQ(info.decoding_normal, stats.decoding_normal);
    EXPECT_EQ(info.decoding_plc, stats.decoding_plc);
    EXPECT_EQ(info.decoding_cng, stats.decoding_cng);
    EXPECT_EQ(info.decoding_plc_cng, stats.decoding_plc_cng);
    EXPECT_EQ(info.capture_start_ntp_time_ms, stats.capture_start_ntp_time_ms);
  }

 protected:
  cricket::FakeCall call_;
  cricket::FakeWebRtcVoiceEngine voe_;
  FakeVoETraceWrapper* trace_wrapper_;
  cricket::WebRtcVoiceEngine engine_;
  cricket::VoiceMediaChannel* channel_;

  cricket::AudioSendParameters send_parameters_;
  cricket::AudioRecvParameters recv_parameters_;
  cricket::AudioOptions options_adjust_agc_;
};

// Tests that our stub library "works".
TEST_F(WebRtcVoiceEngineTestFake, StartupShutdown) {
  EXPECT_FALSE(voe_.IsInited());
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  EXPECT_TRUE(voe_.IsInited());
  engine_.Terminate();
  EXPECT_FALSE(voe_.IsInited());
}

// Tests that we can create and destroy a channel.
TEST_F(WebRtcVoiceEngineTestFake, CreateChannel) {
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  channel_ = engine_.CreateChannel(&call_, cricket::AudioOptions());
  EXPECT_TRUE(channel_ != nullptr);
}

// Tests that the list of supported codecs is created properly and ordered
// correctly
TEST_F(WebRtcVoiceEngineTestFake, CodecPreference) {
  const std::vector<cricket::AudioCodec>& codecs = engine_.codecs();
  ASSERT_FALSE(codecs.empty());
  EXPECT_STRCASEEQ("opus", codecs[0].name.c_str());
  EXPECT_EQ(48000, codecs[0].clockrate);
  EXPECT_EQ(2, codecs[0].channels);
  EXPECT_EQ(64000, codecs[0].bitrate);
  int pref = codecs[0].preference;
  for (size_t i = 1; i < codecs.size(); ++i) {
    EXPECT_GT(pref, codecs[i].preference);
    pref = codecs[i].preference;
  }
}

// Tests that we can find codecs by name or id, and that we interpret the
// clockrate and bitrate fields properly.
TEST_F(WebRtcVoiceEngineTestFake, FindCodec) {
  cricket::AudioCodec codec;
  webrtc::CodecInst codec_inst;
  // Find PCMU with explicit clockrate and bitrate.
  EXPECT_TRUE(engine_.FindWebRtcCodec(kPcmuCodec, &codec_inst));
  // Find ISAC with explicit clockrate and 0 bitrate.
  EXPECT_TRUE(engine_.FindWebRtcCodec(kIsacCodec, &codec_inst));
  // Find telephone-event with explicit clockrate and 0 bitrate.
  EXPECT_TRUE(engine_.FindWebRtcCodec(kTelephoneEventCodec, &codec_inst));
  // Find ISAC with a different payload id.
  codec = kIsacCodec;
  codec.id = 127;
  EXPECT_TRUE(engine_.FindWebRtcCodec(codec, &codec_inst));
  EXPECT_EQ(codec.id, codec_inst.pltype);
  // Find PCMU with a 0 clockrate.
  codec = kPcmuCodec;
  codec.clockrate = 0;
  EXPECT_TRUE(engine_.FindWebRtcCodec(codec, &codec_inst));
  EXPECT_EQ(codec.id, codec_inst.pltype);
  EXPECT_EQ(8000, codec_inst.plfreq);
  // Find PCMU with a 0 bitrate.
  codec = kPcmuCodec;
  codec.bitrate = 0;
  EXPECT_TRUE(engine_.FindWebRtcCodec(codec, &codec_inst));
  EXPECT_EQ(codec.id, codec_inst.pltype);
  EXPECT_EQ(64000, codec_inst.rate);
  // Find ISAC with an explicit bitrate.
  codec = kIsacCodec;
  codec.bitrate = 32000;
  EXPECT_TRUE(engine_.FindWebRtcCodec(codec, &codec_inst));
  EXPECT_EQ(codec.id, codec_inst.pltype);
  EXPECT_EQ(32000, codec_inst.rate);
}

// Test that we set our inbound codecs properly, including changing PT.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecs) {
  EXPECT_TRUE(SetupEngine());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs[0].id = 106;  // collide with existing telephone-event
  parameters.codecs[2].id = 126;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_num = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  rtc::strcpyn(gcodec.plname, ARRAY_SIZE(gcodec.plname), "ISAC");
  gcodec.plfreq = 16000;
  gcodec.channels = 1;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num, gcodec));
  EXPECT_EQ(106, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  rtc::strcpyn(gcodec.plname, ARRAY_SIZE(gcodec.plname),
      "telephone-event");
  gcodec.plfreq = 8000;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num, gcodec));
  EXPECT_EQ(126, gcodec.pltype);
  EXPECT_STREQ("telephone-event", gcodec.plname);
}

// Test that we fail to set an unknown inbound codec.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsUnsupportedCodec) {
  EXPECT_TRUE(SetupEngine());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(cricket::AudioCodec(127, "XYZ", 32000, 0, 1, 0));
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

// Test that we fail if we have duplicate types in the inbound list.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsDuplicatePayloadType) {
  EXPECT_TRUE(SetupEngine());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs[1].id = kIsacCodec.id;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
}

// Test that we can decode OPUS without stereo parameters.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWithOpusNoStereo) {
  EXPECT_TRUE(SetupEngine());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_num = voe_.GetLastChannel();
  webrtc::CodecInst opus;
  engine_.FindWebRtcCodec(kOpusCodec, &opus);
  // Even without stereo parameters, recv codecs still specify channels = 2.
  EXPECT_EQ(2, opus.channels);
  EXPECT_EQ(111, opus.pltype);
  EXPECT_STREQ("opus", opus.plname);
  opus.pltype = 0;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num, opus));
  EXPECT_EQ(111, opus.pltype);
}

// Test that we can decode OPUS with stereo = 0.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWithOpus0Stereo) {
  EXPECT_TRUE(SetupEngine());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[2].params["stereo"] = "0";
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst opus;
  engine_.FindWebRtcCodec(kOpusCodec, &opus);
  // Even when stereo is off, recv codecs still specify channels = 2.
  EXPECT_EQ(2, opus.channels);
  EXPECT_EQ(111, opus.pltype);
  EXPECT_STREQ("opus", opus.plname);
  opus.pltype = 0;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, opus));
  EXPECT_EQ(111, opus.pltype);
}

// Test that we can decode OPUS with stereo = 1.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWithOpus1Stereo) {
  EXPECT_TRUE(SetupEngine());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[2].params["stereo"] = "1";
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst opus;
  engine_.FindWebRtcCodec(kOpusCodec, &opus);
  EXPECT_EQ(2, opus.channels);
  EXPECT_EQ(111, opus.pltype);
  EXPECT_STREQ("opus", opus.plname);
  opus.pltype = 0;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, opus));
  EXPECT_EQ(111, opus.pltype);
}

// Test that changes to recv codecs are applied to all streams.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWithMultipleStreams) {
  EXPECT_TRUE(SetupEngine());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs[0].id = 106;  // collide with existing telephone-event
  parameters.codecs[2].id = 126;
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  rtc::strcpyn(gcodec.plname, ARRAY_SIZE(gcodec.plname), "ISAC");
  gcodec.plfreq = 16000;
  gcodec.channels = 1;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, gcodec));
  EXPECT_EQ(106, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  rtc::strcpyn(gcodec.plname, ARRAY_SIZE(gcodec.plname),
      "telephone-event");
  gcodec.plfreq = 8000;
  gcodec.channels = 1;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, gcodec));
  EXPECT_EQ(126, gcodec.pltype);
  EXPECT_STREQ("telephone-event", gcodec.plname);
}

TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsAfterAddingStreams) {
  EXPECT_TRUE(SetupEngineWithRecvStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs[0].id = 106;  // collide with existing telephone-event
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  rtc::strcpyn(gcodec.plname, ARRAY_SIZE(gcodec.plname), "ISAC");
  gcodec.plfreq = 16000;
  gcodec.channels = 1;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, gcodec));
  EXPECT_EQ(106, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
}

// Test that we can apply the same set of codecs again while playing.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWhilePlaying) {
  EXPECT_TRUE(SetupEngineWithRecvStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kCn16000Codec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(channel_->SetPlayout(true));
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));

  // Changing the payload type of a codec should fail.
  parameters.codecs[0].id = 127;
  EXPECT_FALSE(channel_->SetRecvParameters(parameters));
  int channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(voe_.GetPlayout(channel_num));
}

// Test that we can add a codec while playing.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvCodecsWhilePlaying) {
  EXPECT_TRUE(SetupEngineWithRecvStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kCn16000Codec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(channel_->SetPlayout(true));

  parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  int channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(voe_.GetPlayout(channel_num));
  webrtc::CodecInst gcodec;
  EXPECT_TRUE(engine_.FindWebRtcCodec(kOpusCodec, &gcodec));
  EXPECT_EQ(kOpusCodec.id, gcodec.pltype);
}

TEST_F(WebRtcVoiceEngineTestFake, SetSendBandwidthAuto) {
  EXPECT_TRUE(SetupEngineWithSendStream());

  // Test that when autobw is enabled, bitrate is kept as the default
  // value. autobw is enabled for the following tests because the target
  // bitrate is <= 0.

  // ISAC, default bitrate == 32000.
  TestSendBandwidth(kIsacCodec, 0, true, 32000);

  // PCMU, default bitrate == 64000.
  TestSendBandwidth(kPcmuCodec, -1, true, 64000);

  // opus, default bitrate == 64000.
  TestSendBandwidth(kOpusCodec, -1, true, 64000);
}

TEST_F(WebRtcVoiceEngineTestFake, SetMaxSendBandwidthMultiRateAsCaller) {
  EXPECT_TRUE(SetupEngineWithSendStream());

  // Test that the bitrate of a multi-rate codec is always the maximum.

  // ISAC, default bitrate == 32000.
  TestSendBandwidth(kIsacCodec, 128000, true, 128000);
  TestSendBandwidth(kIsacCodec, 16000, true, 16000);

  // opus, default bitrate == 64000.
  TestSendBandwidth(kOpusCodec, 96000, true, 96000);
  TestSendBandwidth(kOpusCodec, 48000, true, 48000);
}

TEST_F(WebRtcVoiceEngineTestFake, SetMaxSendBandwidthFixedRateAsCaller) {
  EXPECT_TRUE(SetupEngineWithSendStream());

  // Test that we can only set a maximum bitrate for a fixed-rate codec
  // if it's bigger than the fixed rate.

  // PCMU, fixed bitrate == 64000.
  TestSendBandwidth(kPcmuCodec, 0, true, 64000);
  TestSendBandwidth(kPcmuCodec, 1, false, 64000);
  TestSendBandwidth(kPcmuCodec, 128000, true, 64000);
  TestSendBandwidth(kPcmuCodec, 32000, false, 64000);
  TestSendBandwidth(kPcmuCodec, 64000, true, 64000);
  TestSendBandwidth(kPcmuCodec, 63999, false, 64000);
  TestSendBandwidth(kPcmuCodec, 64001, true, 64000);
}

TEST_F(WebRtcVoiceEngineTestFake, SetMaxSendBandwidthMultiRateAsCallee) {
  EXPECT_TRUE(SetupEngine());
  const int kDesiredBitrate = 128000;
  cricket::AudioSendParameters parameters;
  parameters.codecs = engine_.codecs();
  parameters.max_bandwidth_bps = kDesiredBitrate;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));

  int channel_num = voe_.GetLastChannel();
  webrtc::CodecInst codec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, codec));
  EXPECT_EQ(kDesiredBitrate, codec.rate);
}

// Test that bitrate cannot be set for CBR codecs.
// Bitrate is ignored if it is higher than the fixed bitrate.
// Bitrate less then the fixed bitrate is an error.
TEST_F(WebRtcVoiceEngineTestFake, SetMaxSendBandwidthCbr) {
  EXPECT_TRUE(SetupEngineWithSendStream());

  // PCMU, default bitrate == 64000.
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  int channel_num = voe_.GetLastChannel();
  webrtc::CodecInst codec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, codec));
  EXPECT_EQ(64000, codec.rate);

  send_parameters_.max_bandwidth_bps = 128000;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, codec));
  EXPECT_EQ(64000, codec.rate);

  send_parameters_.max_bandwidth_bps = 128;
  EXPECT_FALSE(channel_->SetSendParameters(send_parameters_));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, codec));
  EXPECT_EQ(64000, codec.rate);
}

// Test that we apply codecs properly.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecs) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs[0].id = 96;
  parameters.codecs[0].bitrate = 48000;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(1, voe_.GetNumSetSendCodecs());
  int channel_num = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_EQ(48000, gcodec.rate);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetVAD(channel_num));
  EXPECT_FALSE(voe_.GetRED(channel_num));
  EXPECT_EQ(13, voe_.GetSendCNPayloadType(channel_num, false));
  EXPECT_EQ(105, voe_.GetSendCNPayloadType(channel_num, true));
  EXPECT_EQ(106, voe_.GetSendTelephoneEventPayloadType(channel_num));
}

// Test that VoE Channel doesn't call SetSendCodec again if same codec is tried
// to apply.
TEST_F(WebRtcVoiceEngineTestFake, DontResetSetSendCodec) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs[0].id = 96;
  parameters.codecs[0].bitrate = 48000;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(1, voe_.GetNumSetSendCodecs());
  // Calling SetSendCodec again with same codec which is already set.
  // In this case media channel shouldn't send codec to VoE.
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(1, voe_.GetNumSetSendCodecs());
}

// Verify that G722 is set with 16000 samples per second to WebRTC.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecG722) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kG722CodecSdp);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("G722", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(16000, gcodec.plfreq);
}

// Test that if clockrate is not 48000 for opus, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBadClockrate) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].clockrate = 50000;
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that if channels=0 for opus, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBad0ChannelsNoStereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].channels = 0;
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that if channels=0 for opus, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBad0Channels1Stereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].channels = 0;
  parameters.codecs[0].params["stereo"] = "1";
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that if channel is 1 for opus and there's no stereo, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpus1ChannelNoStereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].channels = 1;
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that if channel is 1 for opus and stereo=0, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBad1Channel0Stereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].channels = 1;
  parameters.codecs[0].params["stereo"] = "0";
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that if channel is 1 for opus and stereo=1, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBad1Channel1Stereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].channels = 1;
  parameters.codecs[0].params["stereo"] = "1";
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that with bitrate=0 and no stereo,
// channels and bitrate are 1 and 32000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGood0BitrateNoStereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(32000, gcodec.rate);
}

// Test that with bitrate=0 and stereo=0,
// channels and bitrate are 1 and 32000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGood0Bitrate0Stereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].params["stereo"] = "0";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(32000, gcodec.rate);
}

// Test that with bitrate=invalid and stereo=0,
// channels and bitrate are 1 and 32000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodXBitrate0Stereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].params["stereo"] = "0";
  webrtc::CodecInst gcodec;

  // bitrate that's out of the range between 6000 and 510000 will be clamped.
  parameters.codecs[0].bitrate = 5999;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(6000, gcodec.rate);

  parameters.codecs[0].bitrate = 510001;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(510000, gcodec.rate);
}

// Test that with bitrate=0 and stereo=1,
// channels and bitrate are 2 and 64000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGood0Bitrate1Stereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].params["stereo"] = "1";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(2, gcodec.channels);
  EXPECT_EQ(64000, gcodec.rate);
}

// Test that with bitrate=invalid and stereo=1,
// channels and bitrate are 2 and 64000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodXBitrate1Stereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].params["stereo"] = "1";
  webrtc::CodecInst gcodec;

  // bitrate that's out of the range between 6000 and 510000 will be clamped.
  parameters.codecs[0].bitrate = 5999;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(2, gcodec.channels);
  EXPECT_EQ(6000, gcodec.rate);

  parameters.codecs[0].bitrate = 510001;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(2, gcodec.channels);
  EXPECT_EQ(510000, gcodec.rate);
}

// Test that with bitrate=N and stereo unset,
// channels and bitrate are 1 and N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrateNoStereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 96000;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(111, gcodec.pltype);
  EXPECT_EQ(96000, gcodec.rate);
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(48000, gcodec.plfreq);
}

// Test that with bitrate=N and stereo=0,
// channels and bitrate are 1 and N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrate0Stereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 30000;
  parameters.codecs[0].params["stereo"] = "0";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(30000, gcodec.rate);
  EXPECT_STREQ("opus", gcodec.plname);
}

// Test that with bitrate=N and without any parameters,
// channels and bitrate are 1 and N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrateNoParameters) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 30000;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(30000, gcodec.rate);
  EXPECT_STREQ("opus", gcodec.plname);
}

// Test that with bitrate=N and stereo=1,
// channels and bitrate are 2 and N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrate1Stereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 30000;
  parameters.codecs[0].params["stereo"] = "1";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(2, gcodec.channels);
  EXPECT_EQ(30000, gcodec.rate);
  EXPECT_STREQ("opus", gcodec.plname);
}

// Test that bitrate will be overridden by the "maxaveragebitrate" parameter.
// Also test that the "maxaveragebitrate" can't be set to values outside the
// range of 6000 and 510000
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusMaxAverageBitrate) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 30000;
  webrtc::CodecInst gcodec;

  // Ignore if less than 6000.
  parameters.codecs[0].params["maxaveragebitrate"] = "5999";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(6000, gcodec.rate);

  // Ignore if larger than 510000.
  parameters.codecs[0].params["maxaveragebitrate"] = "510001";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(510000, gcodec.rate);

  parameters.codecs[0].params["maxaveragebitrate"] = "200000";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(200000, gcodec.rate);
}

// Test that we can enable NACK with opus as caller.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecEnableNackAsCaller) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  EXPECT_FALSE(voe_.GetNACK(channel_num));
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(voe_.GetNACK(channel_num));
}

// Test that we can enable NACK with opus as callee.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecEnableNackAsCallee) {
  EXPECT_TRUE(SetupEngineWithRecvStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  EXPECT_FALSE(voe_.GetNACK(channel_num));
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_FALSE(voe_.GetNACK(channel_num));

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  EXPECT_TRUE(voe_.GetNACK(voe_.GetLastChannel()));
}

// Test that we can enable NACK on receive streams.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecEnableNackRecvStreams) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num1 = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int channel_num2 = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  EXPECT_FALSE(voe_.GetNACK(channel_num1));
  EXPECT_FALSE(voe_.GetNACK(channel_num2));
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(voe_.GetNACK(channel_num1));
  EXPECT_TRUE(voe_.GetNACK(channel_num2));
}

// Test that we can disable NACK.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecDisableNack) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(voe_.GetNACK(channel_num));

  parameters.codecs.clear();
  parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_FALSE(voe_.GetNACK(channel_num));
}

// Test that we can disable NACK on receive streams.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecDisableNackRecvStreams) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num1 = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int channel_num2 = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(voe_.GetNACK(channel_num1));
  EXPECT_TRUE(voe_.GetNACK(channel_num2));

  parameters.codecs.clear();
  parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_FALSE(voe_.GetNACK(channel_num1));
  EXPECT_FALSE(voe_.GetNACK(channel_num2));
}

// Test that NACK is enabled on a new receive stream.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvStreamEnableNack) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs[0].AddFeedbackParam(
      cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                             cricket::kParamValueEmpty));
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(voe_.GetNACK(channel_num));

  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(voe_.GetNACK(channel_num));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(3)));
  channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(voe_.GetNACK(channel_num));
}

// Test that without useinbandfec, Opus FEC is off.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecNoOpusFec) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_FALSE(voe_.GetCodecFEC(channel_num));
}

// Test that with useinbandfec=0, Opus FEC is off.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusDisableFec) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].params["useinbandfec"] = "0";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_FALSE(voe_.GetCodecFEC(channel_num));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(32000, gcodec.rate);
}

// Test that with useinbandfec=1, Opus FEC is on.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusEnableFec) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].params["useinbandfec"] = "1";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(voe_.GetCodecFEC(channel_num));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(32000, gcodec.rate);
}

// Test that with useinbandfec=1, stereo=1, Opus FEC is on.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusEnableFecStereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].params["stereo"] = "1";
  parameters.codecs[0].params["useinbandfec"] = "1";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(voe_.GetCodecFEC(channel_num));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(2, gcodec.channels);
  EXPECT_EQ(64000, gcodec.rate);
}

// Test that with non-Opus, codec FEC is off.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecIsacNoFec) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_FALSE(voe_.GetCodecFEC(channel_num));
}

// Test the with non-Opus, even if useinbandfec=1, FEC is off.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecIsacWithParamNoFec) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs[0].params["useinbandfec"] = "1";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_FALSE(voe_.GetCodecFEC(channel_num));
}

// Test that Opus FEC status can be changed.
TEST_F(WebRtcVoiceEngineTestFake, ChangeOpusFecStatus) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_FALSE(voe_.GetCodecFEC(channel_num));
  parameters.codecs[0].params["useinbandfec"] = "1";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(voe_.GetCodecFEC(channel_num));
}

// Test maxplaybackrate <= 8000 triggers Opus narrow band mode.
TEST_F(WebRtcVoiceEngineTestFake, SetOpusMaxPlaybackRateNb) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 8000);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(cricket::kOpusBandwidthNb,
            voe_.GetMaxEncodingBandwidth(channel_num));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);

  EXPECT_EQ(12000, gcodec.rate);
  parameters.codecs[0].SetParam(cricket::kCodecParamStereo, "1");
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(24000, gcodec.rate);
}

// Test 8000 < maxplaybackrate <= 12000 triggers Opus medium band mode.
TEST_F(WebRtcVoiceEngineTestFake, SetOpusMaxPlaybackRateMb) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 8001);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(cricket::kOpusBandwidthMb,
            voe_.GetMaxEncodingBandwidth(channel_num));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);

  EXPECT_EQ(20000, gcodec.rate);
  parameters.codecs[0].SetParam(cricket::kCodecParamStereo, "1");
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(40000, gcodec.rate);
}

// Test 12000 < maxplaybackrate <= 16000 triggers Opus wide band mode.
TEST_F(WebRtcVoiceEngineTestFake, SetOpusMaxPlaybackRateWb) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 12001);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(cricket::kOpusBandwidthWb,
            voe_.GetMaxEncodingBandwidth(channel_num));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);

  EXPECT_EQ(20000, gcodec.rate);
  parameters.codecs[0].SetParam(cricket::kCodecParamStereo, "1");
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(40000, gcodec.rate);
}

// Test 16000 < maxplaybackrate <= 24000 triggers Opus super wide band mode.
TEST_F(WebRtcVoiceEngineTestFake, SetOpusMaxPlaybackRateSwb) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 16001);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(cricket::kOpusBandwidthSwb,
            voe_.GetMaxEncodingBandwidth(channel_num));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);

  EXPECT_EQ(32000, gcodec.rate);
  parameters.codecs[0].SetParam(cricket::kCodecParamStereo, "1");
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(64000, gcodec.rate);
}

// Test 24000 < maxplaybackrate triggers Opus full band mode.
TEST_F(WebRtcVoiceEngineTestFake, SetOpusMaxPlaybackRateFb) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].bitrate = 0;
  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 24001);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(cricket::kOpusBandwidthFb,
            voe_.GetMaxEncodingBandwidth(channel_num));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);

  EXPECT_EQ(32000, gcodec.rate);
  parameters.codecs[0].SetParam(cricket::kCodecParamStereo, "1");
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(64000, gcodec.rate);
}

// Test Opus that without maxplaybackrate, default playback rate is used.
TEST_F(WebRtcVoiceEngineTestFake, DefaultOpusMaxPlaybackRate) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(cricket::kOpusBandwidthFb,
            voe_.GetMaxEncodingBandwidth(channel_num));
}

// Test the with non-Opus, maxplaybackrate has no effect.
TEST_F(WebRtcVoiceEngineTestFake, SetNonOpusMaxPlaybackRate) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 32000);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetMaxEncodingBandwidth(channel_num));
}

// Test maxplaybackrate can be set on two streams.
TEST_F(WebRtcVoiceEngineTestFake, SetOpusMaxPlaybackRateOnTwoStreams) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  // Default bandwidth is 24000.
  EXPECT_EQ(cricket::kOpusBandwidthFb,
            voe_.GetMaxEncodingBandwidth(channel_num));

  parameters.codecs[0].SetParam(cricket::kCodecParamMaxPlaybackRate, 8000);

  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(cricket::kOpusBandwidthNb,
            voe_.GetMaxEncodingBandwidth(channel_num));

  channel_->AddSendStream(cricket::StreamParams::CreateLegacy(kSsrc2));
  channel_num = voe_.GetLastChannel();
  EXPECT_EQ(cricket::kOpusBandwidthNb,
            voe_.GetMaxEncodingBandwidth(channel_num));
}

// Test that with usedtx=0, Opus DTX is off.
TEST_F(WebRtcVoiceEngineTestFake, DisableOpusDtxOnOpus) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].params["usedtx"] = "0";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_FALSE(voe_.GetOpusDtx(channel_num));
}

// Test that with usedtx=1, Opus DTX is on.
TEST_F(WebRtcVoiceEngineTestFake, EnableOpusDtxOnOpus) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].params["usedtx"] = "1";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(voe_.GetOpusDtx(channel_num));
  EXPECT_FALSE(voe_.GetVAD(channel_num));  // Opus DTX should not affect VAD.
}

// Test that usedtx=1 works with stereo Opus.
TEST_F(WebRtcVoiceEngineTestFake, EnableOpusDtxOnOpusStereo) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].params["usedtx"] = "1";
  parameters.codecs[0].params["stereo"] = "1";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(voe_.GetOpusDtx(channel_num));
  EXPECT_FALSE(voe_.GetVAD(channel_num));   // Opus DTX should not affect VAD.
}

// Test that usedtx=1 does not work with non Opus.
TEST_F(WebRtcVoiceEngineTestFake, CannotEnableOpusDtxOnNonOpus) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs[0].params["usedtx"] = "1";
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_FALSE(voe_.GetOpusDtx(channel_num));
}

// Test that we can switch back and forth between Opus and ISAC with CN.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsIsacOpusSwitching) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters opus_parameters;
  opus_parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetSendParameters(opus_parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(111, gcodec.pltype);
  EXPECT_STREQ("opus", gcodec.plname);

  cricket::AudioSendParameters isac_parameters;
  isac_parameters.codecs.push_back(kIsacCodec);
  isac_parameters.codecs.push_back(kCn16000Codec);
  isac_parameters.codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetSendParameters(isac_parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(103, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);

  EXPECT_TRUE(channel_->SetSendParameters(opus_parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(111, gcodec.pltype);
  EXPECT_STREQ("opus", gcodec.plname);
}

// Test that we handle various ways of specifying bitrate.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBitrate) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);  // bitrate == 32000
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(103, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_EQ(32000, gcodec.rate);

  parameters.codecs[0].bitrate = 0;         // bitrate == default
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(103, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_EQ(-1, gcodec.rate);

  parameters.codecs[0].bitrate = 28000;     // bitrate == 28000
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(103, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_EQ(28000, gcodec.rate);

  parameters.codecs[0] = kPcmuCodec;        // bitrate == 64000
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(0, gcodec.pltype);
  EXPECT_STREQ("PCMU", gcodec.plname);
  EXPECT_EQ(64000, gcodec.rate);

  parameters.codecs[0].bitrate = 0;         // bitrate == default
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(0, gcodec.pltype);
  EXPECT_STREQ("PCMU", gcodec.plname);
  EXPECT_EQ(64000, gcodec.rate);

  parameters.codecs[0] = kOpusCodec;
  parameters.codecs[0].bitrate = 0;         // bitrate == default
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(111, gcodec.pltype);
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(32000, gcodec.rate);
}

// Test that we could set packet size specified in kCodecParamPTime.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsPTimeAsPacketSize) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kOpusCodec);
  parameters.codecs[0].SetParam(cricket::kCodecParamPTime, 40); // Within range.
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(1920, gcodec.pacsize);  // Opus gets 40ms.

  parameters.codecs[0].SetParam(cricket::kCodecParamPTime, 5); // Below range.
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(480, gcodec.pacsize);  // Opus gets 10ms.

  parameters.codecs[0].SetParam(cricket::kCodecParamPTime, 80); // Beyond range.
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(2880, gcodec.pacsize);  // Opus gets 60ms.

  parameters.codecs[0] = kIsacCodec;  // Also try Isac, with unsupported size.
  parameters.codecs[0].SetParam(cricket::kCodecParamPTime, 40); // Within range.
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(480, gcodec.pacsize);  // Isac gets 30ms as the next smallest value.

  parameters.codecs[0] = kG722CodecSdp;  // Try G722 @8kHz as negotiated in SDP.
  parameters.codecs[0].SetParam(cricket::kCodecParamPTime, 40);
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(640, gcodec.pacsize);  // G722 gets 40ms @16kHz as defined in VoE.
}

// Test that we fail if no codecs are specified.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsNoCodecs) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::AudioSendParameters parameters;
  EXPECT_FALSE(channel_->SetSendParameters(parameters));
}

// Test that we can set send codecs even with telephone-event codec as the first
// one on the list.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsDTMFOnTop) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 98;  // DTMF
  parameters.codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_EQ(98, voe_.GetSendTelephoneEventPayloadType(channel_num));
}

// Test that we can set send codecs even with CN codec as the first
// one on the list.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCNOnTop) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 98;  // wideband CN
  parameters.codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_EQ(98, voe_.GetSendCNPayloadType(channel_num, true));
}

// Test that we set VAD and DTMF types correctly as caller.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCNandDTMFAsCaller) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  // TODO(juberti): cn 32000
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs.push_back(kCn8000Codec);
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs[0].id = 96;
  parameters.codecs[2].id = 97;  // wideband CN
  parameters.codecs[4].id = 98;  // DTMF
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetVAD(channel_num));
  EXPECT_FALSE(voe_.GetRED(channel_num));
  EXPECT_EQ(13, voe_.GetSendCNPayloadType(channel_num, false));
  EXPECT_EQ(97, voe_.GetSendCNPayloadType(channel_num, true));
  EXPECT_EQ(98, voe_.GetSendTelephoneEventPayloadType(channel_num));
}

// Test that we set VAD and DTMF types correctly as callee.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCNandDTMFAsCallee) {
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  channel_ = engine_.CreateChannel(&call_, cricket::AudioOptions());
  EXPECT_TRUE(channel_ != nullptr);

  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  // TODO(juberti): cn 32000
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs.push_back(kCn8000Codec);
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs[0].id = 96;
  parameters.codecs[2].id = 97;  // wideband CN
  parameters.codecs[4].id = 98;  // DTMF
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_num = voe_.GetLastChannel();

  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetVAD(channel_num));
  EXPECT_FALSE(voe_.GetRED(channel_num));
  EXPECT_EQ(13, voe_.GetSendCNPayloadType(channel_num, false));
  EXPECT_EQ(97, voe_.GetSendCNPayloadType(channel_num, true));
  EXPECT_EQ(98, voe_.GetSendTelephoneEventPayloadType(channel_num));
}

// Test that we only apply VAD if we have a CN codec that matches the
// send codec clockrate.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCNNoMatch) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  // Set ISAC(16K) and CN(16K). VAD should be activated.
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs[1].id = 97;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetVAD(channel_num));
  EXPECT_EQ(97, voe_.GetSendCNPayloadType(channel_num, true));
  // Set PCMU(8K) and CN(16K). VAD should not be activated.
  parameters.codecs[0] = kPcmuCodec;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("PCMU", gcodec.plname);
  EXPECT_FALSE(voe_.GetVAD(channel_num));
  // Set PCMU(8K) and CN(8K). VAD should be activated.
  parameters.codecs[1] = kCn8000Codec;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("PCMU", gcodec.plname);
  EXPECT_TRUE(voe_.GetVAD(channel_num));
  EXPECT_EQ(13, voe_.GetSendCNPayloadType(channel_num, false));
  // Set ISAC(16K) and CN(8K). VAD should not be activated.
  parameters.codecs[0] = kIsacCodec;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetVAD(channel_num));
}

// Test that we perform case-insensitive matching of codec names.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCaseInsensitive) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs.push_back(kCn8000Codec);
  parameters.codecs.push_back(kTelephoneEventCodec);
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs[0].name = "iSaC";
  parameters.codecs[0].id = 96;
  parameters.codecs[2].id = 97;  // wideband CN
  parameters.codecs[4].id = 98;  // DTMF
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetVAD(channel_num));
  EXPECT_FALSE(voe_.GetRED(channel_num));
  EXPECT_EQ(13, voe_.GetSendCNPayloadType(channel_num, false));
  EXPECT_EQ(97, voe_.GetSendCNPayloadType(channel_num, true));
  EXPECT_EQ(98, voe_.GetSendTelephoneEventPayloadType(channel_num));
}

// Test that we set up RED correctly as caller.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsREDAsCaller) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 127;
  parameters.codecs[0].params[""] = "96/96";
  parameters.codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetRED(channel_num));
  EXPECT_EQ(127, voe_.GetSendREDPayloadType(channel_num));
}

// Test that we set up RED correctly as callee.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsREDAsCallee) {
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  channel_ = engine_.CreateChannel(&call_, cricket::AudioOptions());
  EXPECT_TRUE(channel_ != nullptr);

  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 127;
  parameters.codecs[0].params[""] = "96/96";
  parameters.codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_num = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetRED(channel_num));
  EXPECT_EQ(127, voe_.GetSendREDPayloadType(channel_num));
}

// Test that we set up RED correctly if params are omitted.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsREDNoParams) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 127;
  parameters.codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetRED(channel_num));
  EXPECT_EQ(127, voe_.GetSendREDPayloadType(channel_num));
}

// Test that we ignore RED if the parameters aren't named the way we expect.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBadRED1) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 127;
  parameters.codecs[0].params["ABC"] = "96/96";
  parameters.codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetRED(channel_num));
}

// Test that we ignore RED if it uses different primary/secondary encoding.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBadRED2) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 127;
  parameters.codecs[0].params[""] = "96/0";
  parameters.codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetRED(channel_num));
}

// Test that we ignore RED if it uses more than 2 encodings.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBadRED3) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 127;
  parameters.codecs[0].params[""] = "96/96/96";
  parameters.codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetRED(channel_num));
}

// Test that we ignore RED if it has bogus codec ids.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBadRED4) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 127;
  parameters.codecs[0].params[""] = "ABC/ABC";
  parameters.codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetRED(channel_num));
}

// Test that we ignore RED if it refers to a codec that is not present.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBadRED5) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioSendParameters parameters;
  parameters.codecs.push_back(kRedCodec);
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  parameters.codecs[0].id = 127;
  parameters.codecs[0].params[""] = "97/97";
  parameters.codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetRED(channel_num));
}

// Test support for audio level header extension.
TEST_F(WebRtcVoiceEngineTestFake, SendAudioLevelHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(kRtpAudioLevelHeaderExtension);
}
TEST_F(WebRtcVoiceEngineTestFake, RecvAudioLevelHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(kRtpAudioLevelHeaderExtension);
}

// Test support for absolute send time header extension.
TEST_F(WebRtcVoiceEngineTestFake, SendAbsoluteSendTimeHeaderExtensions) {
  TestSetSendRtpHeaderExtensions(kRtpAbsoluteSenderTimeHeaderExtension);
}
TEST_F(WebRtcVoiceEngineTestFake, RecvAbsoluteSendTimeHeaderExtensions) {
  TestSetRecvRtpHeaderExtensions(kRtpAbsoluteSenderTimeHeaderExtension);
}

// Test that we can create a channel and start sending on it.
TEST_F(WebRtcVoiceEngineTestFake, Send) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_TRUE(voe_.GetSend(channel_num));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_NOTHING));
  EXPECT_FALSE(voe_.GetSend(channel_num));
}

// Test that we can create a channel and start playing out on it.
TEST_F(WebRtcVoiceEngineTestFake, Playout) {
  EXPECT_TRUE(SetupEngineWithRecvStream());
  int channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));
  EXPECT_TRUE(channel_->SetPlayout(true));
  EXPECT_TRUE(voe_.GetPlayout(channel_num));
  EXPECT_TRUE(channel_->SetPlayout(false));
  EXPECT_FALSE(voe_.GetPlayout(channel_num));
}

// Test that we can add and remove send streams.
TEST_F(WebRtcVoiceEngineTestFake, CreateAndDeleteMultipleSendStreams) {
  SetupForMultiSendStream();

  // Set the global state for sending.
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));

  for (uint32_t ssrc : kSsrcs4) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(ssrc)));
    EXPECT_NE(nullptr, call_.GetAudioSendStream(ssrc));

    // Verify that we are in a sending state for all the created streams.
    int channel_num = voe_.GetChannelFromLocalSsrc(ssrc);
    EXPECT_TRUE(voe_.GetSend(channel_num));
  }
  EXPECT_EQ(ARRAY_SIZE(kSsrcs4), call_.GetAudioSendStreams().size());

  // Delete the send streams.
  for (uint32_t ssrc : kSsrcs4) {
    EXPECT_TRUE(channel_->RemoveSendStream(ssrc));
    EXPECT_EQ(nullptr, call_.GetAudioSendStream(ssrc));
    // Stream should already be deleted.
    EXPECT_FALSE(channel_->RemoveSendStream(ssrc));
    EXPECT_EQ(-1, voe_.GetChannelFromLocalSsrc(ssrc));
  }
  EXPECT_EQ(0u, call_.GetAudioSendStreams().size());
}

// Test SetSendCodecs correctly configure the codecs in all send streams.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsWithMultipleSendStreams) {
  SetupForMultiSendStream();

  // Create send streams.
  for (uint32_t ssrc : kSsrcs4) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(ssrc)));
  }

  cricket::AudioSendParameters parameters;
  // Set ISAC(16K) and CN(16K). VAD should be activated.
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kCn16000Codec);
  parameters.codecs[1].id = 97;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));

  // Verify ISAC and VAD are corrected configured on all send channels.
  webrtc::CodecInst gcodec;
  for (uint32_t ssrc : kSsrcs4) {
    int channel_num = voe_.GetChannelFromLocalSsrc(ssrc);
    EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
    EXPECT_STREQ("ISAC", gcodec.plname);
    EXPECT_TRUE(voe_.GetVAD(channel_num));
    EXPECT_EQ(97, voe_.GetSendCNPayloadType(channel_num, true));
  }

  // Change to PCMU(8K) and CN(16K). VAD should not be activated.
  parameters.codecs[0] = kPcmuCodec;
  EXPECT_TRUE(channel_->SetSendParameters(parameters));
  for (uint32_t ssrc : kSsrcs4) {
    int channel_num = voe_.GetChannelFromLocalSsrc(ssrc);
    EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
    EXPECT_STREQ("PCMU", gcodec.plname);
    EXPECT_FALSE(voe_.GetVAD(channel_num));
  }
}

// Test we can SetSend on all send streams correctly.
TEST_F(WebRtcVoiceEngineTestFake, SetSendWithMultipleSendStreams) {
  SetupForMultiSendStream();

  // Create the send channels and they should be a SEND_NOTHING date.
  for (uint32_t ssrc : kSsrcs4) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(ssrc)));
    int channel_num = voe_.GetLastChannel();
    EXPECT_FALSE(voe_.GetSend(channel_num));
  }

  // Set the global state for starting sending.
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  for (uint32_t ssrc : kSsrcs4) {
    // Verify that we are in a sending state for all the send streams.
    int channel_num = voe_.GetChannelFromLocalSsrc(ssrc);
    EXPECT_TRUE(voe_.GetSend(channel_num));
  }

  // Set the global state for stopping sending.
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_NOTHING));
  for (uint32_t ssrc : kSsrcs4) {
    // Verify that we are in a stop state for all the send streams.
    int channel_num = voe_.GetChannelFromLocalSsrc(ssrc);
    EXPECT_FALSE(voe_.GetSend(channel_num));
  }
}

// Test we can set the correct statistics on all send streams.
TEST_F(WebRtcVoiceEngineTestFake, GetStatsWithMultipleSendStreams) {
  SetupForMultiSendStream();

  // Create send streams.
  for (uint32_t ssrc : kSsrcs4) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(ssrc)));
  }
  SetAudioSendStreamStats();

  // Create a receive stream to check that none of the send streams end up in
  // the receive stream stats.
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc2)));
  // We need send codec to be set to get all stats.
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  // Check stats for the added streams.
  {
    cricket::VoiceMediaInfo info;
    EXPECT_EQ(true, channel_->GetStats(&info));

    // We have added 4 send streams. We should see empty stats for all.
    EXPECT_EQ(static_cast<size_t>(ARRAY_SIZE(kSsrcs4)), info.senders.size());
    for (const auto& sender : info.senders) {
      VerifyVoiceSenderInfo(sender, false);
    }

    // We have added one receive stream. We should see empty stats.
    EXPECT_EQ(info.receivers.size(), 1u);
    EXPECT_EQ(info.receivers[0].ssrc(), 0);
  }

  // Remove the kSsrc2 stream. No receiver stats.
  {
    cricket::VoiceMediaInfo info;
    EXPECT_TRUE(channel_->RemoveRecvStream(kSsrc2));
    EXPECT_EQ(true, channel_->GetStats(&info));
    EXPECT_EQ(static_cast<size_t>(ARRAY_SIZE(kSsrcs4)), info.senders.size());
    EXPECT_EQ(0u, info.receivers.size());
  }

  // Deliver a new packet - a default receive stream should be created and we
  // should see stats again.
  {
    cricket::VoiceMediaInfo info;
    DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
    SetAudioReceiveStreamStats();
    EXPECT_EQ(true, channel_->GetStats(&info));
    EXPECT_EQ(static_cast<size_t>(ARRAY_SIZE(kSsrcs4)), info.senders.size());
    EXPECT_EQ(1u, info.receivers.size());
    VerifyVoiceReceiverInfo(info.receivers[0]);
  }
}

// Test that we can add and remove receive streams, and do proper send/playout.
// We can receive on multiple streams while sending one stream.
TEST_F(WebRtcVoiceEngineTestFake, PlayoutWithMultipleStreams) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num1 = voe_.GetLastChannel();

  // Start playout without a receive stream.
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_TRUE(channel_->SetPlayout(true));
  EXPECT_FALSE(voe_.GetPlayout(channel_num1));

  // Adding another stream should enable playout on the new stream only.
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int channel_num2 = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_TRUE(voe_.GetSend(channel_num1));
  EXPECT_FALSE(voe_.GetSend(channel_num2));

  // Make sure only the new stream is played out.
  EXPECT_FALSE(voe_.GetPlayout(channel_num1));
  EXPECT_TRUE(voe_.GetPlayout(channel_num2));

  // Adding yet another stream should have stream 2 and 3 enabled for playout.
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(3)));
  int channel_num3 = voe_.GetLastChannel();
  EXPECT_FALSE(voe_.GetPlayout(channel_num1));
  EXPECT_TRUE(voe_.GetPlayout(channel_num2));
  EXPECT_TRUE(voe_.GetPlayout(channel_num3));
  EXPECT_FALSE(voe_.GetSend(channel_num3));

  // Stop sending.
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_NOTHING));
  EXPECT_FALSE(voe_.GetSend(channel_num1));
  EXPECT_FALSE(voe_.GetSend(channel_num2));
  EXPECT_FALSE(voe_.GetSend(channel_num3));

  // Stop playout.
  EXPECT_TRUE(channel_->SetPlayout(false));
  EXPECT_FALSE(voe_.GetPlayout(channel_num1));
  EXPECT_FALSE(voe_.GetPlayout(channel_num2));
  EXPECT_FALSE(voe_.GetPlayout(channel_num3));

  // Restart playout and make sure only recv streams are played out.
  EXPECT_TRUE(channel_->SetPlayout(true));
  EXPECT_FALSE(voe_.GetPlayout(channel_num1));
  EXPECT_TRUE(voe_.GetPlayout(channel_num2));
  EXPECT_TRUE(voe_.GetPlayout(channel_num3));

  // Now remove the recv streams and verify that the send stream doesn't play.
  EXPECT_TRUE(channel_->RemoveRecvStream(3));
  EXPECT_TRUE(channel_->RemoveRecvStream(2));
  EXPECT_FALSE(voe_.GetPlayout(channel_num1));
}

// Test that we can set the devices to use.
TEST_F(WebRtcVoiceEngineTestFake, SetDevices) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int send_channel = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int recv_channel = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  cricket::Device default_dev(cricket::kFakeDefaultDeviceName,
                              cricket::kFakeDefaultDeviceId);
  cricket::Device dev(cricket::kFakeDeviceName,
                      cricket::kFakeDeviceId);

  // Test SetDevices() while not sending or playing.
  EXPECT_TRUE(engine_.SetDevices(&default_dev, &default_dev));

  // Test SetDevices() while sending and playing.
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_TRUE(channel_->SetPlayout(true));
  EXPECT_TRUE(voe_.GetSend(send_channel));
  EXPECT_TRUE(voe_.GetPlayout(recv_channel));

  EXPECT_TRUE(engine_.SetDevices(&dev, &dev));

  EXPECT_TRUE(voe_.GetSend(send_channel));
  EXPECT_TRUE(voe_.GetPlayout(recv_channel));

  // Test that failure to open newly selected devices does not prevent opening
  // ones after that.
  voe_.set_playout_fail_channel(recv_channel);
  voe_.set_send_fail_channel(send_channel);

  EXPECT_FALSE(engine_.SetDevices(&default_dev, &default_dev));

  EXPECT_FALSE(voe_.GetSend(send_channel));
  EXPECT_FALSE(voe_.GetPlayout(recv_channel));

  voe_.set_playout_fail_channel(-1);
  voe_.set_send_fail_channel(-1);

  EXPECT_TRUE(engine_.SetDevices(&dev, &dev));

  EXPECT_TRUE(voe_.GetSend(send_channel));
  EXPECT_TRUE(voe_.GetPlayout(recv_channel));
}

// Test that we can set the devices to use even if we failed to
// open the initial ones.
TEST_F(WebRtcVoiceEngineTestFake, SetDevicesWithInitiallyBadDevices) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int send_channel = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int recv_channel = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  cricket::Device default_dev(cricket::kFakeDefaultDeviceName,
                              cricket::kFakeDefaultDeviceId);
  cricket::Device dev(cricket::kFakeDeviceName,
                      cricket::kFakeDeviceId);

  // Test that failure to open devices selected before starting
  // send/play does not prevent opening newly selected ones after that.
  voe_.set_playout_fail_channel(recv_channel);
  voe_.set_send_fail_channel(send_channel);

  EXPECT_TRUE(engine_.SetDevices(&default_dev, &default_dev));

  EXPECT_FALSE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_FALSE(channel_->SetPlayout(true));
  EXPECT_FALSE(voe_.GetSend(send_channel));
  EXPECT_FALSE(voe_.GetPlayout(recv_channel));

  voe_.set_playout_fail_channel(-1);
  voe_.set_send_fail_channel(-1);

  EXPECT_TRUE(engine_.SetDevices(&dev, &dev));

  EXPECT_TRUE(voe_.GetSend(send_channel));
  EXPECT_TRUE(voe_.GetPlayout(recv_channel));
}

// Test that we can create a channel configured for Codian bridges,
// and start sending on it.
TEST_F(WebRtcVoiceEngineTestFake, CodianSend) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  int channel_num = voe_.GetLastChannel();
  webrtc::AgcConfig agc_config;
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(0, agc_config.targetLeveldBOv);
  send_parameters_.options = options_adjust_agc_;
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_TRUE(voe_.GetSend(channel_num));
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(agc_config.targetLeveldBOv, 10);  // level was attenuated
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_NOTHING));
  EXPECT_FALSE(voe_.GetSend(channel_num));
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(0, agc_config.targetLeveldBOv);  // level was restored
}

TEST_F(WebRtcVoiceEngineTestFake, TxAgcConfigViaOptions) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  webrtc::AgcConfig agc_config;
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(0, agc_config.targetLeveldBOv);

  cricket::AudioOptions options;
  options.tx_agc_target_dbov = rtc::Maybe<uint16_t>(3);
  options.tx_agc_digital_compression_gain = rtc::Maybe<uint16_t>(9);
  options.tx_agc_limiter = rtc::Maybe<bool>(true);
  options.auto_gain_control = rtc::Maybe<bool>(true);
  EXPECT_TRUE(engine_.SetOptions(options));

  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(3, agc_config.targetLeveldBOv);
  EXPECT_EQ(9, agc_config.digitalCompressionGaindB);
  EXPECT_TRUE(agc_config.limiterEnable);

  // Check interaction with adjust_agc_delta. Both should be respected, for
  // backwards compatibility.
  options.adjust_agc_delta = rtc::Maybe<int>(-10);
  EXPECT_TRUE(engine_.SetOptions(options));

  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(13, agc_config.targetLeveldBOv);
}

TEST_F(WebRtcVoiceEngineTestFake, SampleRatesViaOptions) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::AudioOptions options;
  options.recording_sample_rate = rtc::Maybe<uint32_t>(48000);
  options.playout_sample_rate = rtc::Maybe<uint32_t>(44100);
  EXPECT_TRUE(engine_.SetOptions(options));

  unsigned int recording_sample_rate, playout_sample_rate;
  EXPECT_EQ(0, voe_.RecordingSampleRate(&recording_sample_rate));
  EXPECT_EQ(0, voe_.PlayoutSampleRate(&playout_sample_rate));
  EXPECT_EQ(48000u, recording_sample_rate);
  EXPECT_EQ(44100u, playout_sample_rate);
}

TEST_F(WebRtcVoiceEngineTestFake, TraceFilterViaTraceOptions) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  engine_.SetLogging(rtc::LS_INFO, "");
  EXPECT_EQ(
      // Info:
      webrtc::kTraceStateInfo | webrtc::kTraceInfo |
      // Warning:
      webrtc::kTraceTerseInfo | webrtc::kTraceWarning |
      // Error:
      webrtc::kTraceError | webrtc::kTraceCritical,
      static_cast<int>(trace_wrapper_->filter_));
  // Now set it explicitly
  std::string filter =
      "tracefilter " + rtc::ToString(webrtc::kTraceDefault);
  engine_.SetLogging(rtc::LS_VERBOSE, filter.c_str());
  EXPECT_EQ(static_cast<unsigned int>(webrtc::kTraceDefault),
            trace_wrapper_->filter_);
}

// Test that we can set the outgoing SSRC properly.
// SSRC is set in SetupEngine by calling AddSendStream.
TEST_F(WebRtcVoiceEngineTestFake, SetSendSsrc) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  EXPECT_EQ(kSsrc1, voe_.GetLocalSSRC(voe_.GetLastChannel()));
}

TEST_F(WebRtcVoiceEngineTestFake, GetStats) {
  // Setup. We need send codec to be set to get all stats.
  EXPECT_TRUE(SetupEngineWithSendStream());
  SetAudioSendStreamStats();
  // SetupEngineWithSendStream adds a send stream with kSsrc1, so the receive
  // stream has to use a different SSRC.
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc2)));
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_TRUE(channel_->SetRecvParameters(recv_parameters_));

  // Check stats for the added streams.
  {
    cricket::VoiceMediaInfo info;
    EXPECT_EQ(true, channel_->GetStats(&info));

    // We have added one send stream. We should see the stats we've set.
    EXPECT_EQ(1u, info.senders.size());
    VerifyVoiceSenderInfo(info.senders[0], false);
    // We have added one receive stream. We should see empty stats.
    EXPECT_EQ(info.receivers.size(), 1u);
    EXPECT_EQ(info.receivers[0].ssrc(), 0);
  }

  // Start sending - this affects some reported stats.
  {
    cricket::VoiceMediaInfo info;
    EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
    EXPECT_EQ(true, channel_->GetStats(&info));
    VerifyVoiceSenderInfo(info.senders[0], true);
  }

  // Remove the kSsrc2 stream. No receiver stats.
  {
    cricket::VoiceMediaInfo info;
    EXPECT_TRUE(channel_->RemoveRecvStream(kSsrc2));
    EXPECT_EQ(true, channel_->GetStats(&info));
    EXPECT_EQ(1u, info.senders.size());
    EXPECT_EQ(0u, info.receivers.size());
  }

  // Deliver a new packet - a default receive stream should be created and we
  // should see stats again.
  {
    cricket::VoiceMediaInfo info;
    DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
    SetAudioReceiveStreamStats();
    EXPECT_EQ(true, channel_->GetStats(&info));
    EXPECT_EQ(1u, info.senders.size());
    EXPECT_EQ(1u, info.receivers.size());
    VerifyVoiceReceiverInfo(info.receivers[0]);
  }
}

// Test that we can set the outgoing SSRC properly with multiple streams.
// SSRC is set in SetupEngine by calling AddSendStream.
TEST_F(WebRtcVoiceEngineTestFake, SetSendSsrcWithMultipleStreams) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  EXPECT_EQ(kSsrc1, voe_.GetLocalSSRC(voe_.GetLastChannel()));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  EXPECT_EQ(kSsrc1, voe_.GetLocalSSRC(voe_.GetLastChannel()));
}

// Test that the local SSRC is the same on sending and receiving channels if the
// receive channel is created before the send channel.
TEST_F(WebRtcVoiceEngineTestFake, SetSendSsrcAfterCreatingReceiveChannel) {
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));
  channel_ = engine_.CreateChannel(&call_, cricket::AudioOptions());

  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  int receive_channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(1234)));
  int send_channel_num = voe_.GetLastChannel();

  EXPECT_EQ(1234U, voe_.GetLocalSSRC(send_channel_num));
  EXPECT_EQ(1234U, voe_.GetLocalSSRC(receive_channel_num));
}

// Test that we can properly receive packets.
TEST_F(WebRtcVoiceEngineTestFake, Recv) {
  EXPECT_TRUE(SetupEngine());
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  int channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(voe_.CheckPacket(channel_num, kPcmuFrame,
                               sizeof(kPcmuFrame)));
}

// Test that we can properly receive packets on multiple streams.
TEST_F(WebRtcVoiceEngineTestFake, RecvWithMultipleStreams) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  int channel_num1 = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int channel_num2 = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(3)));
  int channel_num3 = voe_.GetLastChannel();
  // Create packets with the right SSRCs.
  char packets[4][sizeof(kPcmuFrame)];
  for (size_t i = 0; i < ARRAY_SIZE(packets); ++i) {
    memcpy(packets[i], kPcmuFrame, sizeof(kPcmuFrame));
    rtc::SetBE32(packets[i] + 8, static_cast<uint32_t>(i));
  }
  EXPECT_TRUE(voe_.CheckNoPacket(channel_num1));
  EXPECT_TRUE(voe_.CheckNoPacket(channel_num2));
  EXPECT_TRUE(voe_.CheckNoPacket(channel_num3));
  DeliverPacket(packets[0], sizeof(packets[0]));
  EXPECT_TRUE(voe_.CheckNoPacket(channel_num1));
  EXPECT_TRUE(voe_.CheckNoPacket(channel_num2));
  EXPECT_TRUE(voe_.CheckNoPacket(channel_num3));
  DeliverPacket(packets[1], sizeof(packets[1]));
  EXPECT_TRUE(voe_.CheckPacket(channel_num1, packets[1],
                               sizeof(packets[1])));
  EXPECT_TRUE(voe_.CheckNoPacket(channel_num2));
  EXPECT_TRUE(voe_.CheckNoPacket(channel_num3));
  DeliverPacket(packets[2], sizeof(packets[2]));
  EXPECT_TRUE(voe_.CheckNoPacket(channel_num1));
  EXPECT_TRUE(voe_.CheckPacket(channel_num2, packets[2],
                               sizeof(packets[2])));
  EXPECT_TRUE(voe_.CheckNoPacket(channel_num3));
  DeliverPacket(packets[3], sizeof(packets[3]));
  EXPECT_TRUE(voe_.CheckNoPacket(channel_num1));
  EXPECT_TRUE(voe_.CheckNoPacket(channel_num2));
  EXPECT_TRUE(voe_.CheckPacket(channel_num3, packets[3],
                               sizeof(packets[3])));
  EXPECT_TRUE(channel_->RemoveRecvStream(3));
  EXPECT_TRUE(channel_->RemoveRecvStream(2));
  EXPECT_TRUE(channel_->RemoveRecvStream(1));
}

// Test that we properly handle failures to add a receive stream.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvStreamFail) {
  EXPECT_TRUE(SetupEngine());
  voe_.set_fail_create_channel(true);
  EXPECT_FALSE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
}

// Test that we properly handle failures to add a send stream.
TEST_F(WebRtcVoiceEngineTestFake, AddSendStreamFail) {
  EXPECT_TRUE(SetupEngine());
  voe_.set_fail_create_channel(true);
  EXPECT_FALSE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(2)));
}

// Test that AddRecvStream creates new stream.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvStream) {
  EXPECT_TRUE(SetupEngineWithRecvStream());
  int channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_NE(channel_num, voe_.GetLastChannel());
}

// Test that after adding a recv stream, we do not decode more codecs than
// those previously passed into SetRecvCodecs.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvStreamUnsupportedCodec) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::AudioRecvParameters parameters;
  parameters.codecs.push_back(kIsacCodec);
  parameters.codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetRecvParameters(parameters));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  rtc::strcpyn(gcodec.plname, ARRAY_SIZE(gcodec.plname), "opus");
  gcodec.plfreq = 48000;
  gcodec.channels = 2;
  EXPECT_EQ(-1, voe_.GetRecPayloadType(channel_num2, gcodec));
}

// Test that we properly clean up any streams that were added, even if
// not explicitly removed.
TEST_F(WebRtcVoiceEngineTestFake, StreamCleanup) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  EXPECT_EQ(3, voe_.GetNumChannels());  // default channel + 2 added
  delete channel_;
  channel_ = NULL;
  EXPECT_EQ(0, voe_.GetNumChannels());
}

TEST_F(WebRtcVoiceEngineTestFake, TestAddRecvStreamFailWithZeroSsrc) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  EXPECT_FALSE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(0)));
}

TEST_F(WebRtcVoiceEngineTestFake, TestNoLeakingWhenAddRecvStreamFail) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  // Manually delete channel to simulate a failure.
  int channel = voe_.GetLastChannel();
  EXPECT_EQ(0, voe_.DeleteChannel(channel));
  // Add recv stream 2 should work.
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int new_channel = voe_.GetLastChannel();
  EXPECT_NE(channel, new_channel);
  // The last created channel is deleted too.
  EXPECT_EQ(0, voe_.DeleteChannel(new_channel));
}

// Test the InsertDtmf on default send stream as caller.
TEST_F(WebRtcVoiceEngineTestFake, InsertDtmfOnDefaultSendStreamAsCaller) {
  TestInsertDtmf(0, true);
}

// Test the InsertDtmf on default send stream as callee
TEST_F(WebRtcVoiceEngineTestFake, InsertDtmfOnDefaultSendStreamAsCallee) {
  TestInsertDtmf(0, false);
}

// Test the InsertDtmf on specified send stream as caller.
TEST_F(WebRtcVoiceEngineTestFake, InsertDtmfOnSendStreamAsCaller) {
  TestInsertDtmf(kSsrc1, true);
}

// Test the InsertDtmf on specified send stream as callee.
TEST_F(WebRtcVoiceEngineTestFake, InsertDtmfOnSendStreamAsCallee) {
  TestInsertDtmf(kSsrc1, false);
}

TEST_F(WebRtcVoiceEngineTestFake, TestSetPlayoutError) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(3)));
  EXPECT_TRUE(channel_->SetPlayout(true));
  voe_.set_playout_fail_channel(voe_.GetLastChannel() - 1);
  EXPECT_TRUE(channel_->SetPlayout(false));
  EXPECT_FALSE(channel_->SetPlayout(true));
}

TEST_F(WebRtcVoiceEngineTestFake, SetAudioOptions) {
  EXPECT_TRUE(SetupEngineWithSendStream());

  bool ec_enabled;
  webrtc::EcModes ec_mode;
  webrtc::AecmModes aecm_mode;
  bool cng_enabled;
  bool agc_enabled;
  webrtc::AgcModes agc_mode;
  webrtc::AgcConfig agc_config;
  bool ns_enabled;
  webrtc::NsModes ns_mode;
  bool highpass_filter_enabled;
  bool stereo_swapping_enabled;
  bool typing_detection_enabled;
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAecmMode(aecm_mode, cng_enabled);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetAgcConfig(agc_config);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(voe_.ec_metrics_enabled());
  EXPECT_FALSE(cng_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_EQ(0, agc_config.targetLeveldBOv);
  EXPECT_TRUE(ns_enabled);
  EXPECT_TRUE(highpass_filter_enabled);
  EXPECT_FALSE(stereo_swapping_enabled);
  EXPECT_TRUE(typing_detection_enabled);
  EXPECT_EQ(ec_mode, webrtc::kEcConference);
  EXPECT_EQ(ns_mode, webrtc::kNsHighSuppression);

  // Nothing set, so all ignored.
  cricket::AudioOptions options;
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAecmMode(aecm_mode, cng_enabled);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetAgcConfig(agc_config);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(voe_.ec_metrics_enabled());
  EXPECT_FALSE(cng_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_EQ(0, agc_config.targetLeveldBOv);
  EXPECT_TRUE(ns_enabled);
  EXPECT_TRUE(highpass_filter_enabled);
  EXPECT_FALSE(stereo_swapping_enabled);
  EXPECT_TRUE(typing_detection_enabled);
  EXPECT_EQ(ec_mode, webrtc::kEcConference);
  EXPECT_EQ(ns_mode, webrtc::kNsHighSuppression);
  EXPECT_EQ(50, voe_.GetNetEqCapacity());  // From GetDefaultEngineOptions().
  EXPECT_FALSE(
      voe_.GetNetEqFastAccelerate());  // From GetDefaultEngineOptions().

  // Turn echo cancellation off
  options.echo_cancellation = rtc::Maybe<bool>(false);
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetEcStatus(ec_enabled, ec_mode);
  EXPECT_FALSE(ec_enabled);

  // Turn echo cancellation back on, with settings, and make sure
  // nothing else changed.
  options.echo_cancellation = rtc::Maybe<bool>(true);
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAecmMode(aecm_mode, cng_enabled);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetAgcConfig(agc_config);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(voe_.ec_metrics_enabled());
  EXPECT_TRUE(agc_enabled);
  EXPECT_EQ(0, agc_config.targetLeveldBOv);
  EXPECT_TRUE(ns_enabled);
  EXPECT_TRUE(highpass_filter_enabled);
  EXPECT_FALSE(stereo_swapping_enabled);
  EXPECT_TRUE(typing_detection_enabled);
  EXPECT_EQ(ec_mode, webrtc::kEcConference);
  EXPECT_EQ(ns_mode, webrtc::kNsHighSuppression);

  // Turn on delay agnostic aec and make sure nothing change w.r.t. echo
  // control.
  options.delay_agnostic_aec = rtc::Maybe<bool>(true);
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAecmMode(aecm_mode, cng_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(voe_.ec_metrics_enabled());
  EXPECT_EQ(ec_mode, webrtc::kEcConference);

  // Turn off echo cancellation and delay agnostic aec.
  options.delay_agnostic_aec = rtc::Maybe<bool>(false);
  options.extended_filter_aec = rtc::Maybe<bool>(false);
  options.echo_cancellation = rtc::Maybe<bool>(false);
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetEcStatus(ec_enabled, ec_mode);
  EXPECT_FALSE(ec_enabled);
  // Turning delay agnostic aec back on should also turn on echo cancellation.
  options.delay_agnostic_aec = rtc::Maybe<bool>(true);
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetEcStatus(ec_enabled, ec_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(voe_.ec_metrics_enabled());
  EXPECT_EQ(ec_mode, webrtc::kEcConference);

  // Turn off AGC
  options.auto_gain_control = rtc::Maybe<bool>(false);
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  EXPECT_FALSE(agc_enabled);

  // Turn AGC back on
  options.auto_gain_control = rtc::Maybe<bool>(true);
  options.adjust_agc_delta = rtc::Maybe<int>();
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  EXPECT_TRUE(agc_enabled);
  voe_.GetAgcConfig(agc_config);
  EXPECT_EQ(0, agc_config.targetLeveldBOv);

  // Turn off other options (and stereo swapping on).
  options.noise_suppression = rtc::Maybe<bool>(false);
  options.highpass_filter = rtc::Maybe<bool>(false);
  options.typing_detection = rtc::Maybe<bool>(false);
  options.stereo_swapping = rtc::Maybe<bool>(true);
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_FALSE(ns_enabled);
  EXPECT_FALSE(highpass_filter_enabled);
  EXPECT_FALSE(typing_detection_enabled);
  EXPECT_TRUE(stereo_swapping_enabled);

  // Set options again to ensure it has no impact.
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_EQ(webrtc::kEcConference, ec_mode);
  EXPECT_FALSE(ns_enabled);
  EXPECT_EQ(webrtc::kNsHighSuppression, ns_mode);
}

TEST_F(WebRtcVoiceEngineTestFake, DefaultOptions) {
  EXPECT_TRUE(SetupEngineWithSendStream());

  bool ec_enabled;
  webrtc::EcModes ec_mode;
  bool agc_enabled;
  webrtc::AgcModes agc_mode;
  bool ns_enabled;
  webrtc::NsModes ns_mode;
  bool highpass_filter_enabled;
  bool stereo_swapping_enabled;
  bool typing_detection_enabled;

  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_TRUE(ns_enabled);
  EXPECT_TRUE(highpass_filter_enabled);
  EXPECT_TRUE(typing_detection_enabled);
  EXPECT_FALSE(stereo_swapping_enabled);
}

TEST_F(WebRtcVoiceEngineTestFake, InitDoesNotOverwriteDefaultAgcConfig) {
  webrtc::AgcConfig set_config = {0};
  set_config.targetLeveldBOv = 3;
  set_config.digitalCompressionGaindB = 9;
  set_config.limiterEnable = true;
  EXPECT_EQ(0, voe_.SetAgcConfig(set_config));
  EXPECT_TRUE(engine_.Init(rtc::Thread::Current()));

  webrtc::AgcConfig config = {0};
  EXPECT_EQ(0, voe_.GetAgcConfig(config));
  EXPECT_EQ(set_config.targetLeveldBOv, config.targetLeveldBOv);
  EXPECT_EQ(set_config.digitalCompressionGaindB,
            config.digitalCompressionGaindB);
  EXPECT_EQ(set_config.limiterEnable, config.limiterEnable);
}

TEST_F(WebRtcVoiceEngineTestFake, SetOptionOverridesViaChannels) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  rtc::scoped_ptr<cricket::WebRtcVoiceMediaChannel> channel1(
      static_cast<cricket::WebRtcVoiceMediaChannel*>(
          engine_.CreateChannel(&call_, cricket::AudioOptions())));
  rtc::scoped_ptr<cricket::WebRtcVoiceMediaChannel> channel2(
      static_cast<cricket::WebRtcVoiceMediaChannel*>(
          engine_.CreateChannel(&call_, cricket::AudioOptions())));

  // Have to add a stream to make SetSend work.
  cricket::StreamParams stream1;
  stream1.ssrcs.push_back(1);
  channel1->AddSendStream(stream1);
  cricket::StreamParams stream2;
  stream2.ssrcs.push_back(2);
  channel2->AddSendStream(stream2);

  // AEC and AGC and NS
  cricket::AudioSendParameters parameters_options_all = send_parameters_;
  parameters_options_all.options.echo_cancellation = rtc::Maybe<bool>(true);
  parameters_options_all.options.auto_gain_control = rtc::Maybe<bool>(true);
  parameters_options_all.options.noise_suppression = rtc::Maybe<bool>(true);
  ASSERT_TRUE(channel1->SetSendParameters(parameters_options_all));
  EXPECT_EQ(parameters_options_all.options, channel1->options());
  ASSERT_TRUE(channel2->SetSendParameters(parameters_options_all));
  EXPECT_EQ(parameters_options_all.options, channel2->options());

  // unset NS
  cricket::AudioSendParameters parameters_options_no_ns = send_parameters_;
  parameters_options_no_ns.options.noise_suppression = rtc::Maybe<bool>(false);
  ASSERT_TRUE(channel1->SetSendParameters(parameters_options_no_ns));
  cricket::AudioOptions expected_options = parameters_options_all.options;
  expected_options.echo_cancellation = rtc::Maybe<bool>(true);
  expected_options.auto_gain_control = rtc::Maybe<bool>(true);
  expected_options.noise_suppression = rtc::Maybe<bool>(false);
  EXPECT_EQ(expected_options, channel1->options());

  // unset AGC
  cricket::AudioSendParameters parameters_options_no_agc = send_parameters_;
  parameters_options_no_agc.options.auto_gain_control = rtc::Maybe<bool>(false);
  ASSERT_TRUE(channel2->SetSendParameters(parameters_options_no_agc));
  expected_options.echo_cancellation = rtc::Maybe<bool>(true);
  expected_options.auto_gain_control = rtc::Maybe<bool>(false);
  expected_options.noise_suppression = rtc::Maybe<bool>(true);
  EXPECT_EQ(expected_options, channel2->options());

  ASSERT_TRUE(engine_.SetOptions(parameters_options_all.options));
  bool ec_enabled;
  webrtc::EcModes ec_mode;
  bool agc_enabled;
  webrtc::AgcModes agc_mode;
  bool ns_enabled;
  webrtc::NsModes ns_mode;
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_TRUE(ns_enabled);

  channel1->SetSend(cricket::SEND_MICROPHONE);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_FALSE(ns_enabled);

  channel1->SetSend(cricket::SEND_NOTHING);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_TRUE(ns_enabled);

  channel2->SetSend(cricket::SEND_MICROPHONE);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_FALSE(agc_enabled);
  EXPECT_TRUE(ns_enabled);

  channel2->SetSend(cricket::SEND_NOTHING);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_TRUE(ns_enabled);

  // Make sure settings take effect while we are sending.
  ASSERT_TRUE(engine_.SetOptions(parameters_options_all.options));
  cricket::AudioSendParameters parameters_options_no_agc_nor_ns =
      send_parameters_;
  parameters_options_no_agc_nor_ns.options.auto_gain_control =
      rtc::Maybe<bool>(false);
  parameters_options_no_agc_nor_ns.options.noise_suppression =
      rtc::Maybe<bool>(false);
  channel2->SetSend(cricket::SEND_MICROPHONE);
  channel2->SetSendParameters(parameters_options_no_agc_nor_ns);
  expected_options.echo_cancellation = rtc::Maybe<bool>(true);
  expected_options.auto_gain_control = rtc::Maybe<bool>(false);
  expected_options.noise_suppression = rtc::Maybe<bool>(false);
  EXPECT_EQ(expected_options, channel2->options());
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_FALSE(agc_enabled);
  EXPECT_FALSE(ns_enabled);
}

// This test verifies DSCP settings are properly applied on voice media channel.
TEST_F(WebRtcVoiceEngineTestFake, TestSetDscpOptions) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  rtc::scoped_ptr<cricket::VoiceMediaChannel> channel(
      engine_.CreateChannel(&call_, cricket::AudioOptions()));
  rtc::scoped_ptr<cricket::FakeNetworkInterface> network_interface(
      new cricket::FakeNetworkInterface);
  channel->SetInterface(network_interface.get());
  cricket::AudioSendParameters parameters = send_parameters_;
  parameters.options.dscp = rtc::Maybe<bool>(true);
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  EXPECT_EQ(rtc::DSCP_EF, network_interface->dscp());
  // Verify previous value is not modified if dscp option is not set.
  EXPECT_TRUE(channel->SetSendParameters(send_parameters_));
  EXPECT_EQ(rtc::DSCP_EF, network_interface->dscp());
  parameters.options.dscp = rtc::Maybe<bool>(false);
  EXPECT_TRUE(channel->SetSendParameters(parameters));
  EXPECT_EQ(rtc::DSCP_DEFAULT, network_interface->dscp());
}

TEST_F(WebRtcVoiceEngineTestFake, TestGetReceiveChannelId) {
  EXPECT_TRUE(SetupEngine());
  cricket::WebRtcVoiceMediaChannel* media_channel =
        static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  EXPECT_EQ(-1, media_channel->GetReceiveChannelId(0));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_id = voe_.GetLastChannel();
  EXPECT_EQ(channel_id, media_channel->GetReceiveChannelId(kSsrc1));
  EXPECT_EQ(-1, media_channel->GetReceiveChannelId(kSsrc2));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc2)));
  int channel_id2 = voe_.GetLastChannel();
  EXPECT_EQ(channel_id2, media_channel->GetReceiveChannelId(kSsrc2));
}

TEST_F(WebRtcVoiceEngineTestFake, TestGetSendChannelId) {
  EXPECT_TRUE(SetupEngine());
  cricket::WebRtcVoiceMediaChannel* media_channel =
        static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  EXPECT_EQ(-1, media_channel->GetSendChannelId(0));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_id = voe_.GetLastChannel();
  EXPECT_EQ(channel_id, media_channel->GetSendChannelId(kSsrc1));
  EXPECT_EQ(-1, media_channel->GetSendChannelId(kSsrc2));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc2)));
  int channel_id2 = voe_.GetLastChannel();
  EXPECT_EQ(channel_id2, media_channel->GetSendChannelId(kSsrc2));
}

TEST_F(WebRtcVoiceEngineTestFake, SetOutputVolume) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_FALSE(channel_->SetOutputVolume(kSsrc2, 0.5));
  cricket::StreamParams stream;
  stream.ssrcs.push_back(kSsrc2);
  EXPECT_TRUE(channel_->AddRecvStream(stream));
  int channel_id = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->SetOutputVolume(kSsrc2, 3));
  float scale = 0;
  EXPECT_EQ(0, voe_.GetChannelOutputVolumeScaling(channel_id, scale));
  EXPECT_DOUBLE_EQ(3, scale);
}

TEST_F(WebRtcVoiceEngineTestFake, SetOutputVolumeDefaultRecvStream) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOutputVolume(0, 2));
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  int channel_id = voe_.GetLastChannel();
  float scale = 0;
  EXPECT_EQ(0, voe_.GetChannelOutputVolumeScaling(channel_id, scale));
  EXPECT_DOUBLE_EQ(2, scale);
  EXPECT_TRUE(channel_->SetOutputVolume(0, 3));
  EXPECT_EQ(0, voe_.GetChannelOutputVolumeScaling(channel_id, scale));
  EXPECT_DOUBLE_EQ(3, scale);
}

TEST_F(WebRtcVoiceEngineTestFake, SetsSyncGroupFromSyncLabel) {
  const uint32_t kAudioSsrc = 123;
  const std::string kSyncLabel = "AvSyncLabel";

  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::StreamParams sp = cricket::StreamParams::CreateLegacy(kAudioSsrc);
  sp.sync_label = kSyncLabel;
  // Creating two channels to make sure that sync label is set properly for both
  // the default voice channel and following ones.
  EXPECT_TRUE(channel_->AddRecvStream(sp));
  sp.ssrcs[0] += 1;
  EXPECT_TRUE(channel_->AddRecvStream(sp));

  ASSERT_EQ(2, call_.GetAudioReceiveStreams().size());
  EXPECT_EQ(kSyncLabel,
            call_.GetAudioReceiveStream(kAudioSsrc)->GetConfig().sync_group)
      << "SyncGroup should be set based on sync_label";
  EXPECT_EQ(kSyncLabel,
            call_.GetAudioReceiveStream(kAudioSsrc + 1)->GetConfig().sync_group)
      << "SyncGroup should be set based on sync_label";
}

TEST_F(WebRtcVoiceEngineTestFake, CanChangeCombinedBweOption) {
  // Test that changing the combined_audio_video_bwe option results in the
  // expected state changes on an associated Call.
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(223);
  ssrcs.push_back(224);

  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::WebRtcVoiceMediaChannel* media_channel =
      static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  for (uint32_t ssrc : ssrcs) {
    EXPECT_TRUE(media_channel->AddRecvStream(
        cricket::StreamParams::CreateLegacy(ssrc)));
  }
  EXPECT_EQ(2, call_.GetAudioReceiveStreams().size());

  // Combined BWE should be disabled.
  for (uint32_t ssrc : ssrcs) {
    const auto* s = call_.GetAudioReceiveStream(ssrc);
    EXPECT_NE(nullptr, s);
    EXPECT_FALSE(s->GetConfig().combined_audio_video_bwe);
  }

  // Enable combined BWE option - now it should be set up.
  send_parameters_.options.combined_audio_video_bwe = rtc::Maybe<bool>(true);
  EXPECT_TRUE(media_channel->SetSendParameters(send_parameters_));
  for (uint32_t ssrc : ssrcs) {
    const auto* s = call_.GetAudioReceiveStream(ssrc);
    EXPECT_NE(nullptr, s);
    EXPECT_EQ(true, s->GetConfig().combined_audio_video_bwe);
  }

  // Disable combined BWE option - should be disabled again.
  send_parameters_.options.combined_audio_video_bwe = rtc::Maybe<bool>(false);
  EXPECT_TRUE(media_channel->SetSendParameters(send_parameters_));
  for (uint32_t ssrc : ssrcs) {
    const auto* s = call_.GetAudioReceiveStream(ssrc);
    EXPECT_NE(nullptr, s);
    EXPECT_FALSE(s->GetConfig().combined_audio_video_bwe);
  }

  EXPECT_EQ(2, call_.GetAudioReceiveStreams().size());
}

TEST_F(WebRtcVoiceEngineTestFake, ConfigureCombinedBweForNewRecvStreams) {
  // Test that adding receive streams after enabling combined bandwidth
  // estimation will correctly configure each channel.
  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::WebRtcVoiceMediaChannel* media_channel =
      static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  send_parameters_.options.combined_audio_video_bwe = rtc::Maybe<bool>(true);
  EXPECT_TRUE(media_channel->SetSendParameters(send_parameters_));

  static const uint32_t kSsrcs[] = {1, 2, 3, 4};
  for (unsigned int i = 0; i < ARRAY_SIZE(kSsrcs); ++i) {
    EXPECT_TRUE(media_channel->AddRecvStream(
        cricket::StreamParams::CreateLegacy(kSsrcs[i])));
    EXPECT_NE(nullptr, call_.GetAudioReceiveStream(kSsrcs[i]));
  }
  EXPECT_EQ(ARRAY_SIZE(kSsrcs), call_.GetAudioReceiveStreams().size());
}

TEST_F(WebRtcVoiceEngineTestFake, ConfiguresAudioReceiveStreamRtpExtensions) {
  // Test that setting the header extensions results in the expected state
  // changes on an associated Call.
  std::vector<uint32_t> ssrcs;
  ssrcs.push_back(223);
  ssrcs.push_back(224);

  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::WebRtcVoiceMediaChannel* media_channel =
      static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  send_parameters_.options.combined_audio_video_bwe = rtc::Maybe<bool>(true);
  EXPECT_TRUE(media_channel->SetSendParameters(send_parameters_));
  for (uint32_t ssrc : ssrcs) {
    EXPECT_TRUE(media_channel->AddRecvStream(
        cricket::StreamParams::CreateLegacy(ssrc)));
  }

  // Combined BWE should be set up, but with no configured extensions.
  EXPECT_EQ(2, call_.GetAudioReceiveStreams().size());
  for (uint32_t ssrc : ssrcs) {
    const auto* s = call_.GetAudioReceiveStream(ssrc);
    EXPECT_NE(nullptr, s);
    EXPECT_EQ(0, s->GetConfig().rtp.extensions.size());
  }

  // Set up receive extensions.
  const auto& e_exts = engine_.rtp_header_extensions();
  cricket::AudioRecvParameters recv_parameters;
  recv_parameters.extensions = e_exts;
  channel_->SetRecvParameters(recv_parameters);
  EXPECT_EQ(2, call_.GetAudioReceiveStreams().size());
  for (uint32_t ssrc : ssrcs) {
    const auto* s = call_.GetAudioReceiveStream(ssrc);
    EXPECT_NE(nullptr, s);
    const auto& s_exts = s->GetConfig().rtp.extensions;
    EXPECT_EQ(e_exts.size(), s_exts.size());
    for (const auto& e_ext : e_exts) {
      for (const auto& s_ext : s_exts) {
        if (e_ext.id == s_ext.id) {
          EXPECT_EQ(e_ext.uri, s_ext.name);
        }
      }
    }
  }

  // Disable receive extensions.
  channel_->SetRecvParameters(cricket::AudioRecvParameters());
  for (uint32_t ssrc : ssrcs) {
    const auto* s = call_.GetAudioReceiveStream(ssrc);
    EXPECT_NE(nullptr, s);
    EXPECT_EQ(0, s->GetConfig().rtp.extensions.size());
  }
}

TEST_F(WebRtcVoiceEngineTestFake, DeliverAudioPacket_Call) {
  // Test that packets are forwarded to the Call when configured accordingly.
  const uint32_t kAudioSsrc = 1;
  rtc::Buffer kPcmuPacket(kPcmuFrame, sizeof(kPcmuFrame));
  static const unsigned char kRtcp[] = {
    0x80, 0xc9, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  rtc::Buffer kRtcpPacket(kRtcp, sizeof(kRtcp));

  EXPECT_TRUE(SetupEngineWithSendStream());
  cricket::WebRtcVoiceMediaChannel* media_channel =
      static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  send_parameters_.options.combined_audio_video_bwe = rtc::Maybe<bool>(true);
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  EXPECT_TRUE(media_channel->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kAudioSsrc)));

  EXPECT_EQ(1, call_.GetAudioReceiveStreams().size());
  const cricket::FakeAudioReceiveStream* s =
      call_.GetAudioReceiveStream(kAudioSsrc);
  EXPECT_EQ(0, s->received_packets());
  channel_->OnPacketReceived(&kPcmuPacket, rtc::PacketTime());
  EXPECT_EQ(1, s->received_packets());
  channel_->OnRtcpReceived(&kRtcpPacket, rtc::PacketTime());
  EXPECT_EQ(2, s->received_packets());
}

// All receive channels should be associated with the first send channel,
// since they do not send RTCP SR.
TEST_F(WebRtcVoiceEngineTestFake, AssociateFirstSendChannel) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));
  int default_channel = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  int recv_ch = voe_.GetLastChannel();
  EXPECT_NE(recv_ch, default_channel);
  EXPECT_EQ(voe_.GetAssociateSendChannel(recv_ch), default_channel);
  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(2)));
  EXPECT_EQ(voe_.GetAssociateSendChannel(recv_ch), default_channel);
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(3)));
  recv_ch = voe_.GetLastChannel();
  EXPECT_NE(recv_ch, default_channel);
  EXPECT_EQ(voe_.GetAssociateSendChannel(recv_ch), default_channel);
}

TEST_F(WebRtcVoiceEngineTestFake, AssociateChannelResetUponDeleteChannnel) {
  EXPECT_TRUE(SetupEngineWithSendStream());
  EXPECT_TRUE(channel_->SetSendParameters(send_parameters_));

  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  int recv_ch = voe_.GetLastChannel();

  EXPECT_TRUE(channel_->AddSendStream(cricket::StreamParams::CreateLegacy(2)));
  int send_ch = voe_.GetLastChannel();

  // Manually associate |recv_ch| to |send_ch|. This test is to verify a
  // deleting logic, i.e., deleting |send_ch| will reset the associate send
  // channel of |recv_ch|.This is not a common case, since， normally, only the
  // default channel can be associated. However, the default is not deletable.
  // So we force the |recv_ch| to associate with a non-default channel.
  EXPECT_EQ(0, voe_.AssociateSendChannel(recv_ch, send_ch));
  EXPECT_EQ(voe_.GetAssociateSendChannel(recv_ch), send_ch);

  EXPECT_TRUE(channel_->RemoveSendStream(2));
  EXPECT_EQ(voe_.GetAssociateSendChannel(recv_ch), -1);
}

// Tests for the actual WebRtc VoE library.

TEST(WebRtcVoiceEngineTest, TestDefaultOptionsBeforeInit) {
  cricket::WebRtcVoiceEngine engine;
  cricket::AudioOptions options = engine.GetOptions();
  // The default options should have at least a few things set. We purposefully
  // don't check the option values here, though.
  EXPECT_TRUE(options.echo_cancellation);
  EXPECT_TRUE(options.auto_gain_control);
  EXPECT_TRUE(options.noise_suppression);
}

// Tests that the library initializes and shuts down properly.
TEST(WebRtcVoiceEngineTest, StartupShutdown) {
  cricket::WebRtcVoiceEngine engine;
  EXPECT_TRUE(engine.Init(rtc::Thread::Current()));
  rtc::scoped_ptr<webrtc::Call> call(
      webrtc::Call::Create(webrtc::Call::Config()));
  cricket::VoiceMediaChannel* channel =
      engine.CreateChannel(call.get(), cricket::AudioOptions());
  EXPECT_TRUE(channel != nullptr);
  delete channel;
  engine.Terminate();

  // Reinit to catch regression where VoiceEngineObserver reference is lost
  EXPECT_TRUE(engine.Init(rtc::Thread::Current()));
  engine.Terminate();
}

// Tests that the library is configured with the codecs we want.
TEST(WebRtcVoiceEngineTest, HasCorrectCodecs) {
  cricket::WebRtcVoiceEngine engine;
  // Check codecs by name.
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "OPUS", 48000, 0, 2, 0)));
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "ISAC", 16000, 0, 1, 0)));
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "ISAC", 32000, 0, 1, 0)));
  // Check that name matching is case-insensitive.
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "ILBC", 8000, 0, 1, 0)));
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "iLBC", 8000, 0, 1, 0)));
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "PCMU", 8000, 0, 1, 0)));
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "PCMA", 8000, 0, 1, 0)));
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "G722", 8000, 0, 1, 0)));
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "red", 8000, 0, 1, 0)));
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "CN", 32000, 0, 1, 0)));
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "CN", 16000, 0, 1, 0)));
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "CN", 8000, 0, 1, 0)));
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(96, "telephone-event", 8000, 0, 1, 0)));
  // Check codecs with an id by id.
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(0, "", 8000, 0, 1, 0)));   // PCMU
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(8, "", 8000, 0, 1, 0)));   // PCMA
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(9, "", 8000, 0, 1, 0)));  // G722
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(13, "", 8000, 0, 1, 0)));  // CN
  // Check sample/bitrate matching.
  EXPECT_TRUE(engine.FindCodec(
      cricket::AudioCodec(0, "PCMU", 8000, 64000, 1, 0)));
  // Check that bad codecs fail.
  EXPECT_FALSE(engine.FindCodec(cricket::AudioCodec(99, "ABCD", 0, 0, 1, 0)));
  EXPECT_FALSE(engine.FindCodec(cricket::AudioCodec(88, "", 0, 0, 1, 0)));
  EXPECT_FALSE(engine.FindCodec(cricket::AudioCodec(0, "", 0, 0, 2, 0)));
  EXPECT_FALSE(engine.FindCodec(cricket::AudioCodec(0, "", 5000, 0, 1, 0)));
  EXPECT_FALSE(engine.FindCodec(cricket::AudioCodec(0, "", 0, 5000, 1, 0)));
  // Verify the payload id of common audio codecs, including CN, ISAC, and G722.
  for (std::vector<cricket::AudioCodec>::const_iterator it =
      engine.codecs().begin(); it != engine.codecs().end(); ++it) {
    if (it->name == "CN" && it->clockrate == 16000) {
      EXPECT_EQ(105, it->id);
    } else if (it->name == "CN" && it->clockrate == 32000) {
      EXPECT_EQ(106, it->id);
    } else if (it->name == "ISAC" && it->clockrate == 16000) {
      EXPECT_EQ(103, it->id);
    } else if (it->name == "ISAC" && it->clockrate == 32000) {
      EXPECT_EQ(104, it->id);
    } else if (it->name == "G722" && it->clockrate == 8000) {
      EXPECT_EQ(9, it->id);
    } else if (it->name == "telephone-event") {
      EXPECT_EQ(126, it->id);
    } else if (it->name == "red") {
      EXPECT_EQ(127, it->id);
    } else if (it->name == "opus") {
      EXPECT_EQ(111, it->id);
      ASSERT_TRUE(it->params.find("minptime") != it->params.end());
      EXPECT_EQ("10", it->params.find("minptime")->second);
      ASSERT_TRUE(it->params.find("maxptime") != it->params.end());
      EXPECT_EQ("60", it->params.find("maxptime")->second);
      ASSERT_TRUE(it->params.find("useinbandfec") != it->params.end());
      EXPECT_EQ("1", it->params.find("useinbandfec")->second);
    }
  }

  engine.Terminate();
}

// Tests that VoE supports at least 32 channels
TEST(WebRtcVoiceEngineTest, Has32Channels) {
  cricket::WebRtcVoiceEngine engine;
  EXPECT_TRUE(engine.Init(rtc::Thread::Current()));
  rtc::scoped_ptr<webrtc::Call> call(
      webrtc::Call::Create(webrtc::Call::Config()));

  cricket::VoiceMediaChannel* channels[32];
  int num_channels = 0;
  while (num_channels < ARRAY_SIZE(channels)) {
    cricket::VoiceMediaChannel* channel =
        engine.CreateChannel(call.get(), cricket::AudioOptions());
    if (!channel)
      break;
    channels[num_channels++] = channel;
  }

  int expected = ARRAY_SIZE(channels);
  EXPECT_EQ(expected, num_channels);

  while (num_channels > 0) {
    delete channels[--num_channels];
  }
  engine.Terminate();
}

// Test that we set our preferred codecs properly.
TEST(WebRtcVoiceEngineTest, SetRecvCodecs) {
  cricket::WebRtcVoiceEngine engine;
  EXPECT_TRUE(engine.Init(rtc::Thread::Current()));
  rtc::scoped_ptr<webrtc::Call> call(
      webrtc::Call::Create(webrtc::Call::Config()));
  cricket::WebRtcVoiceMediaChannel channel(&engine, cricket::AudioOptions(),
                                           call.get());
  cricket::AudioRecvParameters parameters;
  parameters.codecs = engine.codecs();
  EXPECT_TRUE(channel.SetRecvParameters(parameters));
}
