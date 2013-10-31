// Copyright 2008 Google Inc.
//
// Author: Justin Uberti (juberti@google.com)

#ifdef WIN32
#include "talk/base/win32.h"
#include <objbase.h>
#endif

#include "talk/base/byteorder.h"
#include "talk/base/gunit.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/base/fakemediaprocessor.h"
#include "talk/media/base/fakenetworkinterface.h"
#include "talk/media/base/fakertp.h"
#include "talk/media/webrtc/fakewebrtcvoiceengine.h"
#include "talk/media/webrtc/webrtcvoiceengine.h"
#include "talk/p2p/base/fakesession.h"
#include "talk/session/media/channel.h"

// Tests for the WebRtcVoiceEngine/VoiceChannel code.

static const cricket::AudioCodec kPcmuCodec(0, "PCMU", 8000, 64000, 1, 0);
static const cricket::AudioCodec kIsacCodec(103, "ISAC", 16000, 32000, 1, 0);
static const cricket::AudioCodec kCeltCodec(110, "CELT", 32000, 64000, 2, 0);
static const cricket::AudioCodec kOpusCodec(111, "opus", 48000, 64000, 2, 0);
static const cricket::AudioCodec kRedCodec(117, "red", 8000, 0, 1, 0);
static const cricket::AudioCodec kCn8000Codec(13, "CN", 8000, 0, 1, 0);
static const cricket::AudioCodec kCn16000Codec(105, "CN", 16000, 0, 1, 0);
static const cricket::AudioCodec
    kTelephoneEventCodec(106, "telephone-event", 8000, 0, 1, 0);
static const cricket::AudioCodec* const kAudioCodecs[] = {
    &kPcmuCodec, &kIsacCodec, &kCeltCodec, &kOpusCodec, &kRedCodec,
    &kCn8000Codec, &kCn16000Codec, &kTelephoneEventCodec,
};
const char kRingbackTone[] = "RIFF____WAVE____ABCD1234";
static uint32 kSsrc1 = 0x99;
static uint32 kSsrc2 = 0x98;

class FakeVoEWrapper : public cricket::VoEWrapper {
 public:
  explicit FakeVoEWrapper(cricket::FakeWebRtcVoiceEngine* engine)
      : cricket::VoEWrapper(engine,  // processing
                            engine,  // base
                            engine,  // codec
                            engine,  // dtmf
                            engine,  // file
                            engine,  // hw
                            engine,  // media
                            engine,  // neteq
                            engine,  // network
                            engine,  // rtp
                            engine,  // sync
                            engine) {  // volume
  }
};

class FakeVoETraceWrapper : public cricket::VoETraceWrapper {
 public:
  virtual int SetTraceFilter(const unsigned int filter) {
    filter_ = filter;
    return 0;
  }
  virtual int SetTraceFile(const char* fileNameUTF8) {
    return 0;
  }
  virtual int SetTraceCallback(webrtc::TraceCallback* callback) {
    return 0;
  }
  unsigned int filter_;
};

class WebRtcVoiceEngineTestFake : public testing::Test {
 public:
  class ChannelErrorListener : public sigslot::has_slots<> {
   public:
    explicit ChannelErrorListener(cricket::VoiceMediaChannel* channel)
        : ssrc_(0), error_(cricket::VoiceMediaChannel::ERROR_NONE) {
      ASSERT(channel != NULL);
      channel->SignalMediaError.connect(
          this, &ChannelErrorListener::OnVoiceChannelError);
    }
    void OnVoiceChannelError(uint32 ssrc,
                             cricket::VoiceMediaChannel::Error error) {
      ssrc_ = ssrc;
      error_ = error;
    }
    void Reset() {
      ssrc_ = 0;
      error_ = cricket::VoiceMediaChannel::ERROR_NONE;
    }
    uint32 ssrc() const {
      return ssrc_;
    }
    cricket::VoiceMediaChannel::Error error() const {
      return error_;
    }

   private:
    uint32 ssrc_;
    cricket::VoiceMediaChannel::Error error_;
  };

  WebRtcVoiceEngineTestFake()
      : voe_(kAudioCodecs, ARRAY_SIZE(kAudioCodecs)),
        voe_sc_(kAudioCodecs, ARRAY_SIZE(kAudioCodecs)),
        trace_wrapper_(new FakeVoETraceWrapper()),
        engine_(new FakeVoEWrapper(&voe_),
                new FakeVoEWrapper(&voe_sc_),
                trace_wrapper_),
        channel_(NULL), soundclip_(NULL) {
    options_conference_.conference_mode.Set(true);
    options_adjust_agc_.adjust_agc_delta.Set(-10);
  }
  bool SetupEngine() {
    bool result = engine_.Init(talk_base::Thread::Current());
    if (result) {
      channel_ = engine_.CreateChannel();
      result = (channel_ != NULL);
    }
    if (result) {
      result = channel_->AddSendStream(
          cricket::StreamParams::CreateLegacy(kSsrc1));
    }
    return result;
  }
  void SetupForMultiSendStream() {
    EXPECT_TRUE(SetupEngine());
    // Remove stream added in Setup, which is corresponding to default channel.
    int default_channel_num = voe_.GetLastChannel();
    uint32 default_send_ssrc = 0u;
    EXPECT_EQ(0, voe_.GetLocalSSRC(default_channel_num, default_send_ssrc));
    EXPECT_EQ(kSsrc1, default_send_ssrc);
    EXPECT_TRUE(channel_->RemoveSendStream(default_send_ssrc));

    // Verify the default channel still exists.
    EXPECT_EQ(0, voe_.GetLocalSSRC(default_channel_num, default_send_ssrc));
  }
  void DeliverPacket(const void* data, int len) {
    talk_base::Buffer packet(data, len);
    channel_->OnPacketReceived(&packet);
  }
  virtual void TearDown() {
    delete soundclip_;
    delete channel_;
    engine_.Terminate();
  }

  void TestInsertDtmf(uint32 ssrc, bool caller) {
    EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
    channel_ = engine_.CreateChannel();
    EXPECT_TRUE(channel_ != NULL);
    if (caller) {
      // if this is a caller, local description will be applied and add the
      // send stream.
      EXPECT_TRUE(channel_->AddSendStream(
          cricket::StreamParams::CreateLegacy(kSsrc1)));
    }
    int channel_id = voe_.GetLastChannel();

    // Test we can only InsertDtmf when the other side supports telephone-event.
    std::vector<cricket::AudioCodec> codecs;
    codecs.push_back(kPcmuCodec);
    EXPECT_TRUE(channel_->SetSendCodecs(codecs));
    EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
    EXPECT_FALSE(channel_->CanInsertDtmf());
    EXPECT_FALSE(channel_->InsertDtmf(ssrc, 1, 111, cricket::DF_SEND));
    codecs.push_back(kTelephoneEventCodec);
    EXPECT_TRUE(channel_->SetSendCodecs(codecs));
    EXPECT_TRUE(channel_->CanInsertDtmf());

    if (!caller) {
      // There's no active send channel yet.
      EXPECT_FALSE(channel_->InsertDtmf(ssrc, 2, 123, cricket::DF_SEND));
      EXPECT_TRUE(channel_->AddSendStream(
          cricket::StreamParams::CreateLegacy(kSsrc1)));
    }

    // Check we fail if the ssrc is invalid.
    EXPECT_FALSE(channel_->InsertDtmf(-1, 1, 111, cricket::DF_SEND));

    // Test send
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
  // |default_bitrate| is the default bitrate for the codec.
  // |auto_bitrate| is a parameter to set to SetSendBandwidth().
  // |desired_bitrate| is a parameter to set to SetSendBandwidth().
  // |expected_result| is expected results from SetSendBandwidth().
  void TestSendBandwidth(const cricket::AudioCodec& codec,
                         int default_bitrate,
                         bool auto_bitrate,
                         int desired_bitrate,
                         bool expected_result) {
    int channel_num = voe_.GetLastChannel();
    std::vector<cricket::AudioCodec> codecs;

    codecs.push_back(codec);
    EXPECT_TRUE(channel_->SetSendCodecs(codecs));

    bool result = channel_->SetSendBandwidth(auto_bitrate, desired_bitrate);
    EXPECT_EQ(expected_result, result);

    webrtc::CodecInst temp_codec;
    EXPECT_FALSE(voe_.GetSendCodec(channel_num, temp_codec));

    if (result) {
      // If SetSendBandwidth() returns true then bitrate is set correctly.
      if (auto_bitrate) {
        EXPECT_EQ(default_bitrate, temp_codec.rate);
      } else {
        EXPECT_EQ(desired_bitrate, temp_codec.rate);
      }
    } else {
      // If SetSendBandwidth() returns false then bitrate is set to the
      // default value.
      EXPECT_EQ(default_bitrate, temp_codec.rate);
    }
  }


  void TestSetSendRtpHeaderExtensions(int channel_id) {
    std::vector<cricket::RtpHeaderExtension> extensions;
    bool enable = false;
    unsigned char id = 0;

    // Ensure audio levels are off by default.
    EXPECT_EQ(0, voe_.GetRTPAudioLevelIndicationStatus(
        channel_id, enable, id));
    EXPECT_FALSE(enable);

    // Ensure unknown extensions won't cause an error.
    extensions.push_back(cricket::RtpHeaderExtension(
        "urn:ietf:params:unknowextention", 1));
    EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
    EXPECT_EQ(0, voe_.GetRTPAudioLevelIndicationStatus(
        channel_id, enable, id));
    EXPECT_FALSE(enable);

    // Ensure audio levels stay off with an empty list of headers.
    EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
    EXPECT_EQ(0, voe_.GetRTPAudioLevelIndicationStatus(
        channel_id, enable, id));
    EXPECT_FALSE(enable);

    // Ensure audio levels are enabled if the audio-level header is specified.
    extensions.push_back(cricket::RtpHeaderExtension(
        "urn:ietf:params:rtp-hdrext:ssrc-audio-level", 8));
    EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
    EXPECT_EQ(0, voe_.GetRTPAudioLevelIndicationStatus(
        channel_id, enable, id));
    EXPECT_TRUE(enable);
    EXPECT_EQ(8, id);

    // Ensure audio levels go back off with an empty list.
    extensions.clear();
    EXPECT_TRUE(channel_->SetSendRtpHeaderExtensions(extensions));
    EXPECT_EQ(0, voe_.GetRTPAudioLevelIndicationStatus(
        channel_id, enable, id));
    EXPECT_FALSE(enable);
  }

 protected:
  cricket::FakeWebRtcVoiceEngine voe_;
  cricket::FakeWebRtcVoiceEngine voe_sc_;
  FakeVoETraceWrapper* trace_wrapper_;
  cricket::WebRtcVoiceEngine engine_;
  cricket::VoiceMediaChannel* channel_;
  cricket::SoundclipMedia* soundclip_;

  cricket::AudioOptions options_conference_;
  cricket::AudioOptions options_adjust_agc_;
};

// Tests that our stub library "works".
TEST_F(WebRtcVoiceEngineTestFake, StartupShutdown) {
  EXPECT_FALSE(voe_.IsInited());
  EXPECT_FALSE(voe_sc_.IsInited());
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  EXPECT_TRUE(voe_.IsInited());
  // The soundclip engine is lazily initialized.
  EXPECT_FALSE(voe_sc_.IsInited());
  engine_.Terminate();
  EXPECT_FALSE(voe_.IsInited());
  EXPECT_FALSE(voe_sc_.IsInited());
}

// Tests that we can create and destroy a channel.
TEST_F(WebRtcVoiceEngineTestFake, CreateChannel) {
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  channel_ = engine_.CreateChannel();
  EXPECT_TRUE(channel_ != NULL);
}

// Tests that we properly handle failures in CreateChannel.
TEST_F(WebRtcVoiceEngineTestFake, CreateChannelFail) {
  voe_.set_fail_create_channel(true);
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  channel_ = engine_.CreateChannel();
  EXPECT_TRUE(channel_ == NULL);
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
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs.push_back(kTelephoneEventCodec);
  codecs[0].id = 106;  // collide with existing telephone-event
  codecs[2].id = 126;
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
  webrtc::CodecInst gcodec;
  talk_base::strcpyn(gcodec.plname, ARRAY_SIZE(gcodec.plname), "ISAC");
  gcodec.plfreq = 16000;
  gcodec.channels = 1;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num, gcodec));
  EXPECT_EQ(106, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  talk_base::strcpyn(gcodec.plname, ARRAY_SIZE(gcodec.plname),
      "telephone-event");
  gcodec.plfreq = 8000;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num, gcodec));
  EXPECT_EQ(126, gcodec.pltype);
  EXPECT_STREQ("telephone-event", gcodec.plname);
}

