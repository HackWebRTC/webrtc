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
#include "webrtc/video/rtp_stream_receiver.h"
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
class ReceiveStatisticsProxy;
class RtcpRttStats;
class ViERTPObserver;
class VoEVideoSync;

namespace vcm {
class VideoReceiver;
}  // namespace vcm

enum StreamType {
  kViEStreamTypeNormal = 0,  // Normal media stream
  kViEStreamTypeRtx = 1      // Retransmission media stream
};

class ViEChannel : public VCMFrameTypeCallback,
                   public VCMReceiveCallback,
                   public VCMReceiveStatisticsCallback,
                   public VCMDecoderTimingCallback,
                   public VCMPacketRequestCallback {
 public:
  friend class ChannelStatsObserver;

  ViEChannel(vcm::VideoReceiver* video_receiver,
             RtpStreamReceiver* rtp_stream_receiver);
  ~ViEChannel();

  int32_t Init();

  RtpRtcp* rtp_rtcp() const { return rtp_rtcp_; }

  void SetProtectionMode(bool enable_nack,
                         bool enable_fec,
                         int payload_type_red,
                         int payload_type_fec);

  RtpState GetRtpStateForSsrc(uint32_t ssrc) const;


  CallStatsObserver* GetStatsObserver();

  // Implements VCMReceiveCallback.
  int32_t FrameToRender(VideoFrame& video_frame) override;  // NOLINT

  // Implements VCMReceiveCallback.
  int32_t ReceivedDecodedReferenceFrame(const uint64_t picture_id) override;

  // Implements VCMReceiveCallback.
  void OnIncomingPayloadType(int payload_type) override;
  void OnDecoderImplementationName(const char* implementation_name) override;

  // Implements VCMReceiveStatisticsCallback.
  void OnReceiveRatesUpdated(uint32_t bit_rate, uint32_t frame_rate) override;
  void OnDiscardedPacketsUpdated(int discarded_packets) override;
  void OnFrameCountsUpdated(const FrameCounts& frame_counts) override;

  // Implements VCMDecoderTimingCallback.
  void OnDecoderTiming(int decode_ms,
                       int max_decode_ms,
                       int current_delay_ms,
                       int target_delay_ms,
                       int jitter_buffer_ms,
                       int min_playout_delay_ms,
                       int render_delay_ms) override;

  // Implements FrameTypeCallback.
  int32_t RequestKeyFrame() override;

  // Implements FrameTypeCallback.
  int32_t SliceLossIndicationRequest(
      const uint64_t picture_id) override;

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

  // Used for all registered callbacks except rendering.
  rtc::CriticalSection crit_;

  vcm::VideoReceiver* const video_receiver_;
  RtpStreamReceiver* const rtp_stream_receiver_;
  RtpRtcp* const rtp_rtcp_;

  // Helper to report call statistics.
  std::unique_ptr<ChannelStatsObserver> stats_observer_;

  // Not owned.
  ReceiveStatisticsProxy* receive_stats_callback_ GUARDED_BY(crit_);
  FrameCounts receive_frame_counts_ GUARDED_BY(crit_);
  IncomingVideoStream* incoming_video_stream_ GUARDED_BY(crit_);

  int max_nack_reordering_threshold_;
  I420FrameCallback* pre_render_callback_ GUARDED_BY(crit_);

  int64_t last_rtt_ms_ GUARDED_BY(crit_);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIE_CHANNEL_H_
