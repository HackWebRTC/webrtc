/*
 * libjingle
 * Copyright 2014 Google Inc.
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
#include "talk/media/webrtc/webrtcvideoengine2.h"

#include <set>
#include <string>

#include "libyuv/convert_from.h"
#include "talk/base/buffer.h"
#include "talk/base/logging.h"
#include "talk/base/stringutils.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videorenderer.h"
#include "talk/media/webrtc/constants.h"
#include "talk/media/webrtc/webrtcvideocapturer.h"
#include "talk/media/webrtc/webrtcvideoframe.h"
#include "talk/media/webrtc/webrtcvoiceengine.h"
#include "webrtc/call.h"
// TODO(pbos): Move codecs out of modules (webrtc:3070).
#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"

#define UNIMPLEMENTED                                                 \
  LOG(LS_ERROR) << "Call to unimplemented function " << __FUNCTION__; \
  ASSERT(false)

namespace cricket {

// This constant is really an on/off, lower-level configurable NACK history
// duration hasn't been implemented.
static const int kNackHistoryMs = 1000;

static const int kDefaultRtcpReceiverReportSsrc = 1;

struct VideoCodecPref {
  int payload_type;
  const char* name;
  int rtx_payload_type;
} kDefaultVideoCodecPref = {100, kVp8CodecName, 96};

VideoCodecPref kRedPref = {116, kRedCodecName, -1};
VideoCodecPref kUlpfecPref = {117, kUlpfecCodecName, -1};

// The formats are sorted by the descending order of width. We use the order to
// find the next format for CPU and bandwidth adaptation.
const VideoFormatPod kDefaultMaxVideoFormat = {
    640, 400, FPS_TO_INTERVAL(kDefaultFramerate), FOURCC_ANY};

static bool FindFirstMatchingCodec(const std::vector<VideoCodec>& codecs,
                                   const VideoCodec& requested_codec,
                                   VideoCodec* matching_codec) {
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (requested_codec.Matches(codecs[i])) {
      *matching_codec = codecs[i];
      return true;
    }
  }
  return false;
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

static bool IsNackEnabled(const VideoCodec& codec) {
  return codec.HasFeedbackParam(
      FeedbackParam(kRtcpFbParamNack, kParamValueEmpty));
}

static VideoCodec DefaultVideoCodec() {
  VideoCodec default_codec(kDefaultVideoCodecPref.payload_type,
                           kDefaultVideoCodecPref.name,
                           kDefaultMaxVideoFormat.width,
                           kDefaultMaxVideoFormat.height,
                           kDefaultFramerate,
                           0);
  AddDefaultFeedbackParams(&default_codec);
  return default_codec;
}

static VideoCodec DefaultRedCodec() {
  return VideoCodec(kRedPref.payload_type, kRedPref.name, 0, 0, 0, 0);
}

static VideoCodec DefaultUlpfecCodec() {
  return VideoCodec(kUlpfecPref.payload_type, kUlpfecPref.name, 0, 0, 0, 0);
}

static std::vector<VideoCodec> DefaultVideoCodecs() {
  std::vector<VideoCodec> codecs;
  codecs.push_back(DefaultVideoCodec());
  codecs.push_back(DefaultRedCodec());
  codecs.push_back(DefaultUlpfecCodec());
  if (kDefaultVideoCodecPref.rtx_payload_type != -1) {
    codecs.push_back(
        VideoCodec::CreateRtxCodec(kDefaultVideoCodecPref.rtx_payload_type,
                                   kDefaultVideoCodecPref.payload_type));
  }
  return codecs;
}

static bool ValidateRtpHeaderExtensionIds(
    const std::vector<RtpHeaderExtension>& extensions) {
  std::set<int> extensions_used;
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (extensions[i].id < 0 || extensions[i].id >= 15 ||
        !extensions_used.insert(extensions[i].id).second) {
      LOG(LS_ERROR) << "RTP extensions are with incorrect or duplicate ids.";
      return false;
    }
  }
  return true;
}

static std::vector<webrtc::RtpExtension> FilterRtpExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  std::vector<webrtc::RtpExtension> webrtc_extensions;
  for (size_t i = 0; i < extensions.size(); ++i) {
    // Unsupported extensions will be ignored.
    if (webrtc::RtpExtension::IsSupported(extensions[i].uri)) {
      webrtc_extensions.push_back(webrtc::RtpExtension(
          extensions[i].uri, extensions[i].id));
    } else {
      LOG(LS_WARNING) << "Unsupported RTP extension: " << extensions[i].uri;
    }
  }
  return webrtc_extensions;
}

WebRtcVideoEncoderFactory2::~WebRtcVideoEncoderFactory2() {
}

std::vector<webrtc::VideoStream> WebRtcVideoEncoderFactory2::CreateVideoStreams(
    const VideoCodec& codec,
    const VideoOptions& options,
    size_t num_streams) {
  assert(SupportsCodec(codec));
  if (num_streams != 1) {
    LOG(LS_ERROR) << "Unsupported number of streams: " << num_streams;
    return std::vector<webrtc::VideoStream>();
  }

  webrtc::VideoStream stream;
  stream.width = codec.width;
  stream.height = codec.height;
  stream.max_framerate =
      codec.framerate != 0 ? codec.framerate : kDefaultFramerate;

  int min_bitrate = kMinVideoBitrate;
  codec.GetParam(kCodecParamMinBitrate, &min_bitrate);
  int max_bitrate = kMaxVideoBitrate;
  codec.GetParam(kCodecParamMaxBitrate, &max_bitrate);
  stream.min_bitrate_bps = min_bitrate * 1000;
  stream.target_bitrate_bps = stream.max_bitrate_bps = max_bitrate * 1000;

  int max_qp = 56;
  codec.GetParam(kCodecParamMaxQuantization, &max_qp);
  stream.max_qp = max_qp;
  std::vector<webrtc::VideoStream> streams;
  streams.push_back(stream);
  return streams;
}

webrtc::VideoEncoder* WebRtcVideoEncoderFactory2::CreateVideoEncoder(
    const VideoCodec& codec,
    const VideoOptions& options) {
  assert(SupportsCodec(codec));
  if (_stricmp(codec.name.c_str(), kVp8CodecName) == 0) {
    return webrtc::VP8Encoder::Create();
  }
  // This shouldn't happen, we should be able to create encoders for all codecs
  // we support.
  assert(false);
  return NULL;
}

void* WebRtcVideoEncoderFactory2::CreateVideoEncoderSettings(
    const VideoCodec& codec,
    const VideoOptions& options) {
  assert(SupportsCodec(codec));
  if (_stricmp(codec.name.c_str(), kVp8CodecName) == 0) {
    webrtc::VideoCodecVP8* settings = new webrtc::VideoCodecVP8();
    settings->resilience = webrtc::kResilientStream;
    settings->numberOfTemporalLayers = 1;
    options.video_noise_reduction.Get(&settings->denoisingOn);
    settings->errorConcealmentOn = false;
    settings->automaticResizeOn = false;
    settings->frameDroppingOn = true;
    settings->keyFrameInterval = 3000;
    return settings;
  }
  return NULL;
}

void WebRtcVideoEncoderFactory2::DestroyVideoEncoderSettings(
    const VideoCodec& codec,
    void* encoder_settings) {
  assert(SupportsCodec(codec));
  if (encoder_settings == NULL) {
    return;
  }

  if (_stricmp(codec.name.c_str(), kVp8CodecName) == 0) {
    delete reinterpret_cast<webrtc::VideoCodecVP8*>(encoder_settings);
    return;
  }
  // We should be able to destroy all encoder settings we've allocated.
  assert(false);
}

bool WebRtcVideoEncoderFactory2::SupportsCodec(const VideoCodec& codec) {
  return _stricmp(codec.name.c_str(), kVp8CodecName) == 0;
}

WebRtcVideoEngine2::WebRtcVideoEngine2() {
  // Construct without a factory or voice engine.
  Construct(NULL, NULL, new talk_base::CpuMonitor(NULL));
}

WebRtcVideoEngine2::WebRtcVideoEngine2(
    WebRtcVideoChannelFactory* channel_factory) {
  // Construct without a voice engine.
  Construct(channel_factory, NULL, new talk_base::CpuMonitor(NULL));
}

void WebRtcVideoEngine2::Construct(WebRtcVideoChannelFactory* channel_factory,
                                   WebRtcVoiceEngine* voice_engine,
                                   talk_base::CpuMonitor* cpu_monitor) {
  LOG(LS_INFO) << "WebRtcVideoEngine2::WebRtcVideoEngine2";
  worker_thread_ = NULL;
  voice_engine_ = voice_engine;
  initialized_ = false;
  capture_started_ = false;
  cpu_monitor_.reset(cpu_monitor);
  channel_factory_ = channel_factory;

  video_codecs_ = DefaultVideoCodecs();
  default_codec_format_ = VideoFormat(kDefaultMaxVideoFormat);

  rtp_header_extensions_.push_back(
      RtpHeaderExtension(kRtpTimestampOffsetHeaderExtension,
                         kRtpTimestampOffsetHeaderExtensionDefaultId));
  rtp_header_extensions_.push_back(
      RtpHeaderExtension(kRtpAbsoluteSenderTimeHeaderExtension,
                         kRtpAbsoluteSenderTimeHeaderExtensionDefaultId));
}

WebRtcVideoEngine2::~WebRtcVideoEngine2() {
  LOG(LS_INFO) << "WebRtcVideoEngine2::~WebRtcVideoEngine2";

  if (initialized_) {
    Terminate();
  }
}

bool WebRtcVideoEngine2::Init(talk_base::Thread* worker_thread) {
  LOG(LS_INFO) << "WebRtcVideoEngine2::Init";
  worker_thread_ = worker_thread;
  ASSERT(worker_thread_ != NULL);

  cpu_monitor_->set_thread(worker_thread_);
  if (!cpu_monitor_->Start(kCpuMonitorPeriodMs)) {
    LOG(LS_ERROR) << "Failed to start CPU monitor.";
    cpu_monitor_.reset();
  }

  initialized_ = true;
  return true;
}

void WebRtcVideoEngine2::Terminate() {
  LOG(LS_INFO) << "WebRtcVideoEngine2::Terminate";

  cpu_monitor_->Stop();

  initialized_ = false;
}

int WebRtcVideoEngine2::GetCapabilities() { return VIDEO_RECV | VIDEO_SEND; }

bool WebRtcVideoEngine2::SetOptions(const VideoOptions& options) {
  // TODO(pbos): Do we need this? This is a no-op in the existing
  // WebRtcVideoEngine implementation.
  LOG(LS_VERBOSE) << "SetOptions: " << options.ToString();
  //  options_ = options;
  return true;
}

bool WebRtcVideoEngine2::SetDefaultEncoderConfig(
    const VideoEncoderConfig& config) {
  const VideoCodec& codec = config.max_codec;
  // TODO(pbos): Make use of external encoder factory.
  if (!GetVideoEncoderFactory()->SupportsCodec(codec)) {
    LOG(LS_ERROR) << "SetDefaultEncoderConfig, codec not supported:"
                  << codec.ToString();
    return false;
  }

  default_codec_format_ =
      VideoFormat(codec.width,
                  codec.height,
                  VideoFormat::FpsToInterval(codec.framerate),
                  FOURCC_ANY);
  video_codecs_.clear();
  video_codecs_.push_back(codec);
  return true;
}

VideoEncoderConfig WebRtcVideoEngine2::GetDefaultEncoderConfig() const {
  return VideoEncoderConfig(DefaultVideoCodec());
}

WebRtcVideoChannel2* WebRtcVideoEngine2::CreateChannel(
    VoiceMediaChannel* voice_channel) {
  LOG(LS_INFO) << "CreateChannel: "
               << (voice_channel != NULL ? "With" : "Without")
               << " voice channel.";
  WebRtcVideoChannel2* channel =
      channel_factory_ != NULL
          ? channel_factory_->Create(this, voice_channel)
          : new WebRtcVideoChannel2(
                this, voice_channel, GetVideoEncoderFactory());
  if (!channel->Init()) {
    delete channel;
    return NULL;
  }
  channel->SetRecvCodecs(video_codecs_);
  return channel;
}

const std::vector<VideoCodec>& WebRtcVideoEngine2::codecs() const {
  return video_codecs_;
}

const std::vector<RtpHeaderExtension>&
WebRtcVideoEngine2::rtp_header_extensions() const {
  return rtp_header_extensions_;
}

void WebRtcVideoEngine2::SetLogging(int min_sev, const char* filter) {
  // TODO(pbos): Set up logging.
  LOG(LS_VERBOSE) << "SetLogging: " << min_sev << '"' << filter << '"';
  // if min_sev == -1, we keep the current log level.
  if (min_sev < 0) {
    assert(min_sev == -1);
    return;
  }
}

bool WebRtcVideoEngine2::EnableTimedRender() {
  // TODO(pbos): Figure out whether this can be removed.
  return true;
}

bool WebRtcVideoEngine2::SetLocalRenderer(VideoRenderer* renderer) {
  // TODO(pbos): Implement or remove. Unclear which stream should be rendered
  // locally even.
  return true;
}

// Checks to see whether we comprehend and could receive a particular codec
bool WebRtcVideoEngine2::FindCodec(const VideoCodec& in) {
  // TODO(pbos): Probe encoder factory to figure out that the codec is supported
  // if supported by the encoder factory. Add a corresponding test that fails
  // with this code (that doesn't ask the factory).
  for (size_t j = 0; j < video_codecs_.size(); ++j) {
    VideoCodec codec(video_codecs_[j].id, video_codecs_[j].name, 0, 0, 0, 0);
    if (codec.Matches(in)) {
      return true;
    }
  }
  return false;
}

// Tells whether the |requested| codec can be transmitted or not. If it can be
// transmitted |out| is set with the best settings supported. Aspect ratio will
// be set as close to |current|'s as possible. If not set |requested|'s
// dimensions will be used for aspect ratio matching.
bool WebRtcVideoEngine2::CanSendCodec(const VideoCodec& requested,
                                      const VideoCodec& current,
                                      VideoCodec* out) {
  assert(out != NULL);

  if (requested.width != requested.height &&
      (requested.height == 0 || requested.width == 0)) {
    // 0xn and nx0 are invalid resolutions.
    return false;
  }

  VideoCodec matching_codec;
  if (!FindFirstMatchingCodec(video_codecs_, requested, &matching_codec)) {
    // Codec not supported.
    return false;
  }

  out->id = requested.id;
  out->name = requested.name;
  out->preference = requested.preference;
  out->params = requested.params;
  out->framerate =
      talk_base::_min(requested.framerate, matching_codec.framerate);
  out->params = requested.params;
  out->feedback_params = requested.feedback_params;
  out->width = requested.width;
  out->height = requested.height;
  if (requested.width == 0 && requested.height == 0) {
    return true;
  }

  while (out->width > matching_codec.width) {
    out->width /= 2;
    out->height /= 2;
  }

  return out->width > 0 && out->height > 0;
}

bool WebRtcVideoEngine2::SetVoiceEngine(WebRtcVoiceEngine* voice_engine) {
  if (initialized_) {
    LOG(LS_WARNING) << "SetVoiceEngine can not be called after Init";
    return false;
  }
  voice_engine_ = voice_engine;
  return true;
}

// Ignore spammy trace messages, mostly from the stats API when we haven't
// gotten RTCP info yet from the remote side.
bool WebRtcVideoEngine2::ShouldIgnoreTrace(const std::string& trace) {
  static const char* const kTracesToIgnore[] = {NULL};
  for (const char* const* p = kTracesToIgnore; *p; ++p) {
    if (trace.find(*p) == 0) {
      return true;
    }
  }
  return false;
}

WebRtcVideoEncoderFactory2* WebRtcVideoEngine2::GetVideoEncoderFactory() {
  return &default_video_encoder_factory_;
}

// Thin map between VideoFrame and an existing webrtc::I420VideoFrame
// to avoid having to copy the rendered VideoFrame prematurely.
// This implementation is only safe to use in a const context and should never
// be written to.
class WebRtcVideoRenderFrame : public VideoFrame {
 public:
  explicit WebRtcVideoRenderFrame(const webrtc::I420VideoFrame* frame)
      : frame_(frame) {}

  virtual bool InitToBlack(int w,
                           int h,
                           size_t pixel_width,
                           size_t pixel_height,
                           int64 elapsed_time,
                           int64 time_stamp) OVERRIDE {
    UNIMPLEMENTED;
    return false;
  }

  virtual bool Reset(uint32 fourcc,
                     int w,
                     int h,
                     int dw,
                     int dh,
                     uint8* sample,
                     size_t sample_size,
                     size_t pixel_width,
                     size_t pixel_height,
                     int64 elapsed_time,
                     int64 time_stamp,
                     int rotation) OVERRIDE {
    UNIMPLEMENTED;
    return false;
  }

  virtual size_t GetWidth() const OVERRIDE {
    return static_cast<size_t>(frame_->width());
  }
  virtual size_t GetHeight() const OVERRIDE {
    return static_cast<size_t>(frame_->height());
  }

  virtual const uint8* GetYPlane() const OVERRIDE {
    return frame_->buffer(webrtc::kYPlane);
  }
  virtual const uint8* GetUPlane() const OVERRIDE {
    return frame_->buffer(webrtc::kUPlane);
  }
  virtual const uint8* GetVPlane() const OVERRIDE {
    return frame_->buffer(webrtc::kVPlane);
  }

  virtual uint8* GetYPlane() OVERRIDE {
    UNIMPLEMENTED;
    return NULL;
  }
  virtual uint8* GetUPlane() OVERRIDE {
    UNIMPLEMENTED;
    return NULL;
  }
  virtual uint8* GetVPlane() OVERRIDE {
    UNIMPLEMENTED;
    return NULL;
  }

  virtual int32 GetYPitch() const OVERRIDE {
    return frame_->stride(webrtc::kYPlane);
  }
  virtual int32 GetUPitch() const OVERRIDE {
    return frame_->stride(webrtc::kUPlane);
  }
  virtual int32 GetVPitch() const OVERRIDE {
    return frame_->stride(webrtc::kVPlane);
  }

  virtual void* GetNativeHandle() const OVERRIDE { return NULL; }

  virtual size_t GetPixelWidth() const OVERRIDE { return 1; }
  virtual size_t GetPixelHeight() const OVERRIDE { return 1; }

  virtual int64 GetElapsedTime() const OVERRIDE {
    // Convert millisecond render time to ns timestamp.
    return frame_->render_time_ms() * talk_base::kNumNanosecsPerMillisec;
  }
  virtual int64 GetTimeStamp() const OVERRIDE {
    // Convert 90K rtp timestamp to ns timestamp.
    return (frame_->timestamp() / 90) * talk_base::kNumNanosecsPerMillisec;
  }
  virtual void SetElapsedTime(int64 elapsed_time) OVERRIDE { UNIMPLEMENTED; }
  virtual void SetTimeStamp(int64 time_stamp) OVERRIDE { UNIMPLEMENTED; }

  virtual int GetRotation() const OVERRIDE {
    UNIMPLEMENTED;
    return ROTATION_0;
  }

  virtual VideoFrame* Copy() const OVERRIDE {
    UNIMPLEMENTED;
    return NULL;
  }

  virtual bool MakeExclusive() OVERRIDE {
    UNIMPLEMENTED;
    return false;
  }

  virtual size_t CopyToBuffer(uint8* buffer, size_t size) const {
    UNIMPLEMENTED;
    return 0;
  }

  // TODO(fbarchard): Refactor into base class and share with LMI
  virtual size_t ConvertToRgbBuffer(uint32 to_fourcc,
                                    uint8* buffer,
                                    size_t size,
                                    int stride_rgb) const OVERRIDE {
    size_t width = GetWidth();
    size_t height = GetHeight();
    size_t needed = (stride_rgb >= 0 ? stride_rgb : -stride_rgb) * height;
    if (size < needed) {
      LOG(LS_WARNING) << "RGB buffer is not large enough";
      return needed;
    }

    if (libyuv::ConvertFromI420(GetYPlane(),
                                GetYPitch(),
                                GetUPlane(),
                                GetUPitch(),
                                GetVPlane(),
                                GetVPitch(),
                                buffer,
                                stride_rgb,
                                static_cast<int>(width),
                                static_cast<int>(height),
                                to_fourcc)) {
      LOG(LS_ERROR) << "RGB type not supported: " << to_fourcc;
      return 0;  // 0 indicates error
    }
    return needed;
  }

 protected:
  virtual VideoFrame* CreateEmptyFrame(int w,
                                       int h,
                                       size_t pixel_width,
                                       size_t pixel_height,
                                       int64 elapsed_time,
                                       int64 time_stamp) const OVERRIDE {
    // TODO(pbos): Remove WebRtcVideoFrame dependency, and have a non-const
    // version of I420VideoFrame wrapped.
    WebRtcVideoFrame* frame = new WebRtcVideoFrame();
    frame->InitToBlack(
        w, h, pixel_width, pixel_height, elapsed_time, time_stamp);
    return frame;
  }

 private:
  const webrtc::I420VideoFrame* const frame_;
};

WebRtcVideoChannel2::WebRtcVideoChannel2(
    WebRtcVideoEngine2* engine,
    VoiceMediaChannel* voice_channel,
    WebRtcVideoEncoderFactory2* encoder_factory)
    : encoder_factory_(encoder_factory) {
  // TODO(pbos): Connect the video and audio with |voice_channel|.
  webrtc::Call::Config config(this);
  Construct(webrtc::Call::Create(config), engine);
}

WebRtcVideoChannel2::WebRtcVideoChannel2(
    webrtc::Call* call,
    WebRtcVideoEngine2* engine,
    WebRtcVideoEncoderFactory2* encoder_factory)
    : encoder_factory_(encoder_factory) {
  Construct(call, engine);
}

void WebRtcVideoChannel2::Construct(webrtc::Call* call,
                                    WebRtcVideoEngine2* engine) {
  rtcp_receiver_report_ssrc_ = kDefaultRtcpReceiverReportSsrc;
  sending_ = false;
  call_.reset(call);
  default_renderer_ = NULL;
  default_send_ssrc_ = 0;
  default_recv_ssrc_ = 0;

  SetDefaultOptions();
}

void WebRtcVideoChannel2::SetDefaultOptions() {
  options_.video_noise_reduction.Set(true);
  options_.use_payload_padding.Set(false);
  options_.suspend_below_min_bitrate.Set(false);
}

WebRtcVideoChannel2::~WebRtcVideoChannel2() {
  for (std::map<uint32, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end();
       ++it) {
    delete it->second;
  }

  for (std::map<uint32, WebRtcVideoReceiveStream*>::iterator it =
           receive_streams_.begin();
       it != receive_streams_.end();
       ++it) {
    delete it->second;
  }
}

bool WebRtcVideoChannel2::Init() { return true; }

namespace {

static std::string CodecVectorToString(const std::vector<VideoCodec>& codecs) {
  std::stringstream out;
  out << '{';
  for (size_t i = 0; i < codecs.size(); ++i) {
    out << codecs[i].ToString();
    if (i != codecs.size() - 1) {
      out << ", ";
    }
  }
  out << '}';
  return out.str();
}

static bool ValidateCodecFormats(const std::vector<VideoCodec>& codecs) {
  bool has_video = false;
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (!codecs[i].ValidateCodecFormat()) {
      return false;
    }
    if (codecs[i].GetCodecType() == VideoCodec::CODEC_VIDEO) {
      has_video = true;
    }
  }
  if (!has_video) {
    LOG(LS_ERROR) << "Setting codecs without a video codec is invalid: "
                  << CodecVectorToString(codecs);
    return false;
  }
  return true;
}

static std::string RtpExtensionsToString(
    const std::vector<RtpHeaderExtension>& extensions) {
  std::stringstream out;
  out << '{';
  for (size_t i = 0; i < extensions.size(); ++i) {
    out << "{" << extensions[i].uri << ": " << extensions[i].id << "}";
    if (i != extensions.size() - 1) {
      out << ", ";
    }
  }
  out << '}';
  return out.str();
}

}  // namespace

bool WebRtcVideoChannel2::SetRecvCodecs(const std::vector<VideoCodec>& codecs) {
  // TODO(pbos): Must these receive codecs propagate to existing receive
  // streams?
  LOG(LS_INFO) << "SetRecvCodecs: " << CodecVectorToString(codecs);
  if (!ValidateCodecFormats(codecs)) {
    return false;
  }

  const std::vector<VideoCodecSettings> mapped_codecs = MapCodecs(codecs);
  if (mapped_codecs.empty()) {
    LOG(LS_ERROR) << "SetRecvCodecs called without video codec payloads.";
    return false;
  }

  // TODO(pbos): Add a decoder factory which controls supported codecs.
  // Blocked on webrtc:2854.
  for (size_t i = 0; i < mapped_codecs.size(); ++i) {
    if (_stricmp(mapped_codecs[i].codec.name.c_str(), kVp8CodecName) != 0) {
      LOG(LS_ERROR) << "SetRecvCodecs called with unsupported codec: '"
                    << mapped_codecs[i].codec.name << "'";
      return false;
    }
  }

  recv_codecs_ = mapped_codecs;

  for (std::map<uint32, WebRtcVideoReceiveStream*>::iterator it =
           receive_streams_.begin();
       it != receive_streams_.end();
       ++it) {
    it->second->SetRecvCodecs(recv_codecs_);
  }

  return true;
}

bool WebRtcVideoChannel2::SetSendCodecs(const std::vector<VideoCodec>& codecs) {
  LOG(LS_INFO) << "SetSendCodecs: " << CodecVectorToString(codecs);
  if (!ValidateCodecFormats(codecs)) {
    return false;
  }

  const std::vector<VideoCodecSettings> supported_codecs =
      FilterSupportedCodecs(MapCodecs(codecs));

  if (supported_codecs.empty()) {
    LOG(LS_ERROR) << "No video codecs supported by encoder factory.";
    return false;
  }

  send_codec_.Set(supported_codecs.front());
  LOG(LS_INFO) << "Using codec: " << supported_codecs.front().codec.ToString();

  for (std::map<uint32, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end();
       ++it) {
    assert(it->second != NULL);
    it->second->SetCodec(supported_codecs.front());
  }

  return true;
}

bool WebRtcVideoChannel2::GetSendCodec(VideoCodec* codec) {
  VideoCodecSettings codec_settings;
  if (!send_codec_.Get(&codec_settings)) {
    LOG(LS_VERBOSE) << "GetSendCodec: No send codec set.";
    return false;
  }
  *codec = codec_settings.codec;
  return true;
}

bool WebRtcVideoChannel2::SetSendStreamFormat(uint32 ssrc,
                                              const VideoFormat& format) {
  LOG(LS_VERBOSE) << "SetSendStreamFormat:" << ssrc << " -> "
                  << format.ToString();
  if (send_streams_.find(ssrc) == send_streams_.end()) {
    return false;
  }
  return send_streams_[ssrc]->SetVideoFormat(format);
}

bool WebRtcVideoChannel2::SetRender(bool render) {
  // TODO(pbos): Implement. Or refactor away as it shouldn't be needed.
  LOG(LS_VERBOSE) << "SetRender: " << (render ? "true" : "false");
  return true;
}

bool WebRtcVideoChannel2::SetSend(bool send) {
  LOG(LS_VERBOSE) << "SetSend: " << (send ? "true" : "false");
  if (send && !send_codec_.IsSet()) {
    LOG(LS_ERROR) << "SetSend(true) called before setting codec.";
    return false;
  }
  if (send) {
    StartAllSendStreams();
  } else {
    StopAllSendStreams();
  }
  sending_ = send;
  return true;
}

bool WebRtcVideoChannel2::AddSendStream(const StreamParams& sp) {
  LOG(LS_INFO) << "AddSendStream: " << sp.ToString();
  if (sp.ssrcs.empty()) {
    LOG(LS_ERROR) << "No SSRCs in stream parameters.";
    return false;
  }

  uint32 ssrc = sp.first_ssrc();
  assert(ssrc != 0);
  // TODO(pbos): Make sure none of sp.ssrcs are used, not just the identifying
  // ssrc.
  if (send_streams_.find(ssrc) != send_streams_.end()) {
    LOG(LS_ERROR) << "Send stream with ssrc '" << ssrc << "' already exists.";
    return false;
  }

  std::vector<uint32> primary_ssrcs;
  sp.GetPrimarySsrcs(&primary_ssrcs);
  std::vector<uint32> rtx_ssrcs;
  sp.GetFidSsrcs(primary_ssrcs, &rtx_ssrcs);
  if (!rtx_ssrcs.empty() && primary_ssrcs.size() != rtx_ssrcs.size()) {
    LOG(LS_ERROR)
        << "RTX SSRCs exist, but don't cover all SSRCs (unsupported): "
        << sp.ToString();
    return false;
  }

  WebRtcVideoSendStream* stream =
      new WebRtcVideoSendStream(call_.get(),
                                encoder_factory_,
                                options_,
                                send_codec_,
                                sp,
                                send_rtp_extensions_);

  send_streams_[ssrc] = stream;

  if (rtcp_receiver_report_ssrc_ == kDefaultRtcpReceiverReportSsrc) {
    rtcp_receiver_report_ssrc_ = ssrc;
  }
  if (default_send_ssrc_ == 0) {
    default_send_ssrc_ = ssrc;
  }
  if (sending_) {
    stream->Start();
  }

  return true;
}

bool WebRtcVideoChannel2::RemoveSendStream(uint32 ssrc) {
  LOG(LS_INFO) << "RemoveSendStream: " << ssrc;

  if (ssrc == 0) {
    if (default_send_ssrc_ == 0) {
      LOG(LS_ERROR) << "No default send stream active.";
      return false;
    }

    LOG(LS_VERBOSE) << "Removing default stream: " << default_send_ssrc_;
    ssrc = default_send_ssrc_;
  }

  std::map<uint32, WebRtcVideoSendStream*>::iterator it =
      send_streams_.find(ssrc);
  if (it == send_streams_.end()) {
    return false;
  }

  delete it->second;
  send_streams_.erase(it);

  if (ssrc == default_send_ssrc_) {
    default_send_ssrc_ = 0;
  }

  return true;
}

bool WebRtcVideoChannel2::AddRecvStream(const StreamParams& sp) {
  LOG(LS_INFO) << "AddRecvStream: " << sp.ToString();
  assert(sp.ssrcs.size() > 0);

  uint32 ssrc = sp.first_ssrc();
  assert(ssrc != 0);  // TODO(pbos): Is this ever valid?
  if (default_recv_ssrc_ == 0) {
    default_recv_ssrc_ = ssrc;
  }

  // TODO(pbos): Check if any of the SSRCs overlap.
  if (receive_streams_.find(ssrc) != receive_streams_.end()) {
    LOG(LS_ERROR) << "Receive stream for SSRC " << ssrc << "already exists.";
    return false;
  }

  webrtc::VideoReceiveStream::Config config;
  ConfigureReceiverRtp(&config, sp);
  receive_streams_[ssrc] =
      new WebRtcVideoReceiveStream(call_.get(), config, recv_codecs_);

  return true;
}

void WebRtcVideoChannel2::ConfigureReceiverRtp(
    webrtc::VideoReceiveStream::Config* config,
    const StreamParams& sp) const {
  uint32 ssrc = sp.first_ssrc();

  config->rtp.remote_ssrc = ssrc;
  config->rtp.local_ssrc = rtcp_receiver_report_ssrc_;

  if (IsNackEnabled(recv_codecs_.begin()->codec)) {
    config->rtp.nack.rtp_history_ms = kNackHistoryMs;
  }
  config->rtp.remb = true;
  config->rtp.extensions = recv_rtp_extensions_;
  // TODO(pbos): This protection is against setting the same local ssrc as
  // remote which is not permitted by the lower-level API. RTCP requires a
  // corresponding sender SSRC. Figure out what to do when we don't have
  // (receive-only) or know a good local SSRC.
  if (config->rtp.remote_ssrc == config->rtp.local_ssrc) {
    if (config->rtp.local_ssrc != kDefaultRtcpReceiverReportSsrc) {
      config->rtp.local_ssrc = kDefaultRtcpReceiverReportSsrc;
    } else {
      config->rtp.local_ssrc = kDefaultRtcpReceiverReportSsrc + 1;
    }
  }

  for (size_t i = 0; i < recv_codecs_.size(); ++i) {
    if (recv_codecs_[i].codec.id == kDefaultVideoCodecPref.payload_type) {
      config->rtp.fec = recv_codecs_[i].fec;
      uint32 rtx_ssrc;
      if (recv_codecs_[i].rtx_payload_type != -1 &&
          sp.GetFidSsrc(ssrc, &rtx_ssrc)) {
        config->rtp.rtx[kDefaultVideoCodecPref.payload_type].ssrc = rtx_ssrc;
        config->rtp.rtx[kDefaultVideoCodecPref.payload_type].payload_type =
            recv_codecs_[i].rtx_payload_type;
      }
      break;
    }
  }

}

bool WebRtcVideoChannel2::RemoveRecvStream(uint32 ssrc) {
  LOG(LS_INFO) << "RemoveRecvStream: " << ssrc;
  if (ssrc == 0) {
    ssrc = default_recv_ssrc_;
  }

  std::map<uint32, WebRtcVideoReceiveStream*>::iterator stream =
      receive_streams_.find(ssrc);
  if (stream == receive_streams_.end()) {
    LOG(LS_ERROR) << "Stream not found for ssrc: " << ssrc;
    return false;
  }
  delete stream->second;
  receive_streams_.erase(stream);

  if (ssrc == default_recv_ssrc_) {
    default_recv_ssrc_ = 0;
  }

  return true;
}

bool WebRtcVideoChannel2::SetRenderer(uint32 ssrc, VideoRenderer* renderer) {
  LOG(LS_INFO) << "SetRenderer: ssrc:" << ssrc << " "
               << (renderer ? "(ptr)" : "NULL");
  if (ssrc == 0) {
    if (default_recv_ssrc_!= 0) {
      receive_streams_[default_recv_ssrc_]->SetRenderer(renderer);
    }
    ssrc = default_recv_ssrc_;
    default_renderer_ = renderer;
    return true;
  }

  std::map<uint32, WebRtcVideoReceiveStream*>::iterator it =
      receive_streams_.find(ssrc);
  if (it == receive_streams_.end()) {
    return false;
  }

  it->second->SetRenderer(renderer);
  return true;
}

bool WebRtcVideoChannel2::GetRenderer(uint32 ssrc, VideoRenderer** renderer) {
  if (ssrc == 0) {
    if (default_renderer_ == NULL) {
      return false;
    }
    *renderer = default_renderer_;
    return true;
  }

  std::map<uint32, WebRtcVideoReceiveStream*>::iterator it =
      receive_streams_.find(ssrc);
  if (it == receive_streams_.end()) {
    return false;
  }
  *renderer = it->second->GetRenderer();
  return true;
}

bool WebRtcVideoChannel2::GetStats(const StatsOptions& options,
                                   VideoMediaInfo* info) {
  info->Clear();
  FillSenderStats(info);
  FillReceiverStats(info);
  FillBandwidthEstimationStats(info);
  return true;
}

void WebRtcVideoChannel2::FillSenderStats(VideoMediaInfo* video_media_info) {
  for (std::map<uint32, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end();
       ++it) {
    video_media_info->senders.push_back(it->second->GetVideoSenderInfo());
  }
}

void WebRtcVideoChannel2::FillReceiverStats(VideoMediaInfo* video_media_info) {
  for (std::map<uint32, WebRtcVideoReceiveStream*>::iterator it =
           receive_streams_.begin();
       it != receive_streams_.end();
       ++it) {
    video_media_info->receivers.push_back(it->second->GetVideoReceiverInfo());
  }
}

void WebRtcVideoChannel2::FillBandwidthEstimationStats(
    VideoMediaInfo* video_media_info) {
  // TODO(pbos): Implement.
}

bool WebRtcVideoChannel2::SetCapturer(uint32 ssrc, VideoCapturer* capturer) {
  LOG(LS_INFO) << "SetCapturer: " << ssrc << " -> "
               << (capturer != NULL ? "(capturer)" : "NULL");
  assert(ssrc != 0);
  if (send_streams_.find(ssrc) == send_streams_.end()) {
    LOG(LS_ERROR) << "No sending stream on ssrc " << ssrc;
    return false;
  }
  return send_streams_[ssrc]->SetCapturer(capturer);
}

bool WebRtcVideoChannel2::SendIntraFrame() {
  // TODO(pbos): Implement.
  LOG(LS_VERBOSE) << "SendIntraFrame().";
  return true;
}

bool WebRtcVideoChannel2::RequestIntraFrame() {
  // TODO(pbos): Implement.
  LOG(LS_VERBOSE) << "SendIntraFrame().";
  return true;
}

void WebRtcVideoChannel2::OnPacketReceived(
    talk_base::Buffer* packet,
    const talk_base::PacketTime& packet_time) {
  const webrtc::PacketReceiver::DeliveryStatus delivery_result =
      call_->Receiver()->DeliverPacket(
          reinterpret_cast<const uint8_t*>(packet->data()), packet->length());
  switch (delivery_result) {
    case webrtc::PacketReceiver::DELIVERY_OK:
      return;
    case webrtc::PacketReceiver::DELIVERY_PACKET_ERROR:
      return;
    case webrtc::PacketReceiver::DELIVERY_UNKNOWN_SSRC:
      break;
  }

  uint32 ssrc = 0;
  if (default_recv_ssrc_ != 0) {  // Already one default stream.
    LOG(LS_WARNING) << "Unknown SSRC, but default receive stream already set.";
    return;
  }

  if (!GetRtpSsrc(packet->data(), packet->length(), &ssrc)) {
    return;
  }

  StreamParams sp;
  sp.ssrcs.push_back(ssrc);
  LOG(LS_INFO) << "Creating default receive stream for SSRC=" << ssrc << ".";
  AddRecvStream(sp);
  SetRenderer(0, default_renderer_);

  if (call_->Receiver()->DeliverPacket(
          reinterpret_cast<const uint8_t*>(packet->data()), packet->length()) !=
      webrtc::PacketReceiver::DELIVERY_OK) {
    LOG(LS_WARNING) << "Failed to deliver RTP packet after creating default "
                       "receiver.";
    return;
  }
}

void WebRtcVideoChannel2::OnRtcpReceived(
    talk_base::Buffer* packet,
    const talk_base::PacketTime& packet_time) {
  if (call_->Receiver()->DeliverPacket(
          reinterpret_cast<const uint8_t*>(packet->data()), packet->length()) !=
      webrtc::PacketReceiver::DELIVERY_OK) {
    LOG(LS_WARNING) << "Failed to deliver RTCP packet.";
  }
}

void WebRtcVideoChannel2::OnReadyToSend(bool ready) {
  LOG(LS_VERBOSE) << "OnReadySend: " << (ready ? "Ready." : "Not ready.");
}

bool WebRtcVideoChannel2::MuteStream(uint32 ssrc, bool mute) {
  LOG(LS_VERBOSE) << "MuteStream: " << ssrc << " -> "
                  << (mute ? "mute" : "unmute");
  assert(ssrc != 0);
  if (send_streams_.find(ssrc) == send_streams_.end()) {
    LOG(LS_ERROR) << "No sending stream on ssrc " << ssrc;
    return false;
  }
  return send_streams_[ssrc]->MuteStream(mute);
}

bool WebRtcVideoChannel2::SetRecvRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  LOG(LS_INFO) << "SetRecvRtpHeaderExtensions: "
               << RtpExtensionsToString(extensions);
  if (!ValidateRtpHeaderExtensionIds(extensions))
    return false;

  recv_rtp_extensions_ = FilterRtpExtensions(extensions);
  for (std::map<uint32, WebRtcVideoReceiveStream*>::iterator it =
           receive_streams_.begin();
       it != receive_streams_.end();
       ++it) {
    it->second->SetRtpExtensions(recv_rtp_extensions_);
  }
  return true;
}

bool WebRtcVideoChannel2::SetSendRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  LOG(LS_INFO) << "SetSendRtpHeaderExtensions: "
               << RtpExtensionsToString(extensions);
  if (!ValidateRtpHeaderExtensionIds(extensions))
    return false;

  send_rtp_extensions_ = FilterRtpExtensions(extensions);
  for (std::map<uint32, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end();
       ++it) {
    it->second->SetRtpExtensions(send_rtp_extensions_);
  }
  return true;
}

bool WebRtcVideoChannel2::SetStartSendBandwidth(int bps) {
  // TODO(pbos): Implement.
  LOG(LS_VERBOSE) << "SetStartSendBandwidth: " << bps;
  return true;
}

bool WebRtcVideoChannel2::SetMaxSendBandwidth(int bps) {
  // TODO(pbos): Implement.
  LOG(LS_VERBOSE) << "SetMaxSendBandwidth: " << bps;
  return true;
}

bool WebRtcVideoChannel2::SetOptions(const VideoOptions& options) {
  LOG(LS_VERBOSE) << "SetOptions: " << options.ToString();
  options_.SetAll(options);
  for (std::map<uint32, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end();
       ++it) {
    it->second->SetOptions(options_);
  }
  return true;
}

void WebRtcVideoChannel2::SetInterface(NetworkInterface* iface) {
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

void WebRtcVideoChannel2::UpdateAspectRatio(int ratio_w, int ratio_h) {
  // TODO(pbos): Implement.
}

void WebRtcVideoChannel2::OnMessage(talk_base::Message* msg) {
  // Ignored.
}

bool WebRtcVideoChannel2::SendRtp(const uint8_t* data, size_t len) {
  talk_base::Buffer packet(data, len, kMaxRtpPacketLen);
  return MediaChannel::SendPacket(&packet);
}

bool WebRtcVideoChannel2::SendRtcp(const uint8_t* data, size_t len) {
  talk_base::Buffer packet(data, len, kMaxRtpPacketLen);
  return MediaChannel::SendRtcp(&packet);
}

void WebRtcVideoChannel2::StartAllSendStreams() {
  for (std::map<uint32, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end();
       ++it) {
    it->second->Start();
  }
}

void WebRtcVideoChannel2::StopAllSendStreams() {
  for (std::map<uint32, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end();
       ++it) {
    it->second->Stop();
  }
}

WebRtcVideoChannel2::WebRtcVideoSendStream::VideoSendStreamParameters::
    VideoSendStreamParameters(
        const webrtc::VideoSendStream::Config& config,
        const VideoOptions& options,
        const Settable<VideoCodecSettings>& codec_settings)
    : config(config), options(options), codec_settings(codec_settings) {
}

WebRtcVideoChannel2::WebRtcVideoSendStream::WebRtcVideoSendStream(
    webrtc::Call* call,
    WebRtcVideoEncoderFactory2* encoder_factory,
    const VideoOptions& options,
    const Settable<VideoCodecSettings>& codec_settings,
    const StreamParams& sp,
    const std::vector<webrtc::RtpExtension>& rtp_extensions)
    : call_(call),
      parameters_(webrtc::VideoSendStream::Config(), options, codec_settings),
      encoder_factory_(encoder_factory),
      capturer_(NULL),
      stream_(NULL),
      sending_(false),
      muted_(false) {
  parameters_.config.rtp.max_packet_size = kVideoMtu;

  sp.GetPrimarySsrcs(&parameters_.config.rtp.ssrcs);
  sp.GetFidSsrcs(parameters_.config.rtp.ssrcs,
                 &parameters_.config.rtp.rtx.ssrcs);
  parameters_.config.rtp.c_name = sp.cname;
  parameters_.config.rtp.extensions = rtp_extensions;

  VideoCodecSettings params;
  if (codec_settings.Get(&params)) {
    SetCodec(params);
  }
}

WebRtcVideoChannel2::WebRtcVideoSendStream::~WebRtcVideoSendStream() {
  DisconnectCapturer();
  if (stream_ != NULL) {
    call_->DestroyVideoSendStream(stream_);
  }
  delete parameters_.config.encoder_settings.encoder;
}

static void SetWebRtcFrameToBlack(webrtc::I420VideoFrame* video_frame) {
  assert(video_frame != NULL);
  memset(video_frame->buffer(webrtc::kYPlane),
         16,
         video_frame->allocated_size(webrtc::kYPlane));
  memset(video_frame->buffer(webrtc::kUPlane),
         128,
         video_frame->allocated_size(webrtc::kUPlane));
  memset(video_frame->buffer(webrtc::kVPlane),
         128,
         video_frame->allocated_size(webrtc::kVPlane));
}

static void CreateBlackFrame(webrtc::I420VideoFrame* video_frame,
                             int width,
                             int height) {
  video_frame->CreateEmptyFrame(
      width, height, width, (width + 1) / 2, (width + 1) / 2);
  SetWebRtcFrameToBlack(video_frame);
}

static void ConvertToI420VideoFrame(const VideoFrame& frame,
                                    webrtc::I420VideoFrame* i420_frame) {
  i420_frame->CreateFrame(
      static_cast<int>(frame.GetYPitch() * frame.GetHeight()),
      frame.GetYPlane(),
      static_cast<int>(frame.GetUPitch() * ((frame.GetHeight() + 1) / 2)),
      frame.GetUPlane(),
      static_cast<int>(frame.GetVPitch() * ((frame.GetHeight() + 1) / 2)),
      frame.GetVPlane(),
      static_cast<int>(frame.GetWidth()),
      static_cast<int>(frame.GetHeight()),
      static_cast<int>(frame.GetYPitch()),
      static_cast<int>(frame.GetUPitch()),
      static_cast<int>(frame.GetVPitch()));
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::InputFrame(
    VideoCapturer* capturer,
    const VideoFrame* frame) {
  LOG(LS_VERBOSE) << "InputFrame: " << frame->GetWidth() << "x"
                  << frame->GetHeight();
  bool is_screencast = capturer->IsScreencast();
  // Lock before copying, can be called concurrently when swapping input source.
  talk_base::CritScope frame_cs(&frame_lock_);
  if (!muted_) {
    ConvertToI420VideoFrame(*frame, &video_frame_);
  } else {
    // Create a tiny black frame to transmit instead.
    CreateBlackFrame(&video_frame_, 1, 1);
    is_screencast = false;
  }
  talk_base::CritScope cs(&lock_);
  if (stream_ == NULL) {
    LOG(LS_WARNING) << "Capturer inputting frames before send codecs are "
                       "configured, dropping.";
    return;
  }
  if (format_.width == 0) {  // Dropping frames.
    assert(format_.height == 0);
    LOG(LS_VERBOSE) << "VideoFormat 0x0 set, Dropping frame.";
    return;
  }
  // Reconfigure codec if necessary.
  if (is_screencast) {
    SetDimensions(video_frame_.width(), video_frame_.height());
  }
  LOG(LS_VERBOSE) << "SwapFrame: " << video_frame_.width() << "x"
                  << video_frame_.height() << " -> (codec) "
                  << parameters_.video_streams.back().width << "x"
                  << parameters_.video_streams.back().height;
  stream_->Input()->SwapFrame(&video_frame_);
}

bool WebRtcVideoChannel2::WebRtcVideoSendStream::SetCapturer(
    VideoCapturer* capturer) {
  if (!DisconnectCapturer() && capturer == NULL) {
    return false;
  }

  {
    talk_base::CritScope cs(&lock_);

    if (capturer == NULL) {
      if (stream_ != NULL) {
        LOG(LS_VERBOSE) << "Disabling capturer, sending black frame.";
        webrtc::I420VideoFrame black_frame;

        int width = format_.width;
        int height = format_.height;
        int half_width = (width + 1) / 2;
        black_frame.CreateEmptyFrame(
            width, height, width, half_width, half_width);
        SetWebRtcFrameToBlack(&black_frame);
        SetDimensions(width, height);
        stream_->Input()->SwapFrame(&black_frame);
      }

      capturer_ = NULL;
      return true;
    }

    capturer_ = capturer;
  }
  // Lock cannot be held while connecting the capturer to prevent lock-order
  // violations.
  capturer->SignalVideoFrame.connect(this, &WebRtcVideoSendStream::InputFrame);
  return true;
}

bool WebRtcVideoChannel2::WebRtcVideoSendStream::SetVideoFormat(
    const VideoFormat& format) {
  if ((format.width == 0 || format.height == 0) &&
      format.width != format.height) {
    LOG(LS_ERROR) << "Can't set VideoFormat, width or height is zero (but not "
                     "both, 0x0 drops frames).";
    return false;
  }

  talk_base::CritScope cs(&lock_);
  if (format.width == 0 && format.height == 0) {
    LOG(LS_INFO)
        << "0x0 resolution selected. Captured frames will be dropped for ssrc: "
        << parameters_.config.rtp.ssrcs[0] << ".";
  } else {
    // TODO(pbos): Fix me, this only affects the last stream!
    parameters_.video_streams.back().max_framerate =
        VideoFormat::IntervalToFps(format.interval);
    SetDimensions(format.width, format.height);
  }

  format_ = format;
  return true;
}

bool WebRtcVideoChannel2::WebRtcVideoSendStream::MuteStream(bool mute) {
  talk_base::CritScope cs(&lock_);
  bool was_muted = muted_;
  muted_ = mute;
  return was_muted != mute;
}

bool WebRtcVideoChannel2::WebRtcVideoSendStream::DisconnectCapturer() {
  talk_base::CritScope cs(&lock_);
  if (capturer_ == NULL) {
    return false;
  }
  capturer_->SignalVideoFrame.disconnect(this);
  capturer_ = NULL;
  return true;
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::SetOptions(
    const VideoOptions& options) {
  talk_base::CritScope cs(&lock_);
  VideoCodecSettings codec_settings;
  if (parameters_.codec_settings.Get(&codec_settings)) {
    SetCodecAndOptions(codec_settings, options);
  } else {
    parameters_.options = options;
  }
}
void WebRtcVideoChannel2::WebRtcVideoSendStream::SetCodec(
    const VideoCodecSettings& codec_settings) {
  talk_base::CritScope cs(&lock_);
  SetCodecAndOptions(codec_settings, parameters_.options);
}
void WebRtcVideoChannel2::WebRtcVideoSendStream::SetCodecAndOptions(
    const VideoCodecSettings& codec_settings,
    const VideoOptions& options) {
  std::vector<webrtc::VideoStream> video_streams =
      encoder_factory_->CreateVideoStreams(
          codec_settings.codec, options, parameters_.config.rtp.ssrcs.size());
  if (video_streams.empty()) {
    return;
  }
  parameters_.video_streams = video_streams;
  format_ = VideoFormat(codec_settings.codec.width,
                        codec_settings.codec.height,
                        VideoFormat::FpsToInterval(30),
                        FOURCC_I420);

  webrtc::VideoEncoder* old_encoder =
      parameters_.config.encoder_settings.encoder;
  parameters_.config.encoder_settings.encoder =
      encoder_factory_->CreateVideoEncoder(codec_settings.codec, options);
  parameters_.config.encoder_settings.payload_name = codec_settings.codec.name;
  parameters_.config.encoder_settings.payload_type = codec_settings.codec.id;
  parameters_.config.rtp.fec = codec_settings.fec;

  // Set RTX payload type if RTX is enabled.
  if (!parameters_.config.rtp.rtx.ssrcs.empty()) {
    parameters_.config.rtp.rtx.payload_type = codec_settings.rtx_payload_type;

    options.use_payload_padding.Get(
        &parameters_.config.rtp.rtx.pad_with_redundant_payloads);
  }

  if (IsNackEnabled(codec_settings.codec)) {
    parameters_.config.rtp.nack.rtp_history_ms = kNackHistoryMs;
  }

  options.suspend_below_min_bitrate.Get(
      &parameters_.config.suspend_below_min_bitrate);

  parameters_.codec_settings.Set(codec_settings);
  parameters_.options = options;

  RecreateWebRtcStream();
  delete old_encoder;
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::SetRtpExtensions(
    const std::vector<webrtc::RtpExtension>& rtp_extensions) {
  talk_base::CritScope cs(&lock_);
  parameters_.config.rtp.extensions = rtp_extensions;
  RecreateWebRtcStream();
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::SetDimensions(int width,
                                                               int height) {
  assert(!parameters_.video_streams.empty());
  LOG(LS_VERBOSE) << "SetDimensions: " << width << "x" << height;
  if (parameters_.video_streams.back().width == width &&
      parameters_.video_streams.back().height == height) {
    return;
  }

  // TODO(pbos): Fix me, this only affects the last stream!
  parameters_.video_streams.back().width = width;
  parameters_.video_streams.back().height = height;

  VideoCodecSettings codec_settings;
  parameters_.codec_settings.Get(&codec_settings);
  void* encoder_settings = encoder_factory_->CreateVideoEncoderSettings(
      codec_settings.codec, parameters_.options);

  bool stream_reconfigured = stream_->ReconfigureVideoEncoder(
      parameters_.video_streams, encoder_settings);

  encoder_factory_->DestroyVideoEncoderSettings(codec_settings.codec,
                                                encoder_settings);

  if (!stream_reconfigured) {
    LOG(LS_WARNING) << "Failed to reconfigure video encoder for dimensions: "
                    << width << "x" << height;
    return;
  }
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::Start() {
  talk_base::CritScope cs(&lock_);
  assert(stream_ != NULL);
  stream_->Start();
  sending_ = true;
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::Stop() {
  talk_base::CritScope cs(&lock_);
  if (stream_ != NULL) {
    stream_->Stop();
  }
  sending_ = false;
}

VideoSenderInfo
WebRtcVideoChannel2::WebRtcVideoSendStream::GetVideoSenderInfo() {
  VideoSenderInfo info;
  talk_base::CritScope cs(&lock_);
  for (size_t i = 0; i < parameters_.config.rtp.ssrcs.size(); ++i) {
    info.add_ssrc(parameters_.config.rtp.ssrcs[i]);
  }

  webrtc::VideoSendStream::Stats stats = stream_->GetStats();
  info.framerate_input = stats.input_frame_rate;
  info.framerate_sent = stats.encode_frame_rate;

  for (std::map<uint32_t, webrtc::StreamStats>::iterator it =
           stats.substreams.begin();
       it != stats.substreams.end();
       ++it) {
    // TODO(pbos): Wire up additional stats, such as padding bytes.
    webrtc::StreamStats stream_stats = it->second;
    info.bytes_sent += stream_stats.rtp_stats.bytes +
                       stream_stats.rtp_stats.header_bytes +
                       stream_stats.rtp_stats.padding_bytes;
    info.packets_sent += stream_stats.rtp_stats.packets;
    info.packets_lost += stream_stats.rtcp_stats.cumulative_lost;
  }

  if (!stats.substreams.empty()) {
    // TODO(pbos): Report fraction lost per SSRC.
    webrtc::StreamStats first_stream_stats = stats.substreams.begin()->second;
    info.fraction_lost =
        static_cast<float>(first_stream_stats.rtcp_stats.fraction_lost) /
        (1 << 8);
  }

  if (capturer_ != NULL && !capturer_->IsMuted()) {
    VideoFormat last_captured_frame_format;
    capturer_->GetStats(&info.adapt_frame_drops,
                        &info.effects_frame_drops,
                        &info.capturer_frame_time,
                        &last_captured_frame_format);
    info.input_frame_width = last_captured_frame_format.width;
    info.input_frame_height = last_captured_frame_format.height;
    info.send_frame_width =
        static_cast<int>(parameters_.video_streams.front().width);
    info.send_frame_height =
        static_cast<int>(parameters_.video_streams.front().height);
  }

  // TODO(pbos): Support or remove the following stats.
  info.packets_cached = -1;
  info.rtt_ms = -1;

  return info;
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::RecreateWebRtcStream() {
  if (stream_ != NULL) {
    call_->DestroyVideoSendStream(stream_);
  }

  VideoCodecSettings codec_settings;
  parameters_.codec_settings.Get(&codec_settings);
  void* encoder_settings = encoder_factory_->CreateVideoEncoderSettings(
      codec_settings.codec, parameters_.options);

  stream_ = call_->CreateVideoSendStream(
      parameters_.config, parameters_.video_streams, encoder_settings);

  encoder_factory_->DestroyVideoEncoderSettings(codec_settings.codec,
                                                encoder_settings);

  if (sending_) {
    stream_->Start();
  }
}

WebRtcVideoChannel2::WebRtcVideoReceiveStream::WebRtcVideoReceiveStream(
    webrtc::Call* call,
    const webrtc::VideoReceiveStream::Config& config,
    const std::vector<VideoCodecSettings>& recv_codecs)
    : call_(call),
      config_(config),
      stream_(NULL),
      last_width_(-1),
      last_height_(-1),
      renderer_(NULL) {
  config_.renderer = this;
  // SetRecvCodecs will also reset (start) the VideoReceiveStream.
  SetRecvCodecs(recv_codecs);
}

WebRtcVideoChannel2::WebRtcVideoReceiveStream::~WebRtcVideoReceiveStream() {
  call_->DestroyVideoReceiveStream(stream_);
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::SetRecvCodecs(
    const std::vector<VideoCodecSettings>& recv_codecs) {
  // TODO(pbos): Reconfigure RTX based on incoming recv_codecs.
  // TODO(pbos): Base receive codecs off recv_codecs_ and set up using a
  // DecoderFactory similar to send side. Pending webrtc:2854.
  // Also set up default codecs if there's nothing in recv_codecs_.
  webrtc::VideoCodec codec;
  memset(&codec, 0, sizeof(codec));

  codec.plType = kDefaultVideoCodecPref.payload_type;
  strcpy(codec.plName, kDefaultVideoCodecPref.name);
  codec.codecType = webrtc::kVideoCodecVP8;
  codec.codecSpecific.VP8.resilience = webrtc::kResilientStream;
  codec.codecSpecific.VP8.numberOfTemporalLayers = 1;
  codec.codecSpecific.VP8.denoisingOn = true;
  codec.codecSpecific.VP8.errorConcealmentOn = false;
  codec.codecSpecific.VP8.automaticResizeOn = false;
  codec.codecSpecific.VP8.frameDroppingOn = true;
  codec.codecSpecific.VP8.keyFrameInterval = 3000;
  // Bitrates don't matter and are ignored for the receiver. This is put in to
  // have the current underlying implementation accept the VideoCodec.
  codec.minBitrate = codec.startBitrate = codec.maxBitrate = 300;
  config_.codecs.clear();
  config_.codecs.push_back(codec);

  config_.rtp.fec = recv_codecs.front().fec;

  RecreateWebRtcStream();
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::SetRtpExtensions(
    const std::vector<webrtc::RtpExtension>& extensions) {
  config_.rtp.extensions = extensions;
  RecreateWebRtcStream();
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::RecreateWebRtcStream() {
  if (stream_ != NULL) {
    call_->DestroyVideoReceiveStream(stream_);
  }
  stream_ = call_->CreateVideoReceiveStream(config_);
  stream_->Start();
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::RenderFrame(
    const webrtc::I420VideoFrame& frame,
    int time_to_render_ms) {
  talk_base::CritScope crit(&renderer_lock_);
  if (renderer_ == NULL) {
    LOG(LS_WARNING) << "VideoReceiveStream not connected to a VideoRenderer.";
    return;
  }

  if (frame.width() != last_width_ || frame.height() != last_height_) {
    SetSize(frame.width(), frame.height());
  }

  LOG(LS_VERBOSE) << "RenderFrame: (" << frame.width() << "x" << frame.height()
                  << ")";

  const WebRtcVideoRenderFrame render_frame(&frame);
  renderer_->RenderFrame(&render_frame);
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::SetRenderer(
    cricket::VideoRenderer* renderer) {
  talk_base::CritScope crit(&renderer_lock_);
  renderer_ = renderer;
  if (renderer_ != NULL && last_width_ != -1) {
    SetSize(last_width_, last_height_);
  }
}

VideoRenderer* WebRtcVideoChannel2::WebRtcVideoReceiveStream::GetRenderer() {
  // TODO(pbos): Remove GetRenderer and all uses of it, it's thread-unsafe by
  // design.
  talk_base::CritScope crit(&renderer_lock_);
  return renderer_;
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::SetSize(int width,
                                                            int height) {
  talk_base::CritScope crit(&renderer_lock_);
  if (!renderer_->SetSize(width, height, 0)) {
    LOG(LS_ERROR) << "Could not set renderer size.";
  }
  last_width_ = width;
  last_height_ = height;
}

VideoReceiverInfo
WebRtcVideoChannel2::WebRtcVideoReceiveStream::GetVideoReceiverInfo() {
  VideoReceiverInfo info;
  info.add_ssrc(config_.rtp.remote_ssrc);
  webrtc::VideoReceiveStream::Stats stats = stream_->GetStats();
  info.bytes_rcvd = stats.rtp_stats.bytes + stats.rtp_stats.header_bytes +
                    stats.rtp_stats.padding_bytes;
  info.packets_rcvd = stats.rtp_stats.packets;

  info.framerate_rcvd = stats.network_frame_rate;
  info.framerate_decoded = stats.decode_frame_rate;
  info.framerate_output = stats.render_frame_rate;

  talk_base::CritScope frame_cs(&renderer_lock_);
  info.frame_width = last_width_;
  info.frame_height = last_height_;

  // TODO(pbos): Support or remove the following stats.
  info.packets_concealed = -1;

  return info;
}

WebRtcVideoChannel2::VideoCodecSettings::VideoCodecSettings()
    : rtx_payload_type(-1) {}

std::vector<WebRtcVideoChannel2::VideoCodecSettings>
WebRtcVideoChannel2::MapCodecs(const std::vector<VideoCodec>& codecs) {
  assert(!codecs.empty());

  std::vector<VideoCodecSettings> video_codecs;
  std::map<int, bool> payload_used;
  std::map<int, VideoCodec::CodecType> payload_codec_type;
  std::map<int, int> rtx_mapping;  // video payload type -> rtx payload type.

  webrtc::FecConfig fec_settings;

  for (size_t i = 0; i < codecs.size(); ++i) {
    const VideoCodec& in_codec = codecs[i];
    int payload_type = in_codec.id;

    if (payload_used[payload_type]) {
      LOG(LS_ERROR) << "Payload type already registered: "
                    << in_codec.ToString();
      return std::vector<VideoCodecSettings>();
    }
    payload_used[payload_type] = true;
    payload_codec_type[payload_type] = in_codec.GetCodecType();

    switch (in_codec.GetCodecType()) {
      case VideoCodec::CODEC_RED: {
        // RED payload type, should not have duplicates.
        assert(fec_settings.red_payload_type == -1);
        fec_settings.red_payload_type = in_codec.id;
        continue;
      }

      case VideoCodec::CODEC_ULPFEC: {
        // ULPFEC payload type, should not have duplicates.
        assert(fec_settings.ulpfec_payload_type == -1);
        fec_settings.ulpfec_payload_type = in_codec.id;
        continue;
      }

      case VideoCodec::CODEC_RTX: {
        int associated_payload_type;
        if (!in_codec.GetParam(kCodecParamAssociatedPayloadType,
                               &associated_payload_type)) {
          LOG(LS_ERROR) << "RTX codec without associated payload type: "
                        << in_codec.ToString();
          return std::vector<VideoCodecSettings>();
        }
        rtx_mapping[associated_payload_type] = in_codec.id;
        continue;
      }

      case VideoCodec::CODEC_VIDEO:
        break;
    }

    video_codecs.push_back(VideoCodecSettings());
    video_codecs.back().codec = in_codec;
  }

  // One of these codecs should have been a video codec. Only having FEC
  // parameters into this code is a logic error.
  assert(!video_codecs.empty());

  for (std::map<int, int>::const_iterator it = rtx_mapping.begin();
       it != rtx_mapping.end();
       ++it) {
    if (!payload_used[it->first]) {
      LOG(LS_ERROR) << "RTX mapped to payload not in codec list.";
      return std::vector<VideoCodecSettings>();
    }
    if (payload_codec_type[it->first] != VideoCodec::CODEC_VIDEO) {
      LOG(LS_ERROR) << "RTX not mapped to regular video codec.";
      return std::vector<VideoCodecSettings>();
    }
  }

  // TODO(pbos): Write tests that figure out that I have not verified that RTX
  // codecs aren't mapped to bogus payloads.
  for (size_t i = 0; i < video_codecs.size(); ++i) {
    video_codecs[i].fec = fec_settings;
    if (rtx_mapping[video_codecs[i].codec.id] != 0) {
      video_codecs[i].rtx_payload_type = rtx_mapping[video_codecs[i].codec.id];
    }
  }

  return video_codecs;
}

std::vector<WebRtcVideoChannel2::VideoCodecSettings>
WebRtcVideoChannel2::FilterSupportedCodecs(
    const std::vector<WebRtcVideoChannel2::VideoCodecSettings>& mapped_codecs) {
  std::vector<VideoCodecSettings> supported_codecs;
  for (size_t i = 0; i < mapped_codecs.size(); ++i) {
    if (encoder_factory_->SupportsCodec(mapped_codecs[i].codec)) {
      supported_codecs.push_back(mapped_codecs[i]);
    }
  }
  return supported_codecs;
}

}  // namespace cricket

#endif  // HAVE_WEBRTC_VIDEO
