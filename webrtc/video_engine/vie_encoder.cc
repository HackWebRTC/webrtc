/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_encoder.h"

#include <assert.h>

#include <algorithm>

#include "webrtc/base/checks.h"
#include "webrtc/common_video/interface/video_image.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/pacing/include/paced_sender.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/modules/video_coding/main/interface/video_coding_defines.h"
#include "webrtc/modules/video_coding/main/source/encoded_frame.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/metrics.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/system_wrappers/interface/trace_event.h"
#include "webrtc/video/send_statistics_proxy.h"
#include "webrtc/video_engine/payload_router.h"
#include "webrtc/video_engine/vie_defines.h"

namespace webrtc {

// Margin on when we pause the encoder when the pacing buffer overflows relative
// to the configured buffer delay.
static const float kEncoderPausePacerMargin = 2.0f;

// Don't stop the encoder unless the delay is above this configured value.
static const int kMinPacingDelayMs = 200;

static const float kStopPaddingThresholdMs = 2000;

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
  explicit QMVideoSettingsCallback(VideoProcessingModule* vpm);

  ~QMVideoSettingsCallback();

  // Update VPM with QM (quality modes: frame size & frame rate) settings.
  int32_t SetVideoQMSettings(const uint32_t frame_rate,
                             const uint32_t width,
                             const uint32_t height);

  // Update target frame rate.
  void SetTargetFramerate(int frame_rate);

 private:
  VideoProcessingModule* vpm_;
};

class ViEBitrateObserver : public BitrateObserver {
 public:
  explicit ViEBitrateObserver(ViEEncoder* owner)
      : owner_(owner) {
  }
  virtual ~ViEBitrateObserver() {}
  // Implements BitrateObserver.
  virtual void OnNetworkChanged(uint32_t bitrate_bps,
                                uint8_t fraction_lost,
                                int64_t rtt) {
    owner_->OnNetworkChanged(bitrate_bps, fraction_lost, rtt);
  }
 private:
  ViEEncoder* owner_;
};

ViEEncoder::ViEEncoder(int32_t channel_id,
                       uint32_t number_of_cores,
                       ProcessThread* module_process_thread,
                       SendStatisticsProxy* stats_proxy,
                       I420FrameCallback* pre_encode_callback,
                       PacedSender* pacer,
                       BitrateAllocator* bitrate_allocator)
    : channel_id_(channel_id),
      number_of_cores_(number_of_cores),
      vpm_(VideoProcessingModule::Create()),
      qm_callback_(new QMVideoSettingsCallback(vpm_.get())),
      vcm_(VideoCodingModule::Create(Clock::GetRealTimeClock(),
                                     this,
                                     qm_callback_.get())),
      send_payload_router_(NULL),
      data_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      stats_proxy_(stats_proxy),
      pre_encode_callback_(pre_encode_callback),
      pacer_(pacer),
      bitrate_allocator_(bitrate_allocator),
      time_of_last_frame_activity_ms_(0),
      simulcast_enabled_(false),
      min_transmit_bitrate_kbps_(0),
      last_observed_bitrate_bps_(0),
      target_delay_ms_(0),
      network_is_transmitting_(true),
      encoder_paused_(false),
      encoder_paused_and_dropped_frame_(false),
      fec_enabled_(false),
      nack_enabled_(false),
      module_process_thread_(module_process_thread),
      has_received_sli_(false),
      picture_id_sli_(0),
      has_received_rpsi_(false),
      picture_id_rpsi_(0),
      video_suspended_(false),
      start_ms_(Clock::GetRealTimeClock()->TimeInMilliseconds()) {
  bitrate_observer_.reset(new ViEBitrateObserver(this));
}

bool ViEEncoder::Init() {
  vpm_->EnableTemporalDecimation(true);

  // Enable/disable content analysis: off by default for now.
  vpm_->EnableContentAnalysis(false);

  if (vcm_->RegisterTransportCallback(this) != 0) {
    return false;
  }
  if (vcm_->RegisterSendStatisticsCallback(this) != 0) {
    return false;
  }
  return true;
}

