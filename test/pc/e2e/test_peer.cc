/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/pc/e2e/test_peer.h"

#include <utility>

#include "absl/memory/memory.h"
#include "absl/types/optional.h"
#include "api/rtc_event_log/rtc_event_log_factory.h"
#include "api/scoped_refptr.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/task_queue/task_queue_factory.h"
#include "api/video_codecs/builtin_video_decoder_factory.h"
#include "api/video_codecs/builtin_video_encoder_factory.h"
#include "media/engine/webrtc_media_engine.h"
#include "media/engine/webrtc_media_engine_defaults.h"
#include "modules/audio_device/include/audio_device.h"
#include "modules/audio_processing/aec_dump/aec_dump_factory.h"
#include "modules/audio_processing/include/audio_processing.h"
#include "p2p/client/basic_port_allocator.h"
#include "rtc_base/location.h"
#include "test/testsupport/copy_to_file_audio_capturer.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

using RemotePeerAudioConfig =
    ::webrtc::webrtc_pc_e2e::TestPeer::RemotePeerAudioConfig;
using AudioConfig =
    ::webrtc::webrtc_pc_e2e::PeerConnectionE2EQualityTestFixture::AudioConfig;

constexpr int16_t kGeneratedAudioMaxAmplitude = 32000;
constexpr int kDefaultSamplingFrequencyInHz = 48000;

// Sets mandatory entities in injectable components like |pcf_dependencies|
// and |pc_dependencies| if they are omitted. Also setup required
// dependencies, that won't be specially provided by factory and will be just
// transferred to peer connection creation code.
void SetMandatoryEntities(InjectableComponents* components) {
  RTC_DCHECK(components->pcf_dependencies);
  RTC_DCHECK(components->pc_dependencies);

  // Setup required peer connection factory dependencies.
  if (components->pcf_dependencies->task_queue_factory == nullptr) {
    components->pcf_dependencies->task_queue_factory =
        CreateDefaultTaskQueueFactory();
  }
  if (components->pcf_dependencies->call_factory == nullptr) {
    components->pcf_dependencies->call_factory = webrtc::CreateCallFactory();
  }
  if (components->pcf_dependencies->event_log_factory == nullptr) {
    components->pcf_dependencies->event_log_factory =
        absl::make_unique<RtcEventLogFactory>(
            components->pcf_dependencies->task_queue_factory.get());
  }
}

struct TestPeerComponents {

  rtc::scoped_refptr<PeerConnectionFactoryInterface> peer_connection_factory;
  rtc::scoped_refptr<PeerConnectionInterface> peer_connection;
  rtc::scoped_refptr<AudioProcessing> audio_processing;

  TestPeerComponents(std::unique_ptr<InjectableComponents> components,
                     const Params& params,
                     MockPeerConnectionObserver* observer,
                     VideoQualityAnalyzerInjectionHelper* video_analyzer_helper,
                     rtc::Thread* signaling_thread,
                     absl::optional<RemotePeerAudioConfig> remote_audio_config,
                     double bitrate_multiplier,
                     rtc::TaskQueue* task_queue) {
    std::map<std::string, absl::optional<int>> stream_required_spatial_index;
    for (auto& video_config : params.video_configs) {
      // Stream label should be set by fixture implementation here.
      RTC_DCHECK(video_config.stream_label);
      bool res =
          stream_required_spatial_index
              .insert({*video_config.stream_label,
                       video_config.simulcast_config
                           ? absl::optional<int>(video_config.simulcast_config
                                                     ->target_spatial_index)
                           : absl::nullopt})
              .second;
      RTC_DCHECK(res) << "Duplicate video_config.stream_label="
                      << *video_config.stream_label;
    }

    // Create audio processing, that will be used to create media engine that
    // then will be added into peer connection. See CreateMediaEngine(...).
    audio_processing = webrtc::AudioProcessingBuilder().Create();
    if (params.aec_dump_path) {
      audio_processing->AttachAecDump(
          AecDumpFactory::Create(*params.aec_dump_path, -1, task_queue));
    }

    // Create peer connection factory.
    PeerConnectionFactoryDependencies pcf_deps = CreatePCFDependencies(
        std::move(components->pcf_dependencies), params.audio_config,
        bitrate_multiplier, std::move(stream_required_spatial_index),
        video_analyzer_helper, components->network_thread, signaling_thread,
        std::move(remote_audio_config), task_queue);
    peer_connection_factory =
        CreateModularPeerConnectionFactory(std::move(pcf_deps));

    // Create peer connection.
    PeerConnectionDependencies pc_deps =
        CreatePCDependencies(std::move(components->pc_dependencies), observer);
    peer_connection = peer_connection_factory->CreatePeerConnection(
        params.rtc_configuration, std::move(pc_deps));
    peer_connection->SetBitrate(params.bitrate_params);
  }

