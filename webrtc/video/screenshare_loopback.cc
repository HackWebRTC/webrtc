/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include <map>

#include "gflags/gflags.h"
#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/base/checks.h"
#include "webrtc/test/field_trial.h"
#include "webrtc/test/frame_generator.h"
#include "webrtc/test/frame_generator_capturer.h"
#include "webrtc/test/run_test.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/typedefs.h"
#include "webrtc/video/loopback.h"
#include "webrtc/video/video_send_stream.h"

namespace webrtc {
namespace flags {

DEFINE_int32(width, 1850, "Video width (crops source).");
size_t Width() {
  return static_cast<size_t>(FLAGS_width);
}

DEFINE_int32(height, 1110, "Video height (crops source).");
size_t Height() {
  return static_cast<size_t>(FLAGS_height);
}

DEFINE_int32(fps, 5, "Frames per second.");
int Fps() {
  return static_cast<int>(FLAGS_fps);
}

DEFINE_int32(slide_change_interval,
             10,
             "Interval (in seconds) between simulated slide changes.");
int SlideChangeInterval() {
  return static_cast<int>(FLAGS_slide_change_interval);
}

DEFINE_int32(
    scroll_duration,
    0,
    "Duration (in seconds) during which a slide will be scrolled into place.");
int ScrollDuration() {
  return static_cast<int>(FLAGS_scroll_duration);
}

DEFINE_int32(min_bitrate, 50, "Minimum video bitrate.");
size_t MinBitrate() {
  return static_cast<size_t>(FLAGS_min_bitrate);
}

DEFINE_int32(tl0_bitrate, 200, "Temporal layer 0 target bitrate.");
size_t StartBitrate() {
  return static_cast<size_t>(FLAGS_tl0_bitrate);
}

DEFINE_int32(tl1_bitrate, 2000, "Temporal layer 1 target bitrate.");
size_t MaxBitrate() {
  return static_cast<size_t>(FLAGS_tl1_bitrate);
}

DEFINE_int32(num_temporal_layers, 2, "Number of temporal layers to use.");
int NumTemporalLayers() {
  return static_cast<int>(FLAGS_num_temporal_layers);
}

DEFINE_int32(num_spatial_layers, 1, "Number of spatial layers to use.");
int NumSpatialLayers() {
  return static_cast<int>(FLAGS_num_spatial_layers);
}

DEFINE_int32(
    tl_discard_threshold,
    0,
    "Discard TLs with id greater or equal the threshold. 0 to disable.");
int TLDiscardThreshold() {
  return static_cast<int>(FLAGS_tl_discard_threshold);
}

DEFINE_int32(
    sl_discard_threshold,
    0,
    "Discard SLs with id greater or equal the threshold. 0 to disable.");
int SLDiscardThreshold() {
  return static_cast<int>(FLAGS_sl_discard_threshold);
}

DEFINE_int32(min_transmit_bitrate, 400, "Min transmit bitrate incl. padding.");
int MinTransmitBitrate() {
  return FLAGS_min_transmit_bitrate;
}

DEFINE_string(codec, "VP8", "Video codec to use.");
std::string Codec() {
  return static_cast<std::string>(FLAGS_codec);
}

DEFINE_int32(loss_percent, 0, "Percentage of packets randomly lost.");
int LossPercent() {
  return static_cast<int>(FLAGS_loss_percent);
}

DEFINE_int32(link_capacity,
             0,
             "Capacity (kbps) of the fake link. 0 means infinite.");
int LinkCapacity() {
  return static_cast<int>(FLAGS_link_capacity);
}

DEFINE_int32(queue_size, 0, "Size of the bottleneck link queue in packets.");
int QueueSize() {
  return static_cast<int>(FLAGS_queue_size);
}

DEFINE_int32(avg_propagation_delay_ms,
             0,
             "Average link propagation delay in ms.");
int AvgPropagationDelayMs() {
  return static_cast<int>(FLAGS_avg_propagation_delay_ms);
}

DEFINE_int32(std_propagation_delay_ms,
             0,
             "Link propagation delay standard deviation in ms.");
int StdPropagationDelayMs() {
  return static_cast<int>(FLAGS_std_propagation_delay_ms);
}

DEFINE_bool(logs, false, "print logs to stderr");

DEFINE_string(
    force_fieldtrials,
    "",
    "Field trials control experimental feature code which can be forced. "
    "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
    " will assign the group Enable to field trial WebRTC-FooFeature. Multiple "
    "trials are separated by \"/\"");
}  // namespace flags

class ScreenshareLoopback : public test::Loopback {
 public:
  explicit ScreenshareLoopback(const Config& config) : Loopback(config) {
    CHECK_GE(config.num_temporal_layers, 1u);
    CHECK_LE(config.num_temporal_layers, 2u);
    CHECK_GE(config.num_spatial_layers, 1u);
    CHECK_LE(config.num_spatial_layers, 5u);
    CHECK(config.num_spatial_layers == 1 || config.codec == "VP9");
    CHECK(config.num_spatial_layers == 1 || config.num_temporal_layers == 1);
    CHECK_LT(config.tl_discard_threshold, config.num_temporal_layers);
    CHECK_LT(config.sl_discard_threshold, config.num_spatial_layers);

    vp8_settings_ = VideoEncoder::GetDefaultVp8Settings();
    vp8_settings_.denoisingOn = false;
    vp8_settings_.frameDroppingOn = false;
    vp8_settings_.numberOfTemporalLayers =
        static_cast<unsigned char>(config.num_temporal_layers);

    vp9_settings_ = VideoEncoder::GetDefaultVp9Settings();
    vp9_settings_.denoisingOn = false;
    vp9_settings_.frameDroppingOn = false;
    vp9_settings_.numberOfTemporalLayers =
        static_cast<unsigned char>(config.num_temporal_layers);
    vp9_settings_.numberOfSpatialLayers =
        static_cast<unsigned char>(config.num_spatial_layers);
  }
  virtual ~ScreenshareLoopback() {}

