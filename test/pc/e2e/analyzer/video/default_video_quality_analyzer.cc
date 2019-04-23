/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/default_video_quality_analyzer.h"

#include <algorithm>
#include <utility>

#include "absl/memory/memory.h"
#include "api/units/time_delta.h"
#include "common_video/libyuv/include/webrtc_libyuv.h"
#include "rtc_base/logging.h"
#include "test/testsupport/perf_test.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

constexpr int kMaxActiveComparisons = 10;
constexpr int kFreezeThresholdMs = 150;
constexpr int kMicrosPerSecond = 1000000;

void LogFrameCounters(const std::string& name, const FrameCounters& counters) {
  RTC_LOG(INFO) << "[" << name << "] Captured    : " << counters.captured;
  RTC_LOG(INFO) << "[" << name << "] Pre encoded : " << counters.pre_encoded;
  RTC_LOG(INFO) << "[" << name << "] Encoded     : " << counters.encoded;
  RTC_LOG(INFO) << "[" << name << "] Received    : " << counters.received;
  RTC_LOG(INFO) << "[" << name << "] Rendered    : " << counters.rendered;
  RTC_LOG(INFO) << "[" << name << "] Dropped     : " << counters.dropped;
}

void LogStreamInternalStats(const std::string& name, const StreamStats& stats) {
  RTC_LOG(INFO) << "[" << name
                << "] Dropped by encoder     : " << stats.dropped_by_encoder;
  RTC_LOG(INFO) << "[" << name << "] Dropped before encoder : "
                << stats.dropped_before_encoder;
}

}  // namespace

void RateCounter::AddEvent(Timestamp event_time) {
  if (event_first_time_.IsMinusInfinity()) {
    event_first_time_ = event_time;
  }
  event_last_time_ = event_time;
  event_count_++;
}

double RateCounter::GetEventsPerSecond() const {
  RTC_DCHECK(!IsEmpty());
  // Divide on us and multiply on kMicrosPerSecond to correctly process cases
  // where there were too small amount of events, so difference is less then 1
  // sec. We can use us here, because Timestamp has us resolution.
  return static_cast<double>(event_count_) /
         (event_last_time_ - event_first_time_).us() * kMicrosPerSecond;
}

DefaultVideoQualityAnalyzer::DefaultVideoQualityAnalyzer()
    : clock_(Clock::GetRealTimeClock()) {}
DefaultVideoQualityAnalyzer::~DefaultVideoQualityAnalyzer() {
  Stop();
}

void DefaultVideoQualityAnalyzer::Start(std::string test_case_name,
                                        int max_threads_count) {
  test_label_ = std::move(test_case_name);
  for (int i = 0; i < max_threads_count; i++) {
    auto thread = absl::make_unique<rtc::PlatformThread>(
        &DefaultVideoQualityAnalyzer::ProcessComparisonsThread, this,
        ("DefaultVideoQualityAnalyzerWorker-" + std::to_string(i)).data(),
        rtc::ThreadPriority::kNormalPriority);
    thread->Start();
    thread_pool_.push_back(std::move(thread));
  }
  {
    rtc::CritScope crit(&lock_);
    state_ = State::kActive;
  }
}

