/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video/vie_encoder.h"

#include <algorithm>
#include <limits>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/modules/pacing/paced_sender.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/video/overuse_frame_detector.h"
#include "webrtc/video/send_statistics_proxy.h"
#include "webrtc/video_frame.h"

namespace webrtc {

namespace {
// Time interval for logging frame counts.
const int64_t kFrameLogIntervalMs = 60000;

VideoCodecType PayloadNameToCodecType(const std::string& payload_name) {
  if (payload_name == "VP8")
    return kVideoCodecVP8;
  if (payload_name == "VP9")
    return kVideoCodecVP9;
  if (payload_name == "H264")
    return kVideoCodecH264;
  return kVideoCodecGeneric;
}

VideoCodec VideoEncoderConfigToVideoCodec(const VideoEncoderConfig& config,
                                          const std::string& payload_name,
                                          int payload_type) {
  const std::vector<VideoStream>& streams = config.streams;
  static const int kEncoderMinBitrateKbps = 30;
  RTC_DCHECK(!streams.empty());
  RTC_DCHECK_GE(config.min_transmit_bitrate_bps, 0);

  VideoCodec video_codec;
  memset(&video_codec, 0, sizeof(video_codec));
  video_codec.codecType = PayloadNameToCodecType(payload_name);

  switch (config.content_type) {
    case VideoEncoderConfig::ContentType::kRealtimeVideo:
      video_codec.mode = kRealtimeVideo;
      break;
    case VideoEncoderConfig::ContentType::kScreen:
      video_codec.mode = kScreensharing;
      if (config.streams.size() == 1 &&
          config.streams[0].temporal_layer_thresholds_bps.size() == 1) {
        video_codec.targetBitrate =
            config.streams[0].temporal_layer_thresholds_bps[0] / 1000;
      }
      break;
  }

  switch (video_codec.codecType) {
    case kVideoCodecVP8: {
      if (config.encoder_specific_settings) {
        video_codec.codecSpecific.VP8 = *reinterpret_cast<const VideoCodecVP8*>(
            config.encoder_specific_settings);
      } else {
        video_codec.codecSpecific.VP8 = VideoEncoder::GetDefaultVp8Settings();
      }
      video_codec.codecSpecific.VP8.numberOfTemporalLayers =
          static_cast<unsigned char>(
              streams.back().temporal_layer_thresholds_bps.size() + 1);
      break;
    }
    case kVideoCodecVP9: {
      if (config.encoder_specific_settings) {
        video_codec.codecSpecific.VP9 = *reinterpret_cast<const VideoCodecVP9*>(
            config.encoder_specific_settings);
        if (video_codec.mode == kScreensharing) {
          video_codec.codecSpecific.VP9.flexibleMode = true;
          // For now VP9 screensharing use 1 temporal and 2 spatial layers.
          RTC_DCHECK_EQ(video_codec.codecSpecific.VP9.numberOfTemporalLayers,
                        1);
          RTC_DCHECK_EQ(video_codec.codecSpecific.VP9.numberOfSpatialLayers, 2);
        }
      } else {
        video_codec.codecSpecific.VP9 = VideoEncoder::GetDefaultVp9Settings();
      }
      video_codec.codecSpecific.VP9.numberOfTemporalLayers =
          static_cast<unsigned char>(
              streams.back().temporal_layer_thresholds_bps.size() + 1);
      break;
    }
    case kVideoCodecH264: {
      if (config.encoder_specific_settings) {
        video_codec.codecSpecific.H264 =
            *reinterpret_cast<const VideoCodecH264*>(
                config.encoder_specific_settings);
      } else {
        video_codec.codecSpecific.H264 = VideoEncoder::GetDefaultH264Settings();
      }
      break;
    }
    default:
      // TODO(pbos): Support encoder_settings codec-agnostically.
      RTC_DCHECK(!config.encoder_specific_settings)
          << "Encoder-specific settings for codec type not wired up.";
      break;
  }

  strncpy(video_codec.plName, payload_name.c_str(), kPayloadNameSize - 1);
  video_codec.plName[kPayloadNameSize - 1] = '\0';
  video_codec.plType = payload_type;
  video_codec.numberOfSimulcastStreams =
      static_cast<unsigned char>(streams.size());
  video_codec.minBitrate = streams[0].min_bitrate_bps / 1000;
  if (video_codec.minBitrate < kEncoderMinBitrateKbps)
    video_codec.minBitrate = kEncoderMinBitrateKbps;
  RTC_DCHECK_LE(streams.size(), static_cast<size_t>(kMaxSimulcastStreams));
  if (video_codec.codecType == kVideoCodecVP9) {
    // If the vector is empty, bitrates will be configured automatically.
    RTC_DCHECK(config.spatial_layers.empty() ||
               config.spatial_layers.size() ==
                   video_codec.codecSpecific.VP9.numberOfSpatialLayers);
    RTC_DCHECK_LE(video_codec.codecSpecific.VP9.numberOfSpatialLayers,
                  kMaxSimulcastStreams);
    for (size_t i = 0; i < config.spatial_layers.size(); ++i)
      video_codec.spatialLayers[i] = config.spatial_layers[i];
  }
  for (size_t i = 0; i < streams.size(); ++i) {
    SimulcastStream* sim_stream = &video_codec.simulcastStream[i];
    RTC_DCHECK_GT(streams[i].width, 0u);
    RTC_DCHECK_GT(streams[i].height, 0u);
    RTC_DCHECK_GT(streams[i].max_framerate, 0);
    // Different framerates not supported per stream at the moment.
    RTC_DCHECK_EQ(streams[i].max_framerate, streams[0].max_framerate);
    RTC_DCHECK_GE(streams[i].min_bitrate_bps, 0);
    RTC_DCHECK_GE(streams[i].target_bitrate_bps, streams[i].min_bitrate_bps);
    RTC_DCHECK_GE(streams[i].max_bitrate_bps, streams[i].target_bitrate_bps);
    RTC_DCHECK_GE(streams[i].max_qp, 0);

    sim_stream->width = static_cast<uint16_t>(streams[i].width);
    sim_stream->height = static_cast<uint16_t>(streams[i].height);
    sim_stream->minBitrate = streams[i].min_bitrate_bps / 1000;
    sim_stream->targetBitrate = streams[i].target_bitrate_bps / 1000;
    sim_stream->maxBitrate = streams[i].max_bitrate_bps / 1000;
    sim_stream->qpMax = streams[i].max_qp;
    sim_stream->numberOfTemporalLayers = static_cast<unsigned char>(
        streams[i].temporal_layer_thresholds_bps.size() + 1);

    video_codec.width =
        std::max(video_codec.width, static_cast<uint16_t>(streams[i].width));
    video_codec.height =
        std::max(video_codec.height, static_cast<uint16_t>(streams[i].height));
    video_codec.minBitrate =
        std::min(static_cast<uint16_t>(video_codec.minBitrate),
                 static_cast<uint16_t>(streams[i].min_bitrate_bps / 1000));
    video_codec.maxBitrate += streams[i].max_bitrate_bps / 1000;
    video_codec.qpMax = std::max(video_codec.qpMax,
                                 static_cast<unsigned int>(streams[i].max_qp));
  }

  if (video_codec.maxBitrate == 0) {
    // Unset max bitrate -> cap to one bit per pixel.
    video_codec.maxBitrate =
        (video_codec.width * video_codec.height * video_codec.maxFramerate) /
        1000;
  }
  if (video_codec.maxBitrate < kEncoderMinBitrateKbps)
    video_codec.maxBitrate = kEncoderMinBitrateKbps;

  RTC_DCHECK_GT(streams[0].max_framerate, 0);
  video_codec.maxFramerate = streams[0].max_framerate;
  video_codec.expect_encode_from_texture = config.expect_encode_from_texture;

  return video_codec;
}

// TODO(pbos): Lower these thresholds (to closer to 100%) when we handle
// pipelining encoders better (multiple input frames before something comes
// out). This should effectively turn off CPU adaptations for systems that
// remotely cope with the load right now.
CpuOveruseOptions GetCpuOveruseOptions(bool full_overuse_time) {
  CpuOveruseOptions options;
  if (full_overuse_time) {
    options.low_encode_usage_threshold_percent = 150;
    options.high_encode_usage_threshold_percent = 200;
  }
  return options;
}

}  //  namespace

class ViEEncoder::EncodeTask : public rtc::QueuedTask {
 public:
  EncodeTask(const VideoFrame& frame,
             ViEEncoder* vie_encoder,
             int64_t time_when_posted_in_ms,
             bool log_stats)
      : vie_encoder_(vie_encoder),
        time_when_posted_ms_(time_when_posted_in_ms),
        log_stats_(log_stats) {
    frame_.ShallowCopy(frame);
    ++vie_encoder_->posted_frames_waiting_for_encode_;
  }

