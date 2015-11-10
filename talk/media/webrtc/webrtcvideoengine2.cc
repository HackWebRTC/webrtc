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

#include <algorithm>
#include <set>
#include <string>

#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videorenderer.h"
#include "talk/media/webrtc/constants.h"
#include "talk/media/webrtc/simulcast.h"
#include "talk/media/webrtc/webrtcmediaengine.h"
#include "talk/media/webrtc/webrtcvideoencoderfactory.h"
#include "talk/media/webrtc/webrtcvideoframe.h"
#include "talk/media/webrtc/webrtcvoiceengine.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/call.h"
#include "webrtc/modules/video_coding/codecs/h264/include/h264.h"
#include "webrtc/modules/video_coding/codecs/vp8/simulcast_encoder_adapter.h"
#include "webrtc/system_wrappers/include/field_trial.h"
#include "webrtc/video_decoder.h"
#include "webrtc/video_encoder.h"

namespace cricket {
namespace {

// Wrap cricket::WebRtcVideoEncoderFactory as a webrtc::VideoEncoderFactory.
class EncoderFactoryAdapter : public webrtc::VideoEncoderFactory {
 public:
  // EncoderFactoryAdapter doesn't take ownership of |factory|, which is owned
  // by e.g. PeerConnectionFactory.
  explicit EncoderFactoryAdapter(cricket::WebRtcVideoEncoderFactory* factory)
      : factory_(factory) {}
  virtual ~EncoderFactoryAdapter() {}

  // Implement webrtc::VideoEncoderFactory.
  webrtc::VideoEncoder* Create() override {
    return factory_->CreateVideoEncoder(webrtc::kVideoCodecVP8);
  }

  void Destroy(webrtc::VideoEncoder* encoder) override {
    return factory_->DestroyVideoEncoder(encoder);
  }

 private:
  cricket::WebRtcVideoEncoderFactory* const factory_;
};

// An encoder factory that wraps Create requests for simulcastable codec types
// with a webrtc::SimulcastEncoderAdapter. Non simulcastable codec type
// requests are just passed through to the contained encoder factory.
class WebRtcSimulcastEncoderFactory
    : public cricket::WebRtcVideoEncoderFactory {
 public:
  // WebRtcSimulcastEncoderFactory doesn't take ownership of |factory|, which is
  // owned by e.g. PeerConnectionFactory.
  explicit WebRtcSimulcastEncoderFactory(
      cricket::WebRtcVideoEncoderFactory* factory)
      : factory_(factory) {}

  static bool UseSimulcastEncoderFactory(
      const std::vector<VideoCodec>& codecs) {
    // If any codec is VP8, use the simulcast factory. If asked to create a
    // non-VP8 codec, we'll just return a contained factory encoder directly.
    for (const auto& codec : codecs) {
      if (codec.type == webrtc::kVideoCodecVP8) {
        return true;
      }
    }
    return false;
  }

  webrtc::VideoEncoder* CreateVideoEncoder(
      webrtc::VideoCodecType type) override {
    RTC_DCHECK(factory_ != NULL);
    // If it's a codec type we can simulcast, create a wrapped encoder.
    if (type == webrtc::kVideoCodecVP8) {
      return new webrtc::SimulcastEncoderAdapter(
          new EncoderFactoryAdapter(factory_));
    }
    webrtc::VideoEncoder* encoder = factory_->CreateVideoEncoder(type);
    if (encoder) {
      non_simulcast_encoders_.push_back(encoder);
    }
    return encoder;
  }

  const std::vector<VideoCodec>& codecs() const override {
    return factory_->codecs();
  }

  bool EncoderTypeHasInternalSource(
      webrtc::VideoCodecType type) const override {
    return factory_->EncoderTypeHasInternalSource(type);
  }

  void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) override {
    // Check first to see if the encoder wasn't wrapped in a
    // SimulcastEncoderAdapter. In that case, ask the factory to destroy it.
    if (std::remove(non_simulcast_encoders_.begin(),
                    non_simulcast_encoders_.end(),
                    encoder) != non_simulcast_encoders_.end()) {
      factory_->DestroyVideoEncoder(encoder);
      return;
    }

    // Otherwise, SimulcastEncoderAdapter can be deleted directly, and will call
    // DestroyVideoEncoder on the factory for individual encoder instances.
    delete encoder;
  }

 private:
  cricket::WebRtcVideoEncoderFactory* factory_;
  // A list of encoders that were created without being wrapped in a
  // SimulcastEncoderAdapter.
  std::vector<webrtc::VideoEncoder*> non_simulcast_encoders_;
};

bool CodecIsInternallySupported(const std::string& codec_name) {
  if (CodecNamesEq(codec_name, kVp8CodecName)) {
    return true;
  }
  if (CodecNamesEq(codec_name, kVp9CodecName)) {
    return true;
  }
  if (CodecNamesEq(codec_name, kH264CodecName)) {
    return webrtc::H264Encoder::IsSupported() &&
        webrtc::H264Decoder::IsSupported();
  }
  return false;
}

void AddDefaultFeedbackParams(VideoCodec* codec) {
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamCcm, kRtcpFbCcmParamFir));
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamNack, kParamValueEmpty));
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamNack, kRtcpFbNackParamPli));
  codec->AddFeedbackParam(FeedbackParam(kRtcpFbParamRemb, kParamValueEmpty));
}

static VideoCodec MakeVideoCodecWithDefaultFeedbackParams(int payload_type,
                                                          const char* name) {
  VideoCodec codec(payload_type, name, kDefaultVideoMaxWidth,
                   kDefaultVideoMaxHeight, kDefaultVideoMaxFramerate, 0);
  AddDefaultFeedbackParams(&codec);
  return codec;
}

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

static bool ValidateStreamParams(const StreamParams& sp) {
  if (sp.ssrcs.empty()) {
    LOG(LS_ERROR) << "No SSRCs in stream parameters: " << sp.ToString();
    return false;
  }

  std::vector<uint32_t> primary_ssrcs;
  sp.GetPrimarySsrcs(&primary_ssrcs);
  std::vector<uint32_t> rtx_ssrcs;
  sp.GetFidSsrcs(primary_ssrcs, &rtx_ssrcs);
  for (uint32_t rtx_ssrc : rtx_ssrcs) {
    bool rtx_ssrc_present = false;
    for (uint32_t sp_ssrc : sp.ssrcs) {
      if (sp_ssrc == rtx_ssrc) {
        rtx_ssrc_present = true;
        break;
      }
    }
    if (!rtx_ssrc_present) {
      LOG(LS_ERROR) << "RTX SSRC '" << rtx_ssrc
                    << "' missing from StreamParams ssrcs: " << sp.ToString();
      return false;
    }
  }
  if (!rtx_ssrcs.empty() && primary_ssrcs.size() != rtx_ssrcs.size()) {
    LOG(LS_ERROR)
        << "RTX SSRCs exist, but don't cover all SSRCs (unsupported): "
        << sp.ToString();
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

inline const webrtc::RtpExtension* FindHeaderExtension(
    const std::vector<webrtc::RtpExtension>& extensions,
    const std::string& name) {
  for (const auto& kv : extensions) {
    if (kv.name == name) {
      return &kv;
    }
  }
  return NULL;
}

// Merges two fec configs and logs an error if a conflict arises
// such that merging in different order would trigger a different output.
static void MergeFecConfig(const webrtc::FecConfig& other,
                           webrtc::FecConfig* output) {
  if (other.ulpfec_payload_type != -1) {
    if (output->ulpfec_payload_type != -1 &&
        output->ulpfec_payload_type != other.ulpfec_payload_type) {
      LOG(LS_WARNING) << "Conflict merging ulpfec_payload_type configs: "
                      << output->ulpfec_payload_type << " and "
                      << other.ulpfec_payload_type;
    }
    output->ulpfec_payload_type = other.ulpfec_payload_type;
  }
  if (other.red_payload_type != -1) {
    if (output->red_payload_type != -1 &&
        output->red_payload_type != other.red_payload_type) {
      LOG(LS_WARNING) << "Conflict merging red_payload_type configs: "
                      << output->red_payload_type << " and "
                      << other.red_payload_type;
    }
    output->red_payload_type = other.red_payload_type;
  }
  if (other.red_rtx_payload_type != -1) {
    if (output->red_rtx_payload_type != -1 &&
        output->red_rtx_payload_type != other.red_rtx_payload_type) {
      LOG(LS_WARNING) << "Conflict merging red_rtx_payload_type configs: "
                      << output->red_rtx_payload_type << " and "
                      << other.red_rtx_payload_type;
    }
    output->red_rtx_payload_type = other.red_rtx_payload_type;
  }
}

// Returns true if the given codec is disallowed from doing simulcast.
bool IsCodecBlacklistedForSimulcast(const std::string& codec_name) {
  return CodecNamesEq(codec_name, kH264CodecName) ||
         CodecNamesEq(codec_name, kVp9CodecName);
}

// The selected thresholds for QVGA and VGA corresponded to a QP around 10.
// The change in QP declined above the selected bitrates.
static int GetMaxDefaultVideoBitrateKbps(int width, int height) {
  if (width * height <= 320 * 240) {
    return 600;
  } else if (width * height <= 640 * 480) {
    return 1700;
  } else if (width * height <= 960 * 540) {
    return 2000;
  } else {
    return 2500;
  }
}
}  // namespace

// Constants defined in talk/media/webrtc/constants.h
// TODO(pbos): Move these to a separate constants.cc file.
const int kMinVideoBitrate = 30;
const int kStartVideoBitrate = 300;

const int kVideoMtu = 1200;
const int kVideoRtpBufferSize = 65536;

// This constant is really an on/off, lower-level configurable NACK history
// duration hasn't been implemented.
static const int kNackHistoryMs = 1000;

static const int kDefaultQpMax = 56;

static const int kDefaultRtcpReceiverReportSsrc = 1;

std::vector<VideoCodec> DefaultVideoCodecList() {
  std::vector<VideoCodec> codecs;
  codecs.push_back(MakeVideoCodecWithDefaultFeedbackParams(kDefaultVp8PlType,
                                                           kVp8CodecName));
  if (CodecIsInternallySupported(kVp9CodecName)) {
    codecs.push_back(MakeVideoCodecWithDefaultFeedbackParams(kDefaultVp9PlType,
                                                             kVp9CodecName));
    // TODO(andresp): Add rtx codec for vp9 and verify it works.
  }
  if (CodecIsInternallySupported(kH264CodecName)) {
    codecs.push_back(MakeVideoCodecWithDefaultFeedbackParams(kDefaultH264PlType,
                                                             kH264CodecName));
  }
  codecs.push_back(
      VideoCodec::CreateRtxCodec(kDefaultRtxVp8PlType, kDefaultVp8PlType));
  codecs.push_back(VideoCodec(kDefaultRedPlType, kRedCodecName));
  codecs.push_back(VideoCodec(kDefaultUlpfecType, kUlpfecCodecName));
  return codecs;
}

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

static bool ValidateRtpHeaderExtensionIds(
    const std::vector<RtpHeaderExtension>& extensions) {
  std::set<int> extensions_used;
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (extensions[i].id <= 0 || extensions[i].id >= 15 ||
        !extensions_used.insert(extensions[i].id).second) {
      LOG(LS_ERROR) << "RTP extensions are with incorrect or duplicate ids.";
      return false;
    }
  }
  return true;
}

static bool CompareRtpHeaderExtensionIds(
    const webrtc::RtpExtension& extension1,
    const webrtc::RtpExtension& extension2) {
  // Sorting on ID is sufficient, more than one extension per ID is unsupported.
  return extension1.id > extension2.id;
}

static std::vector<webrtc::RtpExtension> FilterRtpExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  std::vector<webrtc::RtpExtension> webrtc_extensions;
  for (size_t i = 0; i < extensions.size(); ++i) {
    // Unsupported extensions will be ignored.
    if (webrtc::RtpExtension::IsSupportedForVideo(extensions[i].uri)) {
      webrtc_extensions.push_back(webrtc::RtpExtension(
          extensions[i].uri, extensions[i].id));
    } else {
      LOG(LS_WARNING) << "Unsupported RTP extension: " << extensions[i].uri;
    }
  }

  // Sort filtered headers to make sure that they can later be compared
  // regardless of in which order they were entered.
  std::sort(webrtc_extensions.begin(), webrtc_extensions.end(),
            CompareRtpHeaderExtensionIds);
  return webrtc_extensions;
}

