/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#ifdef HAVE_WEBRTC

#include "talk/session/phone/webrtcvideoengine.h"

#include "talk/base/common.h"
#include "talk/base/buffer.h"
#include "talk/base/byteorder.h"
#include "talk/base/logging.h"
#include "talk/base/stringutils.h"
#include "talk/session/phone/webrtcvoiceengine.h"
#include "talk/session/phone/webrtcvideoframe.h"
#include "talk/session/phone/webrtcvie.h"
#include "talk/session/phone/webrtcvoe.h"

namespace cricket {

static const int kDefaultLogSeverity = talk_base::LS_WARNING;
static const int kStartVideoBitrate = 300;
static const int kMaxVideoBitrate = 1000;

class WebRtcRenderAdapter : public webrtc::ExternalRenderer {
 public:
  explicit WebRtcRenderAdapter(VideoRenderer* renderer)
      : renderer_(renderer) {
  }

  virtual int FrameSizeChange(unsigned int width, unsigned int height,
                              unsigned int /*number_of_streams*/) {
    if (renderer_ == NULL) {
      return 0;
    }
    width_ = width;
    height_ = height;
    return renderer_->SetSize(width_, height_, 0) ? 0 : -1;
  }

  virtual int DeliverFrame(unsigned char* buffer, int buffer_size,
                           unsigned int time_stamp) {
    if (renderer_ == NULL) {
      return 0;
    }
    WebRtcVideoFrame video_frame;
    // TODO(ronghuawu): Currently by the time DeliverFrame got called,
    // ViE expects the frame will be rendered ASAP. However, the libjingle
    // renderer may have its own internal delays. Can you disable the buffering
    // inside ViE and surface the timing information to this callback?
    video_frame.Attach(buffer, buffer_size, width_, height_, 0, time_stamp);
    int ret = renderer_->RenderFrame(&video_frame) ? 0 : -1;
    uint8* buffer_temp;
    size_t buffer_size_temp;
    video_frame.Detach(&buffer_temp, &buffer_size_temp);
    return ret;
  }

  virtual ~WebRtcRenderAdapter() {}

