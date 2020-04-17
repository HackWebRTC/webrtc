/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_
#define VIDEO_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/video/video_adaptation_counters.h"
#include "api/video/video_adaptation_reason.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "api/video/video_stream_encoder_observer.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_config.h"
#include "call/adaptation/resource.h"
#include "call/adaptation/resource_adaptation_processor_interface.h"
#include "call/adaptation/video_stream_adapter.h"
#include "call/adaptation/video_stream_input_state_provider.h"
#include "rtc_base/experiments/quality_rampup_experiment.h"
#include "rtc_base/experiments/quality_scaler_settings.h"
#include "rtc_base/strings/string_builder.h"
#include "system_wrappers/include/clock.h"
#include "video/adaptation/encode_usage_resource.h"
#include "video/adaptation/overuse_frame_detector.h"
#include "video/adaptation/quality_scaler_resource.h"

namespace webrtc {

// The assumed input frame size if we have not yet received a frame.
// TODO(hbos): This is 144p - why are we assuming super low quality? Seems like
// a bad heuristic.
extern const int kDefaultInputPixelsWidth;
extern const int kDefaultInputPixelsHeight;

// This class is used by the VideoStreamEncoder and is responsible for adapting
// resolution up or down based on encode usage percent. It keeps track of video
// source settings, adaptation counters and may get influenced by
// VideoStreamEncoder's quality scaler through AdaptUp() and AdaptDown() calls.
//
// This class is single-threaded. The caller is responsible for ensuring safe
// usage.
// TODO(hbos): Add unittests specific to this class, it is currently only tested
// indirectly in video_stream_encoder_unittest.cc and other tests exercising
// VideoStreamEncoder.
class ResourceAdaptationProcessor : public ResourceAdaptationProcessorInterface,
                                    public ResourceListener {
 public:
  // The processor can be constructed on any sequence, but must be initialized
  // and used on a single sequence, e.g. the encoder queue.
  ResourceAdaptationProcessor(
      VideoStreamInputStateProvider* input_state_provider,
      Clock* clock,
      bool experiment_cpu_load_estimator,
      std::unique_ptr<OveruseFrameDetector> overuse_detector,
      VideoStreamEncoderObserver* encoder_stats_observer,
      ResourceAdaptationProcessorListener* adaptation_listener);
  ~ResourceAdaptationProcessor() override;

  DegradationPreference degradation_preference() const {
    return degradation_preference_;
  }
  DegradationPreference effective_degradation_preference() const {
    return effective_degradation_preference_;
  }

  // ResourceAdaptationProcessorInterface implementation.
  void StartResourceAdaptation(
      ResourceAdaptationProcessorListener* adaptation_listener) override;
  void StopResourceAdaptation() override;
  // Uses a default AdaptReason of kCpu.
  void AddResource(Resource* resource) override;
  void AddResource(Resource* resource, VideoAdaptationReason reason);
  void SetDegradationPreference(
      DegradationPreference degradation_preference) override;

  // Settings that affect the VideoStreamEncoder-specific resources.
  void SetEncoderSettings(EncoderSettings encoder_settings);
  void SetStartBitrate(DataRate start_bitrate);
  void SetTargetBitrate(DataRate target_bitrate);
  void SetEncoderRates(
      const VideoEncoder::RateControlParameters& encoder_rates);
  // TODO(https://crbug.com/webrtc/11338): This can be made private if we
  // configure on SetDegredationPreference and SetEncoderSettings.
  void ConfigureQualityScaler(const VideoEncoder::EncoderInfo& encoder_info);

  // Methods corresponding to different points in the encoding pipeline.
  void OnFrameDroppedDueToSize();
  void OnMaybeEncodeFrame();
  void OnEncodeStarted(const VideoFrame& cropped_frame,
                       int64_t time_when_first_seen_us);
  void OnEncodeCompleted(const EncodedImage& encoded_image,
                         int64_t time_sent_in_us,
                         absl::optional<int> encode_duration_us);
  void OnFrameDropped(EncodedImageCallback::DropReason reason);
  // If true, the VideoStreamEncoder should eexecute its logic to maybe drop
  // frames baseed on size and bitrate.
  bool DropInitialFrames() const;

  // ResourceUsageListener implementation.
  ResourceListenerResponse OnResourceUsageStateMeasured(
      const Resource& resource) override;

  // For reasons of adaptation and statistics, we not only count the total
  // number of adaptations, but we also count the number of adaptations per
  // reason.
  // This method takes the new total number of adaptations and allocates that to
  // the "active" count - number of adaptations for the current reason.
  // The "other" count is the number of adaptations for the other reason.
  // This must be called for each adaptation step made.
  static void OnAdaptationCountChanged(
      const VideoAdaptationCounters& adaptation_count,
      VideoAdaptationCounters* active_count,
      VideoAdaptationCounters* other_active);

 private:
  class InitialFrameDropper;

  enum class State { kStopped, kStarted };

  bool HasSufficientInputForAdaptation(
      const VideoStreamInputState& input_state) const;
  VideoAdaptationReason GetReasonFromResource(const Resource& resource) const;

  // Performs the adaptation by getting the next target, applying it and
  // informing listeners of the new VideoSourceRestriction and adapt counters.
  void OnResourceUnderuse(const Resource& reason_resource);
  ResourceListenerResponse OnResourceOveruse(const Resource& reason_resource);

  CpuOveruseOptions GetCpuOveruseOptions() const;
  int LastInputFrameSizeOrDefault() const;

  // Reinterprets "balanced + screenshare" as "maintain-resolution".
  // When screensharing, as far as ResourceAdaptationProcessor logic is
  // concerned, we ALWAYS use "maintain-resolution". However, on a different
  // layer we may cap the video resolution to 720p to make high fps
  // screensharing feasible. This means that on the API layer the preference is
  // "balanced" (allowing reduction in both resolution and frame rate) but on
  // this layer (not responsible for caping to 720p) the preference is the same
  // as "maintain-resolution".
  void MaybeUpdateEffectiveDegradationPreference();
  // Makes |video_source_restrictions_| up-to-date and informs the
  // |adaptation_listener_| if restrictions are changed, allowing the listener
  // to reconfigure the source accordingly.
  void MaybeUpdateVideoSourceRestrictions();
  // Calculates an up-to-date value of the target frame rate and informs the
  // |encode_usage_resource_| of the new value.
  void MaybeUpdateTargetFrameRate();

  // Use nullopt to disable quality scaling.
  void UpdateQualityScalerSettings(
      absl::optional<VideoEncoder::QpThresholds> qp_thresholds);

  void UpdateAdaptationStats(VideoAdaptationReason reason);
  void UpdateStatsAdaptationSettings() const;

  // Checks to see if we should execute the quality rampup experiment. The
  // experiment resets all video restrictions at the start of the call in the
  // case the bandwidth estimate is high enough.
  // TODO(https://crbug.com/webrtc/11222) Move experiment details into an inner
  // class.
  void MaybePerformQualityRampupExperiment();
  void ResetVideoSourceRestrictions();

  void ResetActiveCounts();
  std::string ActiveCountsToString() const;

  // Does not trigger adaptations, only prevents adapting up based on
  // |active_counts_|.
  class PreventAdaptUpDueToActiveCounts final : public Resource {
   public:
    explicit PreventAdaptUpDueToActiveCounts(
        ResourceAdaptationProcessor* processor);
    ~PreventAdaptUpDueToActiveCounts() override = default;

    std::string name() const override {
      return "PreventAdaptUpDueToActiveCounts";
    }

    bool IsAdaptationUpAllowed(
        const VideoStreamInputState& input_state,
        const VideoSourceRestrictions& restrictions_before,
        const VideoSourceRestrictions& restrictions_after,
        const Resource& reason_resource) const override;

   private:
    ResourceAdaptationProcessor* processor_;
  } prevent_adapt_up_due_to_active_counts_;

  // Does not trigger adaptations, only prevents adapting up resolution.
  class PreventIncreaseResolutionDueToBitrateResource final : public Resource {
   public:
    explicit PreventIncreaseResolutionDueToBitrateResource(
        ResourceAdaptationProcessor* processor);
    ~PreventIncreaseResolutionDueToBitrateResource() override = default;

    std::string name() const override {
      return "PreventIncreaseResolutionDueToBitrateResource";
    }

    bool IsAdaptationUpAllowed(
        const VideoStreamInputState& input_state,
        const VideoSourceRestrictions& restrictions_before,
        const VideoSourceRestrictions& restrictions_after,
        const Resource& reason_resource) const override;

   private:
    ResourceAdaptationProcessor* processor_;
  } prevent_increase_resolution_due_to_bitrate_resource_;

  // Does not trigger adaptations, only prevents adapting up in BALANCED.
  class PreventAdaptUpInBalancedResource final : public Resource {
   public:
    explicit PreventAdaptUpInBalancedResource(
        ResourceAdaptationProcessor* processor);
    ~PreventAdaptUpInBalancedResource() override = default;

    std::string name() const override {
      return "PreventAdaptUpInBalancedResource";
    }

    bool IsAdaptationUpAllowed(
        const VideoStreamInputState& input_state,
        const VideoSourceRestrictions& restrictions_before,
        const VideoSourceRestrictions& restrictions_after,
        const Resource& reason_resource) const override;

   private:
    ResourceAdaptationProcessor* processor_;
  } prevent_adapt_up_in_balanced_resource_;

  EncodeUsageResource encode_usage_resource_;
  QualityScalerResource quality_scaler_resource_;

  VideoStreamInputStateProvider* const input_state_provider_;
  ResourceAdaptationProcessorListener* const adaptation_listener_;
  Clock* clock_;
  State state_;
  const bool experiment_cpu_load_estimator_;
  // The restrictions that |adaptation_listener_| is informed of.
  VideoSourceRestrictions video_source_restrictions_;
  DegradationPreference degradation_preference_;
  DegradationPreference effective_degradation_preference_;
  // Keeps track of source restrictions that this adaptation processor outputs.
  const std::unique_ptr<VideoStreamAdapter> stream_adapter_;
  const std::unique_ptr<InitialFrameDropper> initial_frame_dropper_;
  const bool quality_scaling_experiment_enabled_;
  // This is the last non-zero target bitrate for the encoder.
  absl::optional<uint32_t> encoder_target_bitrate_bps_;
  absl::optional<VideoEncoder::RateControlParameters> encoder_rates_;
  bool quality_rampup_done_;
  QualityRampupExperiment quality_rampup_experiment_;
  absl::optional<EncoderSettings> encoder_settings_;
  VideoStreamEncoderObserver* const encoder_stats_observer_;

  // Ties a resource to a reason for statistical reporting. This AdaptReason is
  // also used by this module to make decisions about how to adapt up/down.
  struct ResourceAndReason {
    ResourceAndReason(Resource* resource, VideoAdaptationReason reason)
        : resource(resource), reason(reason) {}
    virtual ~ResourceAndReason() = default;

    Resource* const resource;
    const VideoAdaptationReason reason;
  };
  std::vector<ResourceAndReason> resources_;
  // One AdaptationCounter for each reason, tracking the number of times we have
  // adapted for each reason. The sum of active_counts_ MUST always equal the
  // total adaptation provided by the VideoSourceRestrictions.
  // TODO(https://crbug.com/webrtc/11392): Move all active count logic to
  // encoder_stats_observer_; Counters used for deciding if the video resolution
  // or framerate is currently restricted, and if so, why, on a per degradation
  // preference basis.
  std::unordered_map<VideoAdaptationReason, VideoAdaptationCounters>
      active_counts_;
};

}  // namespace webrtc

#endif  // VIDEO_ADAPTATION_RESOURCE_ADAPTATION_PROCESSOR_H_
