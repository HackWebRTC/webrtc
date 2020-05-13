/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_ADAPTATION_VIDEO_STREAM_ADAPTER_H_
#define CALL_ADAPTATION_VIDEO_STREAM_ADAPTER_H_

#include <memory>

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/video/video_adaptation_counters.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/video_source_restrictions.h"
#include "call/adaptation/video_stream_input_state.h"
#include "modules/video_coding/utility/quality_scaler.h"
#include "rtc_base/experiments/balanced_degradation_settings.h"

namespace webrtc {

class VideoStreamAdapter;

extern const int kMinFrameRateFps;

VideoSourceRestrictions FilterRestrictionsByDegradationPreference(
    VideoSourceRestrictions source_restrictions,
    DegradationPreference degradation_preference);

VideoAdaptationCounters FilterVideoAdaptationCountersByDegradationPreference(
    VideoAdaptationCounters counters,
    DegradationPreference degradation_preference);

int GetHigherResolutionThan(int pixel_count);

// Represents one step that the VideoStreamAdapter can take when adapting the
// VideoSourceRestrictions up or down. Or, if adaptation is not valid, provides
// a Status code indicating the reason for not adapting.
class Adaptation final {
 public:
  enum class Status {
    // Applying this adaptation will have an effect. All other Status codes
    // indicate that adaptation is not possible and why.
    kValid,
    // Cannot adapt. The minimum or maximum adaptation has already been reached.
    // There are no more steps to take.
    kLimitReached,
    // Cannot adapt. The resolution or frame rate requested by a recent
    // adaptation has not yet been reflected in the input resolution or frame
    // rate; adaptation is refused to avoid "double-adapting".
    kAwaitingPreviousAdaptation,
  };

  // The status of this Adaptation. To find out how this Adaptation affects
  // VideoSourceRestrictions, see VideoStreamAdapter::PeekNextRestrictions().
  Status status() const;
  // Used for stats reporting.
  bool min_pixel_limit_reached() const;

 private:
  // The adapter needs to know about step type and step target in order to
  // construct and perform an Adaptation, which is a detail we do not want to
  // expose to the public interface.
  friend class VideoStreamAdapter;

  enum class StepType {
    kIncreaseResolution,
    kDecreaseResolution,
    kIncreaseFrameRate,
    kDecreaseFrameRate,
  };

  struct Step {
    Step(StepType type, int target);
    const StepType type;
    const int target;  // Pixel or frame rate depending on |type|.
  };

  // Constructs with a valid adaptation Step. Status is kValid.
  Adaptation(int validation_id, Step step);
  Adaptation(int validation_id, Step step, bool min_pixel_limit_reached);
  // Constructor when adaptation is not valid. Status MUST NOT be kValid.
  Adaptation(int validation_id, Status invalid_status);
  Adaptation(int validation_id,
             Status invalid_status,
             bool min_pixel_limit_reached);

  const Step& step() const;  // Only callable if |status_| is kValid.

  // An Adaptation can become invalidated if the state of VideoStreamAdapter is
  // modified before the Adaptation is applied. To guard against this, this ID
  // has to match VideoStreamAdapter::adaptation_validation_id_ when applied.
  const int validation_id_;
  const Status status_;
  const absl::optional<Step> step_;  // Only present if |status_| is kValid.
  const bool min_pixel_limit_reached_;
};

// Owns the VideoSourceRestriction for a single stream and is responsible for
// adapting it up or down when told to do so. This class serves the following
// purposes:
// 1. Keep track of a stream's restrictions.
// 2. Provide valid ways to adapt up or down the stream's restrictions.
// 3. Modify the stream's restrictions in one of the valid ways.
class VideoStreamAdapter {
 public:
  VideoStreamAdapter();
  ~VideoStreamAdapter();

  VideoSourceRestrictions source_restrictions() const;
  const VideoAdaptationCounters& adaptation_counters() const;
  void ClearRestrictions();

  // TODO(hbos): Setting the degradation preference should not clear
  // restrictions! This is not defined in the spec and is unexpected, there is a
  // tiny risk that people would discover and rely on this behavior.
  void SetDegradationPreference(DegradationPreference degradation_preference);
  // The adaptaiton logic depends on these inputs.
  void SetInput(VideoStreamInputState input_state);

  // Returns an adaptation that we are guaranteed to be able to apply, or a
  // status code indicating the reason why we cannot adapt.
  Adaptation GetAdaptationUp() const;
  Adaptation GetAdaptationDown() const;
  // Returns the restrictions that result from applying the adaptation, without
  // actually applying it. If the adaptation is not valid, current restrictions
  // are returned.
  VideoSourceRestrictions PeekNextRestrictions(
      const Adaptation& adaptation) const;
  // Updates source_restrictions() based according to the Adaptation.
  void ApplyAdaptation(const Adaptation& adaptation);

 private:
  class VideoSourceRestrictor;

  // The input frame rate and resolution at the time of an adaptation in the
  // direction described by |mode_| (up or down).
  // TODO(https://crbug.com/webrtc/11393): Can this be renamed? Can this be
  // merged with AdaptationTarget?
  struct AdaptationRequest {
    // The pixel count produced by the source at the time of the adaptation.
    int input_pixel_count_;
    // Framerate received from the source at the time of the adaptation.
    int framerate_fps_;
    // Degradation preference for the request.
    Adaptation::StepType step_type_;
  };

  // Owner and modifier of the VideoSourceRestriction of this stream adaptor.
  const std::unique_ptr<VideoSourceRestrictor> source_restrictor_;
  // Decides the next adaptation target in DegradationPreference::BALANCED.
  const BalancedDegradationSettings balanced_settings_;
  // To guard against applying adaptations that have become invalidated, an
  // Adaptation that is applied has to have a matching validation ID.
  int adaptation_validation_id_;
  // When deciding the next target up or down, different strategies are used
  // depending on the DegradationPreference.
  // https://w3c.github.io/mst-content-hint/#dom-rtcdegradationpreference
  DegradationPreference degradation_preference_;
  VideoStreamInputState input_state_;
  // The input frame rate, resolution and adaptation direction of the last
  // ApplyAdaptationTarget(). Used to avoid adapting twice if a recent
  // adaptation has not had an effect on the input frame rate or resolution yet.
  // TODO(hbos): Can we implement a more general "cooldown" mechanism of
  // resources intead? If we already have adapted it seems like we should wait
  // a while before adapting again, so that we are not acting on usage
  // measurements that are made obsolete/unreliable by an "ongoing" adaptation.
  absl::optional<AdaptationRequest> last_adaptation_request_;
};

}  // namespace webrtc

#endif  // CALL_ADAPTATION_VIDEO_STREAM_ADAPTER_H_
