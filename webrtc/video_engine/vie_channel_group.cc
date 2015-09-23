/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_channel_group.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common.h"
#include "webrtc/modules/pacing/include/paced_sender.h"
#include "webrtc/modules/pacing/include/packet_router.h"
#include "webrtc/modules/remote_bitrate_estimator/include/send_time_history.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_abs_send_time.h"
#include "webrtc/modules/remote_bitrate_estimator/remote_bitrate_estimator_single_stream.h"
#include "webrtc/modules/remote_bitrate_estimator/transport_feedback_adapter.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/utility/interface/process_thread.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/video_engine/call_stats.h"
#include "webrtc/video_engine/encoder_state_feedback.h"
#include "webrtc/video_engine/payload_router.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_engine/vie_remb.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"

namespace webrtc {
namespace {

static const uint32_t kTimeOffsetSwitchThreshold = 30;

class WrappingBitrateEstimator : public RemoteBitrateEstimator {
 public:
  WrappingBitrateEstimator(RemoteBitrateObserver* observer, Clock* clock)
      : observer_(observer),
        clock_(clock),
        crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
        min_bitrate_bps_(RemoteBitrateEstimator::kDefaultMinBitrateBps),
        rbe_(new RemoteBitrateEstimatorSingleStream(observer_,
                                                    clock_,
                                                    min_bitrate_bps_)),
        using_absolute_send_time_(false),
        packets_since_absolute_send_time_(0) {}

  virtual ~WrappingBitrateEstimator() {}

  void IncomingPacket(int64_t arrival_time_ms,
                      size_t payload_size,
                      const RTPHeader& header,
                      bool was_paced) override {
    CriticalSectionScoped cs(crit_sect_.get());
    PickEstimatorFromHeader(header);
    rbe_->IncomingPacket(arrival_time_ms, payload_size, header, was_paced);
  }

  int32_t Process() override {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->Process();
  }

  int64_t TimeUntilNextProcess() override {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->TimeUntilNextProcess();
  }

  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms) override {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->OnRttUpdate(avg_rtt_ms, max_rtt_ms);
  }

  void RemoveStream(unsigned int ssrc) override {
    CriticalSectionScoped cs(crit_sect_.get());
    rbe_->RemoveStream(ssrc);
  }

  bool LatestEstimate(std::vector<unsigned int>* ssrcs,
                      unsigned int* bitrate_bps) const override {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->LatestEstimate(ssrcs, bitrate_bps);
  }

  bool GetStats(ReceiveBandwidthEstimatorStats* output) const override {
    CriticalSectionScoped cs(crit_sect_.get());
    return rbe_->GetStats(output);
  }

 private:
  void PickEstimatorFromHeader(const RTPHeader& header)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get()) {
    if (header.extension.hasAbsoluteSendTime) {
      // If we see AST in header, switch RBE strategy immediately.
      if (!using_absolute_send_time_) {
        LOG(LS_INFO) <<
            "WrappingBitrateEstimator: Switching to absolute send time RBE.";
        using_absolute_send_time_ = true;
        PickEstimator();
      }
      packets_since_absolute_send_time_ = 0;
    } else {
      // When we don't see AST, wait for a few packets before going back to TOF.
      if (using_absolute_send_time_) {
        ++packets_since_absolute_send_time_;
        if (packets_since_absolute_send_time_ >= kTimeOffsetSwitchThreshold) {
          LOG(LS_INFO) << "WrappingBitrateEstimator: Switching to transmission "
                       << "time offset RBE.";
          using_absolute_send_time_ = false;
          PickEstimator();
        }
      }
    }
  }

  // Instantiate RBE for Time Offset or Absolute Send Time extensions.
  void PickEstimator() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_.get()) {
    if (using_absolute_send_time_) {
      rbe_.reset(new RemoteBitrateEstimatorAbsSendTime(observer_, clock_,
                                                       min_bitrate_bps_));
    } else {
      rbe_.reset(new RemoteBitrateEstimatorSingleStream(observer_, clock_,
                                                        min_bitrate_bps_));
    }
  }

  RemoteBitrateObserver* observer_;
  Clock* clock_;
  rtc::scoped_ptr<CriticalSectionWrapper> crit_sect_;
  const uint32_t min_bitrate_bps_;
  rtc::scoped_ptr<RemoteBitrateEstimator> rbe_;
  bool using_absolute_send_time_;
  uint32_t packets_since_absolute_send_time_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(WrappingBitrateEstimator);
};

}  // namespace