  std::unique_ptr<TestAudioDeviceModule::Capturer> CreateAudioCapturer(
      AudioConfig audio_config) {
    if (audio_config.mode == AudioConfig::Mode::kGenerated) {
      return TestAudioDeviceModule::CreatePulsedNoiseCapturer(
          kGeneratedAudioMaxAmplitude, audio_config.sampling_frequency_in_hz);
    }
    if (audio_config.mode == AudioConfig::Mode::kFile) {
      RTC_DCHECK(audio_config.input_file_name);
      return TestAudioDeviceModule::CreateWavFileReader(
          audio_config.input_file_name.value(), /*repeat=*/true);
    }
    RTC_NOTREACHED() << "Unknown audio_config->mode";
    return nullptr;
  }

  rtc::scoped_refptr<AudioDeviceModule> CreateAudioDeviceModule(
      TaskQueueFactory* task_queue_factory,
      absl::optional<AudioConfig> audio_config,
      absl::optional<RemotePeerAudioConfig> remote_audio_config) {
    std::unique_ptr<TestAudioDeviceModule::Capturer> capturer;
    if (audio_config) {
      capturer = CreateAudioCapturer(audio_config.value());
    } else {
      // If we have no audio config we still need to provide some audio device.
      // In such case use generated capturer. Despite of we provided audio here,
      // in test media setup audio stream won't be added into peer connection.
      capturer = TestAudioDeviceModule::CreatePulsedNoiseCapturer(
          kGeneratedAudioMaxAmplitude, kDefaultSamplingFrequencyInHz);
    }
    RTC_DCHECK(capturer);

    if (audio_config && audio_config->input_dump_file_name) {
      capturer = absl::make_unique<test::CopyToFileAudioCapturer>(
          std::move(capturer), audio_config->input_dump_file_name.value());
    }

    std::unique_ptr<TestAudioDeviceModule::Renderer> renderer;
    if (remote_audio_config && remote_audio_config->output_file_name) {
      renderer = TestAudioDeviceModule::CreateBoundedWavFileWriter(
          remote_audio_config->output_file_name.value(),
          remote_audio_config->sampling_frequency_in_hz);
    } else {
      renderer = TestAudioDeviceModule::CreateDiscardRenderer(
          kDefaultSamplingFrequencyInHz);
    }

    return TestAudioDeviceModule::Create(task_queue_factory,
                                         std::move(capturer),
                                         std::move(renderer), /*speed=*/1.f);
  }

  std::unique_ptr<VideoEncoderFactory> CreateVideoEncoderFactory(
      PeerConnectionFactoryComponents* pcf_dependencies,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper,
      double bitrate_multiplier,
      std::map<std::string, absl::optional<int>>
          stream_required_spatial_index) {
    std::unique_ptr<VideoEncoderFactory> video_encoder_factory;
    if (pcf_dependencies->video_encoder_factory != nullptr) {
      video_encoder_factory =
          std::move(pcf_dependencies->video_encoder_factory);
    } else {
      video_encoder_factory = CreateBuiltinVideoEncoderFactory();
    }
    return video_analyzer_helper->WrapVideoEncoderFactory(
        std::move(video_encoder_factory), bitrate_multiplier,
        std::move(stream_required_spatial_index));
  }

