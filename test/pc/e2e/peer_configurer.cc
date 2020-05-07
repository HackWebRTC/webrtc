/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/peer_configurer.h"

#include <set>

#include "test/testsupport/file_utils.h"

namespace webrtc {
namespace webrtc_pc_e2e {

using AudioConfig = PeerConnectionE2EQualityTestFixture::AudioConfig;
using VideoConfig = PeerConnectionE2EQualityTestFixture::VideoConfig;
using RunParams = PeerConnectionE2EQualityTestFixture::RunParams;
using VideoGeneratorType =
    PeerConnectionE2EQualityTestFixture::VideoGeneratorType;
using VideoCodecConfig = PeerConnectionE2EQualityTestFixture::VideoCodecConfig;

void SetDefaultValuesForMissingParams(
    RunParams* run_params,
    std::vector<std::unique_ptr<PeerConfigurerImpl>>* peers) {
  int video_counter = 0;
  int audio_counter = 0;
  std::set<std::string> video_labels;
  std::set<std::string> audio_labels;
  for (size_t i = 0; i < peers->size(); ++i) {
    auto* peer = peers->at(i).get();
    auto* p = peer->params();
    for (VideoConfig& video_config : p->video_configs) {
      if (!video_config.stream_label) {
        std::string label;
        do {
          label = "_auto_video_stream_label_" + std::to_string(video_counter);
          ++video_counter;
        } while (!video_labels.insert(label).second);
        video_config.stream_label = label;
      }
    }
    if (p->audio_config) {
      if (!p->audio_config->stream_label) {
        std::string label;
        do {
          label = "_auto_audio_stream_label_" + std::to_string(audio_counter);
          ++audio_counter;
        } while (!audio_labels.insert(label).second);
        p->audio_config->stream_label = label;
      }
    }
  }

  if (run_params->video_codecs.empty()) {
    run_params->video_codecs.push_back(
        VideoCodecConfig(cricket::kVp8CodecName));
  }
}

void ValidateParams(
    const RunParams& run_params,
    const std::vector<std::unique_ptr<PeerConfigurerImpl>>& peers) {
  RTC_CHECK_GT(run_params.video_encoder_bitrate_multiplier, 0.0);

  std::set<std::string> video_labels;
  std::set<std::string> audio_labels;
  int media_streams_count = 0;

  bool has_simulcast = false;
  for (size_t i = 0; i < peers.size(); ++i) {
    Params* p = peers[i]->params();
    if (p->audio_config) {
      media_streams_count++;
    }
    media_streams_count += p->video_configs.size();

    // Validate that all video stream labels are unique.
    for (const VideoConfig& video_config : p->video_configs) {
      RTC_CHECK(video_config.stream_label);
      bool inserted =
          video_labels.insert(video_config.stream_label.value()).second;
      RTC_CHECK(inserted) << "Duplicate video_config.stream_label="
                          << video_config.stream_label.value();

      if (video_config.simulcast_config) {
        has_simulcast = true;
        RTC_CHECK(!video_config.max_encode_bitrate_bps)
            << "Setting max encode bitrate is not implemented for simulcast.";
        RTC_CHECK(!video_config.min_encode_bitrate_bps)
            << "Setting min encode bitrate is not implemented for simulcast.";
      }
    }
    if (p->audio_config) {
      bool inserted =
          audio_labels.insert(p->audio_config->stream_label.value()).second;
      RTC_CHECK(inserted) << "Duplicate audio_config.stream_label="
                          << p->audio_config->stream_label.value();
      // Check that if mode input file name specified only if mode is kFile.
      if (p->audio_config.value().mode == AudioConfig::Mode::kGenerated) {
        RTC_CHECK(!p->audio_config.value().input_file_name);
      }
      if (p->audio_config.value().mode == AudioConfig::Mode::kFile) {
        RTC_CHECK(p->audio_config.value().input_file_name);
        RTC_CHECK(
            test::FileExists(p->audio_config.value().input_file_name.value()))
            << p->audio_config.value().input_file_name.value()
            << " doesn't exist";
      }
    }
  }
  if (has_simulcast) {
    RTC_CHECK_EQ(run_params.video_codecs.size(), 1)
        << "Only 1 video codec is supported when simulcast is enabled in at "
        << "least 1 video config";
  }

  RTC_CHECK_GT(media_streams_count, 0) << "No media in the call.";
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
