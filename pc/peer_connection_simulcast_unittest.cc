/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <ostream>  // no-presubmit-check TODO(webrtc:8982)

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/create_peerconnection_factory.h"
#include "api/rtp_transceiver_interface.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "pc/peer_connection.h"
#include "pc/peer_connection_wrapper.h"
#include "pc/test/fake_audio_capture_module.h"
#include "pc/test/mock_peer_connection_observers.h"
#include "rtc_base/gunit.h"
#include "test/gmock.h"

using ::testing::Contains;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Ne;
using ::testing::Property;
using ::testing::SizeIs;

using cricket::MediaContentDescription;
using cricket::RidDescription;
using cricket::SimulcastDescription;
using cricket::SimulcastLayer;
using cricket::StreamParams;

namespace cricket {

std::ostream& operator<<(  // no-presubmit-check TODO(webrtc:8982)
    std::ostream& os,      // no-presubmit-check TODO(webrtc:8982)
    const SimulcastLayer& layer) {
  if (layer.is_paused) {
    os << "~";
  }
  return os << layer.rid;
}

}  // namespace cricket

namespace {

std::vector<SimulcastLayer> CreateLayers(const std::vector<std::string>& rids,
                                         const std::vector<bool>& active) {
  RTC_DCHECK_EQ(rids.size(), active.size());
  std::vector<SimulcastLayer> result;
  absl::c_transform(rids, active, std::back_inserter(result),
                    [](const std::string& rid, bool is_active) {
                      return SimulcastLayer(rid, !is_active);
                    });
  return result;
}
std::vector<SimulcastLayer> CreateLayers(const std::vector<std::string>& rids,
                                         bool active) {
  return CreateLayers(rids, std::vector<bool>(rids.size(), active));
}

}  // namespace
namespace webrtc {

class PeerConnectionSimulcastTests : public testing::Test {
 public:
  PeerConnectionSimulcastTests()
      : pc_factory_(
            CreatePeerConnectionFactory(rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        rtc::Thread::Current(),
                                        FakeAudioCaptureModule::Create(),
                                        CreateBuiltinAudioEncoderFactory(),
                                        CreateBuiltinAudioDecoderFactory(),
                                        CreateBuiltinVideoEncoderFactory(),
                                        CreateBuiltinVideoDecoderFactory(),
                                        nullptr,
                                        nullptr)) {}

  rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      MockPeerConnectionObserver* observer) {
    PeerConnectionInterface::RTCConfiguration config;
    config.sdp_semantics = SdpSemantics::kUnifiedPlan;
    PeerConnectionDependencies pcd(observer);
    auto pc = pc_factory_->CreatePeerConnection(config, std::move(pcd));
    EXPECT_TRUE(pc);
    observer->SetPeerConnectionInterface(pc);
    return pc;
  }

  std::unique_ptr<PeerConnectionWrapper> CreatePeerConnectionWrapper() {
    auto observer = absl::make_unique<MockPeerConnectionObserver>();
    auto pc = CreatePeerConnection(observer.get());
    return absl::make_unique<PeerConnectionWrapper>(pc_factory_, pc,
                                                    std::move(observer));
  }

  RtpTransceiverInit CreateTransceiverInit(
      const std::vector<SimulcastLayer>& layers) {
    RtpTransceiverInit init;
    for (const SimulcastLayer& layer : layers) {
      RtpEncodingParameters encoding;
      encoding.rid = layer.rid;
      encoding.active = !layer.is_paused;
      init.send_encodings.push_back(encoding);
    }
    return init;
  }

  rtc::scoped_refptr<RtpTransceiverInterface> AddTransceiver(
      PeerConnectionWrapper* pc,
      const std::vector<SimulcastLayer>& layers) {
    auto init = CreateTransceiverInit(layers);
    return pc->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  }

  SimulcastDescription RemoveSimulcast(SessionDescriptionInterface* sd) {
    auto mcd = sd->description()->contents()[0].media_description();
    auto result = mcd->simulcast_description();
    mcd->set_simulcast_description(SimulcastDescription());
    return result;
  }

