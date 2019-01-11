/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "api/video/video_bitrate_allocation.h"
#include "api/video/video_bitrate_allocator.h"
#include "api/video/video_frame.h"
#include "api/video/video_frame_buffer.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "common_types.h"  // NOLINT(build/include)
#include "modules/video_coding/encoder_database.h"
#include "modules/video_coding/generic_encoder.h"
#include "modules/video_coding/include/video_codec_interface.h"
#include "modules/video_coding/include/video_coding_defines.h"
#include "modules/video_coding/include/video_error_codes.h"
#include "modules/video_coding/internal_defines.h"
#include "modules/video_coding/utility/default_video_bitrate_allocator.h"
#include "modules/video_coding/video_coding_impl.h"
#include "rtc_base/checks.h"
#include "rtc_base/critical_section.h"
#include "rtc_base/logging.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "rtc_base/sequenced_task_checker.h"
#include "system_wrappers/include/clock.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace vcm {

VideoSender::VideoSender(Clock* clock,
                         EncodedImageCallback* post_encode_callback)
    : _encoder(nullptr),
      _encodedFrameCallback(post_encode_callback),
      _codecDataBase(&_encodedFrameCallback),
      current_codec_(),
      encoder_has_internal_source_(false),
      next_frame_types_(1, kVideoFrameDelta) {
  // Allow VideoSender to be created on one thread but used on another, post
  // construction. This is currently how this class is being used by at least
  // one external project (diffractor).
  sequenced_checker_.Detach();
}

VideoSender::~VideoSender() {}

// Register the send codec to be used.
int32_t VideoSender::RegisterSendCodec(const VideoCodec* sendCodec,
                                       uint32_t numberOfCores,
                                       uint32_t maxPayloadSize) {
  RTC_DCHECK(sequenced_checker_.CalledSequentially());
  rtc::CritScope lock(&encoder_crit_);
  if (sendCodec == nullptr) {
    return VCM_PARAMETER_ERROR;
  }

  bool ret =
      _codecDataBase.SetSendCodec(sendCodec, numberOfCores, maxPayloadSize);

  // Update encoder regardless of result to make sure that we're not holding on
  // to a deleted instance.
  _encoder = _codecDataBase.GetEncoder();
  // Cache the current codec here so they can be fetched from this thread
  // without requiring the _sendCritSect lock.
  current_codec_ = *sendCodec;

  if (!ret) {
    RTC_LOG(LS_ERROR) << "Failed to initialize set encoder with codec type '"
                      << sendCodec->codecType << "'.";
    return VCM_CODEC_ERROR;
  }

  // SetSendCodec succeeded, _encoder should be set.
  RTC_DCHECK(_encoder);

  {
    rtc::CritScope cs(&params_crit_);
    next_frame_types_.clear();
    next_frame_types_.resize(VCM_MAX(sendCodec->numberOfSimulcastStreams, 1),
                             kVideoFrameKey);
    // Cache InternalSource() to have this available from IntraFrameRequest()
    // without having to acquire encoder_crit_ (avoid blocking on encoder use).
    encoder_has_internal_source_ = _encoder->InternalSource();
  }

  RTC_LOG(LS_VERBOSE) << " max bitrate " << sendCodec->maxBitrate
                      << " start bitrate " << sendCodec->startBitrate
                      << " max frame rate " << sendCodec->maxFramerate
                      << " max payload size " << maxPayloadSize;
  return VCM_OK;
}

// Register an external decoder object.
// This can not be used together with external decoder callbacks.
void VideoSender::RegisterExternalEncoder(VideoEncoder* externalEncoder,
                                          bool internalSource /*= false*/) {
  RTC_DCHECK(sequenced_checker_.CalledSequentially());

  rtc::CritScope lock(&encoder_crit_);

  if (externalEncoder == nullptr) {
    _codecDataBase.DeregisterExternalEncoder();
    {
      // Make sure the VCM doesn't use the de-registered codec
      rtc::CritScope params_lock(&params_crit_);
      _encoder = nullptr;
      encoder_has_internal_source_ = false;
    }
    return;
  }
  _codecDataBase.RegisterExternalEncoder(externalEncoder,
                                         internalSource);
}

int32_t VideoSender::SetChannelParameters(
    const VideoBitrateAllocation& bitrate_allocation,
    uint32_t framerate_fps) {
  bool encoder_has_internal_source;
  {
    rtc::CritScope cs(&params_crit_);
    encoder_has_internal_source = encoder_has_internal_source_;
  }

  {
    rtc::CritScope cs(&encoder_crit_);
    if (_encoder) {
      // |target_bitrate == 0 | means that the network is down or the send pacer
      // is full. We currently only report this if the encoder has an internal
      // source. If the encoder does not have an internal source, higher levels
      // are expected to not call AddVideoFrame. We do this since its unclear
      // how current encoder implementations behave when given a zero target
      // bitrate.
      // TODO(perkj): Make sure all known encoder implementations handle zero
      // target bitrate and remove this check.
      if (!encoder_has_internal_source &&
          bitrate_allocation.get_sum_bps() == 0) {
        return VCM_OK;
      }

      if (framerate_fps == 0) {
        // No frame rate estimate available, use default.
        framerate_fps = current_codec_.maxFramerate;
      }
      if (_encoder != nullptr)
        _encoder->SetEncoderParameters(bitrate_allocation, framerate_fps);
    }
  }

  return VCM_OK;
}