void ViEEncoder::StartThreadsAndSetSharedMembers(
    rtc::scoped_refptr<PayloadRouter> send_payload_router,
    VCMProtectionCallback* vcm_protection_callback) {
  RTC_DCHECK(send_payload_router_ == NULL);

  send_payload_router_ = send_payload_router;
  vcm_->RegisterProtectionCallback(vcm_protection_callback);
  module_process_thread_->RegisterModule(vcm_.get());
}

void ViEEncoder::StopThreadsAndRemoveSharedMembers() {
  if (bitrate_allocator_)
    bitrate_allocator_->RemoveBitrateObserver(bitrate_observer_.get());
  module_process_thread_->DeRegisterModule(vcm_.get());
  module_process_thread_->DeRegisterModule(vpm_.get());
}

ViEEncoder::~ViEEncoder() {
  UpdateHistograms();
}

void ViEEncoder::UpdateHistograms() {
  int64_t elapsed_sec =
      (Clock::GetRealTimeClock()->TimeInMilliseconds() - start_ms_) / 1000;
  if (elapsed_sec < metrics::kMinRunTimeInSeconds) {
    return;
  }
  webrtc::VCMFrameCount frames;
  if (vcm_->SentFrameCount(frames) != VCM_OK) {
    return;
  }
  uint32_t total_frames = frames.numKeyFrames + frames.numDeltaFrames;
  if (total_frames > 0) {
    RTC_HISTOGRAM_COUNTS_1000("WebRTC.Video.KeyFramesSentInPermille",
        static_cast<int>(
            (frames.numKeyFrames * 1000.0f / total_frames) + 0.5f));
  }
}

int ViEEncoder::Owner() const {
  return channel_id_;
}

void ViEEncoder::SetNetworkTransmissionState(bool is_transmitting) {
  {
    CriticalSectionScoped cs(data_cs_.get());
    network_is_transmitting_ = is_transmitting;
  }
  if (is_transmitting) {
    pacer_->Resume();
  } else {
    pacer_->Pause();
  }
}

void ViEEncoder::Pause() {
  CriticalSectionScoped cs(data_cs_.get());
  encoder_paused_ = true;
}

void ViEEncoder::Restart() {
  CriticalSectionScoped cs(data_cs_.get());
  encoder_paused_ = false;
}

uint8_t ViEEncoder::NumberOfCodecs() {
  return vcm_->NumberOfCodecs();
}

int32_t ViEEncoder::GetCodec(uint8_t list_index, VideoCodec* video_codec) {
  if (vcm_->Codec(list_index, video_codec) != 0) {
    return -1;
  }
  return 0;
}

int32_t ViEEncoder::RegisterExternalEncoder(webrtc::VideoEncoder* encoder,
                                            uint8_t pl_type,
                                            bool internal_source) {
  if (encoder == NULL)
    return -1;

  if (vcm_->RegisterExternalEncoder(encoder, pl_type, internal_source) !=
      VCM_OK) {
    return -1;
  }
  return 0;
}

int32_t ViEEncoder::DeRegisterExternalEncoder(uint8_t pl_type) {
  if (vcm_->RegisterExternalEncoder(NULL, pl_type) != VCM_OK) {
    return -1;
  }
  return 0;
}