  void AddRequestToReceiveSimulcast(const std::vector<SimulcastLayer>& layers,
                                    SessionDescriptionInterface* sd) {
    auto mcd = sd->description()->contents()[0].media_description();
    SimulcastDescription simulcast;
    auto& receive_layers = simulcast.receive_layers();
    for (const SimulcastLayer& layer : layers) {
      receive_layers.AddLayer(layer);
    }
    mcd->set_simulcast_description(simulcast);
  }

  bool ValidateTransceiverParameters(
      rtc::scoped_refptr<RtpTransceiverInterface> transceiver,
      const std::vector<SimulcastLayer>& layers) {
    bool errors_before = ::testing::Test::HasFailure();
    auto parameters = transceiver->sender()->GetParameters();
    std::vector<SimulcastLayer> result_layers;
    absl::c_transform(parameters.encodings, std::back_inserter(result_layers),
                      [](const RtpEncodingParameters& encoding) {
                        return SimulcastLayer(encoding.rid, !encoding.active);
                      });
    EXPECT_THAT(result_layers, ElementsAreArray(layers));
    // If there were errors before this code ran, we cannot tell if this
    // validation succeeded or not.
    return errors_before || !::testing::Test::HasFailure();
  }

 private:
  rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory_;
};

// Validates that RIDs are supported arguments when adding a transceiver.
TEST_F(PeerConnectionSimulcastTests, CanCreateTransceiverWithRid) {
  auto pc = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f"}, true);
  auto transceiver = AddTransceiver(pc.get(), layers);
  ASSERT_TRUE(transceiver);
  auto parameters = transceiver->sender()->GetParameters();
  // Single RID should be removed.
  EXPECT_THAT(parameters.encodings,
              ElementsAre(Field(&RtpEncodingParameters::rid, Eq(""))));
}

TEST_F(PeerConnectionSimulcastTests, CanCreateTransceiverWithSimulcast) {
  auto pc = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, true);
  auto transceiver = AddTransceiver(pc.get(), layers);
  ASSERT_TRUE(transceiver);
  EXPECT_TRUE(ValidateTransceiverParameters(transceiver, layers));
}

TEST_F(PeerConnectionSimulcastTests, RidsAreAutogeneratedIfNotProvided) {
  auto pc = CreatePeerConnectionWrapper();
  auto init = CreateTransceiverInit(CreateLayers({"f", "h", "q"}, true));
  for (RtpEncodingParameters& parameters : init.send_encodings) {
    parameters.rid = "";
  }
  auto transceiver = pc->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  auto parameters = transceiver->sender()->GetParameters();
  ASSERT_EQ(3u, parameters.encodings.size());
  EXPECT_THAT(parameters.encodings,
              Each(Field(&RtpEncodingParameters::rid, Ne(""))));
}

// Validates that an error is returned when there is a mix of supplied and not
// supplied RIDs in a call to AddTransceiver.
TEST_F(PeerConnectionSimulcastTests, MustSupplyAllOrNoRidsInSimulcast) {
  auto pc_wrapper = CreatePeerConnectionWrapper();
  auto pc = pc_wrapper->pc();
  // Cannot create a layer with empty RID. Remove the RID after init is created.
  auto layers = CreateLayers({"f", "h", "remove"}, true);
  auto init = CreateTransceiverInit(layers);
  init.send_encodings[2].rid = "";
  auto error = pc->AddTransceiver(cricket::MEDIA_TYPE_VIDEO, init);
  EXPECT_EQ(RTCErrorType::INVALID_PARAMETER, error.error().type());
}

// Validates that a single RID does not get negotiated.
// This test is currently disabled because a single RID is not supported.
TEST_F(PeerConnectionSimulcastTests, SingleRidIsRemovedFromSessionDescription) {
  auto pc = CreatePeerConnectionWrapper();
  auto transceiver = AddTransceiver(pc.get(), CreateLayers({"1"}, true));
  auto offer = pc->CreateOfferAndSetAsLocal();
  ASSERT_TRUE(offer);
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  EXPECT_THAT(contents[0].media_description()->streams(),
              ElementsAre(Property(&StreamParams::has_rids, false)));
}

