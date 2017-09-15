/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MEDIA_ENGINE_VIDEOENCODERSOFTWAREFALLBACKWRAPPER_H_
#define MEDIA_ENGINE_VIDEOENCODERSOFTWAREFALLBACKWRAPPER_H_

#include <memory>
#include <string>
#include <vector>

#include "api/video_codecs/video_encoder.h"
#include "media/base/codec.h"

namespace webrtc {

// Class used to wrap external VideoEncoders to provide a fallback option on
// software encoding when a hardware encoder fails to encode a stream due to
// hardware restrictions, such as max resolution.
class VideoEncoderSoftwareFallbackWrapper : public VideoEncoder {
 public:
  VideoEncoderSoftwareFallbackWrapper(
      const cricket::VideoCodec& codec,
      std::unique_ptr<webrtc::VideoEncoder> encoder);

  int32_t InitEncode(const VideoCodec* codec_settings,
                     int32_t number_of_cores,
                     size_t max_payload_size) override;

  int32_t RegisterEncodeCompleteCallback(
      EncodedImageCallback* callback) override;

  int32_t Release() override;
  int32_t Encode(const VideoFrame& frame,
                 const CodecSpecificInfo* codec_specific_info,
                 const std::vector<FrameType>* frame_types) override;
  int32_t SetChannelParameters(uint32_t packet_loss, int64_t rtt) override;
  int32_t SetRateAllocation(const BitrateAllocation& bitrate_allocation,
                            uint32_t framerate) override;
  bool SupportsNativeHandle() const override;
  ScalingSettings GetScalingSettings() const override;
  const char *ImplementationName() const override;

 private:
  bool InitFallbackEncoder();

  // If |forced_fallback_possible_| is true:
  // The forced fallback is requested if the target bitrate is below |low_kbps|
  // for more than |min_low_ms| and the input video resolution is not larger
  // than |kMaxPixelsStart|.
  // If the bitrate is above |high_kbps| and the resolution is not smaller than
  // |kMinPixelsStop|, the forced fallback is requested to immediately be
  // stopped.
  class ForcedFallbackParams {
   public:
    bool ShouldStart(uint32_t bitrate_kbps, const VideoCodec& codec);
    bool ShouldStop(uint32_t bitrate_kbps, const VideoCodec& codec) const;
    void Reset() { start_ms.reset(); }
    bool IsValid(const VideoCodec& codec) const {
      return codec.width * codec.height <= kMaxPixelsStart;
    }
    rtc::Optional<int64_t> start_ms;  // Set when bitrate is below |low_kbps|.
    uint32_t low_kbps = 100;
    uint32_t high_kbps = 150;
    int64_t min_low_ms = 10000;
    const int kMaxPixelsStart = 320 * 240;
    const int kMinPixelsStop = 320 * 180;
  };

  bool RequestForcedFallback();
  bool TryReleaseForcedFallbackEncoder();
  bool TryReInitForcedFallbackEncoder();
  void ValidateSettingsForForcedFallback();
  bool IsForcedFallbackActive() const;
  void MaybeModifyCodecForFallback();

  // Settings used in the last InitEncode call and used if a dynamic fallback to
  // software is required.
  VideoCodec codec_settings_;
  int32_t number_of_cores_;
  size_t max_payload_size_;

  // The last bitrate/framerate set, and a flag for noting they are set.
  bool rates_set_;
  BitrateAllocation bitrate_allocation_;
  uint32_t framerate_;

  // The last channel parameters set, and a flag for noting they are set.
  bool channel_parameters_set_;
  uint32_t packet_loss_;
  int64_t rtt_;

  cricket::VideoCodec codec_;
  std::unique_ptr<webrtc::VideoEncoder> encoder_;

  std::unique_ptr<webrtc::VideoEncoder> fallback_encoder_;
  std::string fallback_implementation_name_;
  EncodedImageCallback* callback_;

  bool forced_fallback_possible_;
  ForcedFallbackParams forced_fallback_;
};

}  // namespace webrtc

#endif  // MEDIA_ENGINE_VIDEOENCODERSOFTWAREFALLBACKWRAPPER_H_