uint16_t DefaultVideoQualityAnalyzer::OnFrameCaptured(
    const std::string& stream_label,
    const webrtc::VideoFrame& frame) {
  // |next_frame_id| is atomic, so we needn't lock here.
  uint16_t frame_id = next_frame_id_++;
  {
    // Ensure stats for this stream exists.
    rtc::CritScope crit(&comparison_lock_);
    if (stream_stats_.find(stream_label) == stream_stats_.end()) {
      stream_stats_.insert({stream_label, StreamStats()});
      // Assume that the first freeze was before first stream frame captured.
      // This way time before the first freeze would be counted as time between
      // freezes.
      stream_last_freeze_end_time_.insert({stream_label, Now()});
    }
  }
  {
    rtc::CritScope crit(&lock_);
    frame_counters_.captured++;
    stream_frame_counters_[stream_label].captured++;

    StreamState* state = &stream_states_[stream_label];
    state->frame_ids.push_back(frame_id);
    // Update frames in flight info.
    auto it = captured_frames_in_flight_.find(frame_id);
    if (it != captured_frames_in_flight_.end()) {
      // We overflow uint16_t and hit previous frame id and this frame is still
      // in flight. It means that this stream wasn't rendered for long time and
      // we need to process existing frame as dropped.
      auto stats_it = frame_stats_.find(frame_id);
      RTC_DCHECK(stats_it != frame_stats_.end());

      RTC_DCHECK(frame_id == state->frame_ids.front());
      state->frame_ids.pop_front();
      frame_counters_.dropped++;
      stream_frame_counters_[stream_label].dropped++;
      AddComparison(it->second, absl::nullopt, true, stats_it->second);

      captured_frames_in_flight_.erase(it);
      frame_stats_.erase(stats_it);
    }
    captured_frames_in_flight_.insert(
        std::pair<uint16_t, VideoFrame>(frame_id, frame));
    // Set frame id on local copy of the frame
    captured_frames_in_flight_.at(frame_id).set_id(frame_id);
    frame_stats_.insert(std::pair<uint16_t, FrameStats>(
        frame_id, FrameStats(stream_label, /*captured_time=*/Now())));
  }
  return frame_id;
}

void DefaultVideoQualityAnalyzer::OnFramePreEncode(
    const webrtc::VideoFrame& frame) {
  rtc::CritScope crit(&lock_);
  auto it = frame_stats_.find(frame.id());
  RTC_DCHECK(it != frame_stats_.end());
  frame_counters_.pre_encoded++;
  stream_frame_counters_[it->second.stream_label].pre_encoded++;
  it->second.pre_encode_time = Now();
}

void DefaultVideoQualityAnalyzer::OnFrameEncoded(
    uint16_t frame_id,
    const webrtc::EncodedImage& encoded_image) {
  rtc::CritScope crit(&lock_);
  auto it = frame_stats_.find(frame_id);
  RTC_DCHECK(it != frame_stats_.end());
  RTC_DCHECK(it->second.encoded_time.IsInfinite())
      << "Received multiple spatial layers for stream_label="
      << it->second.stream_label;
  frame_counters_.encoded++;
  stream_frame_counters_[it->second.stream_label].encoded++;
  it->second.encoded_time = Now();
}

void DefaultVideoQualityAnalyzer::OnFrameDropped(
    webrtc::EncodedImageCallback::DropReason reason) {
  // Here we do nothing, because we will see this drop on renderer side.
}

void DefaultVideoQualityAnalyzer::OnFrameReceived(
    uint16_t frame_id,
    const webrtc::EncodedImage& input_image) {
  rtc::CritScope crit(&lock_);
  auto it = frame_stats_.find(frame_id);
  RTC_DCHECK(it != frame_stats_.end());
  RTC_DCHECK(it->second.received_time.IsInfinite())
      << "Received multiple spatial layers for stream_label="
      << it->second.stream_label;
  frame_counters_.received++;
  stream_frame_counters_[it->second.stream_label].received++;
  it->second.received_time = Now();
}

void DefaultVideoQualityAnalyzer::OnFrameDecoded(
    const webrtc::VideoFrame& frame,
    absl::optional<int32_t> decode_time_ms,
    absl::optional<uint8_t> qp) {
  rtc::CritScope crit(&lock_);
  auto it = frame_stats_.find(frame.id());
  RTC_DCHECK(it != frame_stats_.end());
  frame_counters_.decoded++;
  stream_frame_counters_[it->second.stream_label].decoded++;
  it->second.decoded_time = Now();
}