ChannelGroup::ChannelGroup(ProcessThread* process_thread)
    : remb_(new VieRemb()),
      bitrate_allocator_(new BitrateAllocator()),
      call_stats_(new CallStats()),
      encoder_state_feedback_(new EncoderStateFeedback()),
      packet_router_(new PacketRouter()),
      pacer_(new PacedSender(Clock::GetRealTimeClock(),
                             packet_router_.get(),
                             BitrateController::kDefaultStartBitrateKbps,
                             PacedSender::kDefaultPaceMultiplier *
                                 BitrateController::kDefaultStartBitrateKbps,
                             0)),
      process_thread_(process_thread),
      pacer_thread_(ProcessThread::Create("PacerThread")),
      // Constructed last as this object calls the provided callback on
      // construction.
      bitrate_controller_(
          BitrateController::CreateBitrateController(Clock::GetRealTimeClock(),
                                                     this)) {
  remote_bitrate_estimator_.reset(new WrappingBitrateEstimator(
      remb_.get(), Clock::GetRealTimeClock()));

  call_stats_->RegisterStatsObserver(remote_bitrate_estimator_.get());

  pacer_thread_->RegisterModule(pacer_.get());
  pacer_thread_->Start();

  process_thread->RegisterModule(remote_bitrate_estimator_.get());
  process_thread->RegisterModule(call_stats_.get());
  process_thread->RegisterModule(bitrate_controller_.get());
}

ChannelGroup::~ChannelGroup() {
  pacer_thread_->Stop();
  pacer_thread_->DeRegisterModule(pacer_.get());
  process_thread_->DeRegisterModule(bitrate_controller_.get());
  process_thread_->DeRegisterModule(call_stats_.get());
  process_thread_->DeRegisterModule(remote_bitrate_estimator_.get());
  call_stats_->DeregisterStatsObserver(remote_bitrate_estimator_.get());
  RTC_DCHECK(channel_map_.empty());
  RTC_DCHECK(!remb_->InUse());
  RTC_DCHECK(vie_encoder_map_.empty());
}

bool ChannelGroup::CreateSendChannel(int channel_id,
                                     Transport* transport,
                                     SendStatisticsProxy* stats_proxy,
                                     I420FrameCallback* pre_encode_callback,
                                     int number_of_cores,
                                     const std::vector<uint32_t>& ssrcs) {
  RTC_DCHECK(!ssrcs.empty());
  rtc::scoped_ptr<ViEEncoder> vie_encoder(new ViEEncoder(
      channel_id, number_of_cores, process_thread_, stats_proxy,
      pre_encode_callback, pacer_.get(), bitrate_allocator_.get()));
  if (!vie_encoder->Init()) {
    return false;
  }
  ViEEncoder* encoder = vie_encoder.get();
  if (!CreateChannel(channel_id, transport, number_of_cores,
                     vie_encoder.release(), ssrcs.size(), true)) {
    return false;
  }
  ViEChannel* channel = channel_map_[channel_id];
  // Connect the encoder with the send packet router, to enable sending.
  encoder->StartThreadsAndSetSharedMembers(channel->send_payload_router(),
                                           channel->vcm_protection_callback());

  encoder_state_feedback_->AddEncoder(ssrcs, encoder);
  std::vector<uint32_t> first_ssrc(1, ssrcs[0]);
  encoder->SetSsrcs(first_ssrc);
  return true;
}

bool ChannelGroup::CreateReceiveChannel(int channel_id,
                                        Transport* transport,
                                        int number_of_cores) {
  return CreateChannel(channel_id, transport, number_of_cores,
                       nullptr, 1, false);
}

bool ChannelGroup::CreateChannel(int channel_id,
                                 Transport* transport,
                                 int number_of_cores,
                                 ViEEncoder* vie_encoder,
                                 size_t max_rtp_streams,
                                 bool sender) {
  rtc::scoped_ptr<ViEChannel> channel(new ViEChannel(
      number_of_cores, transport, process_thread_,
      encoder_state_feedback_->GetRtcpIntraFrameObserver(),
      bitrate_controller_->CreateRtcpBandwidthObserver(), nullptr,
      remote_bitrate_estimator_.get(), call_stats_->rtcp_rtt_stats(),
      pacer_.get(), packet_router_.get(), max_rtp_streams, sender));
  if (channel->Init() != 0) {
    return false;
  }

  // Register the channel to receive stats updates.
  call_stats_->RegisterStatsObserver(channel->GetStatsObserver());

  // Store the channel, add it to the channel group and save the vie_encoder.
  channel_map_[channel_id] = channel.release();
  if (vie_encoder) {
    rtc::CritScope lock(&encoder_map_crit_);
    vie_encoder_map_[channel_id] = vie_encoder;
  }

  return true;
}

