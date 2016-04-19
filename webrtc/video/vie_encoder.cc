/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/vie_encoder.h"

#include <assert.h>

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/common_video/include/frame_callback.h"
#include "webrtc/common_video/include/video_image.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/modules/video_coding/include/video_codec_interface.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/system_wrappers/include/clock.h"
#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/video/overuse_frame_detector.h"
#include "webrtc/video/payload_router.h"
#include "webrtc/video/send_statistics_proxy.h"
#include "webrtc/video_frame.h"

namespace webrtc {

static const float kStopPaddingThresholdMs = 2000;
static const int kMinKeyFrameRequestIntervalMs = 300;

std::vector<uint32_t> AllocateStreamBitrates(
    uint32_t total_bitrate,
    const SimulcastStream* stream_configs,
    size_t number_of_streams) {
  if (number_of_streams == 0) {
    std::vector<uint32_t> stream_bitrates(1, 0);
    stream_bitrates[0] = total_bitrate;
    return stream_bitrates;
  }
  std::vector<uint32_t> stream_bitrates(number_of_streams, 0);
  uint32_t bitrate_remainder = total_bitrate;
  for (size_t i = 0; i < stream_bitrates.size() && bitrate_remainder > 0; ++i) {
    if (stream_configs[i].maxBitrate * 1000 > bitrate_remainder) {
      stream_bitrates[i] = bitrate_remainder;
    } else {
      stream_bitrates[i] = stream_configs[i].maxBitrate * 1000;
    }
    bitrate_remainder -= stream_bitrates[i];
  }
  return stream_bitrates;
}

class QMVideoSettingsCallback : public VCMQMSettingsCallback {
 public:
  explicit QMVideoSettingsCallback(VideoProcessing* vpm);

  ~QMVideoSettingsCallback();

  // Update VPM with QM (quality modes: frame size & frame rate) settings.
  int32_t SetVideoQMSettings(const uint32_t frame_rate,
                             const uint32_t width,
                             const uint32_t height);

  // Update target frame rate.
  void SetTargetFramerate(int frame_rate);