void DefaultVideoQualityAnalyzer::OnFrameRendered(
    const webrtc::VideoFrame& frame) {
  rtc::CritScope crit(&lock_);
  auto stats_it = frame_stats_.find(frame.id());
  RTC_DCHECK(stats_it != frame_stats_.end());
  FrameStats* frame_stats = &stats_it->second;
  // Update frames counters.
  frame_counters_.rendered++;
  stream_frame_counters_[frame_stats->stream_label].rendered++;

  // Update current frame stats.
  frame_stats->rendered_time = Now();
  frame_stats->rendered_frame_width = frame.width();
  frame_stats->rendered_frame_height = frame.height();

  // Find corresponding captured frame.
  auto frame_it = captured_frames_in_flight_.find(frame.id());
  RTC_DCHECK(frame_it != captured_frames_in_flight_.end());
  const VideoFrame& captured_frame = frame_it->second;

  // After we received frame here we need to check if there are any dropped
  // frames between this one and last one, that was rendered for this video
  // stream.

  const std::string& stream_label = frame_stats->stream_label;
  StreamState* state = &stream_states_[stream_label];
  int dropped_count = 0;
  while (!state->frame_ids.empty() && state->frame_ids.front() != frame.id()) {
    dropped_count++;
    uint16_t dropped_frame_id = state->frame_ids.front();
    state->frame_ids.pop_front();
    // Frame with id |dropped_frame_id| was dropped. We need:
    // 1. Update global and stream frame counters
    // 2. Extract corresponding frame from |captured_frames_in_flight_|
    // 3. Extract corresponding frame stats from |frame_stats_|
    // 4. Send extracted frame to comparison with dropped=true
    // 5. Cleanup dropped frame
    frame_counters_.dropped++;
    stream_frame_counters_[stream_label].dropped++;

    auto dropped_frame_stats_it = frame_stats_.find(dropped_frame_id);
    RTC_DCHECK(dropped_frame_stats_it != frame_stats_.end());
    auto dropped_frame_it = captured_frames_in_flight_.find(dropped_frame_id);
    RTC_CHECK(dropped_frame_it != captured_frames_in_flight_.end());

    AddComparison(dropped_frame_it->second, absl::nullopt, true,
                  dropped_frame_stats_it->second);

    frame_stats_.erase(dropped_frame_stats_it);
    captured_frames_in_flight_.erase(dropped_frame_it);
  }
  RTC_DCHECK(!state->frame_ids.empty());
  state->frame_ids.pop_front();

  if (state->last_rendered_frame_time) {
    frame_stats->prev_frame_rendered_time =
        state->last_rendered_frame_time.value();
  }
  state->last_rendered_frame_time = frame_stats->rendered_time;
  {
    rtc::CritScope cr(&comparison_lock_);
    stream_stats_[stream_label].skipped_between_rendered.AddSample(
        dropped_count);
  }
  AddComparison(captured_frame, frame, false, *frame_stats);

  captured_frames_in_flight_.erase(frame_it);
  frame_stats_.erase(stats_it);
}

void DefaultVideoQualityAnalyzer::OnEncoderError(
    const webrtc::VideoFrame& frame,
    int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Encoder error for frame.id=" << frame.id()
                    << ", code=" << error_code;
}

void DefaultVideoQualityAnalyzer::OnDecoderError(uint16_t frame_id,
                                                 int32_t error_code) {
  RTC_LOG(LS_ERROR) << "Decoder error for frame_id=" << frame_id
                    << ", code=" << error_code;
}