// Test that we fail to set an unknown inbound codec.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsUnsupportedCodec) {
  EXPECT_TRUE(SetupEngine());
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(cricket::AudioCodec(127, "XYZ", 32000, 0, 1, 0));
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs));
}

// Test that we fail if we have duplicate types in the inbound list.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsDuplicatePayloadType) {
  EXPECT_TRUE(SetupEngine());
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kCn16000Codec);
  codecs[1].id = kIsacCodec.id;
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs));
}

// Test that we can decode OPUS without stereo parameters.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWithOpusNoStereo) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst opus;
  engine_.FindWebRtcCodec(kOpusCodec, &opus);
  // Even without stereo parameters, recv codecs still specify channels = 2.
  EXPECT_EQ(2, opus.channels);
  EXPECT_EQ(111, opus.pltype);
  EXPECT_STREQ("opus", opus.plname);
  opus.pltype = 0;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, opus));
  EXPECT_EQ(111, opus.pltype);
}

// Test that we can decode OPUS with stereo = 0.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWithOpus0Stereo) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs.push_back(kOpusCodec);
  codecs[2].params["stereo"] = "0";
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
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
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs.push_back(kOpusCodec);
  codecs[2].params["stereo"] = "1";
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
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
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs.push_back(kTelephoneEventCodec);
  codecs[0].id = 106;  // collide with existing telephone-event
  codecs[2].id = 126;
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  talk_base::strcpyn(gcodec.plname, ARRAY_SIZE(gcodec.plname), "ISAC");
  gcodec.plfreq = 16000;
  gcodec.channels = 1;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, gcodec));
  EXPECT_EQ(106, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  talk_base::strcpyn(gcodec.plname, ARRAY_SIZE(gcodec.plname),
      "telephone-event");
  gcodec.plfreq = 8000;
  gcodec.channels = 1;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, gcodec));
  EXPECT_EQ(126, gcodec.pltype);
  EXPECT_STREQ("telephone-event", gcodec.plname);
}

TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsAfterAddingStreams) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs[0].id = 106;  // collide with existing telephone-event

  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));

  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  talk_base::strcpyn(gcodec.plname, ARRAY_SIZE(gcodec.plname), "ISAC");
  gcodec.plfreq = 16000;
  gcodec.channels = 1;
  EXPECT_EQ(0, voe_.GetRecPayloadType(channel_num2, gcodec));
  EXPECT_EQ(106, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
}

// Test that we can apply the same set of codecs again while playing.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvCodecsWhilePlaying) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kCn16000Codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
  EXPECT_TRUE(channel_->SetPlayout(true));
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));

  // Changing the payload type of a codec should fail.
  codecs[0].id = 127;
  EXPECT_FALSE(channel_->SetRecvCodecs(codecs));
  EXPECT_TRUE(voe_.GetPlayout(channel_num));
}

// Test that we can add a codec while playing.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvCodecsWhilePlaying) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kCn16000Codec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
  EXPECT_TRUE(channel_->SetPlayout(true));

  codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
  EXPECT_TRUE(voe_.GetPlayout(channel_num));
  webrtc::CodecInst gcodec;
  EXPECT_TRUE(engine_.FindWebRtcCodec(kOpusCodec, &gcodec));
  EXPECT_EQ(kOpusCodec.id, gcodec.pltype);
}

TEST_F(WebRtcVoiceEngineTestFake, SetSendBandwidthAuto) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));

  // Test that when autobw is true, bitrate is kept as the default
  // value. autobw is true for the following tests.

  // ISAC, default bitrate == 32000.
  TestSendBandwidth(kIsacCodec, 32000, true, 96000, true);

  // PCMU, default bitrate == 64000.
  TestSendBandwidth(kPcmuCodec, 64000, true, 96000, true);

  // CELT, default bitrate == 64000.
  TestSendBandwidth(kCeltCodec, 64000, true, 96000, true);

  // opus, default bitrate == 64000.
  TestSendBandwidth(kOpusCodec, 64000, true, 96000, true);
}

TEST_F(WebRtcVoiceEngineTestFake, SetSendBandwidthFixedMultiRateAsCaller) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));

  // Test that we can set bitrate if a multi-rate codec is used.
  // autobw is false for the following tests.

  // ISAC, default bitrate == 32000.
  TestSendBandwidth(kIsacCodec, 32000, false, 128000, true);

  // CELT, default bitrate == 64000.
  TestSendBandwidth(kCeltCodec, 64000, false, 96000, true);

  // opus, default bitrate == 64000.
  TestSendBandwidth(kOpusCodec, 64000, false, 96000, true);
}

TEST_F(WebRtcVoiceEngineTestFake, SetSendBandwidthFixedMultiRateAsCallee) {
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  channel_ = engine_.CreateChannel();
  EXPECT_TRUE(channel_ != NULL);
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));

  int desired_bitrate = 128000;
  EXPECT_TRUE(channel_->SetSendBandwidth(false, desired_bitrate));

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));

  int channel_num = voe_.GetLastChannel();
  webrtc::CodecInst codec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, codec));
  EXPECT_EQ(desired_bitrate, codec.rate);
}

// Test that bitrate cannot be set for CBR codecs.
// Bitrate is ignored if it is higher than the fixed bitrate.
// Bitrate less then the fixed bitrate is an error.
TEST_F(WebRtcVoiceEngineTestFake, SetSendBandwidthFixedCbr) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetSendCodecs(engine_.codecs()));

  webrtc::CodecInst codec;
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;

  // PCMU, default bitrate == 64000.
  codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, codec));
  EXPECT_EQ(64000, codec.rate);
  EXPECT_TRUE(channel_->SetSendBandwidth(false, 128000));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, codec));
  EXPECT_EQ(64000, codec.rate);
  EXPECT_FALSE(channel_->SetSendBandwidth(false, 128));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, codec));
  EXPECT_EQ(64000, codec.rate);
}

// Test that we apply codecs properly.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecs) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs.push_back(kRedCodec);
  codecs[0].id = 96;
  codecs[0].bitrate = 48000;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_EQ(48000, gcodec.rate);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetVAD(channel_num));
  EXPECT_FALSE(voe_.GetFEC(channel_num));
  EXPECT_EQ(13, voe_.GetSendCNPayloadType(channel_num, false));
  EXPECT_EQ(105, voe_.GetSendCNPayloadType(channel_num, true));
  EXPECT_EQ(106, voe_.GetSendTelephoneEventPayloadType(channel_num));
}

// TODO(pthatcher): Change failure behavior to returning false rather
// than defaulting to PCMU.
// Test that if clockrate is not 48000 for opus, we fail by fallback to PCMU.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBadClockrate) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 0;
  codecs[0].clockrate = 50000;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("PCMU", gcodec.plname);
}

// Test that if channels=0 for opus, we fail by falling back to PCMU.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBad0ChannelsNoStereo) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 0;
  codecs[0].channels = 0;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("PCMU", gcodec.plname);
}

// Test that if channels=0 for opus, we fail by falling back to PCMU.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBad0Channels1Stereo) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 0;
  codecs[0].channels = 0;
  codecs[0].params["stereo"] = "1";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("PCMU", gcodec.plname);
}

// Test that if channel is 1 for opus and there's no stereo, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpus1ChannelNoStereo) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 0;
  codecs[0].channels = 1;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("PCMU", gcodec.plname);
}

// Test that if channel is 1 for opus and stereo=0, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBad1Channel0Stereo) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 0;
  codecs[0].channels = 1;
  codecs[0].params["stereo"] = "0";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("PCMU", gcodec.plname);
}

// Test that if channel is 1 for opus and stereo=1, we fail.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusBad1Channel1Stereo) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 0;
  codecs[0].channels = 1;
  codecs[0].params["stereo"] = "1";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("PCMU", gcodec.plname);
}

// Test that with bitrate=0 and no stereo,
// channels and bitrate are 1 and 32000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGood0BitrateNoStereo) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 0;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(32000, gcodec.rate);
}

// Test that with bitrate=0 and stereo=0,
// channels and bitrate are 1 and 32000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGood0Bitrate0Stereo) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 0;
  codecs[0].params["stereo"] = "0";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(32000, gcodec.rate);
}

// Test that with bitrate=invalid and stereo=0,
// channels and bitrate are 1 and 32000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodXBitrate0Stereo) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].params["stereo"] = "0";
  webrtc::CodecInst gcodec;

  // bitrate that's out of the range between 6000 and 510000 will be considered
  // as invalid and ignored.
  codecs[0].bitrate = 5999;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(32000, gcodec.rate);

  codecs[0].bitrate = 510001;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(32000, gcodec.rate);
}

// Test that with bitrate=0 and stereo=1,
// channels and bitrate are 2 and 64000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGood0Bitrate1Stereo) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 0;
  codecs[0].params["stereo"] = "1";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(2, gcodec.channels);
  EXPECT_EQ(64000, gcodec.rate);
}

// Test that with bitrate=invalid and stereo=1,
// channels and bitrate are 2 and 64000.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodXBitrate1Stereo) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].params["stereo"] = "1";
  webrtc::CodecInst gcodec;

  // bitrate that's out of the range between 6000 and 510000 will be considered
  // as invalid and ignored.
  codecs[0].bitrate = 5999;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(2, gcodec.channels);
  EXPECT_EQ(64000, gcodec.rate);

  codecs[0].bitrate = 510001;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(2, gcodec.channels);
  EXPECT_EQ(64000, gcodec.rate);
}

// Test that with bitrate=N and stereo unset,
// channels and bitrate are 1 and N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrateNoStereo) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 96000;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
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
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 30000;
  codecs[0].params["stereo"] = "0";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(30000, gcodec.rate);
  EXPECT_STREQ("opus", gcodec.plname);
}