int32_t ViEEncoder::SetEncoder(const webrtc::VideoCodec& video_codec) {
  RTC_DCHECK(send_payload_router_ != NULL);
  // Setting target width and height for VPM.
  if (vpm_->SetTargetResolution(video_codec.width, video_codec.height,
                                video_codec.maxFramerate) != VPM_OK) {
    return -1;
  }

  {
    CriticalSectionScoped cs(data_cs_.get());
    simulcast_enabled_ = video_codec.numberOfSimulcastStreams > 1;
  }

  // Add a bitrate observer to the allocator and update the start, max and
  // min bitrates of the bitrate controller as needed.
  int allocated_bitrate_bps = bitrate_allocator_->AddBitrateObserver(
      bitrate_observer_.get(), video_codec.minBitrate * 1000,
      video_codec.maxBitrate * 1000);

  webrtc::VideoCodec modified_video_codec = video_codec;
  modified_video_codec.startBitrate = allocated_bitrate_bps / 1000;

  size_t max_data_payload_length = send_payload_router_->MaxPayloadLength();
  if (vcm_->RegisterSendCodec(&modified_video_codec, number_of_cores_,
                              static_cast<uint32_t>(max_data_payload_length)) !=
      VCM_OK) {
    return -1;
  }
  return 0;
}

int32_t ViEEncoder::GetEncoder(VideoCodec* video_codec) {
  *video_codec = vcm_->GetSendCodec();
  return 0;
}

int32_t ViEEncoder::GetCodecConfigParameters(
    unsigned char config_parameters[kConfigParameterSize],
    unsigned char& config_parameters_size) {
  int32_t num_parameters =
      vcm_->CodecConfigParameters(config_parameters, kConfigParameterSize);
  if (num_parameters <= 0) {
    config_parameters_size = 0;
    return -1;
  }
  config_parameters_size = static_cast<unsigned char>(num_parameters);
  return 0;
}

int32_t ViEEncoder::ScaleInputImage(bool enable) {
  VideoFrameResampling resampling_mode = kFastRescaling;
  // TODO(mflodman) What?
  if (enable) {
    // kInterpolation is currently not supported.
    LOG_F(LS_ERROR) << "Not supported.";
    return -1;
  }
  vpm_->SetInputFrameResampleMode(resampling_mode);

  return 0;
}

