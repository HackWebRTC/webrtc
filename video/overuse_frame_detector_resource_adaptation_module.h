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
#include "api/video_codecs/video_encoder.h"
#include "api/video_codecs/video_encoder_config.h"
#include "call/adaptation/resource_adaptation_module_interface.h"
#include "rtc_base/experiments/balanced_degradation_settings.h"
#include "video/overuse_frame_detector.h"
#include "video/video_source_sink_controller.h"

namespace webrtc {

class VideoStreamEncoder;

// This class is used by the VideoStreamEncoder and is responsible for adapting
// resolution up or down based on encode usage percent. It keeps track of video
// source settings, adaptation counters and may get influenced by
// VideoStreamEncoder's quality scaler through AdaptUp() and AdaptDown() calls.
// TODO(hbos): Reduce the coupling with VideoStreamEncoder.
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
  OveruseFrameDetectorResourceAdaptationModule(
      VideoStreamEncoder* video_stream_encoder,
      VideoSourceSinkController* video_source_controller,
      std::unique_ptr<OveruseFrameDetector> overuse_detector,
      VideoStreamEncoderObserver* encoder_stats_observer,
      ResourceAdaptationModuleListener* adaptation_listener);
  ~OveruseFrameDetectorResourceAdaptationModule() override;

  void Initialize(rtc::TaskQueue* encoder_queue);
  // Sets the encoder to reconfigure based on overuse.
  // TODO(hbos): Don't reconfigure the encoder directly. Instead, define the
  // output of a resource adaptation module as a struct and let the
  // VideoStreamEncoder handle the interaction with the actual encoder.
  void SetEncoder(VideoEncoder* encoder);

  DegradationPreference degradation_preference() const {
    RTC_DCHECK(encoder_queue_);
    RTC_DCHECK_RUN_ON(encoder_queue_);
    return degradation_preference_;
  }

  // ResourceAdaptationModuleInterface implementation.
  void StartCheckForOveruse(
      ResourceAdaptationModuleListener* adaptation_listener) override;
  void StopCheckForOveruse() override;

  // Input to the OveruseFrameDetector, which are required for this module to
  // function. These map to OveruseFrameDetector methods.
  // TODO(hbos): Define virtual methods in ResourceAdaptationModuleInterface
  // for input that are more generic so that this class can be used without
  // assumptions about underlying implementation.
  void FrameCaptured(const VideoFrame& frame, int64_t time_when_first_seen_us);
  void FrameSent(uint32_t timestamp,
                 int64_t time_sent_in_us,
                 int64_t capture_time_us,
                 absl::optional<int> encode_duration_us);

  // Various other settings and feedback mechanisms.
  // TODO(hbos): Find a common interface that would make sense for a generic
  // resource adaptation module. Unify code paths where possible. Do we really
  // need this many public methods?
  void SetLastFramePixelCount(absl::optional<int> last_frame_pixel_count);
  void SetEncoderConfig(VideoEncoderConfig encoder_config);
  void SetCodecMaxFramerate(int codec_max_framerate);
  void SetEncoderStartBitrateBps(uint32_t encoder_start_bitrate_bps);
  // Inform the detector whether or not the quality scaler is enabled. This
  // helps GetActiveCounts() return absl::nullopt when appropriate.
  // TODO(hbos): This feels really hacky, can we report the right values without
  // this boolean? It would be really easy to report the wrong thing if this
  // method is called incorrectly.
  void SetIsQualityScalerEnabled(bool is_quality_scaler_enabled);

  void SetHasInputVideoAndDegradationPreference(
      bool has_input_video,
      DegradationPreference degradation_preference);

  // TODO(hbos): Can we get rid of this? Seems we should know whether the frame
  // rate has updated.
  void RefreshTargetFramerate();
  void ResetAdaptationCounters();

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

  // Used by VideoStreamEncoder when ConfigureQualityScaler() occurs and the
  // |encoder_stats_observer_| is called outside of this class.
  // TODO(hbos): Decouple quality scaling and resource adaptation logic and make
  // this method private.
  VideoStreamEncoderObserver::AdaptationSteps GetActiveCounts(
      AdaptReason reason);