TEST_F(PeerConnectionSimulcastTests, SimulcastLayersRemovedFromTail) {
  static_assert(
      kMaxSimulcastStreams < 8,
      "Test assumes that the platform does not allow 8 simulcast layers");
  auto pc = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3", "4", "5", "6", "7", "8"}, true);
  std::vector<SimulcastLayer> expected_layers;
  std::copy_n(layers.begin(), kMaxSimulcastStreams,
              std::back_inserter(expected_layers));
  auto transceiver = AddTransceiver(pc.get(), layers);
  EXPECT_TRUE(ValidateTransceiverParameters(transceiver, expected_layers));
}

// Checks that an offfer to send simulcast contains a SimulcastDescription.
TEST_F(PeerConnectionSimulcastTests, SimulcastAppearsInSessionDescription) {
  auto pc = CreatePeerConnectionWrapper();
  std::vector<std::string> rids({"f", "h", "q"});
  auto layers = CreateLayers(rids, true);
  auto transceiver = AddTransceiver(pc.get(), layers);
  auto offer = pc->CreateOffer();
  ASSERT_TRUE(offer);
  auto contents = offer->description()->contents();
  ASSERT_EQ(1u, contents.size());
  auto content = contents[0];
  auto mcd = content.media_description();
  ASSERT_TRUE(mcd->HasSimulcast());
  auto simulcast = mcd->simulcast_description();
  EXPECT_THAT(simulcast.receive_layers(), IsEmpty());
  // The size is validated separately because GetAllLayers() flattens the list.
  EXPECT_THAT(simulcast.send_layers(), SizeIs(3));
  std::vector<SimulcastLayer> result = simulcast.send_layers().GetAllLayers();
  EXPECT_THAT(result, ElementsAreArray(layers));
  auto streams = mcd->streams();
  ASSERT_EQ(1u, streams.size());
  auto stream = streams[0];
  EXPECT_FALSE(stream.has_ssrcs());
  EXPECT_TRUE(stream.has_rids());
  std::vector<std::string> result_rids;
  absl::c_transform(stream.rids(), std::back_inserter(result_rids),
                    [](const RidDescription& rid) { return rid.rid; });
  EXPECT_THAT(result_rids, ElementsAreArray(rids));
}

// Checks that Simulcast layers propagate to the sender parameters.
TEST_F(PeerConnectionSimulcastTests, SimulcastLayersAreSetInSender) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, true);
  auto transceiver = AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  EXPECT_TRUE(ValidateTransceiverParameters(transceiver, layers));

  // Remove simulcast as the second peer connection won't support it.
  auto simulcast = RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = remote->CreateAnswerAndSetAsLocal();

  // Setup an answer that mimics a server accepting simulcast.
  auto mcd_answer = answer->description()->contents()[0].media_description();
  mcd_answer->mutable_streams().clear();
  auto simulcast_layers = simulcast.send_layers().GetAllLayers();
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const auto& layer : simulcast_layers) {
    receive_layers.AddLayer(layer);
  }
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  EXPECT_TRUE(ValidateTransceiverParameters(transceiver, layers));
}

// Checks that paused Simulcast layers propagate to the sender parameters.
TEST_F(PeerConnectionSimulcastTests, PausedSimulcastLayersAreDisabledInSender) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, {true, false, true});
  auto server_layers = CreateLayers({"f", "h", "q"}, {true, false, false});
  RTC_DCHECK_EQ(layers.size(), server_layers.size());
  auto transceiver = AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  EXPECT_TRUE(ValidateTransceiverParameters(transceiver, layers));

  // Remove simulcast as the second peer connection won't support it.
  RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = remote->CreateAnswerAndSetAsLocal();

  // Setup an answer that mimics a server accepting simulcast.
  auto mcd_answer = answer->description()->contents()[0].media_description();
  mcd_answer->mutable_streams().clear();
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const SimulcastLayer& layer : server_layers) {
    receive_layers.AddLayer(layer);
  }
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  EXPECT_TRUE(ValidateTransceiverParameters(transceiver, server_layers));
}

