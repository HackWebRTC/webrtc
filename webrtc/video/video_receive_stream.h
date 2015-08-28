/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIDEO_RECEIVE_STREAM_H_
#define WEBRTC_VIDEO_VIDEO_RECEIVE_STREAM_H_

#include <vector>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/call.h"
#include "webrtc/common_video/interface/incoming_video_stream.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/video_render/include/video_render_defines.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/video/encoded_frame_callback_adapter.h"
#include "webrtc/video/receive_statistics_proxy.h"
#include "webrtc/video/transport_adapter.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/video_engine/vie_channel_group.h"
#include "webrtc/video_engine/vie_encoder.h"
#include "webrtc/video_receive_stream.h"

namespace webrtc {

class VoiceEngine;

namespace internal {

class VideoReceiveStream : public webrtc::VideoReceiveStream,
                           public I420FrameCallback,
                           public VideoRenderCallback {
 public:
  VideoReceiveStream(int num_cpu_cores,
                     ChannelGroup* channel_group,
                     int channel_id,
                     const VideoReceiveStream::Config& config,
                     webrtc::VoiceEngine* voice_engine);
  ~VideoReceiveStream() override;

  // webrtc::ReceiveStream implementation.
  void Start() override;
  void Stop() override;
  void SignalNetworkState(NetworkState state) override;
  bool DeliverRtcp(const uint8_t* packet, size_t length) override;
  bool DeliverRtp(const uint8_t* packet, size_t length) override;

  // webrtc::VideoReceiveStream implementation.
  webrtc::VideoReceiveStream::Stats GetStats() const override;

  // Overrides I420FrameCallback.
  void FrameCallback(VideoFrame* video_frame) override;

  // Overrides VideoRenderCallback.
  int RenderFrame(const uint32_t /*stream_id*/,
                  const VideoFrame& video_frame) override;

  const Config& config() const { return config_; }

  void SetSyncChannel(VoiceEngine* voice_engine, int audio_channel_id);

 private:
  void SetRtcpMode(newapi::RtcpMode mode);

  TransportAdapter transport_adapter_;
  EncodedFrameCallbackAdapter encoded_frame_proxy_;
  const VideoReceiveStream::Config config_;
  Clock* const clock_;

  ChannelGroup* const channel_group_;
  const int channel_id_;

  ViEChannel* vie_channel_;
  rtc::scoped_ptr<IncomingVideoStream> incoming_video_stream_;

  rtc::scoped_ptr<ReceiveStatisticsProxy> stats_proxy_;
};
}  // namespace internal
}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIDEO_RECEIVE_STREAM_H_
