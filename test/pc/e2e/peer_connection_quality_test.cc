/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/peer_connection_quality_test.h"

#include <set>
#include <utility>

#include "absl/memory/memory.h"
#include "api/peer_connection_interface.h"
#include "api/scoped_refptr.h"
#include "api/units/time_delta.h"
#include "rtc_base/bind.h"
#include "rtc_base/gunit.h"
#include "system_wrappers/include/cpu_info.h"
#include "test/pc/e2e/analyzer/video/example_video_quality_analyzer.h"
#include "test/pc/e2e/api/video_quality_analyzer_interface.h"
#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace test {
namespace {

using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;

constexpr int kDefaultTimeoutMs = 10000;
constexpr char kSignalThreadName[] = "signaling_thread";
// 1 signaling, 2 network, 2 worker and 2 extra for codecs etc.
constexpr int kPeerConnectionUsedThreads = 7;
// Framework has extra thread for network layer and extra thread for peer
// connection stats polling.
constexpr int kFrameworkUsedThreads = 2;
constexpr int kMaxVideoAnalyzerThreads = 8;

std::string VideoConfigSourcePresenceToString(const VideoConfig& video_config) {
  char buf[1024];
  rtc::SimpleStringBuilder builder(buf);
  builder << "video_config.generator=" << video_config.generator.has_value()
          << "; video_config.input_file_name="
          << video_config.input_file_name.has_value()
          << "; video_config.screen_share_config="
          << video_config.screen_share_config.has_value() << ";";
  return builder.str();
}

}  // namespace

PeerConnectionE2EQualityTest::PeerConnectionE2EQualityTest(
    std::unique_ptr<Analyzers> analyzers)
    : clock_(Clock::GetRealTimeClock()) {
  RTC_CHECK(analyzers);

  // Create default video quality analyzer. We will always create an analyzer,
  // even if there are no video streams, because it will be installed into video
  // encoder/decoder factories.
  if (analyzers->video_quality_analyzer == nullptr) {
    analyzers->video_quality_analyzer =
        absl::make_unique<ExampleVideoQualityAnalyzer>();
  }
  encoded_image_id_controller_ =
      absl::make_unique<SingleProcessEncodedImageIdInjector>();
  video_quality_analyzer_injection_helper_ =
      absl::make_unique<VideoQualityAnalyzerInjectionHelper>(
          std::move(analyzers->video_quality_analyzer),
          encoded_image_id_controller_.get(),
          encoded_image_id_controller_.get());
}

