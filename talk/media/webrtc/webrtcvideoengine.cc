/*
 * libjingle
 * Copyright 2004 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifdef HAVE_WEBRTC_VIDEO
#include "talk/media/webrtc/webrtcvideoengine.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <math.h>
#include <set>

#include "talk/base/basictypes.h"
#include "talk/base/buffer.h"
#include "talk/base/byteorder.h"
#include "talk/base/common.h"
#include "talk/base/cpumonitor.h"
#include "talk/base/logging.h"
#include "talk/base/stringutils.h"
#include "talk/base/thread.h"
#include "talk/base/timeutils.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/rtputils.h"
#include "talk/media/base/streamparams.h"
#include "talk/media/base/videoadapter.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videorenderer.h"
#include "talk/media/devices/filevideocapturer.h"
#include "talk/media/webrtc/webrtcpassthroughrender.h"
#include "talk/media/webrtc/webrtctexturevideoframe.h"
#include "talk/media/webrtc/webrtcvideocapturer.h"
#include "talk/media/webrtc/webrtcvideodecoderfactory.h"
#include "talk/media/webrtc/webrtcvideoencoderfactory.h"
#include "talk/media/webrtc/webrtcvideoframe.h"
#include "talk/media/webrtc/webrtcvie.h"
#include "talk/media/webrtc/webrtcvoe.h"
#include "talk/media/webrtc/webrtcvoiceengine.h"

#if !defined(LIBPEERCONNECTION_LIB)
#ifndef HAVE_WEBRTC_VIDEO
#error Need webrtc video
#endif
#include "talk/media/webrtc/webrtcmediaengine.h"

WRME_EXPORT
cricket::MediaEngineInterface* CreateWebRtcMediaEngine(
    webrtc::AudioDeviceModule* adm, webrtc::AudioDeviceModule* adm_sc,
    cricket::WebRtcVideoEncoderFactory* encoder_factory,
    cricket::WebRtcVideoDecoderFactory* decoder_factory) {
  return new cricket::WebRtcMediaEngine(adm, adm_sc, encoder_factory,
                                        decoder_factory);
}

WRME_EXPORT
void DestroyWebRtcMediaEngine(cricket::MediaEngineInterface* media_engine) {
  delete static_cast<cricket::WebRtcMediaEngine*>(media_engine);
}
#endif


namespace cricket {


static const int kDefaultLogSeverity = talk_base::LS_WARNING;

static const int kMinVideoBitrate = 50;
static const int kStartVideoBitrate = 300;
static const int kMaxVideoBitrate = 2000;
static const int kDefaultConferenceModeMaxVideoBitrate = 500;

// Controlled by exp, try a super low minimum bitrate for poor connections.
static const int kLowerMinBitrate = 30;

static const int kVideoMtu = 1200;

static const int kVideoRtpBufferSize = 65536;

static const char kVp8PayloadName[] = "VP8";
static const char kRedPayloadName[] = "red";
static const char kFecPayloadName[] = "ulpfec";

static const int kDefaultNumberOfTemporalLayers = 1;  // 1:1

static const int kTimestampDeltaInSecondsForWarning = 2;

static const int kMaxExternalVideoCodecs = 8;
static const int kExternalVideoPayloadTypeBase = 120;

// Static allocation of payload type values for external video codec.
static int GetExternalVideoPayloadType(int index) {
  ASSERT(index >= 0 && index < kMaxExternalVideoCodecs);
  return kExternalVideoPayloadTypeBase + index;
}

static void LogMultiline(talk_base::LoggingSeverity sev, char* text) {
  const char* delim = "\r\n";
  // TODO(fbarchard): Fix strtok lint warning.
  for (char* tok = strtok(text, delim); tok; tok = strtok(NULL, delim)) {
    LOG_V(sev) << tok;
  }
}

// Severity is an integer because it comes is assumed to be from command line.
static int SeverityToFilter(int severity) {
  int filter = webrtc::kTraceNone;
  switch (severity) {
    case talk_base::LS_VERBOSE:
      filter |= webrtc::kTraceAll;
    case talk_base::LS_INFO:
      filter |= (webrtc::kTraceStateInfo | webrtc::kTraceInfo);
    case talk_base::LS_WARNING:
      filter |= (webrtc::kTraceTerseInfo | webrtc::kTraceWarning);
    case talk_base::LS_ERROR:
      filter |= (webrtc::kTraceError | webrtc::kTraceCritical);
  }
  return filter;
}

static const int kCpuMonitorPeriodMs = 2000;  // 2 seconds.

static const bool kNotSending = false;

// Extension header for RTP timestamp offset, see RFC 5450 for details:
// http://tools.ietf.org/html/rfc5450
static const char kRtpTimestampOffsetHeaderExtension[] =
    "urn:ietf:params:rtp-hdrext:toffset";
static const int kRtpTimeOffsetExtensionId = 2;

// Extension header for absolute send time, see url for details:
// http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
static const char kRtpAbsoluteSendTimeHeaderExtension[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";
static const int kRtpAbsoluteSendTimeExtensionId = 3;
// Default video dscp value.
// See http://tools.ietf.org/html/rfc2474 for details
// See also http://tools.ietf.org/html/draft-jennings-rtcweb-qos-00
static const talk_base::DiffServCodePoint kVideoDscpValue =
    talk_base::DSCP_AF41;

static bool IsNackEnabled(const VideoCodec& codec) {
  return codec.HasFeedbackParam(FeedbackParam(kRtcpFbParamNack,
                                              kParamValueEmpty));
}

// Returns true if Receiver Estimated Max Bitrate is enabled.
static bool IsRembEnabled(const VideoCodec& codec) {
  return codec.HasFeedbackParam(FeedbackParam(kRtcpFbParamRemb,
                                              kParamValueEmpty));
}

struct FlushBlackFrameData : public talk_base::MessageData {
  FlushBlackFrameData(uint32 s, int64 t) : ssrc(s), timestamp(t) {
  }
  uint32 ssrc;
  int64 timestamp;
};

class WebRtcRenderAdapter : public webrtc::ExternalRenderer {
 public:
  explicit WebRtcRenderAdapter(VideoRenderer* renderer)
      : renderer_(renderer), width_(0), height_(0), watermark_enabled_(false) {
  }

  virtual ~WebRtcRenderAdapter() {
  }

  void set_watermark_enabled(bool enable) {
    talk_base::CritScope cs(&crit_);
    watermark_enabled_ = enable;
  }

  void SetRenderer(VideoRenderer* renderer) {
    talk_base::CritScope cs(&crit_);
    renderer_ = renderer;
    // FrameSizeChange may have already been called when renderer was not set.
    // If so we should call SetSize here.
    // TODO(ronghuawu): Add unit test for this case. Didn't do it now
    // because the WebRtcRenderAdapter is currently hiding in cc file. No
    // good way to get access to it from the unit test.
    if (width_ > 0 && height_ > 0 && renderer_ != NULL) {
      if (!renderer_->SetSize(width_, height_, 0)) {
        LOG(LS_ERROR)
            << "WebRtcRenderAdapter SetRenderer failed to SetSize to: "
            << width_ << "x" << height_;
      }
    }
  }

  // Implementation of webrtc::ExternalRenderer.
  virtual int FrameSizeChange(unsigned int width, unsigned int height,
                              unsigned int /*number_of_streams*/) {
    talk_base::CritScope cs(&crit_);
    width_ = width;
    height_ = height;
    LOG(LS_INFO) << "WebRtcRenderAdapter frame size changed to: "
                 << width << "x" << height;
    if (renderer_ == NULL) {
      LOG(LS_VERBOSE) << "WebRtcRenderAdapter the renderer has not been set. "
                      << "SetSize will be called later in SetRenderer.";
      return 0;
    }
    return renderer_->SetSize(width_, height_, 0) ? 0 : -1;
  }

  virtual int DeliverFrame(unsigned char* buffer, int buffer_size,
                           uint32_t time_stamp, int64_t render_time,
                           void* handle) {
    talk_base::CritScope cs(&crit_);
    frame_rate_tracker_.Update(1);
    if (renderer_ == NULL) {
      return 0;
    }
    // Convert 90K rtp timestamp to ns timestamp.
    int64 rtp_time_stamp_in_ns = (time_stamp / 90) *
        talk_base::kNumNanosecsPerMillisec;
    // Convert milisecond render time to ns timestamp.
    int64 render_time_stamp_in_ns = render_time *
        talk_base::kNumNanosecsPerMillisec;
    // Send the rtp timestamp to renderer as the VideoFrame timestamp.
    // and the render timestamp as the VideoFrame elapsed_time.
    if (handle == NULL) {
      return DeliverBufferFrame(buffer, buffer_size, render_time_stamp_in_ns,
                                rtp_time_stamp_in_ns);
    } else {
      return DeliverTextureFrame(handle, render_time_stamp_in_ns,
                                 rtp_time_stamp_in_ns);
    }
  }

  virtual bool IsTextureSupported() { return true; }

  int DeliverBufferFrame(unsigned char* buffer, int buffer_size,
                         int64 elapsed_time, int64 time_stamp) {
    WebRtcVideoFrame video_frame;
    video_frame.Alias(buffer, buffer_size, width_, height_,
                      1, 1, elapsed_time, time_stamp, 0);


    // Sanity check on decoded frame size.
    if (buffer_size != static_cast<int>(VideoFrame::SizeOf(width_, height_))) {
      LOG(LS_WARNING) << "WebRtcRenderAdapter received a strange frame size: "
                      << buffer_size;
    }

    int ret = renderer_->RenderFrame(&video_frame) ? 0 : -1;
    return ret;
  }

  int DeliverTextureFrame(void* handle, int64 elapsed_time, int64 time_stamp) {
    WebRtcTextureVideoFrame video_frame(
        static_cast<webrtc::NativeHandle*>(handle), width_, height_,
        elapsed_time, time_stamp);
    return renderer_->RenderFrame(&video_frame);
  }

  unsigned int width() {
    talk_base::CritScope cs(&crit_);
    return width_;
  }

  unsigned int height() {
    talk_base::CritScope cs(&crit_);
    return height_;
  }

  int framerate() {
    talk_base::CritScope cs(&crit_);
    return static_cast<int>(frame_rate_tracker_.units_second());
  }

  VideoRenderer* renderer() {
    talk_base::CritScope cs(&crit_);
    return renderer_;
  }

 private:
  talk_base::CriticalSection crit_;
  VideoRenderer* renderer_;
  unsigned int width_;
  unsigned int height_;
  talk_base::RateTracker frame_rate_tracker_;
  bool watermark_enabled_;
};

class WebRtcDecoderObserver : public webrtc::ViEDecoderObserver {
 public:
  explicit WebRtcDecoderObserver(int video_channel)
       : video_channel_(video_channel),
         framerate_(0),
         bitrate_(0),
         decode_ms_(0),
         max_decode_ms_(0),
         current_delay_ms_(0),
         target_delay_ms_(0),
         jitter_buffer_ms_(0),
         min_playout_delay_ms_(0),
         render_delay_ms_(0),
         firs_requested_(0) {
  }

  // virtual functions from VieDecoderObserver.
  virtual void IncomingCodecChanged(const int videoChannel,
                                    const webrtc::VideoCodec& videoCodec) {}
  virtual void IncomingRate(const int videoChannel,
                            const unsigned int framerate,
                            const unsigned int bitrate) {
    talk_base::CritScope cs(&crit_);
    ASSERT(video_channel_ == videoChannel);
    framerate_ = framerate;
    bitrate_ = bitrate;
  }

  virtual void DecoderTiming(int decode_ms,
                             int max_decode_ms,
                             int current_delay_ms,
                             int target_delay_ms,
                             int jitter_buffer_ms,
                             int min_playout_delay_ms,
                             int render_delay_ms) {
    talk_base::CritScope cs(&crit_);
    decode_ms_ = decode_ms;
    max_decode_ms_ = max_decode_ms;
    current_delay_ms_ = current_delay_ms;
    target_delay_ms_ = target_delay_ms;
    jitter_buffer_ms_ = jitter_buffer_ms;
    min_playout_delay_ms_ = min_playout_delay_ms;
    render_delay_ms_ = render_delay_ms;
  }

  virtual void RequestNewKeyFrame(const int videoChannel) {
    talk_base::CritScope cs(&crit_);
    ASSERT(video_channel_ == videoChannel);
    ++firs_requested_;
  }

  // Populate |rinfo| based on previously-set data in |*this|.
  void ExportTo(VideoReceiverInfo* rinfo) {
    talk_base::CritScope cs(&crit_);
    rinfo->firs_sent = firs_requested_;
    rinfo->framerate_rcvd = framerate_;
    rinfo->decode_ms = decode_ms_;
    rinfo->max_decode_ms = max_decode_ms_;
    rinfo->current_delay_ms = current_delay_ms_;
    rinfo->target_delay_ms = target_delay_ms_;
    rinfo->jitter_buffer_ms = jitter_buffer_ms_;
    rinfo->min_playout_delay_ms = min_playout_delay_ms_;
    rinfo->render_delay_ms = render_delay_ms_;
  }

 private:
  mutable talk_base::CriticalSection crit_;
  int video_channel_;
  int framerate_;
  int bitrate_;
  int decode_ms_;
  int max_decode_ms_;
  int current_delay_ms_;
  int target_delay_ms_;
  int jitter_buffer_ms_;
  int min_playout_delay_ms_;
  int render_delay_ms_;
  int firs_requested_;
};

class WebRtcEncoderObserver : public webrtc::ViEEncoderObserver {
 public:
  explicit WebRtcEncoderObserver(int video_channel)
      : video_channel_(video_channel),
        framerate_(0),
        bitrate_(0),
        suspended_(false) {
  }

  // virtual functions from VieEncoderObserver.
  virtual void OutgoingRate(const int videoChannel,
                            const unsigned int framerate,
                            const unsigned int bitrate) {
    talk_base::CritScope cs(&crit_);
    ASSERT(video_channel_ == videoChannel);
    framerate_ = framerate;
    bitrate_ = bitrate;
  }

  virtual void SuspendChange(int video_channel, bool is_suspended) {
    talk_base::CritScope cs(&crit_);
    ASSERT(video_channel_ == video_channel);
    suspended_ = is_suspended;
  }

  int framerate() const {
    talk_base::CritScope cs(&crit_);
    return framerate_;
  }
  int bitrate() const {
    talk_base::CritScope cs(&crit_);
    return bitrate_;
  }
  bool suspended() const {
    talk_base::CritScope cs(&crit_);
    return suspended_;
  }

 private:
  mutable talk_base::CriticalSection crit_;
  int video_channel_;
  int framerate_;
  int bitrate_;
  bool suspended_;
};

class WebRtcLocalStreamInfo {
 public:
  WebRtcLocalStreamInfo()
      : width_(0), height_(0), elapsed_time_(-1), time_stamp_(-1) {}
  size_t width() const {
    talk_base::CritScope cs(&crit_);
    return width_;
  }
  size_t height() const {
    talk_base::CritScope cs(&crit_);
    return height_;
  }
  int64 elapsed_time() const {
    talk_base::CritScope cs(&crit_);
    return elapsed_time_;
  }
  int64 time_stamp() const {
    talk_base::CritScope cs(&crit_);
    return time_stamp_;
  }
  int framerate() {
    talk_base::CritScope cs(&crit_);
    return static_cast<int>(rate_tracker_.units_second());
  }
  void GetLastFrameInfo(
      size_t* width, size_t* height, int64* elapsed_time) const {
    talk_base::CritScope cs(&crit_);
    *width = width_;
    *height = height_;
    *elapsed_time = elapsed_time_;
  }

  void UpdateFrame(const VideoFrame* frame) {
    talk_base::CritScope cs(&crit_);

    width_ = frame->GetWidth();
    height_ = frame->GetHeight();
    elapsed_time_ = frame->GetElapsedTime();
    time_stamp_ = frame->GetTimeStamp();

    rate_tracker_.Update(1);
  }

 private:
  mutable talk_base::CriticalSection crit_;
  size_t width_;
  size_t height_;
  int64 elapsed_time_;
  int64 time_stamp_;
  talk_base::RateTracker rate_tracker_;

  DISALLOW_COPY_AND_ASSIGN(WebRtcLocalStreamInfo);
};

// WebRtcVideoChannelRecvInfo is a container class with members such as renderer
// and a decoder observer that is used by receive channels.
// It must exist as long as the receive channel is connected to renderer or a
// decoder observer in this class and methods in the class should only be called
// from the worker thread.
class WebRtcVideoChannelRecvInfo  {
 public:
  typedef std::map<int, webrtc::VideoDecoder*> DecoderMap;  // key: payload type
  explicit WebRtcVideoChannelRecvInfo(int channel_id)
      : channel_id_(channel_id),
        render_adapter_(NULL),
        decoder_observer_(channel_id) {
  }
  int channel_id() { return channel_id_; }
  void SetRenderer(VideoRenderer* renderer) {
    render_adapter_.SetRenderer(renderer);
  }
  WebRtcRenderAdapter* render_adapter() { return &render_adapter_; }
  WebRtcDecoderObserver* decoder_observer() { return &decoder_observer_; }
  void RegisterDecoder(int pl_type, webrtc::VideoDecoder* decoder) {
    ASSERT(!IsDecoderRegistered(pl_type));
    registered_decoders_[pl_type] = decoder;
  }
  bool IsDecoderRegistered(int pl_type) {
    return registered_decoders_.count(pl_type) != 0;
  }
  const DecoderMap& registered_decoders() {
    return registered_decoders_;
  }
  void ClearRegisteredDecoders() {
    registered_decoders_.clear();
  }

 private:
  int channel_id_;  // Webrtc video channel number.
  // Renderer for this channel.
  WebRtcRenderAdapter render_adapter_;
  WebRtcDecoderObserver decoder_observer_;
  DecoderMap registered_decoders_;
};