static bool RtpExtensionsHaveChanged(
    const std::vector<webrtc::RtpExtension>& before,
    const std::vector<webrtc::RtpExtension>& after) {
  if (before.size() != after.size())
    return true;
  for (size_t i = 0; i < before.size(); ++i) {
    if (before[i].id != after[i].id)
      return true;
    if (before[i].name != after[i].name)
      return true;
  }
  return false;
}

std::vector<webrtc::VideoStream>
WebRtcVideoChannel2::WebRtcVideoSendStream::CreateSimulcastVideoStreams(
    const VideoCodec& codec,
    const VideoOptions& options,
    int max_bitrate_bps,
    size_t num_streams) {
  int max_qp = kDefaultQpMax;
  codec.GetParam(kCodecParamMaxQuantization, &max_qp);

  return GetSimulcastConfig(
      num_streams, codec.width, codec.height, max_bitrate_bps, max_qp,
      codec.framerate != 0 ? codec.framerate : kDefaultVideoMaxFramerate);
}

std::vector<webrtc::VideoStream>
WebRtcVideoChannel2::WebRtcVideoSendStream::CreateVideoStreams(
    const VideoCodec& codec,
    const VideoOptions& options,
    int max_bitrate_bps,
    size_t num_streams) {
  int codec_max_bitrate_kbps;
  if (codec.GetParam(kCodecParamMaxBitrate, &codec_max_bitrate_kbps)) {
    max_bitrate_bps = codec_max_bitrate_kbps * 1000;
  }
  if (num_streams != 1) {
    return CreateSimulcastVideoStreams(codec, options, max_bitrate_bps,
                                       num_streams);
  }

  // For unset max bitrates set default bitrate for non-simulcast.
  if (max_bitrate_bps <= 0) {
    max_bitrate_bps =
        GetMaxDefaultVideoBitrateKbps(codec.width, codec.height) * 1000;
  }

  webrtc::VideoStream stream;
  stream.width = codec.width;
  stream.height = codec.height;
  stream.max_framerate =
      codec.framerate != 0 ? codec.framerate : kDefaultVideoMaxFramerate;

  stream.min_bitrate_bps = kMinVideoBitrate * 1000;
  stream.target_bitrate_bps = stream.max_bitrate_bps = max_bitrate_bps;

  int max_qp = kDefaultQpMax;
  codec.GetParam(kCodecParamMaxQuantization, &max_qp);
  stream.max_qp = max_qp;
  std::vector<webrtc::VideoStream> streams;
  streams.push_back(stream);
  return streams;
}

void* WebRtcVideoChannel2::WebRtcVideoSendStream::ConfigureVideoEncoderSettings(
    const VideoCodec& codec,
    const VideoOptions& options,
    bool is_screencast) {
  // No automatic resizing when using simulcast or screencast.
  bool automatic_resize =
      !is_screencast && parameters_.config.rtp.ssrcs.size() == 1;
  bool frame_dropping = !is_screencast;
  bool denoising;
  bool codec_default_denoising = false;
  if (is_screencast) {
    denoising = false;
  } else {
    // Use codec default if video_noise_reduction is unset.
    codec_default_denoising = !options.video_noise_reduction;
    denoising = options.video_noise_reduction.value_or(false);
  }

  if (CodecNamesEq(codec.name, kVp8CodecName)) {
    encoder_settings_.vp8 = webrtc::VideoEncoder::GetDefaultVp8Settings();
    encoder_settings_.vp8.automaticResizeOn = automatic_resize;
    // VP8 denoising is enabled by default.
    encoder_settings_.vp8.denoisingOn =
        codec_default_denoising ? true : denoising;
    encoder_settings_.vp8.frameDroppingOn = frame_dropping;
    return &encoder_settings_.vp8;
  }
  if (CodecNamesEq(codec.name, kVp9CodecName)) {
    encoder_settings_.vp9 = webrtc::VideoEncoder::GetDefaultVp9Settings();
    // VP9 denoising is disabled by default.
    encoder_settings_.vp9.denoisingOn =
        codec_default_denoising ? false : denoising;
    encoder_settings_.vp9.frameDroppingOn = frame_dropping;
    return &encoder_settings_.vp9;
  }
  return NULL;
}

DefaultUnsignalledSsrcHandler::DefaultUnsignalledSsrcHandler()
    : default_recv_ssrc_(0), default_renderer_(NULL) {}

UnsignalledSsrcHandler::Action DefaultUnsignalledSsrcHandler::OnUnsignalledSsrc(
    WebRtcVideoChannel2* channel,
    uint32_t ssrc) {
  if (default_recv_ssrc_ != 0) {  // Already one default stream.
    LOG(LS_WARNING) << "Unknown SSRC, but default receive stream already set.";
    return kDropPacket;
  }

  StreamParams sp;
  sp.ssrcs.push_back(ssrc);
  LOG(LS_INFO) << "Creating default receive stream for SSRC=" << ssrc << ".";
  if (!channel->AddRecvStream(sp, true)) {
    LOG(LS_WARNING) << "Could not create default receive stream.";
  }

  channel->SetRenderer(ssrc, default_renderer_);
  default_recv_ssrc_ = ssrc;
  return kDeliverPacket;
}

VideoRenderer* DefaultUnsignalledSsrcHandler::GetDefaultRenderer() const {
  return default_renderer_;
}

void DefaultUnsignalledSsrcHandler::SetDefaultRenderer(
    VideoMediaChannel* channel,
    VideoRenderer* renderer) {
  default_renderer_ = renderer;
  if (default_recv_ssrc_ != 0) {
    channel->SetRenderer(default_recv_ssrc_, default_renderer_);
  }
}

WebRtcVideoEngine2::WebRtcVideoEngine2()
    : initialized_(false),
      external_decoder_factory_(NULL),
      external_encoder_factory_(NULL) {
  LOG(LS_INFO) << "WebRtcVideoEngine2::WebRtcVideoEngine2()";
  video_codecs_ = GetSupportedCodecs();
  rtp_header_extensions_.push_back(
      RtpHeaderExtension(kRtpTimestampOffsetHeaderExtension,
                         kRtpTimestampOffsetHeaderExtensionDefaultId));
  rtp_header_extensions_.push_back(
      RtpHeaderExtension(kRtpAbsoluteSenderTimeHeaderExtension,
                         kRtpAbsoluteSenderTimeHeaderExtensionDefaultId));
  rtp_header_extensions_.push_back(
      RtpHeaderExtension(kRtpVideoRotationHeaderExtension,
                         kRtpVideoRotationHeaderExtensionDefaultId));
  if (webrtc::field_trial::FindFullName("WebRTC-SendSideBwe") == "Enabled") {
    rtp_header_extensions_.push_back(RtpHeaderExtension(
        kRtpTransportSequenceNumberHeaderExtension,
        kRtpTransportSequenceNumberHeaderExtensionDefaultId));
  }
}

WebRtcVideoEngine2::~WebRtcVideoEngine2() {
  LOG(LS_INFO) << "WebRtcVideoEngine2::~WebRtcVideoEngine2";
}

void WebRtcVideoEngine2::Init() {
  LOG(LS_INFO) << "WebRtcVideoEngine2::Init";
  initialized_ = true;
}

bool WebRtcVideoEngine2::SetDefaultEncoderConfig(
    const VideoEncoderConfig& config) {
  const VideoCodec& codec = config.max_codec;
  bool supports_codec = false;
  for (size_t i = 0; i < video_codecs_.size(); ++i) {
    if (CodecNamesEq(video_codecs_[i].name, codec.name)) {
      video_codecs_[i].width = codec.width;
      video_codecs_[i].height = codec.height;
      video_codecs_[i].framerate = codec.framerate;
      supports_codec = true;
      break;
    }
  }

  if (!supports_codec) {
    LOG(LS_ERROR) << "SetDefaultEncoderConfig, codec not supported: "
                  << codec.ToString();
    return false;
  }

  return true;
}

WebRtcVideoChannel2* WebRtcVideoEngine2::CreateChannel(
    webrtc::Call* call,
    const VideoOptions& options) {
  RTC_DCHECK(initialized_);
  LOG(LS_INFO) << "CreateChannel. Options: " << options.ToString();
  return new WebRtcVideoChannel2(call, options, video_codecs_,
      external_encoder_factory_, external_decoder_factory_);
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
    RTC_DCHECK(min_sev == -1);
    return;
  }
}

void WebRtcVideoEngine2::SetExternalDecoderFactory(
    WebRtcVideoDecoderFactory* decoder_factory) {
  RTC_DCHECK(!initialized_);
  external_decoder_factory_ = decoder_factory;
}

void WebRtcVideoEngine2::SetExternalEncoderFactory(
    WebRtcVideoEncoderFactory* encoder_factory) {
  RTC_DCHECK(!initialized_);
  if (external_encoder_factory_ == encoder_factory)
    return;

  // No matter what happens we shouldn't hold on to a stale
  // WebRtcSimulcastEncoderFactory.
  simulcast_encoder_factory_.reset();

  if (encoder_factory &&
      WebRtcSimulcastEncoderFactory::UseSimulcastEncoderFactory(
          encoder_factory->codecs())) {
    simulcast_encoder_factory_.reset(
        new WebRtcSimulcastEncoderFactory(encoder_factory));
    encoder_factory = simulcast_encoder_factory_.get();
  }
  external_encoder_factory_ = encoder_factory;

  video_codecs_ = GetSupportedCodecs();
}

bool WebRtcVideoEngine2::EnableTimedRender() {
  // TODO(pbos): Figure out whether this can be removed.
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
  RTC_DCHECK(out != NULL);

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
  out->framerate = std::min(requested.framerate, matching_codec.framerate);
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

std::vector<VideoCodec> WebRtcVideoEngine2::GetSupportedCodecs() const {
  std::vector<VideoCodec> supported_codecs = DefaultVideoCodecList();

  if (external_encoder_factory_ == NULL) {
    return supported_codecs;
  }

  const std::vector<WebRtcVideoEncoderFactory::VideoCodec>& codecs =
      external_encoder_factory_->codecs();
  for (size_t i = 0; i < codecs.size(); ++i) {
    // Don't add internally-supported codecs twice.
    if (CodecIsInternallySupported(codecs[i].name)) {
      continue;
    }

    // External video encoders are given payloads 120-127. This also means that
    // we only support up to 8 external payload types.
    const int kExternalVideoPayloadTypeBase = 120;
    size_t payload_type = kExternalVideoPayloadTypeBase + i;
    RTC_DCHECK(payload_type < 128);
    VideoCodec codec(static_cast<int>(payload_type),
                     codecs[i].name,
                     codecs[i].max_width,
                     codecs[i].max_height,
                     codecs[i].max_fps,
                     0);

    AddDefaultFeedbackParams(&codec);
    supported_codecs.push_back(codec);
  }
  return supported_codecs;
}

WebRtcVideoChannel2::WebRtcVideoChannel2(
    webrtc::Call* call,
    const VideoOptions& options,
    const std::vector<VideoCodec>& recv_codecs,
    WebRtcVideoEncoderFactory* external_encoder_factory,
    WebRtcVideoDecoderFactory* external_decoder_factory)
    : call_(call),
      unsignalled_ssrc_handler_(&default_unsignalled_ssrc_handler_),
      external_encoder_factory_(external_encoder_factory),
      external_decoder_factory_(external_decoder_factory) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  SetDefaultOptions();
  options_.SetAll(options);
  if (options_.cpu_overuse_detection)
    signal_cpu_adaptation_ = *options_.cpu_overuse_detection;
  rtcp_receiver_report_ssrc_ = kDefaultRtcpReceiverReportSsrc;
  sending_ = false;
  default_send_ssrc_ = 0;
  SetRecvCodecs(recv_codecs);
}

