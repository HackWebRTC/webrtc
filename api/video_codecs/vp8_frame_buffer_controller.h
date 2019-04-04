/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_VIDEO_CODECS_VP8_FRAME_BUFFER_CONTROLLER_H_
#define API_VIDEO_CODECS_VP8_FRAME_BUFFER_CONTROLLER_H_

#include <memory>
#include <vector>

#include "absl/types/optional.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/vp8_frame_config.h"

namespace webrtc {

// Some notes on the prerequisites of the TemporalLayers interface.
// * Vp8FrameBufferController is not thread safe, synchronization is the
//   caller's responsibility.
// * The encoder is assumed to encode all frames in order, and callbacks to
//   PopulateCodecSpecific() / FrameEncoded() must happen in the same order.
//
// This means that in the case of pipelining encoders, it is OK to have a chain
// of calls such as this:
// - UpdateLayerConfig(timestampA)
// - UpdateLayerConfig(timestampB)
// - PopulateCodecSpecific(timestampA, ...)
// - UpdateLayerConfig(timestampC)
// - OnEncodeDone(timestampA, 1234, ...)
// - UpdateLayerConfig(timestampC)
// - OnEncodeDone(timestampB, 0, ...)
// - OnEncodeDone(timestampC, 1234, ...)
// Note that UpdateLayerConfig() for a new frame can happen before
// FrameEncoded() for a previous one, but calls themselves must be both
// synchronized (e.g. run on a task queue) and in order (per type).

struct CodecSpecificInfo;

struct Vp8EncoderConfig {
  static constexpr size_t kMaxPeriodicity = 16;
  static constexpr size_t kMaxLayers = 5;

  // Number of active temporal layers. Set to 0 if not used.
  uint32_t ts_number_layers;
  // Arrays of length |ts_number_layers|, indicating (cumulative) target bitrate
  // and rate decimator (e.g. 4 if every 4th frame is in the given layer) for
  // each active temporal layer, starting with temporal id 0.
  uint32_t ts_target_bitrate[kMaxLayers];
  uint32_t ts_rate_decimator[kMaxLayers];

  // The periodicity of the temporal pattern. Set to 0 if not used.
  uint32_t ts_periodicity;
  // Array of length |ts_periodicity| indicating the sequence of temporal id's
  // to assign to incoming frames.
  uint32_t ts_layer_id[kMaxPeriodicity];

  // Target bitrate, in bps.
  uint32_t rc_target_bitrate;

  // Clamp QP to min/max. Use 0 to disable clamping.
  uint32_t rc_min_quantizer;
  uint32_t rc_max_quantizer;
};

// This interface defines a way of delegating the logic of buffer management.
// Multiple streams may be controlled by a single controller, demuxing between
// them using stream_index.
class Vp8FrameBufferController {
 public:
  virtual ~Vp8FrameBufferController() = default;

  // Number of streamed controlled by |this|.
  virtual size_t StreamCount() const = 0;

  // If this method returns true, the encoder is free to drop frames for
  // instance in an effort to uphold encoding bitrate.
  // If this return false, the encoder must not drop any frames unless:
  //  1. Requested to do so via Vp8FrameConfig.drop_frame
  //  2. The frame to be encoded is requested to be a keyframe
  //  3. The encoded detected a large overshoot and decided to drop and then
  //     re-encode the image at a low bitrate. In this case the encoder should
  //     call OnEncodeDone() once with size = 0 to indicate drop, and then call
  //     OnEncodeDone() again when the frame has actually been encoded.
  virtual bool SupportsEncoderFrameDropping(size_t stream_index) const = 0;

  // New target bitrate, per temporal layer.
  virtual void OnRatesUpdated(size_t stream_index,
                              const std::vector<uint32_t>& bitrates_bps,
                              int framerate_fps) = 0;

  // Called by the encoder before encoding a frame. |cfg| contains the current
  // configuration. If the TemporalLayers instance wishes any part of that
  // to be changed before the encode step, |cfg| should be changed and then
  // return true. If false is returned, the encoder will proceed without
  // updating the configuration.
  virtual bool UpdateConfiguration(size_t stream_index,
                                   Vp8EncoderConfig* cfg) = 0;

  // Returns the recommended VP8 encode flags needed, and moves the temporal
  // pattern to the next frame.
  // The timestamp may be used as both a time and a unique identifier, and so
  // the caller must make sure no two frames use the same timestamp.
  // The timestamp uses a 90kHz RTP clock.
  // After calling this method, first call the actual encoder with the provided
  // frame configuration, and then OnEncodeDone() below.
  virtual Vp8FrameConfig UpdateLayerConfig(size_t stream_index,
                                           uint32_t rtp_timestamp) = 0;

  // Called after the encode step is done. |rtp_timestamp| must match the
  // parameter use in the UpdateLayerConfig() call.
  // |is_keyframe| must be true iff the encoder decided to encode this frame as
  // a keyframe.
  // If the encoder decided to drop this frame, |size_bytes| must be set to 0,
  // otherwise it should indicate the size in bytes of the encoded frame.
  // If |size_bytes| > 0, and |info| is not null, the TemporalLayers
  // instance my update |info| with codec specific data such as temporal id.
  // Some fields of this struct may have already been populated by the encoder,
  // check before overwriting.
  // If |size_bytes| > 0, |qp| should indicate the frame-level QP this frame was
  // encoded at. If the encoder does not support extracting this, |qp| should be
  // set to 0.
  virtual void OnEncodeDone(size_t stream_index,
                            uint32_t rtp_timestamp,
                            size_t size_bytes,
                            bool is_keyframe,
                            int qp,
                            CodecSpecificInfo* info) = 0;

  // Called by the encoder when the packet loss rate changes.
  // |packet_loss_rate| runs between 0.0 (no loss) and 1.0 (everything lost).
  virtual void OnPacketLossRateUpdate(float packet_loss_rate) = 0;

  // Called by the encoder when the round trip time changes.
  virtual void OnRttUpdate(int64_t rtt_ms) = 0;

  // Called when a loss notification is received.
  virtual void OnLossNotification(
      const VideoEncoder::LossNotification loss_notification) = 0;
};

// Interface for a factory of Vp8FrameBufferController instances.
class Vp8FrameBufferControllerFactory {
 public:
  virtual ~Vp8FrameBufferControllerFactory() = default;

  virtual std::unique_ptr<Vp8FrameBufferController> Create(
      const VideoCodec& codec) = 0;
};

}  // namespace webrtc

#endif  // API_VIDEO_CODECS_VP8_FRAME_BUFFER_CONTROLLER_H_
