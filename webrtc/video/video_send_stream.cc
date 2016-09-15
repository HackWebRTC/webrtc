/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/video/video_send_stream.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/bitrate_controller/include/bitrate_controller.h"
#include "webrtc/modules/congestion_controller/include/congestion_controller.h"
#include "webrtc/modules/pacing/packet_router.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/modules/video_coding/utility/ivf_file_writer.h"
#include "webrtc/video/call_stats.h"
#include "webrtc/video/vie_remb.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

static const int kMinSendSidePacketHistorySize = 600;
namespace {

std::vector<RtpRtcp*> CreateRtpRtcpModules(
    Transport* outgoing_transport,
    RtcpIntraFrameObserver* intra_frame_callback,
    RtcpBandwidthObserver* bandwidth_callback,
    TransportFeedbackObserver* transport_feedback_callback,
    RtcpRttStats* rtt_stats,
    RtpPacketSender* paced_sender,
    TransportSequenceNumberAllocator* transport_sequence_number_allocator,
    SendStatisticsProxy* stats_proxy,
    SendDelayStats* send_delay_stats,
    RtcEventLog* event_log,
    RateLimiter* retransmission_rate_limiter,
    size_t num_modules) {
  RTC_DCHECK_GT(num_modules, 0u);
  RtpRtcp::Configuration configuration;
  ReceiveStatistics* null_receive_statistics = configuration.receive_statistics;
  configuration.audio = false;
  configuration.receiver_only = false;
  configuration.receive_statistics = null_receive_statistics;
  configuration.outgoing_transport = outgoing_transport;
  configuration.intra_frame_callback = intra_frame_callback;
  configuration.bandwidth_callback = bandwidth_callback;
  configuration.transport_feedback_callback = transport_feedback_callback;
  configuration.rtt_stats = rtt_stats;
  configuration.rtcp_packet_type_counter_observer = stats_proxy;
  configuration.paced_sender = paced_sender;
  configuration.transport_sequence_number_allocator =
      transport_sequence_number_allocator;
  configuration.send_bitrate_observer = stats_proxy;
  configuration.send_frame_count_observer = stats_proxy;
  configuration.send_side_delay_observer = stats_proxy;
  configuration.send_packet_observer = send_delay_stats;
  configuration.event_log = event_log;
  configuration.retransmission_rate_limiter = retransmission_rate_limiter;

  std::vector<RtpRtcp*> modules;
  for (size_t i = 0; i < num_modules; ++i) {
    RtpRtcp* rtp_rtcp = RtpRtcp::CreateRtpRtcp(configuration);
    rtp_rtcp->SetSendingStatus(false);
    rtp_rtcp->SetSendingMediaStatus(false);
    rtp_rtcp->SetRTCPStatus(RtcpMode::kCompound);
    modules.push_back(rtp_rtcp);
  }
  return modules;
}

}  // namespace

