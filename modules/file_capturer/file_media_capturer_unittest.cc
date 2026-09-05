/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree.
 */

#include "api/file_capturer/file_media_capturer.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "modules/file_capturer/file_media_capturer_internal.h"
#include "test/gtest.h"

namespace webrtc {
namespace {

class RecordingObserver : public FileMediaCapturerObserver {
 public:
  void OnVideoFrame(VideoFrame frame) override {
    std::lock_guard lock(mutex_);
    callback_order_.push_back({"video", frame.timestamp_us()});
    video_timestamps_us_.push_back(frame.timestamp_us());
  }

  void OnAudioBlock(FileMediaAudioBlock block) override {
    std::lock_guard lock(mutex_);
    callback_order_.push_back({"audio", block.timestamp_us});
    audio_timestamps_us_.push_back(block.timestamp_us);
    audio_block_sizes_.push_back(block.samples.size());
    sample_rates_hz_.push_back(block.sample_rate_hz);
    channel_counts_.push_back(block.channels);
    for (int16_t sample : block.samples) {
      audio_hash_ ^= static_cast<uint16_t>(sample);
      audio_hash_ *= 1099511628211ULL;
    }
  }

  void OnLoopBoundary(FileMediaLoopBoundary boundary) override {
    std::lock_guard lock(mutex_);
    callback_order_.push_back({"loop", boundary.timestamp_us});
    loop_boundaries_.push_back(boundary);
  }

  void OnTerminal(FileMediaTerminal terminal) override {
    {
      std::lock_guard lock(mutex_);
      terminals_.push_back(std::move(terminal));
    }
    terminal_condition_.notify_all();
  }

  bool WaitForTerminal() {
    std::unique_lock lock(mutex_);
    return terminal_condition_.wait_for(lock, std::chrono::seconds(5),
                                        [this] { return !terminals_.empty(); });
  }

  std::mutex mutex_;
  std::condition_variable terminal_condition_;
  std::vector<int64_t> video_timestamps_us_;
  std::vector<int64_t> audio_timestamps_us_;
  std::vector<size_t> audio_block_sizes_;
  std::vector<int> sample_rates_hz_;
  std::vector<size_t> channel_counts_;
  std::vector<FileMediaLoopBoundary> loop_boundaries_;
  std::vector<FileMediaTerminal> terminals_;
  std::vector<std::pair<std::string, int64_t>> callback_order_;
  uint64_t audio_hash_ = 14695981039346656037ULL;
};

void StartAndWait(FileMediaCapturer* capturer, RecordingObserver* observer) {
  ASSERT_TRUE(capturer->Start().ok());
  ASSERT_TRUE(observer->WaitForTerminal());
}

std::string Fixture(const char* name) {
  return std::string(FILE_CAPTURER_TESTDATA_DIR) + "/" + name + ".mp4";
}

// Catches regressions where the formal source still requires both tracks, or
// exposes the AAC decoder's native 44.1 kHz mono FLTP instead of the public
// 48 kHz stereo signed-16 contract.
TEST(FileMediaCapturerTest, ConvertsAudioOnlyInputToOwnedTenMillisecondBlocks) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("audio_only");
  config.eof_mode = FileMediaEofMode::kStop;

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  ASSERT_TRUE(created.capturer->Start().ok());
  ASSERT_TRUE(observer.WaitForTerminal());

  std::lock_guard lock(observer.mutex_);
  EXPECT_TRUE(observer.video_timestamps_us_.empty());
  ASSERT_FALSE(observer.audio_timestamps_us_.empty());
  EXPECT_EQ(observer.audio_timestamps_us_.front(), 0);
  EXPECT_EQ(observer.audio_block_sizes_.front(), 960u);
  EXPECT_EQ(observer.sample_rates_hz_.front(), 48000);
  EXPECT_EQ(observer.channel_counts_.front(), 2u);
  ASSERT_EQ(observer.terminals_.size(), 1u);
  EXPECT_EQ(observer.terminals_.front().reason,
            FileMediaTerminalReason::kEndOfFile);
  EXPECT_TRUE(observer.terminals_.front().error.ok());
}

// Catches regressions where a missing audio stream prevents valid video-only
// capture, or decoded frames borrow memory owned by a reused FFmpeg AVFrame.
TEST(FileMediaCapturerTest, EmitsOwnedI420FramesForVideoOnlyInput) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("video_only");

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  EXPECT_FALSE(created.capturer->media_info().audio.has_value());
  ASSERT_TRUE(created.capturer->media_info().video.has_value());
  StartAndWait(created.capturer.get(), &observer);