  std::unique_ptr<VideoDecoderFactory> CreateVideoDecoderFactory(
      PeerConnectionFactoryComponents* pcf_dependencies,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper) {
    std::unique_ptr<VideoDecoderFactory> video_decoder_factory;
    if (pcf_dependencies->video_decoder_factory != nullptr) {
      video_decoder_factory =
          std::move(pcf_dependencies->video_decoder_factory);
    } else {
      video_decoder_factory = CreateBuiltinVideoDecoderFactory();
    }
    return video_analyzer_helper->WrapVideoDecoderFactory(
        std::move(video_decoder_factory));
  }

  std::unique_ptr<cricket::MediaEngineInterface> CreateMediaEngine(
      PeerConnectionFactoryComponents* pcf_dependencies,
      absl::optional<AudioConfig> audio_config,
      double bitrate_multiplier,
      std::map<std::string, absl::optional<int>> stream_required_spatial_index,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper,
      absl::optional<RemotePeerAudioConfig> remote_audio_config) {
    cricket::MediaEngineDependencies media_deps;
    media_deps.task_queue_factory = pcf_dependencies->task_queue_factory.get();
    media_deps.adm = CreateAudioDeviceModule(media_deps.task_queue_factory,
                                             std::move(audio_config),
                                             std::move(remote_audio_config));
    media_deps.audio_processing = audio_processing;
    media_deps.video_encoder_factory = CreateVideoEncoderFactory(
        pcf_dependencies, video_analyzer_helper, bitrate_multiplier,
        std::move(stream_required_spatial_index));
    media_deps.video_decoder_factory =
        CreateVideoDecoderFactory(pcf_dependencies, video_analyzer_helper);
    webrtc::SetMediaEngineDefaults(&media_deps);
    return cricket::CreateMediaEngine(std::move(media_deps));
  }

  // Creates PeerConnectionFactoryDependencies objects, providing entities
  // from InjectableComponents::PeerConnectionFactoryComponents and also
  // creating entities, that are required for correct injection of media quality
  // analyzers.
  PeerConnectionFactoryDependencies CreatePCFDependencies(
      std::unique_ptr<PeerConnectionFactoryComponents> pcf_dependencies,
      absl::optional<AudioConfig> audio_config,
      double bitrate_multiplier,
      std::map<std::string, absl::optional<int>> stream_required_spatial_index,
      VideoQualityAnalyzerInjectionHelper* video_analyzer_helper,
      rtc::Thread* network_thread,
      rtc::Thread* signaling_thread,
      absl::optional<RemotePeerAudioConfig> remote_audio_config,
      rtc::TaskQueue* task_queue) {
    PeerConnectionFactoryDependencies pcf_deps;
    pcf_deps.network_thread = network_thread;
    pcf_deps.signaling_thread = signaling_thread;
    pcf_deps.media_engine = CreateMediaEngine(
        pcf_dependencies.get(), std::move(audio_config), bitrate_multiplier,
        std::move(stream_required_spatial_index), video_analyzer_helper,
        std::move(remote_audio_config));

    pcf_deps.call_factory = std::move(pcf_dependencies->call_factory);
    pcf_deps.event_log_factory = std::move(pcf_dependencies->event_log_factory);
    pcf_deps.task_queue_factory =
        std::move(pcf_dependencies->task_queue_factory);

    if (pcf_dependencies->fec_controller_factory != nullptr) {
      pcf_deps.fec_controller_factory =
          std::move(pcf_dependencies->fec_controller_factory);
    }
    if (pcf_dependencies->network_controller_factory != nullptr) {
      pcf_deps.network_controller_factory =
          std::move(pcf_dependencies->network_controller_factory);
    }
    if (pcf_dependencies->media_transport_factory != nullptr) {
      pcf_deps.media_transport_factory =
          std::move(pcf_dependencies->media_transport_factory);
    }

    return pcf_deps;
  }

