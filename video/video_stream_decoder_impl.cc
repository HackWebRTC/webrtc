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
#include "rtc_base/numerics/mod_ops.h"
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
      decode_thread_(&DecodeLoop,
                     this,
                     "video_stream_decoder_decode_thread",
                     rtc::kHighestPriority),
      jitter_estimator_(Clock::GetRealTimeClock()),
      timing_(Clock::GetRealTimeClock()),
      frame_buffer_(Clock::GetRealTimeClock(),
                    &jitter_estimator_,
                    &timing_,
                    nullptr),
      next_start_time_index_(0) {
  decode_start_time_.fill({-1, 0});
  decode_thread_.Start();
}

VideoStreamDecoderImpl::~VideoStreamDecoderImpl() {
  frame_buffer_.Stop();
  decode_thread_.Stop();
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

// static
void VideoStreamDecoderImpl::DecodeLoop(void* ptr) {
  // TODO(philipel): Remove this and use rtc::Event::kForever when it's
  //                 supported by the |frame_buffer_|.
  static constexpr int kForever = 100000000;

  int max_wait_time_ms = kForever;
  bool keyframe_required = true;
  auto* vs_decoder = static_cast<VideoStreamDecoderImpl*>(ptr);
  while (true) {
    DecodeResult decode_result =
        vs_decoder->DecodeNextFrame(max_wait_time_ms, keyframe_required);

    switch (decode_result) {
      case kOk: {
        max_wait_time_ms = kForever;
        keyframe_required = false;
        break;
      }
      case kDecodeFailure: {
        max_wait_time_ms = 0;
        keyframe_required = true;
        break;
      }
      case kNoFrame: {
        max_wait_time_ms = kForever;
        // If we end up here it means that we got a decoding error and there is
        // no keyframe available in the |frame_buffer_|.
        vs_decoder->bookkeeping_queue_.PostTask([vs_decoder]() {
          RTC_DCHECK_RUN_ON(&vs_decoder->bookkeeping_queue_);
          vs_decoder->callbacks_->OnNonDecodableState();
        });
        break;
      }
      case kNoDecoder: {
        max_wait_time_ms = kForever;
        break;
      }
      case kShutdown: {
        return;
      }
    }
  }
}

VideoStreamDecoderImpl::DecodeResult VideoStreamDecoderImpl::DecodeNextFrame(
    int max_wait_time_ms,
    bool keyframe_required) {
  std::unique_ptr<video_coding::EncodedFrame> frame;
  video_coding::FrameBuffer::ReturnReason res =
      frame_buffer_.NextFrame(max_wait_time_ms, &frame, keyframe_required);

  if (res == video_coding::FrameBuffer::ReturnReason::kStopped)
    return kShutdown;

  if (frame) {
    VideoDecoder* decoder = GetDecoder(frame->PayloadType());
    if (!decoder) {
      RTC_LOG(LS_WARNING) << "Failed to get decoder, dropping frame ("
                          << frame->id.picture_id << ":"
                          << frame->id.spatial_layer << ").";
      return kNoDecoder;
    }

    int64_t decode_start_time_ms = rtc::TimeMillis();
    uint32_t frame_timestamp = frame->timestamp;
    bookkeeping_queue_.PostTask(
        [this, decode_start_time_ms, frame_timestamp]() {
          RTC_DCHECK_RUN_ON(&bookkeeping_queue_);
          // Saving decode start time this way wont work if we decode spatial
          // layers sequentially.
          decode_start_time_[next_start_time_index_] = {frame_timestamp,
                                                        decode_start_time_ms};
          next_start_time_index_ =
              Add<kDecodeTimeMemory>(next_start_time_index_, 1);
        });

    int32_t decode_result =
        decoder->Decode(frame->EncodedImage(),
                        false,    // missing_frame
                        nullptr,  // rtp fragmentation header
                        nullptr,  // codec specific info
                        frame->RenderTimeMs());

    return decode_result == WEBRTC_VIDEO_CODEC_OK ? kOk : kDecodeFailure;
  }

  return kNoFrame;
}

}  // namespace webrtc