void PeerConnectionE2EQualityTest::Run(
    std::unique_ptr<InjectableComponents> alice_components,
    std::unique_ptr<Params> alice_params,
    std::unique_ptr<InjectableComponents> bob_components,
    std::unique_ptr<Params> bob_params,
    RunParams run_params) {
  RTC_CHECK(alice_components);
  RTC_CHECK(alice_params);
  RTC_CHECK(bob_components);
  RTC_CHECK(bob_params);

  SetMissedVideoStreamLabels({alice_params.get(), bob_params.get()});
  ValidateParams({alice_params.get(), bob_params.get()});

  // Print test summary
  RTC_LOG(INFO)
      << "Media quality test: Alice will make a call to Bob with media video="
      << !alice_params->video_configs.empty()
      << "; audio=" << alice_params->audio_config.has_value()
      << ". Bob will respond with media video="
      << !bob_params->video_configs.empty()
      << "; audio=" << bob_params->audio_config.has_value();

  const std::unique_ptr<rtc::Thread> signaling_thread = rtc::Thread::Create();
  signaling_thread->SetName(kSignalThreadName, nullptr);
  signaling_thread->Start();

  // Create call participants: Alice and Bob.
  // Audio streams are intercepted in AudioDeviceModule, so if it is required to
  // catch output of Alice's stream, Alice's output_dump_file_name should be
  // passed to Bob's TestPeer setup as audio output file name.
  absl::optional<std::string> alice_audio_output_dump_file_name =
      bob_params->audio_config ? bob_params->audio_config->output_dump_file_name
                               : absl::nullopt;
  absl::optional<std::string> bob_audio_output_dump_file_name =
      alice_params->audio_config
          ? alice_params->audio_config->output_dump_file_name
          : absl::nullopt;
  alice_ = TestPeer::CreateTestPeer(
      std::move(alice_components), std::move(alice_params),
      video_quality_analyzer_injection_helper_.get(), signaling_thread.get(),
      alice_audio_output_dump_file_name);
  bob_ = TestPeer::CreateTestPeer(
      std::move(bob_components), std::move(bob_params),
      video_quality_analyzer_injection_helper_.get(), signaling_thread.get(),
      bob_audio_output_dump_file_name);

  int num_cores = CpuInfo::DetectNumberOfCores();
  RTC_DCHECK_GE(num_cores, 1);

  int video_analyzer_threads =
      num_cores - kPeerConnectionUsedThreads - kFrameworkUsedThreads;
  if (video_analyzer_threads <= 0) {
    video_analyzer_threads = 1;
  }
  video_analyzer_threads =
      std::min(video_analyzer_threads, kMaxVideoAnalyzerThreads);
  RTC_LOG(INFO) << "video_analyzer_threads=" << video_analyzer_threads;

  video_quality_analyzer_injection_helper_->Start(video_analyzer_threads);
  signaling_thread->Invoke<void>(
      RTC_FROM_HERE,
      rtc::Bind(&PeerConnectionE2EQualityTest::RunOnSignalingThread, this,
                run_params));
  video_quality_analyzer_injection_helper_->Stop();

  // Ensuring that TestPeers have been destroyed in order to correctly close
  // Audio dumps.
  RTC_CHECK(!alice_);
  RTC_CHECK(!bob_);
  // Ensuring that FrameGeneratorCapturerVideoTrackSource and VideoFrameWriter
  // are destroyed on the right thread.
  RTC_CHECK(video_sources_.empty());
  RTC_CHECK(video_writers_.empty());
}

void PeerConnectionE2EQualityTest::SetMissedVideoStreamLabels(
    std::vector<Params*> params) {
  int counter = 0;
  std::set<std::string> video_labels;
  for (auto* p : params) {
    for (auto& video_config : p->video_configs) {
      if (!video_config.stream_label) {
        std::string label;
        do {
          label = "_auto_video_stream_label_" + std::to_string(counter);
          ++counter;
        } while (!video_labels.insert(label).second);
        video_config.stream_label = label;
      }
    }
  }
}

void PeerConnectionE2EQualityTest::ValidateParams(std::vector<Params*> params) {
  std::set<std::string> video_labels;
  int media_streams_count = 0;

  for (Params* p : params) {
    if (p->audio_config) {
      media_streams_count++;
    }
    media_streams_count += p->video_configs.size();

    // Validate that each video config has exactly one of |generator|,
    // |input_file_name| or |screen_share_config| set. Also validate that all
    // video stream labels are unique.
    for (auto& video_config : p->video_configs) {
      RTC_CHECK(video_config.stream_label);
      bool inserted =
          video_labels.insert(video_config.stream_label.value()).second;
      RTC_CHECK(inserted) << "Duplicate video_config.stream_label="
                          << video_config.stream_label.value();
      RTC_CHECK(video_config.generator || video_config.input_file_name ||
                video_config.screen_share_config)
          << VideoConfigSourcePresenceToString(video_config);
      RTC_CHECK(!(video_config.input_file_name && video_config.generator))
          << VideoConfigSourcePresenceToString(video_config);
      RTC_CHECK(
          !(video_config.input_file_name && video_config.screen_share_config))
          << VideoConfigSourcePresenceToString(video_config);
      RTC_CHECK(!(video_config.screen_share_config && video_config.generator))
          << VideoConfigSourcePresenceToString(video_config);
    }
    if (p->audio_config) {
      // Check that if mode input file name specified only if mode is kFile.
      if (p->audio_config.value().mode == AudioConfig::Mode::kGenerated) {
        RTC_CHECK(!p->audio_config.value().input_file_name);
      }
      if (p->audio_config.value().mode == AudioConfig::Mode::kFile) {
        RTC_CHECK(p->audio_config.value().input_file_name);
        RTC_CHECK(FileExists(p->audio_config.value().input_file_name.value()));
      }
    }
  }

  RTC_CHECK_GT(media_streams_count, 0) << "No media in the call.";
}