void WebRtcVideoChannel2::SetDefaultOptions() {
  options_.cpu_overuse_detection = rtc::Maybe<bool>(true);
  options_.dscp = rtc::Maybe<bool>(false);
  options_.suspend_below_min_bitrate = rtc::Maybe<bool>(false);
  options_.screencast_min_bitrate = rtc::Maybe<int>(0);
}

WebRtcVideoChannel2::~WebRtcVideoChannel2() {
  for (auto& kv : send_streams_)
    delete kv.second;
  for (auto& kv : receive_streams_)
    delete kv.second;
}

bool WebRtcVideoChannel2::CodecIsExternallySupported(
    const std::string& name) const {
  if (external_encoder_factory_ == NULL) {
    return false;
  }

  const std::vector<WebRtcVideoEncoderFactory::VideoCodec> external_codecs =
      external_encoder_factory_->codecs();
  for (size_t c = 0; c < external_codecs.size(); ++c) {
    if (CodecNamesEq(name, external_codecs[c].name)) {
      return true;
    }
  }
  return false;
}

std::vector<WebRtcVideoChannel2::VideoCodecSettings>
WebRtcVideoChannel2::FilterSupportedCodecs(
    const std::vector<WebRtcVideoChannel2::VideoCodecSettings>& mapped_codecs)
    const {
  std::vector<VideoCodecSettings> supported_codecs;
  for (size_t i = 0; i < mapped_codecs.size(); ++i) {
    const VideoCodecSettings& codec = mapped_codecs[i];
    if (CodecIsInternallySupported(codec.codec.name) ||
        CodecIsExternallySupported(codec.codec.name)) {
      supported_codecs.push_back(codec);
    }
  }
  return supported_codecs;
}

bool WebRtcVideoChannel2::ReceiveCodecsHaveChanged(
    std::vector<VideoCodecSettings> before,
    std::vector<VideoCodecSettings> after) {
  if (before.size() != after.size()) {
    return true;
  }
  // The receive codec order doesn't matter, so we sort the codecs before
  // comparing. This is necessary because currently the
  // only way to change the send codec is to munge SDP, which causes
  // the receive codec list to change order, which causes the streams
  // to be recreates which causes a "blink" of black video.  In order
  // to support munging the SDP in this way without recreating receive
  // streams, we ignore the order of the received codecs so that
  // changing the order doesn't cause this "blink".
  auto comparison =
      [](const VideoCodecSettings& codec1, const VideoCodecSettings& codec2) {
        return codec1.codec.id > codec2.codec.id;
      };
  std::sort(before.begin(), before.end(), comparison);
  std::sort(after.begin(), after.end(), comparison);
  for (size_t i = 0; i < before.size(); ++i) {
    // For the same reason that we sort the codecs, we also ignore the
    // preference.  We don't want a preference change on the receive
    // side to cause recreation of the stream.
    before[i].codec.preference = 0;
    after[i].codec.preference = 0;
    if (before[i] != after[i]) {
      return true;
    }
  }
  return false;
}

bool WebRtcVideoChannel2::SetSendParameters(const VideoSendParameters& params) {
  // TODO(pbos): Refactor this to only recreate the send streams once
  // instead of 4 times.
  return (SetSendCodecs(params.codecs) &&
          SetSendRtpHeaderExtensions(params.extensions) &&
          SetMaxSendBandwidth(params.max_bandwidth_bps) &&
          SetOptions(params.options));
}

bool WebRtcVideoChannel2::SetRecvParameters(const VideoRecvParameters& params) {
  // TODO(pbos): Refactor this to only recreate the recv streams once
  // instead of twice.
  return (SetRecvCodecs(params.codecs) &&
          SetRecvRtpHeaderExtensions(params.extensions));
}

std::string WebRtcVideoChannel2::CodecSettingsVectorToString(
    const std::vector<VideoCodecSettings>& codecs) {
  std::stringstream out;
  out << '{';
  for (size_t i = 0; i < codecs.size(); ++i) {
    out << codecs[i].codec.ToString();
    if (i != codecs.size() - 1) {
      out << ", ";
    }
  }
  out << '}';
  return out.str();
}

bool WebRtcVideoChannel2::SetRecvCodecs(const std::vector<VideoCodec>& codecs) {
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel2::SetRecvCodecs");
  LOG(LS_INFO) << "SetRecvCodecs: " << CodecVectorToString(codecs);
  if (!ValidateCodecFormats(codecs)) {
    return false;
  }

  const std::vector<VideoCodecSettings> mapped_codecs = MapCodecs(codecs);
  if (mapped_codecs.empty()) {
    LOG(LS_ERROR) << "SetRecvCodecs called without any video codecs.";
    return false;
  }

  std::vector<VideoCodecSettings> supported_codecs =
      FilterSupportedCodecs(mapped_codecs);

  if (mapped_codecs.size() != supported_codecs.size()) {
    LOG(LS_ERROR) << "SetRecvCodecs called with unsupported video codecs.";
    return false;
  }

  // Prevent reconfiguration when setting identical receive codecs.
  if (!ReceiveCodecsHaveChanged(recv_codecs_, supported_codecs)) {
    LOG(LS_INFO)
        << "Ignoring call to SetRecvCodecs because codecs haven't changed.";
    return true;
  }

  LOG(LS_INFO) << "Changing recv codecs from "
               << CodecSettingsVectorToString(recv_codecs_) << " to "
               << CodecSettingsVectorToString(supported_codecs);
  recv_codecs_ = supported_codecs;

  rtc::CritScope stream_lock(&stream_crit_);
  for (std::map<uint32_t, WebRtcVideoReceiveStream*>::iterator it =
           receive_streams_.begin();
       it != receive_streams_.end(); ++it) {
    it->second->SetRecvCodecs(recv_codecs_);
  }

  return true;
}

bool WebRtcVideoChannel2::SetSendCodecs(const std::vector<VideoCodec>& codecs) {
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel2::SetSendCodecs");
  LOG(LS_INFO) << "SetSendCodecs: " << CodecVectorToString(codecs);
  if (!ValidateCodecFormats(codecs)) {
    return false;
  }

  const std::vector<VideoCodecSettings> supported_codecs =
      FilterSupportedCodecs(MapCodecs(codecs));

  if (supported_codecs.empty()) {
    LOG(LS_ERROR) << "No video codecs supported.";
    return false;
  }

  LOG(LS_INFO) << "Using codec: " << supported_codecs.front().codec.ToString();

  if (send_codec_ && supported_codecs.front() == *send_codec_) {
    LOG(LS_INFO) << "Ignore call to SetSendCodecs because first supported "
                    "codec hasn't changed.";
    // Using same codec, avoid reconfiguring.
    return true;
  }

  send_codec_ = rtc::Maybe<WebRtcVideoChannel2::VideoCodecSettings>(
      supported_codecs.front());

  rtc::CritScope stream_lock(&stream_crit_);
  LOG(LS_INFO) << "Change the send codec because SetSendCodecs has a different "
                  "first supported codec.";
  for (auto& kv : send_streams_) {
    RTC_DCHECK(kv.second != nullptr);
    kv.second->SetCodec(supported_codecs.front());
  }
  LOG(LS_INFO) << "SetNackAndRemb on all the receive streams because the send "
                  "codec has changed.";
  for (auto& kv : receive_streams_) {
    RTC_DCHECK(kv.second != nullptr);
    kv.second->SetNackAndRemb(HasNack(supported_codecs.front().codec),
                              HasRemb(supported_codecs.front().codec));
  }

  // TODO(holmer): Changing the codec parameters shouldn't necessarily mean that
  // we change the min/max of bandwidth estimation. Reevaluate this.
  VideoCodec codec = supported_codecs.front().codec;
  int bitrate_kbps;
  if (codec.GetParam(kCodecParamMinBitrate, &bitrate_kbps) &&
      bitrate_kbps > 0) {
    bitrate_config_.min_bitrate_bps = bitrate_kbps * 1000;
  } else {
    bitrate_config_.min_bitrate_bps = 0;
  }
  if (codec.GetParam(kCodecParamStartBitrate, &bitrate_kbps) &&
      bitrate_kbps > 0) {
    bitrate_config_.start_bitrate_bps = bitrate_kbps * 1000;
  } else {
    // Do not reconfigure start bitrate unless it's specified and positive.
    bitrate_config_.start_bitrate_bps = -1;
  }
  if (codec.GetParam(kCodecParamMaxBitrate, &bitrate_kbps) &&
      bitrate_kbps > 0) {
    bitrate_config_.max_bitrate_bps = bitrate_kbps * 1000;
  } else {
    bitrate_config_.max_bitrate_bps = -1;
  }
  call_->SetBitrateConfig(bitrate_config_);

  return true;
}

bool WebRtcVideoChannel2::GetSendCodec(VideoCodec* codec) {
  if (!send_codec_) {
    LOG(LS_VERBOSE) << "GetSendCodec: No send codec set.";
    return false;
  }
  *codec = send_codec_->codec;
  return true;
}

bool WebRtcVideoChannel2::SetSendStreamFormat(uint32_t ssrc,
                                              const VideoFormat& format) {
  LOG(LS_VERBOSE) << "SetSendStreamFormat:" << ssrc << " -> "
                  << format.ToString();
  rtc::CritScope stream_lock(&stream_crit_);
  if (send_streams_.find(ssrc) == send_streams_.end()) {
    return false;
  }
  return send_streams_[ssrc]->SetVideoFormat(format);
}

