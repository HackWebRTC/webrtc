/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video/video_stream_decoder_impl.h"

#include "rtc_base/logging.h"
#include "rtc_base/ptr_util.h"

namespace webrtc {

VideoStreamDecoderImpl::VideoStreamDecoderImpl(
    VideoStreamDecoder::Callbacks* callbacks,
    VideoDecoderFactory* decoder_factory,
    std::map<int, std::pair<SdpVideoFormat, int>> decoder_settings)
    : callbacks_(callbacks),
      decoder_factory_(decoder_factory),
      decoder_settings_(std::move(decoder_settings)),
      bookkeeping_queue_("video_stream_decoder_bookkeeping_queue"),
      jitter_estimator_(Clock::GetRealTimeClock()),
      timing_(Clock::GetRealTimeClock()),
      frame_buffer_(Clock::GetRealTimeClock(),
                    &jitter_estimator_,
                    &timing_,
                    nullptr) {}

VideoStreamDecoderImpl::~VideoStreamDecoderImpl() {
  frame_buffer_.Stop();
}

void VideoStreamDecoderImpl::OnFrame(
    std::unique_ptr<video_coding::EncodedFrame> frame) {
  if (!bookkeeping_queue_.IsCurrent()) {
    struct OnFrameTask : rtc::QueuedTask {
      OnFrameTask(std::unique_ptr<video_coding::EncodedFrame> frame,
                  VideoStreamDecoderImpl* video_stream_decoder)
          : frame_(std::move(frame)),
            video_stream_decoder_(video_stream_decoder) {}

      bool Run() override {
        video_stream_decoder_->OnFrame(std::move(frame_));
        return true;
      }

      std::unique_ptr<video_coding::EncodedFrame> frame_;
      VideoStreamDecoderImpl* video_stream_decoder_;
    };

    bookkeeping_queue_.PostTask(
        rtc::MakeUnique<OnFrameTask>(std::move(frame), this));
    return;
  }

  RTC_DCHECK_RUN_ON(&bookkeeping_queue_);

  uint64_t continuous_pid = frame_buffer_.InsertFrame(std::move(frame));
  video_coding::VideoLayerFrameId continuous_id(continuous_pid, 0);
  if (last_continuous_id_ < continuous_id) {
    last_continuous_id_ = continuous_id;
    callbacks_->OnContinuousUntil(last_continuous_id_);
  }
}

VideoDecoder* VideoStreamDecoderImpl::GetDecoder(int payload_type) {
  if (current_payload_type_ == payload_type) {
    RTC_DCHECK(decoder_);
    return decoder_.get();
  }

  current_payload_type_.reset();
  decoder_.reset();

  auto decoder_settings_it = decoder_settings_.find(payload_type);
  if (decoder_settings_it == decoder_settings_.end()) {
    RTC_LOG(LS_WARNING) << "Payload type " << payload_type
                        << " not registered.";
    return nullptr;
  }

  const SdpVideoFormat& video_format = decoder_settings_it->second.first;
  std::unique_ptr<VideoDecoder> decoder =
      decoder_factory_->CreateVideoDecoder(video_format);
  if (!decoder) {
    RTC_LOG(LS_WARNING) << "Failed to create decoder for payload type "
                        << payload_type << ".";
    return nullptr;
  }

  int num_cores = decoder_settings_it->second.second;
  int32_t init_result = decoder->InitDecode(nullptr, num_cores);
  if (init_result != WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "Failed to initialize decoder for payload type "
                        << payload_type << ".";
    return nullptr;
  }

  int32_t register_result = decoder->RegisterDecodeCompleteCallback(this);
  if (register_result != WEBRTC_VIDEO_CODEC_OK) {
    RTC_LOG(LS_WARNING) << "Failed to register decode callback.";
    return nullptr;
  }

  current_payload_type_.emplace(payload_type);
  decoder_ = std::move(decoder);
  return decoder_.get();
}

}  // namespace webrtc
