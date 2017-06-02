/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <list>
#include <map>
#include <memory>
#include <utility>

#include "webrtc/api/test/mock_audio_mixer.h"
#include "webrtc/base/ptr_util.h"
#include "webrtc/call/audio_state.h"
#include "webrtc/call/call.h"
#include "webrtc/call/fake_rtp_transport_controller_send.h"
#include "webrtc/logging/rtc_event_log/rtc_event_log.h"
#include "webrtc/modules/audio_device/include/mock_audio_device.h"
#include "webrtc/modules/audio_mixer/audio_mixer_impl.h"
#include "webrtc/modules/congestion_controller/include/mock/mock_send_side_congestion_controller.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/mock_audio_decoder_factory.h"
#include "webrtc/test/mock_transport.h"
#include "webrtc/test/mock_voice_engine.h"

namespace {

struct CallHelper {
  explicit CallHelper(
      rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory = nullptr)
      : voice_engine_(decoder_factory) {
    webrtc::AudioState::Config audio_state_config;
    audio_state_config.voice_engine = &voice_engine_;
    audio_state_config.audio_mixer = webrtc::AudioMixerImpl::Create();
    EXPECT_CALL(voice_engine_, audio_device_module());
    EXPECT_CALL(voice_engine_, audio_processing());
    EXPECT_CALL(voice_engine_, audio_transport());
    webrtc::Call::Config config(&event_log_);
    config.audio_state = webrtc::AudioState::Create(audio_state_config);
    call_.reset(webrtc::Call::Create(config));
  }

  webrtc::Call* operator->() { return call_.get(); }
  webrtc::test::MockVoiceEngine* voice_engine() { return &voice_engine_; }

 private:
  testing::NiceMock<webrtc::test::MockVoiceEngine> voice_engine_;
  webrtc::RtcEventLogNullImpl event_log_;
  std::unique_ptr<webrtc::Call> call_;
};
}  // namespace