// Test that with bitrate=N and without any parameters,
// channels and bitrate are 1 and N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrateNoParameters) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 30000;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(30000, gcodec.rate);
  EXPECT_STREQ("opus", gcodec.plname);
}

// Test that with bitrate=N and stereo=1,
// channels and bitrate are 2 and N.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecOpusGoodNBitrate1Stereo) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 30000;
  codecs[0].params["stereo"] = "1";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
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
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].bitrate = 30000;
  webrtc::CodecInst gcodec;

  // Ignore if less than 6000.
  codecs[0].params["maxaveragebitrate"] = "5999";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(30000, gcodec.rate);

  // Ignore if larger than 510000.
  codecs[0].params["maxaveragebitrate"] = "510001";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(30000, gcodec.rate);

  codecs[0].params["maxaveragebitrate"] = "200000";
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(200000, gcodec.rate);
}

// Test that we can enable NACK with opus as caller.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecEnableNackAsCaller) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].AddFeedbackParam(cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                                                    cricket::kParamValueEmpty));
  EXPECT_FALSE(voe_.GetNACK(channel_num));
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(voe_.GetNACK(channel_num));
}

// Test that we can enable NACK with opus as callee.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecEnableNackAsCallee) {
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  channel_ = engine_.CreateChannel();
  EXPECT_TRUE(channel_ != NULL);

  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].AddFeedbackParam(cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                                                    cricket::kParamValueEmpty));
  EXPECT_FALSE(voe_.GetNACK(channel_num));
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_FALSE(voe_.GetNACK(channel_num));

  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  EXPECT_TRUE(voe_.GetNACK(channel_num));
}

// Test that we can enable NACK on receive streams.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecEnableNackRecvStreams) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  int channel_num1 = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int channel_num2 = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].AddFeedbackParam(cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                                                    cricket::kParamValueEmpty));
  EXPECT_FALSE(voe_.GetNACK(channel_num1));
  EXPECT_FALSE(voe_.GetNACK(channel_num2));
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(voe_.GetNACK(channel_num1));
  EXPECT_TRUE(voe_.GetNACK(channel_num2));
}

// Test that we can disable NACK.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecDisableNack) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].AddFeedbackParam(cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                                                    cricket::kParamValueEmpty));
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(voe_.GetNACK(channel_num));

  codecs.clear();
  codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_FALSE(voe_.GetNACK(channel_num));
}

// Test that we can disable NACK on receive streams.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecDisableNackRecvStreams) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  int channel_num1 = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int channel_num2 = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kOpusCodec);
  codecs[0].AddFeedbackParam(cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                                                    cricket::kParamValueEmpty));
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(voe_.GetNACK(channel_num1));
  EXPECT_TRUE(voe_.GetNACK(channel_num2));

  codecs.clear();
  codecs.push_back(kOpusCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_FALSE(voe_.GetNACK(channel_num1));
  EXPECT_FALSE(voe_.GetNACK(channel_num2));
}

// Test that NACK is enabled on a new receive stream.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvStreamEnableNack) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs[0].AddFeedbackParam(cricket::FeedbackParam(cricket::kRtcpFbParamNack,
                                                    cricket::kParamValueEmpty));
  codecs.push_back(kCn16000Codec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(voe_.GetNACK(channel_num));

  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(voe_.GetNACK(channel_num));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(3)));
  channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(voe_.GetNACK(channel_num));
}

// Test that we can apply CELT with stereo mode but fail with mono mode.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCelt) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kCeltCodec);
  codecs.push_back(kPcmuCodec);
  codecs[0].id = 96;
  codecs[0].channels = 2;
  codecs[0].bitrate = 96000;
  codecs[1].bitrate = 96000;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_EQ(96000, gcodec.rate);
  EXPECT_EQ(2, gcodec.channels);
  EXPECT_STREQ("CELT", gcodec.plname);
  // Doesn't support mono, expect it to fall back to the next codec in the list.
  codecs[0].channels = 1;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(0, gcodec.pltype);
  EXPECT_EQ(1, gcodec.channels);
  EXPECT_EQ(64000, gcodec.rate);
  EXPECT_STREQ("PCMU", gcodec.plname);
}

// Test that we can switch back and forth between CELT and ISAC with CN.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsIsacCeltSwitching) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> celt_codecs;
  celt_codecs.push_back(kCeltCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(celt_codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(110, gcodec.pltype);
  EXPECT_STREQ("CELT", gcodec.plname);

  std::vector<cricket::AudioCodec> isac_codecs;
  isac_codecs.push_back(kIsacCodec);
  isac_codecs.push_back(kCn16000Codec);
  isac_codecs.push_back(kCeltCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(isac_codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(103, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);

  EXPECT_TRUE(channel_->SetSendCodecs(celt_codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(110, gcodec.pltype);
  EXPECT_STREQ("CELT", gcodec.plname);
}

// Test that we handle various ways of specifying bitrate.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBitrate) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);  // bitrate == 32000
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(103, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_EQ(32000, gcodec.rate);

  codecs[0].bitrate = 0;         // bitrate == default
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(103, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_EQ(-1, gcodec.rate);

  codecs[0].bitrate = 28000;     // bitrate == 28000
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(103, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_EQ(28000, gcodec.rate);

  codecs[0] = kPcmuCodec;        // bitrate == 64000
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(0, gcodec.pltype);
  EXPECT_STREQ("PCMU", gcodec.plname);
  EXPECT_EQ(64000, gcodec.rate);

  codecs[0].bitrate = 0;         // bitrate == default
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(0, gcodec.pltype);
  EXPECT_STREQ("PCMU", gcodec.plname);
  EXPECT_EQ(64000, gcodec.rate);

  codecs[0] = kOpusCodec;
  codecs[0].bitrate = 0;         // bitrate == default
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(111, gcodec.pltype);
  EXPECT_STREQ("opus", gcodec.plname);
  EXPECT_EQ(32000, gcodec.rate);
}

// Test that we fall back to PCMU if no codecs are specified.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsNoCodecs) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(0, gcodec.pltype);
  EXPECT_STREQ("PCMU", gcodec.plname);
  EXPECT_FALSE(voe_.GetVAD(channel_num));
  EXPECT_FALSE(voe_.GetFEC(channel_num));
  EXPECT_EQ(13, voe_.GetSendCNPayloadType(channel_num, false));
  EXPECT_EQ(105, voe_.GetSendCNPayloadType(channel_num, true));
  EXPECT_EQ(106, voe_.GetSendTelephoneEventPayloadType(channel_num));
}

// Test that we set VAD and DTMF types correctly as caller.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCNandDTMFAsCaller) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  // TODO(juberti): cn 32000
  codecs.push_back(kCn16000Codec);
  codecs.push_back(kCn8000Codec);
  codecs.push_back(kTelephoneEventCodec);
  codecs.push_back(kRedCodec);
  codecs[0].id = 96;
  codecs[2].id = 97;  // wideband CN
  codecs[4].id = 98;  // DTMF
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetVAD(channel_num));
  EXPECT_FALSE(voe_.GetFEC(channel_num));
  EXPECT_EQ(13, voe_.GetSendCNPayloadType(channel_num, false));
  EXPECT_EQ(97, voe_.GetSendCNPayloadType(channel_num, true));
  EXPECT_EQ(98, voe_.GetSendTelephoneEventPayloadType(channel_num));
}

// Test that we set VAD and DTMF types correctly as callee.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCNandDTMFAsCallee) {
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  channel_ = engine_.CreateChannel();
  EXPECT_TRUE(channel_ != NULL);

  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  // TODO(juberti): cn 32000
  codecs.push_back(kCn16000Codec);
  codecs.push_back(kCn8000Codec);
  codecs.push_back(kTelephoneEventCodec);
  codecs.push_back(kRedCodec);
  codecs[0].id = 96;
  codecs[2].id = 97;  // wideband CN
  codecs[4].id = 98;  // DTMF
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));

  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetVAD(channel_num));
  EXPECT_FALSE(voe_.GetFEC(channel_num));
  EXPECT_EQ(13, voe_.GetSendCNPayloadType(channel_num, false));
  EXPECT_EQ(97, voe_.GetSendCNPayloadType(channel_num, true));
  EXPECT_EQ(98, voe_.GetSendTelephoneEventPayloadType(channel_num));
}

// Test that we only apply VAD if we have a CN codec that matches the
// send codec clockrate.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCNNoMatch) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  // Set ISAC(16K) and CN(16K). VAD should be activated.
  codecs.push_back(kIsacCodec);
  codecs.push_back(kCn16000Codec);
  codecs[1].id = 97;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetVAD(channel_num));
  EXPECT_EQ(97, voe_.GetSendCNPayloadType(channel_num, true));
  // Set PCMU(8K) and CN(16K). VAD should not be activated.
  codecs[0] = kPcmuCodec;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("PCMU", gcodec.plname);
  EXPECT_FALSE(voe_.GetVAD(channel_num));
  // Set PCMU(8K) and CN(8K). VAD should be activated.
  codecs[1] = kCn8000Codec;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("PCMU", gcodec.plname);
  EXPECT_TRUE(voe_.GetVAD(channel_num));
  EXPECT_EQ(13, voe_.GetSendCNPayloadType(channel_num, false));
   // Set ISAC(16K) and CN(8K). VAD should not be activated.
  codecs[0] = kIsacCodec;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetVAD(channel_num));
}

// Test that we perform case-insensitive matching of codec names.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsCaseInsensitive) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs.push_back(kCn16000Codec);
  codecs.push_back(kCn8000Codec);
  codecs.push_back(kTelephoneEventCodec);
  codecs.push_back(kRedCodec);
  codecs[0].name = "iSaC";
  codecs[0].id = 96;
  codecs[2].id = 97;  // wideband CN
  codecs[4].id = 98;  // DTMF
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetVAD(channel_num));
  EXPECT_FALSE(voe_.GetFEC(channel_num));
  EXPECT_EQ(13, voe_.GetSendCNPayloadType(channel_num, false));
  EXPECT_EQ(97, voe_.GetSendCNPayloadType(channel_num, true));
  EXPECT_EQ(98, voe_.GetSendTelephoneEventPayloadType(channel_num));
}

// Test that we set up FEC correctly as caller.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsREDAsCaller) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kRedCodec);
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs[0].id = 127;
  codecs[0].params[""] = "96/96";
  codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetFEC(channel_num));
  EXPECT_EQ(127, voe_.GetSendFECPayloadType(channel_num));
}

// Test that we set up FEC correctly as callee.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsREDAsCallee) {
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  channel_ = engine_.CreateChannel();
  EXPECT_TRUE(channel_ != NULL);

  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kRedCodec);
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs[0].id = 127;
  codecs[0].params[""] = "96/96";
  codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetFEC(channel_num));
  EXPECT_EQ(127, voe_.GetSendFECPayloadType(channel_num));
}

// Test that we set up FEC correctly if params are omitted.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsREDNoParams) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kRedCodec);
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs[0].id = 127;
  codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_TRUE(voe_.GetFEC(channel_num));
  EXPECT_EQ(127, voe_.GetSendFECPayloadType(channel_num));
}