 private:
  VideoRenderer* renderer_;
  unsigned int width_;
  unsigned int height_;
};

const WebRtcVideoEngine::VideoCodecPref
    WebRtcVideoEngine::kVideoCodecPrefs[] = {
    {"VP8", 104, 0},
    {"H264", 105, 1}
};

WebRtcVideoEngine::WebRtcVideoEngine()
    : vie_wrapper_(new ViEWrapper()),
      capture_(NULL),
      external_capture_(false),
      capture_id_(-1),
      renderer_(webrtc::VideoRender::CreateVideoRender(0, NULL,
                  false, webrtc::kRenderExternal)),
      voice_engine_(NULL),
      log_level_(kDefaultLogSeverity),
      capture_started_(false) {
}

WebRtcVideoEngine::WebRtcVideoEngine(WebRtcVoiceEngine* voice_engine,
                                     webrtc::VideoCaptureModule* capture)
    : vie_wrapper_(new ViEWrapper()),
      capture_(capture),
      external_capture_(true),
      capture_id_(-1),
      renderer_(webrtc::VideoRender::CreateVideoRender(0, NULL,
                  false, webrtc::kRenderExternal)),
      voice_engine_(voice_engine),
      log_level_(kDefaultLogSeverity),
      capture_started_(false) {
}

WebRtcVideoEngine::WebRtcVideoEngine(WebRtcVoiceEngine* voice_engine,
                                     ViEWrapper* vie_wrapper)
    : vie_wrapper_(vie_wrapper),
      capture_(NULL),
      external_capture_(false),
      capture_id_(-1),
      renderer_(webrtc::VideoRender::CreateVideoRender(0, NULL,
                  false, webrtc::kRenderExternal)),
      voice_engine_(voice_engine),
      log_level_(kDefaultLogSeverity),
      capture_started_(false) {
}

WebRtcVideoEngine::~WebRtcVideoEngine() {
  LOG(LS_INFO) << " WebRtcVideoEngine::~WebRtcVideoEngine";
  vie_wrapper_->engine()->SetTraceCallback(NULL);
  Terminate();
  vie_wrapper_.reset();
  if (capture_) {
    webrtc::VideoCaptureModule::Destroy(capture_);
  }
  if (renderer_) {
    webrtc::VideoRender::DestroyVideoRender(renderer_);
  }
}

bool WebRtcVideoEngine::Init() {
  LOG(LS_INFO) << "WebRtcVideoEngine::Init";
  ApplyLogging();
  if (vie_wrapper_->engine()->SetTraceCallback(this) != 0) {
    LOG_RTCERR1(SetTraceCallback, this);
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

  if (vie_wrapper_->base()->Init() != 0) {
    LOG_RTCERR0(Init);
    return false;
  }

  if (!voice_engine_) {
    LOG(LS_WARNING) << "NULL voice engine";
  } else if ((vie_wrapper_->base()->SetVoiceEngine(
      voice_engine_->voe()->engine())) != 0) {
    LOG_RTCERR0(SetVoiceEngine);
    return false;
  }

  if ((vie_wrapper_->base()->RegisterObserver(*this)) != 0) {
    LOG_RTCERR0(RegisterObserver);
    return false;
  }

  int ncodecs = vie_wrapper_->codec()->NumberOfCodecs();
  for (int i = 0; i < ncodecs; ++i) {
    webrtc::VideoCodec wcodec;
    if ((vie_wrapper_->codec()->GetCodec(i, wcodec) == 0) &&
        (strncmp(wcodec.plName, "I420", 4) != 0) &&
        (strncmp(wcodec.plName, "ULPFEC", 4) != 0) &&
        (strncmp(wcodec.plName, "RED", 4) != 0)) {
      // ignore I420, FEC(RED and ULPFEC)
      VideoCodec codec(wcodec.plType, wcodec.plName, wcodec.width,
                       wcodec.height, wcodec.maxFramerate, i);
      LOG(LS_INFO) << codec.ToString();
      video_codecs_.push_back(codec);
    }
  }

  if (vie_wrapper_->render()->RegisterVideoRenderModule(*renderer_) != 0) {
    LOG_RTCERR0(RegisterVideoRenderModule);
    return false;
  }

  std::sort(video_codecs_.begin(), video_codecs_.end(),
            &VideoCodec::Preferable);
  return true;
}

void WebRtcVideoEngine::PerformanceAlarm(const unsigned int cpu_load) {
  LOG(LS_INFO) << "WebRtcVideoEngine::PerformanceAlarm";
}

// Ignore spammy trace messages, mostly from the stats API when we haven't
// gotten RTCP info yet from the remote side.
static bool ShouldIgnoreTrace(const std::string& trace) {
  static const char* kTracesToIgnore[] = {
    "\tfailed to GetReportBlockInformation",
    NULL
  };
  for (const char* const* p = kTracesToIgnore; *p; ++p) {
    if (trace.find(*p) == 0) {
      return true;
    }
  }
  return false;
}

void WebRtcVideoEngine::Print(const webrtc::TraceLevel level,
                              const char* trace, const int length) {
  talk_base::LoggingSeverity sev = talk_base::LS_VERBOSE;
  if (level == webrtc::kTraceError || level == webrtc::kTraceCritical)
    sev = talk_base::LS_ERROR;
  else if (level == webrtc::kTraceWarning)
    sev = talk_base::LS_WARNING;
  else if (level == webrtc::kTraceStateInfo || level == webrtc::kTraceInfo)
    sev = talk_base::LS_INFO;

  if (sev >= log_level_) {
    // Skip past boilerplate prefix text
    if (length < 72) {
      std::string msg(trace, length);
      LOG(LS_ERROR) << "Malformed webrtc log message: ";
      LOG_V(sev) << msg;
    } else {
      std::string msg(trace + 71, length - 72);
      if (!ShouldIgnoreTrace(msg)) {
        LOG_V(sev) << "WebRtc ViE:" << msg;
      }
    }
  }
}

int WebRtcVideoEngine::GetCodecPreference(const char* name) {
  for (size_t i = 0; i < ARRAY_SIZE(kVideoCodecPrefs); ++i) {
    if (strcmp(kVideoCodecPrefs[i].payload_name, name) == 0) {
     return kVideoCodecPrefs[i].pref;
    }
  }
  return -1;
}

void WebRtcVideoEngine::ApplyLogging() {
  int filter = 0;
  switch (log_level_) {
    case talk_base::LS_VERBOSE: filter |= webrtc::kTraceAll;
    case talk_base::LS_INFO: filter |= webrtc::kTraceStateInfo;
    case talk_base::LS_WARNING: filter |= webrtc::kTraceWarning;
    case talk_base::LS_ERROR: filter |=
        webrtc::kTraceError | webrtc::kTraceCritical;
  }
}

void WebRtcVideoEngine::Terminate() {
  LOG(LS_INFO) << "WebRtcVideoEngine::Terminate";
  SetCapture(false);
  if (local_renderer_.get()) {
    // If the renderer already set, stop it first
    if (vie_wrapper_->render()->StopRender(capture_id_) != 0)
      LOG_RTCERR1(StopRender, capture_id_);
  }

  if (vie_wrapper_->render()->DeRegisterVideoRenderModule(*renderer_) != 0)
    LOG_RTCERR0(DeRegisterVideoRenderModule);

  if ((vie_wrapper_->base()->DeregisterObserver()) != 0)
    LOG_RTCERR0(DeregisterObserver);

  if ((vie_wrapper_->base()->SetVoiceEngine(NULL)) != 0)
    LOG_RTCERR0(SetVoiceEngine);

  if (vie_wrapper_->engine()->SetTraceCallback(NULL) != 0)
    LOG_RTCERR0(SetTraceCallback);
}

int WebRtcVideoEngine::GetCapabilities() {
  return MediaEngine::VIDEO_RECV | MediaEngine::VIDEO_SEND;
}

bool WebRtcVideoEngine::SetOptions(int options) {
  return true;
}

bool WebRtcVideoEngine::ReleaseCaptureDevice() {
  if (capture_id_ != -1) {
    // Stop capture
    SetCapture(false);
    // DisconnectCaptureDevice
    WebRtcVideoMediaChannel* channel;
    for (VideoChannels::const_iterator it = channels_.begin();
        it != channels_.end(); ++it) {
      ASSERT(*it != NULL);
      channel = *it;
      vie_wrapper_->capture()->DisconnectCaptureDevice(
          channel->video_channel());
    }
    // ReleaseCaptureDevice
    vie_wrapper_->capture()->ReleaseCaptureDevice(capture_id_);
    capture_id_ = -1;
  }

  return true;
}

bool WebRtcVideoEngine::SetCaptureDevice(const Device* cam) {
  ASSERT(vie_wrapper_.get());
  ASSERT(cam != NULL);

  ReleaseCaptureDevice();

  webrtc::ViECapture* vie_capture = vie_wrapper_->capture();

  // There's an external VCM
  if (capture_) {
    if (vie_capture->AllocateCaptureDevice(*capture_, capture_id_) != 0)
      ASSERT(capture_id_ == -1);
  } else if (!external_capture_) {
    const unsigned int KMaxDeviceNameLength = 128;
    const unsigned int KMaxUniqueIdLength = 256;
    char device_name[KMaxDeviceNameLength];
    char device_id[KMaxUniqueIdLength];
    bool found = false;
    for (int i = 0; i < vie_capture->NumberOfCaptureDevices(); ++i) {
      memset(device_name, 0, KMaxDeviceNameLength);
      memset(device_id, 0, KMaxUniqueIdLength);
      if (vie_capture->GetCaptureDevice(i, device_name, KMaxDeviceNameLength,
                                        device_id, KMaxUniqueIdLength) == 0) {
        // TODO(ronghuawu): We should only compare the device_id here,
        // however the devicemanager and webrtc use different format for th v4l2
        // device id. So here we also compare the device_name for now.
        // For example "usb-0000:00:1d.7-6" vs "/dev/video0".
        if ((cam->name.compare(reinterpret_cast<char*>(device_name)) == 0) ||
            (cam->id.compare(reinterpret_cast<char*>(device_id)) == 0)) {
          LOG(INFO) << "Found video capture device: " << device_name;
          found = true;
          break;
        }
      }
    }
    if (!found)
      return false;
    if (vie_capture->AllocateCaptureDevice(device_id, KMaxUniqueIdLength,
                                           capture_id_) != 0)
      ASSERT(capture_id_ == -1);
  }

  if (capture_id_ != -1) {
    // Connect to all the channels
    WebRtcVideoMediaChannel* channel;
    for (VideoChannels::const_iterator it = channels_.begin();
         it != channels_.end(); ++it) {
      ASSERT(*it != NULL);
      channel = *it;
      vie_capture->ConnectCaptureDevice(capture_id_, channel->video_channel());
    }
    SetCapture(true);
  }

  return (capture_id_ != -1);
}

bool WebRtcVideoEngine::SetCaptureModule(webrtc::VideoCaptureModule* vcm) {
  ReleaseCaptureDevice();
  if (capture_) {
    webrtc::VideoCaptureModule::Destroy(capture_);
  }
  capture_ = vcm;
  external_capture_ = true;
  return true;
}

bool WebRtcVideoEngine::SetLocalRenderer(VideoRenderer* renderer) {
  if (local_renderer_.get()) {
    // If the renderer already set, stop it first
    vie_wrapper_->render()->StopRender(capture_id_);
    vie_wrapper_->render()->RemoveRenderer(capture_id_);
  }
  local_renderer_.reset(new WebRtcRenderAdapter(renderer));

  int ret;
  ret = vie_wrapper_->render()->AddRenderer(capture_id_,
                                            webrtc::kVideoI420,
                                            local_renderer_.get());
  if (ret != 0)
    return false;
  ret = vie_wrapper_->render()->StartRender(capture_id_);
  return (ret == 0);
}

CaptureResult WebRtcVideoEngine::SetCapture(bool capture) {
  if ((capture_started_ != capture) && (capture_id_ != -1)) {
    int ret;
    if (capture)
      ret = vie_wrapper_->capture()->StartCapture(capture_id_);
    else
      ret = vie_wrapper_->capture()->StopCapture(capture_id_);
    if (ret != 0)
      return CR_NO_DEVICE;
    capture_started_ = capture;
  }
  return CR_SUCCESS;
}

const std::vector<VideoCodec>& WebRtcVideoEngine::codecs() const {
  return video_codecs_;
}

void WebRtcVideoEngine::SetLogging(int min_sev, const char* filter) {
  log_level_ = min_sev;
  ApplyLogging();
}

int WebRtcVideoEngine::GetLastEngineError() {
  return vie_wrapper_->error();
}

bool WebRtcVideoEngine::SetDefaultEncoderConfig(
    const VideoEncoderConfig& config) {
  default_encoder_config_ = config;
  return true;
}

WebRtcVideoMediaChannel* WebRtcVideoEngine::CreateChannel(
    VoiceMediaChannel* voice_channel) {
  WebRtcVideoMediaChannel* channel =
      new WebRtcVideoMediaChannel(this, voice_channel);
  if (channel) {
    if (!channel->Init()) {
      delete channel;
      channel = NULL;
    }
  }
  return channel;
}

bool WebRtcVideoEngine::FindCodec(const VideoCodec& codec) {
  for (size_t i = 0; i < video_codecs_.size(); ++i) {
    if (video_codecs_[i].Matches(codec)) {
      return true;
    }
  }
  return false;
}

void WebRtcVideoEngine::ConvertToCricketVideoCodec(
    const webrtc::VideoCodec& in_codec, VideoCodec& out_codec) {
  out_codec.id = in_codec.plType;
  out_codec.name = in_codec.plName;
  out_codec.width = in_codec.width;
  out_codec.height = in_codec.height;
  out_codec.framerate = in_codec.maxFramerate;
}

bool WebRtcVideoEngine::ConvertFromCricketVideoCodec(
    const VideoCodec& in_codec, webrtc::VideoCodec& out_codec) {
  bool found = false;
  int ncodecs = vie_wrapper_->codec()->NumberOfCodecs();
  for (int i = 0; i < ncodecs; ++i) {
    if ((vie_wrapper_->codec()->GetCodec(i, out_codec) == 0) &&
        (strncmp(out_codec.plName,
                 in_codec.name.c_str(),
                 webrtc::kPayloadNameSize - 1) == 0)) {
      found = true;
      break;
    }
  }

  if (!found) {
    LOG(LS_ERROR) << "invalid codec type";
    return false;
  }

  if (in_codec.id != 0)
    out_codec.plType = in_codec.id;

  if (in_codec.width != 0)
    out_codec.width = in_codec.width;

  if (in_codec.height != 0)
    out_codec.height = in_codec.height;

  if (in_codec.framerate != 0)
    out_codec.maxFramerate = in_codec.framerate;

  out_codec.maxBitrate = kMaxVideoBitrate;
  out_codec.startBitrate = kStartVideoBitrate;
  out_codec.minBitrate = kStartVideoBitrate;

  return true;
}

int WebRtcVideoEngine::GetLastVideoEngineError() {
  return vie_wrapper_->base()->LastError();
}

void WebRtcVideoEngine::RegisterChannel(WebRtcVideoMediaChannel *channel) {
  channels_.push_back(channel);
}

void WebRtcVideoEngine::UnregisterChannel(WebRtcVideoMediaChannel *channel) {
  VideoChannels::iterator i = std::find(channels_.begin(),
                                      channels_.end(),
                                      channel);
  if (i != channels_.end()) {
    channels_.erase(i);
  }
}




// WebRtcVideoMediaChannel

WebRtcVideoMediaChannel::WebRtcVideoMediaChannel(
    WebRtcVideoEngine* engine, VoiceMediaChannel* channel)
    : engine_(engine),
      voice_channel_(channel),
      vie_channel_(-1),
      sending_(false),
      render_started_(false),
      send_codec_(NULL) {
  engine->RegisterChannel(this);
}

bool WebRtcVideoMediaChannel::Init() {
  bool ret = true;
  if (engine_->video_engine()->base()->CreateChannel(vie_channel_) != 0) {
    LOG_RTCERR1(CreateChannel, vie_channel_);
    return false;
  }

  LOG(LS_INFO) << "WebRtcVideoMediaChannel::Init "
               << "video_channel " << vie_channel_ << " created";
  // connect audio channel
  if (voice_channel_) {
    WebRtcVoiceMediaChannel* channel =
        static_cast<WebRtcVoiceMediaChannel*> (voice_channel_);
    if (engine_->video_engine()->base()->ConnectAudioChannel(
        vie_channel_, channel->voe_channel()) != 0) {
      LOG(LS_WARNING) << "ViE ConnectAudioChannel failed"
                   << "A/V not synchronized";
      // Don't set ret to false;
    }
  }

  // Register external transport
  if (engine_->video_engine()->network()->RegisterSendTransport(
      vie_channel_, *this) != 0) {
    ret = false;
  } else {
    EnableRtcp();
    EnablePLI();
  }
  return ret;
}

WebRtcVideoMediaChannel::~WebRtcVideoMediaChannel() {
  // Stop and remote renderer
  SetRender(false);
  if (engine()->video_engine()->render()->RemoveRenderer(vie_channel_)
      == -1) {
    LOG_RTCERR1(RemoveRenderer, vie_channel_);
  }

  // DeRegister external transport
  if (engine()->video_engine()->network()->DeregisterSendTransport(
      vie_channel_) == -1) {
    LOG_RTCERR1(DeregisterSendTransport, vie_channel_);
  }

  // Unregister RtcChannel with the engine.
  engine()->UnregisterChannel(this);

  // Delete VideoChannel
  if (engine()->video_engine()->base()->DeleteChannel(vie_channel_) == -1) {
    LOG_RTCERR1(DeleteChannel, vie_channel_);
  }
}

bool WebRtcVideoMediaChannel::SetRecvCodecs(
    const std::vector<VideoCodec>& codecs) {
  bool ret = true;
  for (std::vector<VideoCodec>::const_iterator iter = codecs.begin();
      iter != codecs.end(); ++iter) {
    if (engine()->FindCodec(*iter)) {
      webrtc::VideoCodec wcodec;
      if (engine()->ConvertFromCricketVideoCodec(*iter, wcodec)) {
        if (engine()->video_engine()->codec()->SetReceiveCodec(
            vie_channel_,  wcodec) != 0) {
          LOG_RTCERR2(SetReceiveCodec, vie_channel_, wcodec.plName);
          ret = false;
        }
      }
    } else {
      LOG(LS_INFO) << "Unknown codec" << iter->name;
      ret = false;
    }
  }

  // make channel ready to receive packets
  if (ret) {
    if (engine()->video_engine()->base()->StartReceive(vie_channel_) != 0) {
      LOG_RTCERR1(StartReceive, vie_channel_);
      ret = false;
    }
  }
  return ret;
}

bool WebRtcVideoMediaChannel::SetSendCodecs(
    const std::vector<VideoCodec>& codecs) {
  if (sending_) {
    LOG(LS_ERROR) << "channel is alredy sending";
    return false;
  }

  // match with local video codec list
  std::vector<webrtc::VideoCodec> send_codecs;
  for (std::vector<VideoCodec>::const_iterator iter = codecs.begin();
      iter != codecs.end(); ++iter) {
    if (engine()->FindCodec(*iter)) {
      webrtc::VideoCodec wcodec;
      if (engine()->ConvertFromCricketVideoCodec(*iter, wcodec))
        send_codecs.push_back(wcodec);
    }
  }

  // if none matches, return with set
  if (send_codecs.empty()) {
    LOG(LS_ERROR) << "No matching codecs avilable";
    return false;
  }

  // select the first matched codec
  const webrtc::VideoCodec& codec(send_codecs[0]);
  send_codec_.reset(new webrtc::VideoCodec(codec));
  if (engine()->video_engine()->codec()->SetSendCodec(
      vie_channel_, codec) != 0) {
    LOG_RTCERR2(SetSendCodec, vie_channel_, codec.plName);
    return false;
  }
  return true;
}

bool WebRtcVideoMediaChannel::SetRender(bool render) {
  if (render != render_started_) {
    int ret;
    if (render) {
      ret = engine()->video_engine()->render()->StartRender(vie_channel_);
    } else {
      ret = engine()->video_engine()->render()->StopRender(vie_channel_);
    }
    if (ret != 0) {
      return false;
    }
    render_started_ = render;
  }
  return true;
}

bool WebRtcVideoMediaChannel::SetSend(bool send) {
  if (send == sending()) {
    return true;  // no action required
  }

  bool ret = true;
  if (send) {  // enable
    if (engine()->video_engine()->base()->StartSend(vie_channel_) != 0) {
      LOG_RTCERR1(StartSend, vie_channel_);
      ret = false;
    }
  } else {  // disable
    if (engine()->video_engine()->base()->StopSend(vie_channel_) != 0) {
      LOG_RTCERR1(StopSend, vie_channel_);
      ret = false;
    }
  }
  if (ret)
    sending_ = send;

  return ret;
}

bool WebRtcVideoMediaChannel::AddStream(uint32 ssrc, uint32 voice_ssrc) {
  return false;
}

bool WebRtcVideoMediaChannel::RemoveStream(uint32 ssrc) {
  return false;
}

bool WebRtcVideoMediaChannel::SetRenderer(
    uint32 ssrc, VideoRenderer* renderer) {
  ASSERT(vie_channel_ != -1);
  if (ssrc != 0)
    return false;
  if (remote_renderer_.get()) {
    // If the renderer already set, stop it first
    engine_->video_engine()->render()->StopRender(vie_channel_);
    engine_->video_engine()->render()->RemoveRenderer(vie_channel_);
  }
  remote_renderer_.reset(new WebRtcRenderAdapter(renderer));

  if (engine_->video_engine()->render()->AddRenderer(vie_channel_,
      webrtc::kVideoI420, remote_renderer_.get()) != 0) {
    LOG_RTCERR3(AddRenderer, vie_channel_, webrtc::kVideoI420,
                remote_renderer_.get());
    remote_renderer_.reset();
    return false;
  }

  if (engine_->video_engine()->render()->StartRender(vie_channel_) != 0) {
    LOG_RTCERR1(StartRender, vie_channel_);
    return false;
  }

  return true;
}

bool WebRtcVideoMediaChannel::GetStats(VideoMediaInfo* info) {
  VideoSenderInfo sinfo;
  memset(&sinfo, 0, sizeof(sinfo));

  unsigned int ssrc;
  if (engine_->video_engine()->rtp()->GetLocalSSRC(vie_channel_,
                                                   ssrc) != 0) {
    LOG_RTCERR2(GetLocalSSRC, vie_channel_, ssrc);
    return false;
  }
  sinfo.ssrc = ssrc;

  unsigned int cumulative_lost, extended_max, jitter;
  int rtt_ms;
  uint16 fraction_lost;

  if (engine_->video_engine()->rtp()->GetReceivedRTCPStatistics(vie_channel_,
          fraction_lost, cumulative_lost, extended_max, jitter, rtt_ms) != 0) {
    LOG_RTCERR6(GetReceivedRTCPStatistics, vie_channel_,
        fraction_lost, cumulative_lost, extended_max, jitter, rtt_ms);
    return false;
  }

  sinfo.fraction_lost = fraction_lost;
  sinfo.packets_lost = cumulative_lost;
  sinfo.rtt_ms = rtt_ms;

  unsigned int bytes_sent, packets_sent, bytes_recv, packets_recv;
  if (engine_->video_engine()->rtp()->GetRTPStatistics(vie_channel_,
          bytes_sent, packets_sent, bytes_recv, packets_recv) != 0) {
    LOG_RTCERR5(GetRTPStatistics, vie_channel_,
        bytes_sent, packets_sent, bytes_recv, packets_recv);
    return false;
  }
  sinfo.packets_sent = packets_sent;
  sinfo.bytes_sent = bytes_sent;
  sinfo.packets_lost = -1;
  sinfo.packets_cached = -1;

  info->senders.push_back(sinfo);

  // build receiver info.
  // reusing the above local variables
  VideoReceiverInfo rinfo;
  memset(&rinfo, 0, sizeof(rinfo));
  if (engine_->video_engine()->rtp()->GetSentRTCPStatistics(vie_channel_,
          fraction_lost, cumulative_lost, extended_max, jitter, rtt_ms) != 0) {
    LOG_RTCERR6(GetSentRTCPStatistics, vie_channel_,
        fraction_lost, cumulative_lost, extended_max, jitter, rtt_ms);
    return false;
  }
  rinfo.bytes_rcvd = bytes_recv;
  rinfo.packets_rcvd = packets_recv;
  rinfo.fraction_lost = fraction_lost;
  rinfo.packets_lost = cumulative_lost;

  if (engine_->video_engine()->rtp()->GetRemoteSSRC(vie_channel_,
                                                    ssrc) != 0) {
    return false;
  }
  rinfo.ssrc = ssrc;

  // Get codec for wxh
  info->receivers.push_back(rinfo);
  return true;
}

bool WebRtcVideoMediaChannel::SendIntraFrame() {
  bool ret = true;
  if (engine()->video_engine()->codec()->SendKeyFrame(vie_channel_) != 0) {
    LOG_RTCERR1(SendKeyFrame, vie_channel_);
    ret = false;
  }

  return ret;
}

bool WebRtcVideoMediaChannel::RequestIntraFrame() {
  // There is no API exposed to application to request a key frame
  // ViE does this internally when there are errors from decoder
  return false;
}

void WebRtcVideoMediaChannel::OnPacketReceived(talk_base::Buffer* packet) {
  engine()->video_engine()->network()->ReceivedRTPPacket(vie_channel_,
                                                         packet->data(),
                                                         packet->length());
}

void WebRtcVideoMediaChannel::OnRtcpReceived(talk_base::Buffer* packet) {
  engine_->video_engine()->network()->ReceivedRTCPPacket(vie_channel_,
                                                         packet->data(),
                                                         packet->length());
}

void WebRtcVideoMediaChannel::SetSendSsrc(uint32 id) {
  if (!sending_) {
    if (engine()->video_engine()->rtp()->SetLocalSSRC(vie_channel_,
                                                      id) != 0) {
      LOG_RTCERR1(SetLocalSSRC, vie_channel_);
    }
  } else {
    LOG(LS_ERROR) << "Channel already in send state";
  }
}

bool WebRtcVideoMediaChannel::SetRtcpCName(const std::string& cname) {
  if (engine()->video_engine()->rtp()->SetRTCPCName(vie_channel_,
                                                    cname.c_str()) != 0) {
    LOG_RTCERR2(SetRTCPCName, vie_channel_, cname.c_str());
    return false;
  }
  return true;
}

bool WebRtcVideoMediaChannel::Mute(bool on) {
  // stop send??
  return false;
}

bool WebRtcVideoMediaChannel::SetSendBandwidth(bool autobw, int bps) {
  LOG(LS_INFO) << "RtcVideoMediaChanne::SetSendBandwidth";

  if (!send_codec_.get()) {
    LOG(LS_INFO) << "The send codec has not been set up yet.";
    return true;
  }

  if (!autobw) {
    send_codec_->startBitrate = bps;
    send_codec_->minBitrate = bps;
  }
  send_codec_->maxBitrate = bps;

  if (engine()->video_engine()->codec()->SetSendCodec(vie_channel_,
      *send_codec_.get()) != 0) {
    LOG_RTCERR2(SetSendCodec, vie_channel_, send_codec_->plName);
    return false;
  }

  return true;
}

bool WebRtcVideoMediaChannel::SetOptions(int options) {
  return true;
}

void WebRtcVideoMediaChannel::EnableRtcp() {
  engine()->video_engine()->rtp()->SetRTCPStatus(
      vie_channel_, webrtc::kRtcpCompound_RFC4585);
}

void WebRtcVideoMediaChannel::EnablePLI() {
  engine_->video_engine()->rtp()->SetKeyFrameRequestMethod(
      vie_channel_, webrtc::kViEKeyFrameRequestPliRtcp);
}

void WebRtcVideoMediaChannel::EnableTMMBR() {
  engine_->video_engine()->rtp()->SetTMMBRStatus(vie_channel_, true);
}

int WebRtcVideoMediaChannel::SendPacket(int channel, const void* data,
                                        int len) {
  if (!network_interface_) {
    return -1;
  }

  talk_base::Buffer packet(data, len, kMaxRtpPacketLen);
  return network_interface_->SendPacket(&packet) ? len : -1;
}

int WebRtcVideoMediaChannel::SendRTCPPacket(int channel,
                                         const void* data,
                                         int len) {
  if (!network_interface_) {
    return -1;
  }
  talk_base::Buffer packet(data, len, kMaxRtpPacketLen);
  return network_interface_->SendRtcp(&packet) ? len : -1;
}

}  // namespace cricket

#endif  // HAVE_WEBRTC