std::string
VideoSendStream::Config::EncoderSettings::ToString() const {
  std::stringstream ss;
  ss << "{payload_name: " << payload_name;
  ss << ", payload_type: " << payload_type;
  ss << ", encoder: " << (encoder ? "(VideoEncoder)" : "nullptr");
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::Rtp::Rtx::ToString()
    const {
  std::stringstream ss;
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", payload_type: " << payload_type;
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::Rtp::ToString() const {
  std::stringstream ss;
  ss << "{ssrcs: [";
  for (size_t i = 0; i < ssrcs.size(); ++i) {
    ss << ssrcs[i];
    if (i != ssrcs.size() - 1)
      ss << ", ";
  }
  ss << ']';
  ss << ", rtcp_mode: "
     << (rtcp_mode == RtcpMode::kCompound ? "RtcpMode::kCompound"
                                          : "RtcpMode::kReducedSize");
  ss << ", max_packet_size: " << max_packet_size;
  ss << ", extensions: [";
  for (size_t i = 0; i < extensions.size(); ++i) {
    ss << extensions[i].ToString();
    if (i != extensions.size() - 1)
      ss << ", ";
  }
  ss << ']';

  ss << ", nack: {rtp_history_ms: " << nack.rtp_history_ms << '}';
  ss << ", fec: " << fec.ToString();
  ss << ", rtx: " << rtx.ToString();
  ss << ", c_name: " << c_name;
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Config::ToString() const {
  std::stringstream ss;
  ss << "{encoder_settings: " << encoder_settings.ToString();
  ss << ", rtp: " << rtp.ToString();
  ss << ", pre_encode_callback: "
     << (pre_encode_callback ? "(I420FrameCallback)" : "nullptr");
  ss << ", post_encode_callback: "
     << (post_encode_callback ? "(EncodedFrameObserver)" : "nullptr");
  ss << ", render_delay_ms: " << render_delay_ms;
  ss << ", target_delay_ms: " << target_delay_ms;
  ss << ", suspend_below_min_bitrate: " << (suspend_below_min_bitrate ? "on"
                                                                      : "off");
  ss << '}';
  return ss.str();
}

std::string VideoSendStream::Stats::ToString(int64_t time_ms) const {
  std::stringstream ss;
  ss << "VideoSendStream stats: " << time_ms << ", {";
  ss << "input_fps: " << input_frame_rate << ", ";
  ss << "encode_fps: " << encode_frame_rate << ", ";
  ss << "encode_ms: " << avg_encode_time_ms << ", ";
  ss << "encode_usage_perc: " << encode_usage_percent << ", ";
  ss << "target_bps: " << target_media_bitrate_bps << ", ";
  ss << "media_bps: " << media_bitrate_bps << ", ";
  ss << "suspended: " << (suspended ? "true" : "false") << ", ";
  ss << "bw_adapted: " << (bw_limited_resolution ? "true" : "false");
  ss << '}';
  for (const auto& substream : substreams) {
    if (!substream.second.is_rtx) {
      ss << " {ssrc: " << substream.first << ", ";
      ss << substream.second.ToString();
      ss << '}';
    }
  }
  return ss.str();
}

std::string VideoSendStream::StreamStats::ToString() const {
  std::stringstream ss;
  ss << "width: " << width << ", ";
  ss << "height: " << height << ", ";
  ss << "key: " << frame_counts.key_frames << ", ";
  ss << "delta: " << frame_counts.delta_frames << ", ";
  ss << "total_bps: " << total_bitrate_bps << ", ";
  ss << "retransmit_bps: " << retransmit_bitrate_bps << ", ";
  ss << "avg_delay_ms: " << avg_delay_ms << ", ";
  ss << "max_delay_ms: " << max_delay_ms << ", ";
  ss << "cum_loss: " << rtcp_stats.cumulative_lost << ", ";
  ss << "max_ext_seq: " << rtcp_stats.extended_max_sequence_number << ", ";
  ss << "nack: " << rtcp_packet_type_counts.nack_packets << ", ";
  ss << "fir: " << rtcp_packet_type_counts.fir_packets << ", ";
  ss << "pli: " << rtcp_packet_type_counts.pli_packets;
  return ss.str();
}

namespace {

bool PayloadTypeSupportsSkippingFecPackets(const std::string& payload_name) {
  if (payload_name == "VP8" || payload_name == "VP9")
    return true;
  RTC_DCHECK(payload_name == "H264" || payload_name == "FAKE")
      << "unknown payload_name " << payload_name;
  return false;
}

int CalculateMaxPadBitrateBps(const VideoEncoderConfig& config,
                              bool pad_to_min_bitrate) {
  int pad_up_to_bitrate_bps = 0;
  // Calculate max padding bitrate for a multi layer codec.
  if (config.streams.size() > 1) {
    // Pad to min bitrate of the highest layer.
    pad_up_to_bitrate_bps =
        config.streams[config.streams.size() - 1].min_bitrate_bps;
    // Add target_bitrate_bps of the lower layers.
    for (size_t i = 0; i < config.streams.size() - 1; ++i)
      pad_up_to_bitrate_bps += config.streams[i].target_bitrate_bps;
  } else if (pad_to_min_bitrate) {
    pad_up_to_bitrate_bps = config.streams[0].min_bitrate_bps;
  }

  pad_up_to_bitrate_bps =
      std::max(pad_up_to_bitrate_bps, config.min_transmit_bitrate_bps);

  return pad_up_to_bitrate_bps;
}

}  // namespace

namespace internal {

// VideoSendStreamImpl implements internal::VideoSendStream.
// It is created and destroyed on |worker_queue|. The intent is to decrease the
// need for locking and to ensure methods are called in sequence.
// Public methods except |DeliverRtcp| must be called on |worker_queue|.
// DeliverRtcp is called on the libjingle worker thread or a network thread.
// An encoder may deliver frames through the EncodedImageCallback on an
// arbitrary thread.
class VideoSendStreamImpl : public webrtc::BitrateAllocatorObserver,
                            public webrtc::VCMProtectionCallback,
                            public EncodedImageCallback {
 public:
  VideoSendStreamImpl(SendStatisticsProxy* stats_proxy,
                      rtc::TaskQueue* worker_queue,
                      CallStats* call_stats,
                      CongestionController* congestion_controller,
                      BitrateAllocator* bitrate_allocator,
                      SendDelayStats* send_delay_stats,
                      VieRemb* remb,
                      ViEEncoder* vie_encoder,
                      RtcEventLog* event_log,
                      const VideoSendStream::Config* config,
                      std::map<uint32_t, RtpState> suspended_ssrcs);
  ~VideoSendStreamImpl() override;

  // RegisterProcessThread register |module_process_thread| with those objects
  // that use it. Registration has to happen on the thread were
  // |module_process_thread| was created (libjingle's worker thread).
  // TODO(perkj): Replace the use of |module_process_thread| with a TaskQueue,
  // maybe |worker_queue|.
  void RegisterProcessThread(ProcessThread* module_process_thread);
  void DeRegisterProcessThread();

  void SignalNetworkState(NetworkState state);
  bool DeliverRtcp(const uint8_t* packet, size_t length);
  void Start();
  void Stop();

  void SignalEncoderConfigurationChanged(const VideoEncoderConfig& config);
  VideoSendStream::RtpStateMap GetRtpStates() const;

 private:
  class CheckEncoderActivityTask;

  // Implements BitrateAllocatorObserver.
  uint32_t OnBitrateUpdated(uint32_t bitrate_bps,
                            uint8_t fraction_loss,
                            int64_t rtt) override;

  // Implements webrtc::VCMProtectionCallback.
  int ProtectionRequest(const FecProtectionParams* delta_params,
                        const FecProtectionParams* key_params,
                        uint32_t* sent_video_rate_bps,
                        uint32_t* sent_nack_rate_bps,
                        uint32_t* sent_fec_rate_bps) override;

  // Implements EncodedImageCallback. The implementation routes encoded frames
  // to the |payload_router_| and |config.pre_encode_callback| if set.
  // Called on an arbitrary encoder callback thread.
  EncodedImageCallback::Result OnEncodedImage(
      const EncodedImage& encoded_image,
      const CodecSpecificInfo* codec_specific_info,
      const RTPFragmentationHeader* fragmentation) override;

  void ConfigureProtection();
  void ConfigureSsrcs();
  void SignalEncoderTimedOut();
  void SignalEncoderActive();

  SendStatisticsProxy* const stats_proxy_;
  const VideoSendStream::Config* const config_;
  std::map<uint32_t, RtpState> suspended_ssrcs_;

  ProcessThread* module_process_thread_;
  rtc::ThreadChecker module_process_thread_checker_;
  rtc::TaskQueue* const worker_queue_;

  rtc::CriticalSection encoder_activity_crit_sect_;
  CheckEncoderActivityTask* check_encoder_activity_task_
      GUARDED_BY(encoder_activity_crit_sect_);
  CallStats* const call_stats_;
  CongestionController* const congestion_controller_;
  BitrateAllocator* const bitrate_allocator_;
  VieRemb* const remb_;

  static const bool kEnableFrameRecording = false;
  static const int kMaxLayers = 3;
  std::unique_ptr<IvfFileWriter> file_writers_[kMaxLayers];

  int max_padding_bitrate_;
  int encoder_min_bitrate_bps_;
  uint32_t encoder_max_bitrate_bps_;
  uint32_t encoder_target_rate_bps_;

  ViEEncoder* const vie_encoder_;
  EncoderStateFeedback encoder_feedback_;
  ProtectionBitrateCalculator protection_bitrate_calculator_;

  const std::unique_ptr<RtcpBandwidthObserver> bandwidth_observer_;
  // RtpRtcp modules, declared here as they use other members on construction.
  const std::vector<RtpRtcp*> rtp_rtcp_modules_;
  PayloadRouter payload_router_;
};

// TODO(tommi): See if there's a more elegant way to create a task that creates
// an object on the correct task queue.
class VideoSendStream::ConstructionTask : public rtc::QueuedTask {
 public:
  ConstructionTask(std::unique_ptr<VideoSendStreamImpl>* send_stream,
                   rtc::Event* done_event,
                   SendStatisticsProxy* stats_proxy,
                   ViEEncoder* vie_encoder,
                   ProcessThread* module_process_thread,
                   CallStats* call_stats,
                   CongestionController* congestion_controller,
                   BitrateAllocator* bitrate_allocator,
                   SendDelayStats* send_delay_stats,
                   VieRemb* remb,
                   RtcEventLog* event_log,
                   const VideoSendStream::Config* config,
                   const std::map<uint32_t, RtpState>& suspended_ssrcs)
      : send_stream_(send_stream),
        done_event_(done_event),
        stats_proxy_(stats_proxy),
        vie_encoder_(vie_encoder),
        call_stats_(call_stats),
        congestion_controller_(congestion_controller),
        bitrate_allocator_(bitrate_allocator),
        send_delay_stats_(send_delay_stats),
        remb_(remb),
        event_log_(event_log),
        config_(config),
        suspended_ssrcs_(suspended_ssrcs) {}

  ~ConstructionTask() override { done_event_->Set(); }

 private:
  bool Run() override {
    send_stream_->reset(new VideoSendStreamImpl(
        stats_proxy_, rtc::TaskQueue::Current(), call_stats_,
        congestion_controller_, bitrate_allocator_, send_delay_stats_, remb_,
        vie_encoder_, event_log_, config_, std::move(suspended_ssrcs_)));
    return true;
  }

  std::unique_ptr<VideoSendStreamImpl>* const send_stream_;
  rtc::Event* const done_event_;
  SendStatisticsProxy* const stats_proxy_;
  ViEEncoder* const vie_encoder_;
  CallStats* const call_stats_;
  CongestionController* const congestion_controller_;
  BitrateAllocator* const bitrate_allocator_;
  SendDelayStats* const send_delay_stats_;
  VieRemb* const remb_;
  RtcEventLog* const event_log_;
  const VideoSendStream::Config* config_;
  std::map<uint32_t, RtpState> suspended_ssrcs_;
};

class VideoSendStream::DestructAndGetRtpStateTask : public rtc::QueuedTask {
 public:
  DestructAndGetRtpStateTask(VideoSendStream::RtpStateMap* state_map,
                             std::unique_ptr<VideoSendStreamImpl> send_stream,
                             rtc::Event* done_event)
      : state_map_(state_map),
        send_stream_(std::move(send_stream)),
        done_event_(done_event) {}

  ~DestructAndGetRtpStateTask() override { RTC_CHECK(!send_stream_); }

 private:
  bool Run() override {
    send_stream_->Stop();
    *state_map_ = send_stream_->GetRtpStates();
    send_stream_.reset();
    done_event_->Set();
    return true;
  }

  VideoSendStream::RtpStateMap* state_map_;
  std::unique_ptr<VideoSendStreamImpl> send_stream_;
  rtc::Event* done_event_;
};

// CheckEncoderActivityTask is used for tracking when the encoder last produced
// and encoded video frame. If the encoder has not produced anything the last
// kEncoderTimeOutMs we also want to stop sending padding.
class VideoSendStreamImpl::CheckEncoderActivityTask : public rtc::QueuedTask {
 public:
  static const int kEncoderTimeOutMs = 2000;
  explicit CheckEncoderActivityTask(VideoSendStreamImpl* send_stream)
      : activity_(0), send_stream_(send_stream), timed_out_(false) {}

  void Stop() {
    RTC_CHECK(task_checker_.CalledSequentially());
    send_stream_ = nullptr;
  }

  void UpdateEncoderActivity() {
    // UpdateEncoderActivity is called from VideoSendStreamImpl::Encoded on
    // whatever thread the real encoder implementation run on. In the case of
    // hardware encoders, there might be several encoders
    // running in parallel on different threads.
    rtc::AtomicOps::ReleaseStore(&activity_, 1);
  }

 private:
  bool Run() override {
    RTC_CHECK(task_checker_.CalledSequentially());
    if (!send_stream_)
      return true;
    if (!rtc::AtomicOps::AcquireLoad(&activity_)) {
      if (!timed_out_) {
        send_stream_->SignalEncoderTimedOut();
      }
      timed_out_ = true;
    } else if (timed_out_) {
      send_stream_->SignalEncoderActive();
      timed_out_ = false;
    }
    rtc::AtomicOps::ReleaseStore(&activity_, 0);

    rtc::TaskQueue::Current()->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), kEncoderTimeOutMs);
    // Return false to prevent this task from being deleted. Ownership has been
    // transferred to the task queue when PostDelayedTask was called.
    return false;
  }
  volatile int activity_;

  rtc::SequencedTaskChecker task_checker_;
  VideoSendStreamImpl* send_stream_;
  bool timed_out_;
};

class ReconfigureVideoEncoderTask : public rtc::QueuedTask {
 public:
  ReconfigureVideoEncoderTask(VideoSendStreamImpl* send_stream,
                              VideoEncoderConfig config)
      : send_stream_(send_stream), config_(std::move(config)) {}

 private:
  bool Run() override {
    send_stream_->SignalEncoderConfigurationChanged(std::move(config_));
    return true;
  }

  VideoSendStreamImpl* send_stream_;
  VideoEncoderConfig config_;
};

VideoSendStream::VideoSendStream(
    int num_cpu_cores,
    ProcessThread* module_process_thread,
    rtc::TaskQueue* worker_queue,
    CallStats* call_stats,
    CongestionController* congestion_controller,
    BitrateAllocator* bitrate_allocator,
    SendDelayStats* send_delay_stats,
    VieRemb* remb,
    RtcEventLog* event_log,
    VideoSendStream::Config config,
    VideoEncoderConfig encoder_config,
    const std::map<uint32_t, RtpState>& suspended_ssrcs)
    : worker_queue_(worker_queue),
      thread_sync_event_(false /* manual_reset */, false),
      stats_proxy_(Clock::GetRealTimeClock(),
                   config,
                   encoder_config.content_type),
      config_(std::move(config)) {
  vie_encoder_.reset(
      new ViEEncoder(num_cpu_cores, &stats_proxy_, config_.encoder_settings,
                     config_.pre_encode_callback, config_.overuse_callback,
                     config_.post_encode_callback));

  worker_queue_->PostTask(std::unique_ptr<rtc::QueuedTask>(new ConstructionTask(
      &send_stream_, &thread_sync_event_, &stats_proxy_, vie_encoder_.get(),
      module_process_thread, call_stats, congestion_controller,
      bitrate_allocator, send_delay_stats, remb, event_log, &config_,
      suspended_ssrcs)));

  // Wait for ConstructionTask to complete so that |send_stream_| can be used.
  // |module_process_thread| must be registered and deregistered on the thread
  // it was created on.
  thread_sync_event_.Wait(rtc::Event::kForever);
  send_stream_->RegisterProcessThread(module_process_thread);

  vie_encoder_->RegisterProcessThread(module_process_thread);

  ReconfigureVideoEncoder(std::move(encoder_config));
}

VideoSendStream::~VideoSendStream() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  RTC_DCHECK(!send_stream_);
}

void VideoSendStream::Start() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  LOG(LS_INFO) << "VideoSendStream::Start";
  VideoSendStreamImpl* send_stream = send_stream_.get();
  worker_queue_->PostTask([this, send_stream] {
    send_stream->Start();
    thread_sync_event_.Set();
  });

  // It is expected that after VideoSendStream::Start has been called, incoming
  // frames are not dropped in ViEEncoder. To ensure this, Start has to be
  // synchronized.
  thread_sync_event_.Wait(rtc::Event::kForever);
}