  std::lock_guard lock(observer.mutex_);
  EXPECT_TRUE(observer.audio_timestamps_us_.empty());
  EXPECT_EQ(observer.video_timestamps_us_,
            (std::vector<int64_t>{0, 100000, 200000}));
  ASSERT_EQ(observer.terminals_.size(), 1u);
  EXPECT_EQ(observer.terminals_[0].reason, FileMediaTerminalReason::kEndOfFile);
}

// Catches using container duration or the longer track as the dual-track
// boundary. No video frame at or beyond the shorter 300 ms track may escape.
TEST(FileMediaCapturerTest, TruncatesBothTracksAtShorterTrackBoundary) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("video_shorter");

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  EXPECT_EQ(created.capturer->media_info().loop_duration_us, 300000);
  StartAndWait(created.capturer.get(), &observer);

  std::lock_guard lock(observer.mutex_);
  ASSERT_FALSE(observer.video_timestamps_us_.empty());
  ASSERT_FALSE(observer.audio_timestamps_us_.empty());
  EXPECT_LT(observer.video_timestamps_us_.back(), 300000);
  EXPECT_LT(observer.audio_timestamps_us_.back(), 300000);
  EXPECT_EQ(observer.audio_timestamps_us_.size(), 30u);
  ASSERT_GE(observer.callback_order_.size(), 2u);
  EXPECT_EQ(observer.callback_order_[0],
            (std::pair<std::string, int64_t>{"audio", 0}));
  EXPECT_EQ(observer.callback_order_[1],
            (std::pair<std::string, int64_t>{"video", 0}));
}

// Catches leaking a file's edit-list offset into the public media clock.
TEST(FileMediaCapturerTest, NormalizesNonZeroTrackStartToZero) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("nonzero_start");

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  StartAndWait(created.capturer.get(), &observer);

  std::lock_guard lock(observer.mutex_);
  ASSERT_FALSE(observer.video_timestamps_us_.empty());
  ASSERT_FALSE(observer.audio_timestamps_us_.empty());
  EXPECT_GE(observer.video_timestamps_us_.front(), 0);
  EXPECT_EQ(observer.audio_timestamps_us_.front(), 0);
}

class LoopStoppingObserver final : public RecordingObserver {
 public:
  void OnLoopBoundary(FileMediaLoopBoundary boundary) override {
    RecordingObserver::OnLoopBoundary(boundary);
    capturer->Stop();
  }

  FileMediaCapturer* capturer = nullptr;
};

// Catches loop implementations that emit the boundary after new-cycle media,
// restart timestamps at zero, or deadlock when Stop is called by the observer.
TEST(FileMediaCapturerTest, LoopBoundaryPrecedesMediaAndAllowsCallbackStop) {
  LoopStoppingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("audio_video");
  config.eof_mode = FileMediaEofMode::kLoop;

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  observer.capturer = created.capturer.get();
  StartAndWait(created.capturer.get(), &observer);

  std::lock_guard lock(observer.mutex_);
  ASSERT_EQ(observer.terminals_.size(), 1u)
      << observer.terminals_[0].error.message;
  ASSERT_EQ(observer.loop_boundaries_.size(), 1u)
      << observer.terminals_[0].error.message;
  EXPECT_EQ(observer.loop_boundaries_[0].cycle_index, 1u);
  EXPECT_EQ(observer.loop_boundaries_[0].timestamp_us, 420000);
  EXPECT_EQ(observer.terminals_[0].reason,
            FileMediaTerminalReason::kCallerStopped);
}

class StopOnSecondCycleObserver final : public RecordingObserver {
 public:
  void OnVideoFrame(VideoFrame frame) override {
    const int64_t timestamp_us = frame.timestamp_us();
    RecordingObserver::OnVideoFrame(std::move(frame));
    if (timestamp_us >= second_cycle_start_us)
      capturer->Stop();
  }

  FileMediaCapturer* capturer = nullptr;
  int64_t second_cycle_start_us = 0;
};

// Catches loop implementations that wrap media timestamps back to zero.
TEST(FileMediaCapturerTest, LoopKeepsAContinuousMediaClock) {
  StopOnSecondCycleObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("audio_video");
  config.eof_mode = FileMediaEofMode::kLoop;

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  observer.capturer = created.capturer.get();
  observer.second_cycle_start_us =
      created.capturer->media_info().loop_duration_us;
  StartAndWait(created.capturer.get(), &observer);

  std::lock_guard lock(observer.mutex_);
  ASSERT_EQ(observer.terminals_.size(), 1u)
      << observer.terminals_[0].error.message;
  ASSERT_GE(observer.video_timestamps_us_.size(), 5u)
      << observer.terminals_[0].error.message;
  EXPECT_EQ(observer.video_timestamps_us_.back(), 420000);
  EXPECT_EQ(observer.terminals_[0].reason,
            FileMediaTerminalReason::kCallerStopped);
}