 private:
  bool Run() override {
    RTC_DCHECK_RUN_ON(&vie_encoder_->encoder_queue_);
    RTC_DCHECK_GT(vie_encoder_->posted_frames_waiting_for_encode_.Value(), 0);
    ++vie_encoder_->captured_frame_count_;
    if (--vie_encoder_->posted_frames_waiting_for_encode_ == 0) {
      vie_encoder_->EncodeVideoFrame(frame_, time_when_posted_ms_);
    } else {
      // There is a newer frame in flight. Do not encode this frame.
      LOG(LS_VERBOSE)
          << "Incoming frame dropped due to that the encoder is blocked.";
      ++vie_encoder_->dropped_frame_count_;
    }
    if (log_stats_) {
      LOG(LS_INFO) << "Number of frames: captured "
                   << vie_encoder_->captured_frame_count_
                   << ", dropped (due to encoder blocked) "
                   << vie_encoder_->dropped_frame_count_ << ", interval_ms "
                   << kFrameLogIntervalMs;
      vie_encoder_->captured_frame_count_ = 0;
      vie_encoder_->dropped_frame_count_ = 0;
    }
    return true;
  }
  VideoFrame frame_;
  ViEEncoder* const vie_encoder_;
  const int64_t time_when_posted_ms_;
  const bool log_stats_;
};

// VideoSourceProxy is responsible ensuring thread safety between calls to
// ViEEncoder::SetSource that will happen on libjingles worker thread when a
// video capturer is connected to the encoder and the encoder task queue
// (encoder_queue_) where the encoder reports its VideoSinkWants.
class ViEEncoder::VideoSourceProxy {
 public:
  explicit VideoSourceProxy(ViEEncoder* vie_encoder)
      : vie_encoder_(vie_encoder), source_(nullptr) {}

