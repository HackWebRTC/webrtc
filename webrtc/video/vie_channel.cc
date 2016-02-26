/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/vie_channel.h"

#include <algorithm>
#include <map>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/common_video/include/incoming_video_stream.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/frame_callback.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/modules/video_render/video_render_defines.h"
#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/video/call_stats.h"
#include "webrtc/video/payload_router.h"
#include "webrtc/video/receive_statistics_proxy.h"

namespace webrtc {

static const int kMinSendSidePacketHistorySize = 600;
static const int kMaxPacketAgeToNack = 450;
static const int kMaxNackListSize = 250;

// Helper class receiving statistics callbacks.
class ChannelStatsObserver : public CallStatsObserver {
 public:
  explicit ChannelStatsObserver(ViEChannel* owner) : owner_(owner) {}
  virtual ~ChannelStatsObserver() {}

  // Implements StatsObserver.
  virtual void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
    owner_->OnRttUpdate(avg_rtt_ms, max_rtt_ms);
  }

 private:
  ViEChannel* const owner_;
};

class ViEChannelProtectionCallback : public VCMProtectionCallback {
 public:
  explicit ViEChannelProtectionCallback(ViEChannel* owner) : owner_(owner) {}
  ~ViEChannelProtectionCallback() {}


  int ProtectionRequest(
      const FecProtectionParams* delta_fec_params,
      const FecProtectionParams* key_fec_params,
      uint32_t* sent_video_rate_bps,
      uint32_t* sent_nack_rate_bps,
      uint32_t* sent_fec_rate_bps) override {
    return owner_->ProtectionRequest(delta_fec_params, key_fec_params,
                                     sent_video_rate_bps, sent_nack_rate_bps,
                                     sent_fec_rate_bps);
  }
 private:
  ViEChannel* owner_;
};

ViEChannel::ViEChannel(Transport* transport,
                       ProcessThread* module_process_thread,
                       PayloadRouter* send_payload_router,
                       VideoCodingModule* vcm,
                       RtcpIntraFrameObserver* intra_frame_observer,
                       RtcpBandwidthObserver* bandwidth_observer,
                       TransportFeedbackObserver* transport_feedback_observer,
                       RemoteBitrateEstimator* remote_bitrate_estimator,
                       RtcpRttStats* rtt_stats,
                       PacedSender* paced_sender,
                       PacketRouter* packet_router,
                       size_t max_rtp_streams,
                       bool sender)
    : sender_(sender),
      module_process_thread_(module_process_thread),
      send_payload_router_(send_payload_router),
      vcm_protection_callback_(new ViEChannelProtectionCallback(this)),
      vcm_(vcm),
      vie_receiver_(vcm_, remote_bitrate_estimator, this),
      stats_observer_(new ChannelStatsObserver(this)),
      receive_stats_callback_(nullptr),
      incoming_video_stream_(nullptr),
      intra_frame_observer_(intra_frame_observer),
      rtt_stats_(rtt_stats),
      paced_sender_(paced_sender),
      packet_router_(packet_router),
      bandwidth_observer_(bandwidth_observer),
      transport_feedback_observer_(transport_feedback_observer),
      max_nack_reordering_threshold_(kMaxPacketAgeToNack),
      pre_render_callback_(NULL),
      last_rtt_ms_(0),
      rtp_rtcp_modules_(
          CreateRtpRtcpModules(!sender,
                               vie_receiver_.GetReceiveStatistics(),
                               transport,
                               intra_frame_observer_,
                               bandwidth_observer_.get(),
                               transport_feedback_observer_,
                               rtt_stats_,
                               &rtcp_packet_type_counter_observer_,
                               remote_bitrate_estimator,
                               paced_sender_,
                               packet_router_,
                               &send_bitrate_observer_,
                               &send_frame_count_observer_,
                               &send_side_delay_observer_,
                               max_rtp_streams)),
      num_active_rtp_rtcp_modules_(1) {
  vie_receiver_.SetRtpRtcpModule(rtp_rtcp_modules_[0]);
  if (sender_) {
    RTC_DCHECK(send_payload_router_);
    RTC_DCHECK(!vcm_);
  } else {
    RTC_DCHECK(!send_payload_router_);
    RTC_DCHECK(vcm_);
    vcm_->SetNackSettings(kMaxNackListSize, max_nack_reordering_threshold_, 0);
  }
}

