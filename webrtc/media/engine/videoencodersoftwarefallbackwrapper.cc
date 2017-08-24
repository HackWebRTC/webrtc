/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/engine/videoencodersoftwarefallbackwrapper.h"

#include "webrtc/media/base/h264_profile_level_id.h"
#include "webrtc/media/engine/internalencoderfactory.h"
#include "webrtc/modules/video_coding/include/video_error_codes.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/timeutils.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace webrtc {
namespace {
const char kVp8ForceFallbackEncoderFieldTrial[] =
    "WebRTC-VP8-Forced-Fallback-Encoder";

bool EnableForcedFallback(const cricket::VideoCodec& codec) {
  if (!webrtc::field_trial::IsEnabled(kVp8ForceFallbackEncoderFieldTrial))
    return false;

  return (PayloadStringToCodecType(codec.name) == kVideoCodecVP8);
}

bool IsForcedFallbackPossible(const VideoCodec& codec_settings) {
  return codec_settings.codecType == kVideoCodecVP8 &&
         codec_settings.numberOfSimulcastStreams <= 1 &&
         codec_settings.VP8().numberOfTemporalLayers == 1;
}

void GetForcedFallbackParamsFromFieldTrialGroup(uint32_t* param_low_kbps,
                                                uint32_t* param_high_kbps,
                                                int64_t* param_min_low_ms) {
  RTC_DCHECK(param_low_kbps);
  RTC_DCHECK(param_high_kbps);
  RTC_DCHECK(param_min_low_ms);
  std::string group =
      webrtc::field_trial::FindFullName(kVp8ForceFallbackEncoderFieldTrial);
  if (group.empty())
    return;

  int low_kbps;
  int high_kbps;
  int min_low_ms;
  int min_pixels;
  if (sscanf(group.c_str(), "Enabled-%d,%d,%d,%d", &low_kbps, &high_kbps,
             &min_low_ms, &min_pixels) != 4) {
    LOG(LS_WARNING) << "Invalid number of forced fallback parameters provided.";
    return;
  }
  if (min_low_ms <= 0 || min_pixels <= 0 || low_kbps <= 0 ||
      high_kbps <= low_kbps) {
    LOG(LS_WARNING) << "Invalid forced fallback parameter value provided.";
    return;
  }
  *param_low_kbps = low_kbps;
  *param_high_kbps = high_kbps;
  *param_min_low_ms = min_low_ms;
}
}  // namespace

VideoEncoderSoftwareFallbackWrapper::VideoEncoderSoftwareFallbackWrapper(
    const cricket::VideoCodec& codec,
    webrtc::VideoEncoder* encoder)
    : number_of_cores_(0),
      max_payload_size_(0),
      rates_set_(false),
      framerate_(0),
      channel_parameters_set_(false),
      packet_loss_(0),
      rtt_(0),
      codec_(codec),
      encoder_(encoder),
      callback_(nullptr),
      forced_fallback_possible_(EnableForcedFallback(codec)) {
  if (forced_fallback_possible_) {
    GetForcedFallbackParamsFromFieldTrialGroup(&forced_fallback_.low_kbps,
                                               &forced_fallback_.high_kbps,
                                               &forced_fallback_.min_low_ms);
  }
}

bool VideoEncoderSoftwareFallbackWrapper::InitFallbackEncoder() {
  MaybeModifyCodecForFallback();
  cricket::InternalEncoderFactory internal_factory;
  if (!FindMatchingCodec(internal_factory.supported_codecs(), codec_)) {
    LOG(LS_WARNING)
        << "Encoder requesting fallback to codec not supported in software.";
    return false;
  }
  fallback_encoder_.reset(internal_factory.CreateVideoEncoder(codec_));
  if (fallback_encoder_->InitEncode(&codec_settings_, number_of_cores_,
                                    max_payload_size_) !=
      WEBRTC_VIDEO_CODEC_OK) {
    LOG(LS_ERROR) << "Failed to initialize software-encoder fallback.";
    fallback_encoder_->Release();
    fallback_encoder_.reset();
    return false;
  }
  // Replay callback, rates, and channel parameters.
  if (callback_)
    fallback_encoder_->RegisterEncodeCompleteCallback(callback_);
  if (rates_set_)
    fallback_encoder_->SetRateAllocation(bitrate_allocation_, framerate_);
  if (channel_parameters_set_)
    fallback_encoder_->SetChannelParameters(packet_loss_, rtt_);

  fallback_implementation_name_ =
      std::string(fallback_encoder_->ImplementationName()) +
      " (fallback from: " + encoder_->ImplementationName() + ")";
  // Since we're switching to the fallback encoder, Release the real encoder. It
  // may be re-initialized via InitEncode later, and it will continue to get
  // Set calls for rates and channel parameters in the meantime.
  encoder_->Release();
  return true;
}

