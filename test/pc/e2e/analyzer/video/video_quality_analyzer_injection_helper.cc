/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/video_quality_analyzer_injection_helper.h"

#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "test/pc/e2e/analyzer/video/quality_analyzing_video_decoder.h"
#include "test/pc/e2e/analyzer/video/quality_analyzing_video_encoder.h"
#include "test/pc/e2e/analyzer/video/simulcast_dummy_buffer_helper.h"
#include "test/video_renderer.h"

namespace webrtc {
namespace webrtc_pc_e2e {

namespace {

class VideoWriter final : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  VideoWriter(test::VideoFrameWriter* video_writer)
      : video_writer_(video_writer) {}
  ~VideoWriter() override = default;

  void OnFrame(const VideoFrame& frame) override {
    bool result = video_writer_->WriteFrame(frame);
    RTC_CHECK(result) << "Failed to write frame";
  }

 private:
  test::VideoFrameWriter* video_writer_;
};

class AnalyzingVideoSource : public rtc::VideoSourceInterface<VideoFrame> {
 public:
  AnalyzingVideoSource(
      std::string stream_label,
      VideoQualityAnalyzerInterface* analyzer,
      std::unique_ptr<test::TestVideoCapturer> test_capturer,
      std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks)
      : test_capturer_(std::move(test_capturer)),
        analyzing_sink_(stream_label, analyzer, &broadcaster_),
        sinks_(std::move(sinks)) {
    for (auto& sink : sinks_) {
      broadcaster_.AddOrUpdateSink(sink.get(), rtc::VideoSinkWants());
    }
  }

  void AddOrUpdateSink(rtc::VideoSinkInterface<VideoFrame>* sink,
                       const rtc::VideoSinkWants& wants) override {
    broadcaster_.AddOrUpdateSink(sink, wants);
    test_capturer_->AddOrUpdateSink(&analyzing_sink_, broadcaster_.wants());
  }

  void RemoveSink(rtc::VideoSinkInterface<VideoFrame>* sink) override {
    broadcaster_.RemoveSink(sink);
    test_capturer_->AddOrUpdateSink(&analyzing_sink_, broadcaster_.wants());
  }

 private:
  class AnalyzerCapturingVideoSink
      : public rtc::VideoSinkInterface<VideoFrame> {
   public:
    AnalyzerCapturingVideoSink(std::string stream_label,
                               VideoQualityAnalyzerInterface* analyzer,
                               rtc::VideoBroadcaster* broadcaster)
        : stream_label_(std::move(stream_label)),
          analyzer_(analyzer),
          broadcaster_(broadcaster) {}
    ~AnalyzerCapturingVideoSink() override = default;

    void OnFrame(const VideoFrame& source_frame) override {
      // Copy VideoFrame to be able to set id on it.
      VideoFrame frame = source_frame;
      uint16_t frame_id = analyzer_->OnFrameCaptured(stream_label_, frame);
      frame.set_id(frame_id);
      broadcaster_->OnFrame(frame);
    }

   private:
    const std::string stream_label_;
    VideoQualityAnalyzerInterface* const analyzer_;
    rtc::VideoBroadcaster* const broadcaster_;
  };

  rtc::VideoBroadcaster broadcaster_;
  std::unique_ptr<test::TestVideoCapturer> test_capturer_;
  AnalyzerCapturingVideoSink analyzing_sink_;
  const std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>
      sinks_;
};

// Intercepts generated frames and passes them also to video quality analyzer
// and to provided sinks.
class AnalyzingFrameGenerator final : public test::FrameGenerator {
 public:
  AnalyzingFrameGenerator(
      std::string stream_label,
      std::unique_ptr<test::FrameGenerator> delegate,
      VideoQualityAnalyzerInterface* analyzer,
      std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks)
      : stream_label_(std::move(stream_label)),
        delegate_(std::move(delegate)),
        analyzer_(analyzer),
        sinks_(std::move(sinks)) {}
  ~AnalyzingFrameGenerator() override = default;

  VideoFrame* NextFrame() override {
    VideoFrame* frame = delegate_->NextFrame();

    uint16_t frame_id = analyzer_->OnFrameCaptured(stream_label_, *frame);
    frame->set_id(frame_id);

    for (auto& sink : sinks_) {
      sink->OnFrame(*frame);
    }
    return frame;
  }

  void ChangeResolution(size_t width, size_t height) override {
    delegate_->ChangeResolution(width, height);
  }