int32_t ViEChannel::Init() {
  static const int kDefaultRenderDelayMs = 10;
  module_process_thread_->RegisterModule(vie_receiver_.GetReceiveStatistics());

  // RTP/RTCP initialization.
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    module_process_thread_->RegisterModule(rtp_rtcp);

  rtp_rtcp_modules_[0]->SetKeyFrameRequestMethod(kKeyFrameReqPliRtcp);
  if (paced_sender_) {
    for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
      rtp_rtcp->SetStorePacketsStatus(true, kMinSendSidePacketHistorySize);
  }
  packet_router_->AddRtpModule(rtp_rtcp_modules_[0]);
  if (sender_) {
    send_payload_router_->SetSendingRtpModules(
        std::vector<RtpRtcp*>(1, rtp_rtcp_modules_[0]));
    RTC_DCHECK(!send_payload_router_->active());
  } else {
    if (vcm_->RegisterReceiveCallback(this) != 0) {
      return -1;
    }
    vcm_->RegisterFrameTypeCallback(this);
    vcm_->RegisterReceiveStatisticsCallback(this);
    vcm_->RegisterDecoderTimingCallback(this);
    vcm_->SetRenderDelay(kDefaultRenderDelayMs);
  }
  return 0;
}

ViEChannel::~ViEChannel() {
  UpdateHistograms();
  // Make sure we don't get more callbacks from the RTP module.
  module_process_thread_->DeRegisterModule(
      vie_receiver_.GetReceiveStatistics());
  if (sender_) {
    send_payload_router_->SetSendingRtpModules(std::vector<RtpRtcp*>());
  }
  for (size_t i = 0; i < num_active_rtp_rtcp_modules_; ++i)
    packet_router_->RemoveRtpModule(rtp_rtcp_modules_[i]);
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    module_process_thread_->DeRegisterModule(rtp_rtcp);
    delete rtp_rtcp;
  }
}