void DefaultVideoQualityAnalyzer::Stop() {
  {
    rtc::CritScope crit(&lock_);
    if (state_ == State::kStopped) {
      return;
    }
    state_ = State::kStopped;
  }
  comparison_available_event_.Set();
  for (auto& thread : thread_pool_) {
    thread->Stop();
  }
  // PlatformThread have to be deleted on the same thread, where it was created
  thread_pool_.clear();

  // Perform final Metrics update. On this place analyzer is stopped and no one
  // holds any locks.
  {
    // Time between freezes.
    // Count time since the last freeze to the end of the call as time
    // between freezes.
    rtc::CritScope crit1(&lock_);
    rtc::CritScope crit2(&comparison_lock_);
    for (auto& item : stream_stats_) {
      if (item.second.freeze_time_ms.IsEmpty()) {
        continue;
      }
      const StreamState& state = stream_states_[item.first];
      if (state.last_rendered_frame_time) {
        item.second.time_between_freezes_ms.AddSample(
            (state.last_rendered_frame_time.value() -
             stream_last_freeze_end_time_.at(item.first))
                .ms());
      }
    }
  }
  ReportResults();
}

std::string DefaultVideoQualityAnalyzer::GetStreamLabel(uint16_t frame_id) {
  rtc::CritScope crit1(&lock_);
  auto it = frame_stats_.find(frame_id);
  RTC_DCHECK(it != frame_stats_.end()) << "Unknown frame_id=" << frame_id;
  return it->second.stream_label;
}

std::set<std::string> DefaultVideoQualityAnalyzer::GetKnownVideoStreams()
    const {
  rtc::CritScope crit2(&comparison_lock_);
  std::set<std::string> out;
  for (auto& item : stream_stats_) {
    out.insert(item.first);
  }
  return out;
}

const FrameCounters& DefaultVideoQualityAnalyzer::GetGlobalCounters() {
  rtc::CritScope crit(&lock_);
  return frame_counters_;
}

const std::map<std::string, FrameCounters>&
DefaultVideoQualityAnalyzer::GetPerStreamCounters() const {
  rtc::CritScope crit(&lock_);
  return stream_frame_counters_;
}

std::map<std::string, StreamStats> DefaultVideoQualityAnalyzer::GetStats()
    const {
  rtc::CritScope cri(&comparison_lock_);
  return stream_stats_;
}

AnalyzerStats DefaultVideoQualityAnalyzer::GetAnalyzerStats() const {
  rtc::CritScope crit(&comparison_lock_);
  return analyzer_stats_;
}

void DefaultVideoQualityAnalyzer::AddComparison(
    absl::optional<VideoFrame> captured,
    absl::optional<VideoFrame> rendered,
    bool dropped,
    FrameStats frame_stats) {
  rtc::CritScope crit(&comparison_lock_);
  analyzer_stats_.comparisons_queue_size.AddSample(comparisons_.size());
  // If there too many computations waiting in the queue, we won't provide
  // frames itself to make future computations lighter.
  if (comparisons_.size() >= kMaxActiveComparisons) {
    comparisons_.emplace_back(dropped, frame_stats);
  } else {
    comparisons_.emplace_back(std::move(captured), std::move(rendered), dropped,
                              frame_stats);
  }
  comparison_available_event_.Set();
}

void DefaultVideoQualityAnalyzer::ProcessComparisonsThread(void* obj) {
  static_cast<DefaultVideoQualityAnalyzer*>(obj)->ProcessComparisons();
}

void DefaultVideoQualityAnalyzer::ProcessComparisons() {
  while (true) {
    // Try to pick next comparison to perform from the queue.
    absl::optional<FrameComparison> comparison = absl::nullopt;
    {
      rtc::CritScope crit(&comparison_lock_);
      if (!comparisons_.empty()) {
        comparison = comparisons_.front();
        comparisons_.pop_front();
        if (!comparisons_.empty()) {
          comparison_available_event_.Set();
        }
      }
    }
    if (!comparison) {
      bool more_frames_expected;
      {
        // If there are no comparisons and state is stopped =>
        // no more frames expected.
        rtc::CritScope crit(&lock_);
        more_frames_expected = state_ != State::kStopped;
      }
      if (!more_frames_expected) {
        comparison_available_event_.Set();
        return;
      }
      comparison_available_event_.Wait(1000);
      continue;
    }

    ProcessComparison(comparison.value());
  }
}