// Test that we ignore RED if the parameters aren't named the way we expect.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBadRED1) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kRedCodec);
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs[0].id = 127;
  codecs[0].params["ABC"] = "96/96";
  codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetFEC(channel_num));
}

// Test that we ignore RED if it uses different primary/secondary encoding.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBadRED2) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kRedCodec);
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs[0].id = 127;
  codecs[0].params[""] = "96/0";
  codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetFEC(channel_num));
}

// Test that we ignore RED if it uses more than 2 encodings.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBadRED3) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kRedCodec);
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs[0].id = 127;
  codecs[0].params[""] = "96/96/96";
  codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetFEC(channel_num));
}

// Test that we ignore RED if it has bogus codec ids.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBadRED4) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kRedCodec);
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs[0].id = 127;
  codecs[0].params[""] = "ABC/ABC";
  codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetFEC(channel_num));
}

// Test that we ignore RED if it refers to a codec that is not present.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsBadRED5) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kRedCodec);
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  codecs[0].id = 127;
  codecs[0].params[""] = "97/97";
  codecs[1].id = 96;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  webrtc::CodecInst gcodec;
  EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
  EXPECT_EQ(96, gcodec.pltype);
  EXPECT_STREQ("ISAC", gcodec.plname);
  EXPECT_FALSE(voe_.GetFEC(channel_num));
}

// Test that we support setting an empty list of recv header extensions.
TEST_F(WebRtcVoiceEngineTestFake, SetRecvRtpHeaderExtensions) {
  EXPECT_TRUE(SetupEngine());
  std::vector<cricket::RtpHeaderExtension> extensions;
  int channel_num = voe_.GetLastChannel();
  bool enable = false;
  unsigned char id = 0;

  // An empty list shouldn't cause audio-level headers to be enabled.
  EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));
  EXPECT_EQ(0, voe_.GetRTPAudioLevelIndicationStatus(
      channel_num, enable, id));
  EXPECT_FALSE(enable);

  // Nor should indicating we can receive the audio-level header.
  extensions.push_back(cricket::RtpHeaderExtension(
      "urn:ietf:params:rtp-hdrext:ssrc-audio-level", 8));
  EXPECT_TRUE(channel_->SetRecvRtpHeaderExtensions(extensions));
  EXPECT_EQ(0, voe_.GetRTPAudioLevelIndicationStatus(
      channel_num, enable, id));
  EXPECT_FALSE(enable);
}

// Test that we support setting certain send header extensions.
TEST_F(WebRtcVoiceEngineTestFake, SetSendRtpHeaderExtensions) {
  EXPECT_TRUE(SetupEngine());
  std::vector<cricket::RtpHeaderExtension> extensions;
  int channel_num = voe_.GetLastChannel();
  TestSetSendRtpHeaderExtensions(channel_num);
}

// Test that we can create a channel and start sending/playing out on it.
TEST_F(WebRtcVoiceEngineTestFake, SendAndPlayout) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_TRUE(voe_.GetSend(channel_num));
  EXPECT_TRUE(channel_->SetPlayout(true));
  EXPECT_TRUE(voe_.GetPlayout(channel_num));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_NOTHING));
  EXPECT_FALSE(voe_.GetSend(channel_num));
  EXPECT_TRUE(channel_->SetPlayout(false));
  EXPECT_FALSE(voe_.GetPlayout(channel_num));
}

// Test that we can add and remove send streams.
TEST_F(WebRtcVoiceEngineTestFake, CreateAndDeleteMultipleSendStreams) {
  SetupForMultiSendStream();

  static const uint32 kSsrcs4[] = {1, 2, 3, 4};

  // Set the global state for sending.
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));

  for (unsigned int i = 0; i < ARRAY_SIZE(kSsrcs4); ++i) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrcs4[i])));

    // Verify that we are in a sending state for all the created streams.
    int channel_num = voe_.GetChannelFromLocalSsrc(kSsrcs4[i]);
    EXPECT_TRUE(voe_.GetSend(channel_num));
  }

  // Remove the first send channel, which is the default channel. It will only
  // recycle the default channel but not delete it.
  EXPECT_TRUE(channel_->RemoveSendStream(kSsrcs4[0]));
  // Stream should already be Removed from the send stream list.
  EXPECT_FALSE(channel_->RemoveSendStream(kSsrcs4[0]));
  // But the default still exists.
  EXPECT_EQ(0, voe_.GetChannelFromLocalSsrc(kSsrcs4[0]));

  // Delete the rest of send channel streams.
  for (unsigned int i = 1; i < ARRAY_SIZE(kSsrcs4); ++i) {
    EXPECT_TRUE(channel_->RemoveSendStream(kSsrcs4[i]));
    // Stream should already be deleted.
    EXPECT_FALSE(channel_->RemoveSendStream(kSsrcs4[i]));
    EXPECT_EQ(-1, voe_.GetChannelFromLocalSsrc(kSsrcs4[i]));
  }
}

// Test SetSendCodecs correctly configure the codecs in all send streams.
TEST_F(WebRtcVoiceEngineTestFake, SetSendCodecsWithMultipleSendStreams) {
  SetupForMultiSendStream();

  static const uint32 kSsrcs4[] = {1, 2, 3, 4};
  // Create send streams.
  for (unsigned int i = 0; i < ARRAY_SIZE(kSsrcs4); ++i) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrcs4[i])));
  }

  std::vector<cricket::AudioCodec> codecs;
  // Set ISAC(16K) and CN(16K). VAD should be activated.
  codecs.push_back(kIsacCodec);
  codecs.push_back(kCn16000Codec);
  codecs[1].id = 97;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  // Verify ISAC and VAD are corrected configured on all send channels.
  webrtc::CodecInst gcodec;
  for (unsigned int i = 0; i < ARRAY_SIZE(kSsrcs4); ++i) {
    int channel_num = voe_.GetChannelFromLocalSsrc(kSsrcs4[i]);
    EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
    EXPECT_STREQ("ISAC", gcodec.plname);
    EXPECT_TRUE(voe_.GetVAD(channel_num));
    EXPECT_EQ(97, voe_.GetSendCNPayloadType(channel_num, true));
  }

  // Change to PCMU(8K) and CN(16K). VAD should not be activated.
  codecs[0] = kPcmuCodec;
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  for (unsigned int i = 0; i < ARRAY_SIZE(kSsrcs4); ++i) {
    int channel_num = voe_.GetChannelFromLocalSsrc(kSsrcs4[i]);
    EXPECT_EQ(0, voe_.GetSendCodec(channel_num, gcodec));
    EXPECT_STREQ("PCMU", gcodec.plname);
    EXPECT_FALSE(voe_.GetVAD(channel_num));
  }
}

// Test we can SetSend on all send streams correctly.
TEST_F(WebRtcVoiceEngineTestFake, SetSendWithMultipleSendStreams) {
  SetupForMultiSendStream();

  static const uint32 kSsrcs4[] = {1, 2, 3, 4};
  // Create the send channels and they should be a SEND_NOTHING date.
  for (unsigned int i = 0; i < ARRAY_SIZE(kSsrcs4); ++i) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrcs4[i])));
    int channel_num = voe_.GetLastChannel();
    EXPECT_FALSE(voe_.GetSend(channel_num));
  }

  // Set the global state for starting sending.
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  for (unsigned int i = 0; i < ARRAY_SIZE(kSsrcs4); ++i) {
    // Verify that we are in a sending state for all the send streams.
    int channel_num = voe_.GetChannelFromLocalSsrc(kSsrcs4[i]);
    EXPECT_TRUE(voe_.GetSend(channel_num));
  }

  // Set the global state for stopping sending.
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_NOTHING));
  for (unsigned int i = 1; i < ARRAY_SIZE(kSsrcs4); ++i) {
    // Verify that we are in a stop state for all the send streams.
    int channel_num = voe_.GetChannelFromLocalSsrc(kSsrcs4[i]);
    EXPECT_FALSE(voe_.GetSend(channel_num));
  }
}

// Test we can set the correct statistics on all send streams.
TEST_F(WebRtcVoiceEngineTestFake, GetStatsWithMultipleSendStreams) {
  SetupForMultiSendStream();

  static const uint32 kSsrcs4[] = {1, 2, 3, 4};
  // Create send streams.
  for (unsigned int i = 0; i < ARRAY_SIZE(kSsrcs4); ++i) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrcs4[i])));
  }

  // We need send codec to be set to get all stats.
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  cricket::VoiceMediaInfo info;
  EXPECT_EQ(true, channel_->GetStats(&info));
  EXPECT_EQ(static_cast<size_t>(ARRAY_SIZE(kSsrcs4)), info.senders.size());

  // Verify the statistic information is correct.
  for (unsigned int i = 0; i < ARRAY_SIZE(kSsrcs4); ++i) {
    EXPECT_EQ(kSsrcs4[i], info.senders[i].ssrc);
    EXPECT_EQ(kPcmuCodec.name, info.senders[i].codec_name);
    EXPECT_EQ(cricket::kIntStatValue, info.senders[i].bytes_sent);
    EXPECT_EQ(cricket::kIntStatValue, info.senders[i].packets_sent);
    EXPECT_EQ(cricket::kIntStatValue, info.senders[i].packets_lost);
    EXPECT_EQ(cricket::kFractionLostStatValue, info.senders[i].fraction_lost);
    EXPECT_EQ(cricket::kIntStatValue, info.senders[i].ext_seqnum);
    EXPECT_EQ(cricket::kIntStatValue, info.senders[i].rtt_ms);
    EXPECT_EQ(cricket::kIntStatValue, info.senders[i].jitter_ms);
  }

  EXPECT_EQ(1u, info.receivers.size());
}

// Test that we support setting certain send header extensions on multiple
// send streams.
TEST_F(WebRtcVoiceEngineTestFake,
       SetSendRtpHeaderExtensionsWithMultpleSendStreams) {
  SetupForMultiSendStream();

  static const uint32 kSsrcs4[] = {1, 2, 3, 4};
  // Create send streams.
  for (unsigned int i = 0; i < ARRAY_SIZE(kSsrcs4); ++i) {
    EXPECT_TRUE(channel_->AddSendStream(
        cricket::StreamParams::CreateLegacy(kSsrcs4[i])));
  }

  // Test SendRtpHeaderExtensions on each send channel.
  for (unsigned int i = 0; i < ARRAY_SIZE(kSsrcs4); ++i) {
    int channel_num = voe_.GetChannelFromLocalSsrc(kSsrcs4[i]);
    TestSetSendRtpHeaderExtensions(channel_num);
  }
}

// Test that we can add and remove receive streams, and do proper send/playout.
// We can receive on multiple streams while sending one stream.
TEST_F(WebRtcVoiceEngineTestFake, PlayoutWithMultipleStreams) {
  EXPECT_TRUE(SetupEngine());
  int channel_num1 = voe_.GetLastChannel();

  // Start playout on the default channel.
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  EXPECT_TRUE(channel_->SetPlayout(true));
  EXPECT_TRUE(voe_.GetPlayout(channel_num1));

  // Adding another stream should disable playout on the default channel.
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int channel_num2 = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_TRUE(voe_.GetSend(channel_num1));
  EXPECT_FALSE(voe_.GetSend(channel_num2));

  // Make sure only the new channel is played out.
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

  // Restart playout and make sure the default channel still is not played out.
  EXPECT_TRUE(channel_->SetPlayout(true));
  EXPECT_FALSE(voe_.GetPlayout(channel_num1));
  EXPECT_TRUE(voe_.GetPlayout(channel_num2));
  EXPECT_TRUE(voe_.GetPlayout(channel_num3));

  // Now remove the new streams and verify that the default channel is
  // played out again.
  EXPECT_TRUE(channel_->RemoveRecvStream(3));
  EXPECT_TRUE(channel_->RemoveRecvStream(2));

  EXPECT_TRUE(voe_.GetPlayout(channel_num1));
}

