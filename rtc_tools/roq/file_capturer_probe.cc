/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "rtc_tools/roq/file_capturer_probe.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>

#include "api/file_capturer/file_media_capturer.h"
#include "api/video/i420_buffer.h"

namespace webrtc::roq {
namespace {

constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

void HashBytes(uint64_t* hash, const uint8_t* data, size_t size) {
  for (size_t index = 0; index < size; ++index) {
    *hash ^= data[index];
    *hash *= kFnvPrime;
  }
}

class ProbeObserver final : public FileMediaCapturerObserver {
 public:
  ProbeObserver(int64_t duration_us,
                std::string run_id,
                const std::string& event_log_path)
      : duration_us_(duration_us),
        run_id_(std::move(run_id)),
        output_(event_log_path.empty() ? std::ofstream()
                                       : std::ofstream(event_log_path)),
        start_(std::chrono::steady_clock::now()) {
    result_.video_hash = kFnvOffsetBasis;
    result_.audio_hash = kFnvOffsetBasis;
    result_.video_pts_hash = kFnvOffsetBasis;
    result_.audio_pts_hash = kFnvOffsetBasis;
  }

  bool output_ok() const { return !output_.is_open() || output_.good(); }
  void set_capturer(FileMediaCapturer* capturer) { capturer_ = capturer; }

  void OnVideoFrame(VideoFrame frame) override {
    if (frame.timestamp_us() >= duration_us_) {
      capturer_->Stop();
      return;
    }
    scoped_refptr<I420BufferInterface> buffer =
        frame.video_frame_buffer()->ToI420();
    if (!buffer) {
      result_.error = "file capturer emitted a non-I420 video frame";
      capturer_->Stop();
      return;
    }
    WriteEvent("video_frame", frame.timestamp_us());
    const int64_t timestamp_us = frame.timestamp_us();
    HashBytes(&result_.video_pts_hash,
              reinterpret_cast<const uint8_t*>(&timestamp_us), sizeof(int64_t));
    for (int row = 0; row < buffer->height(); ++row) {
      HashBytes(&result_.video_hash, buffer->DataY() + row * buffer->StrideY(),
                static_cast<size_t>(buffer->width()));
    }
    const int chroma_width = (buffer->width() + 1) / 2;
    const int chroma_height = (buffer->height() + 1) / 2;
    for (int row = 0; row < chroma_height; ++row) {
      HashBytes(&result_.video_hash, buffer->DataU() + row * buffer->StrideU(),
                static_cast<size_t>(chroma_width));
      HashBytes(&result_.video_hash, buffer->DataV() + row * buffer->StrideV(),
                static_cast<size_t>(chroma_width));
    }
    ++result_.video_frames;
    result_.video_width = buffer->width();
    result_.video_height = buffer->height();
  }

  void OnAudioBlock(FileMediaAudioBlock block) override {
    if (block.timestamp_us >= duration_us_) {
      capturer_->Stop();
      return;
    }
    WriteEvent("audio_block", block.timestamp_us);
    HashBytes(&result_.audio_pts_hash,
              reinterpret_cast<const uint8_t*>(&block.timestamp_us),
              sizeof(block.timestamp_us));
    HashBytes(&result_.audio_hash,
              reinterpret_cast<const uint8_t*>(block.samples.data()),
              block.samples.size() * sizeof(int16_t));
    ++result_.audio_blocks;
    result_.audio_samples_per_channel +=
        static_cast<int64_t>(block.samples.size() / block.channels);
    result_.audio_sample_rate_hz = block.sample_rate_hz;
    result_.audio_channels = static_cast<int>(block.channels);
  }

  void OnLoopBoundary(FileMediaLoopBoundary) override {}

  void OnTerminal(FileMediaTerminal terminal) override {
    {
      std::lock_guard lock(mutex_);
      if (terminal.reason == FileMediaTerminalReason::kError &&
          result_.error.empty()) {
        result_.error = terminal.error.message;
      }
      terminal_ = true;
    }
    condition_.notify_all();
  }

  FileCapturerProbeResult Wait() {
    std::unique_lock lock(mutex_);
    condition_.wait(lock, [this] { return terminal_; });
    return result_;
  }

 private:
  void WriteEvent(const char* event, int64_t media_pts_us) {
    const int64_t actual_emit_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start_)
            .count();
    result_.max_pacing_error_us =
        std::max(result_.max_pacing_error_us, actual_emit_us - media_pts_us);
    if (!output_.is_open())
      return;
    output_ << "{\"schema_version\":1,\"run_id\":\"" << run_id_
            << "\",\"process\":\"media-source\",\"event\":\"" << event
            << "\",\"sequence\":" << sequence_++
            << ",\"media_pts_us\":" << media_pts_us
            << ",\"target_emit_us\":" << media_pts_us
            << ",\"actual_emit_us\":" << actual_emit_us << "}\n";
  }

  const int64_t duration_us_;
  const std::string run_id_;
  std::ofstream output_;
  const std::chrono::steady_clock::time_point start_;
  FileMediaCapturer* capturer_ = nullptr;
  uint64_t sequence_ = 0;
  std::mutex mutex_;
  std::condition_variable condition_;
  bool terminal_ = false;
  FileCapturerProbeResult result_;
};

}  // namespace

FileCapturerProbeResult RunFileCapturerProbe(
    const std::string& input_path,
    double duration_seconds,
    const std::string& run_id,
    const std::string& event_log_path) {
  if (duration_seconds <= 0)
    return {.error = "duration must be positive"};
  ProbeObserver observer(static_cast<int64_t>(duration_seconds * 1000000.0),
                         run_id, event_log_path);
  if (!observer.output_ok())
    return {.error = "failed to open file capturer event log"};
  FileMediaCapturerCreateResult created = CreateFileMediaCapturer(
      {.input_path = input_path, .eof_mode = FileMediaEofMode::kLoop},
      &observer);
  if (!created.ok())
    return {.error = created.error.message};
  observer.set_capturer(created.capturer.get());
  FileMediaError start = created.capturer->Start();
  if (!start.ok())
    return {.error = start.message};
  return observer.Wait();
}

std::string HexHash(uint64_t value) {
  std::ostringstream output;
  output << std::hex << std::setw(16) << std::setfill('0') << value;
  return output.str();
}

}  // namespace webrtc::roq
