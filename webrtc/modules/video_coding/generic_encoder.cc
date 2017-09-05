/*
*  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "webrtc/modules/video_coding/generic_encoder.h"

#include <vector>

#include "webrtc/api/optional.h"
#include "webrtc/api/video/i420_buffer.h"
#include "webrtc/modules/pacing/alr_detector.h"
#include "webrtc/modules/video_coding/encoded_frame.h"
#include "webrtc/modules/video_coding/media_optimization.h"
#include "webrtc/rtc_base/checks.h"
#include "webrtc/rtc_base/logging.h"
#include "webrtc/rtc_base/timeutils.h"
#include "webrtc/rtc_base/trace_event.h"
#include "webrtc/system_wrappers/include/field_trial.h"

namespace webrtc {

VCMGenericEncoder::VCMGenericEncoder(
    VideoEncoder* encoder,
    VCMEncodedFrameCallback* encoded_frame_callback,
    bool internal_source)
    : encoder_(encoder),
      vcm_encoded_frame_callback_(encoded_frame_callback),
      internal_source_(internal_source),
      encoder_params_({BitrateAllocation(), 0, 0, 0}),
      is_screenshare_(false),
      streams_or_svc_num_(0) {}

VCMGenericEncoder::~VCMGenericEncoder() {}

int32_t VCMGenericEncoder::Release() {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  TRACE_EVENT0("webrtc", "VCMGenericEncoder::Release");
  return encoder_->Release();
}

int32_t VCMGenericEncoder::InitEncode(const VideoCodec* settings,
                                      int32_t number_of_cores,
                                      size_t max_payload_size) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  TRACE_EVENT0("webrtc", "VCMGenericEncoder::InitEncode");
  is_screenshare_ = settings->mode == VideoCodecMode::kScreensharing;
  streams_or_svc_num_ = settings->numberOfSimulcastStreams;
  if (settings->codecType == kVideoCodecVP9) {
    streams_or_svc_num_ = settings->VP9().numberOfSpatialLayers;
  }
  if (streams_or_svc_num_ == 0)
    streams_or_svc_num_ = 1;

  vcm_encoded_frame_callback_->SetTimingFramesThresholds(
      settings->timing_frame_thresholds);
  vcm_encoded_frame_callback_->OnFrameRateChanged(settings->maxFramerate);

  if (encoder_->InitEncode(settings, number_of_cores, max_payload_size) != 0) {
    LOG(LS_ERROR) << "Failed to initialize the encoder associated with "
                     "payload name: "
                  << settings->plName;
    return -1;
  }
  encoder_->RegisterEncodeCompleteCallback(vcm_encoded_frame_callback_);
  return 0;
}

int32_t VCMGenericEncoder::Encode(const VideoFrame& frame,
                                  const CodecSpecificInfo* codec_specific,
                                  const std::vector<FrameType>& frame_types) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  TRACE_EVENT1("webrtc", "VCMGenericEncoder::Encode", "timestamp",
               frame.timestamp());

  for (FrameType frame_type : frame_types)
    RTC_DCHECK(frame_type == kVideoFrameKey || frame_type == kVideoFrameDelta);

  for (size_t i = 0; i < streams_or_svc_num_; ++i)
    vcm_encoded_frame_callback_->OnEncodeStarted(frame.render_time_ms(), i);
  int32_t result = encoder_->Encode(frame, codec_specific, &frame_types);

  if (is_screenshare_ &&
      result == WEBRTC_VIDEO_CODEC_TARGET_BITRATE_OVERSHOOT) {
    // Target bitrate exceeded, encoder state has been reset - try again.
    return encoder_->Encode(frame, codec_specific, &frame_types);
  }

  return result;
}

void VCMGenericEncoder::SetEncoderParameters(const EncoderParameters& params) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  bool channel_parameters_have_changed;
  bool rates_have_changed;
  {
    rtc::CritScope lock(&params_lock_);
    channel_parameters_have_changed =
        params.loss_rate != encoder_params_.loss_rate ||
        params.rtt != encoder_params_.rtt;
    rates_have_changed =
        params.target_bitrate != encoder_params_.target_bitrate ||
        params.input_frame_rate != encoder_params_.input_frame_rate;
    encoder_params_ = params;
  }
  if (channel_parameters_have_changed) {
    int res = encoder_->SetChannelParameters(params.loss_rate, params.rtt);
    if (res != 0) {
      LOG(LS_WARNING) << "Error set encoder parameters (loss = "
                      << params.loss_rate << ", rtt = " << params.rtt
                      << "): " << res;
    }
  }
  if (rates_have_changed) {
    int res = encoder_->SetRateAllocation(params.target_bitrate,
                                          params.input_frame_rate);
    if (res != 0) {
      LOG(LS_WARNING) << "Error set encoder rate (total bitrate bps = "
                      << params.target_bitrate.get_sum_bps()
                      << ", framerate = " << params.input_frame_rate
                      << "): " << res;
    }
    vcm_encoded_frame_callback_->OnFrameRateChanged(params.input_frame_rate);
    for (size_t i = 0; i < streams_or_svc_num_; ++i) {
      size_t layer_bitrate_bytes_per_sec =
          params.target_bitrate.GetSpatialLayerSum(i) / 8;
      // VP9 rate control is not yet moved out of VP9Impl. Due to that rates
      // are not split among spatial layers.
      if (layer_bitrate_bytes_per_sec == 0)
        layer_bitrate_bytes_per_sec = params.target_bitrate.get_sum_bps() / 8;
      vcm_encoded_frame_callback_->OnTargetBitrateChanged(
          layer_bitrate_bytes_per_sec, i);
    }
  }
}

EncoderParameters VCMGenericEncoder::GetEncoderParameters() const {
  rtc::CritScope lock(&params_lock_);
  return encoder_params_;
}

int32_t VCMGenericEncoder::SetPeriodicKeyFrames(bool enable) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  return encoder_->SetPeriodicKeyFrames(enable);
}

int32_t VCMGenericEncoder::RequestFrame(
    const std::vector<FrameType>& frame_types) {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);

  for (size_t i = 0; i < streams_or_svc_num_; ++i)
    vcm_encoded_frame_callback_->OnEncodeStarted(0, i);
  // TODO(nisse): Used only with internal source. Delete as soon as
  // that feature is removed. The only implementation I've been able
  // to find ignores what's in the frame. With one exception: It seems
  // a few test cases, e.g.,
  // VideoSendStreamTest.VideoSendStreamStopSetEncoderRateToZero, set
  // internal_source to true and use FakeEncoder. And the latter will
  // happily encode this 1x1 frame and pass it on down the pipeline.
  return encoder_->Encode(VideoFrame(I420Buffer::Create(1, 1),
                                     kVideoRotation_0, 0),
                          NULL, &frame_types);
  return 0;
}

bool VCMGenericEncoder::InternalSource() const {
  return internal_source_;
}

bool VCMGenericEncoder::SupportsNativeHandle() const {
  RTC_DCHECK_RUNS_SERIALIZED(&race_checker_);
  return encoder_->SupportsNativeHandle();
}

VCMEncodedFrameCallback::VCMEncodedFrameCallback(
    EncodedImageCallback* post_encode_callback,
    media_optimization::MediaOptimization* media_opt)
    : internal_source_(false),
      post_encode_callback_(post_encode_callback),
      media_opt_(media_opt),
      framerate_(1),
      last_timing_frame_time_ms_(-1),
      timing_frames_thresholds_({-1, 0}) {
  rtc::Optional<AlrDetector::AlrExperimentSettings> experiment_settings =
      AlrDetector::ParseAlrSettingsFromFieldTrial(
          AlrDetector::kStrictPacingAndProbingExperimentName);
  if (experiment_settings) {
    experiment_groups_[0] = experiment_settings->group_id + 1;
  } else {
    experiment_groups_[0] = 0;
  }
  experiment_settings = AlrDetector::ParseAlrSettingsFromFieldTrial(
      AlrDetector::kScreenshareProbingBweExperimentName);
  if (experiment_settings) {
    experiment_groups_[1] = experiment_settings->group_id + 1;
  } else {
    experiment_groups_[1] = 0;
  }
}

VCMEncodedFrameCallback::~VCMEncodedFrameCallback() {}

void VCMEncodedFrameCallback::OnTargetBitrateChanged(
    size_t bitrate_bytes_per_second,
    size_t simulcast_svc_idx) {
  rtc::CritScope crit(&timing_params_lock_);
  if (timing_frames_info_.size() < simulcast_svc_idx + 1)
    timing_frames_info_.resize(simulcast_svc_idx + 1);
  timing_frames_info_[simulcast_svc_idx].target_bitrate_bytes_per_sec =
      bitrate_bytes_per_second;
}

void VCMEncodedFrameCallback::OnFrameRateChanged(size_t framerate) {
  rtc::CritScope crit(&timing_params_lock_);
  framerate_ = framerate;
}

void VCMEncodedFrameCallback::OnEncodeStarted(int64_t capture_time_ms,
                                              size_t simulcast_svc_idx) {
  rtc::CritScope crit(&timing_params_lock_);
  if (timing_frames_info_.size() < simulcast_svc_idx + 1)
    timing_frames_info_.resize(simulcast_svc_idx + 1);
  timing_frames_info_[simulcast_svc_idx].encode_start_time_ms[capture_time_ms] =
      rtc::TimeMillis();
}

EncodedImageCallback::Result VCMEncodedFrameCallback::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific,
    const RTPFragmentationHeader* fragmentation_header) {
  TRACE_EVENT_INSTANT1("webrtc", "VCMEncodedFrameCallback::Encoded",
                       "timestamp", encoded_image._timeStamp);
  size_t simulcast_svc_idx = 0;
  if (codec_specific->codecType == kVideoCodecVP9) {
    if (codec_specific->codecSpecific.VP9.num_spatial_layers > 1)
      simulcast_svc_idx = codec_specific->codecSpecific.VP9.spatial_idx;
  } else if (codec_specific->codecType == kVideoCodecVP8) {
    simulcast_svc_idx = codec_specific->codecSpecific.VP8.simulcastIdx;
  } else if (codec_specific->codecType == kVideoCodecGeneric) {
    simulcast_svc_idx = codec_specific->codecSpecific.generic.simulcast_idx;
  } else if (codec_specific->codecType == kVideoCodecH264) {
    // TODO(ilnik): When h264 simulcast is landed, extract simulcast idx here.
  }

  rtc::Optional<size_t> outlier_frame_size;
  rtc::Optional<int64_t> encode_start_ms;
  size_t num_simulcast_svc_streams = 1;
  uint8_t timing_flags = TimingFrameFlags::kInvalid;
  {
    rtc::CritScope crit(&timing_params_lock_);

    // Encoders with internal sources do not call OnEncodeStarted and
    // OnFrameRateChanged. |timing_frames_info_| may be not filled here.
    num_simulcast_svc_streams = timing_frames_info_.size();
    if (simulcast_svc_idx < num_simulcast_svc_streams) {
      auto encode_start_map =
          &timing_frames_info_[simulcast_svc_idx].encode_start_time_ms;
      auto it = encode_start_map->find(encoded_image.capture_time_ms_);
      if (it != encode_start_map->end()) {
        encode_start_ms.emplace(it->second);
        // Assuming all encoders do not reorder frames within single stream,
        // there may be some dropped frames with smaller timestamps. These
        // should be purged.
        encode_start_map->erase(encode_start_map->begin(), it);
        encode_start_map->erase(it);
      } else {
        // Encoder is with internal source: free our records of any frames just
        // in case to free memory.
        encode_start_map->clear();
      }

      size_t target_bitrate =
          timing_frames_info_[simulcast_svc_idx].target_bitrate_bytes_per_sec;
      if (framerate_ > 0 && target_bitrate > 0) {
        // framerate and target bitrate were reported by encoder.
        size_t average_frame_size = target_bitrate / framerate_;
        outlier_frame_size.emplace(
            average_frame_size *
            timing_frames_thresholds_.outlier_ratio_percent / 100);
      }
    }

    // Check if it's time to send a timing frame.
    int64_t timing_frame_delay_ms =
        encoded_image.capture_time_ms_ - last_timing_frame_time_ms_;
    // Trigger threshold if it's a first frame, too long passed since the last
    // timing frame, or we already sent timing frame on a different simulcast
    // stream with the same capture time.
    if (last_timing_frame_time_ms_ == -1 ||
        timing_frame_delay_ms >= timing_frames_thresholds_.delay_ms ||
        timing_frame_delay_ms == 0) {
      timing_flags = TimingFrameFlags::kTriggeredByTimer;
      last_timing_frame_time_ms_ = encoded_image.capture_time_ms_;
    }

    // Outliers trigger timing frames, but do not affect scheduled timing
    // frames.
    if (outlier_frame_size && encoded_image._length >= *outlier_frame_size) {
      if (timing_flags == TimingFrameFlags::kInvalid)
        timing_flags = 0;
      timing_flags |= TimingFrameFlags::kTriggeredBySize;
    }
  }

  // If encode start is not available that means that encoder uses internal
  // source. In that case capture timestamp may be from a different clock with a
  // drift relative to rtc::TimeMillis(). We can't use it for Timing frames,
  // because to being sent in the network capture time required to be less than
  // all the other timestamps.
  if (timing_flags != TimingFrameFlags::kInvalid && encode_start_ms) {
    encoded_image.SetEncodeTime(*encode_start_ms, rtc::TimeMillis());
    encoded_image.timing_.flags = timing_flags;
  } else {
    encoded_image.timing_.flags = TimingFrameFlags::kInvalid;
  }

  // Piggyback ALR experiment group id and simulcast id into the content type.
  uint8_t experiment_id =
      experiment_groups_[videocontenttypehelpers::IsScreenshare(
          encoded_image.content_type_)];

  // TODO(ilnik): This will force content type extension to be present even
  // for realtime video. At the expense of miniscule overhead we will get
  // sliced receive statistics.
  RTC_CHECK(videocontenttypehelpers::SetExperimentId(
      &encoded_image.content_type_, experiment_id));
  // We count simulcast streams from 1 on the wire. That's why we set simulcast
  // id in content type to +1 of that is actual simulcast index. This is because
  // value 0 on the wire is reserved for 'no simulcast stream specified'.
  RTC_CHECK(videocontenttypehelpers::SetSimulcastId(
      &encoded_image.content_type_,
      static_cast<uint8_t>(simulcast_svc_idx + 1)));

  Result result = post_encode_callback_->OnEncodedImage(
      encoded_image, codec_specific, fragmentation_header);
  if (result.error != Result::OK)
    return result;

  if (media_opt_) {
    media_opt_->UpdateWithEncodedData(encoded_image);
    if (internal_source_) {
      // Signal to encoder to drop next frame.
      result.drop_next_frame = media_opt_->DropFrame();
    }
  }
  return result;
}

}  // namespace webrtc