namespace webrtc {

TEST(CallTest, ConstructDestruct) {
  CallHelper call;
}

TEST(CallTest, CreateDestroy_AudioSendStream) {
  CallHelper call;
  AudioSendStream::Config config(nullptr);
  config.rtp.ssrc = 42;
  config.voe_channel_id = 123;
  AudioSendStream* stream = call->CreateAudioSendStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyAudioSendStream(stream);
}

TEST(CallTest, CreateDestroy_AudioReceiveStream) {
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory(
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>);
  CallHelper call(decoder_factory);
  AudioReceiveStream::Config config;
  config.rtp.remote_ssrc = 42;
  config.voe_channel_id = 123;
  config.decoder_factory = decoder_factory;
  AudioReceiveStream* stream = call->CreateAudioReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyAudioReceiveStream(stream);
}

TEST(CallTest, CreateDestroy_AudioSendStreams) {
  CallHelper call;
  AudioSendStream::Config config(nullptr);
  config.voe_channel_id = 123;
  std::list<AudioSendStream*> streams;
  for (int i = 0; i < 2; ++i) {
    for (uint32_t ssrc = 0; ssrc < 1234567; ssrc += 34567) {
      config.rtp.ssrc = ssrc;
      AudioSendStream* stream = call->CreateAudioSendStream(config);
      EXPECT_NE(stream, nullptr);
      if (ssrc & 1) {
        streams.push_back(stream);
      } else {
        streams.push_front(stream);
      }
    }
    for (auto s : streams) {
      call->DestroyAudioSendStream(s);
    }
    streams.clear();
  }
}

TEST(CallTest, CreateDestroy_AudioReceiveStreams) {
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory(
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>);
  CallHelper call(decoder_factory);
  AudioReceiveStream::Config config;
  config.voe_channel_id = 123;
  config.decoder_factory = decoder_factory;
  std::list<AudioReceiveStream*> streams;
  for (int i = 0; i < 2; ++i) {
    for (uint32_t ssrc = 0; ssrc < 1234567; ssrc += 34567) {
      config.rtp.remote_ssrc = ssrc;
      AudioReceiveStream* stream = call->CreateAudioReceiveStream(config);
      EXPECT_NE(stream, nullptr);
      if (ssrc & 1) {
        streams.push_back(stream);
      } else {
        streams.push_front(stream);
      }
    }
    for (auto s : streams) {
      call->DestroyAudioReceiveStream(s);
    }
    streams.clear();
  }
}

TEST(CallTest, CreateDestroy_AssociateAudioSendReceiveStreams_RecvFirst) {
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory(
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>);
  CallHelper call(decoder_factory);
  ::testing::NiceMock<MockRtpRtcp> mock_rtp_rtcp;

  constexpr int kRecvChannelId = 101;

  // Set up the mock to create a channel proxy which we know of, so that we can
  // add our expectations to it.
  test::MockVoEChannelProxy* recv_channel_proxy = nullptr;
  EXPECT_CALL(*call.voice_engine(), ChannelProxyFactory(testing::_))
      .WillRepeatedly(testing::Invoke([&](int channel_id) {
        test::MockVoEChannelProxy* channel_proxy =
            new testing::NiceMock<test::MockVoEChannelProxy>();
        EXPECT_CALL(*channel_proxy, GetAudioDecoderFactory())
            .WillRepeatedly(testing::ReturnRef(decoder_factory));
        EXPECT_CALL(*channel_proxy, SetReceiveCodecs(testing::_))
            .WillRepeatedly(testing::Invoke(
                [](const std::map<int, SdpAudioFormat>& codecs) {
                  EXPECT_THAT(codecs, testing::IsEmpty());
                }));
        EXPECT_CALL(*channel_proxy, GetRtpRtcp(testing::_, testing::_))
            .WillRepeatedly(testing::SetArgPointee<0>(&mock_rtp_rtcp));
        // If being called for the send channel, save a pointer to the channel
        // proxy for later.
        if (channel_id == kRecvChannelId) {
          EXPECT_FALSE(recv_channel_proxy);
          recv_channel_proxy = channel_proxy;
        }
        return channel_proxy;
      }));

  AudioReceiveStream::Config recv_config;
  recv_config.rtp.remote_ssrc = 42;
  recv_config.rtp.local_ssrc = 777;
  recv_config.voe_channel_id = kRecvChannelId;
  recv_config.decoder_factory = decoder_factory;
  AudioReceiveStream* recv_stream = call->CreateAudioReceiveStream(recv_config);
  EXPECT_NE(recv_stream, nullptr);

  EXPECT_CALL(*recv_channel_proxy, AssociateSendChannel(testing::_)).Times(1);
  AudioSendStream::Config send_config(nullptr);
  send_config.rtp.ssrc = 777;
  send_config.voe_channel_id = 123;
  AudioSendStream* send_stream = call->CreateAudioSendStream(send_config);
  EXPECT_NE(send_stream, nullptr);

  EXPECT_CALL(*recv_channel_proxy, DisassociateSendChannel()).Times(1);
  call->DestroyAudioSendStream(send_stream);

  EXPECT_CALL(*recv_channel_proxy, DisassociateSendChannel()).Times(1);
  call->DestroyAudioReceiveStream(recv_stream);
}

TEST(CallTest, CreateDestroy_AssociateAudioSendReceiveStreams_SendFirst) {
  rtc::scoped_refptr<webrtc::AudioDecoderFactory> decoder_factory(
      new rtc::RefCountedObject<webrtc::MockAudioDecoderFactory>);
  CallHelper call(decoder_factory);
  ::testing::NiceMock<MockRtpRtcp> mock_rtp_rtcp;

  constexpr int kRecvChannelId = 101;

  // Set up the mock to create a channel proxy which we know of, so that we can
  // add our expectations to it.
  test::MockVoEChannelProxy* recv_channel_proxy = nullptr;
  EXPECT_CALL(*call.voice_engine(), ChannelProxyFactory(testing::_))
      .WillRepeatedly(testing::Invoke([&](int channel_id) {
        test::MockVoEChannelProxy* channel_proxy =
            new testing::NiceMock<test::MockVoEChannelProxy>();
        EXPECT_CALL(*channel_proxy, GetAudioDecoderFactory())
            .WillRepeatedly(testing::ReturnRef(decoder_factory));
        EXPECT_CALL(*channel_proxy, SetReceiveCodecs(testing::_))
            .WillRepeatedly(testing::Invoke(
                [](const std::map<int, SdpAudioFormat>& codecs) {
                  EXPECT_THAT(codecs, testing::IsEmpty());
                }));
        EXPECT_CALL(*channel_proxy, GetRtpRtcp(testing::_, testing::_))
            .WillRepeatedly(testing::SetArgPointee<0>(&mock_rtp_rtcp));
        // If being called for the send channel, save a pointer to the channel
        // proxy for later.
        if (channel_id == kRecvChannelId) {
          EXPECT_FALSE(recv_channel_proxy);
          recv_channel_proxy = channel_proxy;
          // We need to set this expectation here since the channel proxy is
          // created as a side effect of CreateAudioReceiveStream().
          EXPECT_CALL(*recv_channel_proxy,
                      AssociateSendChannel(testing::_)).Times(1);
        }
        return channel_proxy;
      }));

  AudioSendStream::Config send_config(nullptr);
  send_config.rtp.ssrc = 777;
  send_config.voe_channel_id = 123;
  AudioSendStream* send_stream = call->CreateAudioSendStream(send_config);
  EXPECT_NE(send_stream, nullptr);

  AudioReceiveStream::Config recv_config;
  recv_config.rtp.remote_ssrc = 42;
  recv_config.rtp.local_ssrc = 777;
  recv_config.voe_channel_id = kRecvChannelId;
  recv_config.decoder_factory = decoder_factory;
  AudioReceiveStream* recv_stream = call->CreateAudioReceiveStream(recv_config);
  EXPECT_NE(recv_stream, nullptr);

  EXPECT_CALL(*recv_channel_proxy, DisassociateSendChannel()).Times(1);
  call->DestroyAudioReceiveStream(recv_stream);

  call->DestroyAudioSendStream(send_stream);
}

TEST(CallTest, CreateDestroy_FlexfecReceiveStream) {
  CallHelper call;
  MockTransport rtcp_send_transport;
  FlexfecReceiveStream::Config config(&rtcp_send_transport);
  config.payload_type = 118;
  config.remote_ssrc = 38837212;
  config.protected_media_ssrcs = {27273};

  FlexfecReceiveStream* stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  call->DestroyFlexfecReceiveStream(stream);
}

TEST(CallTest, CreateDestroy_FlexfecReceiveStreams) {
  CallHelper call;
  MockTransport rtcp_send_transport;
  FlexfecReceiveStream::Config config(&rtcp_send_transport);
  config.payload_type = 118;
  std::list<FlexfecReceiveStream*> streams;

  for (int i = 0; i < 2; ++i) {
    for (uint32_t ssrc = 0; ssrc < 1234567; ssrc += 34567) {
      config.remote_ssrc = ssrc;
      config.protected_media_ssrcs = {ssrc + 1};
      FlexfecReceiveStream* stream = call->CreateFlexfecReceiveStream(config);
      EXPECT_NE(stream, nullptr);
      if (ssrc & 1) {
        streams.push_back(stream);
      } else {
        streams.push_front(stream);
      }
    }
    for (auto s : streams) {
      call->DestroyFlexfecReceiveStream(s);
    }
    streams.clear();
  }
}

TEST(CallTest, MultipleFlexfecReceiveStreamsProtectingSingleVideoStream) {
  CallHelper call;
  MockTransport rtcp_send_transport;
  FlexfecReceiveStream::Config config(&rtcp_send_transport);
  config.payload_type = 118;
  config.protected_media_ssrcs = {1324234};
  FlexfecReceiveStream* stream;
  std::list<FlexfecReceiveStream*> streams;

  config.remote_ssrc = 838383;
  stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  streams.push_back(stream);

  config.remote_ssrc = 424993;
  stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  streams.push_back(stream);

  config.remote_ssrc = 99383;
  stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  streams.push_back(stream);

  config.remote_ssrc = 5548;
  stream = call->CreateFlexfecReceiveStream(config);
  EXPECT_NE(stream, nullptr);
  streams.push_back(stream);

  for (auto s : streams) {
    call->DestroyFlexfecReceiveStream(s);
  }
}

namespace {
struct CallBitrateHelper {
  CallBitrateHelper() : CallBitrateHelper(Call::Config(&event_log_)) {}