bool WebRtcVideoChannel2::SetSend(bool send) {
  LOG(LS_VERBOSE) << "SetSend: " << (send ? "true" : "false");
  if (send && !send_codec_) {
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

bool WebRtcVideoChannel2::SetVideoSend(uint32_t ssrc, bool enable,
                                       const VideoOptions* options) {
  // TODO(solenberg): The state change should be fully rolled back if any one of
  //                  these calls fail.
  if (!MuteStream(ssrc, !enable)) {
    return false;
  }
  if (enable && options) {
    return SetOptions(*options);
  } else {
    return true;
  }
}

bool WebRtcVideoChannel2::ValidateSendSsrcAvailability(
    const StreamParams& sp) const {
  for (uint32_t ssrc: sp.ssrcs) {
    if (send_ssrcs_.find(ssrc) != send_ssrcs_.end()) {
      LOG(LS_ERROR) << "Send stream with SSRC '" << ssrc << "' already exists.";
      return false;
    }
  }
  return true;
}

bool WebRtcVideoChannel2::ValidateReceiveSsrcAvailability(
    const StreamParams& sp) const {
  for (uint32_t ssrc: sp.ssrcs) {
    if (receive_ssrcs_.find(ssrc) != receive_ssrcs_.end()) {
      LOG(LS_ERROR) << "Receive stream with SSRC '" << ssrc
                    << "' already exists.";
      return false;
    }
  }
  return true;
}

bool WebRtcVideoChannel2::AddSendStream(const StreamParams& sp) {
  LOG(LS_INFO) << "AddSendStream: " << sp.ToString();
  if (!ValidateStreamParams(sp))
    return false;

  rtc::CritScope stream_lock(&stream_crit_);

  if (!ValidateSendSsrcAvailability(sp))
    return false;

  for (uint32_t used_ssrc : sp.ssrcs)
    send_ssrcs_.insert(used_ssrc);

  webrtc::VideoSendStream::Config config(this);
  config.overuse_callback = this;

  WebRtcVideoSendStream* stream =
      new WebRtcVideoSendStream(call_,
                                sp,
                                config,
                                external_encoder_factory_,
                                options_,
                                bitrate_config_.max_bitrate_bps,
                                send_codec_,
                                send_rtp_extensions_);

  uint32_t ssrc = sp.first_ssrc();
  RTC_DCHECK(ssrc != 0);
  send_streams_[ssrc] = stream;

  if (rtcp_receiver_report_ssrc_ == kDefaultRtcpReceiverReportSsrc) {
    rtcp_receiver_report_ssrc_ = ssrc;
    LOG(LS_INFO) << "SetLocalSsrc on all the receive streams because we added "
                    "a send stream.";
    for (auto& kv : receive_streams_)
      kv.second->SetLocalSsrc(ssrc);
  }
  if (default_send_ssrc_ == 0) {
    default_send_ssrc_ = ssrc;
  }
  if (sending_) {
    stream->Start();
  }

  return true;
}

bool WebRtcVideoChannel2::RemoveSendStream(uint32_t ssrc) {
  LOG(LS_INFO) << "RemoveSendStream: " << ssrc;

  if (ssrc == 0) {
    if (default_send_ssrc_ == 0) {
      LOG(LS_ERROR) << "No default send stream active.";
      return false;
    }

    LOG(LS_VERBOSE) << "Removing default stream: " << default_send_ssrc_;
    ssrc = default_send_ssrc_;
  }

  WebRtcVideoSendStream* removed_stream;
  {
    rtc::CritScope stream_lock(&stream_crit_);
    std::map<uint32_t, WebRtcVideoSendStream*>::iterator it =
        send_streams_.find(ssrc);
    if (it == send_streams_.end()) {
      return false;
    }

    for (uint32_t old_ssrc : it->second->GetSsrcs())
      send_ssrcs_.erase(old_ssrc);

    removed_stream = it->second;
    send_streams_.erase(it);

    // Switch receiver report SSRCs, the one in use is no longer valid.
    if (rtcp_receiver_report_ssrc_ == ssrc) {
      rtcp_receiver_report_ssrc_ = send_streams_.empty()
                                       ? kDefaultRtcpReceiverReportSsrc
                                       : send_streams_.begin()->first;
      LOG(LS_INFO) << "SetLocalSsrc on all the receive streams because the "
                      "previous local SSRC was removed.";

      for (auto& kv : receive_streams_) {
        kv.second->SetLocalSsrc(rtcp_receiver_report_ssrc_);
      }
    }
  }

  delete removed_stream;

  if (ssrc == default_send_ssrc_) {
    default_send_ssrc_ = 0;
  }

  return true;
}

void WebRtcVideoChannel2::DeleteReceiveStream(
    WebRtcVideoChannel2::WebRtcVideoReceiveStream* stream) {
  for (uint32_t old_ssrc : stream->GetSsrcs())
    receive_ssrcs_.erase(old_ssrc);
  delete stream;
}

bool WebRtcVideoChannel2::AddRecvStream(const StreamParams& sp) {
  return AddRecvStream(sp, false);
}

bool WebRtcVideoChannel2::AddRecvStream(const StreamParams& sp,
                                        bool default_stream) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  LOG(LS_INFO) << "AddRecvStream" << (default_stream ? " (default stream)" : "")
               << ": " << sp.ToString();
  if (!ValidateStreamParams(sp))
    return false;

  uint32_t ssrc = sp.first_ssrc();
  RTC_DCHECK(ssrc != 0);  // TODO(pbos): Is this ever valid?

  rtc::CritScope stream_lock(&stream_crit_);
  // Remove running stream if this was a default stream.
  auto prev_stream = receive_streams_.find(ssrc);
  if (prev_stream != receive_streams_.end()) {
    if (default_stream || !prev_stream->second->IsDefaultStream()) {
      LOG(LS_ERROR) << "Receive stream for SSRC '" << ssrc
                    << "' already exists.";
      return false;
    }
    DeleteReceiveStream(prev_stream->second);
    receive_streams_.erase(prev_stream);
  }

  if (!ValidateReceiveSsrcAvailability(sp))
    return false;

  for (uint32_t used_ssrc : sp.ssrcs)
    receive_ssrcs_.insert(used_ssrc);

  webrtc::VideoReceiveStream::Config config(this);
  ConfigureReceiverRtp(&config, sp);

  // Set up A/V sync group based on sync label.
  config.sync_group = sp.sync_label;

  config.rtp.remb = send_codec_ ? HasRemb(send_codec_->codec) : false;

  receive_streams_[ssrc] = new WebRtcVideoReceiveStream(
      call_, sp, config, external_decoder_factory_, default_stream,
      recv_codecs_);

  return true;
}

void WebRtcVideoChannel2::ConfigureReceiverRtp(
    webrtc::VideoReceiveStream::Config* config,
    const StreamParams& sp) const {
  uint32_t ssrc = sp.first_ssrc();

  config->rtp.remote_ssrc = ssrc;
  config->rtp.local_ssrc = rtcp_receiver_report_ssrc_;

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
    MergeFecConfig(recv_codecs_[i].fec, &config->rtp.fec);
  }

  for (size_t i = 0; i < recv_codecs_.size(); ++i) {
    uint32_t rtx_ssrc;
    if (recv_codecs_[i].rtx_payload_type != -1 &&
        sp.GetFidSsrc(ssrc, &rtx_ssrc)) {
      webrtc::VideoReceiveStream::Config::Rtp::Rtx& rtx =
          config->rtp.rtx[recv_codecs_[i].codec.id];
      rtx.ssrc = rtx_ssrc;
      rtx.payload_type = recv_codecs_[i].rtx_payload_type;
    }
  }
}

bool WebRtcVideoChannel2::RemoveRecvStream(uint32_t ssrc) {
  LOG(LS_INFO) << "RemoveRecvStream: " << ssrc;
  if (ssrc == 0) {
    LOG(LS_ERROR) << "RemoveRecvStream with 0 ssrc is not supported.";
    return false;
  }

  rtc::CritScope stream_lock(&stream_crit_);
  std::map<uint32_t, WebRtcVideoReceiveStream*>::iterator stream =
      receive_streams_.find(ssrc);
  if (stream == receive_streams_.end()) {
    LOG(LS_ERROR) << "Stream not found for ssrc: " << ssrc;
    return false;
  }
  DeleteReceiveStream(stream->second);
  receive_streams_.erase(stream);

  return true;
}

bool WebRtcVideoChannel2::SetRenderer(uint32_t ssrc, VideoRenderer* renderer) {
  LOG(LS_INFO) << "SetRenderer: ssrc:" << ssrc << " "
               << (renderer ? "(ptr)" : "NULL");
  if (ssrc == 0) {
    default_unsignalled_ssrc_handler_.SetDefaultRenderer(this, renderer);
    return true;
  }

  rtc::CritScope stream_lock(&stream_crit_);
  std::map<uint32_t, WebRtcVideoReceiveStream*>::iterator it =
      receive_streams_.find(ssrc);
  if (it == receive_streams_.end()) {
    return false;
  }

  it->second->SetRenderer(renderer);
  return true;
}

bool WebRtcVideoChannel2::GetRenderer(uint32_t ssrc, VideoRenderer** renderer) {
  if (ssrc == 0) {
    *renderer = default_unsignalled_ssrc_handler_.GetDefaultRenderer();
    return *renderer != NULL;
  }

  rtc::CritScope stream_lock(&stream_crit_);
  std::map<uint32_t, WebRtcVideoReceiveStream*>::iterator it =
      receive_streams_.find(ssrc);
  if (it == receive_streams_.end()) {
    return false;
  }
  *renderer = it->second->GetRenderer();
  return true;
}

bool WebRtcVideoChannel2::GetStats(VideoMediaInfo* info) {
  info->Clear();
  FillSenderStats(info);
  FillReceiverStats(info);
  webrtc::Call::Stats stats = call_->GetStats();
  FillBandwidthEstimationStats(stats, info);
  if (stats.rtt_ms != -1) {
    for (size_t i = 0; i < info->senders.size(); ++i) {
      info->senders[i].rtt_ms = stats.rtt_ms;
    }
  }
  return true;
}

void WebRtcVideoChannel2::FillSenderStats(VideoMediaInfo* video_media_info) {
  rtc::CritScope stream_lock(&stream_crit_);
  for (std::map<uint32_t, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end(); ++it) {
    video_media_info->senders.push_back(it->second->GetVideoSenderInfo());
  }
}

void WebRtcVideoChannel2::FillReceiverStats(VideoMediaInfo* video_media_info) {
  rtc::CritScope stream_lock(&stream_crit_);
  for (std::map<uint32_t, WebRtcVideoReceiveStream*>::iterator it =
           receive_streams_.begin();
       it != receive_streams_.end(); ++it) {
    video_media_info->receivers.push_back(it->second->GetVideoReceiverInfo());
  }
}

void WebRtcVideoChannel2::FillBandwidthEstimationStats(
    const webrtc::Call::Stats& stats,
    VideoMediaInfo* video_media_info) {
  BandwidthEstimationInfo bwe_info;
  bwe_info.available_send_bandwidth = stats.send_bandwidth_bps;
  bwe_info.available_recv_bandwidth = stats.recv_bandwidth_bps;
  bwe_info.bucket_delay = stats.pacer_delay_ms;

  // Get send stream bitrate stats.
  rtc::CritScope stream_lock(&stream_crit_);
  for (std::map<uint32_t, WebRtcVideoSendStream*>::iterator stream =
           send_streams_.begin();
       stream != send_streams_.end(); ++stream) {
    stream->second->FillBandwidthEstimationInfo(&bwe_info);
  }
  video_media_info->bw_estimations.push_back(bwe_info);
}

bool WebRtcVideoChannel2::SetCapturer(uint32_t ssrc, VideoCapturer* capturer) {
  LOG(LS_INFO) << "SetCapturer: " << ssrc << " -> "
               << (capturer != NULL ? "(capturer)" : "NULL");
  RTC_DCHECK(ssrc != 0);
  {
    rtc::CritScope stream_lock(&stream_crit_);
    if (send_streams_.find(ssrc) == send_streams_.end()) {
      LOG(LS_ERROR) << "No sending stream on ssrc " << ssrc;
      return false;
    }
    if (!send_streams_[ssrc]->SetCapturer(capturer)) {
      return false;
    }
  }

  if (capturer) {
    capturer->SetApplyRotation(
        !FindHeaderExtension(send_rtp_extensions_,
                             kRtpVideoRotationHeaderExtension));
  }
  {
    rtc::CritScope lock(&capturer_crit_);
    capturers_[ssrc] = capturer;
  }
  return true;
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
    rtc::Buffer* packet,
    const rtc::PacketTime& packet_time) {
  const webrtc::PacketTime webrtc_packet_time(packet_time.timestamp,
                                              packet_time.not_before);
  const webrtc::PacketReceiver::DeliveryStatus delivery_result =
      call_->Receiver()->DeliverPacket(
          webrtc::MediaType::VIDEO,
          reinterpret_cast<const uint8_t*>(packet->data()), packet->size(),
          webrtc_packet_time);
  switch (delivery_result) {
    case webrtc::PacketReceiver::DELIVERY_OK:
      return;
    case webrtc::PacketReceiver::DELIVERY_PACKET_ERROR:
      return;
    case webrtc::PacketReceiver::DELIVERY_UNKNOWN_SSRC:
      break;
  }

  uint32_t ssrc = 0;
  if (!GetRtpSsrc(packet->data(), packet->size(), &ssrc)) {
    return;
  }

  int payload_type = 0;
  if (!GetRtpPayloadType(packet->data(), packet->size(), &payload_type)) {
    return;
  }

  // See if this payload_type is registered as one that usually gets its own
  // SSRC (RTX) or at least is safe to drop either way (ULPFEC). If it is, and
  // it wasn't handled above by DeliverPacket, that means we don't know what
  // stream it associates with, and we shouldn't ever create an implicit channel
  // for these.
  for (auto& codec : recv_codecs_) {
    if (payload_type == codec.rtx_payload_type ||
        payload_type == codec.fec.red_rtx_payload_type ||
        payload_type == codec.fec.ulpfec_payload_type) {
      return;
    }
  }

  switch (unsignalled_ssrc_handler_->OnUnsignalledSsrc(this, ssrc)) {
    case UnsignalledSsrcHandler::kDropPacket:
      return;
    case UnsignalledSsrcHandler::kDeliverPacket:
      break;
  }

  if (call_->Receiver()->DeliverPacket(
          webrtc::MediaType::VIDEO,
          reinterpret_cast<const uint8_t*>(packet->data()), packet->size(),
          webrtc_packet_time) != webrtc::PacketReceiver::DELIVERY_OK) {
    LOG(LS_WARNING) << "Failed to deliver RTP packet on re-delivery.";
    return;
  }
}