// Add one raw video frame to the encoder, blocking.
int32_t VideoSender::AddVideoFrame(
    const VideoFrame& videoFrame,
    const CodecSpecificInfo* codecSpecificInfo,
    absl::optional<VideoEncoder::EncoderInfo> encoder_info) {
  std::vector<FrameType> next_frame_types;
  bool encoder_has_internal_source = false;
  {
    rtc::CritScope lock(&params_crit_);
    next_frame_types = next_frame_types_;
    encoder_has_internal_source = encoder_has_internal_source_;
  }
  rtc::CritScope lock(&encoder_crit_);
  if (_encoder == nullptr)
    return VCM_UNINITIALIZED;
  // TODO(pbos): Make sure setting send codec is synchronized with video
  // processing so frame size always matches.
  if (!_codecDataBase.MatchesCurrentResolution(videoFrame.width(),
                                               videoFrame.height())) {
    RTC_LOG(LS_ERROR)
        << "Incoming frame doesn't match set resolution. Dropping.";
    return VCM_PARAMETER_ERROR;
  }
  VideoFrame converted_frame = videoFrame;
  const VideoFrameBuffer::Type buffer_type =
      converted_frame.video_frame_buffer()->type();
  const bool is_buffer_type_supported =
      buffer_type == VideoFrameBuffer::Type::kI420 ||
      (buffer_type == VideoFrameBuffer::Type::kNative &&
       encoder_info->supports_native_handle);
  if (!is_buffer_type_supported) {
    // This module only supports software encoding.
    // TODO(pbos): Offload conversion from the encoder thread.
    rtc::scoped_refptr<I420BufferInterface> converted_buffer(
        converted_frame.video_frame_buffer()->ToI420());

    if (!converted_buffer) {
      RTC_LOG(LS_ERROR) << "Frame conversion failed, dropping frame.";
      return VCM_PARAMETER_ERROR;
    }
    converted_frame = VideoFrame::Builder()
                          .set_video_frame_buffer(converted_buffer)
                          .set_timestamp_rtp(converted_frame.timestamp())
                          .set_timestamp_ms(converted_frame.render_time_ms())
                          .set_rotation(converted_frame.rotation())
                          .set_id(converted_frame.id())
                          .build();
  }
  int32_t ret =
      _encoder->Encode(converted_frame, codecSpecificInfo, next_frame_types);
  if (ret < 0) {
    RTC_LOG(LS_ERROR) << "Failed to encode frame. Error code: " << ret;
    return ret;
  }

  {
    rtc::CritScope lock(&params_crit_);
    // Change all keyframe requests to encode delta frames the next time.
    for (size_t i = 0; i < next_frame_types_.size(); ++i) {
      // Check for equality (same requested as before encoding) to not
      // accidentally drop a keyframe request while encoding.
      if (next_frame_types[i] == next_frame_types_[i])
        next_frame_types_[i] = kVideoFrameDelta;
    }
  }
  return VCM_OK;
}

int32_t VideoSender::IntraFrameRequest(size_t stream_index) {
  {
    rtc::CritScope lock(&params_crit_);
    if (stream_index >= next_frame_types_.size()) {
      return -1;
    }
    next_frame_types_[stream_index] = kVideoFrameKey;
    if (!encoder_has_internal_source_)
      return VCM_OK;
  }
  // TODO(pbos): Remove when InternalSource() is gone. Both locks have to be
  // held here for internal consistency, since _encoder could be removed while
  // not holding encoder_crit_. Checks have to be performed again since
  // params_crit_ was dropped to not cause lock-order inversions with
  // encoder_crit_.
  rtc::CritScope lock(&encoder_crit_);
  rtc::CritScope params_lock(&params_crit_);
  if (stream_index >= next_frame_types_.size())
    return -1;
  if (_encoder != nullptr && _encoder->InternalSource()) {
    // Try to request the frame if we have an external encoder with
    // internal source since AddVideoFrame never will be called.
    if (_encoder->RequestFrame(next_frame_types_) == WEBRTC_VIDEO_CODEC_OK) {
      // Try to remove just-performed keyframe request, if stream still exists.
      next_frame_types_[stream_index] = kVideoFrameDelta;
    }
  }
  return VCM_OK;
}

}  // namespace vcm
}  // namespace webrtc
