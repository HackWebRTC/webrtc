/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <stdio.h>

#include <algorithm>
#include <deque>
#include <map>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/format_macros.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/call.h"
#include "webrtc/common_video/libyuv/include/webrtc_libyuv.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/system_wrappers/interface/cpu_info.h"
#include "webrtc/test/layer_filtering_transport.h"
#include "webrtc/test/run_loop.h"
#include "webrtc/test/statistics.h"
#include "webrtc/test/testsupport/fileutils.h"
#include "webrtc/test/video_renderer.h"
#include "webrtc/video/video_quality_test.h"

namespace webrtc {

static const int kTransportSeqExtensionId =
    VideoQualityTest::kAbsSendTimeExtensionId + 1;
static const int kSendStatsPollingIntervalMs = 1000;
static const int kPayloadTypeVP8 = 123;
static const int kPayloadTypeVP9 = 124;

class VideoAnalyzer : public PacketReceiver,
                      public Transport,
                      public VideoRenderer,
                      public VideoCaptureInput,
                      public EncodedFrameObserver,
                      public EncodingTimeObserver {
 public:
  VideoAnalyzer(Transport* transport,
                const std::string& test_label,
                double avg_psnr_threshold,
                double avg_ssim_threshold,
                int duration_frames,
                FILE* graph_data_output_file)
      : input_(nullptr),
        transport_(transport),
        receiver_(nullptr),
        send_stream_(nullptr),
        test_label_(test_label),
        graph_data_output_file_(graph_data_output_file),
        frames_to_process_(duration_frames),
        frames_recorded_(0),
        frames_processed_(0),
        dropped_frames_(0),
        last_render_time_(0),
        rtp_timestamp_delta_(0),
        avg_psnr_threshold_(avg_psnr_threshold),
        avg_ssim_threshold_(avg_ssim_threshold),
        comparison_available_event_(EventWrapper::Create()),
        done_(EventWrapper::Create()) {
    // Create thread pool for CPU-expensive PSNR/SSIM calculations.

    // Try to use about as many threads as cores, but leave kMinCoresLeft alone,
    // so that we don't accidentally starve "real" worker threads (codec etc).
    // Also, don't allocate more than kMaxComparisonThreads, even if there are
    // spare cores.

    uint32_t num_cores = CpuInfo::DetectNumberOfCores();
    RTC_DCHECK_GE(num_cores, 1u);
    static const uint32_t kMinCoresLeft = 4;
    static const uint32_t kMaxComparisonThreads = 8;

    if (num_cores <= kMinCoresLeft) {
      num_cores = 1;
    } else {
      num_cores -= kMinCoresLeft;
      num_cores = std::min(num_cores, kMaxComparisonThreads);
    }

    for (uint32_t i = 0; i < num_cores; ++i) {
      rtc::scoped_ptr<ThreadWrapper> thread =
          ThreadWrapper::CreateThread(&FrameComparisonThread, this, "Analyzer");
      EXPECT_TRUE(thread->Start());
      comparison_thread_pool_.push_back(thread.release());
    }

    stats_polling_thread_ =
        ThreadWrapper::CreateThread(&PollStatsThread, this, "StatsPoller");
    EXPECT_TRUE(stats_polling_thread_->Start());
  }

  ~VideoAnalyzer() {
    for (ThreadWrapper* thread : comparison_thread_pool_) {
      EXPECT_TRUE(thread->Stop());
      delete thread;
    }
  }

  virtual void SetReceiver(PacketReceiver* receiver) { receiver_ = receiver; }

  DeliveryStatus DeliverPacket(MediaType media_type,
                               const uint8_t* packet,
                               size_t length,
                               const PacketTime& packet_time) override {
    rtc::scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    RTPHeader header;
    parser->Parse(packet, length, &header);
    {
      rtc::CritScope lock(&crit_);
      recv_times_[header.timestamp - rtp_timestamp_delta_] =
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
    }

    return receiver_->DeliverPacket(media_type, packet, length, packet_time);
  }

  // EncodingTimeObserver.
  void OnReportEncodedTime(int64_t ntp_time_ms, int encode_time_ms) override {
    rtc::CritScope crit(&comparison_lock_);
    samples_encode_time_ms_[ntp_time_ms] = encode_time_ms;
  }

  void IncomingCapturedFrame(const VideoFrame& video_frame) override {
    VideoFrame copy = video_frame;
    copy.set_timestamp(copy.ntp_time_ms() * 90);

    {
      rtc::CritScope lock(&crit_);
      if (first_send_frame_.IsZeroSize() && rtp_timestamp_delta_ == 0)
        first_send_frame_ = copy;

      frames_.push_back(copy);
    }

    input_->IncomingCapturedFrame(video_frame);
  }

  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override {
    rtc::scoped_ptr<RtpHeaderParser> parser(RtpHeaderParser::Create());
    RTPHeader header;
    parser->Parse(packet, length, &header);

    {
      rtc::CritScope lock(&crit_);
      if (rtp_timestamp_delta_ == 0) {
        rtp_timestamp_delta_ = header.timestamp - first_send_frame_.timestamp();
        first_send_frame_.Reset();
      }
      uint32_t timestamp = header.timestamp - rtp_timestamp_delta_;
      send_times_[timestamp] =
          Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
      encoded_frame_sizes_[timestamp] +=
          length - (header.headerLength + header.paddingLength);
    }

    return transport_->SendRtp(packet, length, options);
  }

  bool SendRtcp(const uint8_t* packet, size_t length) override {
    return transport_->SendRtcp(packet, length);
  }

  void EncodedFrameCallback(const EncodedFrame& frame) override {
    rtc::CritScope lock(&comparison_lock_);
    if (frames_recorded_ < frames_to_process_)
      encoded_frame_size_.AddSample(frame.length_);
  }

  void RenderFrame(const VideoFrame& video_frame,
                   int time_to_render_ms) override {
    int64_t render_time_ms =
        Clock::GetRealTimeClock()->CurrentNtpInMilliseconds();
    uint32_t send_timestamp = video_frame.timestamp() - rtp_timestamp_delta_;

    rtc::CritScope lock(&crit_);

    while (frames_.front().timestamp() < send_timestamp) {
      AddFrameComparison(frames_.front(), last_rendered_frame_, true,
                         render_time_ms);
      frames_.pop_front();
    }

    VideoFrame reference_frame = frames_.front();
    frames_.pop_front();
    assert(!reference_frame.IsZeroSize());
    EXPECT_EQ(reference_frame.timestamp(), send_timestamp);
    assert(reference_frame.timestamp() == send_timestamp);

    AddFrameComparison(reference_frame, video_frame, false, render_time_ms);

    last_rendered_frame_ = video_frame;
  }

  bool IsTextureSupported() const override { return false; }

  void Wait() {
    // Frame comparisons can be very expensive. Wait for test to be done, but
    // at time-out check if frames_processed is going up. If so, give it more
    // time, otherwise fail. Hopefully this will reduce test flakiness.

    int last_frames_processed = -1;
    EventTypeWrapper eventType;
    int iteration = 0;
    while ((eventType = done_->Wait(VideoQualityTest::kDefaultTimeoutMs)) !=
           kEventSignaled) {
      int frames_processed;
      {
        rtc::CritScope crit(&comparison_lock_);
        frames_processed = frames_processed_;
      }

      // Print some output so test infrastructure won't think we've crashed.
      const char* kKeepAliveMessages[3] = {
          "Uh, I'm-I'm not quite dead, sir.",
          "Uh, I-I think uh, I could pull through, sir.",
          "Actually, I think I'm all right to come with you--"};
      printf("- %s\n", kKeepAliveMessages[iteration++ % 3]);

      if (last_frames_processed == -1) {
        last_frames_processed = frames_processed;
        continue;
      }
      ASSERT_GT(frames_processed, last_frames_processed)
          << "Analyzer stalled while waiting for test to finish.";
      last_frames_processed = frames_processed;
    }

    if (iteration > 0)
      printf("- Farewell, sweet Concorde!\n");

    // Signal stats polling thread if that is still waiting and stop it now,
    // since it uses the send_stream_ reference that might be reclaimed after
    // returning from this method.
    done_->Set();
    EXPECT_TRUE(stats_polling_thread_->Stop());
  }

  VideoCaptureInput* input_;
  Transport* const transport_;
  PacketReceiver* receiver_;
  VideoSendStream* send_stream_;

 private:
  struct FrameComparison {
    FrameComparison()
        : dropped(false),
          send_time_ms(0),
          recv_time_ms(0),
          render_time_ms(0),
          encoded_frame_size(0) {}

    FrameComparison(const VideoFrame& reference,
                    const VideoFrame& render,
                    bool dropped,
                    int64_t send_time_ms,
                    int64_t recv_time_ms,
                    int64_t render_time_ms,
                    size_t encoded_frame_size)
        : reference(reference),
          render(render),
          dropped(dropped),
          send_time_ms(send_time_ms),
          recv_time_ms(recv_time_ms),
          render_time_ms(render_time_ms),
          encoded_frame_size(encoded_frame_size) {}

    VideoFrame reference;
    VideoFrame render;
    bool dropped;
    int64_t send_time_ms;
    int64_t recv_time_ms;
    int64_t render_time_ms;
    size_t encoded_frame_size;
  };

  struct Sample {
    Sample(int dropped,
           int64_t input_time_ms,
           int64_t send_time_ms,
           int64_t recv_time_ms,
           int64_t render_time_ms,
           size_t encoded_frame_size,
           double psnr,
           double ssim)
        : dropped(dropped),
          input_time_ms(input_time_ms),
          send_time_ms(send_time_ms),
          recv_time_ms(recv_time_ms),
          render_time_ms(render_time_ms),
          encoded_frame_size(encoded_frame_size),
          psnr(psnr),
          ssim(ssim) {}

    int dropped;
    int64_t input_time_ms;
    int64_t send_time_ms;
    int64_t recv_time_ms;
    int64_t render_time_ms;
    size_t encoded_frame_size;
    double psnr;
    double ssim;
  };

  void AddFrameComparison(const VideoFrame& reference,
                          const VideoFrame& render,
                          bool dropped,
                          int64_t render_time_ms)
      EXCLUSIVE_LOCKS_REQUIRED(crit_) {
    int64_t send_time_ms = send_times_[reference.timestamp()];
    send_times_.erase(reference.timestamp());
    int64_t recv_time_ms = recv_times_[reference.timestamp()];
    recv_times_.erase(reference.timestamp());

    size_t encoded_size = encoded_frame_sizes_[reference.timestamp()];
    encoded_frame_sizes_.erase(reference.timestamp());

    VideoFrame reference_copy;
    VideoFrame render_copy;
    reference_copy.CopyFrame(reference);
    render_copy.CopyFrame(render);

    rtc::CritScope crit(&comparison_lock_);
    comparisons_.push_back(FrameComparison(reference_copy, render_copy, dropped,
                                           send_time_ms, recv_time_ms,
                                           render_time_ms, encoded_size));
    comparison_available_event_->Set();
  }

  static bool PollStatsThread(void* obj) {
    return static_cast<VideoAnalyzer*>(obj)->PollStats();
  }

  bool PollStats() {
    switch (done_->Wait(kSendStatsPollingIntervalMs)) {
      case kEventSignaled:
      case kEventError:
        done_->Set();  // Make sure main thread is also signaled.
        return false;
      case kEventTimeout:
        break;
      default:
        RTC_NOTREACHED();
    }

    VideoSendStream::Stats stats = send_stream_->GetStats();

    rtc::CritScope crit(&comparison_lock_);
    encode_frame_rate_.AddSample(stats.encode_frame_rate);
    encode_time_ms.AddSample(stats.avg_encode_time_ms);
    encode_usage_percent.AddSample(stats.encode_usage_percent);
    media_bitrate_bps.AddSample(stats.media_bitrate_bps);

    return true;
  }

  static bool FrameComparisonThread(void* obj) {
    return static_cast<VideoAnalyzer*>(obj)->CompareFrames();
  }

  bool CompareFrames() {
    if (AllFramesRecorded())
      return false;

    VideoFrame reference;
    VideoFrame render;
    FrameComparison comparison;

    if (!PopComparison(&comparison)) {
      // Wait until new comparison task is available, or test is done.
      // If done, wake up remaining threads waiting.
      comparison_available_event_->Wait(1000);
      if (AllFramesRecorded()) {
        comparison_available_event_->Set();
        return false;
      }
      return true;  // Try again.
    }

    PerformFrameComparison(comparison);

    if (FrameProcessed()) {
      PrintResults();
      if (graph_data_output_file_)
        PrintSamplesToFile();
      done_->Set();
      comparison_available_event_->Set();
      return false;
    }

    return true;
  }

  bool PopComparison(FrameComparison* comparison) {
    rtc::CritScope crit(&comparison_lock_);
    // If AllFramesRecorded() is true, it means we have already popped
    // frames_to_process_ frames from comparisons_, so there is no more work
    // for this thread to be done. frames_processed_ might still be lower if
    // all comparisons are not done, but those frames are currently being
    // worked on by other threads.
    if (comparisons_.empty() || AllFramesRecorded())
      return false;

    *comparison = comparisons_.front();
    comparisons_.pop_front();

    FrameRecorded();
    return true;
  }

  // Increment counter for number of frames received for comparison.
  void FrameRecorded() {
    rtc::CritScope crit(&comparison_lock_);
    ++frames_recorded_;
  }

  // Returns true if all frames to be compared have been taken from the queue.
  bool AllFramesRecorded() {
    rtc::CritScope crit(&comparison_lock_);
    assert(frames_recorded_ <= frames_to_process_);
    return frames_recorded_ == frames_to_process_;
  }

  // Increase count of number of frames processed. Returns true if this was the
  // last frame to be processed.
  bool FrameProcessed() {
    rtc::CritScope crit(&comparison_lock_);
    ++frames_processed_;
    assert(frames_processed_ <= frames_to_process_);
    return frames_processed_ == frames_to_process_;
  }

  void PrintResults() {
    rtc::CritScope crit(&comparison_lock_);
    PrintResult("psnr", psnr_, " dB");
    PrintResult("ssim", ssim_, "");
    PrintResult("sender_time", sender_time_, " ms");
    printf("RESULT dropped_frames: %s = %d frames\n", test_label_.c_str(),
           dropped_frames_);
    PrintResult("receiver_time", receiver_time_, " ms");
    PrintResult("total_delay_incl_network", end_to_end_, " ms");
    PrintResult("time_between_rendered_frames", rendered_delta_, " ms");
    PrintResult("encoded_frame_size", encoded_frame_size_, " bytes");
    PrintResult("encode_frame_rate", encode_frame_rate_, " fps");
    PrintResult("encode_time", encode_time_ms, " ms");
    PrintResult("encode_usage_percent", encode_usage_percent, " percent");
    PrintResult("media_bitrate", media_bitrate_bps, " bps");

    EXPECT_GT(psnr_.Mean(), avg_psnr_threshold_);
    EXPECT_GT(ssim_.Mean(), avg_ssim_threshold_);
  }

  void PerformFrameComparison(const FrameComparison& comparison) {
    // Perform expensive psnr and ssim calculations while not holding lock.
    double psnr = I420PSNR(&comparison.reference, &comparison.render);
    double ssim = I420SSIM(&comparison.reference, &comparison.render);

    int64_t input_time_ms = comparison.reference.ntp_time_ms();

    rtc::CritScope crit(&comparison_lock_);
    if (graph_data_output_file_) {
      samples_.push_back(
          Sample(comparison.dropped, input_time_ms, comparison.send_time_ms,
                 comparison.recv_time_ms, comparison.render_time_ms,
                 comparison.encoded_frame_size, psnr, ssim));
    }
    psnr_.AddSample(psnr);
    ssim_.AddSample(ssim);

    if (comparison.dropped) {
      ++dropped_frames_;
      return;
    }
    if (last_render_time_ != 0)
      rendered_delta_.AddSample(comparison.render_time_ms - last_render_time_);
    last_render_time_ = comparison.render_time_ms;

    sender_time_.AddSample(comparison.send_time_ms - input_time_ms);
    receiver_time_.AddSample(comparison.render_time_ms -
                             comparison.recv_time_ms);
    end_to_end_.AddSample(comparison.render_time_ms - input_time_ms);
    encoded_frame_size_.AddSample(comparison.encoded_frame_size);
  }

  void PrintResult(const char* result_type,
                   test::Statistics stats,
                   const char* unit) {
    printf("RESULT %s: %s = {%f, %f}%s\n",
           result_type,
           test_label_.c_str(),
           stats.Mean(),
           stats.StandardDeviation(),
           unit);
  }

  void PrintSamplesToFile(void) {
    FILE* out = graph_data_output_file_;
    rtc::CritScope crit(&comparison_lock_);
    std::sort(samples_.begin(), samples_.end(),
              [](const Sample& A, const Sample& B) -> bool {
                return A.input_time_ms < B.input_time_ms;
              });

    fprintf(out, "%s\n", test_label_.c_str());
    fprintf(out, "%" PRIuS "\n", samples_.size());
    fprintf(out,
            "dropped "
            "input_time_ms "
            "send_time_ms "
            "recv_time_ms "
            "render_time_ms "
            "encoded_frame_size "
            "psnr "
            "ssim "
            "encode_time_ms\n");
    int missing_encode_time_samples = 0;
    for (const Sample& sample : samples_) {
      auto it = samples_encode_time_ms_.find(sample.input_time_ms);
      int encode_time_ms;
      if (it != samples_encode_time_ms_.end()) {
        encode_time_ms = it->second;
      } else {
        ++missing_encode_time_samples;
        encode_time_ms = -1;
      }
      fprintf(out, "%d %" PRId64 " %" PRId64 " %" PRId64 " %" PRId64 " %" PRIuS
                   " %lf %lf %d\n",
              sample.dropped, sample.input_time_ms, sample.send_time_ms,
              sample.recv_time_ms, sample.render_time_ms,
              sample.encoded_frame_size, sample.psnr, sample.ssim,
              encode_time_ms);
    }
    if (missing_encode_time_samples) {
      fprintf(stderr,
              "Warning: Missing encode_time_ms samples for %d frame(s).\n",
              missing_encode_time_samples);
    }
  }

  const std::string test_label_;
  FILE* const graph_data_output_file_;
  std::vector<Sample> samples_ GUARDED_BY(comparison_lock_);
  std::map<int64_t, int> samples_encode_time_ms_ GUARDED_BY(comparison_lock_);
  test::Statistics sender_time_ GUARDED_BY(comparison_lock_);
  test::Statistics receiver_time_ GUARDED_BY(comparison_lock_);
  test::Statistics psnr_ GUARDED_BY(comparison_lock_);
  test::Statistics ssim_ GUARDED_BY(comparison_lock_);
  test::Statistics end_to_end_ GUARDED_BY(comparison_lock_);
  test::Statistics rendered_delta_ GUARDED_BY(comparison_lock_);
  test::Statistics encoded_frame_size_ GUARDED_BY(comparison_lock_);
  test::Statistics encode_frame_rate_ GUARDED_BY(comparison_lock_);
  test::Statistics encode_time_ms GUARDED_BY(comparison_lock_);
  test::Statistics encode_usage_percent GUARDED_BY(comparison_lock_);
  test::Statistics media_bitrate_bps GUARDED_BY(comparison_lock_);

  const int frames_to_process_;
  int frames_recorded_;
  int frames_processed_;
  int dropped_frames_;
  int64_t last_render_time_;
  uint32_t rtp_timestamp_delta_;

  rtc::CriticalSection crit_;
  std::deque<VideoFrame> frames_ GUARDED_BY(crit_);
  VideoFrame last_rendered_frame_ GUARDED_BY(crit_);
  std::map<uint32_t, int64_t> send_times_ GUARDED_BY(crit_);
  std::map<uint32_t, int64_t> recv_times_ GUARDED_BY(crit_);
  std::map<uint32_t, size_t> encoded_frame_sizes_ GUARDED_BY(crit_);
  VideoFrame first_send_frame_ GUARDED_BY(crit_);
  const double avg_psnr_threshold_;
  const double avg_ssim_threshold_;

  rtc::CriticalSection comparison_lock_;
  std::vector<ThreadWrapper*> comparison_thread_pool_;
  rtc::scoped_ptr<ThreadWrapper> stats_polling_thread_;
  const rtc::scoped_ptr<EventWrapper> comparison_available_event_;
  std::deque<FrameComparison> comparisons_ GUARDED_BY(comparison_lock_);
  const rtc::scoped_ptr<EventWrapper> done_;
};

VideoQualityTest::VideoQualityTest() : clock_(Clock::GetRealTimeClock()) {}

void VideoQualityTest::ValidateParams(const Params& params) {
  RTC_CHECK_GE(params.common.max_bitrate_bps, params.common.target_bitrate_bps);
  RTC_CHECK_GE(params.common.target_bitrate_bps, params.common.min_bitrate_bps);
  RTC_CHECK_LT(params.common.tl_discard_threshold,
               params.common.num_temporal_layers);
}

void VideoQualityTest::TestBody() {}

void VideoQualityTest::SetupFullStack(const Params& params,
                                      Transport* send_transport,
                                      Transport* recv_transport) {
  if (params.logs)
    trace_to_stderr_.reset(new test::TraceToStderr);

  CreateSendConfig(1, send_transport);

  int payload_type;
  if (params.common.codec == "VP8") {
    encoder_.reset(VideoEncoder::Create(VideoEncoder::kVp8));
    payload_type = kPayloadTypeVP8;
  } else if (params.common.codec == "VP9") {
    encoder_.reset(VideoEncoder::Create(VideoEncoder::kVp9));
    payload_type = kPayloadTypeVP9;
  } else {
    RTC_NOTREACHED() << "Codec not supported!";
    return;
  }
  send_config_.encoder_settings.encoder = encoder_.get();
  send_config_.encoder_settings.payload_name = params.common.codec;
  send_config_.encoder_settings.payload_type = payload_type;

  send_config_.rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
  send_config_.rtp.rtx.ssrcs.push_back(kSendRtxSsrcs[0]);
  send_config_.rtp.rtx.payload_type = kSendRtxPayloadType;

  send_config_.rtp.extensions.clear();
  if (params.common.send_side_bwe) {
    send_config_.rtp.extensions.push_back(RtpExtension(
        RtpExtension::kTransportSequenceNumber, kTransportSeqExtensionId));
  } else {
    send_config_.rtp.extensions.push_back(
        RtpExtension(RtpExtension::kAbsSendTime, kAbsSendTimeExtensionId));
  }

  // Automatically fill out streams[0] with params.
  VideoStream* stream = &encoder_config_.streams[0];
  stream->width = params.common.width;
  stream->height = params.common.height;
  stream->min_bitrate_bps = params.common.min_bitrate_bps;
  stream->target_bitrate_bps = params.common.target_bitrate_bps;
  stream->max_bitrate_bps = params.common.max_bitrate_bps;
  stream->max_framerate = static_cast<int>(params.common.fps);

  stream->temporal_layer_thresholds_bps.clear();
  if (params.common.num_temporal_layers > 1) {
    stream->temporal_layer_thresholds_bps.push_back(stream->target_bitrate_bps);
  }

  CreateMatchingReceiveConfigs(recv_transport);

  receive_configs_[0].rtp.nack.rtp_history_ms = kNackRtpHistoryMs;
  receive_configs_[0].rtp.rtx[kSendRtxPayloadType].ssrc = kSendRtxSsrcs[0];
  receive_configs_[0].rtp.rtx[kSendRtxPayloadType].payload_type =
      kSendRtxPayloadType;

  encoder_config_.min_transmit_bitrate_bps = params.common.min_transmit_bps;
}

void VideoQualityTest::SetupScreenshare(const Params& params) {
  RTC_CHECK(params.screenshare.enabled);

  // Fill out codec settings.
  encoder_config_.content_type = VideoEncoderConfig::ContentType::kScreen;
  if (params.common.codec == "VP8") {
    codec_settings_.VP8 = VideoEncoder::GetDefaultVp8Settings();
    codec_settings_.VP8.denoisingOn = false;
    codec_settings_.VP8.frameDroppingOn = false;
    codec_settings_.VP8.numberOfTemporalLayers =
        static_cast<unsigned char>(params.common.num_temporal_layers);
    encoder_config_.encoder_specific_settings = &codec_settings_.VP8;
  } else if (params.common.codec == "VP9") {
    codec_settings_.VP9 = VideoEncoder::GetDefaultVp9Settings();
    codec_settings_.VP9.denoisingOn = false;
    codec_settings_.VP9.frameDroppingOn = false;
    codec_settings_.VP9.numberOfTemporalLayers =
        static_cast<unsigned char>(params.common.num_temporal_layers);
    encoder_config_.encoder_specific_settings = &codec_settings_.VP9;
  }

  // Setup frame generator.
  const size_t kWidth = 1850;
  const size_t kHeight = 1110;
  std::vector<std::string> slides;
  slides.push_back(test::ResourcePath("web_screenshot_1850_1110", "yuv"));
  slides.push_back(test::ResourcePath("presentation_1850_1110", "yuv"));
  slides.push_back(test::ResourcePath("photo_1850_1110", "yuv"));
  slides.push_back(test::ResourcePath("difficult_photo_1850_1110", "yuv"));

  if (params.screenshare.scroll_duration == 0) {
    // Cycle image every slide_change_interval seconds.
    frame_generator_.reset(test::FrameGenerator::CreateFromYuvFile(
        slides, kWidth, kHeight,
        params.screenshare.slide_change_interval * params.common.fps));
  } else {
    RTC_CHECK_LE(params.common.width, kWidth);
    RTC_CHECK_LE(params.common.height, kHeight);
    RTC_CHECK_GT(params.screenshare.slide_change_interval, 0);
    const int kPauseDurationMs = (params.screenshare.slide_change_interval -
                                  params.screenshare.scroll_duration) * 1000;
    RTC_CHECK_LE(params.screenshare.scroll_duration,
                 params.screenshare.slide_change_interval);

    if (params.screenshare.scroll_duration) {
      frame_generator_.reset(
          test::FrameGenerator::CreateScrollingInputFromYuvFiles(
              clock_, slides, kWidth, kHeight, params.common.width,
              params.common.height, params.screenshare.scroll_duration * 1000,
              kPauseDurationMs));
    } else {
      frame_generator_.reset(test::FrameGenerator::CreateFromYuvFile(
              slides, kWidth, kHeight,
              params.screenshare.slide_change_interval * params.common.fps));
    }
  }
}

void VideoQualityTest::CreateCapturer(const Params& params,
                                      VideoCaptureInput* input) {
  if (params.screenshare.enabled) {
    test::FrameGeneratorCapturer *frame_generator_capturer =
        new test::FrameGeneratorCapturer(
            clock_, input, frame_generator_.release(), params.common.fps);
    EXPECT_TRUE(frame_generator_capturer->Init());
    capturer_.reset(frame_generator_capturer);
  } else {
    if (params.video.clip_name.empty()) {
      capturer_.reset(test::VideoCapturer::Create(
          input, params.common.width, params.common.height, params.common.fps,
          clock_));
    } else {
      capturer_.reset(test::FrameGeneratorCapturer::CreateFromYuvFile(
          input, test::ResourcePath(params.video.clip_name, "yuv"),
          params.common.width, params.common.height, params.common.fps,
          clock_));
      ASSERT_TRUE(capturer_.get() != nullptr)
          << "Could not create capturer for " << params.video.clip_name
          << ".yuv. Is this resource file present?";
    }
  }
}

void VideoQualityTest::RunWithAnalyzer(const Params& params) {
  // TODO(ivica): Merge with RunWithRenderer and use a flag / argument to
  // differentiate between the analyzer and the renderer case.
  ValidateParams(params);

  FILE* graph_data_output_file = nullptr;
  if (!params.analyzer.graph_data_output_filename.empty()) {
    graph_data_output_file =
        fopen(params.analyzer.graph_data_output_filename.c_str(), "w");
    RTC_CHECK(graph_data_output_file != nullptr)
        << "Can't open the file "
        << params.analyzer.graph_data_output_filename << "!";
  }

  Call::Config call_config;
  call_config.bitrate_config = params.common.call_bitrate_config;
  CreateCalls(call_config, call_config);

  test::LayerFilteringTransport send_transport(
      params.pipe, sender_call_.get(), kPayloadTypeVP8, kPayloadTypeVP9,
      static_cast<uint8_t>(params.common.tl_discard_threshold), 0);
  test::DirectTransport recv_transport(params.pipe, receiver_call_.get());

  VideoAnalyzer analyzer(
      &send_transport, params.analyzer.test_label,
      params.analyzer.avg_psnr_threshold, params.analyzer.avg_ssim_threshold,
      params.analyzer.test_durations_secs * params.common.fps,
      graph_data_output_file);

  analyzer.SetReceiver(receiver_call_->Receiver());
  send_transport.SetReceiver(&analyzer);
  recv_transport.SetReceiver(sender_call_->Receiver());

  SetupFullStack(params, &analyzer, &recv_transport);
  send_config_.encoding_time_observer = &analyzer;
  receive_configs_[0].renderer = &analyzer;
  for (auto& config : receive_configs_)
    config.pre_decode_callback = &analyzer;

  if (params.screenshare.enabled)
    SetupScreenshare(params);

  CreateStreams();
  analyzer.input_ = send_stream_->Input();
  analyzer.send_stream_ = send_stream_;

  CreateCapturer(params, &analyzer);

  send_stream_->Start();
  for (size_t i = 0; i < receive_streams_.size(); ++i)
    receive_streams_[i]->Start();
  capturer_->Start();

  analyzer.Wait();

  send_transport.StopSending();
  recv_transport.StopSending();

  capturer_->Stop();
  for (size_t i = 0; i < receive_streams_.size(); ++i)
    receive_streams_[i]->Stop();
  send_stream_->Stop();

  DestroyStreams();

  if (graph_data_output_file)
    fclose(graph_data_output_file);
}

void VideoQualityTest::RunWithVideoRenderer(const Params& params) {
  ValidateParams(params);

  rtc::scoped_ptr<test::VideoRenderer> local_preview(
      test::VideoRenderer::Create("Local Preview", params.common.width,
                                  params.common.height));
  rtc::scoped_ptr<test::VideoRenderer> loopback_video(
      test::VideoRenderer::Create("Loopback Video", params.common.width,
                                  params.common.height));

  // TODO(ivica): Remove bitrate_config and use the default Call::Config(), to
  // match the full stack tests.
  Call::Config call_config;
  call_config.bitrate_config = params.common.call_bitrate_config;
  rtc::scoped_ptr<Call> call(Call::Create(call_config));

  test::LayerFilteringTransport transport(
      params.pipe, call.get(), kPayloadTypeVP8, kPayloadTypeVP9,
      static_cast<uint8_t>(params.common.tl_discard_threshold), 0);
  // TODO(ivica): Use two calls to be able to merge with RunWithAnalyzer or at
  // least share as much code as possible. That way this test would also match
  // the full stack tests better.
  transport.SetReceiver(call->Receiver());

  SetupFullStack(params, &transport, &transport);
  send_config_.local_renderer = local_preview.get();
  receive_configs_[0].renderer = loopback_video.get();

  if (params.screenshare.enabled)
    SetupScreenshare(params);

  send_stream_ = call->CreateVideoSendStream(send_config_, encoder_config_);
  VideoReceiveStream* receive_stream =
      call->CreateVideoReceiveStream(receive_configs_[0]);
  CreateCapturer(params, send_stream_->Input());

  receive_stream->Start();
  send_stream_->Start();
  capturer_->Start();

  test::PressEnterToContinue();

  capturer_->Stop();
  send_stream_->Stop();
  receive_stream->Stop();

  call->DestroyVideoReceiveStream(receive_stream);
  call->DestroyVideoSendStream(send_stream_);

  transport.StopSending();
}

}  // namespace webrtc