void DefaultVideoQualityAnalyzer::ProcessComparison(
    const FrameComparison& comparison) {
  // Perform expensive psnr and ssim calculations while not holding lock.
  double psnr = -1.0;
  double ssim = -1.0;
  if (comparison.captured && !comparison.dropped) {
    psnr = I420PSNR(&*comparison.captured, &*comparison.rendered);
    ssim = I420SSIM(&*comparison.captured, &*comparison.rendered);
  }

  const FrameStats& frame_stats = comparison.frame_stats;

  rtc::CritScope crit(&comparison_lock_);
  auto stats_it = stream_stats_.find(frame_stats.stream_label);
  RTC_CHECK(stats_it != stream_stats_.end());
  StreamStats* stats = &stats_it->second;
  analyzer_stats_.comparisons_done++;
  if (!comparison.captured) {
    analyzer_stats_.overloaded_comparisons_done++;
  }
  if (psnr > 0) {
    stats->psnr.AddSample(psnr);
  }
  if (ssim > 0) {
    stats->ssim.AddSample(ssim);
  }
  if (frame_stats.encoded_time.IsFinite()) {
    stats->encode_time_ms.AddSample(
        (frame_stats.encoded_time - frame_stats.pre_encode_time).ms());
    stats->encode_frame_rate.AddEvent(frame_stats.encoded_time);
  } else {
    if (frame_stats.pre_encode_time.IsFinite()) {
      stats->dropped_by_encoder++;
    } else {
      stats->dropped_before_encoder++;
    }
  }
  // Next stats can be calculated only if frame was received on remote side.
  if (!comparison.dropped) {
    stats->resolution_of_rendered_frame.AddSample(
        *comparison.frame_stats.rendered_frame_width *
        *comparison.frame_stats.rendered_frame_height);
    stats->transport_time_ms.AddSample(
        (frame_stats.received_time - frame_stats.encoded_time).ms());
    stats->total_delay_incl_transport_ms.AddSample(
        (frame_stats.rendered_time - frame_stats.captured_time).ms());
    stats->decode_time_ms.AddSample(
        (frame_stats.decoded_time - frame_stats.received_time).ms());

    if (frame_stats.prev_frame_rendered_time.IsFinite()) {
      TimeDelta time_between_rendered_frames =
          frame_stats.rendered_time - frame_stats.prev_frame_rendered_time;
      stats->time_between_rendered_frames_ms.AddSample(
          time_between_rendered_frames.ms());
      double average_time_between_rendered_frames_ms =
          stats->time_between_rendered_frames_ms.GetAverage();
      if (time_between_rendered_frames.ms() >
          std::max(kFreezeThresholdMs + average_time_between_rendered_frames_ms,
                   3 * average_time_between_rendered_frames_ms)) {
        stats->freeze_time_ms.AddSample(time_between_rendered_frames.ms());
        auto freeze_end_it =
            stream_last_freeze_end_time_.find(frame_stats.stream_label);
        RTC_DCHECK(freeze_end_it != stream_last_freeze_end_time_.end());
        stats->time_between_freezes_ms.AddSample(
            (frame_stats.prev_frame_rendered_time - freeze_end_it->second)
                .ms());
        freeze_end_it->second = frame_stats.rendered_time;
      }
    }
  }
}

void DefaultVideoQualityAnalyzer::ReportResults() {
  rtc::CritScope crit1(&lock_);
  rtc::CritScope crit2(&comparison_lock_);
  for (auto& item : stream_stats_) {
    ReportResults(GetTestCaseName(item.first), item.second,
                  stream_frame_counters_.at(item.first));
  }
  LogFrameCounters("Global", frame_counters_);
  for (auto& item : stream_stats_) {
    LogFrameCounters(item.first, stream_frame_counters_.at(item.first));
    LogStreamInternalStats(item.first, item.second);
  }
  if (!analyzer_stats_.comparisons_queue_size.IsEmpty()) {
    RTC_LOG(INFO) << "comparisons_queue_size min="
                  << analyzer_stats_.comparisons_queue_size.GetMin()
                  << "; max=" << analyzer_stats_.comparisons_queue_size.GetMax()
                  << "; 99%="
                  << analyzer_stats_.comparisons_queue_size.GetPercentile(0.99);
  }
  RTC_LOG(INFO) << "comparisons_done=" << analyzer_stats_.comparisons_done;
  RTC_LOG(INFO) << "overloaded_comparisons_done="
                << analyzer_stats_.overloaded_comparisons_done;
}