  void SetSource(rtc::VideoSourceInterface<VideoFrame>* source) {
    RTC_DCHECK_CALLED_SEQUENTIALLY(&main_checker_);
    rtc::VideoSourceInterface<VideoFrame>* old_source = nullptr;
    {
      rtc::CritScope lock(&crit_);
      old_source = source_;
      source_ = source;
    }

    if (old_source != source && old_source != nullptr) {
      old_source->RemoveSink(vie_encoder_);
    }

    if (!source) {
      return;
    }

    // TODO(perkj): Let VideoSourceProxy implement LoadObserver and truly send
    // CPU load as sink wants.
    rtc::VideoSinkWants wants;
    source->AddOrUpdateSink(vie_encoder_, wants);
  }

 private:
  rtc::CriticalSection crit_;
  rtc::SequencedTaskChecker main_checker_;
  ViEEncoder* vie_encoder_;
  rtc::VideoSourceInterface<VideoFrame>* source_ GUARDED_BY(&crit_);

  RTC_DISALLOW_COPY_AND_ASSIGN(VideoSourceProxy);
};

ViEEncoder::ViEEncoder(uint32_t number_of_cores,
                       SendStatisticsProxy* stats_proxy,
                       const VideoSendStream::Config::EncoderSettings& settings,
                       rtc::VideoSinkInterface<VideoFrame>* pre_encode_callback,
                       LoadObserver* overuse_callback,
                       EncodedFrameObserver* encoder_timing)
    : shutdown_event_(true /* manual_reset */, false),
      number_of_cores_(number_of_cores),
      source_proxy_(new VideoSourceProxy(this)),
      settings_(settings),
      vp_(VideoProcessing::Create()),
      video_sender_(Clock::GetRealTimeClock(), this, this),
      overuse_detector_(Clock::GetRealTimeClock(),
                        GetCpuOveruseOptions(settings.full_overuse_time),
                        this,
                        encoder_timing,
                        stats_proxy),
      load_observer_(overuse_callback),
      stats_proxy_(stats_proxy),
      pre_encode_callback_(pre_encode_callback),
      module_process_thread_(nullptr),
      encoder_config_(),
      encoder_start_bitrate_bps_(0),
      last_observed_bitrate_bps_(0),
      encoder_paused_and_dropped_frame_(false),
      has_received_sli_(false),
      picture_id_sli_(0),
      has_received_rpsi_(false),
      picture_id_rpsi_(0),
      clock_(Clock::GetRealTimeClock()),
      last_captured_timestamp_(0),
      delta_ntp_internal_ms_(clock_->CurrentNtpInMilliseconds() -
                             clock_->TimeInMilliseconds()),
      last_frame_log_ms_(clock_->TimeInMilliseconds()),
      captured_frame_count_(0),
      dropped_frame_count_(0),
      encoder_queue_("EncoderQueue") {
  vp_->EnableTemporalDecimation(false);

  encoder_queue_.PostTask([this, encoder_timing] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    video_sender_.RegisterExternalEncoder(
        settings_.encoder, settings_.payload_type, settings_.internal_source);
    overuse_detector_.StartCheckForOveruse();
  });
}

