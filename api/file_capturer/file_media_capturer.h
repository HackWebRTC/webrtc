/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source tree.
 */

#ifndef API_FILE_CAPTURER_FILE_MEDIA_CAPTURER_H_
#define API_FILE_CAPTURER_FILE_MEDIA_CAPTURER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "api/video/video_frame.h"
#include "rtc_base/system/rtc_export.h"

namespace webrtc {

enum class FileMediaEofMode { kStop, kLoop };

enum class FileMediaErrorCode {
  kNone,
  kUnsupportedContainer,
  kUnsupportedCodec,
  kUnsupportedFormat,
  kInvalidTiming,
  kDecodeFailure,
  kResourceExhausted,
  kConsumerTooSlow,
  kInternal,
  kInvalidState,
};

struct RTC_EXPORT FileMediaError {
  FileMediaErrorCode code = FileMediaErrorCode::kNone;
  std::string message;

  bool ok() const { return code == FileMediaErrorCode::kNone; }
};

struct RTC_EXPORT FileMediaCapturerConfig {
  std::string input_path;
  FileMediaEofMode eof_mode = FileMediaEofMode::kStop;
  std::optional<int> audio_stream_index;
  std::optional<int> video_stream_index;
};

struct RTC_EXPORT FileMediaTrackInfo {
  int stream_index = -1;
  int64_t start_time_us = 0;
  int64_t duration_us = 0;
  int width = 0;
  int height = 0;
  int sample_rate_hz = 0;
  size_t channels = 0;
};

struct RTC_EXPORT FileMediaInfo {
  std::optional<FileMediaTrackInfo> audio;
  std::optional<FileMediaTrackInfo> video;
  int64_t loop_duration_us = 0;
};

struct RTC_EXPORT FileMediaAudioBlock {
  std::vector<int16_t> samples;
  int sample_rate_hz = 48000;
  size_t channels = 2;
  int64_t timestamp_us = 0;
};

struct RTC_EXPORT FileMediaLoopBoundary {
  size_t cycle_index = 0;
  int64_t timestamp_us = 0;
};

enum class FileMediaTerminalReason { kEndOfFile, kCallerStopped, kError };

struct RTC_EXPORT FileMediaTerminal {
  FileMediaTerminalReason reason = FileMediaTerminalReason::kError;
  FileMediaError error;
};

class RTC_EXPORT FileMediaCapturerObserver {
 public:
  virtual ~FileMediaCapturerObserver() = default;
  virtual void OnVideoFrame(VideoFrame frame) = 0;
  virtual void OnAudioBlock(FileMediaAudioBlock block) = 0;
  virtual void OnLoopBoundary(FileMediaLoopBoundary boundary) = 0;
  virtual void OnTerminal(FileMediaTerminal terminal) = 0;
};

class RTC_EXPORT FileMediaCapturer {
 public:
  virtual ~FileMediaCapturer() = default;
  virtual FileMediaError Start() = 0;
  virtual void Stop() = 0;
  virtual const FileMediaInfo& media_info() const = 0;
};

struct RTC_EXPORT FileMediaCapturerCreateResult {
  std::unique_ptr<FileMediaCapturer> capturer;
  FileMediaError error;

  bool ok() const { return capturer != nullptr && error.ok(); }
};

RTC_EXPORT FileMediaCapturerCreateResult
CreateFileMediaCapturer(FileMediaCapturerConfig config,
                        FileMediaCapturerObserver* observer);

}  // namespace webrtc

#endif  // API_FILE_CAPTURER_FILE_MEDIA_CAPTURER_H_
