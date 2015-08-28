/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIDEO_SEND_STREAM_H_
#define WEBRTC_VIDEO_VIDEO_SEND_STREAM_H_

#include <map>
#include <vector>

#include "webrtc/call.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp_defines.h"
#include "webrtc/video/encoded_frame_callback_adapter.h"
#include "webrtc/video/send_statistics_proxy.h"
#include "webrtc/video/transport_adapter.h"
#include "webrtc/video/video_capture_input.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"

namespace webrtc {

class ChannelGroup;
class CpuOveruseObserver;
class ProcessThread;
class ViEChannel;
class ViEEncoder;

namespace internal {

class VideoSendStream : public webrtc::VideoSendStream {
 public:
  VideoSendStream(CpuOveruseObserver* overuse_observer,
                  int num_cpu_cores,
                  ProcessThread* module_process_thread,
                  ChannelGroup* channel_group,
                  int channel_id,
                  const VideoSendStream::Config& config,
                  const VideoEncoderConfig& encoder_config,
                  const std::map<uint32_t, RtpState>& suspended_ssrcs);

  ~VideoSendStream() override;

  // webrtc::SendStream implementation.
  void Start() override;
  void Stop() override;
  void SignalNetworkState(NetworkState state) override;
  bool DeliverRtcp(const uint8_t* packet, size_t length) override;

  // webrtc::VideoSendStream implementation.
  VideoCaptureInput* Input() override;
  bool ReconfigureVideoEncoder(const VideoEncoderConfig& config) override;
  Stats GetStats() override;

  typedef std::map<uint32_t, RtpState> RtpStateMap;
  RtpStateMap GetRtpStates() const;

  int64_t GetRtt() const;

 private:
  bool SetSendCodec(VideoCodec video_codec);
  void ConfigureSsrcs();
  TransportAdapter transport_adapter_;
  EncodedFrameCallbackAdapter encoded_frame_proxy_;
  const VideoSendStream::Config config_;
  VideoEncoderConfig encoder_config_;
  std::map<uint32_t, RtpState> suspended_ssrcs_;

  ProcessThread* const module_process_thread_;
  ChannelGroup* const channel_group_;
  const int channel_id_;

  rtc::scoped_ptr<VideoCaptureInput> input_;
  ViEChannel* vie_channel_;
  ViEEncoder* vie_encoder_;

  // Used as a workaround to indicate that we should be using the configured
  // start bitrate initially, instead of the one reported by VideoEngine (which
  // defaults to too high).
  bool use_config_bitrate_;

  SendStatisticsProxy stats_proxy_;
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIDEO_SEND_STREAM_H_