class WebRtcOveruseObserver : public webrtc::CpuOveruseObserver {
 public:
  explicit WebRtcOveruseObserver(CoordinatedVideoAdapter* video_adapter)
      : video_adapter_(video_adapter),
        enabled_(false) {
  }

  // TODO(mflodman): Consider sending resolution as part of event, to let
  // adapter know what resolution the request is based on. Helps eliminate stale
  // data, race conditions.
  virtual void OveruseDetected() OVERRIDE {
    talk_base::CritScope cs(&crit_);
    if (!enabled_) {
      return;
    }

    video_adapter_->OnCpuResolutionRequest(CoordinatedVideoAdapter::DOWNGRADE);
  }

  virtual void NormalUsage() OVERRIDE {
    talk_base::CritScope cs(&crit_);
    if (!enabled_) {
      return;
    }

    video_adapter_->OnCpuResolutionRequest(CoordinatedVideoAdapter::UPGRADE);
  }

  void Enable(bool enable) {
    talk_base::CritScope cs(&crit_);
    enabled_ = enable;
  }

 private:
  CoordinatedVideoAdapter* video_adapter_;
  bool enabled_;
  talk_base::CriticalSection crit_;
};


class WebRtcVideoChannelSendInfo : public sigslot::has_slots<> {
 public:
  typedef std::map<int, webrtc::VideoEncoder*> EncoderMap;  // key: payload type
  WebRtcVideoChannelSendInfo(int channel_id, int capture_id,
                             webrtc::ViEExternalCapture* external_capture,
                             talk_base::CpuMonitor* cpu_monitor)
      : channel_id_(channel_id),
        capture_id_(capture_id),
        sending_(false),
        muted_(false),
        video_capturer_(NULL),
        encoder_observer_(channel_id),
        external_capture_(external_capture),
        capturer_updated_(false),
        interval_(0),
        cpu_monitor_(cpu_monitor) {
    overuse_observer_.reset(new WebRtcOveruseObserver(&video_adapter_));
    SignalCpuAdaptationUnable.repeat(video_adapter_.SignalCpuAdaptationUnable);
    if (cpu_monitor) {
      cpu_monitor->SignalUpdate.connect(
          &video_adapter_, &CoordinatedVideoAdapter::OnCpuLoadUpdated);
    }
  }

  int channel_id() const { return channel_id_; }
  int capture_id() const { return capture_id_; }
  void set_sending(bool sending) { sending_ = sending; }
  bool sending() const { return sending_; }
  void set_muted(bool on) {
    // TODO(asapersson): add support.
    // video_adapter_.SetBlackOutput(on);
    muted_ = on;
  }
  bool muted() {return muted_; }

  WebRtcEncoderObserver* encoder_observer() { return &encoder_observer_; }
  webrtc::ViEExternalCapture* external_capture() { return external_capture_; }
  const VideoFormat& video_format() const {
    return video_format_;
  }
  void set_video_format(const VideoFormat& video_format) {
    video_format_ = video_format;
    if (video_format_ != cricket::VideoFormat()) {
      interval_ = video_format_.interval;
    }
    video_adapter_.OnOutputFormatRequest(video_format_);
  }
  void set_interval(int64 interval) {
    if (video_format() == cricket::VideoFormat()) {
      interval_ = interval;
    }
  }
  int64 interval() { return interval_; }

  void InitializeAdapterOutputFormat(const webrtc::VideoCodec& codec) {
    VideoFormat format(codec.width, codec.height,
                       VideoFormat::FpsToInterval(codec.maxFramerate),
                       FOURCC_I420);
    if (video_adapter_.output_format().IsSize0x0()) {
      video_adapter_.SetOutputFormat(format);
    }
  }

  int CurrentAdaptReason() const {
    return video_adapter_.adapt_reason();
  }
  webrtc::CpuOveruseObserver* overuse_observer() {
    return overuse_observer_.get();
  }

  StreamParams* stream_params() { return stream_params_.get(); }
  void set_stream_params(const StreamParams& sp) {
    stream_params_.reset(new StreamParams(sp));
  }
  void ClearStreamParams() { stream_params_.reset(); }
  bool has_ssrc(uint32 local_ssrc) const {
    return !stream_params_ ? false :
        stream_params_->has_ssrc(local_ssrc);
  }
  WebRtcLocalStreamInfo* local_stream_info() {
    return &local_stream_info_;
  }
  VideoCapturer* video_capturer() {
    return video_capturer_;
  }
  void set_video_capturer(VideoCapturer* video_capturer) {
    if (video_capturer == video_capturer_) {
      return;
    }
    capturer_updated_ = true;

    // Disconnect from the previous video capturer.
    if (video_capturer_) {
      video_capturer_->SignalAdaptFrame.disconnect(this);
    }

    video_capturer_ = video_capturer;
    if (video_capturer && !video_capturer->IsScreencast()) {
      const VideoFormat* capture_format = video_capturer->GetCaptureFormat();
      if (capture_format) {
        // TODO(thorcarpenter): This is broken. Video capturer doesn't have
        // a capture format until the capturer is started. So, if
        // the capturer is started immediately after calling set_video_capturer
        // video adapter may not have the input format set, the interval may
        // be zero, and all frames may be dropped.
        // Consider fixing this by having video_adapter keep a pointer to the
        // video capturer.
        video_adapter_.SetInputFormat(*capture_format);
      }
      // TODO(thorcarpenter): When the adapter supports "only frame dropping"
      // mode, also hook it up to screencast capturers.
      video_capturer->SignalAdaptFrame.connect(
          this, &WebRtcVideoChannelSendInfo::AdaptFrame);
    }
  }

  CoordinatedVideoAdapter* video_adapter() { return &video_adapter_; }

  void AdaptFrame(VideoCapturer* capturer, const VideoFrame* input,
                  VideoFrame** adapted) {
    video_adapter_.AdaptFrame(input, adapted);
  }

  void ApplyCpuOptions(const VideoOptions& options) {
    bool cpu_adapt, cpu_smoothing, adapt_third;
    float low, med, high;
    if (options.adapt_input_to_cpu_usage.Get(&cpu_adapt)) {
      video_adapter_.set_cpu_adaptation(cpu_adapt);
    }
    if (options.adapt_cpu_with_smoothing.Get(&cpu_smoothing)) {
      video_adapter_.set_cpu_smoothing(cpu_smoothing);
    }
    if (options.process_adaptation_threshhold.Get(&med)) {
      video_adapter_.set_process_threshold(med);
    }
    if (options.system_low_adaptation_threshhold.Get(&low)) {
      video_adapter_.set_low_system_threshold(low);
    }
    if (options.system_high_adaptation_threshhold.Get(&high)) {
      video_adapter_.set_high_system_threshold(high);
    }
    if (options.video_adapt_third.Get(&adapt_third)) {
      video_adapter_.set_scale_third(adapt_third);
    }
  }

  void SetCpuOveruseDetection(bool enable) {
    if (cpu_monitor_ && enable) {
      cpu_monitor_->SignalUpdate.disconnect(&video_adapter_);
    }
    overuse_observer_->Enable(enable);
    video_adapter_.set_cpu_adaptation(enable);
  }

  void ProcessFrame(const VideoFrame& original_frame, bool mute,
                    VideoFrame** processed_frame) {
    if (!mute) {
      *processed_frame = original_frame.Copy();
    } else {
      WebRtcVideoFrame* black_frame = new WebRtcVideoFrame();
      black_frame->InitToBlack(static_cast<int>(original_frame.GetWidth()),
                               static_cast<int>(original_frame.GetHeight()),
                               1, 1,
                               original_frame.GetElapsedTime(),
                               original_frame.GetTimeStamp());
      *processed_frame = black_frame;
    }
    local_stream_info_.UpdateFrame(*processed_frame);
  }
  void RegisterEncoder(int pl_type, webrtc::VideoEncoder* encoder) {
    ASSERT(!IsEncoderRegistered(pl_type));
    registered_encoders_[pl_type] = encoder;
  }
  bool IsEncoderRegistered(int pl_type) {
    return registered_encoders_.count(pl_type) != 0;
  }
  const EncoderMap& registered_encoders() {
    return registered_encoders_;
  }
  void ClearRegisteredEncoders() {
    registered_encoders_.clear();
  }

  sigslot::repeater0<> SignalCpuAdaptationUnable;

 private:
  int channel_id_;
  int capture_id_;
  bool sending_;
  bool muted_;
  VideoCapturer* video_capturer_;
  WebRtcEncoderObserver encoder_observer_;
  webrtc::ViEExternalCapture* external_capture_;
  EncoderMap registered_encoders_;

  VideoFormat video_format_;

  talk_base::scoped_ptr<StreamParams> stream_params_;

  WebRtcLocalStreamInfo local_stream_info_;

  bool capturer_updated_;

  int64 interval_;

  CoordinatedVideoAdapter video_adapter_;
  talk_base::CpuMonitor* cpu_monitor_;
  talk_base::scoped_ptr<WebRtcOveruseObserver> overuse_observer_;
};

const WebRtcVideoEngine::VideoCodecPref
    WebRtcVideoEngine::kVideoCodecPrefs[] = {
    {kVp8PayloadName, 100, -1, 0},
    {kRedPayloadName, 116, -1, 1},
    {kFecPayloadName, 117, -1, 2},
    {kRtxCodecName, 96, 100, 3},
};