void ViEChannel::UpdateHistograms() {
  if (sender_) {
    StreamDataCounters rtp;
    StreamDataCounters rtx;
    GetSendStreamDataCounters(&rtp, &rtx);
    StreamDataCounters rtp_rtx = rtp;
    rtp_rtx.Add(rtx);
    int64_t elapsed_sec = rtp_rtx.TimeSinceFirstPacketInMs(
                              Clock::GetRealTimeClock()->TimeInMilliseconds()) /
                          1000;
    if (elapsed_sec > metrics::kMinRunTimeInSeconds) {
      RTC_HISTOGRAM_COUNTS_100000(
          "WebRTC.Video.BitrateSentInKbps",
          static_cast<int>(rtp_rtx.transmitted.TotalBytes() * 8 / elapsed_sec /
                           1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.MediaBitrateSentInKbps",
          static_cast<int>(rtp.MediaPayloadBytes() * 8 / elapsed_sec / 1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.PaddingBitrateSentInKbps",
          static_cast<int>(rtp_rtx.transmitted.padding_bytes * 8 / elapsed_sec /
                           1000));
      RTC_HISTOGRAM_COUNTS_10000(
          "WebRTC.Video.RetransmittedBitrateSentInKbps",
          static_cast<int>(rtp_rtx.retransmitted.TotalBytes() * 8 /
                           elapsed_sec / 1000));
      if (rtp_rtcp_modules_[0]->RtxSendStatus() != kRtxOff) {
        RTC_HISTOGRAM_COUNTS_10000(
            "WebRTC.Video.RtxBitrateSentInKbps",
            static_cast<int>(rtx.transmitted.TotalBytes() * 8 / elapsed_sec /
                             1000));
      }
      bool fec_enabled = false;
      uint8_t pltype_red;
      uint8_t pltype_fec;
      rtp_rtcp_modules_[0]->GenericFECStatus(&fec_enabled, &pltype_red,
                                             &pltype_fec);
      if (fec_enabled) {
        RTC_HISTOGRAM_COUNTS_10000("WebRTC.Video.FecBitrateSentInKbps",
                                   static_cast<int>(rtp_rtx.fec.TotalBytes() *
                                                    8 / elapsed_sec / 1000));
      }
    }
  }
}

int32_t ViEChannel::SetSendCodec(const VideoCodec& video_codec,
                                 bool new_stream) {
  RTC_DCHECK(sender_);
  if (video_codec.codecType == kVideoCodecRED ||
      video_codec.codecType == kVideoCodecULPFEC) {
    LOG_F(LS_ERROR) << "Not a valid send codec " << video_codec.codecType;
    return -1;
  }
  if (kMaxSimulcastStreams < video_codec.numberOfSimulcastStreams) {
    LOG_F(LS_ERROR) << "Incorrect config "
                    << video_codec.numberOfSimulcastStreams;
    return -1;
  }
  // Update the RTP module with the settings.
  // Stop and Start the RTP module -> trigger new SSRC, if an SSRC hasn't been
  // set explicitly.
  // The first layer is always active, so the first module can be checked for
  // sending status.
  bool is_sending = rtp_rtcp_modules_[0]->Sending();
  bool router_was_active = send_payload_router_->active();
  send_payload_router_->set_active(false);
  send_payload_router_->SetSendingRtpModules(std::vector<RtpRtcp*>());

  std::vector<RtpRtcp*> registered_modules;
  std::vector<RtpRtcp*> deregistered_modules;
  size_t num_active_modules = video_codec.numberOfSimulcastStreams > 0
                                   ? video_codec.numberOfSimulcastStreams
                                   : 1;
  size_t num_prev_active_modules;
  {
    // Cache which modules are active so StartSend can know which ones to start.
    rtc::CritScope lock(&crit_);
    num_prev_active_modules = num_active_rtp_rtcp_modules_;
    num_active_rtp_rtcp_modules_ = num_active_modules;
  }
  for (size_t i = 0; i < num_active_modules; ++i)
    registered_modules.push_back(rtp_rtcp_modules_[i]);

  for (size_t i = num_active_modules; i < rtp_rtcp_modules_.size(); ++i)
    deregistered_modules.push_back(rtp_rtcp_modules_[i]);

  // Disable inactive modules.
  for (RtpRtcp* rtp_rtcp : deregistered_modules) {
    rtp_rtcp->SetSendingStatus(false);
    rtp_rtcp->SetSendingMediaStatus(false);
  }

  // Configure active modules.
  for (RtpRtcp* rtp_rtcp : registered_modules) {
    rtp_rtcp->DeRegisterSendPayload(video_codec.plType);
    if (rtp_rtcp->RegisterSendPayload(video_codec) != 0) {
      return -1;
    }
    rtp_rtcp->SetSendingStatus(is_sending);
    rtp_rtcp->SetSendingMediaStatus(is_sending);
  }

  // |RegisterSimulcastRtpRtcpModules| resets all old weak pointers and old
  // modules can be deleted after this step.
  vie_receiver_.RegisterRtpRtcpModules(registered_modules);

  // Update the packet and payload routers with the sending RtpRtcp modules.
  send_payload_router_->SetSendingRtpModules(registered_modules);

  if (router_was_active)
    send_payload_router_->set_active(true);

  // Deregister previously registered modules.
  for (size_t i = num_active_modules; i < num_prev_active_modules; ++i)
    packet_router_->RemoveRtpModule(rtp_rtcp_modules_[i]);
  // Register new active modules.
  for (size_t i = num_prev_active_modules; i < num_active_modules; ++i)
    packet_router_->AddRtpModule(rtp_rtcp_modules_[i]);
  return 0;
}

void ViEChannel::SetProtectionMode(bool enable_nack,
                                   bool enable_fec,
                                   int payload_type_red,
                                   int payload_type_fec) {
  // Validate payload types. If either RED or FEC payload types are set then
  // both should be. If FEC is enabled then they both have to be set.
  if (enable_fec || payload_type_red != -1 || payload_type_fec != -1) {
    RTC_DCHECK_GE(payload_type_red, 0);
    RTC_DCHECK_GE(payload_type_fec, 0);
    RTC_DCHECK_LE(payload_type_red, 127);
    RTC_DCHECK_LE(payload_type_fec, 127);
  } else {
    // Payload types unset.
    RTC_DCHECK_EQ(payload_type_red, -1);
    RTC_DCHECK_EQ(payload_type_fec, -1);
    // Set to valid uint8_ts to be castable later without signed overflows.
    payload_type_red = 0;
    payload_type_fec = 0;
  }

  VCMVideoProtection protection_method;
  if (enable_nack) {
    protection_method = enable_fec ? kProtectionNackFEC : kProtectionNack;
  } else {
    protection_method = kProtectionNone;
  }

  if (!sender_)
    vcm_->SetVideoProtection(protection_method, true);

  // Set NACK.
  ProcessNACKRequest(enable_nack);

  // Set FEC.
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->SetGenericFECStatus(enable_fec,
                                  static_cast<uint8_t>(payload_type_red),
                                  static_cast<uint8_t>(payload_type_fec));
  }
}

void ViEChannel::ProcessNACKRequest(const bool enable) {
  if (enable) {
    // Turn on NACK.
    if (rtp_rtcp_modules_[0]->RTCP() == RtcpMode::kOff)
      return;
    vie_receiver_.SetNackStatus(true, max_nack_reordering_threshold_);

    for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
      rtp_rtcp->SetStorePacketsStatus(true, kMinSendSidePacketHistorySize);

    if (!sender_) {
      vcm_->RegisterPacketRequestCallback(this);
      // Don't introduce errors when NACK is enabled.
      vcm_->SetDecodeErrorMode(kNoErrors);
    }
  } else {
    if (!sender_) {
      vcm_->RegisterPacketRequestCallback(nullptr);
      for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
        rtp_rtcp->SetStorePacketsStatus(false, 0);
      // When NACK is off, allow decoding with errors. Otherwise, the video
      // will freeze, and will only recover with a complete key frame.
      vcm_->SetDecodeErrorMode(kWithErrors);
    }
    vie_receiver_.SetNackStatus(false, max_nack_reordering_threshold_);
  }
}

int ViEChannel::GetRequiredNackListSize(int target_delay_ms) {
  // The max size of the nack list should be large enough to accommodate the
  // the number of packets (frames) resulting from the increased delay.
  // Roughly estimating for ~40 packets per frame @ 30fps.
  return target_delay_ms * 40 * 30 / 1000;
}

RtpState ViEChannel::GetRtpStateForSsrc(uint32_t ssrc) const {
  RTC_DCHECK(!rtp_rtcp_modules_[0]->Sending());
  RtpState rtp_state;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    if (rtp_rtcp->GetRtpStateForSsrc(ssrc, &rtp_state))
      return rtp_state;
  }
  LOG(LS_ERROR) << "Couldn't get RTP state for ssrc: " << ssrc;
  return rtp_state;
}