void VideoSendStream::Stop() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  LOG(LS_INFO) << "VideoSendStream::Stop";
  VideoSendStreamImpl* send_stream = send_stream_.get();
  worker_queue_->PostTask([send_stream] { send_stream->Stop(); });
}

void VideoSendStream::SetSource(
    rtc::VideoSourceInterface<webrtc::VideoFrame>* source) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  vie_encoder_->SetSource(source);
}

void VideoSendStream::ReconfigureVideoEncoder(VideoEncoderConfig config) {
  // ReconfigureVideoEncoder will be called on the thread that deliverers video
  // frames. We must change the encoder settings immediately so that
  // the codec settings matches the next frame.
  // TODO(perkj): Move logic for reconfiguration the encoder due to frame size
  // change from WebRtcVideoChannel2::WebRtcVideoSendStream::OnFrame to
  // be internally handled by ViEEncoder.
  vie_encoder_->ConfigureEncoder(config, config_.rtp.max_packet_size);

  worker_queue_->PostTask(std::unique_ptr<rtc::QueuedTask>(
      new ReconfigureVideoEncoderTask(send_stream_.get(), std::move(config))));
}

VideoSendStream::Stats VideoSendStream::GetStats() {
  // TODO(perkj, solenberg): Some test cases in EndToEndTest call GetStats from
  // a network thread. See comment in Call::GetStats().
  // RTC_DCHECK_RUN_ON(&thread_checker_);
  return stats_proxy_.GetStats();
}