  // Creates PeerConnectionDependencies objects, providing entities
  // from InjectableComponents::PeerConnectionComponents.
  PeerConnectionDependencies CreatePCDependencies(
      std::unique_ptr<PeerConnectionComponents> pc_dependencies,
      PeerConnectionObserver* observer) {
    PeerConnectionDependencies pc_deps(observer);

    auto port_allocator = absl::make_unique<cricket::BasicPortAllocator>(
        pc_dependencies->network_manager);

    // This test does not support TCP
    int flags = cricket::PORTALLOCATOR_DISABLE_TCP;
    port_allocator->set_flags(port_allocator->flags() | flags);

    pc_deps.allocator = std::move(port_allocator);

    if (pc_dependencies->async_resolver_factory != nullptr) {
      pc_deps.async_resolver_factory =
          std::move(pc_dependencies->async_resolver_factory);
    }
    if (pc_dependencies->cert_generator != nullptr) {
      pc_deps.cert_generator = std::move(pc_dependencies->cert_generator);
    }
    if (pc_dependencies->tls_cert_verifier != nullptr) {
      pc_deps.tls_cert_verifier = std::move(pc_dependencies->tls_cert_verifier);
    }
    return pc_deps;
  }
};

}  // namespace

absl::optional<RemotePeerAudioConfig> TestPeer::CreateRemoteAudioConfig(
    absl::optional<AudioConfig> config) {
  if (!config) {
    return absl::nullopt;
  }
  return RemotePeerAudioConfig(config.value());
}

std::unique_ptr<TestPeer> TestPeer::CreateTestPeer(
    std::unique_ptr<InjectableComponents> components,
    std::unique_ptr<Params> params,
    std::unique_ptr<MockPeerConnectionObserver> observer,
    VideoQualityAnalyzerInjectionHelper* video_analyzer_helper,
    rtc::Thread* signaling_thread,
    absl::optional<RemotePeerAudioConfig> remote_audio_config,
    double bitrate_multiplier,
    rtc::TaskQueue* task_queue) {
  RTC_DCHECK(components);
  RTC_DCHECK(params);
  SetMandatoryEntities(components.get());
  params->rtc_configuration.sdp_semantics = SdpSemantics::kUnifiedPlan;

  TestPeerComponents tpc(std::move(components), *params, observer.get(),
                         video_analyzer_helper, signaling_thread,
                         std::move(remote_audio_config), bitrate_multiplier,
                         task_queue);

  return absl::WrapUnique(new TestPeer(
      tpc.peer_connection_factory, tpc.peer_connection, std::move(observer),
      std::move(params), tpc.audio_processing));
}

bool TestPeer::AddIceCandidates(
    std::vector<std::unique_ptr<IceCandidateInterface>> candidates) {
  bool success = true;
  for (auto& candidate : candidates) {
    if (!pc()->AddIceCandidate(candidate.get())) {
      std::string candidate_str;
      bool res = candidate->ToString(&candidate_str);
      RTC_CHECK(res);
      RTC_LOG(LS_ERROR) << "Failed to add ICE candidate, candidate_str="
                        << candidate_str;
      success = false;
    } else {
      remote_ice_candidates_.push_back(std::move(candidate));
    }
  }
  return success;
}

TestPeer::TestPeer(
    rtc::scoped_refptr<PeerConnectionFactoryInterface> pc_factory,
    rtc::scoped_refptr<PeerConnectionInterface> pc,
    std::unique_ptr<MockPeerConnectionObserver> observer,
    std::unique_ptr<Params> params,
    rtc::scoped_refptr<AudioProcessing> audio_processing)
    : PeerConnectionWrapper::PeerConnectionWrapper(std::move(pc_factory),
                                                   std::move(pc),
                                                   std::move(observer)),
      params_(std::move(params)),
      audio_processing_(audio_processing) {}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