void ViEChannel::RegisterRtcpPacketTypeCounterObserver(
    RtcpPacketTypeCounterObserver* observer) {
  rtcp_packet_type_counter_observer_.Set(observer);
}

void ViEChannel::GetSendStreamDataCounters(
    StreamDataCounters* rtp_counters,
    StreamDataCounters* rtx_counters) const {
  *rtp_counters = StreamDataCounters();
  *rtx_counters = StreamDataCounters();
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    StreamDataCounters rtp_data;
    StreamDataCounters rtx_data;
    rtp_rtcp->GetSendStreamDataCounters(&rtp_data, &rtx_data);
    rtp_counters->Add(rtp_data);
    rtx_counters->Add(rtx_data);
  }
}

void ViEChannel::RegisterSendSideDelayObserver(
    SendSideDelayObserver* observer) {
  send_side_delay_observer_.Set(observer);
}

void ViEChannel::RegisterSendBitrateObserver(
    BitrateStatisticsObserver* observer) {
  send_bitrate_observer_.Set(observer);
}

int32_t ViEChannel::StartSend() {
  rtc::CritScope lock(&crit_);

  if (rtp_rtcp_modules_[0]->Sending())
    return -1;

  for (size_t i = 0; i < num_active_rtp_rtcp_modules_; ++i) {
    RtpRtcp* rtp_rtcp = rtp_rtcp_modules_[i];
    // Only have senders send media.
    rtp_rtcp->SetSendingMediaStatus(sender_);
    rtp_rtcp->SetSendingStatus(true);
  }
  if (sender_)
    send_payload_router_->set_active(true);
  return 0;
}

int32_t ViEChannel::StopSend() {
  if (sender_)
    send_payload_router_->set_active(false);
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->SetSendingMediaStatus(false);

  if (!rtp_rtcp_modules_[0]->Sending()) {
    return -1;
  }

  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->SetSendingStatus(false);
  }
  return 0;
}

const std::vector<RtpRtcp*>& ViEChannel::rtp_rtcp() const {
  return rtp_rtcp_modules_;
}

ViEReceiver* ViEChannel::vie_receiver() {
  return &vie_receiver_;
}

VCMProtectionCallback* ViEChannel::vcm_protection_callback() {
  return vcm_protection_callback_.get();
}