// Catches deferring an unsupported decoded pixel format until asynchronous
// playback instead of rejecting it during Create's first-frame probe.
TEST(FileMediaCapturerTest, RejectsUnsupportedDecodedPixelFormatAtCreate) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("unsupported_yuv444");

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  EXPECT_FALSE(created.ok());
  EXPECT_EQ(created.error.code, FileMediaErrorCode::kUnsupportedFormat);
}

// Catches accidentally launching two worker sequences from one capturer.
TEST(FileMediaCapturerTest, StartSucceedsOnlyOnce) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("audio_only");
  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;

  ASSERT_TRUE(created.capturer->Start().ok());
  EXPECT_EQ(created.capturer->Start().code, FileMediaErrorCode::kInvalidState);
  ASSERT_TRUE(observer.WaitForTerminal());
}

// Catches rounding a dual-track loop boundary up and leaking a partial audio
// block into the next cycle.
TEST(FileMediaCapturerTest, FloorsDualTrackBoundaryToTenMilliseconds) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("non_aligned");

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  EXPECT_EQ(created.capturer->media_info().loop_duration_us, 340000);
  StartAndWait(created.capturer.get(), &observer);

  std::lock_guard lock(observer.mutex_);
  EXPECT_EQ(observer.audio_timestamps_us_.size(), 34u);
  ASSERT_FALSE(observer.audio_timestamps_us_.empty());
  EXPECT_EQ(observer.audio_timestamps_us_.back(), 330000);
}

// Catches rejecting valid selected tracks solely because their MP4 duration
// metadata is absent. Packet and decoded-frame timing still define a finite
// common interval.
TEST(FileMediaCapturerTest, DerivesBoundaryWhenTrackDurationMetadataIsMissing) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("missing_duration");

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  EXPECT_EQ(created.capturer->media_info().loop_duration_us, 0);
  StartAndWait(created.capturer.get(), &observer);

  std::lock_guard lock(observer.mutex_);
  EXPECT_GT(created.capturer->media_info().loop_duration_us, 0);
  ASSERT_FALSE(observer.audio_timestamps_us_.empty());
  ASSERT_FALSE(observer.video_timestamps_us_.empty());
  EXPECT_EQ(observer.audio_timestamps_us_.front(), 0);
  EXPECT_EQ(observer.video_timestamps_us_.front(), 0);
  EXPECT_LT(observer.audio_timestamps_us_.back(),
            created.capturer->media_info().loop_duration_us);
  EXPECT_LT(observer.video_timestamps_us_.back(),
            created.capturer->media_info().loop_duration_us);
  ASSERT_EQ(observer.terminals_.size(), 1u);
  EXPECT_EQ(observer.terminals_[0].reason, FileMediaTerminalReason::kEndOfFile);
}

TEST(FileMediaCapturerTest, DerivesUnknownTrackBeforeFreezingBoundary) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("partial_missing_duration");

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  EXPECT_EQ(created.capturer->media_info().loop_duration_us, 0);
  StartAndWait(created.capturer.get(), &observer);

  std::lock_guard lock(observer.mutex_);
  const FileMediaInfo info = created.capturer->media_info();
  EXPECT_EQ(info.loop_duration_us, 300000);
  ASSERT_FALSE(observer.audio_timestamps_us_.empty());
  ASSERT_FALSE(observer.video_timestamps_us_.empty());
  EXPECT_LT(observer.audio_timestamps_us_.back(), info.loop_duration_us);
  EXPECT_LT(observer.video_timestamps_us_.back(), info.loop_duration_us);
}

// Catches Stop failing to wake a capturer that is waiting for a future PTS.
TEST(FileMediaCapturerTest, ExternalStopTerminatesExactlyOnce) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("video_only");
  config.eof_mode = FileMediaEofMode::kLoop;

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  ASSERT_TRUE(created.capturer->Start().ok());
  created.capturer->Stop();
  ASSERT_TRUE(observer.WaitForTerminal());

  std::lock_guard lock(observer.mutex_);
  ASSERT_EQ(observer.terminals_.size(), 1u);
  EXPECT_EQ(observer.terminals_[0].reason,
            FileMediaTerminalReason::kCallerStopped);
}

class SlowObserver final : public RecordingObserver {
 public:
  void OnAudioBlock(FileMediaAudioBlock block) override {
    RecordingObserver::OnAudioBlock(std::move(block));
    if (!slept_) {
      slept_ = true;
      std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    }
  }

 private:
  bool slept_ = false;
};

// Catches an unbounded latency build-up when the consumer blocks callbacks.
TEST(FileMediaCapturerTest, SlowConsumerTerminatesWithStableErrorCode) {
  SlowObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("audio_only");

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  StartAndWait(created.capturer.get(), &observer);

  std::lock_guard lock(observer.mutex_);
  ASSERT_EQ(observer.terminals_.size(), 1u);
  EXPECT_EQ(observer.terminals_[0].reason, FileMediaTerminalReason::kError);
  EXPECT_EQ(observer.terminals_[0].error.code,
            FileMediaErrorCode::kConsumerTooSlow);
}