void VideoSendStream::SignalNetworkState(NetworkState state) {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  VideoSendStreamImpl* send_stream = send_stream_.get();
  worker_queue_->PostTask(
      [send_stream, state] { send_stream->SignalNetworkState(state); });
}

VideoSendStream::RtpStateMap VideoSendStream::StopPermanentlyAndGetRtpStates() {
  RTC_DCHECK_RUN_ON(&thread_checker_);
  vie_encoder_->Stop();
  vie_encoder_->DeRegisterProcessThread();
  VideoSendStream::RtpStateMap state_map;
  send_stream_->DeRegisterProcessThread();
  worker_queue_->PostTask(
      std::unique_ptr<rtc::QueuedTask>(new DestructAndGetRtpStateTask(
          &state_map, std::move(send_stream_), &thread_sync_event_)));
  thread_sync_event_.Wait(rtc::Event::kForever);
  return state_map;
}

bool VideoSendStream::DeliverRtcp(const uint8_t* packet, size_t length) {
  // Called on a network thread.
  return send_stream_->DeliverRtcp(packet, length);
}

VideoSendStreamImpl::VideoSendStreamImpl(
    SendStatisticsProxy* stats_proxy,
    rtc::TaskQueue* worker_queue,
    CallStats* call_stats,
    CongestionController* congestion_controller,
    BitrateAllocator* bitrate_allocator,
    SendDelayStats* send_delay_stats,
    VieRemb* remb,
    ViEEncoder* vie_encoder,
    RtcEventLog* event_log,
    const VideoSendStream::Config* config,
    std::map<uint32_t, RtpState> suspended_ssrcs)
    : stats_proxy_(stats_proxy),
      config_(config),
      suspended_ssrcs_(std::move(suspended_ssrcs)),
      module_process_thread_(nullptr),
      worker_queue_(worker_queue),
      check_encoder_activity_task_(nullptr),
      call_stats_(call_stats),
      congestion_controller_(congestion_controller),
      bitrate_allocator_(bitrate_allocator),
      remb_(remb),
      max_padding_bitrate_(0),
      encoder_min_bitrate_bps_(0),
      encoder_max_bitrate_bps_(0),
      encoder_target_rate_bps_(0),
      vie_encoder_(vie_encoder),
      encoder_feedback_(Clock::GetRealTimeClock(),
                        config_->rtp.ssrcs,
                        vie_encoder),
      protection_bitrate_calculator_(Clock::GetRealTimeClock(), this),
      bandwidth_observer_(congestion_controller_->GetBitrateController()
                              ->CreateRtcpBandwidthObserver()),
      rtp_rtcp_modules_(CreateRtpRtcpModules(
          config_->send_transport,
          &encoder_feedback_,
          bandwidth_observer_.get(),
          congestion_controller_->GetTransportFeedbackObserver(),
          call_stats_->rtcp_rtt_stats(),
          congestion_controller_->pacer(),
          congestion_controller_->packet_router(),
          stats_proxy_,
          send_delay_stats,
          event_log,
          congestion_controller_->GetRetransmissionRateLimiter(),
          config_->rtp.ssrcs.size())),
      payload_router_(rtp_rtcp_modules_,
                      config_->encoder_settings.payload_type) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  LOG(LS_INFO) << "VideoSendStreamInternal: " << config_->ToString();
  module_process_thread_checker_.DetachFromThread();

  RTC_DCHECK(!config_->rtp.ssrcs.empty());
  RTC_DCHECK(call_stats_);
  RTC_DCHECK(congestion_controller_);
  RTC_DCHECK(remb_);

  // RTP/RTCP initialization.
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    congestion_controller_->packet_router()->AddRtpModule(rtp_rtcp);
  }

  for (size_t i = 0; i < config_->rtp.extensions.size(); ++i) {
    const std::string& extension = config_->rtp.extensions[i].uri;
    int id = config_->rtp.extensions[i].id;
    // One-byte-extension local identifiers are in the range 1-14 inclusive.
    RTC_DCHECK_GE(id, 1);
    RTC_DCHECK_LE(id, 14);
    RTC_DCHECK(RtpExtension::IsSupportedForVideo(extension));
    for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
      RTC_CHECK_EQ(0, rtp_rtcp->RegisterSendRtpHeaderExtension(
                          StringToRtpExtensionType(extension), id));
    }
  }

  remb_->AddRembSender(rtp_rtcp_modules_[0]);
  rtp_rtcp_modules_[0]->SetREMBStatus(true);

  ConfigureProtection();
  ConfigureSsrcs();

  // TODO(pbos): Should we set CNAME on all RTP modules?
  rtp_rtcp_modules_.front()->SetCNAME(config_->rtp.c_name.c_str());
  // 28 to match packet overhead in ModuleRtpRtcpImpl.
  static const size_t kRtpPacketSizeOverhead = 28;
  RTC_DCHECK_LE(config_->rtp.max_packet_size, 0xFFFFu + kRtpPacketSizeOverhead);
  const uint16_t mtu = static_cast<uint16_t>(config_->rtp.max_packet_size +
                                             kRtpPacketSizeOverhead);
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->RegisterRtcpStatisticsCallback(stats_proxy_);
    rtp_rtcp->RegisterSendChannelRtpStatisticsCallback(stats_proxy_);
    rtp_rtcp->SetMaxTransferUnit(mtu);
    rtp_rtcp->RegisterVideoSendPayload(
        config_->encoder_settings.payload_type,
        config_->encoder_settings.payload_name.c_str());
  }

  RTC_DCHECK(config_->encoder_settings.encoder);
  RTC_DCHECK_GE(config_->encoder_settings.payload_type, 0);
  RTC_DCHECK_LE(config_->encoder_settings.payload_type, 127);

  vie_encoder_->SetStartBitrate(bitrate_allocator_->GetStartBitrate(this));
  vie_encoder_->SetSink(this);
}