int ViEEncoder::GetPaddingNeededBps() const {
  int64_t time_of_last_frame_activity_ms;
  int min_transmit_bitrate_bps;
  int bitrate_bps;
  {
    CriticalSectionScoped cs(data_cs_.get());
    bool send_padding = simulcast_enabled_ || video_suspended_ ||
                        min_transmit_bitrate_kbps_ > 0;
    if (!send_padding)
      return 0;
    time_of_last_frame_activity_ms = time_of_last_frame_activity_ms_;
    min_transmit_bitrate_bps = 1000 * min_transmit_bitrate_kbps_;
    bitrate_bps = last_observed_bitrate_bps_;
  }

  VideoCodec send_codec;
  if (vcm_->SendCodec(&send_codec) != 0)
    return 0;

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
  if (target_delay_ms_ > 0) {
    // Buffered mode.
    // TODO(pwestin): Workaround until nack is configured as a time and not
    // number of packets.
    return pacer_->QueueInMs() >=
           std::max(
               static_cast<int>(target_delay_ms_ * kEncoderPausePacerMargin),
               kMinPacingDelayMs);
  }
  if (pacer_->ExpectedQueueTimeMs() > PacedSender::kDefaultMaxQueueLengthMs) {
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

void ViEEncoder::DeliverFrame(VideoFrame video_frame) {
  RTC_DCHECK(send_payload_router_ != NULL);
  if (!send_payload_router_->active()) {
    // We've paused or we have no channels attached, don't waste resources on
    // encoding.
    return;
  }
  {
    CriticalSectionScoped cs(data_cs_.get());
    time_of_last_frame_activity_ms_ = TickTime::MillisecondTimestamp();
    if (EncoderPaused()) {
      TraceFrameDropStart();
      return;
    }
    TraceFrameDropEnd();
  }

  TRACE_EVENT_ASYNC_STEP0("webrtc", "Video", video_frame.render_time_ms(),
                          "Encode");
  VideoFrame* decimated_frame = NULL;
  // TODO(wuchengli): support texture frames.
  if (video_frame.native_handle() == NULL) {
    // Pass frame via preprocessor.
    const int ret = vpm_->PreprocessFrame(video_frame, &decimated_frame);
    if (ret == 1) {
      // Drop this frame.
      return;
    }
    if (ret != VPM_OK) {
      return;
    }
  }

  // If we haven't resampled the frame and we have a FrameCallback, we need to
  // make a deep copy of |video_frame|.
  VideoFrame copied_frame;
  if (pre_encode_callback_) {
    // If the frame was not resampled or scaled => use copy of original.
    if (decimated_frame == NULL) {
      copied_frame.CopyFrame(video_frame);
      decimated_frame = &copied_frame;
    }
    pre_encode_callback_->FrameCallback(decimated_frame);
  }

  // If the frame was not resampled, scaled, or touched by FrameCallback => use
  // original. The frame is const from here.
  const VideoFrame* output_frame =
      (decimated_frame != NULL) ? decimated_frame : &video_frame;

#ifdef VIDEOCODEC_VP8
  if (vcm_->SendCodec() == webrtc::kVideoCodecVP8) {
    webrtc::CodecSpecificInfo codec_specific_info;
    codec_specific_info.codecType = webrtc::kVideoCodecVP8;
    {
      CriticalSectionScoped cs(data_cs_.get());
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

    vcm_->AddVideoFrame(*output_frame, vpm_->ContentMetrics(),
                        &codec_specific_info);
    return;
  }
#endif
  vcm_->AddVideoFrame(*output_frame);
}

int ViEEncoder::SendKeyFrame() {
  return vcm_->IntraFrameRequest(0);
}

int32_t ViEEncoder::SendCodecStatistics(
    uint32_t* num_key_frames, uint32_t* num_delta_frames) {
  webrtc::VCMFrameCount sent_frames;
  if (vcm_->SentFrameCount(sent_frames) != VCM_OK) {
    return -1;
  }
  *num_key_frames = sent_frames.numKeyFrames;
  *num_delta_frames = sent_frames.numDeltaFrames;
  return 0;
}

uint32_t ViEEncoder::LastObservedBitrateBps() const {
  CriticalSectionScoped cs(data_cs_.get());
  return last_observed_bitrate_bps_;
}

int ViEEncoder::CodecTargetBitrate(uint32_t* bitrate) const {
  if (vcm_->Bitrate(bitrate) != 0)
    return -1;
  return 0;
}

int32_t ViEEncoder::UpdateProtectionMethod(bool nack, bool fec) {
  RTC_DCHECK(send_payload_router_ != NULL);

  if (fec_enabled_ == fec && nack_enabled_ == nack) {
    // No change needed, we're already in correct state.
    return 0;
  }
  fec_enabled_ = fec;
  nack_enabled_ = nack;

  // Set Video Protection for VCM.
  VCMVideoProtection protection_mode;
  if (fec_enabled_) {
    protection_mode =
        nack_enabled_ ? webrtc::kProtectionNackFEC : kProtectionFEC;
  } else {
    protection_mode = nack_enabled_ ? kProtectionNack : kProtectionNone;
  }
  vcm_->SetVideoProtection(protection_mode, true);

  if (fec_enabled_ || nack_enabled_) {
    // The send codec must be registered to set correct MTU.
    webrtc::VideoCodec codec;
    if (vcm_->SendCodec(&codec) == 0) {
      uint32_t current_bitrate_bps = 0;
      if (vcm_->Bitrate(&current_bitrate_bps) != 0) {
        LOG_F(LS_WARNING) <<
            "Failed to get the current encoder target bitrate.";
      }
      // Convert to start bitrate in kbps.
      codec.startBitrate = (current_bitrate_bps + 500) / 1000;
      size_t max_payload_length = send_payload_router_->MaxPayloadLength();
      if (vcm_->RegisterSendCodec(&codec, number_of_cores_,
                                  static_cast<uint32_t>(max_payload_length)) !=
          0) {
        return -1;
      }
    }
  }
  return 0;
}

void ViEEncoder::SetSenderBufferingMode(int target_delay_ms) {
  {
    CriticalSectionScoped cs(data_cs_.get());
    target_delay_ms_ = target_delay_ms;
  }
  if (target_delay_ms > 0) {
    // Disable external frame-droppers.
    vcm_->EnableFrameDropper(false);
    vpm_->EnableTemporalDecimation(false);
  } else {
    // Real-time mode - enable frame droppers.
    vpm_->EnableTemporalDecimation(true);
    vcm_->EnableFrameDropper(true);
  }
}

void ViEEncoder::OnSetRates(uint32_t bitrate_bps, int framerate) {
  if (stats_proxy_)
    stats_proxy_->OnSetRates(bitrate_bps, framerate);
}

int32_t ViEEncoder::SendData(
    const uint8_t payload_type,
    const EncodedImage& encoded_image,
    const webrtc::RTPFragmentationHeader& fragmentation_header,
    const RTPVideoHeader* rtp_video_hdr) {
  RTC_DCHECK(send_payload_router_ != NULL);

  {
    CriticalSectionScoped cs(data_cs_.get());
    time_of_last_frame_activity_ms_ = TickTime::MillisecondTimestamp();
  }

  if (stats_proxy_ != NULL)
    stats_proxy_->OnSendEncodedImage(encoded_image, rtp_video_hdr);

  return send_payload_router_->RoutePayload(
      VCMEncodedFrame::ConvertFrameType(encoded_image._frameType), payload_type,
      encoded_image._timeStamp, encoded_image.capture_time_ms_,
      encoded_image._buffer, encoded_image._length, &fragmentation_header,
      rtp_video_hdr) ? 0 : -1;
}

int32_t ViEEncoder::SendStatistics(const uint32_t bit_rate,
                                   const uint32_t frame_rate) {
  if (stats_proxy_)
    stats_proxy_->OnOutgoingRate(frame_rate, bit_rate);
  return 0;
}

void ViEEncoder::OnReceivedSLI(uint32_t /*ssrc*/,
                               uint8_t picture_id) {
  CriticalSectionScoped cs(data_cs_.get());
  picture_id_sli_ = picture_id;
  has_received_sli_ = true;
}

void ViEEncoder::OnReceivedRPSI(uint32_t /*ssrc*/,
                                uint64_t picture_id) {
  CriticalSectionScoped cs(data_cs_.get());
  picture_id_rpsi_ = picture_id;
  has_received_rpsi_ = true;
}

void ViEEncoder::OnReceivedIntraFrameRequest(uint32_t ssrc) {
  // Key frame request from remote side, signal to VCM.
  TRACE_EVENT0("webrtc", "OnKeyFrameRequest");

  int idx = 0;
  {
    CriticalSectionScoped cs(data_cs_.get());
    auto stream_it = ssrc_streams_.find(ssrc);
    if (stream_it == ssrc_streams_.end()) {
      LOG_F(LS_WARNING) << "ssrc not found: " << ssrc << ", map size "
                        << ssrc_streams_.size();
      return;
    }
    std::map<unsigned int, int64_t>::iterator time_it =
        time_last_intra_request_ms_.find(ssrc);
    if (time_it == time_last_intra_request_ms_.end()) {
      time_last_intra_request_ms_[ssrc] = 0;
    }

    int64_t now = TickTime::MillisecondTimestamp();
    if (time_last_intra_request_ms_[ssrc] + kViEMinKeyRequestIntervalMs > now) {
      return;
    }
    time_last_intra_request_ms_[ssrc] = now;
    idx = stream_it->second;
  }
  // Release the critsect before triggering key frame.
  vcm_->IntraFrameRequest(idx);
}

void ViEEncoder::OnLocalSsrcChanged(uint32_t old_ssrc, uint32_t new_ssrc) {
  CriticalSectionScoped cs(data_cs_.get());
  std::map<unsigned int, int>::iterator it = ssrc_streams_.find(old_ssrc);
  if (it == ssrc_streams_.end()) {
    return;
  }

  ssrc_streams_[new_ssrc] = it->second;
  ssrc_streams_.erase(it);

  std::map<unsigned int, int64_t>::iterator time_it =
      time_last_intra_request_ms_.find(old_ssrc);
  int64_t last_intra_request_ms = 0;
  if (time_it != time_last_intra_request_ms_.end()) {
    last_intra_request_ms = time_it->second;
    time_last_intra_request_ms_.erase(time_it);
  }
  time_last_intra_request_ms_[new_ssrc] = last_intra_request_ms;
}

bool ViEEncoder::SetSsrcs(const std::vector<uint32_t>& ssrcs) {
  VideoCodec codec;
  if (vcm_->SendCodec(&codec) != 0)
    return false;

  if (codec.numberOfSimulcastStreams > 0 &&
      ssrcs.size() != codec.numberOfSimulcastStreams) {
    return false;
  }

  CriticalSectionScoped cs(data_cs_.get());
  ssrc_streams_.clear();
  time_last_intra_request_ms_.clear();
  int idx = 0;
  for (uint32_t ssrc : ssrcs) {
    ssrc_streams_[ssrc] = idx++;
  }
  return true;
}

void ViEEncoder::SetMinTransmitBitrate(int min_transmit_bitrate_kbps) {
  assert(min_transmit_bitrate_kbps >= 0);
  CriticalSectionScoped crit(data_cs_.get());
  min_transmit_bitrate_kbps_ = min_transmit_bitrate_kbps;
}

// Called from ViEBitrateObserver.
void ViEEncoder::OnNetworkChanged(uint32_t bitrate_bps,
                                  uint8_t fraction_lost,
                                  int64_t round_trip_time_ms) {
  LOG(LS_VERBOSE) << "OnNetworkChanged, bitrate" << bitrate_bps
                  << " packet loss " << static_cast<int>(fraction_lost)
                  << " rtt " << round_trip_time_ms;
  RTC_DCHECK(send_payload_router_ != NULL);
  vcm_->SetChannelParameters(bitrate_bps, fraction_lost, round_trip_time_ms);
  bool video_is_suspended = vcm_->VideoSuspended();

  VideoCodec send_codec;
  if (vcm_->SendCodec(&send_codec) != 0) {
    return;
  }
  SimulcastStream* stream_configs = send_codec.simulcastStream;
  // Allocate the bandwidth between the streams.
  std::vector<uint32_t> stream_bitrates = AllocateStreamBitrates(
      bitrate_bps, stream_configs, send_codec.numberOfSimulcastStreams);
  send_payload_router_->SetTargetSendBitrates(stream_bitrates);

  {
    CriticalSectionScoped cs(data_cs_.get());
    last_observed_bitrate_bps_ = bitrate_bps;
    if (video_suspended_ == video_is_suspended)
      return;
    video_suspended_ = video_is_suspended;
    LOG(LS_INFO) << "Video suspended " << video_is_suspended
                 << " for channel " << channel_id_;
  }
  // Video suspend-state changed, inform codec observer.
  if (stats_proxy_)
    stats_proxy_->OnSuspendChange(video_is_suspended);
}

void ViEEncoder::SuspendBelowMinBitrate() {
  vcm_->SuspendBelowMinBitrate();
  bitrate_allocator_->EnforceMinBitrate(false);
}

void ViEEncoder::RegisterPostEncodeImageCallback(
      EncodedImageCallback* post_encode_callback) {
  vcm_->RegisterPostEncodeImageCallback(post_encode_callback);
}

QMVideoSettingsCallback::QMVideoSettingsCallback(VideoProcessingModule* vpm)
    : vpm_(vpm) {
}

QMVideoSettingsCallback::~QMVideoSettingsCallback() {
}

int32_t QMVideoSettingsCallback::SetVideoQMSettings(
    const uint32_t frame_rate,
    const uint32_t width,
    const uint32_t height) {
  return vpm_->SetTargetResolution(width, height, frame_rate);
}

void QMVideoSettingsCallback::SetTargetFramerate(int frame_rate) {
  vpm_->SetTargetFramerate(frame_rate);
}

}  // namespace webrtc
