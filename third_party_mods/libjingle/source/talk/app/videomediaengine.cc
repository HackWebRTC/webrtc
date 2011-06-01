

#include "talk/app/videomediaengine.h"

#include <iostream>

#ifdef PLATFORM_CHROMIUM
#include "content/renderer/video_capture_chrome.h"
#endif
#include "talk/base/buffer.h"
#include "talk/base/byteorder.h"
#include "talk/base/logging.h"
#include "talk/base/stringutils.h"
#include "talk/app/voicemediaengine.h"

#include "modules/video_capture/main/interface/video_capture.h"

#ifndef ARRAYSIZE
#define ARRAYSIZE(a)  (sizeof(a) / sizeof((a)[0]))
#endif

namespace webrtc {

static const int kDefaultLogSeverity = 3;
static const int kStartVideoBitrate = 300;
static const int kMaxVideoBitrate = 1000;

const RtcVideoEngine::VideoCodecPref RtcVideoEngine::kVideoCodecPrefs[] = {
    {"VP8", 104, 0},
    {"H264", 105, 1}
};

RtcVideoEngine::RtcVideoEngine()
    : video_engine_(new VideoEngineWrapper()),
      capture_(NULL),
      capture_id_(-1),
      voice_engine_(NULL),
      initialized_(false),
      log_level_(kDefaultLogSeverity),
      capture_started_(false){
}

RtcVideoEngine::RtcVideoEngine(RtcVoiceEngine* voice_engine)
    : video_engine_(new VideoEngineWrapper()),
      capture_(NULL),
      capture_id_(-1),
      voice_engine_(voice_engine),
      initialized_(false),
      log_level_(kDefaultLogSeverity),
      capture_started_(false){
}

RtcVideoEngine::~RtcVideoEngine() {
  LOG(LS_VERBOSE) << " RtcVideoEngine::~RtcVideoEngine";
  video_engine_->engine()->SetTraceCallback(NULL);
  Terminate();
}

bool RtcVideoEngine::Init() {
  LOG(LS_VERBOSE) << "RtcVideoEngine::Init";
  ApplyLogging();
  if (video_engine_->engine()->SetTraceCallback(this) != 0) {
    LOG(LS_ERROR) << "SetTraceCallback error";
  }

  bool result = InitVideoEngine(voice_engine_);
  if (result) {
    LOG(LS_INFO) << "VideoEngine Init done";
  } else {
    LOG(LS_ERROR) << "VideoEngine Init failed, releasing";
    Terminate();
  }
  return result;
}

bool RtcVideoEngine::InitVideoEngine(RtcVoiceEngine* voice_engine) {
  LOG(LS_VERBOSE) << "RtcVideoEngine::InitVideoEngine";

  bool ret = true;
  if (video_engine_->base()->Init() != 0) {
    LOG(LS_ERROR) << "VideoEngine Init method failed";
    ret = false;
  }

  if (!voice_engine) {
    LOG(LS_WARNING) << "NULL voice engine";
  } else if ((video_engine_->base()->SetVoiceEngine(
      voice_engine->webrtc()->engine())) != 0) {
    LOG(LS_WARNING) << "Failed to SetVoiceEngine";
  }

  if ((video_engine_->base()->RegisterObserver(*this)) != 0) {
    LOG(LS_WARNING) << "Failed to register observer";
  }

  int ncodecs = video_engine_->codec()->NumberOfCodecs();
  for (int i = 0; i < ncodecs - 2; ++i) {
    VideoCodec wcodec;
    if ((video_engine_->codec()->GetCodec(i, wcodec) == 0) &&
        (strncmp(wcodec.plName, "I420", 4) != 0)) { //ignore I420
      cricket::VideoCodec codec(wcodec.plType, wcodec.plName, wcodec.width,
                                wcodec.height, wcodec.maxFramerate, i);
      LOG(LS_INFO) << codec.ToString();
      video_codecs_.push_back(codec);
    }
  }

  std::sort(video_codecs_.begin(), video_codecs_.end(),
            &cricket::VideoCodec::Preferable);
  return ret;
}

void RtcVideoEngine::PerformanceAlarm(const unsigned int cpuLoad) {
  return;
}

void RtcVideoEngine::Print(const TraceLevel level, const char *traceString,
                       const int length) {
  return;
}

int RtcVideoEngine::GetCodecPreference(const char* name) {
  for (size_t i = 0; i < ARRAY_SIZE(kVideoCodecPrefs); ++i) {
    if (strcmp(kVideoCodecPrefs[i].payload_name, name) == 0) {
     return kVideoCodecPrefs[i].pref;
    }
  }
  return -1;
}

void RtcVideoEngine::ApplyLogging() {
  int filter = 0;
  switch(log_level_) {
    case talk_base::LS_VERBOSE: filter |= kTraceAll;
    case talk_base::LS_INFO: filter |= kTraceStateInfo;
    case talk_base::LS_WARNING: filter |= kTraceWarning;
    case talk_base::LS_ERROR: filter |= kTraceError | kTraceCritical;
  }
}

void RtcVideoEngine::Terminate() {
  LOG(LS_INFO) << "RtcVideoEngine::Terminate";
  ReleaseCaptureDevice();
}

int RtcVideoEngine::GetCapabilities() {
  return cricket::MediaEngine::VIDEO_RECV | cricket::MediaEngine::VIDEO_SEND;
}

bool RtcVideoEngine::SetOptions(int options) {
  return true;
}

bool RtcVideoEngine::ReleaseCaptureDevice() {
  if (capture_) {
    // Stop capture
    SetCapture(false);
    // DisconnectCaptureDevice
    RtcVideoMediaChannel* channel;
    for (VideoChannels::const_iterator it = channels_.begin();
        it != channels_.end(); ++it) {
      ASSERT(*it != NULL);
      channel = *it;
      video_engine_->capture()->DisconnectCaptureDevice(channel->video_channel());
    }
    // ReleaseCaptureDevice
    video_engine_->capture()->ReleaseCaptureDevice(capture_id_);
    capture_id_ = -1;
#ifdef PLATFORM_CHROMIUM
    VideoCaptureChrome::DestroyVideoCapture(
        static_cast<VideoCaptureChrome*>(capture_));
#else
    webrtc::VideoCaptureModule::Destroy(capture_);
#endif
    capture_ = NULL;
  }
  return true;
}

bool RtcVideoEngine::SetCaptureDevice(const cricket::Device* cam) {
  ASSERT(video_engine_.get());
  ASSERT(cam != NULL);

  ReleaseCaptureDevice();

#ifdef PLATFORM_CHROMIUM
  int cam_id = atol(cam->id.c_str());
  if (cam_id == -1)
    return false;
  unsigned char uniqueId[16];
  capture_ = VideoCaptureChrome::CreateVideoCapture(cam_id, uniqueId);
#else
  WebRtc_UWord8 device_name[128];
  WebRtc_UWord8 device_id[260];
  VideoCaptureModule::DeviceInfo* device_info =
      VideoCaptureModule::CreateDeviceInfo(0);
  for (WebRtc_UWord32 i = 0; i < device_info->NumberOfDevices(); ++i) {
    if (device_info->GetDeviceName(i, device_name, ARRAYSIZE(device_name),
                                   device_id, ARRAYSIZE(device_id)) == 0) {
      if ((cam->name.compare("") == 0) ||
          (cam->id.compare((char*) device_id) == 0)) {
        capture_ = VideoCaptureModule::Create(1234, device_id);
        if (capture_) {
          LOG(INFO) << "Found video capture device: " << device_name;
          break;
        }
      }
    }
  }
  VideoCaptureModule::DestroyDeviceInfo(device_info);
#endif

  if (!capture_)
    return false;

  ViECapture* vie_capture = video_engine_->capture();
  if (vie_capture->AllocateCaptureDevice(*capture_, capture_id_) == 0) {
    // Connect to all the channels
    RtcVideoMediaChannel* channel;
    for (VideoChannels::const_iterator it = channels_.begin();
         it != channels_.end(); ++it) {
      ASSERT(*it != NULL);
      channel = *it;
      vie_capture->ConnectCaptureDevice(capture_id_, channel->video_channel());
    }
    SetCapture(true);
  } else {
    ASSERT(capture_id_ == -1);
  }

  return (capture_id_ != -1);
}

bool RtcVideoEngine::SetVideoRenderer(int channel_id,
                                      void* window,
                                      unsigned int zOrder,
                                      float left,
                                      float top,
                                      float right,
                                      float bottom) {
  int ret;
  if (channel_id == -1)
    channel_id = capture_id_;
  ret = video_engine_->render()->AddRenderer(
      channel_id, window, zOrder, left, top, right, bottom);
  if (ret !=0 )
    return false;
  ret = video_engine_->render()->StartRender(channel_id);
  if (ret !=0 )
      return false;
  return true;
}

bool RtcVideoEngine::SetLocalRenderer(cricket::VideoRenderer* renderer) {
  LOG(LS_WARNING) << "Not required call SetLocalRenderer for webrtc";
  return false;
}

cricket::CaptureResult RtcVideoEngine::SetCapture(bool capture) {
  if (capture_started_ == capture)
    return cricket::CR_SUCCESS;

  if (capture_id_ != -1) {
    int ret;
    if (capture)
      ret = video_engine_->capture()->StartCapture(capture_id_);
    else
      ret = video_engine_->capture()->StopCapture(capture_id_);
    if (ret == 0) {
      capture_started_ = capture;
      return cricket::CR_SUCCESS;
    }
  }

  return cricket::CR_NO_DEVICE;
}

const std::vector<cricket::VideoCodec>& RtcVideoEngine::codecs() const {
  return video_codecs_;
}

void RtcVideoEngine::SetLogging(int min_sev, const char* filter) {
  log_level_ = min_sev;
  ApplyLogging();
}

bool RtcVideoEngine::SetDefaultEncoderConfig(
    const cricket::VideoEncoderConfig& config) {
  bool ret = SetDefaultCodec(config.max_codec);
  if (ret) {
    default_encoder_config_ = config;
  }
  return ret;
}

bool RtcVideoEngine::SetDefaultCodec(const cricket::VideoCodec& codec) {
  default_codec_ = codec;
  return true;
}

RtcVideoMediaChannel* RtcVideoEngine::CreateChannel(
    cricket::VoiceMediaChannel* voice_channel) {
  RtcVideoMediaChannel* channel =
      new RtcVideoMediaChannel(this, voice_channel);
  if (channel) {
    if (!channel->Init()) {
      delete channel;
      channel = NULL;
    }
  }
  return channel;
}

bool RtcVideoEngine::FindCodec(const cricket::VideoCodec& codec) {
  for (size_t i = 0; i < video_codecs_.size(); ++i) {
    if (video_codecs_[i].Matches(codec)) {
      return true;
    }
  }
  return false;
}

void RtcVideoEngine::ConvertToCricketVideoCodec(
    const VideoCodec& in_codec, cricket::VideoCodec& out_codec) {
  out_codec.id = in_codec.plType;
  out_codec.name = in_codec.plName;
  out_codec.width = in_codec.width;
  out_codec.height = in_codec.height;
  out_codec.framerate = in_codec.maxFramerate;
}

void RtcVideoEngine::ConvertFromCricketVideoCodec(
    const cricket::VideoCodec& in_codec, VideoCodec& out_codec) {
  out_codec.plType = in_codec.id;
  strcpy(out_codec.plName, in_codec.name.c_str());
  out_codec.width = 352; //in_codec.width;
  out_codec.height = 288; //in_codec.height;
  out_codec.maxFramerate = 30; //in_codec.framerate;

  if (strncmp(out_codec.plName, "VP8", 3) == 0) {
    out_codec.codecType = kVideoCodecVP8;
  } else if (strncmp(out_codec.plName, "H263", 4) == 0) {
    out_codec.codecType = kVideoCodecH263;
  } else if (strncmp(out_codec.plName, "H264", 4) == 0) {
    out_codec.codecType = kVideoCodecH264;
  } else if (strncmp(out_codec.plName, "I420", 4) == 0) {
    out_codec.codecType = kVideoCodecI420;
  } else {
    LOG(LS_INFO) << "invalid codec type";
  }

  out_codec.maxBitrate = kMaxVideoBitrate;
  out_codec.startBitrate = kStartVideoBitrate;
  out_codec.minBitrate = kStartVideoBitrate;
}

int RtcVideoEngine::GetLastVideoEngineError() {
  return video_engine_->base()->LastError();
}

void RtcVideoEngine::RegisterChannel(RtcVideoMediaChannel *channel) {
  talk_base::CritScope lock(&channels_cs_);
  channels_.push_back(channel);
}

void RtcVideoEngine::UnregisterChannel(RtcVideoMediaChannel *channel) {
  talk_base::CritScope lock(&channels_cs_);
  VideoChannels::iterator i = std::find(channels_.begin(),
                                      channels_.end(),
                                      channel);
  if (i != channels_.end()) {
    channels_.erase(i);
  }
}




// RtcVideoMediaChannel

RtcVideoMediaChannel::RtcVideoMediaChannel(
    RtcVideoEngine* engine, cricket::VoiceMediaChannel* channel)
    : engine_(engine),
      voice_channel_(channel),
      video_channel_(-1),
      sending_(false),
      render_started_(false) {
  engine->RegisterChannel(this);
}

bool RtcVideoMediaChannel::Init() {
  bool ret = true;
  if (engine_->video_engine()->base()->CreateChannel(video_channel_) != 0) {
    LOG(LS_ERROR) << "ViE CreateChannel Failed!!";
    ret = false;
  }

  LOG(LS_INFO) << "RtcVideoMediaChannel::Init "
               << "video_channel " << video_channel_ << " created";
  //connect audio channel
  if (voice_channel_) {
    RtcVoiceMediaChannel* channel =
        static_cast<RtcVoiceMediaChannel*> (voice_channel_);
    if (engine_->video_engine()->base()->ConnectAudioChannel(
        video_channel_, channel->audio_channel()) != 0) {
      LOG(LS_WARNING) << "ViE ConnectAudioChannel failed"
                   << "A/V not synchronized";
      // Don't set ret to false;
    }
  }

  //Register external transport
  if (engine_->video_engine()->network()->RegisterSendTransport(
      video_channel_, *this) != 0) {
    ret = false;
  } else {
    EnableRtcp();
    EnablePLI();
  }
  return ret;
}

RtcVideoMediaChannel::~RtcVideoMediaChannel() {
  // Stop and remote renderer
  SetRender(false);
  if (engine()->video_engine()->render()->RemoveRenderer(video_channel_) == -1) {
    LOG(LS_ERROR) << "Video RemoveRenderer failed for channel "
                  << video_channel_;
  }

  // DeRegister external transport
  if (engine()->video_engine()->network()->DeregisterSendTransport(
      video_channel_) == -1) {
    LOG(LS_ERROR) << "DeRegisterSendTransport failed for channel id "
                  << video_channel_;
  }

  // Unregister RtcChannel with the engine.
  engine()->UnregisterChannel(this);

  // Delete VideoChannel
  if (engine()->video_engine()->base()->DeleteChannel(video_channel_) == -1) {
    LOG(LS_ERROR) << "Video DeleteChannel failed for channel "
                  << video_channel_;
  }
}

bool RtcVideoMediaChannel::SetRecvCodecs(
    const std::vector<cricket::VideoCodec>& codecs) {
  bool ret = true;
  for (std::vector<cricket::VideoCodec>::const_iterator iter = codecs.begin();
      iter != codecs.end(); ++iter) {
    if (engine()->FindCodec(*iter)) {
      VideoCodec wcodec;
      engine()->ConvertFromCricketVideoCodec(*iter, wcodec);
      if (engine()->video_engine()->codec()->SetReceiveCodec(
          video_channel_,  wcodec) != 0) {
        LOG(LS_ERROR) << "ViE SetReceiveCodec failed"
                      << " VideoChannel : " << video_channel_ << " Error: "
                      << engine()->video_engine()->base()->LastError()
                      << "wcodec " << wcodec.plName;
        ret = false;
      }
    } else {
      LOG(LS_INFO) << "Unknown codec" << iter->name;
      ret = false;
    }
  }

  // make channel ready to receive packets
  if (ret) {
    if (engine()->video_engine()->base()->StartReceive(video_channel_) != 0) {
      LOG(LS_ERROR) << "ViE StartReceive failure";
      ret = false;
    }
  }
  return ret;
}

bool RtcVideoMediaChannel::SetSendCodecs(
    const std::vector<cricket::VideoCodec>& codecs) {
  if (sending_) {
    LOG(LS_ERROR) << "channel is alredy sending";
    return false;
  }

  //match with local video codec list
  std::vector<VideoCodec> send_codecs;
  for (std::vector<cricket::VideoCodec>::const_iterator iter = codecs.begin();
      iter != codecs.end(); ++iter) {
    if (engine()->FindCodec(*iter)) {
      VideoCodec wcodec;
      engine()->ConvertFromCricketVideoCodec(*iter, wcodec);
      send_codecs.push_back(wcodec);
    }
  }

  // if none matches, return with set
  if (send_codecs.empty()) {
    LOG(LS_ERROR) << "No matching codecs avilable";
    return false;
  }

  //select the first matched codec
  const VideoCodec& codec(send_codecs[0]);
  send_codec_ = codec;
  if (engine()->video_engine()->codec()->SetSendCodec(
      video_channel_, codec) != 0) {
    LOG(LS_ERROR) << "ViE SetSendCodec failed";
    return false;
  }
  return true;
}

bool RtcVideoMediaChannel::SetRender(bool render) {
  if (video_channel_ != -1) {
    int ret = -1;
    if (render == render_started_)
      return true;

    if (render) {
      ret = engine()->video_engine()->render()->StartRender(video_channel_);
    } else {
      ret = engine()->video_engine()->render()->StopRender(video_channel_);
    }

    if (ret == 0) {
      render_started_ = render;
      return true;
    }
  }
  return false;
}

bool RtcVideoMediaChannel::SetSend(bool send) {
  if (send == sending()) {
    return true; // no action required
  }

  bool ret = true;
  if (send) { //enable
    if (engine()->video_engine()->base()->StartSend(video_channel_) != 0) {
      LOG(LS_ERROR) << "ViE StartSend failed";
      ret = false;
    }
  } else { // disable
    if (engine()->video_engine()->base()->StopSend(video_channel_) != 0) {
      LOG(LS_ERROR) << "ViE StopSend failed";
      ret = false;
    }
  }
  if (ret)
    sending_ = send;

  return ret;
}

bool RtcVideoMediaChannel::AddStream(uint32 ssrc, uint32 voice_ssrc) {
  return false;
}

bool RtcVideoMediaChannel::RemoveStream(uint32 ssrc) {
  return false;
}

bool RtcVideoMediaChannel::SetRenderer(
    uint32 ssrc, cricket::VideoRenderer* renderer) {
  return false;
}

bool RtcVideoMediaChannel::SetExternalRenderer(uint32 ssrc, void* renderer)
{
  int ret;
  ret = engine_->video_engine()->render()->AddRenderer(
      video_channel_,
      kVideoI420,
      static_cast<ExternalRenderer*>(renderer));
  if (ret !=0 )
    return false;
  ret = engine_->video_engine()->render()->StartRender(video_channel_);
  if (ret !=0 )
      return false;
  return true;
}

bool RtcVideoMediaChannel::GetStats(cricket::VideoMediaInfo* info) {
  cricket::VideoSenderInfo sinfo;
  memset(&sinfo, 0, sizeof(sinfo));

  unsigned int ssrc;
  if (engine_->video_engine()->rtp()->GetLocalSSRC(video_channel_,
                                                   ssrc) != 0) {
    LOG(LS_ERROR) << "ViE GetLocalSSRC failed";
    return false;
  }
  sinfo.ssrc = ssrc;

  unsigned int cumulative_lost, extended_max, jitter;
  int rtt_ms;
  unsigned short fraction_lost;

  if (engine_->video_engine()->rtp()->GetSentRTCPStatistics(video_channel_,
          fraction_lost, cumulative_lost, extended_max, jitter, rtt_ms) != 0) {
    LOG(LS_ERROR) << "ViE GetLocalSSRC failed";
    return false;
  }

  sinfo.fraction_lost = fraction_lost;
  sinfo.rtt_ms = rtt_ms;

  unsigned int bytes_sent, packets_sent, bytes_recv, packets_recv;
  if (engine_->video_engine()->rtp()->GetRTPStatistics(video_channel_,
          bytes_sent, packets_sent, bytes_recv, packets_recv) != 0) {
    LOG(LS_ERROR) << "ViE GetRTPStatistics";
    return false;
  }
  sinfo.packets_sent = packets_sent;
  sinfo.bytes_sent = bytes_sent;
  sinfo.packets_lost = -1;
  sinfo.packets_cached = -1;

  info->senders.push_back(sinfo);

  //build receiver info.
  // reusing the above local variables
  cricket::VideoReceiverInfo rinfo;
  memset(&rinfo, 0, sizeof(rinfo));
  if (engine_->video_engine()->rtp()->GetReceivedRTCPStatistics(video_channel_,
          fraction_lost, cumulative_lost, extended_max, jitter, rtt_ms) != 0) {
    LOG(LS_ERROR) << "ViE GetReceivedRTPStatistics Failed";
    return false;
  }
  rinfo.bytes_rcvd = bytes_recv;
  rinfo.packets_rcvd = packets_recv;
  rinfo.fraction_lost = fraction_lost;

  if (engine_->video_engine()->rtp()->GetRemoteSSRC(video_channel_,
                                                    ssrc) != 0) {
    return false;
  }
  rinfo.ssrc = ssrc;

  //Get codec for wxh
  info->receivers.push_back(rinfo);
  return true;
}

bool RtcVideoMediaChannel::SendIntraFrame() {
  bool ret = true;
  if (engine()->video_engine()->codec()->SendKeyFrame(video_channel_) != 0) {
    LOG(LS_ERROR) << "ViE SendKeyFrame failed";
    ret = false;
  }

  return ret;
}

bool RtcVideoMediaChannel::RequestIntraFrame() {
  //There is no API exposed to application to request a key frame
  // ViE does this internally when there are errors from decoder
  return true;
}

void RtcVideoMediaChannel::OnPacketReceived(talk_base::Buffer* packet) {
  engine()->video_engine()->network()->ReceivedRTPPacket(video_channel_,
                                                         packet->data(),
                                                         packet->length());

}

void RtcVideoMediaChannel::OnRtcpReceived(talk_base::Buffer* packet) {
  engine_->video_engine()->network()->ReceivedRTCPPacket(video_channel_,
                                                         packet->data(),
                                                         packet->length());

}

void RtcVideoMediaChannel::SetSendSsrc(uint32 id) {
  if (!sending_){
    if (engine()->video_engine()->rtp()->SetLocalSSRC(video_channel_, id) != 0) {
      LOG(LS_ERROR) << "ViE SetLocalSSRC failed";
    }
  } else {
    LOG(LS_ERROR) << "Channel already in send state";
  }
}

bool RtcVideoMediaChannel::SetRtcpCName(const std::string& cname) {
  if (engine()->video_engine()->rtp()->SetRTCPCName(video_channel_,
                                                    cname.c_str()) != 0) {
    LOG(LS_ERROR) << "ViE SetRTCPCName failed";
    return false;
  }
  return true;
}

bool RtcVideoMediaChannel::Mute(bool on) {
  // stop send??
  return false;
}

bool RtcVideoMediaChannel::SetSendBandwidth(bool autobw, int bps) {
  LOG(LS_VERBOSE) << "RtcVideoMediaChanne::SetSendBandwidth";

  VideoCodec current = send_codec_;
  send_codec_.startBitrate = bps;

  if (engine()->video_engine()->codec()->SetSendCodec(video_channel_,
                                                      send_codec_) != 0) {
    LOG(LS_ERROR) << "ViE SetSendCodec failed";
    if (engine()->video_engine()->codec()->SetSendCodec(video_channel_,
                                                        current) != 0) {
      // should call be ended in this case?
    }
    return false;
  }
  return true;
}

bool RtcVideoMediaChannel::SetOptions(int options) {
  return true;
}

void RtcVideoMediaChannel::EnableRtcp() {
  engine()->video_engine()->rtp()->SetRTCPStatus(
      video_channel_, kRtcpCompound_RFC4585);
}

void RtcVideoMediaChannel::EnablePLI() {
  engine_->video_engine()->rtp()->SetKeyFrameRequestMethod(
      video_channel_, kViEKeyFrameRequestPliRtcp);
}

void RtcVideoMediaChannel::EnableTMMBR() {
  engine_->video_engine()->rtp()->SetTMMBRStatus(video_channel_, true);
}

int RtcVideoMediaChannel::SendPacket(int channel, const void* data, int len) {
  if (!network_interface_) {
    return -1;
  }
  talk_base::Buffer packet(data, len, cricket::kMaxRtpPacketLen);
  return network_interface_->SendPacket(&packet) ? len : -1;
}

int RtcVideoMediaChannel::SendRTCPPacket(int channel,
                                         const void* data,
                                         int len) {
  if (!network_interface_) {
    return -1;
  }
  talk_base::Buffer packet(data, len, cricket::kMaxRtpPacketLen);
  return network_interface_->SendRtcp(&packet) ? len : -1;
}

} // namespace webrtc