ViEEncoder::~ViEEncoder() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(shutdown_event_.Wait(0))
      << "Must call ::Stop() before destruction.";
}

void ViEEncoder::Stop() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  source_proxy_->SetSource(nullptr);
  encoder_queue_.PostTask([this] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    video_sender_.RegisterExternalEncoder(nullptr, settings_.payload_type,
                                          false);
    overuse_detector_.StopCheckForOveruse();
    shutdown_event_.Set();
  });

  shutdown_event_.Wait(rtc::Event::kForever);
}

void ViEEncoder::RegisterProcessThread(ProcessThread* module_process_thread) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(!module_process_thread_);
  module_process_thread_ = module_process_thread;
  module_process_thread_->RegisterModule(&video_sender_);
  module_process_thread_checker_.DetachFromThread();
}

void ViEEncoder::DeRegisterProcessThread() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  module_process_thread_->DeRegisterModule(&video_sender_);
}

void ViEEncoder::SetSource(rtc::VideoSourceInterface<VideoFrame>* source) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  source_proxy_->SetSource(source);
}

void ViEEncoder::SetSink(EncodedImageCallback* sink) {
  encoder_queue_.PostTask([this, sink] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    sink_ = sink;
  });
}

void ViEEncoder::SetStartBitrate(int start_bitrate_bps) {
  encoder_queue_.PostTask([this, start_bitrate_bps] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    encoder_start_bitrate_bps_ = start_bitrate_bps;
  });
}

void ViEEncoder::ConfigureEncoder(const VideoEncoderConfig& config,
                                  size_t max_data_payload_length) {
  VideoCodec video_codec = VideoEncoderConfigToVideoCodec(
      config, settings_.payload_name, settings_.payload_type);
  encoder_queue_.PostTask([this, video_codec, max_data_payload_length] {
    ConfigureEncoderInternal(video_codec, max_data_payload_length);
  });
  return;
}

void ViEEncoder::ConfigureEncoderInternal(const VideoCodec& video_codec,
                                          size_t max_data_payload_length) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  RTC_DCHECK_GE(encoder_start_bitrate_bps_, 0);
  RTC_DCHECK(sink_);

  // Setting target width and height for VPM.
  RTC_CHECK_EQ(VPM_OK,
               vp_->SetTargetResolution(video_codec.width, video_codec.height,
                                        video_codec.maxFramerate));

  encoder_config_ = video_codec;
  encoder_config_.startBitrate = encoder_start_bitrate_bps_ / 1000;
  encoder_config_.startBitrate =
      std::max(encoder_config_.startBitrate, video_codec.minBitrate);
  encoder_config_.startBitrate =
      std::min(encoder_config_.startBitrate, video_codec.maxBitrate);

  bool success = video_sender_.RegisterSendCodec(
                     &encoder_config_, number_of_cores_,
                     static_cast<uint32_t>(max_data_payload_length)) == VCM_OK;

  if (!success) {
    LOG(LS_ERROR) << "Failed to configure encoder.";
    RTC_DCHECK(success);
  }

  if (stats_proxy_) {
    VideoEncoderConfig::ContentType content_type =
        VideoEncoderConfig::ContentType::kRealtimeVideo;
    switch (video_codec.mode) {
      case kRealtimeVideo:
        content_type = VideoEncoderConfig::ContentType::kRealtimeVideo;
        break;
      case kScreensharing:
        content_type = VideoEncoderConfig::ContentType::kScreen;
        break;
      default:
        RTC_NOTREACHED();
        break;
    }
    stats_proxy_->SetContentType(content_type);
  }
}

