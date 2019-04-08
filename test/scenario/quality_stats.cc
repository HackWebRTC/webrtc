/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/quality_stats.h"

#include <utility>

#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/checks.h"
#include "rtc_base/event.h"

namespace webrtc {
namespace test {
namespace {
constexpr int kThumbWidth = 96;
constexpr int kThumbHeight = 96;
}  // namespace

VideoFrameMatcher::VideoFrameMatcher(
    std::vector<std::function<void(const VideoFramePair&)> >
        frame_pair_handlers)
    : frame_pair_handlers_(frame_pair_handlers), task_queue_("VideoAnalyzer") {}

VideoFrameMatcher::~VideoFrameMatcher() {
  task_queue_.SendTask([this] { Finalize(); });
}

void VideoFrameMatcher::RegisterLayer(int layer_id) {
  task_queue_.PostTask([this, layer_id] { layers_[layer_id] = VideoLayer(); });
}

void VideoFrameMatcher::OnCapturedFrame(const VideoFrame& frame,
                                        Timestamp at_time) {
  CapturedFrame captured;
  captured.id = next_capture_id_++;
  captured.capture_time = at_time;
  captured.frame = frame.video_frame_buffer();
  captured.thumb = ScaleVideoFrameBuffer(*frame.video_frame_buffer()->ToI420(),
                                         kThumbWidth, kThumbHeight),
  task_queue_.PostTask([this, captured]() {
    for (auto& layer : layers_) {
      CapturedFrame copy = captured;
      if (layer.second.last_decode) {
        copy.best_score = I420SSE(*captured.thumb->GetI420(),
                                  *layer.second.last_decode->thumb->GetI420());
        copy.best_decode = layer.second.last_decode;
      }
      layer.second.captured_frames.push_back(std::move(copy));
    }
  });
}

void VideoFrameMatcher::OnDecodedFrame(const VideoFrame& frame,
                                       Timestamp render_time,
                                       int layer_id) {
  rtc::scoped_refptr<DecodedFrame> decoded(new DecodedFrame{});
  decoded->render_time = render_time;
  decoded->frame = frame.video_frame_buffer();
  decoded->thumb = ScaleVideoFrameBuffer(*frame.video_frame_buffer()->ToI420(),
                                         kThumbWidth, kThumbHeight);
  decoded->render_time = render_time;

  task_queue_.PostTask([this, decoded, layer_id] {
    auto& layer = layers_[layer_id];
    decoded->id = layer.next_decoded_id++;
    layer.last_decode = decoded;
    for (auto& captured : layer.captured_frames) {
      double score =
          I420SSE(*captured.thumb->GetI420(), *decoded->thumb->GetI420());
      if (score < captured.best_score) {
        captured.best_score = score;
        captured.best_decode = decoded;
        captured.matched = false;
      } else {
        captured.matched = true;
      }
    }
    while (!layer.captured_frames.empty() &&
           layer.captured_frames.front().matched) {
      HandleMatch(layer.captured_frames.front(), layer_id);
      layer.captured_frames.pop_front();
    }
  });
}

bool VideoFrameMatcher::Active() const {
  return !frame_pair_handlers_.empty();
}

void VideoFrameMatcher::Finalize() {
  for (auto& layer : layers_) {
    while (!layer.second.captured_frames.empty()) {
      HandleMatch(layer.second.captured_frames.front(), layer.first);
      layer.second.captured_frames.pop_front();
    }
  }
}

ForwardingCapturedFrameTap::ForwardingCapturedFrameTap(
    Clock* clock,
    VideoFrameMatcher* matcher,
    rtc::VideoSourceInterface<VideoFrame>* source)
    : clock_(clock), matcher_(matcher), source_(source) {}

ForwardingCapturedFrameTap::~ForwardingCapturedFrameTap() {}

void ForwardingCapturedFrameTap::OnFrame(const VideoFrame& frame) {
  RTC_CHECK(sink_);
  matcher_->OnCapturedFrame(frame, Timestamp::ms(clock_->TimeInMilliseconds()));
  sink_->OnFrame(frame);
}
void ForwardingCapturedFrameTap::OnDiscardedFrame() {
  RTC_CHECK(sink_);
  discarded_count_++;
  sink_->OnDiscardedFrame();
}

void ForwardingCapturedFrameTap::AddOrUpdateSink(
    VideoSinkInterface<VideoFrame>* sink,
    const rtc::VideoSinkWants& wants) {
  sink_ = sink;
  source_->AddOrUpdateSink(this, wants);
}
void ForwardingCapturedFrameTap::RemoveSink(
    VideoSinkInterface<VideoFrame>* sink) {
  source_->RemoveSink(this);
  sink_ = nullptr;
}

DecodedFrameTap::DecodedFrameTap(VideoFrameMatcher* matcher, int layer_id)
    : matcher_(matcher), layer_id_(layer_id) {
  matcher_->RegisterLayer(layer_id_);
}

void DecodedFrameTap::OnFrame(const VideoFrame& frame) {
  matcher_->OnDecodedFrame(frame, Timestamp::ms(frame.render_time_ms()),
                           layer_id_);
}

VideoQualityAnalyzer::VideoQualityAnalyzer(
    VideoQualityAnalyzerConfig config,
    std::unique_ptr<RtcEventLogOutput> writer)
    : config_(config), writer_(std::move(writer)) {
  if (writer_) {
    PrintHeaders();
  }
}

VideoQualityAnalyzer::~VideoQualityAnalyzer() = default;

void VideoQualityAnalyzer::PrintHeaders() {
  writer_->Write(
      "capture_time render_time capture_width capture_height render_width "
      "render_height psnr\n");
}

std::function<void(const VideoFramePair&)> VideoQualityAnalyzer::Handler() {
  return [this](VideoFramePair pair) { HandleFramePair(pair); };
}

void VideoQualityAnalyzer::HandleFramePair(VideoFramePair sample) {
  double psnr = NAN;
  RTC_CHECK(sample.captured);
  ++stats_.captures_count;
  if (!sample.decoded) {
    ++stats_.lost_count;
  } else {
    psnr = I420PSNR(*sample.captured->ToI420(), *sample.decoded->ToI420());
    ++stats_.valid_count;
    stats_.end_to_end_seconds.AddSample(
        (sample.render_time - sample.capture_time).seconds<double>());
    stats_.psnr.AddSample(psnr);
  }
  if (writer_) {
    LogWriteFormat(writer_.get(), "%.3f %.3f %.3f %i %i %i %i %.3f\n",
                   sample.capture_time.seconds<double>(),
                   sample.render_time.seconds<double>(),
                   sample.captured->width(), sample.captured->height(),
                   sample.decoded->width(), sample.decoded->height(), psnr);
  }
}

VideoQualityStats VideoQualityAnalyzer::stats() const {
  return stats_;
}

}  // namespace test
}  // namespace webrtc
