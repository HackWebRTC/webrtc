/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_VIDEO_CODING_CODECS_VP8_INCLUDE_VP8_TEMPORAL_LAYERS_H_
#define MODULES_VIDEO_CODING_CODECS_VP8_INCLUDE_VP8_TEMPORAL_LAYERS_H_

#include <memory>
#include <vector>

namespace webrtc {

// Some notes on the prerequisites of the TemporalLayers interface.
// * Implementations of TemporalLayers may not contain internal synchronization
//   so caller must make sure doing so thread safe.
// * The encoder is assumed to encode all frames in order, and callbacks to
//   PopulateCodecSpecific() / FrameEncoded() must happen in the same order.
//
// This means that in the case of pipelining encoders, it is OK to have a chain
// of calls such as this:
// - UpdateLayerConfig(timestampA)
// - UpdateLayerConfig(timestampB)
// - PopulateCodecSpecific(timestampA, ...)
// - UpdateLayerConfig(timestampC)
// - FrameEncoded(timestampA, 1234, ...)
// - FrameEncoded(timestampB, 0, ...)
// - PopulateCodecSpecific(timestampC, ...)
// - FrameEncoded(timestampC, 1234, ...)
// Note that UpdateLayerConfig() for a new frame can happen before
// FrameEncoded() for a previous one, but calls themselves must be both
// synchronized (e.g. run on a task queue) and in order (per type).

enum class TemporalLayersType { kFixedPattern, kBitrateDynamic };

struct CodecSpecificInfoVP8;
enum class Vp8BufferReference : uint8_t {
  kNone = 0,
  kLast = 1,
  kGolden = 2,
  kAltref = 4
};

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

// This interface defines a way of getting the encoder settings needed to
// realize a temporal layer structure of predefined size.
class TemporalLayers {
 public:
  enum BufferFlags : int {
    kNone = 0,
    kReference = 1,
    kUpdate = 2,
    kReferenceAndUpdate = kReference | kUpdate,
  };
  enum FreezeEntropy { kFreezeEntropy };

  struct FrameConfig {
    FrameConfig();

    FrameConfig(BufferFlags last, BufferFlags golden, BufferFlags arf);
    FrameConfig(BufferFlags last,
                BufferFlags golden,
                BufferFlags arf,
                FreezeEntropy);

    bool drop_frame;
    BufferFlags last_buffer_flags;
    BufferFlags golden_buffer_flags;
    BufferFlags arf_buffer_flags;

    // The encoder layer ID is used to utilize the correct bitrate allocator
    // inside the encoder. It does not control references nor determine which
    // "actual" temporal layer this is. The packetizer temporal index determines
    // which layer the encoded frame should be packetized into.
    // Normally these are the same, but current temporal-layer strategies for
    // screenshare use one bitrate allocator for all layers, but attempt to
    // packetize / utilize references to split a stream into multiple layers,
    // with different quantizer settings, to hit target bitrate.
    // TODO(pbos): Screenshare layers are being reconsidered at the time of
    // writing, we might be able to remove this distinction, and have a temporal
    // layer imply both (the normal case).
    int encoder_layer_id;
    int packetizer_temporal_idx;

    bool layer_sync;

    bool freeze_entropy;

    // Indicates in which order the encoder should search the reference buffers
    // when doing motion prediction. Set to kNone to use unspecified order. Any
    // buffer indicated here must not have the corresponding no_ref bit set.
    // If all three buffers can be reference, the one not listed here should be
    // searched last.
    Vp8BufferReference first_reference;
    Vp8BufferReference second_reference;

    bool operator==(const FrameConfig& o) const;
    bool operator!=(const FrameConfig& o) const { return !(*this == o); }

   private:
    FrameConfig(BufferFlags last,
                BufferFlags golden,
                BufferFlags arf,
                bool freeze_entropy);
  };

  // Factory for TemporalLayer strategy. Default behavior is a fixed pattern
  // of temporal layers. See default_temporal_layers.cc
  static std::unique_ptr<TemporalLayers> CreateTemporalLayers(
      TemporalLayersType type,
      int num_temporal_layers);

  virtual ~TemporalLayers() = default;

  virtual bool SupportsEncoderFrameDropping() const = 0;

  // New target bitrate, per temporal layer.
  virtual void OnRatesUpdated(const std::vector<uint32_t>& bitrates_bps,
                              int framerate_fps) = 0;

  // Update the encoder configuration with target bitrates or other parameters.
  // Returns true iff the configuration was actually modified.
  virtual bool UpdateConfiguration(Vp8EncoderConfig* cfg) = 0;

  // Returns the recommended VP8 encode flags needed, and moves the temporal
  // pattern to the next frame.
  // The timestamp may be used as both a time and a unique identifier, and so
  // the caller must make sure no two frames use the same timestamp.
  // The timestamp uses a 90kHz RTP clock.
  // After calling this method, the actual encoder should be called with the
  // provided frame configuration, after which:
  // * On success, call PopulateCodecSpecific() and then FrameEncoded();
  // * On failure/ frame drop: Call FrameEncoded() with size = 0.
  virtual FrameConfig UpdateLayerConfig(uint32_t rtp_timestamp) = 0;

  // Called after successful encoding of a frame. The rtp timestamp must match
  // the one using in UpdateLayerConfig(). Some fields in |vp8_info| may have
  // already been populated by the encoder, check before overwriting.
  // |tl_config| is the frame config returned by UpdateLayerConfig() for this
  // rtp_timestamp;
  // If |is_keyframe| is true, the flags in |tl_config| will be ignored.
  virtual void PopulateCodecSpecific(
      bool is_keyframe,
      const TemporalLayers::FrameConfig& tl_config,
      CodecSpecificInfoVP8* vp8_info,
      uint32_t rtp_timestamp) = 0;

  // Called after an encode event. If the frame was dropped, |size_bytes| must
  // be set to 0. The rtp timestamp must match the one using in
  // UpdateLayerConfig()
  virtual void FrameEncoded(uint32_t rtp_timestamp,
                            size_t size_bytes,
                            int qp) = 0;
};

}  // namespace webrtc

#endif  // MODULES_VIDEO_CODING_CODECS_VP8_INCLUDE_VP8_TEMPORAL_LAYERS_H_