  // Used by VideoStreamEncoder::MaybeEncodeVideoFrame().
  // TODO(hbos): VideoStreamEncoder should not be responsible for any part of
  // the adaptation. Move this logic to this module?
  const AdaptCounter& GetConstAdaptCounter();

  // Used by VideoStreamEncoder::ConfigureQualityScaler().
  // TODO(hbos): Decouple quality scaling and resource adaptation logic and
  // delete this method.
  absl::optional<VideoEncoder::QpThresholds> GetQpThresholds() const;

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

  void UpdateAdaptationStats(AdaptReason reason) RTC_RUN_ON(encoder_queue_);
  DegradationPreference EffectiveDegradataionPreference()
      RTC_RUN_ON(encoder_queue_);
  AdaptCounter& GetAdaptCounter() RTC_RUN_ON(encoder_queue_);
  bool CanAdaptUpResolution(int pixels, uint32_t bitrate_bps) const
      RTC_RUN_ON(encoder_queue_);

  // TODO(hbos): Can we move the |source_restrictor_| to the |encoder_queue_|
  // and replace |encoder_queue_| with a sequence checker instead?
  rtc::TaskQueue* encoder_queue_;
  ResourceAdaptationModuleListener* const adaptation_listener_
      RTC_GUARDED_BY(encoder_queue_);
  // Used to query CpuOveruseOptions at StartCheckForOveruse().
  VideoStreamEncoder* video_stream_encoder_ RTC_GUARDED_BY(encoder_queue_);
  // TODO(https://crbug.com/webrtc/11222): When the VideoSourceSinkController is
  // no longer aware of DegradationPreference, and the degradation
  // preference-related logic resides within this class, we can remove this
  // dependency on the VideoSourceSinkController.
  VideoSourceSinkController* const video_source_sink_controller_;
  DegradationPreference degradation_preference_ RTC_GUARDED_BY(encoder_queue_);
  // Counters used for deciding if the video resolution or framerate is
  // currently restricted, and if so, why, on a per degradation preference
  // basis.
  // TODO(sprang): Replace this with a state holding a relative overuse measure
  // instead, that can be translated into suitable down-scale or fps limit.
  std::map<const DegradationPreference, AdaptCounter> adapt_counters_
      RTC_GUARDED_BY(encoder_queue_);
  const BalancedDegradationSettings balanced_settings_
      RTC_GUARDED_BY(encoder_queue_);
  // Stores a snapshot of the last adaptation request triggered by an AdaptUp
  // or AdaptDown signal.
  absl::optional<AdaptationRequest> last_adaptation_request_
      RTC_GUARDED_BY(encoder_queue_);
  absl::optional<int> last_frame_pixel_count_ RTC_GUARDED_BY(encoder_queue_);
  // Keeps track of source restrictions that this adaptation module outputs.
  const std::unique_ptr<VideoSourceRestrictor> source_restrictor_;
  const std::unique_ptr<OveruseFrameDetector> overuse_detector_
      RTC_PT_GUARDED_BY(encoder_queue_);
  int codec_max_framerate_ RTC_GUARDED_BY(encoder_queue_);
  uint32_t encoder_start_bitrate_bps_ RTC_GUARDED_BY(encoder_queue_);
  bool is_quality_scaler_enabled_ RTC_GUARDED_BY(encoder_queue_);
  VideoEncoderConfig encoder_config_ RTC_GUARDED_BY(encoder_queue_);
  VideoEncoder* encoder_ RTC_GUARDED_BY(encoder_queue_);
  VideoStreamEncoderObserver* const encoder_stats_observer_
      RTC_GUARDED_BY(encoder_queue_);
};

}  // namespace webrtc

#endif  // VIDEO_OVERUSE_FRAME_DETECTOR_RESOURCE_ADAPTATION_MODULE_H_
