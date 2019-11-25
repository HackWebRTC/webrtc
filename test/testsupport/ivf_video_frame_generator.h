/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_TESTSUPPORT_IVF_VIDEO_FRAME_GENERATOR_H_
#define TEST_TESTSUPPORT_IVF_VIDEO_FRAME_GENERATOR_H_

#include <memory>
#include <string>

#include "absl/types/optional.h"
#include "api/video/video_codec_type.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_decoder.h"
#include "modules/video_coding/utility/ivf_file_reader.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/event.h"
#include "rtc_base/synchronization/sequence_checker.h"
#include "test/frame_generator.h"

namespace webrtc {
namespace test {

// All methods except constructor must be used from the same thread.
class IvfVideoFrameGenerator : public FrameGenerator {
 public:
  explicit IvfVideoFrameGenerator(const std::string& file_name);
  ~IvfVideoFrameGenerator() override;

  VideoFrame* NextFrame() override;
  void ChangeResolution(size_t width, size_t height) override;

 private:
  class DecodedCallback : public DecodedImageCallback {
   public:
    explicit DecodedCallback(IvfVideoFrameGenerator* reader)
        : reader_(reader) {}

    int32_t Decoded(VideoFrame& decoded_image) override;
    int32_t Decoded(VideoFrame& decoded_image, int64_t decode_time_ms) override;
    void Decoded(VideoFrame& decoded_image,
                 absl::optional<int32_t> decode_time_ms,
                 absl::optional<uint8_t> qp) override;

   private:
    IvfVideoFrameGenerator* const reader_;
  };

  void OnFrameDecoded(const VideoFrame& decoded_frame);
  static std::unique_ptr<VideoDecoder> CreateVideoDecoder(
      VideoCodecType codec_type);

  DecodedCallback callback_;
  std::unique_ptr<IvfFileReader> file_reader_;
  std::unique_ptr<VideoDecoder> video_decoder_;

  size_t width_;
  size_t height_;

  rtc::Event next_frame_decoded_;
  SequenceChecker sequence_checker_;

  rtc::CriticalSection lock_;
  absl::optional<VideoFrame> next_frame_ RTC_GUARDED_BY(lock_);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_IVF_VIDEO_FRAME_GENERATOR_H_