void VideoSendStreamImpl::RegisterProcessThread(
    ProcessThread* module_process_thread) {
  RTC_DCHECK_RUN_ON(&module_process_thread_checker_);
  RTC_DCHECK(!module_process_thread_);
  module_process_thread_ = module_process_thread;

  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    module_process_thread_->RegisterModule(rtp_rtcp);
}

void VideoSendStreamImpl::DeRegisterProcessThread() {
  RTC_DCHECK_RUN_ON(&module_process_thread_checker_);
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    module_process_thread_->DeRegisterModule(rtp_rtcp);
}

VideoSendStreamImpl::~VideoSendStreamImpl() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_DCHECK(!payload_router_.active())
      << "VideoSendStreamImpl::Stop not called";
  LOG(LS_INFO) << "~VideoSendStreamInternal: " << config_->ToString();

  rtp_rtcp_modules_[0]->SetREMBStatus(false);
  remb_->RemoveRembSender(rtp_rtcp_modules_[0]);

  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    congestion_controller_->packet_router()->RemoveRtpModule(rtp_rtcp);
    delete rtp_rtcp;
  }
}

bool VideoSendStreamImpl::DeliverRtcp(const uint8_t* packet, size_t length) {
  // Runs on a network thread.
  RTC_DCHECK(!worker_queue_->IsCurrent());
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_)
    rtp_rtcp->IncomingRtcpPacket(packet, length);
  return true;
}

