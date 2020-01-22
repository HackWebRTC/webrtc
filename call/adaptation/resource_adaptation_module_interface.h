/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_RESOURCE_ADAPTATION_MODULE_INTERFACE_H_
#define CALL_ADAPTATION_RESOURCE_ADAPTATION_MODULE_INTERFACE_H_

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/video/video_frame.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_config.h"
#include "call/adaptation/video_source_restrictions.h"

namespace webrtc {

// Information about an encoder available when reconfiguring the encoder.
class EncoderSettings {
 public:
  EncoderSettings(VideoEncoder::EncoderInfo encoder_info,
                  VideoEncoderConfig encoder_config,
                  VideoCodec video_codec);

  // Encoder capabilities, implementation info, etc.
  const VideoEncoder::EncoderInfo& encoder_info() const;
  // Configuration parameters, ultimately coming from the API and negotiation.
  const VideoEncoderConfig& encoder_config() const;
  // Lower level config, heavily based on the VideoEncoderConfig.
  const VideoCodec& video_codec() const;

 private:
  VideoEncoder::EncoderInfo encoder_info_;
  VideoEncoderConfig encoder_config_;
  VideoCodec video_codec_;
};

// The listener is responsible for carrying out the reconfiguration of the video
// source such that the VideoSourceRestrictions are fulfilled.
class ResourceAdaptationModuleListener {
 public:
  virtual ~ResourceAdaptationModuleListener();

  // TODO(hbos): When we support the muli-stream use case, the arguments need to
  // specify which video stream's source needs to be reconfigured.
  virtual void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions) = 0;
};

// Responsible for reconfiguring encoded streams based on resource consumption,
// such as scaling down resolution or frame rate when CPU is overused. This
// interface is meant to be injectable into VideoStreamEncoder.
//
// [UNDER CONSTRUCTION] This interface is work-in-progress. In the future it
// needs to be able to handle all the necessary input and output for resource
// adaptation decision making.
//
// TODO(https://crbug.com/webrtc/11222): Make this interface feature-complete so
// that a module (such as OveruseFrameDetectorResourceAdaptationModule) is fully
// operational through this abstract interface.
class ResourceAdaptationModuleInterface {
 public:
  virtual ~ResourceAdaptationModuleInterface();

  // TODO(hbos): When input/output of the module is adequetly handled by this
  // interface, these methods need to say which stream to start/stop, enabling
  // multi-stream aware implementations of ResourceAdaptationModuleInterface. We
  // don't want to do this before we have the right interfaces (e.g. if we pass
  // in a VideoStreamEncoder here directly then have a dependency on a different
  // build target). For the multi-stream use case we may consider making
  // ResourceAdaptationModuleInterface reference counted.
  virtual void StartResourceAdaptation(
      ResourceAdaptationModuleListener* adaptation_listener) = 0;
  virtual void StopResourceAdaptation() = 0;

  // The following methods are callable whether or not adaption is started.

  // Informs the module whether we have input video. By default, the module must
  // assume the value is false.
  virtual void SetHasInputVideo(bool has_input_video) = 0;
  virtual void SetDegradationPreference(
      DegradationPreference degradation_preference) = 0;
  virtual void SetEncoderSettings(EncoderSettings encoder_settings) = 0;
  virtual void SetEncoderTargetBitrate(
      absl::optional<uint32_t> target_bitrate_bps) = 0;
  // Removes all restrictions; the module will need to adapt all over again.
  // TODO(hbos): It's not clear why anybody should be able to tell the module to
  // reset like this; can we get rid of this method?
  virtual void ResetVideoSourceRestrictions() = 0;

  // The following methods correspond to the pipeline that a frame goes through.
  // Note that if the encoder is parallelized, multiple frames may be processed
  // in parallel and methods may be invoked in unexpected orders.
  //
  // The implementation must not retain VideoFrames. Doing so may keep video
  // frame buffers alive - this may even stall encoding.
  // TODO(hbos): Can we replace VideoFrame with a different struct, maybe width
  // and height is enough, and some sort of way to identify it at each step?

  // 1. A frame is delivered to the encoder, e.g. from the camera. Next up: it
  // may get dropped or it may get encoded, see OnFrameDroppedDueToSize() and
  // OnEncodeStarted().
  virtual void OnFrame(const VideoFrame& frame) = 0;
  // 2.i) An input frame was dropped because its resolution is too big (e.g. for
  // the target bitrate). This frame will not continue through the rest of the
  // pipeline. The module should adapt down in resolution to avoid subsequent
  // frames getting dropped for the same reason.
  // TODO(hbos): If we take frame rate into account perhaps it would be valid to
  // adapt down in frame rate as well.
  virtual void OnFrameDroppedDueToSize() = 0;
  // 2.ii) An input frame is about to be encoded. It may have been cropped and
  // have different dimensions than what was observed at OnFrame(). Next
  // up: encoding completes or fails, see OnEncodeCompleted(). There is
  // currently no signal for encode failure.
  virtual void OnEncodeStarted(const VideoFrame& cropped_frame,
                               int64_t time_when_first_seen_us) = 0;
  // 3. The frame has successfully completed encoding. Next up: The encoded
  // frame is dropped or packetized and sent over the network. There is
  // currently no signal what happens beyond this point.
  virtual void OnEncodeCompleted(uint32_t timestamp,
                                 int64_t time_sent_in_us,
                                 int64_t capture_time_us,
                                 absl::optional<int> encode_duration_us) = 0;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_RESOURCE_ADAPTATION_MODULE_INTERFACE_H_
