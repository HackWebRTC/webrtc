/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_VIDEO_STREAM_DECODER_IMPL_H_
#define VIDEO_VIDEO_STREAM_DECODER_IMPL_H_

#include <map>
#include <memory>
#include <utility>

#include "api/video/video_stream_decoder.h"
#include "modules/video_coding/frame_buffer2.h"
#include "modules/video_coding/jitter_estimator.h"
#include "modules/video_coding/timing.h"
#include "rtc_base/task_queue.h"
#include "rtc_base/thread_checker.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {

class VideoStreamDecoderImpl : public VideoStreamDecoder,
                               private DecodedImageCallback {
 public:
  VideoStreamDecoderImpl(
      VideoStreamDecoder::Callbacks* callbacks,
      VideoDecoderFactory* decoder_factory,
      std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings);

  ~VideoStreamDecoderImpl() override;

  void OnFrame(std::unique_ptr<video_coding::EncodedFrame> frame) override;

 private:
  // Implements DecodedImageCallback interface
  int32_t Decoded(VideoFrame& decodedImage) override;
  int32_t Decoded(VideoFrame& decodedImage, int64_t decode_time_ms) override;
  void Decoded(VideoFrame& decodedImage,
               rtc::Optional<int32_t> decode_time_ms,
               rtc::Optional<uint8_t> qp) override;

  VideoStreamDecoder::Callbacks* const callbacks_
      RTC_PT_GUARDED_BY(bookkeeping_queue_);
  VideoDecoderFactory* const decoder_factory_;
  std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings_;

  // The |bookkeeping_queue_| is used to:
  //  - Make |callbacks_|.
  //  - Insert/extract frames from the |frame_buffer_|
  //  - Synchronize with whatever thread that makes the Decoded callback.
  rtc::TaskQueue bookkeeping_queue_;

  VCMJitterEstimator jitter_estimator_;
  VCMTiming timing_;
  video_coding::FrameBuffer frame_buffer_;
  video_coding::VideoLayerFrameId last_continuous_id_;
};

}  // namespace webrtc

#endif  // VIDEO_VIDEO_STREAM_DECODER_IMPL_H_