  explicit CallBitrateHelper(const Call::Config& config)
      : mock_cc_(Clock::GetRealTimeClock(), &event_log_, &packet_router_),
        call_(Call::Create(
            config,
            rtc::MakeUnique<FakeRtpTransportControllerSend>(&packet_router_,
                                                            &mock_cc_))) {}

  webrtc::Call* operator->() { return call_.get(); }
  testing::NiceMock<test::MockSendSideCongestionController>& mock_cc() {
    return mock_cc_;
  }

 private:
  webrtc::RtcEventLogNullImpl event_log_;
  PacketRouter packet_router_;
  testing::NiceMock<test::MockSendSideCongestionController> mock_cc_;
  std::unique_ptr<Call> call_;
};
}  // namespace

TEST(CallBitrateTest, SetBitrateConfigWithValidConfigCallsSetBweBitrates) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 1;
  bitrate_config.start_bitrate_bps = 2;
  bitrate_config.max_bitrate_bps = 3;

  EXPECT_CALL(call.mock_cc(), SetBweBitrates(1, 2, 3));
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallBitrateTest, SetBitrateConfigWithDifferentMinCallsSetBweBitrates) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 10;
  bitrate_config.start_bitrate_bps = 20;
  bitrate_config.max_bitrate_bps = 30;
  call->SetBitrateConfig(bitrate_config);

  bitrate_config.min_bitrate_bps = 11;
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(11, 20, 30));
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallBitrateTest, SetBitrateConfigWithDifferentStartCallsSetBweBitrates) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 10;
  bitrate_config.start_bitrate_bps = 20;
  bitrate_config.max_bitrate_bps = 30;
  call->SetBitrateConfig(bitrate_config);

  bitrate_config.start_bitrate_bps = 21;
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(10, 21, 30));
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallBitrateTest, SetBitrateConfigWithDifferentMaxCallsSetBweBitrates) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 10;
  bitrate_config.start_bitrate_bps = 20;
  bitrate_config.max_bitrate_bps = 30;
  call->SetBitrateConfig(bitrate_config);

  bitrate_config.max_bitrate_bps = 31;
  EXPECT_CALL(call.mock_cc(), SetBweBitrates(10, 20, 31));
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallBitrateTest, SetBitrateConfigWithSameConfigElidesSecondCall) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 1;
  bitrate_config.start_bitrate_bps = 2;
  bitrate_config.max_bitrate_bps = 3;

  EXPECT_CALL(call.mock_cc(), SetBweBitrates(1, 2, 3)).Times(1);
  call->SetBitrateConfig(bitrate_config);
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallBitrateTest,
     SetBitrateConfigWithSameMinMaxAndNegativeStartElidesSecondCall) {
  CallBitrateHelper call;

  Call::Config::BitrateConfig bitrate_config;
  bitrate_config.min_bitrate_bps = 1;
  bitrate_config.start_bitrate_bps = 2;
  bitrate_config.max_bitrate_bps = 3;

  EXPECT_CALL(call.mock_cc(), SetBweBitrates(1, 2, 3)).Times(1);
  call->SetBitrateConfig(bitrate_config);

  bitrate_config.start_bitrate_bps = -1;
  call->SetBitrateConfig(bitrate_config);
}