 private:
  VideoProcessing* vp_;
};

ViEEncoder::ViEEncoder(uint32_t number_of_cores,
                       const std::vector<uint32_t>& ssrcs,
                       ProcessThread* module_process_thread,
                       SendStatisticsProxy* stats_proxy,
                       rtc::VideoSinkInterface<VideoFrame>* pre_encode_callback,
                       OveruseFrameDetector* overuse_detector,
                       PacedSender* pacer,
                       PayloadRouter* payload_router)
    : number_of_cores_(number_of_cores),
      ssrcs_(ssrcs),
      vp_(VideoProcessing::Create()),
      qm_callback_(new QMVideoSettingsCallback(vp_.get())),
      vcm_(VideoCodingModule::Create(Clock::GetRealTimeClock(),
                                     this,
                                     qm_callback_.get())),
      stats_proxy_(stats_proxy),
      pre_encode_callback_(pre_encode_callback),
      overuse_detector_(overuse_detector),
      pacer_(pacer),
      send_payload_router_(payload_router),
      time_of_last_frame_activity_ms_(0),
      encoder_config_(),
      min_transmit_bitrate_bps_(0),
      last_observed_bitrate_bps_(0),
      network_is_transmitting_(true),
      encoder_paused_(false),
      encoder_paused_and_dropped_frame_(false),
      time_last_intra_request_ms_(ssrcs.size(), -1),
      module_process_thread_(module_process_thread),
      has_received_sli_(false),
      picture_id_sli_(0),
      has_received_rpsi_(false),
      picture_id_rpsi_(0),
      video_suspended_(false) {
  module_process_thread_->RegisterModule(vcm_.get());
}

bool ViEEncoder::Init() {
  vp_->EnableTemporalDecimation(true);

  // Enable/disable content analysis: off by default for now.
  vp_->EnableContentAnalysis(false);

  if (vcm_->RegisterTransportCallback(this) != 0) {
    return false;
  }
  if (vcm_->RegisterSendStatisticsCallback(this) != 0) {
    return false;
  }
  return true;
}

VideoCodingModule* ViEEncoder::vcm() const {
  return vcm_.get();
}

ViEEncoder::~ViEEncoder() {
  module_process_thread_->DeRegisterModule(vcm_.get());
}

void ViEEncoder::SetNetworkTransmissionState(bool is_transmitting) {
  {
    rtc::CritScope lock(&data_cs_);
    network_is_transmitting_ = is_transmitting;
  }
}

void ViEEncoder::Pause() {
  rtc::CritScope lock(&data_cs_);
  encoder_paused_ = true;
}

void ViEEncoder::Restart() {
  rtc::CritScope lock(&data_cs_);
  encoder_paused_ = false;
}

int32_t ViEEncoder::RegisterExternalEncoder(webrtc::VideoEncoder* encoder,
                                            uint8_t pl_type,
                                            bool internal_source) {
  if (vcm_->RegisterExternalEncoder(encoder, pl_type, internal_source) !=
      VCM_OK) {
    return -1;
  }
  return 0;
}

int32_t ViEEncoder::DeRegisterExternalEncoder(uint8_t pl_type) {
  if (vcm_->RegisterExternalEncoder(nullptr, pl_type) != VCM_OK) {
    return -1;
  }
  return 0;
}
void ViEEncoder::SetEncoder(const webrtc::VideoCodec& video_codec,
                            int min_transmit_bitrate_bps) {
  RTC_DCHECK(send_payload_router_);
  // Setting target width and height for VPM.
  RTC_CHECK_EQ(VPM_OK,
               vp_->SetTargetResolution(video_codec.width, video_codec.height,
                                        video_codec.maxFramerate));

  // Cache codec before calling AddBitrateObserver (which calls OnBitrateUpdated
  // that makes use of the number of simulcast streams configured).
  {
    rtc::CritScope lock(&data_cs_);
    encoder_config_ = video_codec;
    encoder_paused_ = true;
    min_transmit_bitrate_bps_ = min_transmit_bitrate_bps;
  }

  size_t max_data_payload_length = send_payload_router_->MaxPayloadLength();
  bool success = vcm_->RegisterSendCodec(
                     &video_codec, number_of_cores_,
                     static_cast<uint32_t>(max_data_payload_length)) == VCM_OK;
  if (!success) {
    LOG(LS_ERROR) << "Failed to configure encoder.";
    RTC_DCHECK(success);
  }

  send_payload_router_->SetSendingRtpModules(
      video_codec.numberOfSimulcastStreams);

  // Restart the media flow
  Restart();
  if (stats_proxy_) {
    // Clear stats for disabled layers.
    for (size_t i = video_codec.numberOfSimulcastStreams; i < ssrcs_.size();
         ++i) {
      stats_proxy_->OnInactiveSsrc(ssrcs_[i]);
    }
    VideoEncoderConfig::ContentType content_type =
        VideoEncoderConfig::ContentType::kRealtimeVideo;
    switch (video_codec.mode) {
      case kRealtimeVideo:
        content_type = VideoEncoderConfig::ContentType::kRealtimeVideo;
        break;
      case kScreensharing:
        content_type = VideoEncoderConfig::ContentType::kScreen;
        break;
      default:
        RTC_NOTREACHED();
        break;
    }
    stats_proxy_->SetContentType(content_type);
  }
}

int ViEEncoder::GetPaddingNeededBps() const {
  int64_t time_of_last_frame_activity_ms;
  int min_transmit_bitrate_bps;
  int bitrate_bps;
  VideoCodec send_codec;
  {
    rtc::CritScope lock(&data_cs_);
    bool send_padding = encoder_config_.numberOfSimulcastStreams > 1 ||
                        video_suspended_ || min_transmit_bitrate_bps_ > 0;
    if (!send_padding)
      return 0;
    time_of_last_frame_activity_ms = time_of_last_frame_activity_ms_;
    min_transmit_bitrate_bps = min_transmit_bitrate_bps_;
    bitrate_bps = last_observed_bitrate_bps_;
    send_codec = encoder_config_;
  }

  bool video_is_suspended = vcm_->VideoSuspended();

  // Find the max amount of padding we can allow ourselves to send at this
  // point, based on which streams are currently active and what our current
  // available bandwidth is.
  int pad_up_to_bitrate_bps = 0;
  if (send_codec.numberOfSimulcastStreams == 0) {
    pad_up_to_bitrate_bps = send_codec.minBitrate * 1000;
  } else {
    SimulcastStream* stream_configs = send_codec.simulcastStream;
    pad_up_to_bitrate_bps =
        stream_configs[send_codec.numberOfSimulcastStreams - 1].minBitrate *
        1000;
    for (int i = 0; i < send_codec.numberOfSimulcastStreams - 1; ++i) {
      pad_up_to_bitrate_bps += stream_configs[i].targetBitrate * 1000;
    }
  }

  // Disable padding if only sending one stream and video isn't suspended and
  // min-transmit bitrate isn't used (applied later).
  if (!video_is_suspended && send_codec.numberOfSimulcastStreams <= 1)
    pad_up_to_bitrate_bps = 0;

  // The amount of padding should decay to zero if no frames are being
  // captured/encoded unless a min-transmit bitrate is used.
  int64_t now_ms = TickTime::MillisecondTimestamp();
  if (now_ms - time_of_last_frame_activity_ms > kStopPaddingThresholdMs)
    pad_up_to_bitrate_bps = 0;

  // Pad up to min bitrate.
  if (pad_up_to_bitrate_bps < min_transmit_bitrate_bps)
    pad_up_to_bitrate_bps = min_transmit_bitrate_bps;

  // Padding may never exceed bitrate estimate.
  if (pad_up_to_bitrate_bps > bitrate_bps)
    pad_up_to_bitrate_bps = bitrate_bps;

  return pad_up_to_bitrate_bps;
}

bool ViEEncoder::EncoderPaused() const {
  // Pause video if paused by caller or as long as the network is down or the
  // pacer queue has grown too large in buffered mode.
  if (encoder_paused_) {
    return true;
  }
  if (pacer_->ExpectedQueueTimeMs() > PacedSender::kMaxQueueLengthMs) {
    // Too much data in pacer queue, drop frame.
    return true;
  }
  return !network_is_transmitting_;
}

void ViEEncoder::TraceFrameDropStart() {
  // Start trace event only on the first frame after encoder is paused.
  if (!encoder_paused_and_dropped_frame_) {
    TRACE_EVENT_ASYNC_BEGIN0("webrtc", "EncoderPaused", this);
  }
  encoder_paused_and_dropped_frame_ = true;
  return;
}

void ViEEncoder::TraceFrameDropEnd() {
  // End trace event on first frame after encoder resumes, if frame was dropped.
  if (encoder_paused_and_dropped_frame_) {
    TRACE_EVENT_ASYNC_END0("webrtc", "EncoderPaused", this);
  }
  encoder_paused_and_dropped_frame_ = false;
}

void ViEEncoder::EncodeVideoFrame(const VideoFrame& video_frame) {
  if (!send_payload_router_->active()) {
    // We've paused or we have no channels attached, don't waste resources on
    // encoding.
    return;
  }
  VideoCodecType codec_type;
  {
    rtc::CritScope lock(&data_cs_);
    time_of_last_frame_activity_ms_ = TickTime::MillisecondTimestamp();
    if (EncoderPaused()) {
      TraceFrameDropStart();
      return;
    }
    TraceFrameDropEnd();
    codec_type = encoder_config_.codecType;
  }

  TRACE_EVENT_ASYNC_STEP0("webrtc", "Video", video_frame.render_time_ms(),
                          "Encode");
  const VideoFrame* frame_to_send = &video_frame;
  // TODO(wuchengli): support texture frames.
  if (!video_frame.video_frame_buffer()->native_handle()) {
    // Pass frame via preprocessor.
    frame_to_send = vp_->PreprocessFrame(video_frame);
    if (!frame_to_send) {
      // Drop this frame, or there was an error processing it.
      return;
    }
  }

  if (pre_encode_callback_) {
    pre_encode_callback_->OnFrame(*frame_to_send);
  }

  if (codec_type == webrtc::kVideoCodecVP8) {
    webrtc::CodecSpecificInfo codec_specific_info;
    codec_specific_info.codecType = webrtc::kVideoCodecVP8;
    {
      rtc::CritScope lock(&data_cs_);
      codec_specific_info.codecSpecific.VP8.hasReceivedRPSI =
          has_received_rpsi_;
      codec_specific_info.codecSpecific.VP8.hasReceivedSLI =
          has_received_sli_;
      codec_specific_info.codecSpecific.VP8.pictureIdRPSI =
          picture_id_rpsi_;
      codec_specific_info.codecSpecific.VP8.pictureIdSLI  =
          picture_id_sli_;
      has_received_sli_ = false;
      has_received_rpsi_ = false;
    }

    vcm_->AddVideoFrame(*frame_to_send, vp_->GetContentMetrics(),
                        &codec_specific_info);
    return;
  }
  vcm_->AddVideoFrame(*frame_to_send);
}

void ViEEncoder::SendKeyFrame() {
  vcm_->IntraFrameRequest(0);
}

uint32_t ViEEncoder::LastObservedBitrateBps() const {
  rtc::CritScope lock(&data_cs_);
  return last_observed_bitrate_bps_;
}

int ViEEncoder::CodecTargetBitrate(uint32_t* bitrate) const {
  if (vcm_->Bitrate(bitrate) != 0)
    return -1;
  return 0;
}

void ViEEncoder::SetProtectionMethod(bool nack, bool fec) {
  // Set Video Protection for VCM.
  VCMVideoProtection protection_mode;
  if (fec) {
    protection_mode =
        nack ? webrtc::kProtectionNackFEC : kProtectionFEC;
  } else {
    protection_mode = nack ? kProtectionNack : kProtectionNone;
  }
  vcm_->SetVideoProtection(protection_mode, true);
}

void ViEEncoder::OnSetRates(uint32_t bitrate_bps, int framerate) {
  if (stats_proxy_)
    stats_proxy_->OnSetRates(bitrate_bps, framerate);
}

int32_t ViEEncoder::SendData(const uint8_t payload_type,
                             const EncodedImage& encoded_image,
                             const RTPFragmentationHeader* fragmentation_header,
                             const RTPVideoHeader* rtp_video_hdr) {
  RTC_DCHECK(send_payload_router_);

  {
    rtc::CritScope lock(&data_cs_);
    time_of_last_frame_activity_ms_ = TickTime::MillisecondTimestamp();
  }

  if (stats_proxy_)
    stats_proxy_->OnSendEncodedImage(encoded_image, rtp_video_hdr);

  bool success = send_payload_router_->RoutePayload(
      encoded_image._frameType, payload_type, encoded_image._timeStamp,
      encoded_image.capture_time_ms_, encoded_image._buffer,
      encoded_image._length, fragmentation_header, rtp_video_hdr);
  overuse_detector_->FrameSent(encoded_image._timeStamp);

  if (kEnableFrameRecording) {
    int layer = rtp_video_hdr->simulcastIdx;
    IvfFileWriter* file_writer;
    {
      rtc::CritScope lock(&data_cs_);
      if (file_writers_[layer] == nullptr) {
        std::ostringstream oss;
        oss << "send_bitstream_ssrc";
        for (uint32_t ssrc : ssrcs_)
          oss << "_" << ssrc;
        oss << "_layer" << layer << ".ivf";
        file_writers_[layer] =
            IvfFileWriter::Open(oss.str(), rtp_video_hdr->codec);
      }
      file_writer = file_writers_[layer].get();
    }
    if (file_writer) {
      bool ok = file_writer->WriteFrame(encoded_image);
      RTC_DCHECK(ok);
    }
  }

  return success ? 0 : -1;
}

void ViEEncoder::OnEncoderImplementationName(
    const char* implementation_name) {
  if (stats_proxy_)
    stats_proxy_->OnEncoderImplementationName(implementation_name);
}

int32_t ViEEncoder::SendStatistics(const uint32_t bit_rate,
                                   const uint32_t frame_rate) {
  if (stats_proxy_)
    stats_proxy_->OnOutgoingRate(frame_rate, bit_rate);
  return 0;
}

void ViEEncoder::OnReceivedSLI(uint32_t /*ssrc*/,
                               uint8_t picture_id) {
  rtc::CritScope lock(&data_cs_);
  picture_id_sli_ = picture_id;
  has_received_sli_ = true;
}

void ViEEncoder::OnReceivedRPSI(uint32_t /*ssrc*/,
                                uint64_t picture_id) {
  rtc::CritScope lock(&data_cs_);
  picture_id_rpsi_ = picture_id;
  has_received_rpsi_ = true;
}

void ViEEncoder::OnReceivedIntraFrameRequest(uint32_t ssrc) {
  // Key frame request from remote side, signal to VCM.
  TRACE_EVENT0("webrtc", "OnKeyFrameRequest");

  for (size_t i = 0; i < ssrcs_.size(); ++i) {
    if (ssrcs_[i] != ssrc)
      continue;
    int64_t now_ms = TickTime::MillisecondTimestamp();
    {
      rtc::CritScope lock(&data_cs_);
      if (time_last_intra_request_ms_[i] + kMinKeyFrameRequestIntervalMs >
          now_ms) {
        return;
      }
      time_last_intra_request_ms_[i] = now_ms;
    }
    vcm_->IntraFrameRequest(static_cast<int>(i));
    return;
  }
  RTC_NOTREACHED() << "Should not receive keyframe requests on unknown SSRCs.";
}

void ViEEncoder::OnBitrateUpdated(uint32_t bitrate_bps,
                                  uint8_t fraction_lost,
                                  int64_t round_trip_time_ms) {
  LOG(LS_VERBOSE) << "OnBitrateUpdated, bitrate" << bitrate_bps
                  << " packet loss " << static_cast<int>(fraction_lost)
                  << " rtt " << round_trip_time_ms;
  RTC_DCHECK(send_payload_router_);
  vcm_->SetChannelParameters(bitrate_bps, fraction_lost, round_trip_time_ms);
  bool video_is_suspended = vcm_->VideoSuspended();
  bool video_suspension_changed;
  VideoCodec send_codec;
  {
    rtc::CritScope lock(&data_cs_);
    last_observed_bitrate_bps_ = bitrate_bps;
    video_suspension_changed = video_suspended_ != video_is_suspended;
    video_suspended_ = video_is_suspended;
    send_codec = encoder_config_;
  }

  SimulcastStream* stream_configs = send_codec.simulcastStream;
  // Allocate the bandwidth between the streams.
  std::vector<uint32_t> stream_bitrates = AllocateStreamBitrates(
      bitrate_bps, stream_configs, send_codec.numberOfSimulcastStreams);
  send_payload_router_->SetTargetSendBitrates(stream_bitrates);

  if (!video_suspension_changed)
    return;
  // Video suspend-state changed, inform codec observer.
  LOG(LS_INFO) << "Video suspend state changed " << video_is_suspended
               << " for ssrc " << ssrcs_[0];
  if (stats_proxy_)
    stats_proxy_->OnSuspendChange(video_is_suspended);
}

void ViEEncoder::RegisterPostEncodeImageCallback(
      EncodedImageCallback* post_encode_callback) {
  vcm_->RegisterPostEncodeImageCallback(post_encode_callback);
}

QMVideoSettingsCallback::QMVideoSettingsCallback(VideoProcessing* vpm)
    : vp_(vpm) {
}

QMVideoSettingsCallback::~QMVideoSettingsCallback() {
}

int32_t QMVideoSettingsCallback::SetVideoQMSettings(
    const uint32_t frame_rate,
    const uint32_t width,
    const uint32_t height) {
  return vp_->SetTargetResolution(width, height, frame_rate);
}

void QMVideoSettingsCallback::SetTargetFramerate(int frame_rate) {
  vp_->SetTargetFramerate(frame_rate);
}

}  // namespace webrtc