 private:
  const std::string stream_label_;
  std::unique_ptr<test::FrameGenerator> delegate_;
  VideoQualityAnalyzerInterface* const analyzer_;
  const std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>
      sinks_;
};

// Implements the video sink, that forwards rendered frames to the video quality
// analyzer and provided sinks.
class AnalyzingVideoSink final : public rtc::VideoSinkInterface<VideoFrame> {
 public:
  AnalyzingVideoSink(
      VideoQualityAnalyzerInterface* analyzer,
      std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks)
      : analyzer_(analyzer), sinks_(std::move(sinks)) {
    RTC_DCHECK(analyzer_);
  }
  ~AnalyzingVideoSink() override = default;

  void OnFrame(const VideoFrame& frame) override {
    if (IsDummyFrameBuffer(frame.video_frame_buffer()->ToI420())) {
      // This is dummy frame, so we  don't need to process it further.
      return;
    }
    analyzer_->OnFrameRendered(frame);
    for (auto& sink : sinks_) {
      sink->OnFrame(frame);
    }
  }

 private:
  VideoQualityAnalyzerInterface* const analyzer_;
  const std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>>
      sinks_;
};

}  // namespace

VideoQualityAnalyzerInjectionHelper::VideoQualityAnalyzerInjectionHelper(
    std::unique_ptr<VideoQualityAnalyzerInterface> analyzer,
    EncodedImageDataInjector* injector,
    EncodedImageDataExtractor* extractor)
    : analyzer_(std::move(analyzer)),
      injector_(injector),
      extractor_(extractor),
      encoding_entities_id_generator_(std::make_unique<IntIdGenerator>(1)) {
  RTC_DCHECK(injector_);
  RTC_DCHECK(extractor_);
}
VideoQualityAnalyzerInjectionHelper::~VideoQualityAnalyzerInjectionHelper() =
    default;

std::unique_ptr<VideoEncoderFactory>
VideoQualityAnalyzerInjectionHelper::WrapVideoEncoderFactory(
    std::unique_ptr<VideoEncoderFactory> delegate,
    double bitrate_multiplier,
    std::map<std::string, absl::optional<int>> stream_required_spatial_index)
    const {
  return std::make_unique<QualityAnalyzingVideoEncoderFactory>(
      std::move(delegate), bitrate_multiplier,
      std::move(stream_required_spatial_index),
      encoding_entities_id_generator_.get(), injector_, analyzer_.get());
}

std::unique_ptr<VideoDecoderFactory>
VideoQualityAnalyzerInjectionHelper::WrapVideoDecoderFactory(
    std::unique_ptr<VideoDecoderFactory> delegate) const {
  return std::make_unique<QualityAnalyzingVideoDecoderFactory>(
      std::move(delegate), encoding_entities_id_generator_.get(), extractor_,
      analyzer_.get());
}

rtc::scoped_refptr<TestVideoCapturerVideoTrackSource>
VideoQualityAnalyzerInjectionHelper::CreateVideoTrackSource(
    const VideoConfig& config,
    std::unique_ptr<test::TestVideoCapturer> capturer,
    test::VideoFrameWriter* writer,
    bool is_screencast) const {
  std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks;
  if (writer) {
    sinks.push_back(std::make_unique<VideoWriter>(writer));
  }
  if (config.show_on_screen) {
    sinks.push_back(absl::WrapUnique(
        test::VideoRenderer::Create((*config.stream_label + "-capture").c_str(),
                                    config.width, config.height)));
  }
  return new rtc::RefCountedObject<TestVideoCapturerVideoTrackSource>(
      std::make_unique<AnalyzingVideoSource>(
          std::move(*config.stream_label), analyzer_.get(), std::move(capturer),
          std::move(sinks)),
      is_screencast);
}

std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>
VideoQualityAnalyzerInjectionHelper::CreateVideoSink(
    const VideoConfig& config,
    test::VideoFrameWriter* writer) const {
  std::vector<std::unique_ptr<rtc::VideoSinkInterface<VideoFrame>>> sinks;
  if (writer) {
    sinks.push_back(std::make_unique<VideoWriter>(writer));
  }
  if (config.show_on_screen) {
    sinks.push_back(absl::WrapUnique(
        test::VideoRenderer::Create((*config.stream_label + "-render").c_str(),
                                    config.width, config.height)));
  }
  return std::make_unique<AnalyzingVideoSink>(analyzer_.get(),
                                              std::move(sinks));
}

void VideoQualityAnalyzerInjectionHelper::Start(std::string test_case_name,
                                                int max_threads_count) {
  analyzer_->Start(std::move(test_case_name), max_threads_count);
}

void VideoQualityAnalyzerInjectionHelper::OnStatsReports(
    const std::string& pc_label,
    const StatsReports& stats_reports) {
  analyzer_->OnStatsReports(pc_label, stats_reports);
}

void VideoQualityAnalyzerInjectionHelper::Stop() {
  analyzer_->Stop();
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