// Test that we can set the devices to use.
TEST_F(WebRtcVoiceEngineTestFake, SetDevices) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  cricket::Device default_dev(cricket::kFakeDefaultDeviceName,
                              cricket::kFakeDefaultDeviceId);
  cricket::Device dev(cricket::kFakeDeviceName,
                      cricket::kFakeDeviceId);

  // Test SetDevices() while not sending or playing.
  EXPECT_TRUE(engine_.SetDevices(&default_dev, &default_dev));

  // Test SetDevices() while sending and playing.
  EXPECT_TRUE(engine_.SetLocalMonitor(true));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_TRUE(channel_->SetPlayout(true));
  EXPECT_TRUE(voe_.GetRecordingMicrophone());
  EXPECT_TRUE(voe_.GetSend(channel_num));
  EXPECT_TRUE(voe_.GetPlayout(channel_num));

  EXPECT_TRUE(engine_.SetDevices(&dev, &dev));

  EXPECT_TRUE(voe_.GetRecordingMicrophone());
  EXPECT_TRUE(voe_.GetSend(channel_num));
  EXPECT_TRUE(voe_.GetPlayout(channel_num));

  // Test that failure to open newly selected devices does not prevent opening
  // ones after that.
  voe_.set_fail_start_recording_microphone(true);
  voe_.set_playout_fail_channel(channel_num);
  voe_.set_send_fail_channel(channel_num);

  EXPECT_FALSE(engine_.SetDevices(&default_dev, &default_dev));

  EXPECT_FALSE(voe_.GetRecordingMicrophone());
  EXPECT_FALSE(voe_.GetSend(channel_num));
  EXPECT_FALSE(voe_.GetPlayout(channel_num));

  voe_.set_fail_start_recording_microphone(false);
  voe_.set_playout_fail_channel(-1);
  voe_.set_send_fail_channel(-1);

  EXPECT_TRUE(engine_.SetDevices(&dev, &dev));

  EXPECT_TRUE(voe_.GetRecordingMicrophone());
  EXPECT_TRUE(voe_.GetSend(channel_num));
  EXPECT_TRUE(voe_.GetPlayout(channel_num));
}

// Test that we can set the devices to use even if we failed to
// open the initial ones.
TEST_F(WebRtcVoiceEngineTestFake, SetDevicesWithInitiallyBadDevices) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  cricket::Device default_dev(cricket::kFakeDefaultDeviceName,
                              cricket::kFakeDefaultDeviceId);
  cricket::Device dev(cricket::kFakeDeviceName,
                      cricket::kFakeDeviceId);

  // Test that failure to open devices selected before starting
  // send/play does not prevent opening newly selected ones after that.
  voe_.set_fail_start_recording_microphone(true);
  voe_.set_playout_fail_channel(channel_num);
  voe_.set_send_fail_channel(channel_num);

  EXPECT_TRUE(engine_.SetDevices(&default_dev, &default_dev));

  EXPECT_FALSE(engine_.SetLocalMonitor(true));
  EXPECT_FALSE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_FALSE(channel_->SetPlayout(true));
  EXPECT_FALSE(voe_.GetRecordingMicrophone());
  EXPECT_FALSE(voe_.GetSend(channel_num));
  EXPECT_FALSE(voe_.GetPlayout(channel_num));

  voe_.set_fail_start_recording_microphone(false);
  voe_.set_playout_fail_channel(-1);
  voe_.set_send_fail_channel(-1);

  EXPECT_TRUE(engine_.SetDevices(&dev, &dev));

  EXPECT_TRUE(voe_.GetRecordingMicrophone());
  EXPECT_TRUE(voe_.GetSend(channel_num));
  EXPECT_TRUE(voe_.GetPlayout(channel_num));
}

// Test that we can create a channel configured for multi-point conferences,
// and start sending/playing out on it.
TEST_F(WebRtcVoiceEngineTestFake, ConferenceSendAndPlayout) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_TRUE(voe_.GetSend(channel_num));
}

// Test that we can create a channel configured for Codian bridges,
// and start sending/playing out on it.
TEST_F(WebRtcVoiceEngineTestFake, CodianSendAndPlayout) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  webrtc::AgcConfig agc_config;
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(0, agc_config.targetLeveldBOv);
  EXPECT_TRUE(channel_->SetOptions(options_adjust_agc_));
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_TRUE(voe_.GetSend(channel_num));
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(agc_config.targetLeveldBOv, 10);  // level was attenuated
  EXPECT_TRUE(channel_->SetPlayout(true));
  EXPECT_TRUE(voe_.GetPlayout(channel_num));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_NOTHING));
  EXPECT_FALSE(voe_.GetSend(channel_num));
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(0, agc_config.targetLeveldBOv);  // level was restored
  EXPECT_TRUE(channel_->SetPlayout(false));
  EXPECT_FALSE(voe_.GetPlayout(channel_num));
}

TEST_F(WebRtcVoiceEngineTestFake, TxAgcConfigViaOptions) {
  EXPECT_TRUE(SetupEngine());
  webrtc::AgcConfig agc_config;
  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(0, agc_config.targetLeveldBOv);

  cricket::AudioOptions options;
  options.tx_agc_target_dbov.Set(3);
  options.tx_agc_digital_compression_gain.Set(9);
  options.tx_agc_limiter.Set(true);
  options.auto_gain_control.Set(true);
  EXPECT_TRUE(engine_.SetOptions(options));

  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(3, agc_config.targetLeveldBOv);
  EXPECT_EQ(9, agc_config.digitalCompressionGaindB);
  EXPECT_TRUE(agc_config.limiterEnable);

  // Check interaction with adjust_agc_delta. Both should be respected, for
  // backwards compatibility.
  options.adjust_agc_delta.Set(-10);
  EXPECT_TRUE(engine_.SetOptions(options));

  EXPECT_EQ(0, voe_.GetAgcConfig(agc_config));
  EXPECT_EQ(13, agc_config.targetLeveldBOv);
}

TEST_F(WebRtcVoiceEngineTestFake, RxAgcConfigViaOptions) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  cricket::AudioOptions options;
  options.rx_agc_target_dbov.Set(6);
  options.rx_agc_digital_compression_gain.Set(0);
  options.rx_agc_limiter.Set(true);
  options.rx_auto_gain_control.Set(true);
  EXPECT_TRUE(channel_->SetOptions(options));

  webrtc::AgcConfig agc_config;
  EXPECT_EQ(0, engine_.voe()->processing()->GetRxAgcConfig(
      channel_num, agc_config));
  EXPECT_EQ(6, agc_config.targetLeveldBOv);
  EXPECT_EQ(0, agc_config.digitalCompressionGaindB);
  EXPECT_TRUE(agc_config.limiterEnable);
}

TEST_F(WebRtcVoiceEngineTestFake, SampleRatesViaOptions) {
  EXPECT_TRUE(SetupEngine());
  cricket::AudioOptions options;
  options.recording_sample_rate.Set(48000u);
  options.playout_sample_rate.Set(44100u);
  EXPECT_TRUE(engine_.SetOptions(options));

  unsigned int recording_sample_rate, playout_sample_rate;
  EXPECT_EQ(0, voe_.RecordingSampleRate(&recording_sample_rate));
  EXPECT_EQ(0, voe_.PlayoutSampleRate(&playout_sample_rate));
  EXPECT_EQ(48000u, recording_sample_rate);
  EXPECT_EQ(44100u, playout_sample_rate);
}

TEST_F(WebRtcVoiceEngineTestFake, TraceFilterViaTraceOptions) {
  EXPECT_TRUE(SetupEngine());
  engine_.SetLogging(talk_base::LS_INFO, "");
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
      "tracefilter " + talk_base::ToString(webrtc::kTraceDefault);
  engine_.SetLogging(talk_base::LS_VERBOSE, filter.c_str());
  EXPECT_EQ(static_cast<unsigned int>(webrtc::kTraceDefault),
            trace_wrapper_->filter_);
}

// Test that we can set the outgoing SSRC properly.
// SSRC is set in SetupEngine by calling AddSendStream.
TEST_F(WebRtcVoiceEngineTestFake, SetSendSsrc) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  unsigned int send_ssrc;
  EXPECT_EQ(0, voe_.GetLocalSSRC(channel_num, send_ssrc));
  EXPECT_NE(0U, send_ssrc);
  EXPECT_EQ(0, voe_.GetLocalSSRC(channel_num, send_ssrc));
  EXPECT_EQ(kSsrc1, send_ssrc);
}

TEST_F(WebRtcVoiceEngineTestFake, GetStats) {
  // Setup. We need send codec to be set to get all stats.
  EXPECT_TRUE(SetupEngine());
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));

  cricket::VoiceMediaInfo info;
  EXPECT_EQ(true, channel_->GetStats(&info));
  EXPECT_EQ(1u, info.senders.size());
  EXPECT_EQ(kSsrc1, info.senders[0].ssrc);
  EXPECT_EQ(kPcmuCodec.name, info.senders[0].codec_name);
  EXPECT_EQ(cricket::kIntStatValue, info.senders[0].bytes_sent);
  EXPECT_EQ(cricket::kIntStatValue, info.senders[0].packets_sent);
  EXPECT_EQ(cricket::kIntStatValue, info.senders[0].packets_lost);
  EXPECT_EQ(cricket::kFractionLostStatValue, info.senders[0].fraction_lost);
  EXPECT_EQ(cricket::kIntStatValue, info.senders[0].ext_seqnum);
  EXPECT_EQ(cricket::kIntStatValue, info.senders[0].rtt_ms);
  EXPECT_EQ(cricket::kIntStatValue, info.senders[0].jitter_ms);
  // TODO(sriniv): Add testing for more fields. These are not populated
  // in FakeWebrtcVoiceEngine yet.
  // EXPECT_EQ(cricket::kIntStatValue, info.senders[0].audio_level);
  // EXPECT_EQ(cricket::kIntStatValue, info.senders[0].echo_delay_median_ms);
  // EXPECT_EQ(cricket::kIntStatValue, info.senders[0].echo_delay_std_ms);
  // EXPECT_EQ(cricket::kIntStatValue, info.senders[0].echo_return_loss);
  // EXPECT_EQ(cricket::kIntStatValue,
  //           info.senders[0].echo_return_loss_enhancement);

  EXPECT_EQ(1u, info.receivers.size());
  // TODO(sriniv): Add testing for receiver fields.
}

