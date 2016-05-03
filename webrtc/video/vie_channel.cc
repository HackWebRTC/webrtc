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
#include "webrtc/common_video/include/frame_callback.h"
#include "webrtc/common_video/include/incoming_video_stream.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/video_coding/video_coding_impl.h"
#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/video/call_stats.h"
#include "webrtc/video/payload_router.h"
#include "webrtc/video/receive_statistics_proxy.h"

namespace webrtc {

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

ViEChannel::ViEChannel(vcm::VideoReceiver* video_receiver,
                       RtpStreamReceiver* rtp_stream_receiver)
    : video_receiver_(video_receiver),
      rtp_stream_receiver_(rtp_stream_receiver),
      rtp_rtcp_(rtp_stream_receiver_->rtp_rtcp()),
      stats_observer_(new ChannelStatsObserver(this)),
      receive_stats_callback_(nullptr),
      incoming_video_stream_(nullptr),
      max_nack_reordering_threshold_(kMaxPacketAgeToNack),
      pre_render_callback_(nullptr),
      last_rtt_ms_(0) {
  RTC_DCHECK(video_receiver_);
  video_receiver_->SetNackSettings(kMaxNackListSize,
                                   max_nack_reordering_threshold_, 0);
}

ViEChannel::~ViEChannel() {}

int32_t ViEChannel::Init() {
  static const int kDefaultRenderDelayMs = 10;

  if (video_receiver_->RegisterReceiveCallback(this) != 0) {
    return -1;
  }
  video_receiver_->RegisterFrameTypeCallback(this);
  video_receiver_->RegisterReceiveStatisticsCallback(this);
  video_receiver_->RegisterDecoderTimingCallback(this);
  video_receiver_->SetRenderDelay(kDefaultRenderDelayMs);

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

  video_receiver_->SetVideoProtection(protection_method, true);

  // Set NACK.
  ProcessNACKRequest(enable_nack);

  // Set FEC.
  rtp_rtcp_->SetGenericFECStatus(enable_fec,
                                 static_cast<uint8_t>(payload_type_red),
                                 static_cast<uint8_t>(payload_type_fec));
}

void ViEChannel::ProcessNACKRequest(const bool enable) {
  if (enable) {
    // Turn on NACK.
    if (rtp_rtcp_->RTCP() == RtcpMode::kOff)
      return;
    rtp_stream_receiver_->SetNackStatus(true, max_nack_reordering_threshold_);
    video_receiver_->RegisterPacketRequestCallback(this);
    // Don't introduce errors when NACK is enabled.
    video_receiver_->SetDecodeErrorMode(kNoErrors);

  } else {
    video_receiver_->RegisterPacketRequestCallback(nullptr);
    // When NACK is off, allow decoding with errors. Otherwise, the video
    // will freeze, and will only recover with a complete key frame.
    video_receiver_->SetDecodeErrorMode(kWithErrors);
    rtp_stream_receiver_->SetNackStatus(false, max_nack_reordering_threshold_);
  }
}

int ViEChannel::GetRequiredNackListSize(int target_delay_ms) {
  // The max size of the nack list should be large enough to accommodate the
  // the number of packets (frames) resulting from the increased delay.
  // Roughly estimating for ~40 packets per frame @ 30fps.
  return target_delay_ms * 40 * 30 / 1000;
}

RtpState ViEChannel::GetRtpStateForSsrc(uint32_t ssrc) const {
  RTC_DCHECK(!rtp_rtcp_->Sending());
  RTC_DCHECK_EQ(ssrc, rtp_rtcp_->SSRC());
  return rtp_rtcp_->GetRtpState();
}

void ViEChannel::RegisterRtcpPacketTypeCounterObserver(
    RtcpPacketTypeCounterObserver* observer) {
  rtp_stream_receiver_->RegisterRtcpPacketTypeCounterObserver(observer);
}

CallStatsObserver* ViEChannel::GetStatsObserver() {
  return stats_observer_.get();
}

// Do not acquire the lock of |video_receiver_| in this function. Decode
// callback won't necessarily be called from the decoding thread. The decoding
// thread may have held the lock when calling VideoDecoder::Decode, Reset, or
// Release. Acquiring the same lock in the path of decode callback can deadlock.
int32_t ViEChannel::FrameToRender(VideoFrame& video_frame) {  // NOLINT
  rtc::CritScope lock(&crit_);

  if (pre_render_callback_)
    pre_render_callback_->FrameCallback(&video_frame);

  incoming_video_stream_->OnFrame(video_frame);
  return 0;
}

int32_t ViEChannel::ReceivedDecodedReferenceFrame(
  const uint64_t picture_id) {
  return rtp_rtcp_->SendRTCPReferencePictureSelection(picture_id);
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
  return rtp_rtcp_->RequestKeyFrame();
}

int32_t ViEChannel::SliceLossIndicationRequest(
  const uint64_t picture_id) {
  return rtp_rtcp_->SendRTCPSliceLossIndication(
      static_cast<uint8_t>(picture_id));
}

int32_t ViEChannel::ResendPackets(const uint16_t* sequence_numbers,
                                  uint16_t length) {
  return rtp_rtcp_->SendNACK(sequence_numbers, length);
}

void ViEChannel::OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) {
  video_receiver_->SetReceiveChannelParameters(max_rtt_ms);

  rtc::CritScope lock(&crit_);
  last_rtt_ms_ = avg_rtt_ms;
}

void ViEChannel::RegisterPreRenderCallback(
    I420FrameCallback* pre_render_callback) {
  rtc::CritScope lock(&crit_);
  pre_render_callback_ = pre_render_callback;
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