void WebRtcVideoChannel2::OnRtcpReceived(
    rtc::Buffer* packet,
    const rtc::PacketTime& packet_time) {
  const webrtc::PacketTime webrtc_packet_time(packet_time.timestamp,
                                              packet_time.not_before);
  if (call_->Receiver()->DeliverPacket(
          webrtc::MediaType::VIDEO,
          reinterpret_cast<const uint8_t*>(packet->data()), packet->size(),
          webrtc_packet_time) != webrtc::PacketReceiver::DELIVERY_OK) {
    LOG(LS_WARNING) << "Failed to deliver RTCP packet.";
  }
}

void WebRtcVideoChannel2::OnReadyToSend(bool ready) {
  LOG(LS_VERBOSE) << "OnReadyToSend: " << (ready ? "Ready." : "Not ready.");
  call_->SignalNetworkState(ready ? webrtc::kNetworkUp : webrtc::kNetworkDown);
}

bool WebRtcVideoChannel2::MuteStream(uint32_t ssrc, bool mute) {
  LOG(LS_VERBOSE) << "MuteStream: " << ssrc << " -> "
                  << (mute ? "mute" : "unmute");
  RTC_DCHECK(ssrc != 0);
  rtc::CritScope stream_lock(&stream_crit_);
  if (send_streams_.find(ssrc) == send_streams_.end()) {
    LOG(LS_ERROR) << "No sending stream on ssrc " << ssrc;
    return false;
  }

  send_streams_[ssrc]->MuteStream(mute);
  return true;
}

bool WebRtcVideoChannel2::SetRecvRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel2::SetRecvRtpHeaderExtensions");
  LOG(LS_INFO) << "SetRecvRtpHeaderExtensions: "
               << RtpExtensionsToString(extensions);
  if (!ValidateRtpHeaderExtensionIds(extensions))
    return false;

  std::vector<webrtc::RtpExtension> filtered_extensions =
      FilterRtpExtensions(extensions);
  if (!RtpExtensionsHaveChanged(recv_rtp_extensions_, filtered_extensions)) {
    LOG(LS_INFO) << "Ignoring call to SetRecvRtpHeaderExtensions because "
                    "header extensions haven't changed.";
    return true;
  }

  recv_rtp_extensions_ = filtered_extensions;

  rtc::CritScope stream_lock(&stream_crit_);
  for (std::map<uint32_t, WebRtcVideoReceiveStream*>::iterator it =
           receive_streams_.begin();
       it != receive_streams_.end(); ++it) {
    it->second->SetRtpExtensions(recv_rtp_extensions_);
  }
  return true;
}

bool WebRtcVideoChannel2::SetSendRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel2::SetSendRtpHeaderExtensions");
  LOG(LS_INFO) << "SetSendRtpHeaderExtensions: "
               << RtpExtensionsToString(extensions);
  if (!ValidateRtpHeaderExtensionIds(extensions))
    return false;

  std::vector<webrtc::RtpExtension> filtered_extensions =
      FilterRtpExtensions(FilterRedundantRtpExtensions(
          extensions, kBweExtensionPriorities, kBweExtensionPrioritiesLength));
  if (!RtpExtensionsHaveChanged(send_rtp_extensions_, filtered_extensions)) {
    LOG(LS_INFO) << "Ignoring call to SetSendRtpHeaderExtensions because "
                    "header extensions haven't changed.";
    return true;
  }

  send_rtp_extensions_ = filtered_extensions;

  const webrtc::RtpExtension* cvo_extension = FindHeaderExtension(
      send_rtp_extensions_, kRtpVideoRotationHeaderExtension);

  rtc::CritScope stream_lock(&stream_crit_);
  for (std::map<uint32_t, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end(); ++it) {
    it->second->SetRtpExtensions(send_rtp_extensions_);
    it->second->SetApplyRotation(!cvo_extension);
  }
  return true;
}

// Counter-intuitively this method doesn't only set global bitrate caps but also
// per-stream codec max bitrates. This is to permit SetMaxSendBitrate (b=AS) to
// raise bitrates above the 2000k default bitrate cap.
bool WebRtcVideoChannel2::SetMaxSendBandwidth(int max_bitrate_bps) {
  // TODO(pbos): Figure out whether b=AS means max bitrate for this
  // WebRtcVideoChannel2 (in which case we're good), or per sender (SSRC), in
  // which case this should not set a Call::BitrateConfig but rather reconfigure
  // all senders.
  LOG(LS_INFO) << "SetMaxSendBandwidth: " << max_bitrate_bps << "bps.";
  if (max_bitrate_bps == bitrate_config_.max_bitrate_bps)
    return true;

  if (max_bitrate_bps < 0) {
    // Option not set.
    return true;
  }
  if (max_bitrate_bps == 0) {
    // Unsetting max bitrate.
    max_bitrate_bps = -1;
  }
  bitrate_config_.start_bitrate_bps = -1;
  bitrate_config_.max_bitrate_bps = max_bitrate_bps;
  if (max_bitrate_bps > 0 &&
      bitrate_config_.min_bitrate_bps > max_bitrate_bps) {
    bitrate_config_.min_bitrate_bps = max_bitrate_bps;
  }
  call_->SetBitrateConfig(bitrate_config_);
  rtc::CritScope stream_lock(&stream_crit_);
  for (auto& kv : send_streams_)
    kv.second->SetMaxBitrateBps(max_bitrate_bps);
  return true;
}

bool WebRtcVideoChannel2::SetOptions(const VideoOptions& options) {
  TRACE_EVENT0("webrtc", "WebRtcVideoChannel2::SetOptions");
  LOG(LS_INFO) << "SetOptions: " << options.ToString();
  VideoOptions old_options = options_;
  options_.SetAll(options);
  if (options_ == old_options) {
    // No new options to set.
    return true;
  }
  {
    rtc::CritScope lock(&capturer_crit_);
    if (options_.cpu_overuse_detection)
      signal_cpu_adaptation_ = *options_.cpu_overuse_detection;
  }
  rtc::DiffServCodePoint dscp =
      options_.dscp.value_or(false) ? rtc::DSCP_AF41 : rtc::DSCP_DEFAULT;
  MediaChannel::SetDscp(dscp);
  rtc::CritScope stream_lock(&stream_crit_);
  for (std::map<uint32_t, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end(); ++it) {
    it->second->SetOptions(options_);
  }
  return true;
}

void WebRtcVideoChannel2::SetInterface(NetworkInterface* iface) {
  MediaChannel::SetInterface(iface);
  // Set the RTP recv/send buffer to a bigger size
  MediaChannel::SetOption(NetworkInterface::ST_RTP,
                          rtc::Socket::OPT_RCVBUF,
                          kVideoRtpBufferSize);

  // Speculative change to increase the outbound socket buffer size.
  // In b/15152257, we are seeing a significant number of packets discarded
  // due to lack of socket buffer space, although it's not yet clear what the
  // ideal value should be.
  MediaChannel::SetOption(NetworkInterface::ST_RTP,
                          rtc::Socket::OPT_SNDBUF,
                          kVideoRtpBufferSize);
}

void WebRtcVideoChannel2::UpdateAspectRatio(int ratio_w, int ratio_h) {
  // TODO(pbos): Implement.
}

void WebRtcVideoChannel2::OnMessage(rtc::Message* msg) {
  // Ignored.
}

void WebRtcVideoChannel2::OnLoadUpdate(Load load) {
  // OnLoadUpdate can not take any locks that are held while creating streams
  // etc. Doing so establishes lock-order inversions between the webrtc process
  // thread on stream creation and locks such as stream_crit_ while calling out.
  rtc::CritScope stream_lock(&capturer_crit_);
  if (!signal_cpu_adaptation_)
    return;
  // Do not adapt resolution for screen content as this will likely result in
  // blurry and unreadable text.
  for (auto& kv : capturers_) {
    if (kv.second != nullptr
        && !kv.second->IsScreencast()
        && kv.second->video_adapter() != nullptr) {
      kv.second->video_adapter()->OnCpuResolutionRequest(
          load == kOveruse ? CoordinatedVideoAdapter::DOWNGRADE
                           : CoordinatedVideoAdapter::UPGRADE);
    }
  }
}

bool WebRtcVideoChannel2::SendRtp(const uint8_t* data,
                                  size_t len,
                                  const webrtc::PacketOptions& options) {
  rtc::Buffer packet(data, len, kMaxRtpPacketLen);
  rtc::PacketOptions rtc_options;
  rtc_options.packet_id = options.packet_id;
  return MediaChannel::SendPacket(&packet, rtc_options);
}

bool WebRtcVideoChannel2::SendRtcp(const uint8_t* data, size_t len) {
  rtc::Buffer packet(data, len, kMaxRtpPacketLen);
  return MediaChannel::SendRtcp(&packet, rtc::PacketOptions());
}

void WebRtcVideoChannel2::StartAllSendStreams() {
  rtc::CritScope stream_lock(&stream_crit_);
  for (std::map<uint32_t, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end(); ++it) {
    it->second->Start();
  }
}

void WebRtcVideoChannel2::StopAllSendStreams() {
  rtc::CritScope stream_lock(&stream_crit_);
  for (std::map<uint32_t, WebRtcVideoSendStream*>::iterator it =
           send_streams_.begin();
       it != send_streams_.end(); ++it) {
    it->second->Stop();
  }
}

WebRtcVideoChannel2::WebRtcVideoSendStream::VideoSendStreamParameters::
    VideoSendStreamParameters(
        const webrtc::VideoSendStream::Config& config,
        const VideoOptions& options,
        int max_bitrate_bps,
        const rtc::Maybe<VideoCodecSettings>& codec_settings)
    : config(config),
      options(options),
      max_bitrate_bps(max_bitrate_bps),
      codec_settings(codec_settings) {}

WebRtcVideoChannel2::WebRtcVideoSendStream::AllocatedEncoder::AllocatedEncoder(
    webrtc::VideoEncoder* encoder,
    webrtc::VideoCodecType type,
    bool external)
    : encoder(encoder),
      external_encoder(nullptr),
      type(type),
      external(external) {
  if (external) {
    external_encoder = encoder;
    this->encoder =
        new webrtc::VideoEncoderSoftwareFallbackWrapper(type, encoder);
  }
}

WebRtcVideoChannel2::WebRtcVideoSendStream::WebRtcVideoSendStream(
    webrtc::Call* call,
    const StreamParams& sp,
    const webrtc::VideoSendStream::Config& config,
    WebRtcVideoEncoderFactory* external_encoder_factory,
    const VideoOptions& options,
    int max_bitrate_bps,
    const rtc::Maybe<VideoCodecSettings>& codec_settings,
    const std::vector<webrtc::RtpExtension>& rtp_extensions)
    : ssrcs_(sp.ssrcs),
      ssrc_groups_(sp.ssrc_groups),
      call_(call),
      external_encoder_factory_(external_encoder_factory),
      stream_(NULL),
      parameters_(config, options, max_bitrate_bps, codec_settings),
      allocated_encoder_(NULL, webrtc::kVideoCodecUnknown, false),
      capturer_(NULL),
      sending_(false),
      muted_(false),
      old_adapt_changes_(0),
      first_frame_timestamp_ms_(0),
      last_frame_timestamp_ms_(0) {
  parameters_.config.rtp.max_packet_size = kVideoMtu;

  sp.GetPrimarySsrcs(&parameters_.config.rtp.ssrcs);
  sp.GetFidSsrcs(parameters_.config.rtp.ssrcs,
                 &parameters_.config.rtp.rtx.ssrcs);
  parameters_.config.rtp.c_name = sp.cname;
  parameters_.config.rtp.extensions = rtp_extensions;

  if (codec_settings) {
    SetCodec(*codec_settings);
  }
}

WebRtcVideoChannel2::WebRtcVideoSendStream::~WebRtcVideoSendStream() {
  DisconnectCapturer();
  if (stream_ != NULL) {
    call_->DestroyVideoSendStream(stream_);
  }
  DestroyVideoEncoder(&allocated_encoder_);
}