// Test that we can set the outgoing SSRC properly with multiple streams.
// SSRC is set in SetupEngine by calling AddSendStream.
TEST_F(WebRtcVoiceEngineTestFake, SetSendSsrcWithMultipleStreams) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  int channel_num1 = voe_.GetLastChannel();
  unsigned int send_ssrc;
  EXPECT_EQ(0, voe_.GetLocalSSRC(channel_num1, send_ssrc));
  EXPECT_EQ(kSsrc1, send_ssrc);

  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int channel_num2 = voe_.GetLastChannel();
  EXPECT_EQ(0, voe_.GetLocalSSRC(channel_num2, send_ssrc));
  EXPECT_EQ(kSsrc1, send_ssrc);
}

// Test that the local SSRC is the same on sending and receiving channels if the
// receive channel is created before the send channel.
TEST_F(WebRtcVoiceEngineTestFake, SetSendSsrcAfterCreatingReceiveChannel) {
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  channel_ = engine_.CreateChannel();
  EXPECT_TRUE(channel_->SetOptions(options_conference_));

  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  int receive_channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddSendStream(
      cricket::StreamParams::CreateLegacy(1234)));
  int send_channel_num = voe_.GetLastChannel();

  unsigned int ssrc = 0;
  EXPECT_EQ(0, voe_.GetLocalSSRC(send_channel_num, ssrc));
  EXPECT_EQ(1234U, ssrc);
  ssrc = 0;
  EXPECT_EQ(0, voe_.GetLocalSSRC(receive_channel_num, ssrc));
  EXPECT_EQ(1234U, ssrc);
}

// Test that we can properly receive packets.
TEST_F(WebRtcVoiceEngineTestFake, Recv) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  EXPECT_TRUE(voe_.CheckPacket(channel_num, kPcmuFrame,
                               sizeof(kPcmuFrame)));
}

// Test that we can properly receive packets on multiple streams.
TEST_F(WebRtcVoiceEngineTestFake, RecvWithMultipleStreams) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
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
    talk_base::SetBE32(packets[i] + 8, i);
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

// Test that we properly handle failures to add a stream.
TEST_F(WebRtcVoiceEngineTestFake, AddStreamFail) {
  EXPECT_TRUE(SetupEngine());
  voe_.set_fail_create_channel(true);
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  EXPECT_FALSE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));

  // In 1:1 call, we should not try to create a new channel.
  cricket::AudioOptions options_no_conference_;
  options_no_conference_.conference_mode.Set(false);
  EXPECT_TRUE(channel_->SetOptions(options_no_conference_));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
}

// Test that AddRecvStream doesn't create new channel for 1:1 call.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvStream1On1) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_EQ(channel_num, voe_.GetLastChannel());
}

// Test that after adding a recv stream, we do not decode more codecs than
// those previously passed into SetRecvCodecs.
TEST_F(WebRtcVoiceEngineTestFake, AddRecvStreamUnsupportedCodec) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kIsacCodec);
  codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetRecvCodecs(codecs));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc1)));
  int channel_num2 = voe_.GetLastChannel();
  webrtc::CodecInst gcodec;
  talk_base::strcpyn(gcodec.plname, ARRAY_SIZE(gcodec.plname), "CELT");
  gcodec.plfreq = 32000;
  gcodec.channels = 2;
  EXPECT_EQ(-1, voe_.GetRecPayloadType(channel_num2, gcodec));
}

// Test that we properly clean up any streams that were added, even if
// not explicitly removed.
TEST_F(WebRtcVoiceEngineTestFake, StreamCleanup) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  EXPECT_EQ(3, voe_.GetNumChannels());  // default channel + 2 added
  delete channel_;
  channel_ = NULL;
  EXPECT_EQ(0, voe_.GetNumChannels());
}

TEST_F(WebRtcVoiceEngineTestFake, TestAddRecvStreamFailWithZeroSsrc) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_FALSE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(0)));
}

TEST_F(WebRtcVoiceEngineTestFake, TestNoLeakingWhenAddRecvStreamFail) {
  EXPECT_TRUE(SetupEngine());
  // Stream 1 reuses default channel.
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  // Manually delete default channel to simulate a failure.
  int default_channel = voe_.GetLastChannel();
  EXPECT_EQ(0, voe_.DeleteChannel(default_channel));
  // Add recv stream 2 should fail because default channel is gone.
  EXPECT_FALSE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int new_channel = voe_.GetLastChannel();
  EXPECT_NE(default_channel, new_channel);
  // The last created channel should have already been deleted.
  EXPECT_EQ(-1, voe_.DeleteChannel(new_channel));
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

// Test that we can play a ringback tone properly in a single-stream call.
TEST_F(WebRtcVoiceEngineTestFake, PlayRingback) {
  EXPECT_TRUE(SetupEngine());
  int channel_num = voe_.GetLastChannel();
  EXPECT_EQ(0, voe_.IsPlayingFileLocally(channel_num));
  // Check we fail if no ringback tone specified.
  EXPECT_FALSE(channel_->PlayRingbackTone(0, true, true));
  EXPECT_EQ(0, voe_.IsPlayingFileLocally(channel_num));
  // Check we can set and play a ringback tone.
  EXPECT_TRUE(channel_->SetRingbackTone(kRingbackTone, strlen(kRingbackTone)));
  EXPECT_TRUE(channel_->PlayRingbackTone(0, true, true));
  EXPECT_EQ(1, voe_.IsPlayingFileLocally(channel_num));
  // Check we can stop the tone manually.
  EXPECT_TRUE(channel_->PlayRingbackTone(0, false, false));
  EXPECT_EQ(0, voe_.IsPlayingFileLocally(channel_num));
  // Check we stop the tone if a packet arrives.
  EXPECT_TRUE(channel_->PlayRingbackTone(0, true, true));
  EXPECT_EQ(1, voe_.IsPlayingFileLocally(channel_num));
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  EXPECT_EQ(0, voe_.IsPlayingFileLocally(channel_num));
}

// Test that we can play a ringback tone properly in a multi-stream call.
TEST_F(WebRtcVoiceEngineTestFake, PlayRingbackWithMultipleStreams) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  int channel_num = voe_.GetLastChannel();
  EXPECT_EQ(0, voe_.IsPlayingFileLocally(channel_num));
  // Check we fail if no ringback tone specified.
  EXPECT_FALSE(channel_->PlayRingbackTone(2, true, true));
  EXPECT_EQ(0, voe_.IsPlayingFileLocally(channel_num));
  // Check we can set and play a ringback tone on the correct ssrc.
  EXPECT_TRUE(channel_->SetRingbackTone(kRingbackTone, strlen(kRingbackTone)));
  EXPECT_FALSE(channel_->PlayRingbackTone(77, true, true));
  EXPECT_TRUE(channel_->PlayRingbackTone(2, true, true));
  EXPECT_EQ(1, voe_.IsPlayingFileLocally(channel_num));
  // Check we can stop the tone manually.
  EXPECT_TRUE(channel_->PlayRingbackTone(2, false, false));
  EXPECT_EQ(0, voe_.IsPlayingFileLocally(channel_num));
  // Check we stop the tone if a packet arrives, but only with the right SSRC.
  EXPECT_TRUE(channel_->PlayRingbackTone(2, true, true));
  EXPECT_EQ(1, voe_.IsPlayingFileLocally(channel_num));
  // Send a packet with SSRC 1; the tone should not stop.
  DeliverPacket(kPcmuFrame, sizeof(kPcmuFrame));
  EXPECT_EQ(1, voe_.IsPlayingFileLocally(channel_num));
  // Send a packet with SSRC 2; the tone should stop.
  char packet[sizeof(kPcmuFrame)];
  memcpy(packet, kPcmuFrame, sizeof(kPcmuFrame));
  talk_base::SetBE32(packet + 8, 2);
  DeliverPacket(packet, sizeof(packet));
  EXPECT_EQ(0, voe_.IsPlayingFileLocally(channel_num));
}

// Tests creating soundclips, and make sure they come from the right engine.
TEST_F(WebRtcVoiceEngineTestFake, CreateSoundclip) {
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  EXPECT_FALSE(voe_sc_.IsInited());
  soundclip_ = engine_.CreateSoundclip();
  EXPECT_TRUE(voe_sc_.IsInited());
  ASSERT_TRUE(soundclip_ != NULL);
  EXPECT_EQ(0, voe_.GetNumChannels());
  EXPECT_EQ(1, voe_sc_.GetNumChannels());
  int channel_num = voe_sc_.GetLastChannel();
  EXPECT_TRUE(voe_sc_.GetPlayout(channel_num));
  delete soundclip_;
  soundclip_ = NULL;
  EXPECT_EQ(0, voe_sc_.GetNumChannels());
  // Make sure the soundclip engine is uninitialized on shutdown, now that
  // we've initialized it by creating a soundclip.
  engine_.Terminate();
  EXPECT_FALSE(voe_sc_.IsInited());
}

// Tests playing out a fake sound.
TEST_F(WebRtcVoiceEngineTestFake, PlaySoundclip) {
  static const char kZeroes[16000] = {};
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));
  soundclip_ = engine_.CreateSoundclip();
  ASSERT_TRUE(soundclip_ != NULL);
  EXPECT_TRUE(soundclip_->PlaySound(kZeroes, sizeof(kZeroes), 0));
}

TEST_F(WebRtcVoiceEngineTestFake, MediaEngineCallbackOnError) {
  talk_base::scoped_ptr<ChannelErrorListener> listener;
  cricket::WebRtcVoiceMediaChannel* media_channel;
  unsigned int ssrc = 0;

  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));

  media_channel = static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  listener.reset(new ChannelErrorListener(channel_));

  // Test on WebRtc VoE channel.
  voe_.TriggerCallbackOnError(media_channel->voe_channel(),
                              VE_SATURATION_WARNING);
  EXPECT_EQ(cricket::VoiceMediaChannel::ERROR_REC_DEVICE_SATURATION,
            listener->error());
  EXPECT_NE(-1, voe_.GetLocalSSRC(voe_.GetLastChannel(), ssrc));
  EXPECT_EQ(ssrc, listener->ssrc());

  listener->Reset();
  voe_.TriggerCallbackOnError(-1, VE_TYPING_NOISE_WARNING);
  EXPECT_EQ(cricket::VoiceMediaChannel::ERROR_REC_TYPING_NOISE_DETECTED,
            listener->error());
  EXPECT_EQ(0U, listener->ssrc());

  // Add another stream and test on that.
  ++ssrc;
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(
      ssrc)));
  listener->Reset();
  voe_.TriggerCallbackOnError(voe_.GetLastChannel(),
                              VE_SATURATION_WARNING);
  EXPECT_EQ(cricket::VoiceMediaChannel::ERROR_REC_DEVICE_SATURATION,
            listener->error());
  EXPECT_EQ(ssrc, listener->ssrc());

  // Testing a non-existing channel.
  listener->Reset();
  voe_.TriggerCallbackOnError(voe_.GetLastChannel() + 2,
                              VE_SATURATION_WARNING);
  EXPECT_EQ(0, listener->error());
}

TEST_F(WebRtcVoiceEngineTestFake, TestSetPlayoutError) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  std::vector<cricket::AudioCodec> codecs;
  codecs.push_back(kPcmuCodec);
  EXPECT_TRUE(channel_->SetSendCodecs(codecs));
  EXPECT_TRUE(channel_->SetSend(cricket::SEND_MICROPHONE));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(2)));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(3)));
  EXPECT_TRUE(channel_->SetPlayout(true));
  voe_.set_playout_fail_channel(voe_.GetLastChannel() - 1);
  EXPECT_TRUE(channel_->SetPlayout(false));
  EXPECT_FALSE(channel_->SetPlayout(true));
}

