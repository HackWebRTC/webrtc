/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_ADAPTATION_VIDEO_STREAM_ENCODER_RESOURCE_MANAGER_H_
#define VIDEO_ADAPTATION_VIDEO_STREAM_ENCODER_RESOURCE_MANAGER_H_

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

// Owns adaptation-related Resources pertaining to a single VideoStreamEncoder
// and passes on the relevant input from the encoder to the resources. The
// resources provide resource usage states to the ResourceAdaptationProcessor
// which is responsible for reconfiguring streams in order not to overuse
// resources.
//
// The manager is also involved with various mitigations not part of the
// ResourceAdaptationProcessor code such as the inital frame dropping.
class VideoStreamEncoderResourceManager
    : public ResourceAdaptationProcessorListener {
 public:
  VideoStreamEncoderResourceManager(
      VideoStreamInputStateProvider* input_state_provider,
      ResourceAdaptationProcessorInterface* adaptation_processor,
      VideoStreamEncoderObserver* encoder_stats_observer,
      Clock* clock,
      bool experiment_cpu_load_estimator,
      std::unique_ptr<OveruseFrameDetector> overuse_detector);
  ~VideoStreamEncoderResourceManager() override;

  void SetDegradationPreferences(
      DegradationPreference degradation_preference,
      DegradationPreference effective_degradation_preference);

  // Starts the encode usage resource. The quality scaler resource is
  // automatically started on being configured.
  void StartEncodeUsageResource();
  // Stops the encode usage and quality scaler resources if not already stopped.
  void StopManagedResources();

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

  // Resources need to be mapped to an AdaptReason (kCpu or kQuality) in order
  // to be able to update |active_counts_|, which is used...
  // - Legacy getStats() purposes.
  // - Preventing adapting up in some circumstances (which may be questionable).
  // TODO(hbos): Can we get rid of this?
  void MapResourceToReason(Resource* resource, VideoAdaptationReason reason);
  std::vector<Resource*> MappedResources() const;
  QualityScalerResource* quality_scaler_resource_for_testing();
  // If true, the VideoStreamEncoder should eexecute its logic to maybe drop
  // frames baseed on size and bitrate.
  bool DropInitialFrames() const;

  // ResourceAdaptationProcessorListener implementation.
  // Updates |video_source_restrictions_| and |active_counts_|.
  void OnVideoSourceRestrictionsUpdated(
      VideoSourceRestrictions restrictions,
      const VideoAdaptationCounters& adaptation_counters,
      const Resource* reason) override;

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

  VideoAdaptationReason GetReasonFromResource(const Resource& resource) const;

  CpuOveruseOptions GetCpuOveruseOptions() const;
  int LastInputFrameSizeOrDefault() const;

  // Makes |video_source_restrictions_| up-to-date and informs the
  // |adaptation_listener_| if restrictions are changed, allowing the listener
  // to reconfigure the source accordingly.
  void MaybeUpdateVideoSourceRestrictions(const Resource* reason_resource);
  // Calculates an up-to-date value of the target frame rate and informs the
  // |encode_usage_resource_| of the new value.
  void MaybeUpdateTargetFrameRate();

  // Use nullopt to disable quality scaling.
  void UpdateQualityScalerSettings(
      absl::optional<VideoEncoder::QpThresholds> qp_thresholds);

  void UpdateAdaptationStats(const VideoAdaptationCounters& total_counts,
                             VideoAdaptationReason reason);
  void UpdateStatsAdaptationSettings() const;

  // Checks to see if we should execute the quality rampup experiment. The
  // experiment resets all video restrictions at the start of the call in the
  // case the bandwidth estimate is high enough.
  // TODO(https://crbug.com/webrtc/11222) Move experiment details into an inner
  // class.
  void MaybePerformQualityRampupExperiment();

  void ResetActiveCounts();
  std::string ActiveCountsToString() const;

  // TODO(hbos): Consider moving all of the manager's resources into separate
  // files for testability.

  // Does not trigger adaptations, only prevents adapting up based on
  // |active_counts_|.
  class PreventAdaptUpDueToActiveCounts final : public Resource {
   public:
    explicit PreventAdaptUpDueToActiveCounts(
        VideoStreamEncoderResourceManager* manager);
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
    VideoStreamEncoderResourceManager* manager_;
  } prevent_adapt_up_due_to_active_counts_;

  // Does not trigger adaptations, only prevents adapting up resolution.
  class PreventIncreaseResolutionDueToBitrateResource final : public Resource {
   public:
    explicit PreventIncreaseResolutionDueToBitrateResource(
        VideoStreamEncoderResourceManager* manager);
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
    VideoStreamEncoderResourceManager* manager_;
  } prevent_increase_resolution_due_to_bitrate_resource_;

  // Does not trigger adaptations, only prevents adapting up in BALANCED.
  class PreventAdaptUpInBalancedResource final : public Resource {
   public:
    explicit PreventAdaptUpInBalancedResource(
        VideoStreamEncoderResourceManager* manager);
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
    VideoStreamEncoderResourceManager* manager_;
  } prevent_adapt_up_in_balanced_resource_;

  EncodeUsageResource encode_usage_resource_;
  QualityScalerResource quality_scaler_resource_;

  VideoStreamInputStateProvider* const input_state_provider_;
  ResourceAdaptationProcessorInterface* const adaptation_processor_;
  VideoStreamEncoderObserver* const encoder_stats_observer_;

  DegradationPreference degradation_preference_;
  DegradationPreference effective_degradation_preference_;
  VideoSourceRestrictions video_source_restrictions_;

  const BalancedDegradationSettings balanced_settings_;
  Clock* clock_;
  const bool experiment_cpu_load_estimator_;
  const std::unique_ptr<InitialFrameDropper> initial_frame_dropper_;
  const bool quality_scaling_experiment_enabled_;
  absl::optional<uint32_t> encoder_target_bitrate_bps_;
  absl::optional<VideoEncoder::RateControlParameters> encoder_rates_;
  bool quality_rampup_done_;
  QualityRampupExperiment quality_rampup_experiment_;
  absl::optional<EncoderSettings> encoder_settings_;

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

#endif  // VIDEO_ADAPTATION_VIDEO_STREAM_ENCODER_RESOURCE_MANAGER_H_
