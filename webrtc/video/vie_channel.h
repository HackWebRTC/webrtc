/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIE_CHANNEL_H_
#define WEBRTC_VIDEO_VIE_CHANNEL_H_

#include <list>
#include <map>
#include <memory>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/system_wrappers/include/tick_util.h"
#include "webrtc/typedefs.h"
#include "webrtc/video/vie_receiver.h"
#include "webrtc/video/vie_sync_module.h"

namespace webrtc {

class CallStatsObserver;
class ChannelStatsObserver;
class Config;
class EncodedImageCallback;
class I420FrameCallback;
class IncomingVideoStream;
class PacedSender;
class PacketRouter;
class PayloadRouter;
class ProcessThread;
class ReceiveStatisticsProxy;
class RtcpRttStats;
class ViERTPObserver;
class VideoCodingModule;
class VideoRenderCallback;
class VoEVideoSync;

enum StreamType {
  kViEStreamTypeNormal = 0,  // Normal media stream
  kViEStreamTypeRtx = 1      // Retransmission media stream
};

class ViEChannel : public VCMFrameTypeCallback,
                   public VCMReceiveCallback,
                   public VCMReceiveStatisticsCallback,
                   public VCMDecoderTimingCallback,
                   public VCMPacketRequestCallback,
                   public RtpFeedback {
 public:
  friend class ChannelStatsObserver;

  ViEChannel(Transport* transport,
             ProcessThread* module_process_thread,
             VideoCodingModule* vcm,
             RemoteBitrateEstimator* remote_bitrate_estimator,
             RtcpRttStats* rtt_stats,
             PacedSender* paced_sender,
             PacketRouter* packet_router);
  ~ViEChannel();

  int32_t Init();

  void SetProtectionMode(bool enable_nack,
                         bool enable_fec,
                         int payload_type_red,
                         int payload_type_fec);

  RtpState GetRtpStateForSsrc(uint32_t ssrc) const;

  // Implements RtpFeedback.
  int32_t OnInitializeDecoder(const int8_t payload_type,
                              const char payload_name[RTP_PAYLOAD_NAME_SIZE],
                              const int frequency,
                              const size_t channels,
                              const uint32_t rate) override;
  void OnIncomingSSRCChanged(const uint32_t ssrc) override;
  void OnIncomingCSRCChanged(const uint32_t CSRC, const bool added) override;

  // Gets the module used by the channel.
  RtpRtcp* rtp_rtcp() const;
  ViEReceiver* vie_receiver();

  CallStatsObserver* GetStatsObserver();

  // Implements VCMReceiveCallback.
  virtual int32_t FrameToRender(VideoFrame& video_frame);  // NOLINT

  // Implements VCMReceiveCallback.
  virtual int32_t ReceivedDecodedReferenceFrame(
      const uint64_t picture_id);

  // Implements VCMReceiveCallback.
  void OnIncomingPayloadType(int payload_type) override;
  void OnDecoderImplementationName(const char* implementation_name) override;

  // Implements VCMReceiveStatisticsCallback.
  void OnReceiveRatesUpdated(uint32_t bit_rate, uint32_t frame_rate) override;
  void OnDiscardedPacketsUpdated(int discarded_packets) override;
  void OnFrameCountsUpdated(const FrameCounts& frame_counts) override;

  // Implements VCMDecoderTimingCallback.
  virtual void OnDecoderTiming(int decode_ms,
                               int max_decode_ms,
                               int current_delay_ms,
                               int target_delay_ms,
                               int jitter_buffer_ms,
                               int min_playout_delay_ms,
                               int render_delay_ms);

  // Implements FrameTypeCallback.
  virtual int32_t RequestKeyFrame();

  // Implements FrameTypeCallback.
  virtual int32_t SliceLossIndicationRequest(
      const uint64_t picture_id);

  // Implements VideoPacketRequestCallback.
  int32_t ResendPackets(const uint16_t* sequence_numbers,
                        uint16_t length) override;

  void RegisterPreRenderCallback(I420FrameCallback* pre_render_callback);

  void RegisterRtcpPacketTypeCounterObserver(
      RtcpPacketTypeCounterObserver* observer);
  void RegisterReceiveStatisticsProxy(
      ReceiveStatisticsProxy* receive_statistics_proxy);
  void SetIncomingVideoStream(IncomingVideoStream* incoming_video_stream);

 protected:
  void OnRttUpdate(int64_t avg_rtt_ms, int64_t max_rtt_ms);

 private:
  // Assumed to be protected.
  void StartDecodeThread();
  void StopDecodeThread();

  void ProcessNACKRequest(const bool enable);
  // Compute NACK list parameters for the buffering mode.
  int GetRequiredNackListSize(int target_delay_ms);

  // ViEChannel exposes methods that allow to modify observers and callbacks
  // to be modified. Such an API-style is cumbersome to implement and maintain
  // at all the levels when comparing to only setting them at construction. As
  // so this class instantiates its children with a wrapper that can be modified
  // at a later time.
  template <class T>
  class RegisterableCallback : public T {
   public:
    RegisterableCallback() : callback_(nullptr) {}

    void Set(T* callback) {
      rtc::CritScope lock(&critsect_);
      callback_ = callback;
    }

   protected:
    // Note: this should be implemented with a RW-lock to allow simultaneous
    // calls into the callback. However that doesn't seem to be needed for the
    // current type of callbacks covered by this class.
    rtc::CriticalSection critsect_;
    T* callback_ GUARDED_BY(critsect_);

   private:
    RTC_DISALLOW_COPY_AND_ASSIGN(RegisterableCallback);
  };

  class RegisterableRtcpPacketTypeCounterObserver
      : public RegisterableCallback<RtcpPacketTypeCounterObserver> {
   public:
    void RtcpPacketTypesCounterUpdated(
        uint32_t ssrc,
        const RtcpPacketTypeCounter& packet_counter) override {
      rtc::CritScope lock(&critsect_);
      if (callback_)
        callback_->RtcpPacketTypesCounterUpdated(ssrc, packet_counter);
    }

   private:
  } rtcp_packet_type_counter_observer_;


  ProcessThread* const module_process_thread_;

  // Used for all registered callbacks except rendering.
  rtc::CriticalSection crit_;

  VideoCodingModule* const vcm_;
  ViEReceiver vie_receiver_;

  // Helper to report call statistics.
  std::unique_ptr<ChannelStatsObserver> stats_observer_;

  // Not owned.
  ReceiveStatisticsProxy* receive_stats_callback_ GUARDED_BY(crit_);
  FrameCounts receive_frame_counts_ GUARDED_BY(crit_);
  IncomingVideoStream* incoming_video_stream_ GUARDED_BY(crit_);
  RtcpRttStats* const rtt_stats_;
  PacedSender* const paced_sender_;
  PacketRouter* const packet_router_;

  int max_nack_reordering_threshold_;
  I420FrameCallback* pre_render_callback_ GUARDED_BY(crit_);

  int64_t last_rtt_ms_ GUARDED_BY(crit_);

  // RtpRtcp module, declared last as it use other members on construction.
  const std::unique_ptr<RtpRtcp> rtp_rtcp_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIE_CHANNEL_H_