static void CreateBlackFrame(webrtc::VideoFrame* video_frame,
                             int width,
                             int height) {
  video_frame->CreateEmptyFrame(width, height, width, (width + 1) / 2,
                                (width + 1) / 2);
  memset(video_frame->buffer(webrtc::kYPlane), 16,
         video_frame->allocated_size(webrtc::kYPlane));
  memset(video_frame->buffer(webrtc::kUPlane), 128,
         video_frame->allocated_size(webrtc::kUPlane));
  memset(video_frame->buffer(webrtc::kVPlane), 128,
         video_frame->allocated_size(webrtc::kVPlane));
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::InputFrame(
    VideoCapturer* capturer,
    const VideoFrame* frame) {
  TRACE_EVENT0("webrtc", "WebRtcVideoSendStream::InputFrame");
  webrtc::VideoFrame video_frame(frame->GetVideoFrameBuffer(), 0, 0,
                                 frame->GetVideoRotation());
  rtc::CritScope cs(&lock_);
  if (stream_ == NULL) {
    // Frame input before send codecs are configured, dropping frame.
    return;
  }

  // Not sending, abort early to prevent expensive reconfigurations while
  // setting up codecs etc.
  if (!sending_)
    return;

  if (format_.width == 0) {  // Dropping frames.
    RTC_DCHECK(format_.height == 0);
    LOG(LS_VERBOSE) << "VideoFormat 0x0 set, Dropping frame.";
    return;
  }
  if (muted_) {
    // Create a black frame to transmit instead.
    CreateBlackFrame(&video_frame,
                     static_cast<int>(frame->GetWidth()),
                     static_cast<int>(frame->GetHeight()));
  }

  int64_t frame_delta_ms = frame->GetTimeStamp() / rtc::kNumNanosecsPerMillisec;
  // frame->GetTimeStamp() is essentially a delta, align to webrtc time
  if (first_frame_timestamp_ms_ == 0) {
    first_frame_timestamp_ms_ = rtc::Time() - frame_delta_ms;
  }

  last_frame_timestamp_ms_ = first_frame_timestamp_ms_ + frame_delta_ms;
  video_frame.set_render_time_ms(last_frame_timestamp_ms_);
  // Reconfigure codec if necessary.
  SetDimensions(
      video_frame.width(), video_frame.height(), capturer->IsScreencast());

  stream_->Input()->IncomingCapturedFrame(video_frame);
}

bool WebRtcVideoChannel2::WebRtcVideoSendStream::SetCapturer(
    VideoCapturer* capturer) {
  TRACE_EVENT0("webrtc", "WebRtcVideoSendStream::SetCapturer");
  if (!DisconnectCapturer() && capturer == NULL) {
    return false;
  }

  {
    rtc::CritScope cs(&lock_);

    // Reset timestamps to realign new incoming frames to a webrtc timestamp. A
    // new capturer may have a different timestamp delta than the previous one.
    first_frame_timestamp_ms_ = 0;

    if (capturer == NULL) {
      if (stream_ != NULL) {
        LOG(LS_VERBOSE) << "Disabling capturer, sending black frame.";
        webrtc::VideoFrame black_frame;

        CreateBlackFrame(&black_frame, last_dimensions_.width,
                         last_dimensions_.height);

        // Force this black frame not to be dropped due to timestamp order
        // check. As IncomingCapturedFrame will drop the frame if this frame's
        // timestamp is less than or equal to last frame's timestamp, it is
        // necessary to give this black frame a larger timestamp than the
        // previous one.
        last_frame_timestamp_ms_ +=
            format_.interval / rtc::kNumNanosecsPerMillisec;
        black_frame.set_render_time_ms(last_frame_timestamp_ms_);
        stream_->Input()->IncomingCapturedFrame(black_frame);
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

  rtc::CritScope cs(&lock_);
  if (format.width == 0 && format.height == 0) {
    LOG(LS_INFO)
        << "0x0 resolution selected. Captured frames will be dropped for ssrc: "
        << parameters_.config.rtp.ssrcs[0] << ".";
  } else {
    // TODO(pbos): Fix me, this only affects the last stream!
    parameters_.encoder_config.streams.back().max_framerate =
        VideoFormat::IntervalToFps(format.interval);
    SetDimensions(format.width, format.height, false);
  }

  format_ = format;
  return true;
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::MuteStream(bool mute) {
  rtc::CritScope cs(&lock_);
  muted_ = mute;
}

bool WebRtcVideoChannel2::WebRtcVideoSendStream::DisconnectCapturer() {
  cricket::VideoCapturer* capturer;
  {
    rtc::CritScope cs(&lock_);
    if (capturer_ == NULL)
      return false;

    if (capturer_->video_adapter() != nullptr)
      old_adapt_changes_ += capturer_->video_adapter()->adaptation_changes();

    capturer = capturer_;
    capturer_ = NULL;
  }
  capturer->SignalVideoFrame.disconnect(this);
  return true;
}

const std::vector<uint32_t>&
WebRtcVideoChannel2::WebRtcVideoSendStream::GetSsrcs() const {
  return ssrcs_;
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::SetApplyRotation(
    bool apply_rotation) {
  rtc::CritScope cs(&lock_);
  if (capturer_ == NULL)
    return;

  capturer_->SetApplyRotation(apply_rotation);
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::SetOptions(
    const VideoOptions& options) {
  rtc::CritScope cs(&lock_);
  if (parameters_.codec_settings) {
    LOG(LS_INFO) << "SetCodecAndOptions because of SetOptions; options="
                 << options.ToString();
    SetCodecAndOptions(*parameters_.codec_settings, options);
  } else {
    parameters_.options = options;
  }
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::SetCodec(
    const VideoCodecSettings& codec_settings) {
  rtc::CritScope cs(&lock_);
  LOG(LS_INFO) << "SetCodecAndOptions because of SetCodec.";
  SetCodecAndOptions(codec_settings, parameters_.options);
}

webrtc::VideoCodecType CodecTypeFromName(const std::string& name) {
  if (CodecNamesEq(name, kVp8CodecName)) {
    return webrtc::kVideoCodecVP8;
  } else if (CodecNamesEq(name, kVp9CodecName)) {
    return webrtc::kVideoCodecVP9;
  } else if (CodecNamesEq(name, kH264CodecName)) {
    return webrtc::kVideoCodecH264;
  }
  return webrtc::kVideoCodecUnknown;
}

WebRtcVideoChannel2::WebRtcVideoSendStream::AllocatedEncoder
WebRtcVideoChannel2::WebRtcVideoSendStream::CreateVideoEncoder(
    const VideoCodec& codec) {
  webrtc::VideoCodecType type = CodecTypeFromName(codec.name);

  // Do not re-create encoders of the same type.
  if (type == allocated_encoder_.type && allocated_encoder_.encoder != NULL) {
    return allocated_encoder_;
  }

  if (external_encoder_factory_ != NULL) {
    webrtc::VideoEncoder* encoder =
        external_encoder_factory_->CreateVideoEncoder(type);
    if (encoder != NULL) {
      return AllocatedEncoder(encoder, type, true);
    }
  }

  if (type == webrtc::kVideoCodecVP8) {
    return AllocatedEncoder(
        webrtc::VideoEncoder::Create(webrtc::VideoEncoder::kVp8), type, false);
  } else if (type == webrtc::kVideoCodecVP9) {
    return AllocatedEncoder(
        webrtc::VideoEncoder::Create(webrtc::VideoEncoder::kVp9), type, false);
  } else if (type == webrtc::kVideoCodecH264) {
    return AllocatedEncoder(
        webrtc::VideoEncoder::Create(webrtc::VideoEncoder::kH264), type, false);
  }

  // This shouldn't happen, we should not be trying to create something we don't
  // support.
  RTC_DCHECK(false);
  return AllocatedEncoder(NULL, webrtc::kVideoCodecUnknown, false);
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::DestroyVideoEncoder(
    AllocatedEncoder* encoder) {
  if (encoder->external) {
    external_encoder_factory_->DestroyVideoEncoder(encoder->external_encoder);
  }
  delete encoder->encoder;
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::SetCodecAndOptions(
    const VideoCodecSettings& codec_settings,
    const VideoOptions& options) {
  parameters_.encoder_config =
      CreateVideoEncoderConfig(last_dimensions_, codec_settings.codec);
  if (parameters_.encoder_config.streams.empty())
    return;

  format_ = VideoFormat(codec_settings.codec.width,
                        codec_settings.codec.height,
                        VideoFormat::FpsToInterval(30),
                        FOURCC_I420);

  AllocatedEncoder new_encoder = CreateVideoEncoder(codec_settings.codec);
  parameters_.config.encoder_settings.encoder = new_encoder.encoder;
  parameters_.config.encoder_settings.payload_name = codec_settings.codec.name;
  parameters_.config.encoder_settings.payload_type = codec_settings.codec.id;
  if (new_encoder.external) {
    webrtc::VideoCodecType type = CodecTypeFromName(codec_settings.codec.name);
    parameters_.config.encoder_settings.internal_source =
        external_encoder_factory_->EncoderTypeHasInternalSource(type);
  }
  parameters_.config.rtp.fec = codec_settings.fec;

  // Set RTX payload type if RTX is enabled.
  if (!parameters_.config.rtp.rtx.ssrcs.empty()) {
    if (codec_settings.rtx_payload_type == -1) {
      LOG(LS_WARNING) << "RTX SSRCs configured but there's no configured RTX "
                         "payload type. Ignoring.";
      parameters_.config.rtp.rtx.ssrcs.clear();
    } else {
      parameters_.config.rtp.rtx.payload_type = codec_settings.rtx_payload_type;
    }
  }

  parameters_.config.rtp.nack.rtp_history_ms =
      HasNack(codec_settings.codec) ? kNackHistoryMs : 0;

  RTC_CHECK(options.suspend_below_min_bitrate);
  parameters_.config.suspend_below_min_bitrate =
      *options.suspend_below_min_bitrate;

  parameters_.codec_settings =
      rtc::Maybe<WebRtcVideoChannel2::VideoCodecSettings>(codec_settings);
  parameters_.options = options;

  LOG(LS_INFO)
      << "RecreateWebRtcStream (send) because of SetCodecAndOptions; options="
      << options.ToString();
  RecreateWebRtcStream();
  if (allocated_encoder_.encoder != new_encoder.encoder) {
    DestroyVideoEncoder(&allocated_encoder_);
    allocated_encoder_ = new_encoder;
  }
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::SetRtpExtensions(
    const std::vector<webrtc::RtpExtension>& rtp_extensions) {
  rtc::CritScope cs(&lock_);
  parameters_.config.rtp.extensions = rtp_extensions;
  if (stream_ != nullptr) {
    LOG(LS_INFO) << "RecreateWebRtcStream (send) because of SetRtpExtensions";
    RecreateWebRtcStream();
  }
}

webrtc::VideoEncoderConfig
WebRtcVideoChannel2::WebRtcVideoSendStream::CreateVideoEncoderConfig(
    const Dimensions& dimensions,
    const VideoCodec& codec) const {
  webrtc::VideoEncoderConfig encoder_config;
  if (dimensions.is_screencast) {
    RTC_CHECK(parameters_.options.screencast_min_bitrate);
    encoder_config.min_transmit_bitrate_bps =
        *parameters_.options.screencast_min_bitrate * 1000;
    encoder_config.content_type =
        webrtc::VideoEncoderConfig::ContentType::kScreen;
  } else {
    encoder_config.min_transmit_bitrate_bps = 0;
    encoder_config.content_type =
        webrtc::VideoEncoderConfig::ContentType::kRealtimeVideo;
  }

  // Restrict dimensions according to codec max.
  int width = dimensions.width;
  int height = dimensions.height;
  if (!dimensions.is_screencast) {
    if (codec.width < width)
      width = codec.width;
    if (codec.height < height)
      height = codec.height;
  }

  VideoCodec clamped_codec = codec;
  clamped_codec.width = width;
  clamped_codec.height = height;

  // By default, the stream count for the codec configuration should match the
  // number of negotiated ssrcs. But if the codec is blacklisted for simulcast
  // or a screencast, only configure a single stream.
  size_t stream_count = parameters_.config.rtp.ssrcs.size();
  if (IsCodecBlacklistedForSimulcast(codec.name) || dimensions.is_screencast) {
    stream_count = 1;
  }

  encoder_config.streams =
      CreateVideoStreams(clamped_codec, parameters_.options,
                         parameters_.max_bitrate_bps, stream_count);

  // Conference mode screencast uses 2 temporal layers split at 100kbit.
  if (parameters_.options.conference_mode.value_or(false) &&
      dimensions.is_screencast && encoder_config.streams.size() == 1) {
    ScreenshareLayerConfig config = ScreenshareLayerConfig::GetDefault();

    // For screenshare in conference mode, tl0 and tl1 bitrates are piggybacked
    // on the VideoCodec struct as target and max bitrates, respectively.
    // See eg. webrtc::VP8EncoderImpl::SetRates().
    encoder_config.streams[0].target_bitrate_bps =
        config.tl0_bitrate_kbps * 1000;
    encoder_config.streams[0].max_bitrate_bps = config.tl1_bitrate_kbps * 1000;
    encoder_config.streams[0].temporal_layer_thresholds_bps.clear();
    encoder_config.streams[0].temporal_layer_thresholds_bps.push_back(
        config.tl0_bitrate_kbps * 1000);
  }
  return encoder_config;
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::SetDimensions(
    int width,
    int height,
    bool is_screencast) {
  if (last_dimensions_.width == width && last_dimensions_.height == height &&
      last_dimensions_.is_screencast == is_screencast) {
    // Configured using the same parameters, do not reconfigure.
    return;
  }
  LOG(LS_INFO) << "SetDimensions: " << width << "x" << height
               << (is_screencast ? " (screencast)" : " (not screencast)");

  last_dimensions_.width = width;
  last_dimensions_.height = height;
  last_dimensions_.is_screencast = is_screencast;

  RTC_DCHECK(!parameters_.encoder_config.streams.empty());

  RTC_CHECK(parameters_.codec_settings);
  VideoCodecSettings codec_settings = *parameters_.codec_settings;

  webrtc::VideoEncoderConfig encoder_config =
      CreateVideoEncoderConfig(last_dimensions_, codec_settings.codec);

  encoder_config.encoder_specific_settings = ConfigureVideoEncoderSettings(
      codec_settings.codec, parameters_.options, is_screencast);

  bool stream_reconfigured = stream_->ReconfigureVideoEncoder(encoder_config);

  encoder_config.encoder_specific_settings = NULL;

  if (!stream_reconfigured) {
    LOG(LS_WARNING) << "Failed to reconfigure video encoder for dimensions: "
                    << width << "x" << height;
    return;
  }

  parameters_.encoder_config = encoder_config;
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::Start() {
  rtc::CritScope cs(&lock_);
  RTC_DCHECK(stream_ != NULL);
  stream_->Start();
  sending_ = true;
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::Stop() {
  rtc::CritScope cs(&lock_);
  if (stream_ != NULL) {
    stream_->Stop();
  }
  sending_ = false;
}

VideoSenderInfo
WebRtcVideoChannel2::WebRtcVideoSendStream::GetVideoSenderInfo() {
  VideoSenderInfo info;
  webrtc::VideoSendStream::Stats stats;
  {
    rtc::CritScope cs(&lock_);
    for (uint32_t ssrc : parameters_.config.rtp.ssrcs)
      info.add_ssrc(ssrc);

    if (parameters_.codec_settings)
      info.codec_name = parameters_.codec_settings->codec.name;
    for (size_t i = 0; i < parameters_.encoder_config.streams.size(); ++i) {
      if (i == parameters_.encoder_config.streams.size() - 1) {
        info.preferred_bitrate +=
            parameters_.encoder_config.streams[i].max_bitrate_bps;
      } else {
        info.preferred_bitrate +=
            parameters_.encoder_config.streams[i].target_bitrate_bps;
      }
    }

    if (stream_ == NULL)
      return info;

    stats = stream_->GetStats();

    info.adapt_changes = old_adapt_changes_;
    info.adapt_reason = CoordinatedVideoAdapter::ADAPTREASON_NONE;

    if (capturer_ != NULL) {
      if (!capturer_->IsMuted()) {
        VideoFormat last_captured_frame_format;
        capturer_->GetStats(&info.adapt_frame_drops, &info.effects_frame_drops,
                            &info.capturer_frame_time,
                            &last_captured_frame_format);
        info.input_frame_width = last_captured_frame_format.width;
        info.input_frame_height = last_captured_frame_format.height;
      }
      if (capturer_->video_adapter() != nullptr) {
        info.adapt_changes += capturer_->video_adapter()->adaptation_changes();
        info.adapt_reason = capturer_->video_adapter()->adapt_reason();
      }
    }
  }
  info.ssrc_groups = ssrc_groups_;
  info.framerate_input = stats.input_frame_rate;
  info.framerate_sent = stats.encode_frame_rate;
  info.avg_encode_ms = stats.avg_encode_time_ms;
  info.encode_usage_percent = stats.encode_usage_percent;

  info.nominal_bitrate = stats.media_bitrate_bps;

  info.send_frame_width = 0;
  info.send_frame_height = 0;
  for (std::map<uint32_t, webrtc::VideoSendStream::StreamStats>::iterator it =
           stats.substreams.begin();
       it != stats.substreams.end(); ++it) {
    // TODO(pbos): Wire up additional stats, such as padding bytes.
    webrtc::VideoSendStream::StreamStats stream_stats = it->second;
    info.bytes_sent += stream_stats.rtp_stats.transmitted.payload_bytes +
                       stream_stats.rtp_stats.transmitted.header_bytes +
                       stream_stats.rtp_stats.transmitted.padding_bytes;
    info.packets_sent += stream_stats.rtp_stats.transmitted.packets;
    info.packets_lost += stream_stats.rtcp_stats.cumulative_lost;
    if (stream_stats.width > info.send_frame_width)
      info.send_frame_width = stream_stats.width;
    if (stream_stats.height > info.send_frame_height)
      info.send_frame_height = stream_stats.height;
    info.firs_rcvd += stream_stats.rtcp_packet_type_counts.fir_packets;
    info.nacks_rcvd += stream_stats.rtcp_packet_type_counts.nack_packets;
    info.plis_rcvd += stream_stats.rtcp_packet_type_counts.pli_packets;
  }

  if (!stats.substreams.empty()) {
    // TODO(pbos): Report fraction lost per SSRC.
    webrtc::VideoSendStream::StreamStats first_stream_stats =
        stats.substreams.begin()->second;
    info.fraction_lost =
        static_cast<float>(first_stream_stats.rtcp_stats.fraction_lost) /
        (1 << 8);
  }

  return info;
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::FillBandwidthEstimationInfo(
    BandwidthEstimationInfo* bwe_info) {
  rtc::CritScope cs(&lock_);
  if (stream_ == NULL) {
    return;
  }
  webrtc::VideoSendStream::Stats stats = stream_->GetStats();
  for (std::map<uint32_t, webrtc::VideoSendStream::StreamStats>::iterator it =
           stats.substreams.begin();
       it != stats.substreams.end(); ++it) {
    bwe_info->transmit_bitrate += it->second.total_bitrate_bps;
    bwe_info->retransmit_bitrate += it->second.retransmit_bitrate_bps;
  }
  bwe_info->target_enc_bitrate += stats.target_media_bitrate_bps;
  bwe_info->actual_enc_bitrate += stats.media_bitrate_bps;
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::SetMaxBitrateBps(
    int max_bitrate_bps) {
  rtc::CritScope cs(&lock_);
  parameters_.max_bitrate_bps = max_bitrate_bps;

  // No need to reconfigure if the stream hasn't been configured yet.
  if (parameters_.encoder_config.streams.empty())
    return;

  // Force a stream reconfigure to set the new max bitrate.
  int width = last_dimensions_.width;
  last_dimensions_.width = 0;
  SetDimensions(width, last_dimensions_.height, last_dimensions_.is_screencast);
}

void WebRtcVideoChannel2::WebRtcVideoSendStream::RecreateWebRtcStream() {
  if (stream_ != NULL) {
    call_->DestroyVideoSendStream(stream_);
  }

  RTC_CHECK(parameters_.codec_settings);
  parameters_.encoder_config.encoder_specific_settings =
      ConfigureVideoEncoderSettings(
          parameters_.codec_settings->codec, parameters_.options,
          parameters_.encoder_config.content_type ==
              webrtc::VideoEncoderConfig::ContentType::kScreen);

  webrtc::VideoSendStream::Config config = parameters_.config;
  if (!config.rtp.rtx.ssrcs.empty() && config.rtp.rtx.payload_type == -1) {
    LOG(LS_WARNING) << "RTX SSRCs configured but there's no configured RTX "
                       "payload type the set codec. Ignoring RTX.";
    config.rtp.rtx.ssrcs.clear();
  }
  stream_ = call_->CreateVideoSendStream(config, parameters_.encoder_config);

  parameters_.encoder_config.encoder_specific_settings = NULL;

  if (sending_) {
    stream_->Start();
  }
}

WebRtcVideoChannel2::WebRtcVideoReceiveStream::WebRtcVideoReceiveStream(
    webrtc::Call* call,
    const StreamParams& sp,
    const webrtc::VideoReceiveStream::Config& config,
    WebRtcVideoDecoderFactory* external_decoder_factory,
    bool default_stream,
    const std::vector<VideoCodecSettings>& recv_codecs)
    : call_(call),
      ssrcs_(sp.ssrcs),
      ssrc_groups_(sp.ssrc_groups),
      stream_(NULL),
      default_stream_(default_stream),
      config_(config),
      external_decoder_factory_(external_decoder_factory),
      renderer_(NULL),
      last_width_(-1),
      last_height_(-1),
      first_frame_timestamp_(-1),
      estimated_remote_start_ntp_time_ms_(0) {
  config_.renderer = this;
  // SetRecvCodecs will also reset (start) the VideoReceiveStream.
  LOG(LS_INFO) << "SetRecvCodecs (recv) because we are creating the receive "
                  "stream for the first time: "
               << CodecSettingsVectorToString(recv_codecs);
  SetRecvCodecs(recv_codecs);
}

WebRtcVideoChannel2::WebRtcVideoReceiveStream::AllocatedDecoder::
    AllocatedDecoder(webrtc::VideoDecoder* decoder,
                     webrtc::VideoCodecType type,
                     bool external)
    : decoder(decoder),
      external_decoder(nullptr),
      type(type),
      external(external) {
  if (external) {
    external_decoder = decoder;
    this->decoder =
        new webrtc::VideoDecoderSoftwareFallbackWrapper(type, external_decoder);
  }
}

WebRtcVideoChannel2::WebRtcVideoReceiveStream::~WebRtcVideoReceiveStream() {
  call_->DestroyVideoReceiveStream(stream_);
  ClearDecoders(&allocated_decoders_);
}

const std::vector<uint32_t>&
WebRtcVideoChannel2::WebRtcVideoReceiveStream::GetSsrcs() const {
  return ssrcs_;
}

WebRtcVideoChannel2::WebRtcVideoReceiveStream::AllocatedDecoder
WebRtcVideoChannel2::WebRtcVideoReceiveStream::CreateOrReuseVideoDecoder(
    std::vector<AllocatedDecoder>* old_decoders,
    const VideoCodec& codec) {
  webrtc::VideoCodecType type = CodecTypeFromName(codec.name);

  for (size_t i = 0; i < old_decoders->size(); ++i) {
    if ((*old_decoders)[i].type == type) {
      AllocatedDecoder decoder = (*old_decoders)[i];
      (*old_decoders)[i] = old_decoders->back();
      old_decoders->pop_back();
      return decoder;
    }
  }

  if (external_decoder_factory_ != NULL) {
    webrtc::VideoDecoder* decoder =
        external_decoder_factory_->CreateVideoDecoder(type);
    if (decoder != NULL) {
      return AllocatedDecoder(decoder, type, true);
    }
  }

  if (type == webrtc::kVideoCodecVP8) {
    return AllocatedDecoder(
        webrtc::VideoDecoder::Create(webrtc::VideoDecoder::kVp8), type, false);
  }

  if (type == webrtc::kVideoCodecVP9) {
    return AllocatedDecoder(
        webrtc::VideoDecoder::Create(webrtc::VideoDecoder::kVp9), type, false);
  }

  if (type == webrtc::kVideoCodecH264) {
    return AllocatedDecoder(
        webrtc::VideoDecoder::Create(webrtc::VideoDecoder::kH264), type, false);
  }

  // This shouldn't happen, we should not be trying to create something we don't
  // support.
  RTC_DCHECK(false);
  return AllocatedDecoder(NULL, webrtc::kVideoCodecUnknown, false);
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::SetRecvCodecs(
    const std::vector<VideoCodecSettings>& recv_codecs) {
  std::vector<AllocatedDecoder> old_decoders = allocated_decoders_;
  allocated_decoders_.clear();
  config_.decoders.clear();
  for (size_t i = 0; i < recv_codecs.size(); ++i) {
    AllocatedDecoder allocated_decoder =
        CreateOrReuseVideoDecoder(&old_decoders, recv_codecs[i].codec);
    allocated_decoders_.push_back(allocated_decoder);

    webrtc::VideoReceiveStream::Decoder decoder;
    decoder.decoder = allocated_decoder.decoder;
    decoder.payload_type = recv_codecs[i].codec.id;
    decoder.payload_name = recv_codecs[i].codec.name;
    config_.decoders.push_back(decoder);
  }

  // TODO(pbos): Reconfigure RTX based on incoming recv_codecs.
  config_.rtp.fec = recv_codecs.front().fec;
  config_.rtp.nack.rtp_history_ms =
      HasNack(recv_codecs.begin()->codec) ? kNackHistoryMs : 0;

  ClearDecoders(&old_decoders);
  LOG(LS_INFO) << "RecreateWebRtcStream (recv) because of SetRecvCodecs: "
               << CodecSettingsVectorToString(recv_codecs);
  RecreateWebRtcStream();
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::SetLocalSsrc(
    uint32_t local_ssrc) {
  // TODO(pbos): Consider turning this sanity check into a RTC_DCHECK. You
  // should not be able to create a sender with the same SSRC as a receiver, but
  // right now this can't be done due to unittests depending on receiving what
  // they are sending from the same MediaChannel.
  if (local_ssrc == config_.rtp.remote_ssrc) {
    LOG(LS_INFO) << "Ignoring call to SetLocalSsrc because parameters are "
                    "unchanged; local_ssrc=" << local_ssrc;
    return;
  }

  config_.rtp.local_ssrc = local_ssrc;
  LOG(LS_INFO)
      << "RecreateWebRtcStream (recv) because of SetLocalSsrc; local_ssrc="
      << local_ssrc;
  RecreateWebRtcStream();
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::SetNackAndRemb(
    bool nack_enabled, bool remb_enabled) {
  int nack_history_ms = nack_enabled ? kNackHistoryMs : 0;
  if (config_.rtp.nack.rtp_history_ms == nack_history_ms &&
      config_.rtp.remb == remb_enabled) {
    LOG(LS_INFO) << "Ignoring call to SetNackAndRemb because parameters are "
                    "unchanged; nack=" << nack_enabled
                 << ", remb=" << remb_enabled;
    return;
  }
  config_.rtp.remb = remb_enabled;
  config_.rtp.nack.rtp_history_ms = nack_history_ms;
  LOG(LS_INFO) << "RecreateWebRtcStream (recv) because of SetNackAndRemb; nack="
               << nack_enabled << ", remb=" << remb_enabled;
  RecreateWebRtcStream();
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::SetRtpExtensions(
    const std::vector<webrtc::RtpExtension>& extensions) {
  config_.rtp.extensions = extensions;
  LOG(LS_INFO) << "RecreateWebRtcStream (recv) because of SetRtpExtensions";
  RecreateWebRtcStream();
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::RecreateWebRtcStream() {
  if (stream_ != NULL) {
    call_->DestroyVideoReceiveStream(stream_);
  }
  stream_ = call_->CreateVideoReceiveStream(config_);
  stream_->Start();
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::ClearDecoders(
    std::vector<AllocatedDecoder>* allocated_decoders) {
  for (size_t i = 0; i < allocated_decoders->size(); ++i) {
    if ((*allocated_decoders)[i].external) {
      external_decoder_factory_->DestroyVideoDecoder(
          (*allocated_decoders)[i].external_decoder);
    }
    delete (*allocated_decoders)[i].decoder;
  }
  allocated_decoders->clear();
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::RenderFrame(
    const webrtc::VideoFrame& frame,
    int time_to_render_ms) {
  rtc::CritScope crit(&renderer_lock_);

  if (first_frame_timestamp_ < 0)
    first_frame_timestamp_ = frame.timestamp();
  int64_t rtp_time_elapsed_since_first_frame =
      (timestamp_wraparound_handler_.Unwrap(frame.timestamp()) -
       first_frame_timestamp_);
  int64_t elapsed_time_ms = rtp_time_elapsed_since_first_frame /
                            (cricket::kVideoCodecClockrate / 1000);
  if (frame.ntp_time_ms() > 0)
    estimated_remote_start_ntp_time_ms_ = frame.ntp_time_ms() - elapsed_time_ms;

  if (renderer_ == NULL) {
    LOG(LS_WARNING) << "VideoReceiveStream not connected to a VideoRenderer.";
    return;
  }

  if (frame.width() != last_width_ || frame.height() != last_height_) {
    SetSize(frame.width(), frame.height());
  }

  const WebRtcVideoFrame render_frame(
      frame.video_frame_buffer(),
      frame.render_time_ms() * rtc::kNumNanosecsPerMillisec, frame.rotation());
  renderer_->RenderFrame(&render_frame);
}

bool WebRtcVideoChannel2::WebRtcVideoReceiveStream::IsTextureSupported() const {
  return true;
}

bool WebRtcVideoChannel2::WebRtcVideoReceiveStream::IsDefaultStream() const {
  return default_stream_;
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::SetRenderer(
    cricket::VideoRenderer* renderer) {
  rtc::CritScope crit(&renderer_lock_);
  renderer_ = renderer;
  if (renderer_ != NULL && last_width_ != -1) {
    SetSize(last_width_, last_height_);
  }
}

VideoRenderer* WebRtcVideoChannel2::WebRtcVideoReceiveStream::GetRenderer() {
  // TODO(pbos): Remove GetRenderer and all uses of it, it's thread-unsafe by
  // design.
  rtc::CritScope crit(&renderer_lock_);
  return renderer_;
}

void WebRtcVideoChannel2::WebRtcVideoReceiveStream::SetSize(int width,
                                                            int height) {
  rtc::CritScope crit(&renderer_lock_);
  if (!renderer_->SetSize(width, height, 0)) {
    LOG(LS_ERROR) << "Could not set renderer size.";
  }
  last_width_ = width;
  last_height_ = height;
}

std::string
WebRtcVideoChannel2::WebRtcVideoReceiveStream::GetCodecNameFromPayloadType(
    int payload_type) {
  for (const webrtc::VideoReceiveStream::Decoder& decoder : config_.decoders) {
    if (decoder.payload_type == payload_type) {
      return decoder.payload_name;
    }
  }
  return "";
}

VideoReceiverInfo
WebRtcVideoChannel2::WebRtcVideoReceiveStream::GetVideoReceiverInfo() {
  VideoReceiverInfo info;
  info.ssrc_groups = ssrc_groups_;
  info.add_ssrc(config_.rtp.remote_ssrc);
  webrtc::VideoReceiveStream::Stats stats = stream_->GetStats();
  info.bytes_rcvd = stats.rtp_stats.transmitted.payload_bytes +
                    stats.rtp_stats.transmitted.header_bytes +
                    stats.rtp_stats.transmitted.padding_bytes;
  info.packets_rcvd = stats.rtp_stats.transmitted.packets;
  info.packets_lost = stats.rtcp_stats.cumulative_lost;
  info.fraction_lost =
      static_cast<float>(stats.rtcp_stats.fraction_lost) / (1 << 8);

  info.framerate_rcvd = stats.network_frame_rate;
  info.framerate_decoded = stats.decode_frame_rate;
  info.framerate_output = stats.render_frame_rate;

  {
    rtc::CritScope frame_cs(&renderer_lock_);
    info.frame_width = last_width_;
    info.frame_height = last_height_;
    info.capture_start_ntp_time_ms = estimated_remote_start_ntp_time_ms_;
  }

  info.decode_ms = stats.decode_ms;
  info.max_decode_ms = stats.max_decode_ms;
  info.current_delay_ms = stats.current_delay_ms;
  info.target_delay_ms = stats.target_delay_ms;
  info.jitter_buffer_ms = stats.jitter_buffer_ms;
  info.min_playout_delay_ms = stats.min_playout_delay_ms;
  info.render_delay_ms = stats.render_delay_ms;

  info.codec_name = GetCodecNameFromPayloadType(stats.current_payload_type);

  info.firs_sent = stats.rtcp_packet_type_counts.fir_packets;
  info.plis_sent = stats.rtcp_packet_type_counts.pli_packets;
  info.nacks_sent = stats.rtcp_packet_type_counts.nack_packets;

  return info;
}

WebRtcVideoChannel2::VideoCodecSettings::VideoCodecSettings()
    : rtx_payload_type(-1) {}

bool WebRtcVideoChannel2::VideoCodecSettings::operator==(
    const WebRtcVideoChannel2::VideoCodecSettings& other) const {
  return codec == other.codec &&
         fec.ulpfec_payload_type == other.fec.ulpfec_payload_type &&
         fec.red_payload_type == other.fec.red_payload_type &&
         fec.red_rtx_payload_type == other.fec.red_rtx_payload_type &&
         rtx_payload_type == other.rtx_payload_type;
}

bool WebRtcVideoChannel2::VideoCodecSettings::operator!=(
    const WebRtcVideoChannel2::VideoCodecSettings& other) const {
  return !(*this == other);
}

std::vector<WebRtcVideoChannel2::VideoCodecSettings>
WebRtcVideoChannel2::MapCodecs(const std::vector<VideoCodec>& codecs) {
  RTC_DCHECK(!codecs.empty());

  std::vector<VideoCodecSettings> video_codecs;
  std::map<int, bool> payload_used;
  std::map<int, VideoCodec::CodecType> payload_codec_type;
  // |rtx_mapping| maps video payload type to rtx payload type.
  std::map<int, int> rtx_mapping;

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
        RTC_DCHECK(fec_settings.red_payload_type == -1);
        fec_settings.red_payload_type = in_codec.id;
        continue;
      }

      case VideoCodec::CODEC_ULPFEC: {
        // ULPFEC payload type, should not have duplicates.
        RTC_DCHECK(fec_settings.ulpfec_payload_type == -1);
        fec_settings.ulpfec_payload_type = in_codec.id;
        continue;
      }

      case VideoCodec::CODEC_RTX: {
        int associated_payload_type;
        if (!in_codec.GetParam(kCodecParamAssociatedPayloadType,
                               &associated_payload_type) ||
            !IsValidRtpPayloadType(associated_payload_type)) {
          LOG(LS_ERROR)
              << "RTX codec with invalid or no associated payload type: "
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
  RTC_DCHECK(!video_codecs.empty());

  for (std::map<int, int>::const_iterator it = rtx_mapping.begin();
       it != rtx_mapping.end();
       ++it) {
    if (!payload_used[it->first]) {
      LOG(LS_ERROR) << "RTX mapped to payload not in codec list.";
      return std::vector<VideoCodecSettings>();
    }
    if (payload_codec_type[it->first] != VideoCodec::CODEC_VIDEO &&
        payload_codec_type[it->first] != VideoCodec::CODEC_RED) {
      LOG(LS_ERROR) << "RTX not mapped to regular video codec or RED codec.";
      return std::vector<VideoCodecSettings>();
    }

    if (it->first == fec_settings.red_payload_type) {
      fec_settings.red_rtx_payload_type = it->second;
    }
  }

  for (size_t i = 0; i < video_codecs.size(); ++i) {
    video_codecs[i].fec = fec_settings;
    if (rtx_mapping[video_codecs[i].codec.id] != 0 &&
        rtx_mapping[video_codecs[i].codec.id] !=
            fec_settings.red_payload_type) {
      video_codecs[i].rtx_payload_type = rtx_mapping[video_codecs[i].codec.id];
    }
  }

  return video_codecs;
}

}  // namespace cricket

#endif  // HAVE_WEBRTC_VIDEO