void VideoSendStreamImpl::Start() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  LOG(LS_INFO) << "VideoSendStream::Start";
  if (payload_router_.active())
    return;
  TRACE_EVENT_INSTANT0("webrtc", "VideoSendStream::Start");
  payload_router_.set_active(true);

  bitrate_allocator_->AddObserver(
      this, encoder_min_bitrate_bps_, encoder_max_bitrate_bps_,
      max_padding_bitrate_, !config_->suspend_below_min_bitrate);

  // Start monitoring encoder activity.
  {
    rtc::CritScope lock(&encoder_activity_crit_sect_);
    RTC_DCHECK(!check_encoder_activity_task_);
    check_encoder_activity_task_ = new CheckEncoderActivityTask(this);
    worker_queue_->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(check_encoder_activity_task_),
        CheckEncoderActivityTask::kEncoderTimeOutMs);
  }

  vie_encoder_->SendKeyFrame();
}

void VideoSendStreamImpl::Stop() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  LOG(LS_INFO) << "VideoSendStream::Stop";
  if (!payload_router_.active())
    return;
  TRACE_EVENT_INSTANT0("webrtc", "VideoSendStream::Stop");
  payload_router_.set_active(false);
  bitrate_allocator_->RemoveObserver(this);
  {
    rtc::CritScope lock(&encoder_activity_crit_sect_);
    check_encoder_activity_task_->Stop();
    check_encoder_activity_task_ = nullptr;
  }
  vie_encoder_->OnBitrateUpdated(0, 0, 0);
  stats_proxy_->OnSetEncoderTargetRate(0);
}

void VideoSendStreamImpl::SignalEncoderTimedOut() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  // If the encoder has not produced anything the last kEncoderTimeOutMs and it
  // is supposed to, deregister as BitrateAllocatorObserver. This can happen
  // if a camera stops producing frames.
  if (encoder_target_rate_bps_ > 0) {
    LOG(LS_INFO) << "SignalEncoderTimedOut, Encoder timed out.";
    bitrate_allocator_->RemoveObserver(this);
  }
}

void VideoSendStreamImpl::SignalEncoderActive() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  LOG(LS_INFO) << "SignalEncoderActive, Encoder is active.";
  bitrate_allocator_->AddObserver(
      this, encoder_min_bitrate_bps_, encoder_max_bitrate_bps_,
      max_padding_bitrate_, !config_->suspend_below_min_bitrate);
}