void PeerConnectionE2EQualityTest::RunOnSignalingThread(RunParams run_params) {
  AddMedia(alice_.get());
  AddMedia(bob_.get());

  SetupCall();

  WaitForTransceiversSetup(alice_->params(), bob_.get());
  WaitForTransceiversSetup(bob_->params(), alice_.get());
  SetupVideoSink(alice_->params(), bob_.get());
  SetupVideoSink(bob_->params(), alice_.get());

  StartVideo();

  rtc::Event done;
  done.Wait(static_cast<int>(run_params.run_duration.ms()));

  TearDownCall();
}

void PeerConnectionE2EQualityTest::AddMedia(TestPeer* peer) {
  AddVideo(peer);
  if (peer->params()->audio_config) {
    AddAudio(peer);
  }
}

void PeerConnectionE2EQualityTest::AddVideo(TestPeer* peer) {
  // Params here valid because of pre-run validation.
  Params* params = peer->params();
  for (auto video_config : params->video_configs) {
    // Create video generator.
    std::unique_ptr<FrameGenerator> frame_generator =
        CreateFrameGenerator(video_config);

    // Wrap it to inject video quality analyzer and enable dump of input video
    // if required.
    VideoFrameWriter* writer =
        MaybeCreateVideoWriter(video_config.input_dump_file_name, video_config);
    frame_generator =
        video_quality_analyzer_injection_helper_->WrapFrameGenerator(
            video_config.stream_label.value(), std::move(frame_generator),
            writer);

    // Setup FrameGenerator into peer connection.
    std::unique_ptr<FrameGeneratorCapturer> capturer =
        absl::WrapUnique(FrameGeneratorCapturer::Create(
            std::move(frame_generator), video_config.fps, clock_));
    rtc::scoped_refptr<FrameGeneratorCapturerVideoTrackSource> source =
        new rtc::RefCountedObject<FrameGeneratorCapturerVideoTrackSource>(
            move(capturer));
    video_sources_.push_back(source);
    RTC_LOG(INFO) << "Adding video with video_config.stream_label="
                  << video_config.stream_label.value();
    rtc::scoped_refptr<VideoTrackInterface> track =
        peer->pc_factory()->CreateVideoTrack(video_config.stream_label.value(),
                                             source);
    peer->AddTransceiver(track);
  }
}

std::unique_ptr<FrameGenerator>
PeerConnectionE2EQualityTest::CreateFrameGenerator(
    const VideoConfig& video_config) {
  if (video_config.generator) {
    absl::optional<FrameGenerator::OutputType> frame_generator_type =
        absl::nullopt;
    if (video_config.generator == VideoGeneratorType::kDefault) {
      frame_generator_type = FrameGenerator::OutputType::I420;
    } else if (video_config.generator == VideoGeneratorType::kI420A) {
      frame_generator_type = FrameGenerator::OutputType::I420A;
    } else if (video_config.generator == VideoGeneratorType::kI010) {
      frame_generator_type = FrameGenerator::OutputType::I010;
    }
    return FrameGenerator::CreateSquareGenerator(
        static_cast<int>(video_config.width),
        static_cast<int>(video_config.height), frame_generator_type,
        absl::nullopt);
  }
  if (video_config.input_file_name) {
    return FrameGenerator::CreateFromYuvFile(
        std::vector<std::string>(/*count=*/1,
                                 video_config.input_file_name.value()),
        video_config.width, video_config.height, /*frame_repeat_count=*/1);
  }
  if (video_config.screen_share_config) {
    // TODO(titovartem) implement screen share support
    // (http://bugs.webrtc.org/10138)
    RTC_NOTREACHED() << "Screen share is not implemented";
    return nullptr;
  }
  RTC_NOTREACHED() << "Unsupported video_config input source";
  return nullptr;
}

void PeerConnectionE2EQualityTest::AddAudio(TestPeer* peer) {
  RTC_CHECK(peer->params()->audio_config);
  rtc::scoped_refptr<webrtc::AudioSourceInterface> source =
      peer->pc_factory()->CreateAudioSource(
          peer->params()->audio_config->audio_options);
  rtc::scoped_refptr<AudioTrackInterface> track =
      peer->pc_factory()->CreateAudioTrack("audio", source);
  peer->AddTransceiver(track);
}