// Test that the Registering/Unregistering with the
// webrtcvoiceengine works as expected
TEST_F(WebRtcVoiceEngineTestFake, RegisterVoiceProcessor) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  EXPECT_TRUE(channel_->AddRecvStream(
      cricket::StreamParams::CreateLegacy(kSsrc2)));
  cricket::FakeMediaProcessor vp_1;
  cricket::FakeMediaProcessor vp_2;

  EXPECT_FALSE(engine_.RegisterProcessor(kSsrc2, &vp_1, cricket::MPD_TX));
  EXPECT_TRUE(engine_.RegisterProcessor(kSsrc2, &vp_1, cricket::MPD_RX));
  EXPECT_TRUE(engine_.RegisterProcessor(kSsrc2, &vp_2, cricket::MPD_RX));
  voe_.TriggerProcessPacket(cricket::MPD_RX);
  voe_.TriggerProcessPacket(cricket::MPD_TX);

  EXPECT_TRUE(voe_.IsExternalMediaProcessorRegistered());
  EXPECT_EQ(1, vp_1.voice_frame_count());
  EXPECT_EQ(1, vp_2.voice_frame_count());

  EXPECT_TRUE(engine_.UnregisterProcessor(kSsrc2,
                                          &vp_2,
                                          cricket::MPD_RX));
  voe_.TriggerProcessPacket(cricket::MPD_RX);
  EXPECT_TRUE(voe_.IsExternalMediaProcessorRegistered());
  EXPECT_EQ(1, vp_2.voice_frame_count());
  EXPECT_EQ(2, vp_1.voice_frame_count());

  EXPECT_TRUE(engine_.UnregisterProcessor(kSsrc2,
                                          &vp_1,
                                          cricket::MPD_RX));
  voe_.TriggerProcessPacket(cricket::MPD_RX);
  EXPECT_FALSE(voe_.IsExternalMediaProcessorRegistered());
  EXPECT_EQ(2, vp_1.voice_frame_count());

  EXPECT_FALSE(engine_.RegisterProcessor(kSsrc1, &vp_1, cricket::MPD_RX));
  EXPECT_TRUE(engine_.RegisterProcessor(kSsrc1, &vp_1, cricket::MPD_TX));
  voe_.TriggerProcessPacket(cricket::MPD_RX);
  voe_.TriggerProcessPacket(cricket::MPD_TX);
  EXPECT_TRUE(voe_.IsExternalMediaProcessorRegistered());
  EXPECT_EQ(3, vp_1.voice_frame_count());

  EXPECT_TRUE(engine_.UnregisterProcessor(kSsrc1,
                                          &vp_1,
                                          cricket::MPD_RX_AND_TX));
  voe_.TriggerProcessPacket(cricket::MPD_TX);
  EXPECT_FALSE(voe_.IsExternalMediaProcessorRegistered());
  EXPECT_EQ(3, vp_1.voice_frame_count());
  EXPECT_TRUE(channel_->RemoveRecvStream(kSsrc2));
  EXPECT_FALSE(engine_.RegisterProcessor(kSsrc2, &vp_1, cricket::MPD_RX));
  EXPECT_FALSE(voe_.IsExternalMediaProcessorRegistered());

  // Test that we can register a processor on the receive channel on SSRC 0.
  // This tests the 1:1 case when the receive SSRC is unknown.
  EXPECT_TRUE(engine_.RegisterProcessor(0, &vp_1, cricket::MPD_RX));
  voe_.TriggerProcessPacket(cricket::MPD_RX);
  EXPECT_TRUE(voe_.IsExternalMediaProcessorRegistered());
  EXPECT_EQ(4, vp_1.voice_frame_count());
  EXPECT_TRUE(engine_.UnregisterProcessor(0,
                                          &vp_1,
                                          cricket::MPD_RX));

  // The following tests test that FindChannelNumFromSsrc is doing
  // what we expect.
  // pick an invalid ssrc and make sure we can't register
  EXPECT_FALSE(engine_.RegisterProcessor(99,
                                         &vp_1,
                                         cricket::MPD_RX));
  EXPECT_TRUE(channel_->AddRecvStream(cricket::StreamParams::CreateLegacy(1)));
  EXPECT_TRUE(engine_.RegisterProcessor(1,
                                        &vp_1,
                                        cricket::MPD_RX));
  EXPECT_TRUE(engine_.UnregisterProcessor(1,
                                          &vp_1,
                                          cricket::MPD_RX));
  EXPECT_FALSE(engine_.RegisterProcessor(1,
                                         &vp_1,
                                         cricket::MPD_TX));
  EXPECT_TRUE(channel_->RemoveRecvStream(1));
}