void ViEEncoder::OnFrame(const VideoFrame& video_frame) {
  RTC_DCHECK_RUNS_SERIALIZED(&incoming_frame_race_checker_);
  stats_proxy_->OnIncomingFrame(video_frame.width(), video_frame.height());

  VideoFrame incoming_frame = video_frame;

  // Local time in webrtc time base.
  int64_t current_time = clock_->TimeInMilliseconds();
  incoming_frame.set_render_time_ms(current_time);

  // Capture time may come from clock with an offset and drift from clock_.
  int64_t capture_ntp_time_ms;
  if (video_frame.ntp_time_ms() != 0) {
    capture_ntp_time_ms = video_frame.ntp_time_ms();
  } else if (video_frame.render_time_ms() != 0) {
    capture_ntp_time_ms = video_frame.render_time_ms() + delta_ntp_internal_ms_;
  } else {
    capture_ntp_time_ms = current_time + delta_ntp_internal_ms_;
  }
  incoming_frame.set_ntp_time_ms(capture_ntp_time_ms);

  // Convert NTP time, in ms, to RTP timestamp.
  const int kMsToRtpTimestamp = 90;
  incoming_frame.set_timestamp(
      kMsToRtpTimestamp * static_cast<uint32_t>(incoming_frame.ntp_time_ms()));

  if (incoming_frame.ntp_time_ms() <= last_captured_timestamp_) {
    // We don't allow the same capture time for two frames, drop this one.
    LOG(LS_WARNING) << "Same/old NTP timestamp ("
                    << incoming_frame.ntp_time_ms()
                    << " <= " << last_captured_timestamp_
                    << ") for incoming frame. Dropping.";
    return;
  }

  bool log_stats = false;
  if (current_time - last_frame_log_ms_ > kFrameLogIntervalMs) {
    last_frame_log_ms_ = current_time;
    log_stats = true;
  }

  last_captured_timestamp_ = incoming_frame.ntp_time_ms();
  encoder_queue_.PostTask(std::unique_ptr<rtc::QueuedTask>(new EncodeTask(
      incoming_frame, this, clock_->TimeInMilliseconds(), log_stats)));
}

bool ViEEncoder::EncoderPaused() const {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  // Pause video if paused by caller or as long as the network is down or the
  // pacer queue has grown too large in buffered mode.
  // If the pacer queue has grown too large or the network is down,
  // last_observed_bitrate_bps_ will be 0.
  return last_observed_bitrate_bps_ == 0;
}

void ViEEncoder::TraceFrameDropStart() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  // Start trace event only on the first frame after encoder is paused.
  if (!encoder_paused_and_dropped_frame_) {
    TRACE_EVENT_ASYNC_BEGIN0("webrtc", "EncoderPaused", this);
  }
  encoder_paused_and_dropped_frame_ = true;
  return;
}

void ViEEncoder::TraceFrameDropEnd() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  // End trace event on first frame after encoder resumes, if frame was dropped.
  if (encoder_paused_and_dropped_frame_) {
    TRACE_EVENT_ASYNC_END0("webrtc", "EncoderPaused", this);
  }
  encoder_paused_and_dropped_frame_ = false;
}

void ViEEncoder::EncodeVideoFrame(const VideoFrame& video_frame,
                                  int64_t time_when_posted_in_ms) {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  if (pre_encode_callback_)
    pre_encode_callback_->OnFrame(video_frame);

  if (EncoderPaused()) {
    TraceFrameDropStart();
    return;
  }
  TraceFrameDropEnd();

  TRACE_EVENT_ASYNC_STEP0("webrtc", "Video", video_frame.render_time_ms(),
                          "Encode");
  const VideoFrame* frame_to_send = &video_frame;
  // TODO(wuchengli): support texture frames.
  if (!video_frame.video_frame_buffer()->native_handle()) {
    // Pass frame via preprocessor.
    frame_to_send = vp_->PreprocessFrame(video_frame);
    if (!frame_to_send) {
      // Drop this frame, or there was an error processing it.
      return;
    }
  }

  overuse_detector_.FrameCaptured(video_frame, time_when_posted_in_ms);

  if (encoder_config_.codecType == webrtc::kVideoCodecVP8) {
    webrtc::CodecSpecificInfo codec_specific_info;
    codec_specific_info.codecType = webrtc::kVideoCodecVP8;

      codec_specific_info.codecSpecific.VP8.hasReceivedRPSI =
          has_received_rpsi_;
      codec_specific_info.codecSpecific.VP8.hasReceivedSLI =
          has_received_sli_;
      codec_specific_info.codecSpecific.VP8.pictureIdRPSI =
          picture_id_rpsi_;
      codec_specific_info.codecSpecific.VP8.pictureIdSLI  =
          picture_id_sli_;
      has_received_sli_ = false;
      has_received_rpsi_ = false;

    video_sender_.AddVideoFrame(*frame_to_send, &codec_specific_info);
    return;
  }
  video_sender_.AddVideoFrame(*frame_to_send, nullptr);
}