// Catches treating truncated media as a normal EOF even though MP4 metadata
// promises a longer selected-track interval.
TEST(FileMediaCapturerTest, TruncatedPayloadDoesNotReportSuccessfulEof) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("truncated_audio_video");

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  StartAndWait(created.capturer.get(), &observer);

  std::lock_guard lock(observer.mutex_);
  ASSERT_EQ(observer.terminals_.size(), 1u);
  EXPECT_EQ(observer.terminals_[0].reason, FileMediaTerminalReason::kError);
  EXPECT_TRUE(
      observer.terminals_[0].error.code == FileMediaErrorCode::kInvalidTiming ||
      observer.terminals_[0].error.code == FileMediaErrorCode::kDecodeFailure);
}

// Catches accepting two selected tracks whose effective intervals never
// overlap, which cannot produce a normalized shared timeline.
TEST(FileMediaCapturerTest, RejectsTracksWithoutACommonInterval) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("no_common_interval");

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  EXPECT_FALSE(created.ok());
  EXPECT_EQ(created.error.code, FileMediaErrorCode::kInvalidTiming);
}

// Catches filling a video gap with duplicate frames or stalling the audio
// track while waiting beyond the two-second lookahead horizon.
TEST(FileMediaCapturerTest,
     PreservesGapLongerThanLookaheadWithoutStallingAudio) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("gap_over_2s");

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  ASSERT_TRUE(created.ok()) << created.error.message;
  StartAndWait(created.capturer.get(), &observer);

  std::lock_guard lock(observer.mutex_);
  ASSERT_GE(observer.video_timestamps_us_.size(), 4u);
  EXPECT_GT(observer.video_timestamps_us_[2] - observer.video_timestamps_us_[1],
            2000000);
  EXPECT_GT(observer.audio_timestamps_us_.size(), 200u);
  EXPECT_EQ(observer.terminals_[0].reason, FileMediaTerminalReason::kEndOfFile);
}

// Catches accepting a packet that would grow the compressed lookahead beyond
// its fixed 64 MiB resource budget, including arithmetic overflow cases.
TEST(FileMediaCapturerTest, EnforcesCompressedPacketLookaheadBudget) {
  constexpr size_t kLimit = file_capturer_internal::kMaximumPacketBytes;
  EXPECT_FALSE(file_capturer_internal::ExceedsPacketBudget(kLimit - 1, 1));
  EXPECT_TRUE(file_capturer_internal::ExceedsPacketBudget(kLimit - 1, 2));
  EXPECT_TRUE(file_capturer_internal::ExceedsPacketBudget(kLimit + 1, 1));
}

// Catches ignoring an explicit stream index in a multi-track MP4.
TEST(FileMediaCapturerTest, ExplicitAudioStreamSelectionChangesOutput) {
  RecordingObserver first_observer;
  FileMediaCapturerConfig first_config;
  first_config.input_path = Fixture("two_audio_tracks");
  first_config.audio_stream_index = 0;
  FileMediaCapturerCreateResult first =
      CreateFileMediaCapturer(std::move(first_config), &first_observer);
  ASSERT_TRUE(first.ok()) << first.error.message;
  StartAndWait(first.capturer.get(), &first_observer);

  RecordingObserver second_observer;
  FileMediaCapturerConfig second_config;
  second_config.input_path = Fixture("two_audio_tracks");
  second_config.audio_stream_index = 1;
  FileMediaCapturerCreateResult second =
      CreateFileMediaCapturer(std::move(second_config), &second_observer);
  ASSERT_TRUE(second.ok()) << second.error.message;
  StartAndWait(second.capturer.get(), &second_observer);

  EXPECT_EQ(first.capturer->media_info().audio->stream_index, 0);
  EXPECT_EQ(second.capturer->media_info().audio->stream_index, 1);
  EXPECT_NE(first_observer.audio_hash_, second_observer.audio_hash_);
}

// Catches accepting a non-H.264 video track just because the container is MP4.
TEST(FileMediaCapturerTest, RejectsUnsupportedCodecAtCreate) {
  RecordingObserver observer;
  FileMediaCapturerConfig config;
  config.input_path = Fixture("unsupported_codec");

  FileMediaCapturerCreateResult created =
      CreateFileMediaCapturer(std::move(config), &observer);
  EXPECT_FALSE(created.ok());
  EXPECT_EQ(created.error.code, FileMediaErrorCode::kUnsupportedCodec);
}

}  // namespace
}  // namespace webrtc