CallStatsObserver* ViEChannel::GetStatsObserver() {
  return stats_observer_.get();
}

// Do not acquire the lock of |vcm_| in this function. Decode callback won't
// necessarily be called from the decoding thread. The decoding thread may have
// held the lock when calling VideoDecoder::Decode, Reset, or Release. Acquiring
// the same lock in the path of decode callback can deadlock.
int32_t ViEChannel::FrameToRender(VideoFrame& video_frame) {  // NOLINT
  rtc::CritScope lock(&crit_);

  if (pre_render_callback_ != NULL)
    pre_render_callback_->FrameCallback(&video_frame);

  // TODO(pbos): Remove stream id argument.
  incoming_video_stream_->RenderFrame(0xFFFFFFFF, video_frame);
  return 0;
}

int32_t ViEChannel::ReceivedDecodedReferenceFrame(
  const uint64_t picture_id) {
  return rtp_rtcp_modules_[0]->SendRTCPReferencePictureSelection(picture_id);
}

void ViEChannel::OnIncomingPayloadType(int payload_type) {
  rtc::CritScope lock(&crit_);
  if (receive_stats_callback_)
    receive_stats_callback_->OnIncomingPayloadType(payload_type);
}

void ViEChannel::OnDecoderImplementationName(const char* implementation_name) {
  rtc::CritScope lock(&crit_);
  if (receive_stats_callback_)
    receive_stats_callback_->OnDecoderImplementationName(implementation_name);
}

void ViEChannel::OnReceiveRatesUpdated(uint32_t bit_rate, uint32_t frame_rate) {
  rtc::CritScope lock(&crit_);
  if (receive_stats_callback_)
    receive_stats_callback_->OnIncomingRate(frame_rate, bit_rate);
}

void ViEChannel::OnDiscardedPacketsUpdated(int discarded_packets) {
  rtc::CritScope lock(&crit_);
  if (receive_stats_callback_)
    receive_stats_callback_->OnDiscardedPacketsUpdated(discarded_packets);
}

void ViEChannel::OnFrameCountsUpdated(const FrameCounts& frame_counts) {
  rtc::CritScope lock(&crit_);
  receive_frame_counts_ = frame_counts;
  if (receive_stats_callback_)
    receive_stats_callback_->OnFrameCountsUpdated(frame_counts);
}

void ViEChannel::OnDecoderTiming(int decode_ms,
                                 int max_decode_ms,
                                 int current_delay_ms,
                                 int target_delay_ms,
                                 int jitter_buffer_ms,
                                 int min_playout_delay_ms,
                                 int render_delay_ms) {
  rtc::CritScope lock(&crit_);
  if (!receive_stats_callback_)
    return;
  receive_stats_callback_->OnDecoderTiming(
      decode_ms, max_decode_ms, current_delay_ms, target_delay_ms,
      jitter_buffer_ms, min_playout_delay_ms, render_delay_ms, last_rtt_ms_);
}

int32_t ViEChannel::RequestKeyFrame() {
  return rtp_rtcp_modules_[0]->RequestKeyFrame();
}

int32_t ViEChannel::SliceLossIndicationRequest(
  const uint64_t picture_id) {
  return rtp_rtcp_modules_[0]->SendRTCPSliceLossIndication(
      static_cast<uint8_t>(picture_id));
}

int32_t ViEChannel::ResendPackets(const uint16_t* sequence_numbers,
                                  uint16_t length) {
  return rtp_rtcp_modules_[0]->SendNACK(sequence_numbers, length);
}

void ViEChannel::OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
  if (!sender_)
    vcm_->SetReceiveChannelParameters(max_rtt_ms);

  rtc::CritScope lock(&crit_);
  last_rtt_ms_ = avg_rtt_ms;
}

int ViEChannel::ProtectionRequest(const FecProtectionParams* delta_fec_params,
                                  const FecProtectionParams* key_fec_params,
                                  uint32_t* video_rate_bps,
                                  uint32_t* nack_rate_bps,
                                  uint32_t* fec_rate_bps) {
  *video_rate_bps = 0;
  *nack_rate_bps = 0;
  *fec_rate_bps = 0;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    uint32_t not_used = 0;
    uint32_t module_video_rate = 0;
    uint32_t module_fec_rate = 0;
    uint32_t module_nack_rate = 0;
    rtp_rtcp->SetFecParameters(delta_fec_params, key_fec_params);
    rtp_rtcp->BitrateSent(&not_used, &module_video_rate, &module_fec_rate,
                          &module_nack_rate);
    *video_rate_bps += module_video_rate;
    *nack_rate_bps += module_nack_rate;
    *fec_rate_bps += module_fec_rate;
  }
  return 0;
}

