/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cstddef>
#include <vector>

#include "api/video/video_timing.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "rtc_base/time_utils.h"
#include "test/gtest.h"
#include "video/frame_encode_timer.h"

namespace webrtc {
namespace test {
namespace {
inline size_t FrameSize(const size_t& min_frame_size,
                        const size_t& max_frame_size,
                        const int& s,
                        const int& i) {
  return min_frame_size + (s + 1) * i % (max_frame_size - min_frame_size);
}

class FakeEncodedImageCallback : public EncodedImageCallback {
 public:
  FakeEncodedImageCallback() : num_frames_dropped_(0) {}
  Result OnEncodedImage(const EncodedImage& encoded_image,
                        const CodecSpecificInfo* codec_specific_info,
                        const RTPFragmentationHeader* fragmentation) override {
    return Result(Result::OK);
  }
  void OnDroppedFrame(DropReason reason) override { ++num_frames_dropped_; }
  size_t GetNumFramesDropped() { return num_frames_dropped_; }

 private:
  size_t num_frames_dropped_;
};

enum class FrameType {
  kNormal,
  kTiming,
  kDropped,
};

bool IsTimingFrame(const EncodedImage& image) {
  return image.timing_.flags != VideoSendTiming::kInvalid &&
         image.timing_.flags != VideoSendTiming::kNotTriggered;
}

// Emulates |num_frames| on |num_streams| frames with capture timestamps
// increased by 1 from 0. Size of each frame is between
// |min_frame_size| and |max_frame_size|, outliers are counted relatevely to
// |average_frame_sizes[]| for each stream.
std::vector<std::vector<FrameType>> GetTimingFrames(
    const int64_t delay_ms,
    const size_t min_frame_size,
    const size_t max_frame_size,
    std::vector<size_t> average_frame_sizes,
    const int num_streams,
    const int num_frames) {
  FakeEncodedImageCallback sink;
  FrameEncodeTimer encode_timer(&sink);
  VideoCodec codec_settings;
  codec_settings.numberOfSimulcastStreams = num_streams;
  codec_settings.timing_frame_thresholds = {delay_ms,
                                            kDefaultOutlierFrameSizePercent};
  encode_timer.OnEncoderInit(codec_settings, false);
  const size_t kFramerate = 30;
  VideoBitrateAllocation bitrate_allocation;
  for (int si = 0; si < num_streams; ++si) {
    bitrate_allocation.SetBitrate(si, 0,
                                  average_frame_sizes[si] * 8 * kFramerate);
  }
  encode_timer.OnSetRates(bitrate_allocation, kFramerate);

  std::vector<std::vector<FrameType>> result(num_streams);
  int64_t current_timestamp = 0;
  for (int i = 0; i < num_frames; ++i) {
    current_timestamp += 1;
    encode_timer.OnEncodeStarted(static_cast<uint32_t>(current_timestamp * 90),
                                 current_timestamp);
    for (int si = 0; si < num_streams; ++si) {
      // every (5+s)-th frame is dropped on s-th stream by design.
      bool dropped = i % (5 + si) == 0;

      EncodedImage image;
      image.Allocate(max_frame_size);
      image.set_size(FrameSize(min_frame_size, max_frame_size, si, i));
      image.capture_time_ms_ = current_timestamp;
      image.SetTimestamp(static_cast<uint32_t>(current_timestamp * 90));
      image.SetSpatialIndex(si);

      if (dropped) {
        result[si].push_back(FrameType::kDropped);
        continue;
      }

      encode_timer.FillTimingInfo(si, &image, current_timestamp);

      if (IsTimingFrame(image)) {
        result[si].push_back(FrameType::kTiming);
      } else {
        result[si].push_back(FrameType::kNormal);
      }
    }
  }
  return result;
}
}  // namespace

TEST(FrameEncodeTimerTest, MarksTimingFramesPeriodicallyTogether) {
  const int64_t kDelayMs = 29;
  const size_t kMinFrameSize = 10;
  const size_t kMaxFrameSize = 20;
  const int kNumFrames = 1000;
  const int kNumStreams = 3;
  // No outliers as 1000 is larger than anything from range [10,20].
  const std::vector<size_t> kAverageSize = {1000, 1000, 1000};
  auto frames = GetTimingFrames(kDelayMs, kMinFrameSize, kMaxFrameSize,
                                kAverageSize, kNumStreams, kNumFrames);
  // Timing frames should be tirggered every delayMs.
  // As no outliers are expected, frames on all streams have to be
  // marked together.
  int last_timing_frame = -1;
  for (int i = 0; i < kNumFrames; ++i) {
    int num_normal = 0;
    int num_timing = 0;
    int num_dropped = 0;
    for (int s = 0; s < kNumStreams; ++s) {
      if (frames[s][i] == FrameType::kTiming) {
        ++num_timing;
      } else if (frames[s][i] == FrameType::kNormal) {
        ++num_normal;
      } else {
        ++num_dropped;
      }
    }
    // Can't have both normal and timing frames at the same timstamp.
    EXPECT_TRUE(num_timing == 0 || num_normal == 0);
    if (num_dropped < kNumStreams) {
      if (last_timing_frame == -1 || i >= last_timing_frame + kDelayMs) {
        // If didn't have timing frames for a period, current sent frame has to
        // be one. No normal frames should be sent.
        EXPECT_EQ(num_normal, 0);
      } else {
        // No unneeded timing frames should be sent.
        EXPECT_EQ(num_timing, 0);
      }
    }
    if (num_timing > 0)
      last_timing_frame = i;
  }
}

TEST(FrameEncodeTimerTest, MarksOutliers) {
  const int64_t kDelayMs = 29;
  const size_t kMinFrameSize = 2495;
  const size_t kMaxFrameSize = 2505;
  const int kNumFrames = 1000;
  const int kNumStreams = 3;
  // Possible outliers as 1000 lies in range [995, 1005].
  const std::vector<size_t> kAverageSize = {998, 1000, 1004};
  auto frames = GetTimingFrames(kDelayMs, kMinFrameSize, kMaxFrameSize,
                                kAverageSize, kNumStreams, kNumFrames);
  // All outliers should be marked.
  for (int i = 0; i < kNumFrames; ++i) {
    for (int s = 0; s < kNumStreams; ++s) {
      if (FrameSize(kMinFrameSize, kMaxFrameSize, s, i) >=
          kAverageSize[s] * kDefaultOutlierFrameSizePercent / 100) {
        // Too big frame. May be dropped or timing, but not normal.
        EXPECT_NE(frames[s][i], FrameType::kNormal);
      }
    }
  }
}

TEST(FrameEncodeTimerTest, NoTimingFrameIfNoEncodeStartTime) {
  int64_t timestamp = 1;
  constexpr size_t kFrameSize = 500;
  EncodedImage image;
  image.Allocate(kFrameSize);
  image.set_size(kFrameSize);
  image.capture_time_ms_ = timestamp;
  image.SetTimestamp(static_cast<uint32_t>(timestamp * 90));

  FakeEncodedImageCallback sink;
  FrameEncodeTimer encode_timer(&sink);
  VideoCodec codec_settings;
  // Make all frames timing frames.
  codec_settings.timing_frame_thresholds.delay_ms = 1;
  encode_timer.OnEncoderInit(codec_settings, false);
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  // Verify a single frame works with encode start time set.
  encode_timer.OnEncodeStarted(static_cast<uint32_t>(timestamp * 90),
                               timestamp);
  encode_timer.FillTimingInfo(0, &image, timestamp);
  EXPECT_TRUE(IsTimingFrame(image));

  // New frame, now skip OnEncodeStarted. Should not result in timing frame.
  image.capture_time_ms_ = ++timestamp;
  image.SetTimestamp(static_cast<uint32_t>(timestamp * 90));
  image.timing_ = EncodedImage::Timing();
  encode_timer.FillTimingInfo(0, &image, timestamp);
  EXPECT_FALSE(IsTimingFrame(image));
}

TEST(FrameEncodeTimerTest, AdjustsCaptureTimeForInternalSourceEncoder) {
  const int64_t kEncodeStartDelayMs = 2;
  const int64_t kEncodeFinishDelayMs = 10;
  constexpr size_t kFrameSize = 500;

  int64_t timestamp = 1;
  EncodedImage image;
  image.Allocate(kFrameSize);
  image.set_size(kFrameSize);
  image.capture_time_ms_ = timestamp;
  image.SetTimestamp(static_cast<uint32_t>(timestamp * 90));

  FakeEncodedImageCallback sink;
  FrameEncodeTimer encode_timer(&sink);

  VideoCodec codec_settings;
  // Make all frames timing frames.
  codec_settings.timing_frame_thresholds.delay_ms = 1;
  encode_timer.OnEncoderInit(codec_settings, true);

  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  // Verify a single frame without encode timestamps isn't a timing frame.
  encode_timer.FillTimingInfo(0, &image, timestamp);
  EXPECT_FALSE(IsTimingFrame(image));

  // New frame, but this time with encode timestamps set in timing_.
  // This should be a timing frame.
  image.capture_time_ms_ = ++timestamp;
  image.SetTimestamp(static_cast<uint32_t>(timestamp * 90));
  image.timing_ = EncodedImage::Timing();
  image.timing_.encode_start_ms = timestamp + kEncodeStartDelayMs;
  image.timing_.encode_finish_ms = timestamp + kEncodeFinishDelayMs;
  const int64_t kEncodeDoneTimestamp = 1234567;
  encode_timer.FillTimingInfo(0, &image, kEncodeDoneTimestamp);
  EXPECT_TRUE(IsTimingFrame(image));

  // Frame is captured kEncodeFinishDelayMs before it's encoded, so restored
  // capture timestamp should be kEncodeFinishDelayMs in the past.
  EXPECT_EQ(image.capture_time_ms_,
            kEncodeDoneTimestamp - kEncodeFinishDelayMs);
}

TEST(FrameEncodeTimerTest, NotifiesAboutDroppedFrames) {
  const int64_t kTimestampMs1 = 47721840;
  const int64_t kTimestampMs2 = 47721850;
  const int64_t kTimestampMs3 = 47721860;
  const int64_t kTimestampMs4 = 47721870;

  FakeEncodedImageCallback sink;
  FrameEncodeTimer encode_timer(&sink);
  encode_timer.OnEncoderInit(VideoCodec(), false);
  // Any non-zero bitrate needed to be set before the first frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  EncodedImage image;
  image.capture_time_ms_ = kTimestampMs1;
  image.SetTimestamp(static_cast<uint32_t>(image.capture_time_ms_ * 90));
  encode_timer.OnEncodeStarted(image.Timestamp(), image.capture_time_ms_);

  EXPECT_EQ(0u, sink.GetNumFramesDropped());
  encode_timer.FillTimingInfo(0, &image, kTimestampMs1);

  image.capture_time_ms_ = kTimestampMs2;
  image.SetTimestamp(static_cast<uint32_t>(image.capture_time_ms_ * 90));
  image.timing_ = EncodedImage::Timing();
  encode_timer.OnEncodeStarted(image.Timestamp(), image.capture_time_ms_);
  // No OnEncodedImageCall for timestamp2. Yet, at this moment it's not known
  // that frame with timestamp2 was dropped.
  EXPECT_EQ(0u, sink.GetNumFramesDropped());

  image.capture_time_ms_ = kTimestampMs3;
  image.SetTimestamp(static_cast<uint32_t>(image.capture_time_ms_ * 90));
  image.timing_ = EncodedImage::Timing();
  encode_timer.OnEncodeStarted(image.Timestamp(), image.capture_time_ms_);
  encode_timer.FillTimingInfo(0, &image, kTimestampMs3);
  EXPECT_EQ(1u, sink.GetNumFramesDropped());

  image.capture_time_ms_ = kTimestampMs4;
  image.SetTimestamp(static_cast<uint32_t>(image.capture_time_ms_ * 90));
  image.timing_ = EncodedImage::Timing();
  encode_timer.OnEncodeStarted(image.Timestamp(), image.capture_time_ms_);
  encode_timer.FillTimingInfo(0, &image, kTimestampMs4);
  EXPECT_EQ(1u, sink.GetNumFramesDropped());
}

TEST(FrameEncodeTimerTest, RestoresCaptureTimestamps) {
  EncodedImage image;
  const int64_t kTimestampMs = 123456;
  FakeEncodedImageCallback sink;

  FrameEncodeTimer encode_timer(&sink);
  encode_timer.OnEncoderInit(VideoCodec(), false);
  // Any non-zero bitrate needed to be set before the first frame.
  VideoBitrateAllocation bitrate_allocation;
  bitrate_allocation.SetBitrate(0, 0, 500000);
  encode_timer.OnSetRates(bitrate_allocation, 30);

  image.capture_time_ms_ = kTimestampMs;  // Correct timestamp.
  image.SetTimestamp(static_cast<uint32_t>(image.capture_time_ms_ * 90));
  encode_timer.OnEncodeStarted(image.Timestamp(), image.capture_time_ms_);
  image.capture_time_ms_ = 0;  // Incorrect timestamp.
  encode_timer.FillTimingInfo(0, &image, kTimestampMs);
  EXPECT_EQ(kTimestampMs, image.capture_time_ms_);
}

}  // namespace test
}  // namespace webrtc