void VideoSendStreamImpl::SignalEncoderConfigurationChanged(
    const VideoEncoderConfig& config) {
  RTC_DCHECK_GE(config_->rtp.ssrcs.size(), config.streams.size());
  TRACE_EVENT0("webrtc", "VideoSendStream::SignalEncoderConfigurationChanged");
  LOG(LS_INFO) << "SignalEncoderConfigurationChanged: " << config.ToString();
  RTC_DCHECK_GE(config_->rtp.ssrcs.size(), config.streams.size());
  RTC_DCHECK_RUN_ON(worker_queue_);

  const int kEncoderMinBitrateBps = 30000;
  encoder_min_bitrate_bps_ =
      std::max(config.streams[0].min_bitrate_bps, kEncoderMinBitrateBps);
  encoder_max_bitrate_bps_ = 0;
  for (const auto& stream : config.streams)
    encoder_max_bitrate_bps_ += stream.max_bitrate_bps;
  max_padding_bitrate_ =
      CalculateMaxPadBitrateBps(config, config_->suspend_below_min_bitrate);

  payload_router_.SetSendStreams(config.streams);

  // Clear stats for disabled layers.
  for (size_t i = config.streams.size(); i < config_->rtp.ssrcs.size(); ++i) {
    stats_proxy_->OnInactiveSsrc(config_->rtp.ssrcs[i]);
  }

  size_t number_of_temporal_layers =
      config.streams.back().temporal_layer_thresholds_bps.size() + 1;
  protection_bitrate_calculator_.SetEncodingData(
      config.streams[0].width, config.streams[0].height,
      number_of_temporal_layers, config_->rtp.max_packet_size);

  if (payload_router_.active()) {
    // The send stream is started already. Update the allocator with new bitrate
    // limits.
    bitrate_allocator_->AddObserver(
        this, encoder_min_bitrate_bps_, encoder_max_bitrate_bps_,
        max_padding_bitrate_, !config_->suspend_below_min_bitrate);
  }
}

EncodedImageCallback::Result VideoSendStreamImpl::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  // Encoded is called on whatever thread the real encoder implementation run
  // on. In the case of hardware encoders, there might be several encoders
  // running in parallel on different threads.
  if (config_->post_encode_callback) {
    config_->post_encode_callback->EncodedFrameCallback(
        EncodedFrame(encoded_image._buffer, encoded_image._length,
                     encoded_image._frameType));
  }
  {
    rtc::CritScope lock(&encoder_activity_crit_sect_);
    if (check_encoder_activity_task_)
      check_encoder_activity_task_->UpdateEncoderActivity();
  }

  protection_bitrate_calculator_.UpdateWithEncodedData(encoded_image);
  EncodedImageCallback::Result result = payload_router_.OnEncodedImage(
      encoded_image, codec_specific_info, fragmentation);

  if (kEnableFrameRecording) {
    int layer = codec_specific_info->codecType == kVideoCodecVP8
                    ? codec_specific_info->codecSpecific.VP8.simulcastIdx
                    : 0;
    IvfFileWriter* file_writer;
    {
      if (file_writers_[layer] == nullptr) {
        std::ostringstream oss;
        oss << "send_bitstream_ssrc";
        for (uint32_t ssrc : config_->rtp.ssrcs)
          oss << "_" << ssrc;
        oss << "_layer" << layer << ".ivf";
        file_writers_[layer] =
            IvfFileWriter::Open(oss.str(), codec_specific_info->codecType);
      }
      file_writer = file_writers_[layer].get();
    }
    if (file_writer) {
      bool ok = file_writer->WriteFrame(encoded_image);
      RTC_DCHECK(ok);
    }
  }

  return result;
}

void VideoSendStreamImpl::ConfigureProtection() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  // Enable NACK, FEC or both.
  const bool enable_protection_nack = config_->rtp.nack.rtp_history_ms > 0;
  bool enable_protection_fec = config_->rtp.fec.ulpfec_payload_type != -1;
  // Payload types without picture ID cannot determine that a stream is complete
  // without retransmitting FEC, so using FEC + NACK for H.264 (for instance) is
  // a waste of bandwidth since FEC packets still have to be transmitted. Note
  // that this is not the case with FLEXFEC.
  if (enable_protection_nack &&
      !PayloadTypeSupportsSkippingFecPackets(
          config_->encoder_settings.payload_name)) {
    LOG(LS_WARNING) << "Transmitting payload type without picture ID using"
                       "NACK+FEC is a waste of bandwidth since FEC packets "
                       "also have to be retransmitted. Disabling FEC.";
    enable_protection_fec = false;
  }

  // Set to valid uint8_ts to be castable later without signed overflows.
  uint8_t payload_type_red = 0;
  uint8_t payload_type_fec = 0;

  // TODO(changbin): Should set RTX for RED mapping in RTP sender in future.
  // Validate payload types. If either RED or FEC payload types are set then
  // both should be. If FEC is enabled then they both have to be set.
  if (config_->rtp.fec.red_payload_type != -1) {
    RTC_DCHECK_GE(config_->rtp.fec.red_payload_type, 0);
    RTC_DCHECK_LE(config_->rtp.fec.red_payload_type, 127);
    // TODO(holmer): We should only enable red if ulpfec is also enabled, but
    // but due to an incompatibility issue with previous versions the receiver
    // assumes rtx packets are containing red if it has been configured to
    // receive red. Remove this in a few versions once the incompatibility
    // issue is resolved (M53 timeframe).
    payload_type_red = static_cast<uint8_t>(config_->rtp.fec.red_payload_type);
  }
  if (config_->rtp.fec.ulpfec_payload_type != -1) {
    RTC_DCHECK_GE(config_->rtp.fec.ulpfec_payload_type, 0);
    RTC_DCHECK_LE(config_->rtp.fec.ulpfec_payload_type, 127);
    payload_type_fec =
        static_cast<uint8_t>(config_->rtp.fec.ulpfec_payload_type);
  }

  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    // Set NACK.
    rtp_rtcp->SetStorePacketsStatus(
        enable_protection_nack || congestion_controller_->pacer(),
        kMinSendSidePacketHistorySize);
    // Set FEC.
    for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
      rtp_rtcp->SetGenericFECStatus(enable_protection_fec, payload_type_red,
                                    payload_type_fec);
    }
  }

  protection_bitrate_calculator_.SetProtectionMethod(enable_protection_fec,
                                                     enable_protection_nack);
}