 protected:
  VideoEncoderConfig CreateEncoderConfig() override {
    VideoEncoderConfig encoder_config(test::Loopback::CreateEncoderConfig());
    VideoStream* stream = &encoder_config.streams[0];
    encoder_config.content_type = VideoEncoderConfig::ContentType::kScreen;
    encoder_config.min_transmit_bitrate_bps = flags::MinTransmitBitrate();
    int num_temporal_layers;
    if (config_.codec == "VP8") {
      encoder_config.encoder_specific_settings = &vp8_settings_;
      num_temporal_layers = vp8_settings_.numberOfTemporalLayers;
    } else if (config_.codec == "VP9") {
      encoder_config.encoder_specific_settings = &vp9_settings_;
      num_temporal_layers = vp9_settings_.numberOfTemporalLayers;
    } else {
      RTC_NOTREACHED() << "Codec not supported!";
      abort();
    }
    stream->temporal_layer_thresholds_bps.clear();
    stream->target_bitrate_bps =
        static_cast<int>(config_.start_bitrate_kbps) * 1000;
    if (num_temporal_layers == 2) {
      stream->temporal_layer_thresholds_bps.push_back(
          stream->target_bitrate_bps);
    }
    return encoder_config;
  }

  test::VideoCapturer* CreateCapturer(VideoSendStream* send_stream) override {
    std::vector<std::string> slides;
    slides.push_back(test::ResourcePath("web_screenshot_1850_1110", "yuv"));
    slides.push_back(test::ResourcePath("presentation_1850_1110", "yuv"));
    slides.push_back(test::ResourcePath("photo_1850_1110", "yuv"));
    slides.push_back(test::ResourcePath("difficult_photo_1850_1110", "yuv"));

    // Fixed for input resolution for prerecorded screenshare content.
    const size_t kWidth = 1850;
    const size_t kHeight = 1110;
    CHECK_LE(flags::Width(), kWidth);
    CHECK_LE(flags::Height(), kHeight);
    CHECK_GT(flags::SlideChangeInterval(), 0);
    const int kPauseDurationMs =
        (flags::SlideChangeInterval() - flags::ScrollDuration()) * 1000;
    CHECK_LE(flags::ScrollDuration(), flags::SlideChangeInterval());

    test::FrameGenerator* frame_generator =
        test::FrameGenerator::CreateScrollingInputFromYuvFiles(
            Clock::GetRealTimeClock(), slides, kWidth, kHeight, flags::Width(),
            flags::Height(), flags::ScrollDuration() * 1000, kPauseDurationMs);

    test::FrameGeneratorCapturer* capturer(new test::FrameGeneratorCapturer(
        clock_, send_stream->Input(), frame_generator, flags::Fps()));
    EXPECT_TRUE(capturer->Init());
    return capturer;
  }

  VideoCodecVP8 vp8_settings_;
  VideoCodecVP9 vp9_settings_;
};

void Loopback() {
  test::Loopback::Config config{flags::Width(),
                                flags::Height(),
                                flags::Fps(),
                                flags::MinBitrate(),
                                flags::StartBitrate(),
                                flags::MaxBitrate(),
                                flags::MinTransmitBitrate(),
                                flags::Codec(),
                                flags::NumTemporalLayers(),
                                flags::NumSpatialLayers(),
                                flags::TLDiscardThreshold(),
                                flags::SLDiscardThreshold(),
                                flags::LossPercent(),
                                flags::LinkCapacity(),
                                flags::QueueSize(),
                                flags::AvgPropagationDelayMs(),
                                flags::StdPropagationDelayMs(),
                                flags::FLAGS_logs};
  ScreenshareLoopback loopback(config);
  loopback.Run();
}
}  // namespace webrtc

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  webrtc::test::InitFieldTrialsFromString(
      webrtc::flags::FLAGS_force_fieldtrials);
  webrtc::test::RunTest(webrtc::Loopback);
  return 0;
}