// Checks that when Simulcast is not supported by the remote party, then all
// the layers (except the first) are marked as disabled.
TEST_F(PeerConnectionSimulcastTests, SimulcastRejectedDisablesExtraLayers) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3", "4"}, true);
  // The number of layers does not change.
  auto expected_layers = CreateLayers({"1", "2", "3", "4"}, false);
  RTC_DCHECK_EQ(layers.size(), expected_layers.size());
  expected_layers[0].is_paused = false;
  auto transceiver = AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  // Remove simulcast as the second peer connection won't support it.
  RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = remote->CreateAnswerAndSetAsLocal();
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  EXPECT_TRUE(ValidateTransceiverParameters(transceiver, expected_layers));
}

// Checks that if Simulcast is supported by remote party, but some layer is
// rejected, then only that layer is marked as disabled.
TEST_F(PeerConnectionSimulcastTests, RejectedSimulcastLayersAreDeactivated) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"1", "2", "3", "4"}, true);
  auto transceiver = AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  EXPECT_TRUE(ValidateTransceiverParameters(transceiver, layers));
  // Remove simulcast as the second peer connection won't support it.
  auto removed_simulcast = RemoveSimulcast(offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto answer = remote->CreateAnswerAndSetAsLocal();
  auto mcd_answer = answer->description()->contents()[0].media_description();
  // Setup the answer to look like a server response.
  // Remove one of the layers to reject it in the answer.
  auto simulcast_layers = removed_simulcast.send_layers().GetAllLayers();
  simulcast_layers.erase(simulcast_layers.begin());
  auto& receive_layers = mcd_answer->simulcast_description().receive_layers();
  for (const auto& layer : simulcast_layers) {
    receive_layers.AddLayer(layer);
  }
  ASSERT_TRUE(mcd_answer->HasSimulcast());
  EXPECT_TRUE(local->SetRemoteDescription(std::move(answer), &error)) << error;
  layers[0].is_paused = true;
  EXPECT_TRUE(ValidateTransceiverParameters(transceiver, layers));
}

// Checks that simulcast is set up correctly when the server sends an offer
// requesting to receive simulcast.
TEST_F(PeerConnectionSimulcastTests, ServerSendsOfferToReceiveSimulcast) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  // Remove simulcast as a sender and set it up as a receiver.
  RemoveSimulcast(offer.get());
  AddRequestToReceiveSimulcast(layers, offer.get());
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto transceiver = remote->pc()->GetTransceivers()[0];
  transceiver->SetDirection(RtpTransceiverDirection::kSendRecv);
  EXPECT_TRUE(remote->CreateAnswerAndSetAsLocal());
  EXPECT_TRUE(ValidateTransceiverParameters(transceiver, layers));
}

// Checks that SetRemoteDescription doesn't attempt to associate a transceiver
// when simulcast is requested by the server.
TEST_F(PeerConnectionSimulcastTests, TransceiverIsNotRecycledWithSimulcast) {
  auto local = CreatePeerConnectionWrapper();
  auto remote = CreatePeerConnectionWrapper();
  auto layers = CreateLayers({"f", "h", "q"}, true);
  AddTransceiver(local.get(), layers);
  auto offer = local->CreateOfferAndSetAsLocal();
  // Remove simulcast as a sender and set it up as a receiver.
  RemoveSimulcast(offer.get());
  AddRequestToReceiveSimulcast(layers, offer.get());
  // Call AddTrack so that a transceiver is created.
  remote->AddVideoTrack("fake_track");
  std::string error;
  EXPECT_TRUE(remote->SetRemoteDescription(std::move(offer), &error)) << error;
  auto transceivers = remote->pc()->GetTransceivers();
  ASSERT_EQ(2u, transceivers.size());
  auto transceiver = transceivers[1];
  transceiver->SetDirection(RtpTransceiverDirection::kSendRecv);
  EXPECT_TRUE(remote->CreateAnswerAndSetAsLocal());
  EXPECT_TRUE(ValidateTransceiverParameters(transceiver, layers));
}

}  // namespace webrtc
