/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef WEBRTC_CALL_H_
#define WEBRTC_CALL_H_

#include <string>
#include <vector>

#include "webrtc/common_types.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

class VoiceEngine;

const char* Version();

class PacketReceiver {
 public:
  enum DeliveryStatus {
    DELIVERY_OK,
    DELIVERY_UNKNOWN_SSRC,
    DELIVERY_PACKET_ERROR,
  };

  virtual DeliveryStatus DeliverPacket(const uint8_t* packet,
                                       size_t length) = 0;

 protected:
  virtual ~PacketReceiver() {}
};

// Callback interface for reporting when a system overuse is detected.
// The detection is based on the jitter of incoming captured frames.
class OveruseCallback {
 public:
  // Called as soon as an overuse is detected.
  virtual void OnOveruse() = 0;
  // Called periodically when the system is not overused any longer.
  virtual void OnNormalUse() = 0;

 protected:
  virtual ~OveruseCallback() {}
};

// A Call instance can contain several send and/or receive streams. All streams
// are assumed to have the same remote endpoint and will share bitrate estimates
// etc.
class Call {
 public:
  struct Config {
    explicit Config(newapi::Transport* send_transport)
        : webrtc_config(NULL),
          send_transport(send_transport),
          voice_engine(NULL),
          overuse_callback(NULL),
          start_bitrate_bps(-1) {}

    webrtc::Config* webrtc_config;

    newapi::Transport* send_transport;

    // VoiceEngine used for audio/video synchronization for this Call.
    VoiceEngine* voice_engine;

    // Callback for overuse and normal usage based on the jitter of incoming
    // captured frames. 'NULL' disables the callback.
    OveruseCallback* overuse_callback;

    // Start bitrate used before a valid bitrate estimate is calculated. '-1'
    // lets the call decide start bitrate.
    // Note: This currently only affects video.
    int start_bitrate_bps;
  };

  static Call* Create(const Call::Config& config);

  static Call* Create(const Call::Config& config,
                      const webrtc::Config& webrtc_config);

  virtual VideoSendStream* CreateVideoSendStream(
      const VideoSendStream::Config& config,
      const std::vector<VideoStream>& video_streams,
      const void* encoder_settings) = 0;

  virtual void DestroyVideoSendStream(VideoSendStream* send_stream) = 0;

  virtual VideoReceiveStream* CreateVideoReceiveStream(
      const VideoReceiveStream::Config& config) = 0;
  virtual void DestroyVideoReceiveStream(
      VideoReceiveStream* receive_stream) = 0;

  // All received RTP and RTCP packets for the call should be inserted to this
  // PacketReceiver. The PacketReceiver pointer is valid as long as the
  // Call instance exists.
  virtual PacketReceiver* Receiver() = 0;

  // Returns the estimated total send bandwidth. Note: this can differ from the
  // actual encoded bitrate.
  virtual uint32_t SendBitrateEstimate() = 0;

  // Returns the total estimated receive bandwidth for the call. Note: this can
  // differ from the actual receive bitrate.
  virtual uint32_t ReceiveBitrateEstimate() = 0;

  virtual ~Call() {}
};
}  // namespace webrtc

#endif  // WEBRTC_CALL_H_