void ViEEncoder::SendKeyFrame() {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask([this] { SendKeyFrame(); });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  video_sender_.IntraFrameRequest(0);
}

EncodedImageCallback::Result ViEEncoder::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  // Encoded is called on whatever thread the real encoder implementation run
  // on. In the case of hardware encoders, there might be several encoders
  // running in parallel on different threads.
  if (stats_proxy_) {
    stats_proxy_->OnSendEncodedImage(encoded_image, codec_specific_info);
  }

  EncodedImageCallback::Result result =
      sink_->OnEncodedImage(encoded_image, codec_specific_info, fragmentation);

  int64_t time_sent = clock_->TimeInMilliseconds();
  uint32_t timestamp = encoded_image._timeStamp;
  encoder_queue_.PostTask([this, timestamp, time_sent] {
    RTC_DCHECK_RUN_ON(&encoder_queue_);
    overuse_detector_.FrameSent(timestamp, time_sent);
  });
  return result;
}

void ViEEncoder::SendStatistics(uint32_t bit_rate, uint32_t frame_rate) {
  RTC_DCHECK(module_process_thread_checker_.CalledOnValidThread());
  if (stats_proxy_)
    stats_proxy_->OnEncoderStatsUpdate(frame_rate, bit_rate);
}

void ViEEncoder::OnReceivedSLI(uint8_t picture_id) {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask([this, picture_id] { OnReceivedSLI(picture_id); });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  picture_id_sli_ = picture_id;
  has_received_sli_ = true;
}

void ViEEncoder::OnReceivedRPSI(uint64_t picture_id) {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask([this, picture_id] { OnReceivedRPSI(picture_id); });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  picture_id_rpsi_ = picture_id;
  has_received_rpsi_ = true;
}

void ViEEncoder::OnReceivedIntraFrameRequest(size_t stream_index) {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask(
        [this, stream_index] { OnReceivedIntraFrameRequest(stream_index); });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  // Key frame request from remote side, signal to VCM.
  TRACE_EVENT0("webrtc", "OnKeyFrameRequest");
  video_sender_.IntraFrameRequest(stream_index);
}

void ViEEncoder::OnBitrateUpdated(uint32_t bitrate_bps,
                                  uint8_t fraction_lost,
                                  int64_t round_trip_time_ms) {
  if (!encoder_queue_.IsCurrent()) {
    encoder_queue_.PostTask(
        [this, bitrate_bps, fraction_lost, round_trip_time_ms] {
          OnBitrateUpdated(bitrate_bps, fraction_lost, round_trip_time_ms);
        });
    return;
  }
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  RTC_DCHECK(sink_) << "sink_ must be set before the encoder is active.";

  LOG(LS_VERBOSE) << "OnBitrateUpdated, bitrate " << bitrate_bps
                  << " packet loss " << static_cast<int>(fraction_lost)
                  << " rtt " << round_trip_time_ms;

  video_sender_.SetChannelParameters(bitrate_bps, fraction_lost,
                                     round_trip_time_ms);

  encoder_start_bitrate_bps_ =
      bitrate_bps != 0 ? bitrate_bps : encoder_start_bitrate_bps_;
  bool video_is_suspended = bitrate_bps == 0;
  bool video_suspension_changed =
      video_is_suspended != (last_observed_bitrate_bps_ == 0);
  last_observed_bitrate_bps_ = bitrate_bps;

  if (stats_proxy_ && video_suspension_changed) {
    LOG(LS_INFO) << "Video suspend state changed to: "
                 << (video_is_suspended ? "suspended" : "not suspended");
    stats_proxy_->OnSuspendChange(video_is_suspended);
  }
}

void ViEEncoder::OveruseDetected() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  // TODO(perkj): When ViEEncoder inherit rtc::VideoSink instead of
  // VideoCaptureInput |load_observer_| should be removed and overuse be
  // expressed as rtc::VideoSinkWants instead.
  if (load_observer_)
    load_observer_->OnLoadUpdate(LoadObserver::kOveruse);
}

void ViEEncoder::NormalUsage() {
  RTC_DCHECK_RUN_ON(&encoder_queue_);
  if (load_observer_)
    load_observer_->OnLoadUpdate(LoadObserver::kUnderuse);
}

}  // namespace webrtc