std::vector<RtpRtcp*> ViEChannel::CreateRtpRtcpModules(
    bool receiver_only,
    ReceiveStatistics* receive_statistics,
    Transport* outgoing_transport,
    RtcpIntraFrameObserver* intra_frame_callback,
    RtcpBandwidthObserver* bandwidth_callback,
    TransportFeedbackObserver* transport_feedback_callback,
    RtcpRttStats* rtt_stats,
    RtcpPacketTypeCounterObserver* rtcp_packet_type_counter_observer,
    RemoteBitrateEstimator* remote_bitrate_estimator,
    RtpPacketSender* paced_sender,
    TransportSequenceNumberAllocator* transport_sequence_number_allocator,
    BitrateStatisticsObserver* send_bitrate_observer,
    FrameCountObserver* send_frame_count_observer,
    SendSideDelayObserver* send_side_delay_observer,
    size_t num_modules) {
  RTC_DCHECK_GT(num_modules, 0u);
  RtpRtcp::Configuration configuration;
  ReceiveStatistics* null_receive_statistics = configuration.receive_statistics;
  configuration.audio = false;
  configuration.receiver_only = receiver_only;
  configuration.receive_statistics = receive_statistics;
  configuration.outgoing_transport = outgoing_transport;
  configuration.intra_frame_callback = intra_frame_callback;
  configuration.rtt_stats = rtt_stats;
  configuration.rtcp_packet_type_counter_observer =
      rtcp_packet_type_counter_observer;
  configuration.paced_sender = paced_sender;
  configuration.transport_sequence_number_allocator =
      transport_sequence_number_allocator;
  configuration.send_bitrate_observer = send_bitrate_observer;
  configuration.send_frame_count_observer = send_frame_count_observer;
  configuration.send_side_delay_observer = send_side_delay_observer;
  configuration.bandwidth_callback = bandwidth_callback;
  configuration.transport_feedback_callback = transport_feedback_callback;

  std::vector<RtpRtcp*> modules;
  for (size_t i = 0; i < num_modules; ++i) {
    RtpRtcp* rtp_rtcp = RtpRtcp::CreateRtpRtcp(configuration);
    rtp_rtcp->SetSendingStatus(false);
    rtp_rtcp->SetSendingMediaStatus(false);
    rtp_rtcp->SetRTCPStatus(RtcpMode::kCompound);
    modules.push_back(rtp_rtcp);
    // Receive statistics and remote bitrate estimator should only be set for
    // the primary (first) module.
    configuration.receive_statistics = null_receive_statistics;
    configuration.remote_bitrate_estimator = nullptr;
  }
  return modules;
}

void ViEChannel::RegisterPreRenderCallback(
    I420FrameCallback* pre_render_callback) {
  RTC_DCHECK(!sender_);
  rtc::CritScope lock(&crit_);
  pre_render_callback_ = pre_render_callback;
}

// TODO(pbos): Remove as soon as audio can handle a changing payload type
// without this callback.
int32_t ViEChannel::OnInitializeDecoder(
    const int8_t payload_type,
    const char payload_name[RTP_PAYLOAD_NAME_SIZE],
    const int frequency,
    const size_t channels,
    const uint32_t rate) {
  RTC_NOTREACHED();
  return 0;
}

void ViEChannel::OnIncomingSSRCChanged(const uint32_t ssrc) {
  rtp_rtcp_modules_[0]->SetRemoteSSRC(ssrc);
}

void ViEChannel::OnIncomingCSRCChanged(const uint32_t CSRC, const bool added) {}

void ViEChannel::RegisterSendFrameCountObserver(
    FrameCountObserver* observer) {
  send_frame_count_observer_.Set(observer);
}

void ViEChannel::RegisterReceiveStatisticsProxy(
    ReceiveStatisticsProxy* receive_statistics_proxy) {
  rtc::CritScope lock(&crit_);
  receive_stats_callback_ = receive_statistics_proxy;
}

void ViEChannel::SetIncomingVideoStream(
    IncomingVideoStream* incoming_video_stream) {
  rtc::CritScope lock(&crit_);
  incoming_video_stream_ = incoming_video_stream;
}
}  // namespace webrtc