TEST_F(WebRtcVoiceEngineTestFake, SetAudioOptions) {
  EXPECT_TRUE(SetupEngine());

  bool ec_enabled;
  webrtc::EcModes ec_mode;
  bool ec_metrics_enabled;
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
  voe_.GetEcMetricsStatus(ec_metrics_enabled);
  voe_.GetAecmMode(aecm_mode, cng_enabled);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetAgcConfig(agc_config);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(ec_metrics_enabled);
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
  voe_.GetEcMetricsStatus(ec_metrics_enabled);
  voe_.GetAecmMode(aecm_mode, cng_enabled);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetAgcConfig(agc_config);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(ec_metrics_enabled);
  EXPECT_FALSE(cng_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_EQ(0, agc_config.targetLeveldBOv);
  EXPECT_TRUE(ns_enabled);
  EXPECT_TRUE(highpass_filter_enabled);
  EXPECT_FALSE(stereo_swapping_enabled);
  EXPECT_TRUE(typing_detection_enabled);
  EXPECT_EQ(ec_mode, webrtc::kEcConference);
  EXPECT_EQ(ns_mode, webrtc::kNsHighSuppression);

  // Turn echo cancellation off
  options.echo_cancellation.Set(false);
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetEcStatus(ec_enabled, ec_mode);
  EXPECT_FALSE(ec_enabled);

  // Turn echo cancellation back on, with settings, and make sure
  // nothing else changed.
  options.echo_cancellation.Set(true);
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetEcMetricsStatus(ec_metrics_enabled);
  voe_.GetAecmMode(aecm_mode, cng_enabled);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetAgcConfig(agc_config);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_TRUE(ec_enabled);
  EXPECT_TRUE(ec_metrics_enabled);
  EXPECT_TRUE(agc_enabled);
  EXPECT_EQ(0, agc_config.targetLeveldBOv);
  EXPECT_TRUE(ns_enabled);
  EXPECT_TRUE(highpass_filter_enabled);
  EXPECT_FALSE(stereo_swapping_enabled);
  EXPECT_TRUE(typing_detection_enabled);
  EXPECT_EQ(ec_mode, webrtc::kEcConference);
  EXPECT_EQ(ns_mode, webrtc::kNsHighSuppression);

  // Turn off AGC
  options.auto_gain_control.Set(false);
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  EXPECT_FALSE(agc_enabled);

  // Turn AGC back on
  options.auto_gain_control.Set(true);
  options.adjust_agc_delta.Clear();
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  EXPECT_TRUE(agc_enabled);
  voe_.GetAgcConfig(agc_config);
  EXPECT_EQ(0, agc_config.targetLeveldBOv);

  // Turn off other options (and stereo swapping on).
  options.noise_suppression.Set(false);
  options.highpass_filter.Set(false);
  options.typing_detection.Set(false);
  options.stereo_swapping.Set(true);
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetNsStatus(ns_enabled, ns_mode);
  highpass_filter_enabled = voe_.IsHighPassFilterEnabled();
  stereo_swapping_enabled = voe_.IsStereoChannelSwappingEnabled();
  voe_.GetTypingDetectionStatus(typing_detection_enabled);
  EXPECT_FALSE(ns_enabled);
  EXPECT_FALSE(highpass_filter_enabled);
  EXPECT_FALSE(typing_detection_enabled);
  EXPECT_TRUE(stereo_swapping_enabled);

  // Turn on "conference mode" to ensure it has no impact.
  options.conference_mode.Set(true);
  ASSERT_TRUE(engine_.SetOptions(options));
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_EQ(webrtc::kEcConference, ec_mode);
  EXPECT_FALSE(ns_enabled);
  EXPECT_EQ(webrtc::kNsHighSuppression, ns_mode);
}

TEST_F(WebRtcVoiceEngineTestFake, DefaultOptions) {
  EXPECT_TRUE(SetupEngine());

  bool ec_enabled;
  webrtc::EcModes ec_mode;
  bool ec_metrics_enabled;
  bool agc_enabled;
  webrtc::AgcModes agc_mode;
  bool ns_enabled;
  webrtc::NsModes ns_mode;
  bool highpass_filter_enabled;
  bool stereo_swapping_enabled;
  bool typing_detection_enabled;

  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetEcMetricsStatus(ec_metrics_enabled);
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
  EXPECT_TRUE(engine_.Init(talk_base::Thread::Current()));

  webrtc::AgcConfig config = {0};
  EXPECT_EQ(0, voe_.GetAgcConfig(config));
  EXPECT_EQ(set_config.targetLeveldBOv, config.targetLeveldBOv);
  EXPECT_EQ(set_config.digitalCompressionGaindB,
            config.digitalCompressionGaindB);
  EXPECT_EQ(set_config.limiterEnable, config.limiterEnable);
}


TEST_F(WebRtcVoiceEngineTestFake, SetOptionOverridesViaChannels) {
  EXPECT_TRUE(SetupEngine());
  talk_base::scoped_ptr<cricket::VoiceMediaChannel> channel1(
      engine_.CreateChannel());
  talk_base::scoped_ptr<cricket::VoiceMediaChannel> channel2(
      engine_.CreateChannel());

  // Have to add a stream to make SetSend work.
  cricket::StreamParams stream1;
  stream1.ssrcs.push_back(1);
  channel1->AddSendStream(stream1);
  cricket::StreamParams stream2;
  stream2.ssrcs.push_back(2);
  channel2->AddSendStream(stream2);

  // AEC and AGC and NS
  cricket::AudioOptions options_all;
  options_all.echo_cancellation.Set(true);
  options_all.auto_gain_control.Set(true);
  options_all.noise_suppression.Set(true);

  ASSERT_TRUE(channel1->SetOptions(options_all));
  cricket::AudioOptions expected_options = options_all;
  cricket::AudioOptions actual_options;
  ASSERT_TRUE(channel1->GetOptions(&actual_options));
  EXPECT_EQ(expected_options, actual_options);
  ASSERT_TRUE(channel2->SetOptions(options_all));
  ASSERT_TRUE(channel2->GetOptions(&actual_options));
  EXPECT_EQ(expected_options, actual_options);

  // unset NS
  cricket::AudioOptions options_no_ns;
  options_no_ns.noise_suppression.Set(false);
  ASSERT_TRUE(channel1->SetOptions(options_no_ns));

  expected_options.echo_cancellation.Set(true);
  expected_options.auto_gain_control.Set(true);
  expected_options.noise_suppression.Set(false);
  ASSERT_TRUE(channel1->GetOptions(&actual_options));
  EXPECT_EQ(expected_options, actual_options);

  // unset AGC
  cricket::AudioOptions options_no_agc;
  options_no_agc.auto_gain_control.Set(false);
  ASSERT_TRUE(channel2->SetOptions(options_no_agc));

  expected_options.echo_cancellation.Set(true);
  expected_options.auto_gain_control.Set(false);
  expected_options.noise_suppression.Set(true);
  ASSERT_TRUE(channel2->GetOptions(&actual_options));
  EXPECT_EQ(expected_options, actual_options);

  ASSERT_TRUE(engine_.SetOptions(options_all));
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
  ASSERT_TRUE(engine_.SetOptions(options_all));
  cricket::AudioOptions options_no_agc_nor_ns;
  options_no_agc_nor_ns.auto_gain_control.Set(false);
  options_no_agc_nor_ns.noise_suppression.Set(false);
  channel2->SetSend(cricket::SEND_MICROPHONE);
  channel2->SetOptions(options_no_agc_nor_ns);

  expected_options.echo_cancellation.Set(true);
  expected_options.auto_gain_control.Set(false);
  expected_options.noise_suppression.Set(false);
  ASSERT_TRUE(channel2->GetOptions(&actual_options));
  EXPECT_EQ(expected_options, actual_options);
  voe_.GetEcStatus(ec_enabled, ec_mode);
  voe_.GetAgcStatus(agc_enabled, agc_mode);
  voe_.GetNsStatus(ns_enabled, ns_mode);
  EXPECT_TRUE(ec_enabled);
  EXPECT_FALSE(agc_enabled);
  EXPECT_FALSE(ns_enabled);
}

// This test verifies DSCP settings are properly applied on voice media channel.
TEST_F(WebRtcVoiceEngineTestFake, TestSetDscpOptions) {
  EXPECT_TRUE(SetupEngine());
  talk_base::scoped_ptr<cricket::VoiceMediaChannel> channel(
      engine_.CreateChannel());
  talk_base::scoped_ptr<cricket::FakeNetworkInterface> network_interface(
      new cricket::FakeNetworkInterface);
  channel->SetInterface(network_interface.get());
  cricket::AudioOptions options;
  options.dscp.Set(true);
  EXPECT_TRUE(channel->SetOptions(options));
  EXPECT_EQ(talk_base::DSCP_EF, network_interface->dscp());
  options.dscp.Set(false);
  EXPECT_TRUE(channel->SetOptions(options));
  EXPECT_EQ(talk_base::DSCP_DEFAULT, network_interface->dscp());
}

TEST(WebRtcVoiceEngineTest, TestDefaultOptionsBeforeInit) {
  cricket::WebRtcVoiceEngine engine;
  cricket::AudioOptions options = engine.GetOptions();
  // The default options should have at least a few things set. We purposefully
  // don't check the option values here, though.
  EXPECT_TRUE(options.echo_cancellation.IsSet());
  EXPECT_TRUE(options.auto_gain_control.IsSet());
  EXPECT_TRUE(options.noise_suppression.IsSet());
}

// Test that GetReceiveChannelNum returns the default channel for the first
// recv stream in 1-1 calls.
TEST_F(WebRtcVoiceEngineTestFake, TestGetReceiveChannelNumIn1To1Calls) {
  EXPECT_TRUE(SetupEngine());
  cricket::WebRtcVoiceMediaChannel* media_channel =
        static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  // Test that GetChannelNum returns the default channel if the SSRC is unknown.
  EXPECT_EQ(media_channel->voe_channel(),
            media_channel->GetReceiveChannelNum(0));
  cricket::StreamParams stream;
  stream.ssrcs.push_back(kSsrc2);
  EXPECT_TRUE(channel_->AddRecvStream(stream));
  EXPECT_EQ(media_channel->voe_channel(),
            media_channel->GetReceiveChannelNum(kSsrc2));
}

// Test that GetReceiveChannelNum doesn't return the default channel for the
// first recv stream in conference calls.
TEST_F(WebRtcVoiceEngineTestFake, TestGetChannelNumInConferenceCalls) {
  EXPECT_TRUE(SetupEngine());
  EXPECT_TRUE(channel_->SetOptions(options_conference_));
  cricket::StreamParams stream;
  stream.ssrcs.push_back(kSsrc2);
  EXPECT_TRUE(channel_->AddRecvStream(stream));
  cricket::WebRtcVoiceMediaChannel* media_channel =
      static_cast<cricket::WebRtcVoiceMediaChannel*>(channel_);
  EXPECT_LT(media_channel->voe_channel(),
            media_channel->GetReceiveChannelNum(kSsrc2));
}

TEST_F(WebRtcVoiceEngineTestFake, SetOutputScaling) {
  EXPECT_TRUE(SetupEngine());
  double left, right;
  EXPECT_TRUE(channel_->SetOutputScaling(0, 1, 2));
  EXPECT_TRUE(channel_->GetOutputScaling(0, &left, &right));
  EXPECT_DOUBLE_EQ(1, left);
  EXPECT_DOUBLE_EQ(2, right);

  EXPECT_FALSE(channel_->SetOutputScaling(kSsrc2, 1, 2));
  cricket::StreamParams stream;
  stream.ssrcs.push_back(kSsrc2);
  EXPECT_TRUE(channel_->AddRecvStream(stream));

  EXPECT_TRUE(channel_->SetOutputScaling(kSsrc2, 2, 1));
  EXPECT_TRUE(channel_->GetOutputScaling(kSsrc2, &left, &right));
  EXPECT_DOUBLE_EQ(2, left);
  EXPECT_DOUBLE_EQ(1, right);
}


// Tests for the actual WebRtc VoE library.

// Tests that the library initializes and shuts down properly.
TEST(WebRtcVoiceEngineTest, StartupShutdown) {
  cricket::WebRtcVoiceEngine engine;
  EXPECT_TRUE(engine.Init(talk_base::Thread::Current()));
  cricket::VoiceMediaChannel* channel = engine.CreateChannel();
  EXPECT_TRUE(channel != NULL);
  delete channel;
  engine.Terminate();

  // Reinit to catch regression where VoiceEngineObserver reference is lost
  EXPECT_TRUE(engine.Init(talk_base::Thread::Current()));
  engine.Terminate();
}

// Tests that the logging from the library is cleartext.
TEST(WebRtcVoiceEngineTest, DISABLED_HasUnencryptedLogging) {
  cricket::WebRtcVoiceEngine engine;
  talk_base::scoped_ptr<talk_base::MemoryStream> stream(
      new talk_base::MemoryStream);
  size_t size = 0;
  bool cleartext = true;
  talk_base::LogMessage::AddLogToStream(stream.get(), talk_base::LS_VERBOSE);
  engine.SetLogging(talk_base::LS_VERBOSE, "");
  EXPECT_TRUE(engine.Init(talk_base::Thread::Current()));
  EXPECT_TRUE(stream->GetSize(&size));
  EXPECT_GT(size, 0U);
  engine.Terminate();
  talk_base::LogMessage::RemoveLogToStream(stream.get());
  const char* buf = stream->GetBuffer();
  for (size_t i = 0; i < size && cleartext; ++i) {
    int ch = static_cast<int>(buf[i]);
    ASSERT_GE(ch, 0) << "Out of bounds character in WebRtc VoE log: "
                     << std::hex << ch;
    cleartext = (isprint(ch) || isspace(ch));
  }
  EXPECT_TRUE(cleartext);
}

// Tests we do not see any references to a monitor thread being spun up
// when initiating the engine.
TEST(WebRtcVoiceEngineTest, HasNoMonitorThread) {
  cricket::WebRtcVoiceEngine engine;
  talk_base::scoped_ptr<talk_base::MemoryStream> stream(
      new talk_base::MemoryStream);
  talk_base::LogMessage::AddLogToStream(stream.get(), talk_base::LS_VERBOSE);
  engine.SetLogging(talk_base::LS_VERBOSE, "");
  EXPECT_TRUE(engine.Init(talk_base::Thread::Current()));
  engine.Terminate();
  talk_base::LogMessage::RemoveLogToStream(stream.get());

  size_t size = 0;
  EXPECT_TRUE(stream->GetSize(&size));
  EXPECT_GT(size, 0U);
  const std::string logs(stream->GetBuffer());
  EXPECT_NE(std::string::npos, logs.find("ProcessThread"));
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
      cricket::AudioCodec(96, "G722", 16000, 0, 1, 0)));
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
      cricket::AudioCodec(9, "", 16000, 0, 1, 0)));  // G722
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
  // Check that there aren't any extra codecs lying around.
  EXPECT_EQ(13U, engine.codecs().size());
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
    } else if (it->name == "G722" && it->clockrate == 16000) {
      EXPECT_EQ(9, it->id);
    } else if (it->name == "telephone-event") {
      EXPECT_EQ(126, it->id);
    } else if (it->name == "red") {
      EXPECT_EQ(127, it->id);
    } else if (it->name == "opus") {
      EXPECT_EQ(111, it->id);
      ASSERT_NE(it->params.find("minptime"), it->params.end());
      EXPECT_EQ("10", it->params.find("minptime")->second);
      ASSERT_NE(it->params.find("maxptime"), it->params.end());
      EXPECT_EQ("60", it->params.find("maxptime")->second);
    }
  }

  engine.Terminate();
}

// Tests that VoE supports at least 32 channels
TEST(WebRtcVoiceEngineTest, Has32Channels) {
  cricket::WebRtcVoiceEngine engine;
  EXPECT_TRUE(engine.Init(talk_base::Thread::Current()));

  cricket::VoiceMediaChannel* channels[32];
  int num_channels = 0;

  while (num_channels < ARRAY_SIZE(channels)) {
    cricket::VoiceMediaChannel* channel = engine.CreateChannel();
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
  EXPECT_TRUE(engine.Init(talk_base::Thread::Current()));
  cricket::WebRtcVoiceMediaChannel channel(&engine);
  EXPECT_TRUE(channel.SetRecvCodecs(engine.codecs()));
}

#ifdef WIN32
// Test our workarounds to WebRtc VoE' munging of the coinit count
TEST(WebRtcVoiceEngineTest, CoInitialize) {
  cricket::WebRtcVoiceEngine* engine = new cricket::WebRtcVoiceEngine();

  // Initial refcount should be 0.
  EXPECT_EQ(S_OK, CoInitializeEx(NULL, COINIT_MULTITHREADED));

  // Engine should start even with COM already inited.
  EXPECT_TRUE(engine->Init(talk_base::Thread::Current()));
  engine->Terminate();
  EXPECT_TRUE(engine->Init(talk_base::Thread::Current()));
  engine->Terminate();

  // Refcount after terminate should be 1 (in reality 3); test if it is nonzero.
  EXPECT_EQ(S_FALSE, CoInitializeEx(NULL, COINIT_MULTITHREADED));
  // Decrement refcount to (hopefully) 0.
  CoUninitialize();
  CoUninitialize();
  delete engine;

  // Ensure refcount is 0.
  EXPECT_EQ(S_OK, CoInitializeEx(NULL, COINIT_MULTITHREADED));
  CoUninitialize();
}
#endif


