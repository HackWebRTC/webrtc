/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_VIE_ENCODER_H_
#define WEBRTC_VIDEO_VIE_ENCODER_H_

#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/event.h"
#include "webrtc/base/sequenced_task_checker.h"
#include "webrtc/base/task_queue.h"
#include "webrtc/call.h"
#include "webrtc/common_types.h"
#include "webrtc/common_video/rotation.h"
#include "webrtc/media/base/videosinkinterface.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/modules/video_coding/utility/simulcast_rate_allocator.h"
#include "webrtc/modules/video_coding/video_coding_impl.h"
#include "webrtc/modules/video_processing/include/video_processing.h"
#include "webrtc/system_wrappers/include/atomic32.h"
#include "webrtc/video/overuse_frame_detector.h"
#include "webrtc/video_encoder.h"
#include "webrtc/video_send_stream.h"
#include "webrtc/typedefs.h"

namespace webrtc {

class ProcessThread;
class SendStatisticsProxy;

// VieEncoder represent a video encoder that accepts raw video frames as input
// and produces an encoded bit stream.
// Usage:
//  Instantiate.
//  Call SetSink.
//  Call SetSource.
//  Call ConfigureEncoder with the codec settings.
//  Call Stop() when done.
class ViEEncoder : public rtc::VideoSinkInterface<VideoFrame>,
                   public EncodedImageCallback,
                   public VCMSendStatisticsCallback,
                   public CpuOveruseObserver {
 public:
  // Interface for receiving encoded video frames and notifications about
  // configuration changes.
  class EncoderSink : public EncodedImageCallback {
   public:
    virtual void OnEncoderConfigurationChanged(
        std::vector<VideoStream> streams,
        int min_transmit_bitrate_bps) = 0;
  };

  ViEEncoder(uint32_t number_of_cores,
             SendStatisticsProxy* stats_proxy,
             const webrtc::VideoSendStream::Config::EncoderSettings& settings,
             rtc::VideoSinkInterface<VideoFrame>* pre_encode_callback,
             LoadObserver* overuse_callback,
             EncodedFrameObserver* encoder_timing);
  ~ViEEncoder();
  // RegisterProcessThread register |module_process_thread| with those objects
  // that use it. Registration has to happen on the thread where
  // |module_process_thread| was created (libjingle's worker thread).
  // TODO(perkj): Replace the use of |module_process_thread| with a TaskQueue.
  void RegisterProcessThread(ProcessThread* module_process_thread);
  void DeRegisterProcessThread();

  void SetSource(rtc::VideoSourceInterface<VideoFrame>* source);
  void SetSink(EncoderSink* sink);

  // TODO(perkj): Can we remove VideoCodec.startBitrate ?
  void SetStartBitrate(int start_bitrate_bps);

  void ConfigureEncoder(VideoEncoderConfig config,
                        size_t max_data_payload_length);

  // Permanently stop encoding. After this method has returned, it is
  // guaranteed that no encoded frames will be delivered to the sink.
  void Stop();

  void SendKeyFrame();

  // virtual to test EncoderStateFeedback with mocks.
  virtual void OnReceivedIntraFrameRequest(size_t stream_index);
  virtual void OnReceivedSLI(uint8_t picture_id);
  virtual void OnReceivedRPSI(uint64_t picture_id);

  void OnBitrateUpdated(uint32_t bitrate_bps,
                        uint8_t fraction_lost,
                        int64_t round_trip_time_ms);

 private:
  class ConfigureEncoderTask;
  class EncodeTask;
  class VideoSourceProxy;

  struct VideoFrameInfo {
    VideoFrameInfo(int width,
                   int height,
                   VideoRotation rotation,
                   bool is_texture)
        : width(width),
          height(height),
          rotation(rotation),
          is_texture(is_texture) {}
    int width;
    int height;
    webrtc::VideoRotation rotation;
    bool is_texture;
  };

  void ConfigureEncoderOnTaskQueue(VideoEncoderConfig config,
                                   size_t max_data_payload_length);
  void ReconfigureEncoder();

  // Implements VideoSinkInterface.
  void OnFrame(const VideoFrame& video_frame) override;

  // Implements VideoSendStatisticsCallback.
  void SendStatistics(uint32_t bit_rate,
                      uint32_t frame_rate) override;

  void EncodeVideoFrame(const VideoFrame& frame,
                        int64_t time_when_posted_in_ms);

  // Implements EncodedImageCallback.
  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override;

  // webrtc::CpuOveruseObserver implementation.
  void OveruseDetected() override;
  void NormalUsage() override;

  bool EncoderPaused() const;
  void TraceFrameDropStart();
  void TraceFrameDropEnd();

  rtc::Event shutdown_event_;

  const uint32_t number_of_cores_;

  const std::unique_ptr<VideoSourceProxy> source_proxy_;
  EncoderSink* sink_;
  const VideoSendStream::Config::EncoderSettings settings_;
  const VideoCodecType codec_type_;

  vcm::VideoSender video_sender_ ACCESS_ON(&encoder_queue_);

  OveruseFrameDetector overuse_detector_ ACCESS_ON(&encoder_queue_);
  LoadObserver* const load_observer_ ACCESS_ON(&encoder_queue_);

  SendStatisticsProxy* const stats_proxy_;
  rtc::VideoSinkInterface<VideoFrame>* const pre_encode_callback_;
  ProcessThread* module_process_thread_;
  rtc::ThreadChecker module_process_thread_checker_;
  // |thread_checker_| checks that public methods that are related to lifetime
  // of ViEEncoder are called on the same thread.
  rtc::ThreadChecker thread_checker_;

  VideoEncoderConfig encoder_config_ ACCESS_ON(&encoder_queue_);
  // TODO(sprang): Change |rate_allocator_| to be a codec type
  // agnostic interface. It is currently VP8 simulcast specific if more than
  // one layer is specified.
  std::unique_ptr<SimulcastRateAllocator> rate_allocator_
      ACCESS_ON(&encoder_queue_);

  // Set when ConfigureEncoder has been called in order to lazy reconfigure the
  // encoder on the next frame.
  bool pending_encoder_reconfiguration_ ACCESS_ON(&encoder_queue_);
  rtc::Optional<VideoFrameInfo> last_frame_info_ ACCESS_ON(&encoder_queue_);
  uint32_t encoder_start_bitrate_bps_ ACCESS_ON(&encoder_queue_);
  size_t max_data_payload_length_ ACCESS_ON(&encoder_queue_);
  uint32_t last_observed_bitrate_bps_ ACCESS_ON(&encoder_queue_);
  bool encoder_paused_and_dropped_frame_ ACCESS_ON(&encoder_queue_);
  bool has_received_sli_ ACCESS_ON(&encoder_queue_);
  uint8_t picture_id_sli_ ACCESS_ON(&encoder_queue_);
  bool has_received_rpsi_ ACCESS_ON(&encoder_queue_);
  uint64_t picture_id_rpsi_ ACCESS_ON(&encoder_queue_);
  Clock* const clock_;

  rtc::RaceChecker incoming_frame_race_checker_;
  Atomic32 posted_frames_waiting_for_encode_;
  // Used to make sure incoming time stamp is increasing for every frame.
  int64_t last_captured_timestamp_ GUARDED_BY(incoming_frame_race_checker_);
  // Delta used for translating between NTP and internal timestamps.
  const int64_t delta_ntp_internal_ms_;

  int64_t last_frame_log_ms_ GUARDED_BY(incoming_frame_race_checker_);
  int captured_frame_count_ ACCESS_ON(&encoder_queue_);
  int dropped_frame_count_ ACCESS_ON(&encoder_queue_);

  // All public methods are proxied to |encoder_queue_|. It must must be
  // destroyed first to make sure no tasks are run that use other members.
  rtc::TaskQueue encoder_queue_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ViEEncoder);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_VIE_ENCODER_H_
