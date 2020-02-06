/*
 *  Copyright 2019 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VIDEO_OVERUSE_FRAME_DETECTOR_RESOURCE_ADAPTATION_MODULE_H_
#define VIDEO_OVERUSE_FRAME_DETECTOR_RESOURCE_ADAPTATION_MODULE_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "api/rtp_parameters.h"
#include "api/video/video_frame.h"
#include "api/video/video_source_interface.h"
#include "api/video/video_stream_encoder_observer.h"
#include "api/video_codecs/video_codec.h"
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_config.h"
#include "call/adaptation/resource_adaptation_module_interface.h"
#include "rtc_base/experiments/balanced_degradation_settings.h"
#include "rtc_base/experiments/quality_rampup_experiment.h"
#include "rtc_base/experiments/quality_scaler_settings.h"
#include "system_wrappers/include/clock.h"
#include "video/overuse_frame_detector.h"

namespace webrtc {

class VideoStreamEncoder;

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
// TODO(hbos): Create and implement an abstract interface
// ResourceAdaptationModuleInterface and make this class inherit it. Use the
// generic interface in VideoStreamEncoder, unblocking other modules from being
// implemented and used.
class OveruseFrameDetectorResourceAdaptationModule
    : public ResourceAdaptationModuleInterface {
 public:
  // The module can be constructed on any sequence, but must be initialized and
  // used on a single sequence, e.g. the encoder queue.
  OveruseFrameDetectorResourceAdaptationModule(
      Clock* clock,
      bool experiment_cpu_load_estimator,
      std::unique_ptr<OveruseFrameDetector> overuse_detector,
      VideoStreamEncoderObserver* encoder_stats_observer,
      ResourceAdaptationModuleListener* adaptation_listener);
  ~OveruseFrameDetectorResourceAdaptationModule() override;

  DegradationPreference degradation_preference() const {
    return degradation_preference_;
  }

  // ResourceAdaptationModuleInterface implementation.
  void StartResourceAdaptation(
      ResourceAdaptationModuleListener* adaptation_listener) override;
  void StopResourceAdaptation() override;
  void SetHasInputVideo(bool has_input_video) override;
  void SetDegradationPreference(
      DegradationPreference degradation_preference) override;
  void SetEncoderSettings(EncoderSettings encoder_settings) override;
  void SetStartBitrate(DataRate start_bitrate) override;
  void SetTargetBitrate(DataRate target_bitrate) override;
  void SetEncoderRates(
      const VideoEncoder::RateControlParameters& encoder_rates) override;

  void OnFrame(const VideoFrame& frame) override;
  void OnFrameDroppedDueToSize() override;
  void OnMaybeEncodeFrame() override;
  void OnEncodeStarted(const VideoFrame& cropped_frame,
                       int64_t time_when_first_seen_us) override;
  void OnEncodeCompleted(const EncodedImage& encoded_image,
                         int64_t time_sent_in_us,
                         absl::optional<int> encode_duration_us) override;
  void OnFrameDropped(EncodedImageCallback::DropReason reason) override;

  // TODO(hbos): Is dropping initial frames really just a special case of "don't
  // encode frames right now"? Can this be part of VideoSourceRestrictions,
  // which handles the output of the rest of the encoder settings? This is
  // something we'll need to support for "disable video due to overuse", not
  // initial frames.
  bool DropInitialFrames() const;

  // TODO(eshr): This can be made private if we configure on
  // SetDegredationPreference and SetEncoderSettings.
  // (https://crbug.com/webrtc/11338)
  void ConfigureQualityScaler(const VideoEncoder::EncoderInfo& encoder_info);

  // Signal that a resource (kCpu or kQuality) is overused or underused. This is
  // currently used by EncodeUsageResource, QualityScalerResource and testing.
  // TODO(https://crbug.com/webrtc/11222): Make use of ResourceUsageState and
  // implement resources per call/adaptation/resource.h. When adaptation happens
  // because a resource is in specific usage state, get rid of these explicit
  // triggers.
  void OnResourceUnderuse(AdaptationObserverInterface::AdaptReason reason);
  bool OnResourceOveruse(AdaptationObserverInterface::AdaptReason reason);

 private:
  class EncodeUsageResource;
  class QualityScalerResource;
  class VideoSourceRestrictor;
  class AdaptCounter;

  struct AdaptationRequest {
    // The pixel count produced by the source at the time of the adaptation.
    int input_pixel_count_;
    // Framerate received from the source at the time of the adaptation.
    int framerate_fps_;
    // Indicates if request was to adapt up or down.
    enum class Mode { kAdaptUp, kAdaptDown } mode_;
  };

  struct StartBitrate {
    bool has_seen_first_bwe_drop_ = false;
    DataRate set_start_bitrate_ = DataRate::Zero();
    int64_t set_start_bitrate_time_ms_ = 0;
  };

  CpuOveruseOptions GetCpuOveruseOptions() const;
  VideoCodecType GetVideoCodecTypeOrGeneric() const;
  int LastInputFrameSizeOrDefault() const;
  VideoStreamEncoderObserver::AdaptationSteps GetActiveCounts(
      AdaptationObserverInterface::AdaptReason reason);
  const AdaptCounter& GetConstAdaptCounter();

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

  void UpdateAdaptationStats(AdaptationObserverInterface::AdaptReason reason);
  DegradationPreference EffectiveDegradataionPreference();
  AdaptCounter& GetAdaptCounter();
  bool CanAdaptUpResolution(int pixels, uint32_t bitrate_bps) const;

  // Checks to see if we should execute the quality rampup experiment. The
  // experiment resets all video restrictions at the start of the call in the
  // case the bandwidth estimate is high enough.
  // TODO(https://crbug.com/webrtc/11222) Move experiment details into an inner
  // class.
  void MaybePerformQualityRampupExperiment();
  void ResetVideoSourceRestrictions();

  ResourceAdaptationModuleListener* const adaptation_listener_;
  Clock* clock_;
  const bool experiment_cpu_load_estimator_;
  // The restrictions that |adaptation_listener_| is informed of.
  VideoSourceRestrictions video_source_restrictions_;
  bool has_input_video_;
  DegradationPreference degradation_preference_;
  // Counters used for deciding if the video resolution or framerate is
  // currently restricted, and if so, why, on a per degradation preference
  // basis.
  // TODO(sprang): Replace this with a state holding a relative overuse measure
  // instead, that can be translated into suitable down-scale or fps limit.
  std::map<const DegradationPreference, AdaptCounter> adapt_counters_;
  const BalancedDegradationSettings balanced_settings_;
  // Stores a snapshot of the last adaptation request triggered by an AdaptUp
  // or AdaptDown signal.
  absl::optional<AdaptationRequest> last_adaptation_request_;
  // Keeps track of source restrictions that this adaptation module outputs.
  const std::unique_ptr<VideoSourceRestrictor> source_restrictor_;
  const std::unique_ptr<EncodeUsageResource> encode_usage_resource_;
  const std::unique_ptr<QualityScalerResource> quality_scaler_resource_;
  const bool quality_scaling_experiment_enabled_;
  absl::optional<int> last_input_frame_size_;
  absl::optional<double> target_frame_rate_;
  // This is the last non-zero target bitrate for the encoder.
  absl::optional<uint32_t> encoder_target_bitrate_bps_;
  absl::optional<VideoEncoder::RateControlParameters> encoder_rates_;
  const QualityScalerSettings quality_scaler_settings_;
  bool quality_rampup_done_;
  QualityRampupExperiment quality_rampup_experiment_;
  StartBitrate start_bitrate_;
  absl::optional<EncoderSettings> encoder_settings_;
  VideoStreamEncoderObserver* const encoder_stats_observer_;
  // Counts how many frames we've dropped in the initial framedrop phase.
  int initial_framedrop_;
};

}  // namespace webrtc

#endif  // VIDEO_OVERUSE_FRAME_DETECTOR_RESOURCE_ADAPTATION_MODULE_H_
