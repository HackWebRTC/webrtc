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
    : public ResourceAdaptationModuleInterface,
      public AdaptationObserverInterface {
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
  QualityScaler* quality_scaler() const { return quality_scaler_.get(); }

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
  void ResetVideoSourceRestrictions() override;

  void OnFrame(const VideoFrame& frame) override;
  void OnFrameDroppedDueToSize() override;
  void OnMaybeEncodeFrame() override;
  void OnEncodeStarted(const VideoFrame& cropped_frame,
                       int64_t time_when_first_seen_us) override;
  void OnEncodeCompleted(const EncodedImage& encoded_image,
                         int64_t time_sent_in_us,
                         absl::optional<int> encode_duration_us) override;
  void OnFrameDropped(EncodedImageCallback::DropReason reason) override;
  bool DropInitialFrames() const;

  // TODO(eshr): This can be made private if we configure on
  // SetDegredationPreference and SetEncoderSettings.
  // (https://crbug.com/webrtc/11338)
  void ConfigureQualityScaler(const VideoEncoder::EncoderInfo& encoder_info);

  class AdaptCounter final {
   public:
    AdaptCounter();
    ~AdaptCounter();

    // Get number of adaptation downscales for |reason|.
    VideoStreamEncoderObserver::AdaptationSteps Counts(int reason) const;

    std::string ToString() const;

    void IncrementFramerate(int reason);
    void IncrementResolution(int reason);
    void DecrementFramerate(int reason);
    void DecrementResolution(int reason);
    void DecrementFramerate(int reason, int cur_fps);

    // Gets the total number of downgrades (for all adapt reasons).
    int FramerateCount() const;
    int ResolutionCount() const;

    // Gets the total number of downgrades for |reason|.
    int FramerateCount(int reason) const;
    int ResolutionCount(int reason) const;
    int TotalCount(int reason) const;

   private:
    std::string ToString(const std::vector<int>& counters) const;
    int Count(const std::vector<int>& counters) const;
    void MoveCount(std::vector<int>* counters, int from_reason);

    // Degradation counters holding number of framerate/resolution reductions
    // per adapt reason.
    std::vector<int> fps_counters_;
    std::vector<int> resolution_counters_;
  };

  // AdaptationObserverInterface implementation. Used both "internally" as
  // feedback from |overuse_detector_|, and externally from VideoStreamEncoder:
  // - It is wired to the VideoStreamEncoder::quality_scaler_.
  // - It is invoked by VideoStreamEncoder::MaybeEncodeVideoFrame().
  // TODO(hbos): Decouple quality scaling and resource adaptation, or find an
  // interface for reconfiguring externally.
  // TODO(hbos): VideoStreamEncoder should not be responsible for any part of
  // the adaptation.
  void AdaptUp(AdaptReason reason) override;
  bool AdaptDown(AdaptReason reason) override;

  // Used by VideoStreamEncoder::MaybeEncodeVideoFrame().
  // TODO(hbos): VideoStreamEncoder should not be responsible for any part of
  // the adaptation. Move this logic to this module?
  const AdaptCounter& GetConstAdaptCounter();

 private:
  class VideoSourceRestrictor;

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
      AdaptReason reason);

  // Makes |video_source_restrictions_| up-to-date and informs the
  // |adaptation_listener_| if restrictions are changed, allowing the listener
  // to reconfigure the source accordingly.
  void MaybeUpdateVideoSourceRestrictions();
  // Calculates an up-to-date value of |target_frame_rate_| and informs the
  // |overuse_detector_| of the new value if it changed and the detector is
  // started.
  void MaybeUpdateTargetFrameRate();

  // Use nullopt to disable quality scaling.
  void UpdateQualityScalerSettings(
      absl::optional<VideoEncoder::QpThresholds> qp_thresholds);

  void UpdateAdaptationStats(AdaptReason reason);
  DegradationPreference EffectiveDegradataionPreference();
  AdaptCounter& GetAdaptCounter();
  bool CanAdaptUpResolution(int pixels, uint32_t bitrate_bps) const;

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
  const std::unique_ptr<OveruseFrameDetector> overuse_detector_;
  bool overuse_detector_is_started_;
  absl::optional<int> last_input_frame_size_;
  absl::optional<double> target_frame_rate_;
  // This is the last non-zero target bitrate for the encoder.
  absl::optional<uint32_t> encoder_target_bitrate_bps_;
  std::unique_ptr<QualityScaler> quality_scaler_;
  const bool quality_scaling_experiment_enabled_;
  const QualityScalerSettings quality_scaler_settings_;
  StartBitrate start_bitrate_;
  absl::optional<EncoderSettings> encoder_settings_;
  VideoStreamEncoderObserver* const encoder_stats_observer_;
  // Counts how many frames we've dropped in the initial framedrop phase.
  int initial_framedrop_;
};

}  // namespace webrtc

#endif  // VIDEO_OVERUSE_FRAME_DETECTOR_RESOURCE_ADAPTATION_MODULE_H_