// The formats are sorted by the descending order of width. We use the order to
// find the next format for CPU and bandwidth adaptation.
const VideoFormatPod WebRtcVideoEngine::kVideoFormats[] = {
  {1280, 800, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {1280, 720, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {960, 600, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {960, 540, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {640, 400, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {640, 360, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {640, 480, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {480, 300, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {480, 270, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {480, 360, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {320, 200, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {320, 180, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {320, 240, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {240, 150, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {240, 135, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {240, 180, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {160, 100, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {160, 90, FPS_TO_INTERVAL(30), FOURCC_ANY},
  {160, 120, FPS_TO_INTERVAL(30), FOURCC_ANY},
};

const VideoFormatPod WebRtcVideoEngine::kDefaultVideoFormat =
  {640, 400, FPS_TO_INTERVAL(30), FOURCC_ANY};

static void UpdateVideoCodec(const cricket::VideoFormat& video_format,
                             webrtc::VideoCodec* target_codec) {
  if ((target_codec == NULL) || (video_format == cricket::VideoFormat())) {
    return;
  }
  target_codec->width = video_format.width;
  target_codec->height = video_format.height;
  target_codec->maxFramerate = cricket::VideoFormat::IntervalToFps(
      video_format.interval);
}

WebRtcVideoEngine::WebRtcVideoEngine() {
  Construct(new ViEWrapper(), new ViETraceWrapper(), NULL,
      new talk_base::CpuMonitor(NULL));
}

WebRtcVideoEngine::WebRtcVideoEngine(WebRtcVoiceEngine* voice_engine,
                                     ViEWrapper* vie_wrapper,
                                     talk_base::CpuMonitor* cpu_monitor) {
  Construct(vie_wrapper, new ViETraceWrapper(), voice_engine, cpu_monitor);
}

WebRtcVideoEngine::WebRtcVideoEngine(WebRtcVoiceEngine* voice_engine,
                                     ViEWrapper* vie_wrapper,
                                     ViETraceWrapper* tracing,
                                     talk_base::CpuMonitor* cpu_monitor) {
  Construct(vie_wrapper, tracing, voice_engine, cpu_monitor);
}

void WebRtcVideoEngine::Construct(ViEWrapper* vie_wrapper,
                                  ViETraceWrapper* tracing,
                                  WebRtcVoiceEngine* voice_engine,
                                  talk_base::CpuMonitor* cpu_monitor) {
  LOG(LS_INFO) << "WebRtcVideoEngine::WebRtcVideoEngine";
  worker_thread_ = NULL;
  vie_wrapper_.reset(vie_wrapper);
  vie_wrapper_base_initialized_ = false;
  tracing_.reset(tracing);
  voice_engine_ = voice_engine;
  initialized_ = false;
  SetTraceFilter(SeverityToFilter(kDefaultLogSeverity));
  render_module_.reset(new WebRtcPassthroughRender());
  local_renderer_w_ = local_renderer_h_ = 0;
  local_renderer_ = NULL;
  capture_started_ = false;
  decoder_factory_ = NULL;
  encoder_factory_ = NULL;
  cpu_monitor_.reset(cpu_monitor);

  SetTraceOptions("");
  if (tracing_->SetTraceCallback(this) != 0) {
    LOG_RTCERR1(SetTraceCallback, this);
  }

  // Set default quality levels for our supported codecs. We override them here
  // if we know your cpu performance is low, and they can be updated explicitly
  // by calling SetDefaultCodec.  For example by a flute preference setting, or
  // by the server with a jec in response to our reported system info.
  VideoCodec max_codec(kVideoCodecPrefs[0].payload_type,
                       kVideoCodecPrefs[0].name,
                       kDefaultVideoFormat.width,
                       kDefaultVideoFormat.height,
                       VideoFormat::IntervalToFps(kDefaultVideoFormat.interval),
                       0);
  if (!SetDefaultCodec(max_codec)) {
    LOG(LS_ERROR) << "Failed to initialize list of supported codec types";
  }


  // Load our RTP Header extensions.
  rtp_header_extensions_.push_back(
      RtpHeaderExtension(kRtpTimestampOffsetHeaderExtension,
                         kRtpTimeOffsetExtensionId));
  rtp_header_extensions_.push_back(
      RtpHeaderExtension(kRtpAbsoluteSendTimeHeaderExtension,
                         kRtpAbsoluteSendTimeExtensionId));
}

WebRtcVideoEngine::~WebRtcVideoEngine() {
  LOG(LS_INFO) << "WebRtcVideoEngine::~WebRtcVideoEngine";
  if (initialized_) {
    Terminate();
  }
  if (encoder_factory_) {
    encoder_factory_->RemoveObserver(this);
  }
  tracing_->SetTraceCallback(NULL);
  // Test to see if the media processor was deregistered properly.
  ASSERT(SignalMediaFrame.is_empty());
}

bool WebRtcVideoEngine::Init(talk_base::Thread* worker_thread) {
  LOG(LS_INFO) << "WebRtcVideoEngine::Init";
  worker_thread_ = worker_thread;
  ASSERT(worker_thread_ != NULL);

  cpu_monitor_->set_thread(worker_thread_);
  if (!cpu_monitor_->Start(kCpuMonitorPeriodMs)) {
    LOG(LS_ERROR) << "Failed to start CPU monitor.";
    cpu_monitor_.reset();
  }

  bool result = InitVideoEngine();
  if (result) {
    LOG(LS_INFO) << "VideoEngine Init done";
  } else {
    LOG(LS_ERROR) << "VideoEngine Init failed, releasing";
    Terminate();
  }
  return result;
}

bool WebRtcVideoEngine::InitVideoEngine() {
  LOG(LS_INFO) << "WebRtcVideoEngine::InitVideoEngine";

  // Init WebRTC VideoEngine.
  if (!vie_wrapper_base_initialized_) {
    if (vie_wrapper_->base()->Init() != 0) {
      LOG_RTCERR0(Init);
      return false;
    }
    vie_wrapper_base_initialized_ = true;
  }

  // Log the VoiceEngine version info.
  char buffer[1024] = "";
  if (vie_wrapper_->base()->GetVersion(buffer) != 0) {
    LOG_RTCERR0(GetVersion);
    return false;
  }

  LOG(LS_INFO) << "WebRtc VideoEngine Version:";
  LogMultiline(talk_base::LS_INFO, buffer);

  // Hook up to VoiceEngine for sync purposes, if supplied.
  if (!voice_engine_) {
    LOG(LS_WARNING) << "NULL voice engine";
  } else if ((vie_wrapper_->base()->SetVoiceEngine(
      voice_engine_->voe()->engine())) != 0) {
    LOG_RTCERR0(SetVoiceEngine);
    return false;
  }

  // Register our custom render module.
  if (vie_wrapper_->render()->RegisterVideoRenderModule(
      *render_module_.get()) != 0) {
    LOG_RTCERR0(RegisterVideoRenderModule);
    return false;
  }

  initialized_ = true;
  return true;
}

void WebRtcVideoEngine::Terminate() {
  LOG(LS_INFO) << "WebRtcVideoEngine::Terminate";
  initialized_ = false;

  if (vie_wrapper_->render()->DeRegisterVideoRenderModule(
      *render_module_.get()) != 0) {
    LOG_RTCERR0(DeRegisterVideoRenderModule);
  }

  if (vie_wrapper_->base()->SetVoiceEngine(NULL) != 0) {
    LOG_RTCERR0(SetVoiceEngine);
  }

  cpu_monitor_->Stop();
}

int WebRtcVideoEngine::GetCapabilities() {
  return VIDEO_RECV | VIDEO_SEND;
}

bool WebRtcVideoEngine::SetOptions(const VideoOptions &options) {
  return true;
}

bool WebRtcVideoEngine::SetDefaultEncoderConfig(
    const VideoEncoderConfig& config) {
  return SetDefaultCodec(config.max_codec);
}

VideoEncoderConfig WebRtcVideoEngine::GetDefaultEncoderConfig() const {
  ASSERT(!video_codecs_.empty());
  VideoCodec max_codec(kVideoCodecPrefs[0].payload_type,
                       kVideoCodecPrefs[0].name,
                       video_codecs_[0].width,
                       video_codecs_[0].height,
                       video_codecs_[0].framerate,
                       0);
  return VideoEncoderConfig(max_codec);
}

// SetDefaultCodec may be called while the capturer is running. For example, a
// test call is started in a page with QVGA default codec, and then a real call
// is started in another page with VGA default codec. This is the corner case
// and happens only when a session is started. We ignore this case currently.
bool WebRtcVideoEngine::SetDefaultCodec(const VideoCodec& codec) {
  if (!RebuildCodecList(codec)) {
    LOG(LS_WARNING) << "Failed to RebuildCodecList";
    return false;
  }

  ASSERT(!video_codecs_.empty());
  default_codec_format_ = VideoFormat(
      video_codecs_[0].width,
      video_codecs_[0].height,
      VideoFormat::FpsToInterval(video_codecs_[0].framerate),
      FOURCC_ANY);
  return true;
}

WebRtcVideoMediaChannel* WebRtcVideoEngine::CreateChannel(
    VoiceMediaChannel* voice_channel) {
  WebRtcVideoMediaChannel* channel =
      new WebRtcVideoMediaChannel(this, voice_channel);
  if (!channel->Init()) {
    delete channel;
    channel = NULL;
  }
  return channel;
}

bool WebRtcVideoEngine::SetLocalRenderer(VideoRenderer* renderer) {
  local_renderer_w_ = local_renderer_h_ = 0;
  local_renderer_ = renderer;
  return true;
}

const std::vector<VideoCodec>& WebRtcVideoEngine::codecs() const {
  return video_codecs_;
}

const std::vector<RtpHeaderExtension>&
WebRtcVideoEngine::rtp_header_extensions() const {
  return rtp_header_extensions_;
}

void WebRtcVideoEngine::SetLogging(int min_sev, const char* filter) {
  // if min_sev == -1, we keep the current log level.
  if (min_sev >= 0) {
    SetTraceFilter(SeverityToFilter(min_sev));
  }
  SetTraceOptions(filter);
}

int WebRtcVideoEngine::GetLastEngineError() {
  return vie_wrapper_->error();
}

// Checks to see whether we comprehend and could receive a particular codec
bool WebRtcVideoEngine::FindCodec(const VideoCodec& in) {
  for (int i = 0; i < ARRAY_SIZE(kVideoFormats); ++i) {
    const VideoFormat fmt(kVideoFormats[i]);
    if ((in.width == 0 && in.height == 0) ||
        (fmt.width == in.width && fmt.height == in.height)) {
      if (encoder_factory_) {
        const std::vector<WebRtcVideoEncoderFactory::VideoCodec>& codecs =
            encoder_factory_->codecs();
        for (size_t j = 0; j < codecs.size(); ++j) {
          VideoCodec codec(GetExternalVideoPayloadType(static_cast<int>(j)),
                           codecs[j].name, 0, 0, 0, 0);
          if (codec.Matches(in))
            return true;
        }
      }
      for (size_t j = 0; j < ARRAY_SIZE(kVideoCodecPrefs); ++j) {
        VideoCodec codec(kVideoCodecPrefs[j].payload_type,
                         kVideoCodecPrefs[j].name, 0, 0, 0, 0);
        if (codec.Matches(in)) {
          return true;
        }
      }
    }
  }
  return false;
}

// Given the requested codec, returns true if we can send that codec type and
// updates out with the best quality we could send for that codec. If current is
// not empty, we constrain out so that its aspect ratio matches current's.
bool WebRtcVideoEngine::CanSendCodec(const VideoCodec& requested,
                                     const VideoCodec& current,
                                     VideoCodec* out) {
  if (!out) {
    return false;
  }

  std::vector<VideoCodec>::const_iterator local_max;
  for (local_max = video_codecs_.begin();
       local_max < video_codecs_.end();
       ++local_max) {
    // First match codecs by payload type
    if (!requested.Matches(*local_max)) {
      continue;
    }

    out->id = requested.id;
    out->name = requested.name;
    out->preference = requested.preference;
    out->params = requested.params;
    out->framerate = talk_base::_min(requested.framerate, local_max->framerate);
    out->width = 0;
    out->height = 0;
    out->params = requested.params;
    out->feedback_params = requested.feedback_params;

    if (0 == requested.width && 0 == requested.height) {
      // Special case with resolution 0. The channel should not send frames.
      return true;
    } else if (0 == requested.width || 0 == requested.height) {
      // 0xn and nx0 are invalid resolutions.
      return false;
    }

    // Pick the best quality that is within their and our bounds and has the
    // correct aspect ratio.
    for (int j = 0; j < ARRAY_SIZE(kVideoFormats); ++j) {
      const VideoFormat format(kVideoFormats[j]);

      // Skip any format that is larger than the local or remote maximums, or
      // smaller than the current best match
      if (format.width > requested.width || format.height > requested.height ||
          format.width > local_max->width ||
          (format.width < out->width && format.height < out->height)) {
        continue;
      }

      bool better = false;

      // Check any further constraints on this prospective format
      if (!out->width || !out->height) {
        // If we don't have any matches yet, this is the best so far.
        better = true;
      } else if (current.width && current.height) {
        // current is set so format must match its ratio exactly.
        better =
            (format.width * current.height == format.height * current.width);
      } else {
        // Prefer closer aspect ratios i.e
        // format.aspect - requested.aspect < out.aspect - requested.aspect
        better = abs(format.width * requested.height * out->height -
                     requested.width * format.height * out->height) <
                 abs(out->width * format.height * requested.height -
                     requested.width * format.height * out->height);
      }

      if (better) {
        out->width = format.width;
        out->height = format.height;
      }
    }
    if (out->width > 0) {
      return true;
    }
  }
  return false;
}

static void ConvertToCricketVideoCodec(
    const webrtc::VideoCodec& in_codec, VideoCodec* out_codec) {
  out_codec->id = in_codec.plType;
  out_codec->name = in_codec.plName;
  out_codec->width = in_codec.width;
  out_codec->height = in_codec.height;
  out_codec->framerate = in_codec.maxFramerate;
  out_codec->SetParam(kCodecParamMinBitrate, in_codec.minBitrate);
  out_codec->SetParam(kCodecParamMaxBitrate, in_codec.maxBitrate);
  if (in_codec.qpMax) {
    out_codec->SetParam(kCodecParamMaxQuantization, in_codec.qpMax);
  }
}

bool WebRtcVideoEngine::ConvertFromCricketVideoCodec(
    const VideoCodec& in_codec, webrtc::VideoCodec* out_codec) {
  bool found = false;
  int ncodecs = vie_wrapper_->codec()->NumberOfCodecs();
  for (int i = 0; i < ncodecs; ++i) {
    if (vie_wrapper_->codec()->GetCodec(i, *out_codec) == 0 &&
        _stricmp(in_codec.name.c_str(), out_codec->plName) == 0) {
      found = true;
      break;
    }
  }

  // If not found, check if this is supported by external encoder factory.
  if (!found && encoder_factory_) {
    const std::vector<WebRtcVideoEncoderFactory::VideoCodec>& codecs =
        encoder_factory_->codecs();
    for (size_t i = 0; i < codecs.size(); ++i) {
      if (_stricmp(in_codec.name.c_str(), codecs[i].name.c_str()) == 0) {
        out_codec->codecType = codecs[i].type;
        out_codec->plType = GetExternalVideoPayloadType(static_cast<int>(i));
        talk_base::strcpyn(out_codec->plName, sizeof(out_codec->plName),
                           codecs[i].name.c_str(), codecs[i].name.length());
        found = true;
        break;
      }
    }
  }

  if (!found) {
    LOG(LS_ERROR) << "invalid codec type";
    return false;
  }

  if (in_codec.id != 0)
    out_codec->plType = in_codec.id;

  if (in_codec.width != 0)
    out_codec->width = in_codec.width;

  if (in_codec.height != 0)
    out_codec->height = in_codec.height;

  if (in_codec.framerate != 0)
    out_codec->maxFramerate = in_codec.framerate;

  // Convert bitrate parameters.
  int max_bitrate = kMaxVideoBitrate;
  int min_bitrate = kMinVideoBitrate;
  int start_bitrate = kStartVideoBitrate;

  in_codec.GetParam(kCodecParamMinBitrate, &min_bitrate);
  in_codec.GetParam(kCodecParamMaxBitrate, &max_bitrate);

  if (max_bitrate < min_bitrate) {
    return false;
  }
  start_bitrate = talk_base::_max(start_bitrate, min_bitrate);
  start_bitrate = talk_base::_min(start_bitrate, max_bitrate);

  out_codec->minBitrate = min_bitrate;
  out_codec->startBitrate = start_bitrate;
  out_codec->maxBitrate = max_bitrate;

  // Convert general codec parameters.
  int max_quantization = 0;
  if (in_codec.GetParam(kCodecParamMaxQuantization, &max_quantization)) {
    if (max_quantization < 0) {
      return false;
    }
    out_codec->qpMax = max_quantization;
  }
  return true;
}

void WebRtcVideoEngine::RegisterChannel(WebRtcVideoMediaChannel *channel) {
  talk_base::CritScope cs(&channels_crit_);
  channels_.push_back(channel);
}

void WebRtcVideoEngine::UnregisterChannel(WebRtcVideoMediaChannel *channel) {
  talk_base::CritScope cs(&channels_crit_);
  channels_.erase(std::remove(channels_.begin(), channels_.end(), channel),
                  channels_.end());
}

bool WebRtcVideoEngine::SetVoiceEngine(WebRtcVoiceEngine* voice_engine) {
  if (initialized_) {
    LOG(LS_WARNING) << "SetVoiceEngine can not be called after Init";
    return false;
  }
  voice_engine_ = voice_engine;
  return true;
}

bool WebRtcVideoEngine::EnableTimedRender() {
  if (initialized_) {
    LOG(LS_WARNING) << "EnableTimedRender can not be called after Init";
    return false;
  }
  render_module_.reset(webrtc::VideoRender::CreateVideoRender(0, NULL,
      false, webrtc::kRenderExternal));
  return true;
}

void WebRtcVideoEngine::SetTraceFilter(int filter) {
  tracing_->SetTraceFilter(filter);
}

// See https://sites.google.com/a/google.com/wavelet/
//     Home/Magic-Flute--RTC-Engine-/Magic-Flute-Command-Line-Parameters
// for all supported command line setttings.
void WebRtcVideoEngine::SetTraceOptions(const std::string& options) {
  // Set WebRTC trace file.
  std::vector<std::string> opts;
  talk_base::tokenize(options, ' ', '"', '"', &opts);
  std::vector<std::string>::iterator tracefile =
      std::find(opts.begin(), opts.end(), "tracefile");
  if (tracefile != opts.end() && ++tracefile != opts.end()) {
    // Write WebRTC debug output (at same loglevel) to file
    if (tracing_->SetTraceFile(tracefile->c_str()) == -1) {
      LOG_RTCERR1(SetTraceFile, *tracefile);
    }
  }
}

static void AddDefaultFeedbackParams(VideoCodec* codec) {
  const FeedbackParam kFir(kRtcpFbParamCcm, kRtcpFbCcmParamFir);
  codec->AddFeedbackParam(kFir);
  const FeedbackParam kNack(kRtcpFbParamNack, kParamValueEmpty);
  codec->AddFeedbackParam(kNack);
  const FeedbackParam kPli(kRtcpFbParamNack, kRtcpFbNackParamPli);
  codec->AddFeedbackParam(kPli);
  const FeedbackParam kRemb(kRtcpFbParamRemb, kParamValueEmpty);
  codec->AddFeedbackParam(kRemb);
}

// Rebuilds the codec list to be only those that are less intensive
// than the specified codec. Prefers internal codec over external with
// higher preference field.
bool WebRtcVideoEngine::RebuildCodecList(const VideoCodec& in_codec) {
  if (!FindCodec(in_codec))
    return false;

  video_codecs_.clear();

  bool found = false;
  std::set<std::string> internal_codec_names;
  for (size_t i = 0; i < ARRAY_SIZE(kVideoCodecPrefs); ++i) {
    const VideoCodecPref& pref(kVideoCodecPrefs[i]);
    if (!found)
      found = (in_codec.name == pref.name);
    if (found) {
      VideoCodec codec(pref.payload_type, pref.name,
                       in_codec.width, in_codec.height, in_codec.framerate,
                       static_cast<int>(ARRAY_SIZE(kVideoCodecPrefs) - i));
      if (_stricmp(kVp8PayloadName, codec.name.c_str()) == 0) {
        AddDefaultFeedbackParams(&codec);
      }
      if (pref.associated_payload_type != -1) {
        codec.SetParam(kCodecParamAssociatedPayloadType,
                       pref.associated_payload_type);
      }
      video_codecs_.push_back(codec);
      internal_codec_names.insert(codec.name);
    }
  }
  if (encoder_factory_) {
    const std::vector<WebRtcVideoEncoderFactory::VideoCodec>& codecs =
        encoder_factory_->codecs();
    for (size_t i = 0; i < codecs.size(); ++i) {
      bool is_internal_codec = internal_codec_names.find(codecs[i].name) !=
          internal_codec_names.end();
      if (!is_internal_codec) {
        if (!found)
          found = (in_codec.name == codecs[i].name);
        VideoCodec codec(
            GetExternalVideoPayloadType(static_cast<int>(i)),
            codecs[i].name,
            codecs[i].max_width,
            codecs[i].max_height,
            codecs[i].max_fps,
            // Use negative preference on external codec to ensure the internal
            // codec is preferred.
            static_cast<int>(0 - i));
        AddDefaultFeedbackParams(&codec);
        video_codecs_.push_back(codec);
      }
    }
  }
  ASSERT(found);
  return true;
}

// Ignore spammy trace messages, mostly from the stats API when we haven't
// gotten RTCP info yet from the remote side.
bool WebRtcVideoEngine::ShouldIgnoreTrace(const std::string& trace) {
  static const char* const kTracesToIgnore[] = {
    NULL
  };
  for (const char* const* p = kTracesToIgnore; *p; ++p) {
    if (trace.find(*p) == 0) {
      return true;
    }
  }
  return false;
}

int WebRtcVideoEngine::GetNumOfChannels() {
  talk_base::CritScope cs(&channels_crit_);
  return static_cast<int>(channels_.size());
}

void WebRtcVideoEngine::Print(webrtc::TraceLevel level, const char* trace,
                              int length) {
  talk_base::LoggingSeverity sev = talk_base::LS_VERBOSE;
  if (level == webrtc::kTraceError || level == webrtc::kTraceCritical)
    sev = talk_base::LS_ERROR;
  else if (level == webrtc::kTraceWarning)
    sev = talk_base::LS_WARNING;
  else if (level == webrtc::kTraceStateInfo || level == webrtc::kTraceInfo)
    sev = talk_base::LS_INFO;
  else if (level == webrtc::kTraceTerseInfo)
    sev = talk_base::LS_INFO;

  // Skip past boilerplate prefix text
  if (length < 72) {
    std::string msg(trace, length);
    LOG(LS_ERROR) << "Malformed webrtc log message: ";
    LOG_V(sev) << msg;
  } else {
    std::string msg(trace + 71, length - 72);
    if (!ShouldIgnoreTrace(msg) &&
        (!voice_engine_ || !voice_engine_->ShouldIgnoreTrace(msg))) {
      LOG_V(sev) << "webrtc: " << msg;
    }
  }
}

webrtc::VideoDecoder* WebRtcVideoEngine::CreateExternalDecoder(
    webrtc::VideoCodecType type) {
  if (decoder_factory_ == NULL) {
    return NULL;
  }
  return decoder_factory_->CreateVideoDecoder(type);
}

void WebRtcVideoEngine::DestroyExternalDecoder(webrtc::VideoDecoder* decoder) {
  ASSERT(decoder_factory_ != NULL);
  if (decoder_factory_ == NULL)
    return;
  decoder_factory_->DestroyVideoDecoder(decoder);
}

webrtc::VideoEncoder* WebRtcVideoEngine::CreateExternalEncoder(
    webrtc::VideoCodecType type) {
  if (encoder_factory_ == NULL) {
    return NULL;
  }
  return encoder_factory_->CreateVideoEncoder(type);
}

void WebRtcVideoEngine::DestroyExternalEncoder(webrtc::VideoEncoder* encoder) {
  ASSERT(encoder_factory_ != NULL);
  if (encoder_factory_ == NULL)
    return;
  encoder_factory_->DestroyVideoEncoder(encoder);
}

bool WebRtcVideoEngine::IsExternalEncoderCodecType(
    webrtc::VideoCodecType type) const {
  if (!encoder_factory_)
    return false;
  const std::vector<WebRtcVideoEncoderFactory::VideoCodec>& codecs =
      encoder_factory_->codecs();
  std::vector<WebRtcVideoEncoderFactory::VideoCodec>::const_iterator it;
  for (it = codecs.begin(); it != codecs.end(); ++it) {
    if (it->type == type)
      return true;
  }
  return false;
}

void WebRtcVideoEngine::SetExternalDecoderFactory(
    WebRtcVideoDecoderFactory* decoder_factory) {
  decoder_factory_ = decoder_factory;
}

void WebRtcVideoEngine::SetExternalEncoderFactory(
    WebRtcVideoEncoderFactory* encoder_factory) {
  if (encoder_factory_ == encoder_factory)
    return;

  if (encoder_factory_) {
    encoder_factory_->RemoveObserver(this);
  }
  encoder_factory_ = encoder_factory;
  if (encoder_factory_) {
    encoder_factory_->AddObserver(this);
  }

  // Invoke OnCodecAvailable() here in case the list of codecs is already
  // available when the encoder factory is installed. If not the encoder
  // factory will invoke the callback later when the codecs become available.
  OnCodecsAvailable();
}

void WebRtcVideoEngine::OnCodecsAvailable() {
  // Rebuild codec list while reapplying the current default codec format.
  VideoCodec max_codec(kVideoCodecPrefs[0].payload_type,
                       kVideoCodecPrefs[0].name,
                       video_codecs_[0].width,
                       video_codecs_[0].height,
                       video_codecs_[0].framerate,
                       0);
  if (!RebuildCodecList(max_codec)) {
    LOG(LS_ERROR) << "Failed to initialize list of supported codec types";
  }
}

// WebRtcVideoMediaChannel

WebRtcVideoMediaChannel::WebRtcVideoMediaChannel(
    WebRtcVideoEngine* engine,
    VoiceMediaChannel* channel)
    : engine_(engine),
      voice_channel_(channel),
      vie_channel_(-1),
      nack_enabled_(true),
      remb_enabled_(false),
      render_started_(false),
      first_receive_ssrc_(0),
      send_rtx_type_(-1),
      send_red_type_(-1),
      send_fec_type_(-1),
      send_min_bitrate_(kMinVideoBitrate),
      send_start_bitrate_(kStartVideoBitrate),
      send_max_bitrate_(kMaxVideoBitrate),
      sending_(false),
      ratio_w_(0),
      ratio_h_(0) {
  engine->RegisterChannel(this);
}

bool WebRtcVideoMediaChannel::Init() {
  const uint32 ssrc_key = 0;
  return CreateChannel(ssrc_key, MD_SENDRECV, &vie_channel_);
}

WebRtcVideoMediaChannel::~WebRtcVideoMediaChannel() {
  const bool send = false;
  SetSend(send);
  const bool render = false;
  SetRender(render);

  while (!send_channels_.empty()) {
    if (!DeleteSendChannel(send_channels_.begin()->first)) {
      LOG(LS_ERROR) << "Unable to delete channel with ssrc key "
                    << send_channels_.begin()->first;
      ASSERT(false);
      break;
    }
  }

  // Remove all receive streams and the default channel.
  while (!recv_channels_.empty()) {
    RemoveRecvStream(recv_channels_.begin()->first);
  }

  // Unregister the channel from the engine.
  engine()->UnregisterChannel(this);
  if (worker_thread()) {
    worker_thread()->Clear(this);
  }
}

bool WebRtcVideoMediaChannel::SetRecvCodecs(
    const std::vector<VideoCodec>& codecs) {
  receive_codecs_.clear();
  for (std::vector<VideoCodec>::const_iterator iter = codecs.begin();
      iter != codecs.end(); ++iter) {
    if (engine()->FindCodec(*iter)) {
      webrtc::VideoCodec wcodec;
      if (engine()->ConvertFromCricketVideoCodec(*iter, &wcodec)) {
        receive_codecs_.push_back(wcodec);
      }
    } else {
      LOG(LS_INFO) << "Unknown codec " << iter->name;
      return false;
    }
  }

  for (RecvChannelMap::iterator it = recv_channels_.begin();
      it != recv_channels_.end(); ++it) {
    if (!SetReceiveCodecs(it->second))
      return false;
  }
  return true;
}

bool WebRtcVideoMediaChannel::SetSendCodecs(
    const std::vector<VideoCodec>& codecs) {
  // Match with local video codec list.
  std::vector<webrtc::VideoCodec> send_codecs;
  VideoCodec checked_codec;
  VideoCodec current;  // defaults to 0x0
  if (sending_) {
    ConvertToCricketVideoCodec(*send_codec_, &current);
  }
  std::map<int, int> primary_rtx_pt_mapping;
  for (std::vector<VideoCodec>::const_iterator iter = codecs.begin();
      iter != codecs.end(); ++iter) {
    if (_stricmp(iter->name.c_str(), kRedPayloadName) == 0) {
      send_red_type_ = iter->id;
    } else if (_stricmp(iter->name.c_str(), kFecPayloadName) == 0) {
      send_fec_type_ = iter->id;
    } else if (_stricmp(iter->name.c_str(), kRtxCodecName) == 0) {
      int rtx_type = iter->id;
      int rtx_primary_type = -1;
      if (iter->GetParam(kCodecParamAssociatedPayloadType, &rtx_primary_type)) {
        primary_rtx_pt_mapping[rtx_primary_type] = rtx_type;
      }
    } else if (engine()->CanSendCodec(*iter, current, &checked_codec)) {
      webrtc::VideoCodec wcodec;
      if (engine()->ConvertFromCricketVideoCodec(checked_codec, &wcodec)) {
        if (send_codecs.empty()) {
          nack_enabled_ = IsNackEnabled(checked_codec);
          remb_enabled_ = IsRembEnabled(checked_codec);
        }
        send_codecs.push_back(wcodec);
      }
    } else {
      LOG(LS_WARNING) << "Unknown codec " << iter->name;
    }
  }

  // Fail if we don't have a match.
  if (send_codecs.empty()) {
    LOG(LS_WARNING) << "No matching codecs available";
    return false;
  }

  // Recv protection.
  for (RecvChannelMap::iterator it = recv_channels_.begin();
      it != recv_channels_.end(); ++it) {
    int channel_id = it->second->channel_id();
    if (!SetNackFec(channel_id, send_red_type_, send_fec_type_,
                    nack_enabled_)) {
      return false;
    }
    if (engine_->vie()->rtp()->SetRembStatus(channel_id,
                                             kNotSending,
                                             remb_enabled_) != 0) {
      LOG_RTCERR3(SetRembStatus, channel_id, kNotSending, remb_enabled_);
      return false;
    }
  }

  // Send settings.
  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    int channel_id = iter->second->channel_id();
    if (!SetNackFec(channel_id, send_red_type_, send_fec_type_,
                    nack_enabled_)) {
      return false;
    }
    if (engine_->vie()->rtp()->SetRembStatus(channel_id,
                                             remb_enabled_,
                                             remb_enabled_) != 0) {
      LOG_RTCERR3(SetRembStatus, channel_id, remb_enabled_, remb_enabled_);
      return false;
    }
  }

  // Select the first matched codec.
  webrtc::VideoCodec& codec(send_codecs[0]);

  // Set RTX payload type if primary now active. This value will be used  in
  // SetSendCodec.
  std::map<int, int>::const_iterator rtx_it =
    primary_rtx_pt_mapping.find(static_cast<int>(codec.plType));
  if (rtx_it != primary_rtx_pt_mapping.end()) {
    send_rtx_type_ = rtx_it->second;
  }

  if (!SetSendCodec(
          codec, codec.minBitrate, codec.startBitrate, codec.maxBitrate)) {
    return false;
  }

  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    WebRtcVideoChannelSendInfo* send_channel = iter->second;
    send_channel->InitializeAdapterOutputFormat(codec);
  }

  LogSendCodecChange("SetSendCodecs()");

  return true;
}

bool WebRtcVideoMediaChannel::GetSendCodec(VideoCodec* send_codec) {
  if (!send_codec_) {
    return false;
  }
  ConvertToCricketVideoCodec(*send_codec_, send_codec);
  return true;
}

bool WebRtcVideoMediaChannel::SetSendStreamFormat(uint32 ssrc,
                                                  const VideoFormat& format) {
  if (!send_codec_) {
    LOG(LS_ERROR) << "The send codec has not been set yet.";
    return false;
  }
  WebRtcVideoChannelSendInfo* send_channel = GetSendChannel(ssrc);
  if (!send_channel) {
    LOG(LS_ERROR) << "The specified ssrc " << ssrc << " is not in use.";
    return false;
  }
  send_channel->set_video_format(format);
  return true;
}

bool WebRtcVideoMediaChannel::SetRender(bool render) {
  if (render == render_started_) {
    return true;  // no action required
  }

  bool ret = true;
  for (RecvChannelMap::iterator it = recv_channels_.begin();
      it != recv_channels_.end(); ++it) {
    if (render) {
      if (engine()->vie()->render()->StartRender(
          it->second->channel_id()) != 0) {
        LOG_RTCERR1(StartRender, it->second->channel_id());
        ret = false;
      }
    } else {
      if (engine()->vie()->render()->StopRender(
          it->second->channel_id()) != 0) {
        LOG_RTCERR1(StopRender, it->second->channel_id());
        ret = false;
      }
    }
  }
  if (ret) {
    render_started_ = render;
  }

  return ret;
}

bool WebRtcVideoMediaChannel::SetSend(bool send) {
  if (!HasReadySendChannels() && send) {
    LOG(LS_ERROR) << "No stream added";
    return false;
  }
  if (send == sending()) {
    return true;  // No action required.
  }

  if (send) {
    // We've been asked to start sending.
    // SetSendCodecs must have been called already.
    if (!send_codec_) {
      return false;
    }
    // Start send now.
    if (!StartSend()) {
      return false;
    }
  } else {
    // We've been asked to stop sending.
    if (!StopSend()) {
      return false;
    }
  }
  sending_ = send;

  return true;
}

bool WebRtcVideoMediaChannel::AddSendStream(const StreamParams& sp) {
  LOG(LS_INFO) << "AddSendStream " << sp.ToString();

  if (!IsOneSsrcStream(sp) && !IsSimulcastStream(sp)) {
    LOG(LS_ERROR) << "AddSendStream: bad local stream parameters";
    return false;
  }

  uint32 ssrc_key;
  if (!CreateSendChannelKey(sp.first_ssrc(), &ssrc_key)) {
    LOG(LS_ERROR) << "Trying to register duplicate ssrc: " << sp.first_ssrc();
    return false;
  }
  // If the default channel is already used for sending create a new channel
  // otherwise use the default channel for sending.
  int channel_id = -1;
  if (send_channels_[0]->stream_params() == NULL) {
    channel_id = vie_channel_;
  } else {
    if (!CreateChannel(ssrc_key, MD_SEND, &channel_id)) {
      LOG(LS_ERROR) << "AddSendStream: unable to create channel";
      return false;
    }
  }
  WebRtcVideoChannelSendInfo* send_channel = send_channels_[ssrc_key];
  // Set the send (local) SSRC.
  // If there are multiple send SSRCs, we can only set the first one here, and
  // the rest of the SSRC(s) need to be set after SetSendCodec has been called
  // (with a codec requires multiple SSRC(s)).
  if (engine()->vie()->rtp()->SetLocalSSRC(channel_id,
                                           sp.first_ssrc()) != 0) {
    LOG_RTCERR2(SetLocalSSRC, channel_id, sp.first_ssrc());
    return false;
  }

  // Set the corresponding RTX SSRC.
  if (!SetLocalRtxSsrc(channel_id, sp, sp.first_ssrc(), 0)) {
    return false;
  }

  // Set RTCP CName.
  if (engine()->vie()->rtp()->SetRTCPCName(channel_id,
                                           sp.cname.c_str()) != 0) {
    LOG_RTCERR2(SetRTCPCName, channel_id, sp.cname.c_str());
    return false;
  }

  // At this point the channel's local SSRC has been updated. If the channel is
  // the default channel make sure that all the receive channels are updated as
  // well. Receive channels have to have the same SSRC as the default channel in
  // order to send receiver reports with this SSRC.
  if (IsDefaultChannel(channel_id)) {
    for (RecvChannelMap::const_iterator it = recv_channels_.begin();
         it != recv_channels_.end(); ++it) {
      WebRtcVideoChannelRecvInfo* info = it->second;
      int channel_id = info->channel_id();
      if (engine()->vie()->rtp()->SetLocalSSRC(channel_id,
                                               sp.first_ssrc()) != 0) {
        LOG_RTCERR1(SetLocalSSRC, it->first);
        return false;
      }
    }
  }

  send_channel->set_stream_params(sp);

  // Reset send codec after stream parameters changed.
  if (send_codec_) {
    if (!SetSendCodec(send_channel, *send_codec_, send_min_bitrate_,
                      send_start_bitrate_, send_max_bitrate_)) {
      return false;
    }
    LogSendCodecChange("SetSendStreamFormat()");
  }

  if (sending_) {
    return StartSend(send_channel);
  }
  return true;
}

bool WebRtcVideoMediaChannel::RemoveSendStream(uint32 ssrc) {
  uint32 ssrc_key;
  if (!GetSendChannelKey(ssrc, &ssrc_key)) {
    LOG(LS_WARNING) << "Try to remove stream with ssrc " << ssrc
                    << " which doesn't exist.";
    return false;
  }
  WebRtcVideoChannelSendInfo* send_channel = send_channels_[ssrc_key];
  int channel_id = send_channel->channel_id();
  if (IsDefaultChannel(channel_id) && (send_channel->stream_params() == NULL)) {
    // Default channel will still exist. However, if stream_params() is NULL
    // there is no stream to remove.
    return false;
  }
  if (sending_) {
    StopSend(send_channel);
  }

  const WebRtcVideoChannelSendInfo::EncoderMap& encoder_map =
      send_channel->registered_encoders();
  for (WebRtcVideoChannelSendInfo::EncoderMap::const_iterator it =
      encoder_map.begin(); it != encoder_map.end(); ++it) {
    if (engine()->vie()->ext_codec()->DeRegisterExternalSendCodec(
        channel_id, it->first) != 0) {
      LOG_RTCERR1(DeregisterEncoderObserver, channel_id);
    }
    engine()->DestroyExternalEncoder(it->second);
  }
  send_channel->ClearRegisteredEncoders();

  // The receive channels depend on the default channel, recycle it instead.
  if (IsDefaultChannel(channel_id)) {
    SetCapturer(GetDefaultChannelSsrc(), NULL);
    send_channel->ClearStreamParams();
  } else {
    return DeleteSendChannel(ssrc_key);
  }
  return true;
}

bool WebRtcVideoMediaChannel::AddRecvStream(const StreamParams& sp) {
  // TODO(zhurunz) Remove this once BWE works properly across different send
  // and receive channels.
  // Reuse default channel for recv stream in 1:1 call.
  if (!InConferenceMode() && first_receive_ssrc_ == 0) {
    LOG(LS_INFO) << "Recv stream " << sp.first_ssrc()
                 << " reuse default channel #"
                 << vie_channel_;
    first_receive_ssrc_ = sp.first_ssrc();
    if (render_started_) {
      if (engine()->vie()->render()->StartRender(vie_channel_) !=0) {
        LOG_RTCERR1(StartRender, vie_channel_);
      }
    }
    return true;
  }

  if (recv_channels_.find(sp.first_ssrc()) != recv_channels_.end() ||
      first_receive_ssrc_ == sp.first_ssrc()) {
    LOG(LS_ERROR) << "Stream already exists";
    return false;
  }

  // TODO(perkj): Implement recv media from multiple media SSRCs per stream.
  // NOTE: We have two SSRCs per stream when RTX is enabled.
  if (!IsOneSsrcStream(sp)) {
    LOG(LS_ERROR) << "WebRtcVideoMediaChannel supports one primary SSRC per"
                  << " stream and one FID SSRC per primary SSRC.";
    return false;
  }

  // Create a new channel for receiving video data.
  // In order to get the bandwidth estimation work fine for
  // receive only channels, we connect all receiving channels
  // to our master send channel.
  int channel_id = -1;
  if (!CreateChannel(sp.first_ssrc(), MD_RECV, &channel_id)) {
    return false;
  }

  // Set the corresponding RTX SSRC.
  uint32 rtx_ssrc;
  bool has_rtx = sp.GetFidSsrc(sp.first_ssrc(), &rtx_ssrc);
  if (has_rtx && engine()->vie()->rtp()->SetRemoteSSRCType(
      channel_id, webrtc::kViEStreamTypeRtx, rtx_ssrc) != 0) {
    LOG_RTCERR3(SetRemoteSSRCType, channel_id, webrtc::kViEStreamTypeRtx,
                rtx_ssrc);
    return false;
  }

  // Get the default renderer.
  VideoRenderer* default_renderer = NULL;
  if (InConferenceMode()) {
    // The recv_channels_ size start out being 1, so if it is two here this
    // is the first receive channel created (vie_channel_ is not used for
    // receiving in a conference call). This means that the renderer stored
    // inside vie_channel_ should be used for the just created channel.
    if (recv_channels_.size() == 2 &&
        recv_channels_.find(0) != recv_channels_.end()) {
      GetRenderer(0, &default_renderer);
    }
  }

  // The first recv stream reuses the default renderer (if a default renderer
  // has been set).
  if (default_renderer) {
    SetRenderer(sp.first_ssrc(), default_renderer);
  }

  LOG(LS_INFO) << "New video stream " << sp.first_ssrc()
               << " registered to VideoEngine channel #"
               << channel_id << " and connected to channel #" << vie_channel_;

  return true;
}

bool WebRtcVideoMediaChannel::RemoveRecvStream(uint32 ssrc) {
  RecvChannelMap::iterator it = recv_channels_.find(ssrc);

  if (it == recv_channels_.end()) {
    // TODO(perkj): Remove this once BWE works properly across different send
    // and receive channels.
    // The default channel is reused for recv stream in 1:1 call.
    if (first_receive_ssrc_ == ssrc) {
      first_receive_ssrc_ = 0;
      // Need to stop the renderer and remove it since the render window can be
      // deleted after this.
      if (render_started_) {
        if (engine()->vie()->render()->StopRender(vie_channel_) !=0) {
          LOG_RTCERR1(StopRender, it->second->channel_id());
        }
      }
      recv_channels_[0]->SetRenderer(NULL);
      return true;
    }
    return false;
  }
  WebRtcVideoChannelRecvInfo* info = it->second;
  int channel_id = info->channel_id();
  if (engine()->vie()->render()->RemoveRenderer(channel_id) != 0) {
    LOG_RTCERR1(RemoveRenderer, channel_id);
  }

  if (engine()->vie()->network()->DeregisterSendTransport(channel_id) !=0) {
    LOG_RTCERR1(DeRegisterSendTransport, channel_id);
  }

  if (engine()->vie()->codec()->DeregisterDecoderObserver(
      channel_id) != 0) {
    LOG_RTCERR1(DeregisterDecoderObserver, channel_id);
  }

  const WebRtcVideoChannelRecvInfo::DecoderMap& decoder_map =
      info->registered_decoders();
  for (WebRtcVideoChannelRecvInfo::DecoderMap::const_iterator it =
       decoder_map.begin(); it != decoder_map.end(); ++it) {
    if (engine()->vie()->ext_codec()->DeRegisterExternalReceiveCodec(
        channel_id, it->first) != 0) {
      LOG_RTCERR1(DeregisterDecoderObserver, channel_id);
    }
    engine()->DestroyExternalDecoder(it->second);
  }
  info->ClearRegisteredDecoders();

  LOG(LS_INFO) << "Removing video stream " << ssrc
               << " with VideoEngine channel #"
               << channel_id;
  bool ret = true;
  if (engine()->vie()->base()->DeleteChannel(channel_id) == -1) {
    LOG_RTCERR1(DeleteChannel, channel_id);
    ret = false;
  }
  // Delete the WebRtcVideoChannelRecvInfo pointed to by it->second.
  delete info;
  recv_channels_.erase(it);
  return ret;
}

bool WebRtcVideoMediaChannel::StartSend() {
  bool success = true;
  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    WebRtcVideoChannelSendInfo* send_channel = iter->second;
    if (!StartSend(send_channel)) {
      success = false;
    }
  }
  return success;
}

bool WebRtcVideoMediaChannel::StartSend(
    WebRtcVideoChannelSendInfo* send_channel) {
  const int channel_id = send_channel->channel_id();
  if (engine()->vie()->base()->StartSend(channel_id) != 0) {
    LOG_RTCERR1(StartSend, channel_id);
    return false;
  }

  send_channel->set_sending(true);
  return true;
}

bool WebRtcVideoMediaChannel::StopSend() {
  bool success = true;
  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    WebRtcVideoChannelSendInfo* send_channel = iter->second;
    if (!StopSend(send_channel)) {
      success = false;
    }
  }
  return success;
}

bool WebRtcVideoMediaChannel::StopSend(
    WebRtcVideoChannelSendInfo* send_channel) {
  const int channel_id = send_channel->channel_id();
  if (engine()->vie()->base()->StopSend(channel_id) != 0) {
    LOG_RTCERR1(StopSend, channel_id);
    return false;
  }
  send_channel->set_sending(false);
  return true;
}

bool WebRtcVideoMediaChannel::SendIntraFrame() {
  bool success = true;
  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end();
       ++iter) {
    WebRtcVideoChannelSendInfo* send_channel = iter->second;
    const int channel_id = send_channel->channel_id();
    if (engine()->vie()->codec()->SendKeyFrame(channel_id) != 0) {
      LOG_RTCERR1(SendKeyFrame, channel_id);
      success = false;
    }
  }
  return success;
}

bool WebRtcVideoMediaChannel::HasReadySendChannels() {
  return !send_channels_.empty() &&
      ((send_channels_.size() > 1) ||
       (send_channels_[0]->stream_params() != NULL));
}

bool WebRtcVideoMediaChannel::GetSendChannelKey(uint32 local_ssrc,
                                                uint32* key) {
  *key = 0;
  // If a send channel is not ready to send it will not have local_ssrc
  // registered to it.
  if (!HasReadySendChannels()) {
    return false;
  }
  // The default channel is stored with key 0. The key therefore does not match
  // the SSRC associated with the default channel. Check if the SSRC provided
  // corresponds to the default channel's SSRC.
  if (local_ssrc == GetDefaultChannelSsrc()) {
    return true;
  }
  if (send_channels_.find(local_ssrc) == send_channels_.end()) {
    for (SendChannelMap::iterator iter = send_channels_.begin();
         iter != send_channels_.end(); ++iter) {
      WebRtcVideoChannelSendInfo* send_channel = iter->second;
      if (send_channel->has_ssrc(local_ssrc)) {
        *key = iter->first;
        return true;
      }
    }
    return false;
  }
  // The key was found in the above std::map::find call. This means that the
  // ssrc is the key.
  *key = local_ssrc;
  return true;
}

WebRtcVideoChannelSendInfo* WebRtcVideoMediaChannel::GetSendChannel(
    uint32 local_ssrc) {
  uint32 key;
  if (!GetSendChannelKey(local_ssrc, &key)) {
    return NULL;
  }
  return send_channels_[key];
}

bool WebRtcVideoMediaChannel::CreateSendChannelKey(uint32 local_ssrc,
                                                   uint32* key) {
  if (GetSendChannelKey(local_ssrc, key)) {
    // If there is a key corresponding to |local_ssrc|, the SSRC is already in
    // use. SSRCs need to be unique in a session and at this point a duplicate
    // SSRC has been detected.
    return false;
  }
  if (send_channels_[0]->stream_params() == NULL) {
    // key should be 0 here as the default channel should be re-used whenever it
    // is not used.
    *key = 0;
    return true;
  }
  // SSRC is currently not in use and the default channel is already in use. Use
  // the SSRC as key since it is supposed to be unique in a session.
  *key = local_ssrc;
  return true;
}

int WebRtcVideoMediaChannel::GetSendChannelNum(VideoCapturer* capturer) {
  int num = 0;
  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    WebRtcVideoChannelSendInfo* send_channel = iter->second;
    if (send_channel->video_capturer() == capturer) {
      ++num;
    }
  }
  return num;
}

uint32 WebRtcVideoMediaChannel::GetDefaultChannelSsrc() {
  WebRtcVideoChannelSendInfo* send_channel = send_channels_[0];
  const StreamParams* sp = send_channel->stream_params();
  if (sp == NULL) {
    // This happens if no send stream is currently registered.
    return 0;
  }
  return sp->first_ssrc();
}

bool WebRtcVideoMediaChannel::DeleteSendChannel(uint32 ssrc_key) {
  if (send_channels_.find(ssrc_key) == send_channels_.end()) {
    return false;
  }
  WebRtcVideoChannelSendInfo* send_channel = send_channels_[ssrc_key];
  MaybeDisconnectCapturer(send_channel->video_capturer());
  send_channel->set_video_capturer(NULL);

  int channel_id = send_channel->channel_id();
  int capture_id = send_channel->capture_id();
  if (engine()->vie()->codec()->DeregisterEncoderObserver(
          channel_id) != 0) {
    LOG_RTCERR1(DeregisterEncoderObserver, channel_id);
  }

  // Destroy the external capture interface.
  if (engine()->vie()->capture()->DisconnectCaptureDevice(
          channel_id) != 0) {
    LOG_RTCERR1(DisconnectCaptureDevice, channel_id);
  }
  if (engine()->vie()->capture()->ReleaseCaptureDevice(
          capture_id) != 0) {
    LOG_RTCERR1(ReleaseCaptureDevice, capture_id);
  }

  // The default channel is stored in both |send_channels_| and
  // |recv_channels_|. To make sure it is only deleted once from vie let the
  // delete call happen when tearing down |recv_channels_| and not here.
  if (!IsDefaultChannel(channel_id)) {
    engine_->vie()->base()->DeleteChannel(channel_id);
  }
  delete send_channel;
  send_channels_.erase(ssrc_key);
  return true;
}

bool WebRtcVideoMediaChannel::RemoveCapturer(uint32 ssrc) {
  WebRtcVideoChannelSendInfo* send_channel = GetSendChannel(ssrc);
  if (!send_channel) {
    return false;
  }
  VideoCapturer* capturer = send_channel->video_capturer();
  if (capturer == NULL) {
    return false;
  }
  MaybeDisconnectCapturer(capturer);
  send_channel->set_video_capturer(NULL);
  const int64 timestamp = send_channel->local_stream_info()->time_stamp();
  if (send_codec_) {
    QueueBlackFrame(ssrc, timestamp, send_codec_->maxFramerate);
  }
  return true;
}

bool WebRtcVideoMediaChannel::SetRenderer(uint32 ssrc,
                                          VideoRenderer* renderer) {
  if (recv_channels_.find(ssrc) == recv_channels_.end()) {
    // TODO(perkj): Remove this once BWE works properly across different send
    // and receive channels.
    // The default channel is reused for recv stream in 1:1 call.
    if (first_receive_ssrc_ == ssrc &&
        recv_channels_.find(0) != recv_channels_.end()) {
      LOG(LS_INFO) << "SetRenderer " << ssrc
                   << " reuse default channel #"
                   << vie_channel_;
      recv_channels_[0]->SetRenderer(renderer);
      return true;
    }
    return false;
  }

  recv_channels_[ssrc]->SetRenderer(renderer);
  return true;
}

bool WebRtcVideoMediaChannel::GetStats(VideoMediaInfo* info) {
  // Get sender statistics and build VideoSenderInfo.
  unsigned int total_bitrate_sent = 0;
  unsigned int video_bitrate_sent = 0;
  unsigned int fec_bitrate_sent = 0;
  unsigned int nack_bitrate_sent = 0;
  unsigned int estimated_send_bandwidth = 0;
  unsigned int target_enc_bitrate = 0;
  if (send_codec_) {
    for (SendChannelMap::const_iterator iter = send_channels_.begin();
         iter != send_channels_.end(); ++iter) {
      WebRtcVideoChannelSendInfo* send_channel = iter->second;
      const int channel_id = send_channel->channel_id();
      VideoSenderInfo sinfo;
      const StreamParams* send_params = send_channel->stream_params();
      if (send_params == NULL) {
        // This should only happen if the default vie channel is not in use.
        // This can happen if no streams have ever been added or the stream
        // corresponding to the default channel has been removed. Note that
        // there may be non-default vie channels in use when this happen so
        // asserting send_channels_.size() == 1 is not correct and neither is
        // breaking out of the loop.
        ASSERT(channel_id == vie_channel_);
        continue;
      }
      unsigned int bytes_sent, packets_sent, bytes_recv, packets_recv;
      if (engine_->vie()->rtp()->GetRTPStatistics(channel_id, bytes_sent,
                                                  packets_sent, bytes_recv,
                                                  packets_recv) != 0) {
        LOG_RTCERR1(GetRTPStatistics, vie_channel_);
        continue;
      }
      WebRtcLocalStreamInfo* channel_stream_info =
          send_channel->local_stream_info();

      for (size_t i = 0; i < send_params->ssrcs.size(); ++i) {
        sinfo.add_ssrc(send_params->ssrcs[i]);
      }
      sinfo.codec_name = send_codec_->plName;
      sinfo.bytes_sent = bytes_sent;
      sinfo.packets_sent = packets_sent;
      sinfo.packets_cached = -1;
      sinfo.packets_lost = -1;
      sinfo.fraction_lost = -1;
      sinfo.firs_rcvd = -1;
      sinfo.nacks_rcvd = -1;
      sinfo.rtt_ms = -1;
      sinfo.input_frame_width = static_cast<int>(channel_stream_info->width());
      sinfo.input_frame_height =
          static_cast<int>(channel_stream_info->height());
      webrtc::VideoCodec vie_codec;
      if (engine()->vie()->codec()->GetSendCodec(channel_id, vie_codec) == 0) {
        sinfo.send_frame_width = vie_codec.width;
        sinfo.send_frame_height = vie_codec.height;
      } else {
        sinfo.send_frame_width = -1;
        sinfo.send_frame_height = -1;
        LOG_RTCERR1(GetSendCodec, channel_id);
      }
      sinfo.framerate_input = channel_stream_info->framerate();
      sinfo.framerate_sent = send_channel->encoder_observer()->framerate();
      sinfo.nominal_bitrate = send_channel->encoder_observer()->bitrate();
      sinfo.preferred_bitrate = send_max_bitrate_;
      sinfo.adapt_reason = send_channel->CurrentAdaptReason();
      sinfo.capture_jitter_ms = -1;
      sinfo.avg_encode_ms = -1;
      sinfo.encode_usage_percent = -1;
      sinfo.capture_queue_delay_ms_per_s = -1;

      int capture_jitter_ms = 0;
      int avg_encode_time_ms = 0;
      int encode_usage_percent = 0;
      int capture_queue_delay_ms_per_s = 0;
      if (engine()->vie()->base()->CpuOveruseMeasures(
          channel_id,
          &capture_jitter_ms,
          &avg_encode_time_ms,
          &encode_usage_percent,
          &capture_queue_delay_ms_per_s) == 0) {
        sinfo.capture_jitter_ms = capture_jitter_ms;
        sinfo.avg_encode_ms = avg_encode_time_ms;
        sinfo.encode_usage_percent = encode_usage_percent;
        sinfo.capture_queue_delay_ms_per_s = capture_queue_delay_ms_per_s;
      }

      // Get received RTCP statistics for the sender (reported by the remote
      // client in a RTCP packet), if available.
      // It's not a fatal error if we can't, since RTCP may not have arrived
      // yet.
      webrtc::RtcpStatistics outgoing_stream_rtcp_stats;
      int outgoing_stream_rtt_ms;

      if (engine_->vie()->rtp()->GetSendChannelRtcpStatistics(
          channel_id,
          outgoing_stream_rtcp_stats,
          outgoing_stream_rtt_ms) == 0) {
        // Convert Q8 to float.
        sinfo.packets_lost = outgoing_stream_rtcp_stats.cumulative_lost;
        sinfo.fraction_lost = static_cast<float>(
            outgoing_stream_rtcp_stats.fraction_lost) / (1 << 8);
        sinfo.rtt_ms = outgoing_stream_rtt_ms;
      }
      info->senders.push_back(sinfo);

      unsigned int channel_total_bitrate_sent = 0;
      unsigned int channel_video_bitrate_sent = 0;
      unsigned int channel_fec_bitrate_sent = 0;
      unsigned int channel_nack_bitrate_sent = 0;
      if (engine_->vie()->rtp()->GetBandwidthUsage(
          channel_id, channel_total_bitrate_sent, channel_video_bitrate_sent,
          channel_fec_bitrate_sent, channel_nack_bitrate_sent) == 0) {
        total_bitrate_sent += channel_total_bitrate_sent;
        video_bitrate_sent += channel_video_bitrate_sent;
        fec_bitrate_sent += channel_fec_bitrate_sent;
        nack_bitrate_sent += channel_nack_bitrate_sent;
      } else {
        LOG_RTCERR1(GetBandwidthUsage, channel_id);
      }

      unsigned int estimated_stream_send_bandwidth = 0;
      if (engine_->vie()->rtp()->GetEstimatedSendBandwidth(
          channel_id, &estimated_stream_send_bandwidth) == 0) {
        estimated_send_bandwidth += estimated_stream_send_bandwidth;
      } else {
        LOG_RTCERR1(GetEstimatedSendBandwidth, channel_id);
      }
      unsigned int target_enc_stream_bitrate = 0;
      if (engine_->vie()->codec()->GetCodecTargetBitrate(
          channel_id, &target_enc_stream_bitrate) == 0) {
        target_enc_bitrate += target_enc_stream_bitrate;
      } else {
        LOG_RTCERR1(GetCodecTargetBitrate, channel_id);
      }
    }
  } else {
    LOG(LS_WARNING) << "GetStats: sender information not ready.";
  }

  // Get the SSRC and stats for each receiver, based on our own calculations.
  unsigned int estimated_recv_bandwidth = 0;
  for (RecvChannelMap::const_iterator it = recv_channels_.begin();
       it != recv_channels_.end(); ++it) {
    // Don't report receive statistics from the default channel if we have
    // specified receive channels.
    if (it->first == 0 && recv_channels_.size() > 1)
      continue;
    WebRtcVideoChannelRecvInfo* channel = it->second;

    unsigned int ssrc;
    // Get receiver statistics and build VideoReceiverInfo, if we have data.
    // Skip the default channel (ssrc == 0).
    if (engine_->vie()->rtp()->GetRemoteSSRC(
            channel->channel_id(), ssrc) != 0 ||
        ssrc == 0)
      continue;

    webrtc::StreamDataCounters sent;
    webrtc::StreamDataCounters received;
    if (engine_->vie()->rtp()->GetRtpStatistics(channel->channel_id(),
                                                sent, received) != 0) {
      LOG_RTCERR1(GetRTPStatistics, channel->channel_id());
      return false;
    }
    VideoReceiverInfo rinfo;
    rinfo.add_ssrc(ssrc);
    rinfo.bytes_rcvd = received.bytes;
    rinfo.packets_rcvd = received.packets;
    rinfo.packets_lost = -1;
    rinfo.packets_concealed = -1;
    rinfo.fraction_lost = -1;  // from SentRTCP
    rinfo.nacks_sent = -1;
    rinfo.frame_width = channel->render_adapter()->width();
    rinfo.frame_height = channel->render_adapter()->height();
    int fps = channel->render_adapter()->framerate();
    rinfo.framerate_decoded = fps;
    rinfo.framerate_output = fps;
    channel->decoder_observer()->ExportTo(&rinfo);

    // Get our locally created statistics of the received RTP stream.
    webrtc::RtcpStatistics incoming_stream_rtcp_stats;
    int incoming_stream_rtt_ms;
    if (engine_->vie()->rtp()->GetReceiveChannelRtcpStatistics(
        channel->channel_id(),
        incoming_stream_rtcp_stats,
        incoming_stream_rtt_ms) == 0) {
      // Convert Q8 to float.
      rinfo.packets_lost = incoming_stream_rtcp_stats.cumulative_lost;
      rinfo.fraction_lost = static_cast<float>(
          incoming_stream_rtcp_stats.fraction_lost) / (1 << 8);
    }
    info->receivers.push_back(rinfo);

    unsigned int estimated_recv_stream_bandwidth = 0;
    if (engine_->vie()->rtp()->GetEstimatedReceiveBandwidth(
        channel->channel_id(), &estimated_recv_stream_bandwidth) == 0) {
      estimated_recv_bandwidth += estimated_recv_stream_bandwidth;
    } else {
      LOG_RTCERR1(GetEstimatedReceiveBandwidth, channel->channel_id());
    }
  }

  // Build BandwidthEstimationInfo.
  // TODO(zhurunz): Add real unittest for this.
  BandwidthEstimationInfo bwe;

  // Calculations done above per send/receive stream.
  bwe.actual_enc_bitrate = video_bitrate_sent;
  bwe.transmit_bitrate = total_bitrate_sent;
  bwe.retransmit_bitrate = nack_bitrate_sent;
  bwe.available_send_bandwidth = estimated_send_bandwidth;
  bwe.available_recv_bandwidth = estimated_recv_bandwidth;
  bwe.target_enc_bitrate = target_enc_bitrate;

  info->bw_estimations.push_back(bwe);

  return true;
}

bool WebRtcVideoMediaChannel::SetCapturer(uint32 ssrc,
                                          VideoCapturer* capturer) {
  ASSERT(ssrc != 0);
  if (!capturer) {
    return RemoveCapturer(ssrc);
  }
  WebRtcVideoChannelSendInfo* send_channel = GetSendChannel(ssrc);
  if (!send_channel) {
    return false;
  }
  VideoCapturer* old_capturer = send_channel->video_capturer();
  MaybeDisconnectCapturer(old_capturer);

  send_channel->set_video_capturer(capturer);
  MaybeConnectCapturer(capturer);
  if (!capturer->IsScreencast() && ratio_w_ != 0 && ratio_h_ != 0) {
    capturer->UpdateAspectRatio(ratio_w_, ratio_h_);
  }
  const int64 timestamp = send_channel->local_stream_info()->time_stamp();
  if (send_codec_) {
    QueueBlackFrame(ssrc, timestamp, send_codec_->maxFramerate);
  }
  return true;
}

bool WebRtcVideoMediaChannel::RequestIntraFrame() {
  // There is no API exposed to application to request a key frame
  // ViE does this internally when there are errors from decoder
  return false;
}

void WebRtcVideoMediaChannel::OnPacketReceived(
    talk_base::Buffer* packet, const talk_base::PacketTime& packet_time) {
  // Pick which channel to send this packet to. If this packet doesn't match
  // any multiplexed streams, just send it to the default channel. Otherwise,
  // send it to the specific decoder instance for that stream.
  uint32 ssrc = 0;
  if (!GetRtpSsrc(packet->data(), packet->length(), &ssrc))
    return;
  int which_channel = GetRecvChannelNum(ssrc);
  if (which_channel == -1) {
    which_channel = video_channel();
  }

  engine()->vie()->network()->ReceivedRTPPacket(
      which_channel,
      packet->data(),
      static_cast<int>(packet->length()),
      webrtc::PacketTime(packet_time.timestamp, packet_time.not_before));
}

void WebRtcVideoMediaChannel::OnRtcpReceived(
    talk_base::Buffer* packet, const talk_base::PacketTime& packet_time) {
// Sending channels need all RTCP packets with feedback information.
// Even sender reports can contain attached report blocks.
// Receiving channels need sender reports in order to create
// correct receiver reports.

  uint32 ssrc = 0;
  if (!GetRtcpSsrc(packet->data(), packet->length(), &ssrc)) {
    LOG(LS_WARNING) << "Failed to parse SSRC from received RTCP packet";
    return;
  }
  int type = 0;
  if (!GetRtcpType(packet->data(), packet->length(), &type)) {
    LOG(LS_WARNING) << "Failed to parse type from received RTCP packet";
    return;
  }

  // If it is a sender report, find the channel that is listening.
  if (type == kRtcpTypeSR) {
    int which_channel = GetRecvChannelNum(ssrc);
    if (which_channel != -1 && !IsDefaultChannel(which_channel)) {
      engine_->vie()->network()->ReceivedRTCPPacket(
          which_channel,
          packet->data(),
          static_cast<int>(packet->length()));
    }
  }
  // SR may continue RR and any RR entry may correspond to any one of the send
  // channels. So all RTCP packets must be forwarded all send channels. ViE
  // will filter out RR internally.
  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    WebRtcVideoChannelSendInfo* send_channel = iter->second;
    int channel_id = send_channel->channel_id();
    engine_->vie()->network()->ReceivedRTCPPacket(
        channel_id,
        packet->data(),
        static_cast<int>(packet->length()));
  }
}

void WebRtcVideoMediaChannel::OnReadyToSend(bool ready) {
  SetNetworkTransmissionState(ready);
}

bool WebRtcVideoMediaChannel::MuteStream(uint32 ssrc, bool muted) {
  WebRtcVideoChannelSendInfo* send_channel = GetSendChannel(ssrc);
  if (!send_channel) {
    LOG(LS_ERROR) << "The specified ssrc " << ssrc << " is not in use.";
    return false;
  }
  send_channel->set_muted(muted);
  return true;
}

bool WebRtcVideoMediaChannel::SetRecvRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  if (receive_extensions_ == extensions) {
    return true;
  }
  receive_extensions_ = extensions;

  const RtpHeaderExtension* offset_extension =
      FindHeaderExtension(extensions, kRtpTimestampOffsetHeaderExtension);
  const RtpHeaderExtension* send_time_extension =
      FindHeaderExtension(extensions, kRtpAbsoluteSendTimeHeaderExtension);

  // Loop through all receive channels and enable/disable the extensions.
  for (RecvChannelMap::iterator channel_it = recv_channels_.begin();
       channel_it != recv_channels_.end(); ++channel_it) {
    int channel_id = channel_it->second->channel_id();
    if (!SetHeaderExtension(
        &webrtc::ViERTP_RTCP::SetReceiveTimestampOffsetStatus, channel_id,
        offset_extension)) {
      return false;
    }
    if (!SetHeaderExtension(
        &webrtc::ViERTP_RTCP::SetReceiveAbsoluteSendTimeStatus, channel_id,
        send_time_extension)) {
      return false;
    }
  }
  return true;
}

bool WebRtcVideoMediaChannel::SetSendRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  send_extensions_ = extensions;

  const RtpHeaderExtension* offset_extension =
      FindHeaderExtension(extensions, kRtpTimestampOffsetHeaderExtension);
  const RtpHeaderExtension* send_time_extension =
      FindHeaderExtension(extensions, kRtpAbsoluteSendTimeHeaderExtension);

  // Loop through all send channels and enable/disable the extensions.
  for (SendChannelMap::iterator channel_it = send_channels_.begin();
       channel_it != send_channels_.end(); ++channel_it) {
    int channel_id = channel_it->second->channel_id();
    if (!SetHeaderExtension(
        &webrtc::ViERTP_RTCP::SetSendTimestampOffsetStatus, channel_id,
        offset_extension)) {
      return false;
    }
    if (!SetHeaderExtension(
        &webrtc::ViERTP_RTCP::SetSendAbsoluteSendTimeStatus, channel_id,
        send_time_extension)) {
      return false;
    }
  }

  if (send_time_extension) {
    // For video RTP packets, we would like to update AbsoluteSendTimeHeader
    // Extension closer to the network, @ socket level before sending.
    // Pushing the extension id to socket layer.
    MediaChannel::SetOption(NetworkInterface::ST_RTP,
                            talk_base::Socket::OPT_RTP_SENDTIME_EXTN_ID,
                            send_time_extension->id);
  }
  return true;
}

bool WebRtcVideoMediaChannel::SetStartSendBandwidth(int bps) {
  LOG(LS_INFO) << "WebRtcVideoMediaChannel::SetStartSendBandwidth";

  if (!send_codec_) {
    LOG(LS_INFO) << "The send codec has not been set up yet";
    return true;
  }

  // On success, SetSendCodec() will reset |send_start_bitrate_| to |bps/1000|,
  // by calling MaybeChangeStartBitrate.  That method will also clamp the
  // start bitrate between min and max, consistent with the override behavior
  // in SetMaxSendBandwidth.
  return SetSendCodec(*send_codec_,
      send_min_bitrate_, bps / 1000, send_max_bitrate_);
}

bool WebRtcVideoMediaChannel::SetMaxSendBandwidth(int bps) {
  LOG(LS_INFO) << "WebRtcVideoMediaChannel::SetMaxSendBandwidth";

  if (InConferenceMode()) {
    LOG(LS_INFO) << "Conference mode ignores SetMaxSendBandwidth";
    return true;
  }

  if (!send_codec_) {
    LOG(LS_INFO) << "The send codec has not been set up yet";
    return true;
  }

  // Use the default value or the bps for the max
  int max_bitrate = (bps <= 0) ? send_max_bitrate_ : (bps / 1000);

  // Reduce the current minimum and start bitrates if necessary.
  int min_bitrate = talk_base::_min(send_min_bitrate_, max_bitrate);
  int start_bitrate = talk_base::_min(send_start_bitrate_, max_bitrate);

  if (!SetSendCodec(*send_codec_, min_bitrate, start_bitrate, max_bitrate)) {
    return false;
  }
  LogSendCodecChange("SetMaxSendBandwidth()");

  return true;
}

bool WebRtcVideoMediaChannel::SetOptions(const VideoOptions &options) {
  // Always accept options that are unchanged.
  if (options_ == options) {
    return true;
  }

  // Trigger SetSendCodec to set correct noise reduction state if the option has
  // changed.
  bool denoiser_changed = options.video_noise_reduction.IsSet() &&
      (options_.video_noise_reduction != options.video_noise_reduction);

  bool leaky_bucket_changed = options.video_leaky_bucket.IsSet() &&
      (options_.video_leaky_bucket != options.video_leaky_bucket);

  bool buffer_latency_changed = options.buffered_mode_latency.IsSet() &&
      (options_.buffered_mode_latency != options.buffered_mode_latency);

  bool cpu_overuse_detection_changed = options.cpu_overuse_detection.IsSet() &&
      (options_.cpu_overuse_detection != options.cpu_overuse_detection);

  bool dscp_option_changed = (options_.dscp != options.dscp);

  bool suspend_below_min_bitrate_changed =
      options.suspend_below_min_bitrate.IsSet() &&
      (options_.suspend_below_min_bitrate != options.suspend_below_min_bitrate);

  bool conference_mode_turned_off = false;
  if (options_.conference_mode.IsSet() && options.conference_mode.IsSet() &&
      options_.conference_mode.GetWithDefaultIfUnset(false) &&
      !options.conference_mode.GetWithDefaultIfUnset(false)) {
    conference_mode_turned_off = true;
  }

  // Save the options, to be interpreted where appropriate.
  // Use options_.SetAll() instead of assignment so that unset value in options
  // will not overwrite the previous option value.
  options_.SetAll(options);

  // Set CPU options for all send channels.
  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    WebRtcVideoChannelSendInfo* send_channel = iter->second;
    send_channel->ApplyCpuOptions(options_);
  }

  // Adjust send codec bitrate if needed.
  int conf_max_bitrate = kDefaultConferenceModeMaxVideoBitrate;

  // Save altered min_bitrate level and apply if necessary.
  bool adjusted_min_bitrate = false;
  if (options.lower_min_bitrate.IsSet()) {
    bool lower;
    options.lower_min_bitrate.Get(&lower);

    int new_send_min_bitrate = lower ? kLowerMinBitrate : kMinVideoBitrate;
    adjusted_min_bitrate = (new_send_min_bitrate != send_min_bitrate_);
    send_min_bitrate_ = new_send_min_bitrate;
  }

  int expected_bitrate = send_max_bitrate_;
  if (InConferenceMode()) {
    expected_bitrate = conf_max_bitrate;
  } else if (conference_mode_turned_off) {
    // This is a special case for turning conference mode off.
    // Max bitrate should go back to the default maximum value instead
    // of the current maximum.
    expected_bitrate = kMaxVideoBitrate;
  }

  if (send_codec_ &&
      (send_max_bitrate_ != expected_bitrate || denoiser_changed ||
       adjusted_min_bitrate)) {
    // On success, SetSendCodec() will reset send_max_bitrate_ to
    // expected_bitrate.
    if (!SetSendCodec(*send_codec_,
                      send_min_bitrate_,
                      send_start_bitrate_,
                      expected_bitrate)) {
      return false;
    }
    LogSendCodecChange("SetOptions()");
  }
  if (leaky_bucket_changed) {
    bool enable_leaky_bucket =
        options_.video_leaky_bucket.GetWithDefaultIfUnset(false);
    for (SendChannelMap::iterator it = send_channels_.begin();
        it != send_channels_.end(); ++it) {
      if (engine()->vie()->rtp()->SetTransmissionSmoothingStatus(
          it->second->channel_id(), enable_leaky_bucket) != 0) {
        LOG_RTCERR2(SetTransmissionSmoothingStatus, it->second->channel_id(),
                    enable_leaky_bucket);
      }
    }
  }
  if (buffer_latency_changed) {
    int buffer_latency =
        options_.buffered_mode_latency.GetWithDefaultIfUnset(
            cricket::kBufferedModeDisabled);
    for (SendChannelMap::iterator it = send_channels_.begin();
        it != send_channels_.end(); ++it) {
      if (engine()->vie()->rtp()->SetSenderBufferingMode(
          it->second->channel_id(), buffer_latency) != 0) {
        LOG_RTCERR2(SetSenderBufferingMode, it->second->channel_id(),
                    buffer_latency);
      }
    }
    for (RecvChannelMap::iterator it = recv_channels_.begin();
        it != recv_channels_.end(); ++it) {
      if (engine()->vie()->rtp()->SetReceiverBufferingMode(
          it->second->channel_id(), buffer_latency) != 0) {
        LOG_RTCERR2(SetReceiverBufferingMode, it->second->channel_id(),
                    buffer_latency);
      }
    }
  }
  if (cpu_overuse_detection_changed) {
    bool cpu_overuse_detection =
        options_.cpu_overuse_detection.GetWithDefaultIfUnset(false);
    for (SendChannelMap::iterator iter = send_channels_.begin();
         iter != send_channels_.end(); ++iter) {
      WebRtcVideoChannelSendInfo* send_channel = iter->second;
      send_channel->SetCpuOveruseDetection(cpu_overuse_detection);
    }
  }
  if (dscp_option_changed) {
    talk_base::DiffServCodePoint dscp = talk_base::DSCP_DEFAULT;
    if (options_.dscp.GetWithDefaultIfUnset(false))
      dscp = kVideoDscpValue;
    if (MediaChannel::SetDscp(dscp) != 0) {
      LOG(LS_WARNING) << "Failed to set DSCP settings for video channel";
    }
  }
  if (suspend_below_min_bitrate_changed) {
    if (options_.suspend_below_min_bitrate.GetWithDefaultIfUnset(false)) {
      for (SendChannelMap::iterator it = send_channels_.begin();
           it != send_channels_.end(); ++it) {
        engine()->vie()->codec()->SuspendBelowMinBitrate(
            it->second->channel_id());
      }
    } else {
      LOG(LS_WARNING) << "Cannot disable video suspension once it is enabled";
    }
  }
  return true;
}

void WebRtcVideoMediaChannel::SetInterface(NetworkInterface* iface) {
  MediaChannel::SetInterface(iface);
  // Set the RTP recv/send buffer to a bigger size
  MediaChannel::SetOption(NetworkInterface::ST_RTP,
                          talk_base::Socket::OPT_RCVBUF,
                          kVideoRtpBufferSize);

    // TODO(sriniv): Remove or re-enable this.
    // As part of b/8030474, send-buffer is size now controlled through
    // portallocator flags.
    // network_interface_->SetOption(NetworkInterface::ST_RTP,
    //                              talk_base::Socket::OPT_SNDBUF,
    //                              kVideoRtpBufferSize);
}

void WebRtcVideoMediaChannel::UpdateAspectRatio(int ratio_w, int ratio_h) {
  ASSERT(ratio_w != 0);
  ASSERT(ratio_h != 0);
  ratio_w_ = ratio_w;
  ratio_h_ = ratio_h;
  // For now assume that all streams want the same aspect ratio.
  // TODO(hellner): remove the need for this assumption.
  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    WebRtcVideoChannelSendInfo* send_channel = iter->second;
    VideoCapturer* capturer = send_channel->video_capturer();
    if (capturer) {
      capturer->UpdateAspectRatio(ratio_w, ratio_h);
    }
  }
}

bool WebRtcVideoMediaChannel::GetRenderer(uint32 ssrc,
                                          VideoRenderer** renderer) {
  RecvChannelMap::const_iterator it = recv_channels_.find(ssrc);
  if (it == recv_channels_.end()) {
    if (first_receive_ssrc_ == ssrc &&
        recv_channels_.find(0) != recv_channels_.end()) {
      LOG(LS_INFO) << " GetRenderer " << ssrc
                   << " reuse default renderer #"
                   << vie_channel_;
      *renderer = recv_channels_[0]->render_adapter()->renderer();
      return true;
    }
    return false;
  }

  *renderer = it->second->render_adapter()->renderer();
  return true;
}

bool WebRtcVideoMediaChannel::GetVideoAdapter(
    uint32 ssrc, CoordinatedVideoAdapter** video_adapter) {
  SendChannelMap::iterator it = send_channels_.find(ssrc);
  if (it == send_channels_.end()) {
    return false;
  }
  *video_adapter = it->second->video_adapter();
  return true;
}

void WebRtcVideoMediaChannel::SendFrame(VideoCapturer* capturer,
                                        const VideoFrame* frame) {
  // If the |capturer| is registered to any send channel, then send the frame
  // to those send channels.
  bool capturer_is_channel_owned = false;
  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    WebRtcVideoChannelSendInfo* send_channel = iter->second;
    if (send_channel->video_capturer() == capturer) {
      SendFrame(send_channel, frame, capturer->IsScreencast());
      capturer_is_channel_owned = true;
    }
  }
  if (capturer_is_channel_owned) {
    return;
  }

  // TODO(hellner): Remove below for loop once the captured frame no longer
  // come from the engine, i.e. the engine no longer owns a capturer.
  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    WebRtcVideoChannelSendInfo* send_channel = iter->second;
    if (send_channel->video_capturer() == NULL) {
      SendFrame(send_channel, frame, capturer->IsScreencast());
    }
  }
}

bool WebRtcVideoMediaChannel::SendFrame(
    WebRtcVideoChannelSendInfo* send_channel,
    const VideoFrame* frame,
    bool is_screencast) {
  if (!send_channel) {
    return false;
  }
  if (!send_codec_) {
    // Send codec has not been set. No reason to process the frame any further.
    return false;
  }
  const VideoFormat& video_format = send_channel->video_format();
  // If the frame should be dropped.
  const bool video_format_set = video_format != cricket::VideoFormat();
  if (video_format_set &&
      (video_format.width == 0 && video_format.height == 0)) {
    return true;
  }

  // Checks if we need to reset vie send codec.
  if (!MaybeResetVieSendCodec(send_channel,
                              static_cast<int>(frame->GetWidth()),
                              static_cast<int>(frame->GetHeight()),
                              is_screencast, NULL)) {
    LOG(LS_ERROR) << "MaybeResetVieSendCodec failed with "
                  << frame->GetWidth() << "x" << frame->GetHeight();
    return false;
  }
  const VideoFrame* frame_out = frame;
  talk_base::scoped_ptr<VideoFrame> processed_frame;
  // Disable muting for screencast.
  const bool mute = (send_channel->muted() && !is_screencast);
  send_channel->ProcessFrame(*frame_out, mute, processed_frame.use());
  if (processed_frame) {
    frame_out = processed_frame.get();
  }

  webrtc::ViEVideoFrameI420 frame_i420;
  // TODO(ronghuawu): Update the webrtc::ViEVideoFrameI420
  // to use const unsigned char*
  frame_i420.y_plane = const_cast<unsigned char*>(frame_out->GetYPlane());
  frame_i420.u_plane = const_cast<unsigned char*>(frame_out->GetUPlane());
  frame_i420.v_plane = const_cast<unsigned char*>(frame_out->GetVPlane());
  frame_i420.y_pitch = frame_out->GetYPitch();
  frame_i420.u_pitch = frame_out->GetUPitch();
  frame_i420.v_pitch = frame_out->GetVPitch();
  frame_i420.width = static_cast<uint16>(frame_out->GetWidth());
  frame_i420.height = static_cast<uint16>(frame_out->GetHeight());

  int64 timestamp_ntp_ms = 0;
  // TODO(justinlin): Reenable after Windows issues with clock drift are fixed.
  // Currently reverted to old behavior of discarding capture timestamp.
#if 0
  // If the frame timestamp is 0, we will use the deliver time.
  const int64 frame_timestamp = frame->GetTimeStamp();
  if (frame_timestamp != 0) {
    if (abs(time(NULL) - frame_timestamp / talk_base::kNumNanosecsPerSec) >
            kTimestampDeltaInSecondsForWarning) {
      LOG(LS_WARNING) << "Frame timestamp differs by more than "
                      << kTimestampDeltaInSecondsForWarning << " seconds from "
                      << "current Unix timestamp.";
    }

    timestamp_ntp_ms =
        talk_base::UnixTimestampNanosecsToNtpMillisecs(frame_timestamp);
  }
#endif

  return send_channel->external_capture()->IncomingFrameI420(
      frame_i420, timestamp_ntp_ms) == 0;
}

bool WebRtcVideoMediaChannel::CreateChannel(uint32 ssrc_key,
                                            MediaDirection direction,
                                            int* channel_id) {
  // There are 3 types of channels. Sending only, receiving only and
  // sending and receiving. The sending and receiving channel is the
  // default channel and there is only one. All other channels that are created
  // are associated with the default channel which must exist. The default
  // channel id is stored in |vie_channel_|. All channels need to know about
  // the default channel to properly handle remb which is why there are
  // different ViE create channel calls.
  // For this channel the local and remote ssrc key is 0. However, it may
  // have a non-zero local and/or remote ssrc depending on if it is currently
  // sending and/or receiving.
  if ((vie_channel_ == -1 || direction == MD_SENDRECV) &&
      (!send_channels_.empty() || !recv_channels_.empty())) {
    ASSERT(false);
    return false;
  }

  *channel_id = -1;
  if (direction == MD_RECV) {
    // All rec channels are associated with the default channel |vie_channel_|
    if (engine_->vie()->base()->CreateReceiveChannel(*channel_id,
                                                     vie_channel_) != 0) {
      LOG_RTCERR2(CreateReceiveChannel, *channel_id, vie_channel_);
      return false;
    }
  } else if (direction == MD_SEND) {
    if (engine_->vie()->base()->CreateChannel(*channel_id,
                                              vie_channel_) != 0) {
      LOG_RTCERR2(CreateChannel, *channel_id, vie_channel_);
      return false;
    }
  } else {
    ASSERT(direction == MD_SENDRECV);
    if (engine_->vie()->base()->CreateChannel(*channel_id) != 0) {
      LOG_RTCERR1(CreateChannel, *channel_id);
      return false;
    }
  }
  if (!ConfigureChannel(*channel_id, direction, ssrc_key)) {
    engine_->vie()->base()->DeleteChannel(*channel_id);
    *channel_id = -1;
    return false;
  }

  return true;
}

bool WebRtcVideoMediaChannel::ConfigureChannel(int channel_id,
                                               MediaDirection direction,
                                               uint32 ssrc_key) {
  const bool receiving = (direction == MD_RECV) || (direction == MD_SENDRECV);
  const bool sending = (direction == MD_SEND) || (direction == MD_SENDRECV);
  // Register external transport.
  if (engine_->vie()->network()->RegisterSendTransport(
      channel_id, *this) != 0) {
    LOG_RTCERR1(RegisterSendTransport, channel_id);
    return false;
  }

  // Set MTU.
  if (engine_->vie()->network()->SetMTU(channel_id, kVideoMtu) != 0) {
    LOG_RTCERR2(SetMTU, channel_id, kVideoMtu);
    return false;
  }
  // Turn on RTCP and loss feedback reporting.
  if (engine()->vie()->rtp()->SetRTCPStatus(
      channel_id, webrtc::kRtcpCompound_RFC4585) != 0) {
    LOG_RTCERR2(SetRTCPStatus, channel_id, webrtc::kRtcpCompound_RFC4585);
    return false;
  }
  // Enable pli as key frame request method.
  if (engine_->vie()->rtp()->SetKeyFrameRequestMethod(
      channel_id, webrtc::kViEKeyFrameRequestPliRtcp) != 0) {
    LOG_RTCERR2(SetKeyFrameRequestMethod,
                channel_id, webrtc::kViEKeyFrameRequestPliRtcp);
    return false;
  }
  if (!SetNackFec(channel_id, send_red_type_, send_fec_type_, nack_enabled_)) {
    // Logged in SetNackFec. Don't spam the logs.
    return false;
  }
  // Note that receiving must always be configured before sending to ensure
  // that send and receive channel is configured correctly (ConfigureReceiving
  // assumes no sending).
  if (receiving) {
    if (!ConfigureReceiving(channel_id, ssrc_key)) {
      return false;
    }
  }
  if (sending) {
    if (!ConfigureSending(channel_id, ssrc_key)) {
      return false;
    }
  }

  return true;
}

bool WebRtcVideoMediaChannel::ConfigureReceiving(int channel_id,
                                                 uint32 remote_ssrc_key) {
  // Make sure that an SSRC/key isn't registered more than once.
  if (recv_channels_.find(remote_ssrc_key) != recv_channels_.end()) {
    return false;
  }
  // Connect the voice channel, if there is one.
  // TODO(perkj): The A/V is synched by the receiving channel. So we need to
  // know the SSRC of the remote audio channel in order to fetch the correct
  // webrtc VoiceEngine channel. For now- only sync the default channel used
  // in 1-1 calls.
  if (remote_ssrc_key == 0 && voice_channel_) {
    WebRtcVoiceMediaChannel* voice_channel =
        static_cast<WebRtcVoiceMediaChannel*>(voice_channel_);
    if (engine_->vie()->base()->ConnectAudioChannel(
        vie_channel_, voice_channel->voe_channel()) != 0) {
      LOG_RTCERR2(ConnectAudioChannel, channel_id,
                  voice_channel->voe_channel());
      LOG(LS_WARNING) << "A/V not synchronized";
      // Not a fatal error.
    }
  }

  talk_base::scoped_ptr<WebRtcVideoChannelRecvInfo> channel_info(
      new WebRtcVideoChannelRecvInfo(channel_id));

  // Install a render adapter.
  if (engine_->vie()->render()->AddRenderer(channel_id,
      webrtc::kVideoI420, channel_info->render_adapter()) != 0) {
    LOG_RTCERR3(AddRenderer, channel_id, webrtc::kVideoI420,
                channel_info->render_adapter());
    return false;
  }


  if (engine_->vie()->rtp()->SetRembStatus(channel_id,
                                           kNotSending,
                                           remb_enabled_) != 0) {
    LOG_RTCERR3(SetRembStatus, channel_id, kNotSending, remb_enabled_);
    return false;
  }

  if (!SetHeaderExtension(&webrtc::ViERTP_RTCP::SetReceiveTimestampOffsetStatus,
      channel_id, receive_extensions_, kRtpTimestampOffsetHeaderExtension)) {
    return false;
  }

  if (!SetHeaderExtension(
      &webrtc::ViERTP_RTCP::SetReceiveAbsoluteSendTimeStatus, channel_id,
      receive_extensions_, kRtpAbsoluteSendTimeHeaderExtension)) {
    return false;
  }

  if (remote_ssrc_key != 0) {
    // Use the same SSRC as our default channel
    // (so the RTCP reports are correct).
    unsigned int send_ssrc = 0;
    webrtc::ViERTP_RTCP* rtp = engine()->vie()->rtp();
    if (rtp->GetLocalSSRC(vie_channel_, send_ssrc) == -1) {
      LOG_RTCERR2(GetLocalSSRC, vie_channel_, send_ssrc);
      return false;
    }
    if (rtp->SetLocalSSRC(channel_id, send_ssrc) == -1) {
      LOG_RTCERR2(SetLocalSSRC, channel_id, send_ssrc);
      return false;
    }
  }  // Else this is the the default channel and we don't change the SSRC.

  // Disable color enhancement since it is a bit too aggressive.
  if (engine()->vie()->image()->EnableColorEnhancement(channel_id,
                                                       false) != 0) {
    LOG_RTCERR1(EnableColorEnhancement, channel_id);
    return false;
  }

  if (!SetReceiveCodecs(channel_info.get())) {
    return false;
  }

  int buffer_latency =
      options_.buffered_mode_latency.GetWithDefaultIfUnset(
          cricket::kBufferedModeDisabled);
  if (buffer_latency != cricket::kBufferedModeDisabled) {
    if (engine()->vie()->rtp()->SetReceiverBufferingMode(
        channel_id, buffer_latency) != 0) {
      LOG_RTCERR2(SetReceiverBufferingMode, channel_id, buffer_latency);
    }
  }

  if (render_started_) {
    if (engine_->vie()->render()->StartRender(channel_id) != 0) {
      LOG_RTCERR1(StartRender, channel_id);
      return false;
    }
  }

  // Register decoder observer for incoming framerate and bitrate.
  if (engine()->vie()->codec()->RegisterDecoderObserver(
      channel_id, *channel_info->decoder_observer()) != 0) {
    LOG_RTCERR1(RegisterDecoderObserver, channel_info->decoder_observer());
    return false;
  }

  recv_channels_[remote_ssrc_key] = channel_info.release();
  return true;
}

bool WebRtcVideoMediaChannel::ConfigureSending(int channel_id,
                                               uint32 local_ssrc_key) {
  // The ssrc key can be zero or correspond to an SSRC.
  // Make sure the default channel isn't configured more than once.
  if (local_ssrc_key == 0 && send_channels_.find(0) != send_channels_.end()) {
    return false;
  }
  // Make sure that the SSRC is not already in use.
  uint32 dummy_key;
  if (GetSendChannelKey(local_ssrc_key, &dummy_key)) {
    return false;
  }
  int vie_capture = 0;
  webrtc::ViEExternalCapture* external_capture = NULL;
  // Register external capture.
  if (engine()->vie()->capture()->AllocateExternalCaptureDevice(
      vie_capture, external_capture) != 0) {
    LOG_RTCERR0(AllocateExternalCaptureDevice);
    return false;
  }

  // Connect external capture.
  if (engine()->vie()->capture()->ConnectCaptureDevice(
      vie_capture, channel_id) != 0) {
    LOG_RTCERR2(ConnectCaptureDevice, vie_capture, channel_id);
    return false;
  }
  talk_base::scoped_ptr<WebRtcVideoChannelSendInfo> send_channel(
      new WebRtcVideoChannelSendInfo(channel_id, vie_capture,
                                     external_capture,
                                     engine()->cpu_monitor()));
  if (engine()->vie()->base()->RegisterCpuOveruseObserver(
      channel_id, send_channel->overuse_observer())) {
    LOG_RTCERR1(RegisterCpuOveruseObserver, channel_id);
    return false;
  }
  send_channel->ApplyCpuOptions(options_);
  send_channel->SignalCpuAdaptationUnable.connect(this,
      &WebRtcVideoMediaChannel::OnCpuAdaptationUnable);

  if (options_.cpu_overuse_detection.GetWithDefaultIfUnset(false)) {
    send_channel->SetCpuOveruseDetection(true);
  }

  // Register encoder observer for outgoing framerate and bitrate.
  if (engine()->vie()->codec()->RegisterEncoderObserver(
      channel_id, *send_channel->encoder_observer()) != 0) {
    LOG_RTCERR1(RegisterEncoderObserver, send_channel->encoder_observer());
    return false;
  }

  if (!SetHeaderExtension(&webrtc::ViERTP_RTCP::SetSendTimestampOffsetStatus,
      channel_id, send_extensions_, kRtpTimestampOffsetHeaderExtension)) {
    return false;
  }

  if (!SetHeaderExtension(&webrtc::ViERTP_RTCP::SetSendAbsoluteSendTimeStatus,
      channel_id, send_extensions_, kRtpAbsoluteSendTimeHeaderExtension)) {
    return false;
  }

  if (options_.video_leaky_bucket.GetWithDefaultIfUnset(false)) {
    if (engine()->vie()->rtp()->SetTransmissionSmoothingStatus(channel_id,
                                                               true) != 0) {
      LOG_RTCERR2(SetTransmissionSmoothingStatus, channel_id, true);
      return false;
    }
  }

  int buffer_latency =
      options_.buffered_mode_latency.GetWithDefaultIfUnset(
          cricket::kBufferedModeDisabled);
  if (buffer_latency != cricket::kBufferedModeDisabled) {
    if (engine()->vie()->rtp()->SetSenderBufferingMode(
        channel_id, buffer_latency) != 0) {
      LOG_RTCERR2(SetSenderBufferingMode, channel_id, buffer_latency);
    }
  }
  // The remb status direction correspond to the RTP stream (and not the RTCP
  // stream). I.e. if send remb is enabled it means it is receiving remote
  // rembs and should use them to estimate bandwidth. Receive remb mean that
  // remb packets will be generated and that the channel should be included in
  // it. If remb is enabled all channels are allowed to contribute to the remb
  // but only receive channels will ever end up actually contributing. This
  // keeps the logic simple.
  if (engine_->vie()->rtp()->SetRembStatus(channel_id,
                                           remb_enabled_,
                                           remb_enabled_) != 0) {
    LOG_RTCERR3(SetRembStatus, channel_id, remb_enabled_, remb_enabled_);
    return false;
  }
  if (!SetNackFec(channel_id, send_red_type_, send_fec_type_, nack_enabled_)) {
    // Logged in SetNackFec. Don't spam the logs.
    return false;
  }

  send_channels_[local_ssrc_key] = send_channel.release();

  return true;
}

bool WebRtcVideoMediaChannel::SetNackFec(int channel_id,
                                         int red_payload_type,
                                         int fec_payload_type,
                                         bool nack_enabled) {
  bool enable = (red_payload_type != -1 && fec_payload_type != -1 &&
      !InConferenceMode());
  if (enable) {
    if (engine_->vie()->rtp()->SetHybridNACKFECStatus(
        channel_id, nack_enabled, red_payload_type, fec_payload_type) != 0) {
      LOG_RTCERR4(SetHybridNACKFECStatus,
                  channel_id, nack_enabled, red_payload_type, fec_payload_type);
      return false;
    }
    LOG(LS_INFO) << "Hybrid NACK/FEC enabled for channel " << channel_id;
  } else {
    if (engine_->vie()->rtp()->SetNACKStatus(channel_id, nack_enabled) != 0) {
      LOG_RTCERR1(SetNACKStatus, channel_id);
      return false;
    }
    std::string enabled = nack_enabled ? "enabled" : "disabled";
    LOG(LS_INFO) << "NACK " << enabled << " for channel " << channel_id;
  }
  return true;
}

bool WebRtcVideoMediaChannel::SetSendCodec(const webrtc::VideoCodec& codec,
                                           int min_bitrate,
                                           int start_bitrate,
                                           int max_bitrate) {
  bool ret_val = true;
  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    WebRtcVideoChannelSendInfo* send_channel = iter->second;
    ret_val = SetSendCodec(send_channel, codec, min_bitrate, start_bitrate,
                           max_bitrate) && ret_val;
  }
  if (ret_val) {
    // All SetSendCodec calls were successful. Update the global state
    // accordingly.
    send_codec_.reset(new webrtc::VideoCodec(codec));
    send_min_bitrate_ = min_bitrate;
    send_start_bitrate_ = start_bitrate;
    send_max_bitrate_ = max_bitrate;
  } else {
    // At least one SetSendCodec call failed, rollback.
    for (SendChannelMap::iterator iter = send_channels_.begin();
         iter != send_channels_.end(); ++iter) {
      WebRtcVideoChannelSendInfo* send_channel = iter->second;
      if (send_codec_) {
        SetSendCodec(send_channel, *send_codec_.get(), send_min_bitrate_,
                     send_start_bitrate_, send_max_bitrate_);
      }
    }
  }
  return ret_val;
}

bool WebRtcVideoMediaChannel::SetSendCodec(
    WebRtcVideoChannelSendInfo* send_channel,
    const webrtc::VideoCodec& codec,
    int min_bitrate,
    int start_bitrate,
    int max_bitrate) {
  if (!send_channel) {
    return false;
  }
  const int channel_id = send_channel->channel_id();
  // Make a copy of the codec
  webrtc::VideoCodec target_codec = codec;
  target_codec.startBitrate = start_bitrate;
  target_codec.minBitrate = min_bitrate;
  target_codec.maxBitrate = max_bitrate;

  // Set the default number of temporal layers for VP8.
  if (webrtc::kVideoCodecVP8 == codec.codecType) {
    target_codec.codecSpecific.VP8.numberOfTemporalLayers =
        kDefaultNumberOfTemporalLayers;

    // Turn off the VP8 error resilience
    target_codec.codecSpecific.VP8.resilience = webrtc::kResilienceOff;

    bool enable_denoising =
        options_.video_noise_reduction.GetWithDefaultIfUnset(false);
    target_codec.codecSpecific.VP8.denoisingOn = enable_denoising;
  }

  // Register external encoder if codec type is supported by encoder factory.
  if (engine()->IsExternalEncoderCodecType(codec.codecType) &&
      !send_channel->IsEncoderRegistered(target_codec.plType)) {
    webrtc::VideoEncoder* encoder =
        engine()->CreateExternalEncoder(codec.codecType);
    if (encoder) {
      if (engine()->vie()->ext_codec()->RegisterExternalSendCodec(
          channel_id, target_codec.plType, encoder, false) == 0) {
        send_channel->RegisterEncoder(target_codec.plType, encoder);
      } else {
        LOG_RTCERR2(RegisterExternalSendCodec, channel_id, target_codec.plName);
        engine()->DestroyExternalEncoder(encoder);
      }
    }
  }

  // Resolution and framerate may vary for different send channels.
  const VideoFormat& video_format = send_channel->video_format();
  UpdateVideoCodec(video_format, &target_codec);

  if (target_codec.width == 0 && target_codec.height == 0) {
    const uint32 ssrc = send_channel->stream_params()->first_ssrc();
    LOG(LS_INFO) << "0x0 resolution selected. Captured frames will be dropped "
                 << "for ssrc: " << ssrc << ".";
  } else {
    MaybeChangeStartBitrate(channel_id, &target_codec);
    if (0 != engine()->vie()->codec()->SetSendCodec(channel_id, target_codec)) {
      LOG_RTCERR2(SetSendCodec, channel_id, target_codec.plName);
      return false;
    }

    // NOTE: SetRtxSendPayloadType must be called after all simulcast SSRCs
    // are configured. Otherwise ssrc's configured after this point will use
    // the primary PT for RTX.
    if (send_rtx_type_ != -1 &&
        engine()->vie()->rtp()->SetRtxSendPayloadType(channel_id,
                                                      send_rtx_type_) != 0) {
        LOG_RTCERR2(SetRtxSendPayloadType, channel_id, send_rtx_type_);
        return false;
    }
  }
  send_channel->set_interval(
      cricket::VideoFormat::FpsToInterval(target_codec.maxFramerate));
  return true;
}


static std::string ToString(webrtc::VideoCodecComplexity complexity) {
  switch (complexity) {
    case webrtc::kComplexityNormal:
      return "normal";
    case webrtc::kComplexityHigh:
      return "high";
    case webrtc::kComplexityHigher:
      return "higher";
    case webrtc::kComplexityMax:
      return "max";
    default:
      return "unknown";
  }
}

static std::string ToString(webrtc::VP8ResilienceMode resilience) {
  switch (resilience) {
    case webrtc::kResilienceOff:
      return "off";
    case webrtc::kResilientStream:
      return "stream";
    case webrtc::kResilientFrames:
      return "frames";
    default:
      return "unknown";
  }
}

void WebRtcVideoMediaChannel::LogSendCodecChange(const std::string& reason) {
  webrtc::VideoCodec vie_codec;
  if (engine()->vie()->codec()->GetSendCodec(vie_channel_, vie_codec) != 0) {
    LOG_RTCERR1(GetSendCodec, vie_channel_);
    return;
  }

  LOG(LS_INFO) << reason << " : selected video codec "
               << vie_codec.plName << "/"
               << vie_codec.width << "x" << vie_codec.height << "x"
               << static_cast<int>(vie_codec.maxFramerate) << "fps"
               << "@" << vie_codec.maxBitrate << "kbps"
               << " (min=" << vie_codec.minBitrate << "kbps,"
               << " start=" << vie_codec.startBitrate << "kbps)";
  LOG(LS_INFO) << "Video max quantization: " << vie_codec.qpMax;
  if (webrtc::kVideoCodecVP8 == vie_codec.codecType) {
    LOG(LS_INFO) << "VP8 number of temporal layers: "
                 << static_cast<int>(
                     vie_codec.codecSpecific.VP8.numberOfTemporalLayers);
    LOG(LS_INFO) << "VP8 options : "
                 << "picture loss indication = "
                 << vie_codec.codecSpecific.VP8.pictureLossIndicationOn
                 << ", feedback mode = "
                 << vie_codec.codecSpecific.VP8.feedbackModeOn
                 << ", complexity = "
                 << ToString(vie_codec.codecSpecific.VP8.complexity)
                 << ", resilience = "
                 << ToString(vie_codec.codecSpecific.VP8.resilience)
                 << ", denoising = "
                 << vie_codec.codecSpecific.VP8.denoisingOn
                 << ", error concealment = "
                 << vie_codec.codecSpecific.VP8.errorConcealmentOn
                 << ", automatic resize = "
                 << vie_codec.codecSpecific.VP8.automaticResizeOn
                 << ", frame dropping = "
                 << vie_codec.codecSpecific.VP8.frameDroppingOn
                 << ", key frame interval = "
                 << vie_codec.codecSpecific.VP8.keyFrameInterval;
  }

  if (send_rtx_type_ != -1) {
    LOG(LS_INFO) << "RTX payload type: " << send_rtx_type_;
  }
}

bool WebRtcVideoMediaChannel::SetReceiveCodecs(
    WebRtcVideoChannelRecvInfo* info) {
  int red_type = -1;
  int fec_type = -1;
  int channel_id = info->channel_id();
  for (std::vector<webrtc::VideoCodec>::iterator it = receive_codecs_.begin();
       it != receive_codecs_.end(); ++it) {
    if (it->codecType == webrtc::kVideoCodecRED) {
      red_type = it->plType;
    } else if (it->codecType == webrtc::kVideoCodecULPFEC) {
      fec_type = it->plType;
    }
    if (engine()->vie()->codec()->SetReceiveCodec(channel_id, *it) != 0) {
      LOG_RTCERR2(SetReceiveCodec, channel_id, it->plName);
      return false;
    }
    if (!info->IsDecoderRegistered(it->plType) &&
        it->codecType != webrtc::kVideoCodecRED &&
        it->codecType != webrtc::kVideoCodecULPFEC) {
      webrtc::VideoDecoder* decoder =
          engine()->CreateExternalDecoder(it->codecType);
      if (decoder) {
        if (engine()->vie()->ext_codec()->RegisterExternalReceiveCodec(
            channel_id, it->plType, decoder) == 0) {
          info->RegisterDecoder(it->plType, decoder);
        } else {
          LOG_RTCERR2(RegisterExternalReceiveCodec, channel_id, it->plName);
          engine()->DestroyExternalDecoder(decoder);
        }
      }
    }
  }

  // Start receiving packets if at least one receive codec has been set.
  if (!receive_codecs_.empty()) {
    if (engine()->vie()->base()->StartReceive(channel_id) != 0) {
      LOG_RTCERR1(StartReceive, channel_id);
      return false;
    }
  }
  return true;
}

int WebRtcVideoMediaChannel::GetRecvChannelNum(uint32 ssrc) {
  if (ssrc == first_receive_ssrc_) {
    return vie_channel_;
  }
  RecvChannelMap::iterator it = recv_channels_.find(ssrc);
  return (it != recv_channels_.end()) ? it->second->channel_id() : -1;
}

// If the new frame size is different from the send codec size we set on vie,
// we need to reset the send codec on vie.
// The new send codec size should not exceed send_codec_ which is controlled
// only by the 'jec' logic.
bool WebRtcVideoMediaChannel::MaybeResetVieSendCodec(
    WebRtcVideoChannelSendInfo* send_channel,
    int new_width,
    int new_height,
    bool is_screencast,
    bool* reset) {
  if (reset) {
    *reset = false;
  }
  ASSERT(send_codec_.get() != NULL);

  webrtc::VideoCodec target_codec = *send_codec_.get();
  const VideoFormat& video_format = send_channel->video_format();
  UpdateVideoCodec(video_format, &target_codec);

  // Vie send codec size should not exceed target_codec.
  int target_width = new_width;
  int target_height = new_height;
  if (!is_screencast &&
      (new_width > target_codec.width || new_height > target_codec.height)) {
    target_width = target_codec.width;
    target_height = target_codec.height;
  }

  // Get current vie codec.
  webrtc::VideoCodec vie_codec;
  const int channel_id = send_channel->channel_id();
  if (engine()->vie()->codec()->GetSendCodec(channel_id, vie_codec) != 0) {
    LOG_RTCERR1(GetSendCodec, channel_id);
    return false;
  }
  const int cur_width = vie_codec.width;
  const int cur_height = vie_codec.height;

  // Only reset send codec when there is a size change. Additionally,
  // automatic resize needs to be turned off when screencasting and on when
  // not screencasting.
  // Don't allow automatic resizing for screencasting.
  bool automatic_resize = !is_screencast;
  // Turn off VP8 frame dropping when screensharing as the current model does
  // not work well at low fps.
  bool vp8_frame_dropping = !is_screencast;
  // Disable denoising for screencasting.
  bool enable_denoising =
      options_.video_noise_reduction.GetWithDefaultIfUnset(false);
  bool denoising = !is_screencast && enable_denoising;
  bool reset_send_codec =
      target_width != cur_width || target_height != cur_height ||
      automatic_resize != vie_codec.codecSpecific.VP8.automaticResizeOn ||
      denoising != vie_codec.codecSpecific.VP8.denoisingOn ||
      vp8_frame_dropping != vie_codec.codecSpecific.VP8.frameDroppingOn;

  if (reset_send_codec) {
    // Set the new codec on vie.
    vie_codec.width = target_width;
    vie_codec.height = target_height;
    vie_codec.maxFramerate = target_codec.maxFramerate;
    vie_codec.startBitrate = target_codec.startBitrate;
    vie_codec.codecSpecific.VP8.automaticResizeOn = automatic_resize;
    vie_codec.codecSpecific.VP8.denoisingOn = denoising;
    vie_codec.codecSpecific.VP8.frameDroppingOn = vp8_frame_dropping;
    // TODO(mflodman): Remove 'is_screencast' check when screen cast settings
    // are treated correctly in WebRTC.
    if (!is_screencast)
      MaybeChangeStartBitrate(channel_id, &vie_codec);

    if (engine()->vie()->codec()->SetSendCodec(channel_id, vie_codec) != 0) {
      LOG_RTCERR1(SetSendCodec, channel_id);
      return false;
    }
    if (reset) {
      *reset = true;
    }
    LogSendCodecChange("Capture size changed");
  }

  return true;
}

void WebRtcVideoMediaChannel::MaybeChangeStartBitrate(
  int channel_id, webrtc::VideoCodec* video_codec) {
  if (video_codec->startBitrate < video_codec->minBitrate) {
    video_codec->startBitrate = video_codec->minBitrate;
  } else if (video_codec->startBitrate > video_codec->maxBitrate) {
    video_codec->startBitrate = video_codec->maxBitrate;
  }

  // Use a previous target bitrate, if there is one.
  unsigned int current_target_bitrate = 0;
  if (engine()->vie()->codec()->GetCodecTargetBitrate(
      channel_id, &current_target_bitrate) == 0) {
    // Convert to kbps.
    current_target_bitrate /= 1000;
    if (current_target_bitrate > video_codec->maxBitrate) {
      current_target_bitrate = video_codec->maxBitrate;
    }
    if (current_target_bitrate > video_codec->startBitrate) {
      video_codec->startBitrate = current_target_bitrate;
    }
  }
}

void WebRtcVideoMediaChannel::OnMessage(talk_base::Message* msg) {
  FlushBlackFrameData* black_frame_data =
      static_cast<FlushBlackFrameData*>(msg->pdata);
  FlushBlackFrame(black_frame_data->ssrc, black_frame_data->timestamp);
  delete black_frame_data;
}

int WebRtcVideoMediaChannel::SendPacket(int channel, const void* data,
                                        int len) {
  talk_base::Buffer packet(data, len, kMaxRtpPacketLen);
  return MediaChannel::SendPacket(&packet) ? len : -1;
}

int WebRtcVideoMediaChannel::SendRTCPPacket(int channel,
                                            const void* data,
                                            int len) {
  talk_base::Buffer packet(data, len, kMaxRtpPacketLen);
  return MediaChannel::SendRtcp(&packet) ? len : -1;
}

void WebRtcVideoMediaChannel::QueueBlackFrame(uint32 ssrc, int64 timestamp,
                                              int framerate) {
  if (timestamp) {
    FlushBlackFrameData* black_frame_data = new FlushBlackFrameData(
        ssrc,
        timestamp);
    const int delay_ms = static_cast<int>(
        2 * cricket::VideoFormat::FpsToInterval(framerate) *
        talk_base::kNumMillisecsPerSec / talk_base::kNumNanosecsPerSec);
    worker_thread()->PostDelayed(delay_ms, this, 0, black_frame_data);
  }
}

void WebRtcVideoMediaChannel::FlushBlackFrame(uint32 ssrc, int64 timestamp) {
  WebRtcVideoChannelSendInfo* send_channel = GetSendChannel(ssrc);
  if (!send_channel) {
    return;
  }
  talk_base::scoped_ptr<const VideoFrame> black_frame_ptr;

  const WebRtcLocalStreamInfo* channel_stream_info =
      send_channel->local_stream_info();
  int64 last_frame_time_stamp = channel_stream_info->time_stamp();
  if (last_frame_time_stamp == timestamp) {
    size_t last_frame_width = 0;
    size_t last_frame_height = 0;
    int64 last_frame_elapsed_time = 0;
    channel_stream_info->GetLastFrameInfo(&last_frame_width, &last_frame_height,
                                          &last_frame_elapsed_time);
    if (!last_frame_width || !last_frame_height) {
      return;
    }
    WebRtcVideoFrame black_frame;
    // Black frame is not screencast.
    const bool screencasting = false;
    const int64 timestamp_delta = send_channel->interval();
    if (!black_frame.InitToBlack(send_codec_->width, send_codec_->height, 1, 1,
                                 last_frame_elapsed_time + timestamp_delta,
                                 last_frame_time_stamp + timestamp_delta) ||
        !SendFrame(send_channel, &black_frame, screencasting)) {
      LOG(LS_ERROR) << "Failed to send black frame.";
    }
  }
}

void WebRtcVideoMediaChannel::OnCpuAdaptationUnable() {
  // ssrc is hardcoded to 0.  This message is based on a system wide issue,
  // so finding which ssrc caused it doesn't matter.
  SignalMediaError(0, VideoMediaChannel::ERROR_REC_CPU_MAX_CANT_DOWNGRADE);
}

void WebRtcVideoMediaChannel::SetNetworkTransmissionState(
    bool is_transmitting) {
  LOG(LS_INFO) << "SetNetworkTransmissionState: " << is_transmitting;
  for (SendChannelMap::iterator iter = send_channels_.begin();
       iter != send_channels_.end(); ++iter) {
    WebRtcVideoChannelSendInfo* send_channel = iter->second;
    int channel_id = send_channel->channel_id();
    engine_->vie()->network()->SetNetworkTransmissionState(channel_id,
                                                           is_transmitting);
  }
}

bool WebRtcVideoMediaChannel::SetHeaderExtension(ExtensionSetterFunction setter,
    int channel_id, const RtpHeaderExtension* extension) {
  bool enable = false;
  int id = 0;
  if (extension) {
    enable = true;
    id = extension->id;
  }
  if ((engine_->vie()->rtp()->*setter)(channel_id, enable, id) != 0) {
    LOG_RTCERR4(*setter, extension->uri, channel_id, enable, id);
    return false;
  }
  return true;
}

bool WebRtcVideoMediaChannel::SetHeaderExtension(ExtensionSetterFunction setter,
    int channel_id, const std::vector<RtpHeaderExtension>& extensions,
    const char header_extension_uri[]) {
  const RtpHeaderExtension* extension = FindHeaderExtension(extensions,
      header_extension_uri);
  return SetHeaderExtension(setter, channel_id, extension);
}

bool WebRtcVideoMediaChannel::SetLocalRtxSsrc(int channel_id,
                                              const StreamParams& send_params,
                                              uint32 primary_ssrc,
                                              int stream_idx) {
  uint32 rtx_ssrc = 0;
  bool has_rtx = send_params.GetFidSsrc(primary_ssrc, &rtx_ssrc);
  if (has_rtx && engine()->vie()->rtp()->SetLocalSSRC(
      channel_id, rtx_ssrc, webrtc::kViEStreamTypeRtx, stream_idx) != 0) {
    LOG_RTCERR4(SetLocalSSRC, channel_id, rtx_ssrc,
                webrtc::kViEStreamTypeRtx, stream_idx);
    return false;
  }
  return true;
}

void WebRtcVideoMediaChannel::MaybeConnectCapturer(VideoCapturer* capturer) {
  if (capturer != NULL && GetSendChannelNum(capturer) == 1) {
    capturer->SignalVideoFrame.connect(this,
                                       &WebRtcVideoMediaChannel::SendFrame);
  }
}

void WebRtcVideoMediaChannel::MaybeDisconnectCapturer(VideoCapturer* capturer) {
  if (capturer != NULL && GetSendChannelNum(capturer) == 1) {
    capturer->SignalVideoFrame.disconnect(this);
  }
}

}  // namespace cricket

#endif  // HAVE_WEBRTC_VIDEO