void DefaultVideoQualityAnalyzer::ReportResults(std::string test_case_name,
                                                StreamStats stats,
                                                FrameCounters frame_counters) {
  ReportResult("psnr", test_case_name, stats.psnr, "dB");
  ReportResult("ssim", test_case_name, stats.ssim, "unitless");
  ReportResult("transport_time", test_case_name, stats.transport_time_ms, "ms");
  ReportResult("total_delay_incl_transport", test_case_name,
               stats.total_delay_incl_transport_ms, "ms");
  ReportResult("time_between_rendered_frames", test_case_name,
               stats.time_between_rendered_frames_ms, "ms");
  test::PrintResult("encode_frame_rate", "", test_case_name,
                    stats.encode_frame_rate.IsEmpty()
                        ? 0
                        : stats.encode_frame_rate.GetEventsPerSecond(),
                    "fps", /*important=*/false);
  ReportResult("encode_time", test_case_name, stats.encode_time_ms, "ms");
  ReportResult("time_between_freezes", test_case_name,
               stats.time_between_freezes_ms, "ms");
  ReportResult("pixels_per_frame", test_case_name,
               stats.resolution_of_rendered_frame, "unitless");
  test::PrintResult("min_psnr", "", test_case_name,
                    stats.psnr.IsEmpty() ? 0 : stats.psnr.GetMin(), "dB",
                    /*important=*/false);
  ReportResult("decode_time", test_case_name, stats.decode_time_ms, "ms");
  test::PrintResult("dropped_frames", "", test_case_name,
                    frame_counters.dropped, "unitless",
                    /*important=*/false);
  ReportResult("max_skipped", test_case_name, stats.skipped_between_rendered,
               "unitless");
}

void DefaultVideoQualityAnalyzer::ReportResult(
    const std::string& metric_name,
    const std::string& test_case_name,
    const SamplesStatsCounter& counter,
    const std::string& unit) {
  test::PrintResultMeanAndError(
      metric_name, /*modifier=*/"", test_case_name,
      counter.IsEmpty() ? 0 : counter.GetAverage(),
      counter.IsEmpty() ? 0 : counter.GetStandardDeviation(), unit,
      /*important=*/false);
}

std::string DefaultVideoQualityAnalyzer::GetTestCaseName(
    const std::string& stream_label) const {
  return test_label_ + "/" + stream_label;
}

Timestamp DefaultVideoQualityAnalyzer::Now() {
  return Timestamp::us(clock_->TimeInMicroseconds());
}

DefaultVideoQualityAnalyzer::FrameStats::FrameStats(std::string stream_label,
                                                    Timestamp captured_time)
    : stream_label(std::move(stream_label)), captured_time(captured_time) {}

DefaultVideoQualityAnalyzer::FrameComparison::FrameComparison(
    absl::optional<VideoFrame> captured,
    absl::optional<VideoFrame> rendered,
    bool dropped,
    FrameStats frame_stats)
    : captured(std::move(captured)),
      rendered(std::move(rendered)),
      dropped(dropped),
      frame_stats(std::move(frame_stats)) {}

DefaultVideoQualityAnalyzer::FrameComparison::FrameComparison(
    bool dropped,
    FrameStats frame_stats)
    : captured(absl::nullopt),
      rendered(absl::nullopt),
      dropped(dropped),
      frame_stats(std::move(frame_stats)) {}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