void ChannelGroup::DeleteChannel(int channel_id) {
  ViEChannel* vie_channel = PopChannel(channel_id);

  ViEEncoder* vie_encoder = GetEncoder(channel_id);

  call_stats_->DeregisterStatsObserver(vie_channel->GetStatsObserver());
  SetChannelRembStatus(false, false, vie_channel);

  // If we're a sender, remove the feedback and stop all encoding threads and
  // processing. This must be done before deleting the channel.
  if (vie_encoder) {
    encoder_state_feedback_->RemoveEncoder(vie_encoder);
    vie_encoder->StopThreadsAndRemoveSharedMembers();
  }

  unsigned int remote_ssrc = 0;
  vie_channel->GetRemoteSSRC(&remote_ssrc);
  channel_map_.erase(channel_id);
  remote_bitrate_estimator_->RemoveStream(remote_ssrc);

  delete vie_channel;

  if (vie_encoder) {
    {
      rtc::CritScope lock(&encoder_map_crit_);
      vie_encoder_map_.erase(vie_encoder_map_.find(channel_id));
    }
    delete vie_encoder;
  }

  LOG(LS_VERBOSE) << "Channel deleted " << channel_id;
}

ViEChannel* ChannelGroup::GetChannel(int channel_id) const {
  ChannelMap::const_iterator it = channel_map_.find(channel_id);
  if (it == channel_map_.end()) {
    LOG(LS_ERROR) << "Channel doesn't exist " << channel_id;
    return NULL;
  }
  return it->second;
}

ViEEncoder* ChannelGroup::GetEncoder(int channel_id) const {
  rtc::CritScope lock(&encoder_map_crit_);
  EncoderMap::const_iterator it = vie_encoder_map_.find(channel_id);
  if (it == vie_encoder_map_.end())
    return nullptr;
  return it->second;
}

ViEChannel* ChannelGroup::PopChannel(int channel_id) {
  ChannelMap::iterator c_it = channel_map_.find(channel_id);
  RTC_DCHECK(c_it != channel_map_.end());
  ViEChannel* channel = c_it->second;
  channel_map_.erase(c_it);

  return channel;
}

void ChannelGroup::SetSyncInterface(VoEVideoSync* sync_interface) {
  for (auto channel : channel_map_)
    channel.second->SetVoiceChannel(-1, sync_interface);
}

BitrateController* ChannelGroup::GetBitrateController() const {
  return bitrate_controller_.get();
}

RemoteBitrateEstimator* ChannelGroup::GetRemoteBitrateEstimator() const {
  return remote_bitrate_estimator_.get();
}

CallStats* ChannelGroup::GetCallStats() const {
  return call_stats_.get();
}

EncoderStateFeedback* ChannelGroup::GetEncoderStateFeedback() const {
  return encoder_state_feedback_.get();
}

int64_t ChannelGroup::GetPacerQueuingDelayMs() const {
  return pacer_->QueueInMs();
}

void ChannelGroup::SetChannelRembStatus(bool sender,
                                        bool receiver,
                                        ViEChannel* channel) {
  // Update the channel state.
  channel->EnableRemb(sender || receiver);
  // Update the REMB instance with necessary RTP modules.
  RtpRtcp* rtp_module = channel->rtp_rtcp();
  if (sender) {
    remb_->AddRembSender(rtp_module);
  } else {
    remb_->RemoveRembSender(rtp_module);
  }
  if (receiver) {
    remb_->AddReceiveChannel(rtp_module);
  } else {
    remb_->RemoveReceiveChannel(rtp_module);
  }
}

void ChannelGroup::OnNetworkChanged(uint32_t target_bitrate_bps,
                                    uint8_t fraction_loss,
                                    int64_t rtt) {
  bitrate_allocator_->OnNetworkChanged(target_bitrate_bps, fraction_loss, rtt);
  int pad_up_to_bitrate_bps = 0;
  {
    rtc::CritScope lock(&encoder_map_crit_);
    for (const auto& encoder : vie_encoder_map_)
      pad_up_to_bitrate_bps += encoder.second->GetPaddingNeededBps();
  }
  pacer_->UpdateBitrate(
      target_bitrate_bps / 1000,
      PacedSender::kDefaultPaceMultiplier * target_bitrate_bps / 1000,
      pad_up_to_bitrate_bps / 1000);
}
}  // namespace webrtc
