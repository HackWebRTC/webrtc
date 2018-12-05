/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_VIDEO_ENCODER_PROXY_FACTORY_H_
#define TEST_VIDEO_ENCODER_PROXY_FACTORY_H_

#include <memory>
#include <vector>

#include "absl/memory/memory.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_factory.h"

namespace webrtc {
namespace test {

// An encoder factory with a single underlying VideoEncoder object,
// intended for test purposes. Each call to CreateVideoEncoder returns
// a proxy for the same encoder, typically an instance of FakeEncoder.
class VideoEncoderProxyFactory final : public VideoEncoderFactory {
 public:
  explicit VideoEncoderProxyFactory(VideoEncoder* encoder)
      : encoder_(encoder) {}

  // Unused by tests.
  std::vector<SdpVideoFormat> GetSupportedFormats() const override {
    RTC_NOTREACHED();
    return {};
  }

  std::unique_ptr<VideoEncoder> CreateVideoEncoder(
      const SdpVideoFormat& format) override {
    return absl::make_unique<EncoderProxy>(encoder_);
  }

 private:
  // Wrapper class, since CreateVideoEncoder needs to surrender
  // ownership to the object it returns.
  class EncoderProxy final : public VideoEncoder {
   public:
    explicit EncoderProxy(VideoEncoder* encoder) : encoder_(encoder) {}

   private:
    int32_t Encode(const VideoFrame& input_image,
                   const CodecSpecificInfo* codec_specific_info,
                   const std::vector<FrameType>* frame_types) override {
      return encoder_->Encode(input_image, codec_specific_info, frame_types);
    }
    int32_t InitEncode(const VideoCodec* config,
                       int32_t number_of_cores,
                       size_t max_payload_size) override {
      return encoder_->InitEncode(config, number_of_cores, max_payload_size);
    }
    int32_t RegisterEncodeCompleteCallback(
        EncodedImageCallback* callback) override {
      return encoder_->RegisterEncodeCompleteCallback(callback);
    }
    int32_t Release() override { return encoder_->Release(); }
    int32_t SetRateAllocation(const VideoBitrateAllocation& rate_allocation,
                              uint32_t framerate) override {
      return encoder_->SetRateAllocation(rate_allocation, framerate);
    }
    VideoEncoder::EncoderInfo GetEncoderInfo() const override {
      return encoder_->GetEncoderInfo();
    }

    VideoEncoder* const encoder_;
  };

  VideoEncoder* const encoder_;
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_VIDEO_ENCODER_PROXY_FACTORY_H_