TEST(CallTest, RecreatingAudioStreamWithSameSsrcReusesRtpState) {
  constexpr uint32_t kSSRC = 12345;
  testing::NiceMock<test::MockAudioDeviceModule> mock_adm;
  // Reply with a 10ms timer every time TimeUntilNextProcess is called to
  // avoid entering a tight loop on the process thread.
  EXPECT_CALL(mock_adm, TimeUntilNextProcess())
       .WillRepeatedly(testing::Return(10));
  rtc::scoped_refptr<test::MockAudioMixer> mock_mixer(
      new rtc::RefCountedObject<test::MockAudioMixer>);

  // There's similar functionality in cricket::VoEWrapper but it's not reachable
  // from here. Since we're working on removing VoE interfaces, I doubt it's
  // worth making VoEWrapper more easily available.
  struct ScopedVoiceEngine {
    ScopedVoiceEngine()
        : voe(VoiceEngine::Create()),
          base(VoEBase::GetInterface(voe)) {}
    ~ScopedVoiceEngine() {
      base->Release();
      EXPECT_TRUE(VoiceEngine::Delete(voe));
    }

    VoiceEngine* voe;
    VoEBase* base;
  };
  ScopedVoiceEngine voice_engine;

  voice_engine.base->Init(&mock_adm);
  AudioState::Config audio_state_config;
  audio_state_config.voice_engine = voice_engine.voe;
  audio_state_config.audio_mixer = mock_mixer;
  auto audio_state = AudioState::Create(audio_state_config);
  RtcEventLogNullImpl event_log;
  Call::Config call_config(&event_log);
  call_config.audio_state = audio_state;
  std::unique_ptr<Call> call(Call::Create(call_config));

  auto create_stream_and_get_rtp_state = [&](uint32_t ssrc) {
    AudioSendStream::Config config(nullptr);
    config.rtp.ssrc = ssrc;
    config.voe_channel_id = voice_engine.base->CreateChannel();
    AudioSendStream* stream = call->CreateAudioSendStream(config);
    VoiceEngineImpl* voe_impl = static_cast<VoiceEngineImpl*>(voice_engine.voe);
    auto channel_proxy = voe_impl->GetChannelProxy(config.voe_channel_id);
    RtpRtcp* rtp_rtcp = nullptr;
    RtpReceiver* rtp_receiver = nullptr;  // Unused but required for call.
    channel_proxy->GetRtpRtcp(&rtp_rtcp, &rtp_receiver);
    const RtpState rtp_state = rtp_rtcp->GetRtpState();
    call->DestroyAudioSendStream(stream);
    voice_engine.base->DeleteChannel(config.voe_channel_id);
    return rtp_state;
  };

  const RtpState rtp_state1 = create_stream_and_get_rtp_state(kSSRC);
  const RtpState rtp_state2 = create_stream_and_get_rtp_state(kSSRC);

  EXPECT_EQ(rtp_state1.sequence_number, rtp_state2.sequence_number);
  EXPECT_EQ(rtp_state1.start_timestamp, rtp_state2.start_timestamp);
  EXPECT_EQ(rtp_state1.timestamp, rtp_state2.timestamp);
  EXPECT_EQ(rtp_state1.capture_time_ms, rtp_state2.capture_time_ms);
  EXPECT_EQ(rtp_state1.last_timestamp_time_ms,
            rtp_state2.last_timestamp_time_ms);
  EXPECT_EQ(rtp_state1.media_has_been_sent, rtp_state2.media_has_been_sent);
}

}  // namespace webrtc