void PeerConnectionE2EQualityTest::SetupCall() {
  // Connect peers.
  ASSERT_TRUE(alice_->ExchangeOfferAnswerWith(bob_.get()));
  // Do the SDP negotiation, and also exchange ice candidates.
  ASSERT_EQ_WAIT(alice_->signaling_state(), PeerConnectionInterface::kStable,
                 kDefaultTimeoutMs);
  ASSERT_TRUE_WAIT(alice_->IsIceGatheringDone(), kDefaultTimeoutMs);
  ASSERT_TRUE_WAIT(bob_->IsIceGatheringDone(), kDefaultTimeoutMs);

  // Connect an ICE candidate pairs.
  ASSERT_TRUE(bob_->AddIceCandidates(alice_->observer()->GetAllCandidates()));
  ASSERT_TRUE(alice_->AddIceCandidates(bob_->observer()->GetAllCandidates()));
  // This means that ICE and DTLS are connected.
  ASSERT_TRUE_WAIT(bob_->IsIceConnected(), kDefaultTimeoutMs);
  ASSERT_TRUE_WAIT(alice_->IsIceConnected(), kDefaultTimeoutMs);
}

void PeerConnectionE2EQualityTest::WaitForTransceiversSetup(
    Params* params,
    TestPeer* remote_peer) {
  uint64_t expected_remote_transceivers =
      params->video_configs.size() + (params->audio_config ? 1 : 0);
  ASSERT_EQ_WAIT(remote_peer->observer()->on_track_transceivers_.size(),
                 expected_remote_transceivers, kDefaultTimeoutMs);
}

void PeerConnectionE2EQualityTest::SetupVideoSink(Params* params,
                                                  TestPeer* remote_peer) {
  if (params->video_configs.empty()) {
    return;
  }
  std::map<std::string, VideoConfig*> video_configs_by_label;
  for (auto& video_config : params->video_configs) {
    video_configs_by_label.insert(std::pair<std::string, VideoConfig*>(
        video_config.stream_label.value(), &video_config));
  }

  for (const auto& transceiver :
       remote_peer->observer()->on_track_transceivers_) {
    const rtc::scoped_refptr<MediaStreamTrackInterface>& track =
        transceiver->receiver()->track();
    if (track->kind() != MediaStreamTrackInterface::kVideoKind) {
      continue;
    }

    auto it = video_configs_by_label.find(track->id());
    RTC_CHECK(it != video_configs_by_label.end());
    VideoConfig* video_config = it->second;

    VideoFrameWriter* writer = MaybeCreateVideoWriter(
        video_config->output_dump_file_name, *video_config);
    // It is safe to cast here, because it is checked above that track->kind()
    // is kVideoKind.
    auto* video_track = static_cast<VideoTrackInterface*>(track.get());
    std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>> video_sink =
        video_quality_analyzer_injection_helper_->CreateVideoSink(writer);
    video_track->AddOrUpdateSink(video_sink.get(), rtc::VideoSinkWants());
    output_video_sinks_.push_back(std::move(video_sink));
  }
}

void PeerConnectionE2EQualityTest::StartVideo() {
  for (const auto& video_source : video_sources_) {
    video_source->Start();
  }
}

void PeerConnectionE2EQualityTest::TearDownCall() {
  for (const auto& video_source : video_sources_) {
    video_source->Stop();
  }

  alice_->pc()->Close();
  bob_->pc()->Close();

  for (const auto& video_writer : video_writers_) {
    video_writer->Close();
  }

  video_sources_.clear();
  video_writers_.clear();
  alice_.reset();
  bob_.reset();
}

VideoFrameWriter* PeerConnectionE2EQualityTest::MaybeCreateVideoWriter(
    absl::optional<std::string> file_name,
    const VideoConfig& config) {
  if (!file_name) {
    return nullptr;
  }
  auto video_writer = absl::make_unique<VideoFrameWriter>(
      file_name.value(), config.width, config.height, config.fps);
  VideoFrameWriter* out = video_writer.get();
  video_writers_.push_back(std::move(video_writer));
  return out;
}

}  // namespace test
}  // namespace webrtc