int32_t VideoEncoderSoftwareFallbackWrapper::InitEncode(
    const VideoCodec* codec_settings,
    int32_t number_of_cores,
    size_t max_payload_size) {
  // Store settings, in case we need to dynamically switch to the fallback
  // encoder after a failed Encode call.
  codec_settings_ = *codec_settings;
  number_of_cores_ = number_of_cores;
  max_payload_size_ = max_payload_size;
  // Clear stored rate/channel parameters.
  rates_set_ = false;
  channel_parameters_set_ = false;
  ValidateSettingsForForcedFallback();

  // Try to reinit forced software codec if it is in use.
  if (TryReInitForcedFallbackEncoder()) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  forced_fallback_.Reset();

  int32_t ret =
      encoder_->InitEncode(codec_settings, number_of_cores, max_payload_size);
  if (ret == WEBRTC_VIDEO_CODEC_OK || codec_.name.empty()) {
    if (fallback_encoder_)
      fallback_encoder_->Release();
    fallback_encoder_.reset();
    if (callback_)
      encoder_->RegisterEncodeCompleteCallback(callback_);
    return ret;
  }
  // Try to instantiate software codec.
  if (InitFallbackEncoder()) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
  // Software encoder failed, use original return code.
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::RegisterEncodeCompleteCallback(
    EncodedImageCallback* callback) {
  callback_ = callback;
  int32_t ret = encoder_->RegisterEncodeCompleteCallback(callback);
  if (fallback_encoder_)
    return fallback_encoder_->RegisterEncodeCompleteCallback(callback);
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::Release() {
  // If the fallback_encoder_ is non-null, it means it was created via
  // InitFallbackEncoder which has Release()d encoder_, so we should only ever
  // need to Release() whichever one is active.
  if (fallback_encoder_)
    return fallback_encoder_->Release();
  return encoder_->Release();
}

int32_t VideoEncoderSoftwareFallbackWrapper::Encode(
    const VideoFrame& frame,
    const CodecSpecificInfo* codec_specific_info,
    const std::vector<FrameType>* frame_types) {
  if (TryReleaseForcedFallbackEncoder()) {
    // Frame may have been converted from kNative to kI420 during fallback.
    if (encoder_->SupportsNativeHandle() &&
        frame.video_frame_buffer()->type() != VideoFrameBuffer::Type::kNative) {
      LOG(LS_WARNING) << "Encoder supports native frames, dropping one frame "
                      << "to avoid possible reconfig due to format change.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }
  }
  if (fallback_encoder_)
    return fallback_encoder_->Encode(frame, codec_specific_info, frame_types);
  int32_t ret = encoder_->Encode(frame, codec_specific_info, frame_types);
  // If requested, try a software fallback.
  bool fallback_requested =
      (ret == WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE) ||
      (ret == WEBRTC_VIDEO_CODEC_OK && RequestForcedFallback());
  if (fallback_requested && InitFallbackEncoder()) {
    // Fallback was successful.
    if (ret == WEBRTC_VIDEO_CODEC_FALLBACK_SOFTWARE)
      forced_fallback_.Reset();  // Not a forced fallback.
    if (frame.video_frame_buffer()->type() == VideoFrameBuffer::Type::kNative &&
        !fallback_encoder_->SupportsNativeHandle()) {
      LOG(LS_WARNING) << "Fallback encoder doesn't support native frames, "
                      << "dropping one frame.";
      return WEBRTC_VIDEO_CODEC_ERROR;
    }

    // Start using the fallback with this frame.
    return fallback_encoder_->Encode(frame, codec_specific_info, frame_types);
  }
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::SetChannelParameters(
    uint32_t packet_loss,
    int64_t rtt) {
  channel_parameters_set_ = true;
  packet_loss_ = packet_loss;
  rtt_ = rtt;
  int32_t ret = encoder_->SetChannelParameters(packet_loss, rtt);
  if (fallback_encoder_)
    return fallback_encoder_->SetChannelParameters(packet_loss, rtt);
  return ret;
}

int32_t VideoEncoderSoftwareFallbackWrapper::SetRateAllocation(
    const BitrateAllocation& bitrate_allocation,
    uint32_t framerate) {
  rates_set_ = true;
  bitrate_allocation_ = bitrate_allocation;
  framerate_ = framerate;
  int32_t ret = encoder_->SetRateAllocation(bitrate_allocation_, framerate);
  if (fallback_encoder_)
    return fallback_encoder_->SetRateAllocation(bitrate_allocation_, framerate);
  return ret;
}

bool VideoEncoderSoftwareFallbackWrapper::SupportsNativeHandle() const {
  if (fallback_encoder_)
    return fallback_encoder_->SupportsNativeHandle();
  return encoder_->SupportsNativeHandle();
}

VideoEncoder::ScalingSettings
VideoEncoderSoftwareFallbackWrapper::GetScalingSettings() const {
  if (forced_fallback_possible_ && fallback_encoder_)
    return fallback_encoder_->GetScalingSettings();
  return encoder_->GetScalingSettings();
}

const char *VideoEncoderSoftwareFallbackWrapper::ImplementationName() const {
  if (fallback_encoder_)
    return fallback_encoder_->ImplementationName();
  return encoder_->ImplementationName();
}

bool VideoEncoderSoftwareFallbackWrapper::IsForcedFallbackActive() const {
  return (forced_fallback_possible_ && fallback_encoder_ &&
          forced_fallback_.start_ms);
}

bool VideoEncoderSoftwareFallbackWrapper::RequestForcedFallback() {
  if (!forced_fallback_possible_ || fallback_encoder_ || !rates_set_)
    return false;

  // No fallback encoder.
  return forced_fallback_.ShouldStart(bitrate_allocation_.get_sum_kbps(),
                                      codec_settings_);
}

bool VideoEncoderSoftwareFallbackWrapper::TryReleaseForcedFallbackEncoder() {
  if (!IsForcedFallbackActive())
    return false;

  if (!forced_fallback_.ShouldStop(bitrate_allocation_.get_sum_kbps(),
                                   codec_settings_)) {
    return false;
  }

  // Release the forced fallback encoder.
  if (encoder_->InitEncode(&codec_settings_, number_of_cores_,
                           max_payload_size_) == WEBRTC_VIDEO_CODEC_OK) {
    LOG(LS_INFO) << "Stop forced SW encoder fallback, max bitrate exceeded.";
    fallback_encoder_->Release();
    fallback_encoder_.reset();
    forced_fallback_.Reset();
    return true;
  }
  return false;
}

bool VideoEncoderSoftwareFallbackWrapper::TryReInitForcedFallbackEncoder() {
  if (!IsForcedFallbackActive())
    return false;

  // Encoder reconfigured.
  if (!forced_fallback_.IsValid(codec_settings_)) {
    LOG(LS_INFO) << "Stop forced SW encoder fallback, max pixels exceeded.";
    return false;
  }
  // Settings valid, reinitialize the forced fallback encoder.
  if (fallback_encoder_->InitEncode(&codec_settings_, number_of_cores_,
                                    max_payload_size_) !=
      WEBRTC_VIDEO_CODEC_OK) {
    LOG(LS_ERROR) << "Failed to init forced SW encoder fallback.";
    return false;
  }
  return true;
}

void VideoEncoderSoftwareFallbackWrapper::ValidateSettingsForForcedFallback() {
  if (!forced_fallback_possible_)
    return;

  if (!IsForcedFallbackPossible(codec_settings_)) {
    if (IsForcedFallbackActive()) {
      fallback_encoder_->Release();
      fallback_encoder_.reset();
    }
    LOG(LS_INFO) << "Disable forced_fallback_possible_ due to settings.";
    forced_fallback_possible_ = false;
  }
}

bool VideoEncoderSoftwareFallbackWrapper::ForcedFallbackParams::ShouldStart(
    uint32_t bitrate_kbps,
    const VideoCodec& codec) {
  if (bitrate_kbps > low_kbps || !IsValid(codec)) {
    start_ms.reset();
    return false;
  }

  // Has bitrate been below |low_kbps| for long enough duration.
  int64_t now_ms = rtc::TimeMillis();
  if (!start_ms)
    start_ms.emplace(now_ms);

  if ((now_ms - *start_ms) >= min_low_ms) {
    LOG(LS_INFO) << "Request forced SW encoder fallback.";
    // In case the request fails, update time to avoid too frequent requests.
    start_ms.emplace(now_ms);
    return true;
  }
  return false;
}

bool VideoEncoderSoftwareFallbackWrapper::ForcedFallbackParams::ShouldStop(
    uint32_t bitrate_kbps,
    const VideoCodec& codec) const {
  return bitrate_kbps >= high_kbps &&
         (codec.width * codec.height >= kMinPixelsStop);
}

void VideoEncoderSoftwareFallbackWrapper::MaybeModifyCodecForFallback() {
  // We have a specific case for H264 ConstrainedBaseline because that is the
  // only supported profile in Sw fallback.
  if (!cricket::CodecNamesEq(codec_.name.c_str(), cricket::kH264CodecName))
    return;
  codec_.SetParam(cricket::kH264FmtpProfileLevelId,
                  cricket::kH264ProfileLevelConstrainedBaseline);
}

}  // namespace webrtc