void VideoSendStreamImpl::ConfigureSsrcs() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  // Configure regular SSRCs.
  for (size_t i = 0; i < config_->rtp.ssrcs.size(); ++i) {
    uint32_t ssrc = config_->rtp.ssrcs[i];
    RtpRtcp* const rtp_rtcp = rtp_rtcp_modules_[i];
    rtp_rtcp->SetSSRC(ssrc);

    // Restore RTP state if previous existed.
    VideoSendStream::RtpStateMap::iterator it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      rtp_rtcp->SetRtpState(it->second);
  }

  // Set up RTX if available.
  if (config_->rtp.rtx.ssrcs.empty())
    return;

  // Configure RTX SSRCs.
  RTC_DCHECK_EQ(config_->rtp.rtx.ssrcs.size(), config_->rtp.ssrcs.size());
  for (size_t i = 0; i < config_->rtp.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = config_->rtp.rtx.ssrcs[i];
    RtpRtcp* const rtp_rtcp = rtp_rtcp_modules_[i];
    rtp_rtcp->SetRtxSsrc(ssrc);
    VideoSendStream::RtpStateMap::iterator it = suspended_ssrcs_.find(ssrc);
    if (it != suspended_ssrcs_.end())
      rtp_rtcp->SetRtxState(it->second);
  }

  // Configure RTX payload types.
  RTC_DCHECK_GE(config_->rtp.rtx.payload_type, 0);
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->SetRtxSendPayloadType(config_->rtp.rtx.payload_type,
                                    config_->encoder_settings.payload_type);
    rtp_rtcp->SetRtxSendStatus(kRtxRetransmitted | kRtxRedundantPayloads);
  }
  if (config_->rtp.fec.red_payload_type != -1 &&
      config_->rtp.fec.red_rtx_payload_type != -1) {
    for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
      rtp_rtcp->SetRtxSendPayloadType(config_->rtp.fec.red_rtx_payload_type,
                                      config_->rtp.fec.red_payload_type);
    }
  }
}

std::map<uint32_t, RtpState> VideoSendStreamImpl::GetRtpStates() const {
  RTC_DCHECK_RUN_ON(worker_queue_);
  std::map<uint32_t, RtpState> rtp_states;
  for (size_t i = 0; i < config_->rtp.ssrcs.size(); ++i) {
    uint32_t ssrc = config_->rtp.ssrcs[i];
    RTC_DCHECK_EQ(ssrc, rtp_rtcp_modules_[i]->SSRC());
    rtp_states[ssrc] = rtp_rtcp_modules_[i]->GetRtpState();
  }

  for (size_t i = 0; i < config_->rtp.rtx.ssrcs.size(); ++i) {
    uint32_t ssrc = config_->rtp.rtx.ssrcs[i];
    rtp_states[ssrc] = rtp_rtcp_modules_[i]->GetRtxState();
  }

  return rtp_states;
}

void VideoSendStreamImpl::SignalNetworkState(NetworkState state) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    rtp_rtcp->SetRTCPStatus(state == kNetworkUp ? config_->rtp.rtcp_mode
                                                : RtcpMode::kOff);
  }
}

uint32_t VideoSendStreamImpl::OnBitrateUpdated(uint32_t bitrate_bps,
                                               uint8_t fraction_loss,
                                               int64_t rtt) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_DCHECK(payload_router_.active())
      << "VideoSendStream::Start has not been called.";
  // Get the encoder target rate. It is the estimated network rate -
  // protection overhead.
  encoder_target_rate_bps_ = protection_bitrate_calculator_.SetTargetRates(
      bitrate_bps, stats_proxy_->GetSendFrameRate(), fraction_loss, rtt);
  uint32_t protection_bitrate = bitrate_bps - encoder_target_rate_bps_;

  encoder_target_rate_bps_ =
      std::min(encoder_max_bitrate_bps_, encoder_target_rate_bps_);
  vie_encoder_->OnBitrateUpdated(encoder_target_rate_bps_, fraction_loss, rtt);
  stats_proxy_->OnSetEncoderTargetRate(encoder_target_rate_bps_);
  return protection_bitrate;
}

int VideoSendStreamImpl::ProtectionRequest(
    const FecProtectionParams* delta_params,
    const FecProtectionParams* key_params,
    uint32_t* sent_video_rate_bps,
    uint32_t* sent_nack_rate_bps,
    uint32_t* sent_fec_rate_bps) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  *sent_video_rate_bps = 0;
  *sent_nack_rate_bps = 0;
  *sent_fec_rate_bps = 0;
  for (RtpRtcp* rtp_rtcp : rtp_rtcp_modules_) {
    uint32_t not_used = 0;
    uint32_t module_video_rate = 0;
    uint32_t module_fec_rate = 0;
    uint32_t module_nack_rate = 0;
    rtp_rtcp->SetFecParameters(delta_params, key_params);
    rtp_rtcp->BitrateSent(&not_used, &module_video_rate, &module_fec_rate,
                          &module_nack_rate);
    *sent_video_rate_bps += module_video_rate;
    *sent_nack_rate_bps += module_nack_rate;
    *sent_fec_rate_bps += module_fec_rate;
  }
  return 0;
}

}  // namespace internal
}  // namespace webrtc
