/*
 * libjingle
 * Copyright 2010 Google Inc.
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

#ifndef TALK_MEDIA_WEBRTC_FAKEWEBRTCVIDEOENGINE_H_
#define TALK_MEDIA_WEBRTC_FAKEWEBRTCVIDEOENGINE_H_

#include <map>
#include <set>
#include <vector>

#include "talk/base/basictypes.h"
#include "talk/base/gunit.h"
#include "talk/base/stringutils.h"
#include "talk/media/base/codec.h"
#include "talk/media/webrtc/fakewebrtccommon.h"
#include "talk/media/webrtc/webrtcvideodecoderfactory.h"
#include "talk/media/webrtc/webrtcvideoencoderfactory.h"
#include "talk/media/webrtc/webrtcvie.h"

namespace webrtc {

bool operator==(const webrtc::VideoCodec& c1, const webrtc::VideoCodec& c2) {
  return memcmp(&c1, &c2, sizeof(c1)) == 0;
}

}

namespace cricket {

#define WEBRTC_CHECK_CAPTURER(capturer) \
  if (capturers_.find(capturer) == capturers_.end()) return -1;

#define WEBRTC_ASSERT_CAPTURER(capturer) \
  ASSERT(capturers_.find(capturer) != capturers_.end());

static const int kMinVideoBitrate = 100;
static const int kStartVideoBitrate = 300;
static const int kMaxVideoBitrate = 1000;

// WebRtc channel id and capture id share the same number space.
// This is how AddRenderer(renderId, ...) is able to tell if it is adding a
// renderer for a channel or it is adding a renderer for a capturer.
static const int kViEChannelIdBase = 0;
static const int kViEChannelIdMax = 1000;
static const int kViECaptureIdBase = 10000;  // Make sure there is a gap.
static const int kViECaptureIdMax = 11000;

// Fake class for mocking out webrtc::VideoDecoder
class FakeWebRtcVideoDecoder : public webrtc::VideoDecoder {
 public:
  FakeWebRtcVideoDecoder()
      : num_frames_received_(0) {
  }

  virtual int32 InitDecode(const webrtc::VideoCodec*, int32) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32 Decode(
      const webrtc::EncodedImage&, bool, const webrtc::RTPFragmentationHeader*,
      const webrtc::CodecSpecificInfo*, int64) {
    num_frames_received_++;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32 RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback*) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32 Release() {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32 Reset() {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int GetNumFramesReceived() const {
    return num_frames_received_;
  }

 private:
  int num_frames_received_;
};

// Fake class for mocking out WebRtcVideoDecoderFactory.
class FakeWebRtcVideoDecoderFactory : public WebRtcVideoDecoderFactory {
 public:
  FakeWebRtcVideoDecoderFactory()
      : num_created_decoders_(0) {
  }

  virtual webrtc::VideoDecoder* CreateVideoDecoder(
      webrtc::VideoCodecType type) {
    if (supported_codec_types_.count(type) == 0) {
      return NULL;
    }
    FakeWebRtcVideoDecoder* decoder = new FakeWebRtcVideoDecoder();
    decoders_.push_back(decoder);
    num_created_decoders_++;
    return decoder;
  }

  virtual void DestroyVideoDecoder(webrtc::VideoDecoder* decoder) {
    decoders_.erase(
        std::remove(decoders_.begin(), decoders_.end(), decoder),
        decoders_.end());
    delete decoder;
  }

  void AddSupportedVideoCodecType(webrtc::VideoCodecType type) {
    supported_codec_types_.insert(type);
  }

  int GetNumCreatedDecoders() {
    return num_created_decoders_;
  }

  const std::vector<FakeWebRtcVideoDecoder*>& decoders() {
    return decoders_;
  }

 private:
  std::set<webrtc::VideoCodecType> supported_codec_types_;
  std::vector<FakeWebRtcVideoDecoder*> decoders_;
  int num_created_decoders_;
};

// Fake class for mocking out webrtc::VideoEnoder
class FakeWebRtcVideoEncoder : public webrtc::VideoEncoder {
 public:
  FakeWebRtcVideoEncoder() {}

  virtual int32 InitEncode(const webrtc::VideoCodec* codecSettings,
                           int32 numberOfCores,
                           uint32 maxPayloadSize) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32 Encode(
      const webrtc::I420VideoFrame& inputImage,
            const webrtc::CodecSpecificInfo* codecSpecificInfo,
            const std::vector<webrtc::VideoFrameType>* frame_types) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32 RegisterEncodeCompleteCallback(
      webrtc::EncodedImageCallback* callback) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32 Release() {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32 SetChannelParameters(uint32 packetLoss,
                                     int rtt) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32 SetRates(uint32 newBitRate,
                         uint32 frameRate) {
    return WEBRTC_VIDEO_CODEC_OK;
  }
};

// Fake class for mocking out WebRtcVideoEncoderFactory.
class FakeWebRtcVideoEncoderFactory : public WebRtcVideoEncoderFactory {
 public:
  FakeWebRtcVideoEncoderFactory()
      : num_created_encoders_(0) {
  }

  virtual webrtc::VideoEncoder* CreateVideoEncoder(
      webrtc::VideoCodecType type) {
    if (supported_codec_types_.count(type) == 0) {
      return NULL;
    }
    FakeWebRtcVideoEncoder* encoder = new FakeWebRtcVideoEncoder();
    encoders_.push_back(encoder);
    num_created_encoders_++;
    return encoder;
  }

  virtual void DestroyVideoEncoder(webrtc::VideoEncoder* encoder) {
    encoders_.erase(
        std::remove(encoders_.begin(), encoders_.end(), encoder),
        encoders_.end());
    delete encoder;
  }

  virtual void AddObserver(WebRtcVideoEncoderFactory::Observer* observer) {
    bool inserted = observers_.insert(observer).second;
    EXPECT_TRUE(inserted);
  }

  virtual void RemoveObserver(WebRtcVideoEncoderFactory::Observer* observer) {
    size_t erased = observers_.erase(observer);
    EXPECT_EQ(erased, 1UL);
  }

  virtual const std::vector<WebRtcVideoEncoderFactory::VideoCodec>& codecs()
      const {
    return codecs_;
  }

  void AddSupportedVideoCodecType(webrtc::VideoCodecType type,
                                  const std::string& name) {
    supported_codec_types_.insert(type);
    codecs_.push_back(
        WebRtcVideoEncoderFactory::VideoCodec(type, name, 1280, 720, 30));
  }

  void NotifyCodecsAvailable() {
    std::set<WebRtcVideoEncoderFactory::Observer*>::iterator it;
    for (it = observers_.begin(); it != observers_.end(); ++it)
      (*it)->OnCodecsAvailable();
  }

  int GetNumCreatedEncoders() {
    return num_created_encoders_;
  }

  const std::vector<FakeWebRtcVideoEncoder*>& encoders() {
    return encoders_;
  }

 private:
  std::set<webrtc::VideoCodecType> supported_codec_types_;
  std::vector<WebRtcVideoEncoderFactory::VideoCodec> codecs_;
  std::vector<FakeWebRtcVideoEncoder*> encoders_;
  std::set<WebRtcVideoEncoderFactory::Observer*> observers_;
  int num_created_encoders_;
};

class FakeWebRtcVideoEngine
    : public webrtc::ViEBase,
      public webrtc::ViECodec,
      public webrtc::ViECapture,
      public webrtc::ViENetwork,
      public webrtc::ViERender,
      public webrtc::ViERTP_RTCP,
      public webrtc::ViEImageProcess,
      public webrtc::ViEExternalCodec {
 public:
  struct Channel {
    Channel()
        : capture_id_(-1),
          original_channel_id_(-1),
          has_renderer_(false),
          render_started_(false),
          send(false),
          receive_(false),
          can_transmit_(true),
          remote_rtx_ssrc_(-1),
          rtx_send_payload_type(-1),
          rtcp_status_(webrtc::kRtcpNone),
          key_frame_request_method_(webrtc::kViEKeyFrameRequestNone),
          tmmbr_(false),
          remb_contribute_(false),
          remb_bw_partition_(false),
          rtp_offset_send_id_(0),
          rtp_offset_receive_id_(0),
          rtp_absolute_send_time_send_id_(0),
          rtp_absolute_send_time_receive_id_(0),
          sender_target_delay_(0),
          receiver_target_delay_(0),
          transmission_smoothing_(false),
          nack_(false),
          hybrid_nack_fec_(false),
          send_video_bitrate_(0),
          send_fec_bitrate_(0),
          send_nack_bitrate_(0),
          send_bandwidth_(0),
          receive_bandwidth_(0) {
      ssrcs_[0] = 0;  // default ssrc.
      memset(&send_codec, 0, sizeof(send_codec));
    }
    int capture_id_;
    int original_channel_id_;
    bool has_renderer_;
    bool render_started_;
    bool send;
    bool receive_;
    bool can_transmit_;
    std::map<int, int> ssrcs_;
    std::map<int, int> rtx_ssrcs_;
    int remote_rtx_ssrc_;
    int rtx_send_payload_type;
    std::string cname_;
    webrtc::ViERTCPMode rtcp_status_;
    webrtc::ViEKeyFrameRequestMethod key_frame_request_method_;
    bool tmmbr_;
    bool remb_contribute_;   // This channel contributes to the remb report.
    bool remb_bw_partition_; // This channel is allocated part of total bw.
    int rtp_offset_send_id_;
    int rtp_offset_receive_id_;
    int rtp_absolute_send_time_send_id_;
    int rtp_absolute_send_time_receive_id_;
    int sender_target_delay_;
    int receiver_target_delay_;
    bool transmission_smoothing_;
    bool nack_;
    bool hybrid_nack_fec_;
    std::vector<webrtc::VideoCodec> recv_codecs;
    std::set<unsigned int> ext_decoder_pl_types_;
    std::set<unsigned int> ext_encoder_pl_types_;
    webrtc::VideoCodec send_codec;
    unsigned int send_video_bitrate_;
    unsigned int send_fec_bitrate_;
    unsigned int send_nack_bitrate_;
    unsigned int send_bandwidth_;
    unsigned int receive_bandwidth_;
  };
  class Capturer : public webrtc::ViEExternalCapture {
   public:
    Capturer() : channel_id_(-1), denoising_(false),
                 last_capture_time_(0), incoming_frame_num_(0) { }
    int channel_id() const { return channel_id_; }
    void set_channel_id(int channel_id) { channel_id_ = channel_id; }
    bool denoising() const { return denoising_; }
    void set_denoising(bool denoising) { denoising_ = denoising; }
    int64 last_capture_time() const { return last_capture_time_; }
    int incoming_frame_num() const { return incoming_frame_num_; }

    // From ViEExternalCapture
    virtual int IncomingFrame(unsigned char* videoFrame,
                              unsigned int videoFrameLength,
                              unsigned short width,
                              unsigned short height,
                              webrtc::RawVideoType videoType,
                              unsigned long long captureTime) {
      return 0;
    }
    virtual int IncomingFrameI420(
        const webrtc::ViEVideoFrameI420& video_frame,
        unsigned long long captureTime) {
      last_capture_time_ = captureTime;
      ++incoming_frame_num_;
      return 0;
    }

   private:
    int channel_id_;
    bool denoising_;
    int64 last_capture_time_;
    int incoming_frame_num_;
  };

  FakeWebRtcVideoEngine(const cricket::VideoCodec* const* codecs,
                        int num_codecs)
      : inited_(false),
        last_channel_(kViEChannelIdBase - 1),
        fail_create_channel_(false),
        last_capturer_(kViECaptureIdBase - 1),
        fail_alloc_capturer_(false),
        codecs_(codecs),
        num_codecs_(num_codecs),
        num_set_send_codecs_(0) {
  }

  ~FakeWebRtcVideoEngine() {
    ASSERT(0 == channels_.size());
    ASSERT(0 == capturers_.size());
  }
  bool IsInited() const { return inited_; }

  int GetLastChannel() const { return last_channel_; }
  int GetChannelFromLocalSsrc(int local_ssrc) const {
    // ssrcs_[0] is the default local ssrc.
    for (std::map<int, Channel*>::const_iterator iter = channels_.begin();
         iter != channels_.end(); ++iter) {
      if (local_ssrc == iter->second->ssrcs_[0]) {
        return iter->first;
      }
    }
    return -1;
  }

  int GetNumChannels() const { return static_cast<int>(channels_.size()); }
  bool IsChannel(int channel) const {
    return (channels_.find(channel) != channels_.end());
  }
  void set_fail_create_channel(bool fail_create_channel) {
    fail_create_channel_ = fail_create_channel;
  }

  int GetLastCapturer() const { return last_capturer_; }
  int GetNumCapturers() const { return static_cast<int>(capturers_.size()); }
  int GetIncomingFrameNum(int channel_id) const {
    for (std::map<int, Capturer*>::const_iterator iter = capturers_.begin();
         iter != capturers_.end(); ++iter) {
      Capturer* capturer = iter->second;
      if (capturer->channel_id() == channel_id) {
        return capturer->incoming_frame_num();
      }
    }
    return -1;
  }
  void set_fail_alloc_capturer(bool fail_alloc_capturer) {
    fail_alloc_capturer_ = fail_alloc_capturer;
  }
  int num_set_send_codecs() const { return num_set_send_codecs_; }

  int GetCaptureId(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->capture_id_;
  }
  int GetOriginalChannelId(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->original_channel_id_;
  }
  bool GetHasRenderer(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->has_renderer_;
  }
  bool GetRenderStarted(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->render_started_;
  }
  bool GetSend(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->send;
  }
  int GetCaptureChannelId(int capture_id) const {
    WEBRTC_ASSERT_CAPTURER(capture_id);
    return capturers_.find(capture_id)->second->channel_id();
  }
  bool GetCaptureDenoising(int capture_id) const {
    WEBRTC_ASSERT_CAPTURER(capture_id);
    return capturers_.find(capture_id)->second->denoising();
  }
  int64 GetCaptureLastTimestamp(int capture_id) const {
    WEBRTC_ASSERT_CAPTURER(capture_id);
    return capturers_.find(capture_id)->second->last_capture_time();
  }
  webrtc::ViERTCPMode GetRtcpStatus(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->rtcp_status_;
  }
  webrtc::ViEKeyFrameRequestMethod GetKeyFrameRequestMethod(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->key_frame_request_method_;
  }
  bool GetTmmbrStatus(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->tmmbr_;
  }
  bool GetRembStatusBwPartition(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->remb_bw_partition_;
  }
  bool GetRembStatusContribute(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->remb_contribute_;
  }
  int GetSendRtpTimestampOffsetExtensionId(int channel) {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->rtp_offset_send_id_;
  }
  int GetReceiveRtpTimestampOffsetExtensionId(int channel) {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->rtp_offset_receive_id_;
  }
  int GetSendAbsoluteSendTimeExtensionId(int channel) {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->rtp_absolute_send_time_send_id_;
  }
  int GetReceiveAbsoluteSendTimeExtensionId(int channel) {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->rtp_absolute_send_time_receive_id_;
  }
  bool GetTransmissionSmoothingStatus(int channel) {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->transmission_smoothing_;
  }
  int GetSenderTargetDelay(int channel) {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->sender_target_delay_;
  }
  int GetReceiverTargetDelay(int channel) {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->receiver_target_delay_;
  }
  bool GetNackStatus(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->nack_;
  }
  bool GetHybridNackFecStatus(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->hybrid_nack_fec_;
  }
  int GetNumSsrcs(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return static_cast<int>(
        channels_.find(channel)->second->ssrcs_.size());
  }
  int GetNumRtxSsrcs(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return static_cast<int>(
        channels_.find(channel)->second->rtx_ssrcs_.size());
  }
  bool GetIsTransmitting(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->can_transmit_;
  }
  int GetRtxSsrc(int channel, int simulcast_idx) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    if (channels_.find(channel)->second->rtx_ssrcs_.find(simulcast_idx) ==
        channels_.find(channel)->second->rtx_ssrcs_.end()) {
      return -1;
    }
    return channels_.find(channel)->second->rtx_ssrcs_[simulcast_idx];
  }
  bool ReceiveCodecRegistered(int channel,
                              const webrtc::VideoCodec& codec) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    const std::vector<webrtc::VideoCodec>& codecs =
      channels_.find(channel)->second->recv_codecs;
    return std::find(codecs.begin(), codecs.end(), codec) != codecs.end();
  };
  bool ExternalDecoderRegistered(int channel,
                                 unsigned int pl_type) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->
        ext_decoder_pl_types_.count(pl_type) != 0;
  };
  int GetNumExternalDecoderRegistered(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return static_cast<int>(
        channels_.find(channel)->second->ext_decoder_pl_types_.size());
  };
  bool ExternalEncoderRegistered(int channel,
                                 unsigned int pl_type) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return channels_.find(channel)->second->
        ext_encoder_pl_types_.count(pl_type) != 0;
  };
  int GetNumExternalEncoderRegistered(int channel) const {
    WEBRTC_ASSERT_CHANNEL(channel);
    return static_cast<int>(
        channels_.find(channel)->second->ext_encoder_pl_types_.size());
  };
  int GetTotalNumExternalEncoderRegistered() const {
    std::map<int, Channel*>::const_iterator it;
    int total_num_registered = 0;
    for (it = channels_.begin(); it != channels_.end(); ++it)
      total_num_registered +=
          static_cast<int>(it->second->ext_encoder_pl_types_.size());
    return total_num_registered;
  }
  void SetSendBitrates(int channel, unsigned int video_bitrate,
                       unsigned int fec_bitrate, unsigned int nack_bitrate) {
    WEBRTC_ASSERT_CHANNEL(channel);
    channels_[channel]->send_video_bitrate_ = video_bitrate;
    channels_[channel]->send_fec_bitrate_ = fec_bitrate;
    channels_[channel]->send_nack_bitrate_ = nack_bitrate;
  }
  void SetSendBandwidthEstimate(int channel, unsigned int send_bandwidth) {
    WEBRTC_ASSERT_CHANNEL(channel);
    channels_[channel]->send_bandwidth_ = send_bandwidth;
  }
  void SetReceiveBandwidthEstimate(int channel,
                                   unsigned int receive_bandwidth) {
    WEBRTC_ASSERT_CHANNEL(channel);
    channels_[channel]->receive_bandwidth_ = receive_bandwidth;
  };
  int GetRtxSendPayloadType(int channel) {
    WEBRTC_CHECK_CHANNEL(channel);
    return channels_[channel]->rtx_send_payload_type;
  }
  int GetRemoteRtxSsrc(int channel) {
    WEBRTC_CHECK_CHANNEL(channel);
    return channels_.find(channel)->second->remote_rtx_ssrc_;
  }

  WEBRTC_STUB(Release, ());

  // webrtc::ViEBase
  WEBRTC_FUNC(Init, ()) {
    inited_ = true;
    return 0;
  };
  WEBRTC_STUB(SetVoiceEngine, (webrtc::VoiceEngine*));
  WEBRTC_FUNC(CreateChannel, (int& channel)) {  // NOLINT
    if (fail_create_channel_) {
      return -1;
    }
    if (kViEChannelIdMax == last_channel_) {
      return -1;
    }
    Channel* ch = new Channel();
    channels_[++last_channel_] = ch;
    channel = last_channel_;
    return 0;
  };
  WEBRTC_FUNC(CreateChannel, (int& channel, int original_channel)) {
    WEBRTC_CHECK_CHANNEL(original_channel);
    if (CreateChannel(channel) != 0) {
      return -1;
    }
    channels_[channel]->original_channel_id_ = original_channel;
    return 0;
  }
  WEBRTC_FUNC(CreateReceiveChannel, (int& channel, int original_channel)) {
    return CreateChannel(channel, original_channel);
  }
  WEBRTC_FUNC(DeleteChannel, (const int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    // Make sure we deregister all the decoders before deleting a channel.
    EXPECT_EQ(0, GetNumExternalDecoderRegistered(channel));
    delete channels_[channel];
    channels_.erase(channel);
    return 0;
  }
  WEBRTC_STUB(RegisterCpuOveruseObserver,
      (int channel, webrtc::CpuOveruseObserver* observer));
#ifdef USE_WEBRTC_DEV_BRANCH
  WEBRTC_STUB(CpuOveruseMeasures, (int, int*, int*, int*, int*));
#endif
  WEBRTC_STUB(ConnectAudioChannel, (const int, const int));
  WEBRTC_STUB(DisconnectAudioChannel, (const int));
  WEBRTC_FUNC(StartSend, (const int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->send = true;
    return 0;
  }
  WEBRTC_FUNC(StopSend, (const int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->send = false;
    return 0;
  }
  WEBRTC_FUNC(StartReceive, (const int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->receive_ = true;
    return 0;
  }
  WEBRTC_FUNC(StopReceive, (const int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->receive_ = false;
    return 0;
  }
  WEBRTC_STUB(GetVersion, (char version[1024]));
  WEBRTC_STUB(LastError, ());

  // webrtc::ViECodec
  WEBRTC_FUNC_CONST(NumberOfCodecs, ()) {
    return num_codecs_;
  };
  WEBRTC_FUNC_CONST(GetCodec, (const unsigned char list_number,
                               webrtc::VideoCodec& out_codec)) {
    if (list_number >= NumberOfCodecs()) {
      return -1;
    }
    memset(&out_codec, 0, sizeof(out_codec));
    const cricket::VideoCodec& c(*codecs_[list_number]);
    if ("I420" == c.name) {
      out_codec.codecType = webrtc::kVideoCodecI420;
    } else if ("VP8" == c.name) {
      out_codec.codecType = webrtc::kVideoCodecVP8;
    } else if ("red" == c.name) {
      out_codec.codecType = webrtc::kVideoCodecRED;
    } else if ("ulpfec" == c.name) {
      out_codec.codecType = webrtc::kVideoCodecULPFEC;
    } else {
      out_codec.codecType = webrtc::kVideoCodecUnknown;
    }
    talk_base::strcpyn(out_codec.plName, sizeof(out_codec.plName),
                       c.name.c_str());
    out_codec.plType = c.id;
    out_codec.width = c.width;
    out_codec.height = c.height;
    out_codec.startBitrate = kStartVideoBitrate;
    out_codec.maxBitrate = kMaxVideoBitrate;
    out_codec.minBitrate = kMinVideoBitrate;
    out_codec.maxFramerate = c.framerate;
    return 0;
  };
  WEBRTC_FUNC(SetSendCodec, (const int channel,
                             const webrtc::VideoCodec& codec)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->send_codec = codec;
    ++num_set_send_codecs_;
    return 0;
  };
  WEBRTC_FUNC_CONST(GetSendCodec, (const int channel,
                                   webrtc::VideoCodec& codec)) {  // NOLINT
    WEBRTC_CHECK_CHANNEL(channel);
    codec = channels_.find(channel)->second->send_codec;
    return 0;
  };
  WEBRTC_FUNC(SetReceiveCodec, (const int channel,
                                const webrtc::VideoCodec& codec)) {  // NOLINT
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->recv_codecs.push_back(codec);
    return 0;
  };
  WEBRTC_STUB_CONST(GetReceiveCodec, (const int, webrtc::VideoCodec&));
  WEBRTC_STUB_CONST(GetCodecConfigParameters, (const int,
      unsigned char*, unsigned char&));
  WEBRTC_STUB(SetImageScaleStatus, (const int, const bool));
  WEBRTC_STUB_CONST(GetSendCodecStastistics, (const int,
      unsigned int&, unsigned int&));
  WEBRTC_STUB_CONST(GetReceiveCodecStastistics, (const int,
      unsigned int&, unsigned int&));
  WEBRTC_STUB_CONST(GetReceiveSideDelay, (const int video_channel,
                                          int* delay_ms));
  WEBRTC_FUNC_CONST(GetCodecTargetBitrate, (const int channel,
      unsigned int* codec_target_bitrate)) {
    WEBRTC_CHECK_CHANNEL(channel);

    std::map<int, Channel*>::const_iterator it = channels_.find(channel);
    if (it->second->send) {
      // Assume the encoder produces the expected rate.
      *codec_target_bitrate = it->second->send_video_bitrate_;
    } else {
      *codec_target_bitrate = 0;
    }
    return 0;
  }
  virtual unsigned int GetDiscardedPackets(const int channel) const {
    return 0;
  }

  WEBRTC_STUB(SetKeyFrameRequestCallbackStatus, (const int, const bool));
  WEBRTC_STUB(SetSignalKeyPacketLossStatus, (const int, const bool,
      const bool));
  WEBRTC_STUB(RegisterEncoderObserver, (const int,
      webrtc::ViEEncoderObserver&));
  WEBRTC_STUB(DeregisterEncoderObserver, (const int));
  WEBRTC_STUB(RegisterDecoderObserver, (const int,
      webrtc::ViEDecoderObserver&));
  WEBRTC_STUB(DeregisterDecoderObserver, (const int));
  WEBRTC_STUB(SendKeyFrame, (const int));
  WEBRTC_STUB(WaitForFirstKeyFrame, (const int, const bool));
  WEBRTC_STUB(StartDebugRecording, (int, const char*));
  WEBRTC_STUB(StopDebugRecording, (int));
  WEBRTC_VOID_STUB(SuspendBelowMinBitrate, (int));

  // webrtc::ViECapture
  WEBRTC_STUB(NumberOfCaptureDevices, ());
  WEBRTC_STUB(GetCaptureDevice, (unsigned int, char*,
      const unsigned int, char*, const unsigned int));
  WEBRTC_STUB(AllocateCaptureDevice, (const char*, const unsigned int, int&));
  WEBRTC_FUNC(AllocateExternalCaptureDevice,
              (int& capture_id, webrtc::ViEExternalCapture*& capture)) {
    if (fail_alloc_capturer_) {
      return -1;
    }
    if (kViECaptureIdMax == last_capturer_) {
      return -1;
    }
    Capturer* cap = new Capturer();
    capturers_[++last_capturer_] = cap;
    capture_id = last_capturer_;
    capture = cap;
    return 0;
  }
  WEBRTC_STUB(AllocateCaptureDevice, (webrtc::VideoCaptureModule&, int&));
  WEBRTC_FUNC(ReleaseCaptureDevice, (const int capture_id)) {
    WEBRTC_CHECK_CAPTURER(capture_id);
    delete capturers_[capture_id];
    capturers_.erase(capture_id);
    return 0;
  }
  WEBRTC_FUNC(ConnectCaptureDevice, (const int capture_id,
                                     const int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    WEBRTC_CHECK_CAPTURER(capture_id);
    channels_[channel]->capture_id_ = capture_id;
    capturers_[capture_id]->set_channel_id(channel);
    return 0;
  }
  WEBRTC_FUNC(DisconnectCaptureDevice, (const int channel)) {
    WEBRTC_CHECK_CHANNEL(channel);
    int capture_id = channels_[channel]->capture_id_;
    WEBRTC_CHECK_CAPTURER(capture_id);
    channels_[channel]->capture_id_ = -1;
    capturers_[capture_id]->set_channel_id(-1);
    return 0;
  }
  WEBRTC_STUB(StartCapture, (const int, const webrtc::CaptureCapability&));
  WEBRTC_STUB(StopCapture, (const int));
  WEBRTC_STUB(SetRotateCapturedFrames, (const int,
      const webrtc::RotateCapturedFrame));
  WEBRTC_STUB(SetCaptureDelay, (const int, const unsigned int));
  WEBRTC_STUB(NumberOfCapabilities, (const char*, const unsigned int));
  WEBRTC_STUB(GetCaptureCapability, (const char*, const unsigned int,
      const unsigned int, webrtc::CaptureCapability&));
  WEBRTC_STUB(ShowCaptureSettingsDialogBox, (const char*, const unsigned int,
      const char*, void*, const unsigned int, const unsigned int));
  WEBRTC_STUB(GetOrientation, (const char*, webrtc::RotateCapturedFrame&));
  WEBRTC_STUB(EnableBrightnessAlarm, (const int, const bool));
  WEBRTC_STUB(RegisterObserver, (const int, webrtc::ViECaptureObserver&));
  WEBRTC_STUB(DeregisterObserver, (const int));

  // webrtc::ViENetwork
  WEBRTC_VOID_FUNC(SetNetworkTransmissionState, (const int channel,
                                                 const bool is_transmitting)) {
    WEBRTC_ASSERT_CHANNEL(channel);
    channels_[channel]->can_transmit_ = is_transmitting;
  }
  WEBRTC_STUB(RegisterSendTransport, (const int, webrtc::Transport&));
  WEBRTC_STUB(DeregisterSendTransport, (const int));
#ifdef USE_WEBRTC_DEV_BRANCH
  WEBRTC_STUB(ReceivedRTPPacket, (const int, const void*, const int,
      const webrtc::PacketTime&));
#else
  WEBRTC_STUB(ReceivedRTPPacket, (const int, const void*, const int));
#endif
  WEBRTC_STUB(ReceivedRTCPPacket, (const int, const void*, const int));
  // Not using WEBRTC_STUB due to bool return value
  virtual bool IsIPv6Enabled(int channel) { return true; }
  WEBRTC_STUB(SetMTU, (int, unsigned int));

  // webrtc::ViERender
  WEBRTC_STUB(RegisterVideoRenderModule, (webrtc::VideoRender&));
  WEBRTC_STUB(DeRegisterVideoRenderModule, (webrtc::VideoRender&));
  WEBRTC_STUB(AddRenderer, (const int, void*, const unsigned int, const float,
      const float, const float, const float));
  WEBRTC_FUNC(RemoveRenderer, (const int render_id)) {
    if (IsCapturerId(render_id)) {
      WEBRTC_CHECK_CAPTURER(render_id);
      return 0;
    } else if (IsChannelId(render_id)) {
      WEBRTC_CHECK_CHANNEL(render_id);
      channels_[render_id]->has_renderer_ = false;
      return 0;
    }
    return -1;
  }
  WEBRTC_FUNC(StartRender, (const int render_id)) {
    if (IsCapturerId(render_id)) {
      WEBRTC_CHECK_CAPTURER(render_id);
      return 0;
    } else if (IsChannelId(render_id)) {
      WEBRTC_CHECK_CHANNEL(render_id);
      channels_[render_id]->render_started_ = true;
      return 0;
    }
    return -1;
  }
  WEBRTC_FUNC(StopRender, (const int render_id)) {
    if (IsCapturerId(render_id)) {
      WEBRTC_CHECK_CAPTURER(render_id);
      return 0;
    } else if (IsChannelId(render_id)) {
      WEBRTC_CHECK_CHANNEL(render_id);
      channels_[render_id]->render_started_ = false;
      return 0;
    }
    return -1;
  }
  WEBRTC_STUB(SetExpectedRenderDelay, (int render_id, int render_delay));
  WEBRTC_STUB(ConfigureRender, (int, const unsigned int, const float,
      const float, const float, const float));
  WEBRTC_STUB(MirrorRenderStream, (const int, const bool, const bool,
      const bool));
  WEBRTC_FUNC(AddRenderer, (const int render_id,
                            webrtc::RawVideoType video_type,
                            webrtc::ExternalRenderer* renderer)) {
    if (IsCapturerId(render_id)) {
      WEBRTC_CHECK_CAPTURER(render_id);
      return 0;
    } else if (IsChannelId(render_id)) {
      WEBRTC_CHECK_CHANNEL(render_id);
      channels_[render_id]->has_renderer_ = true;
      return 0;
    }
    return -1;
  }

  // webrtc::ViERTP_RTCP
  WEBRTC_FUNC(SetLocalSSRC, (const int channel,
                             const unsigned int ssrc,
                             const webrtc::StreamType usage,
                             const unsigned char idx)) {
    WEBRTC_CHECK_CHANNEL(channel);
    switch (usage) {
      case webrtc::kViEStreamTypeNormal:
        channels_[channel]->ssrcs_[idx] = ssrc;
        break;
      case webrtc::kViEStreamTypeRtx:
        channels_[channel]->rtx_ssrcs_[idx] = ssrc;
        break;
      default:
        return -1;
    }
    return 0;
  }

  WEBRTC_FUNC_CONST(SetRemoteSSRCType, (const int channel,
        const webrtc::StreamType usage, const unsigned int ssrc)) {
    WEBRTC_CHECK_CHANNEL(channel);
    if (usage == webrtc::kViEStreamTypeRtx) {
      channels_.find(channel)->second->remote_rtx_ssrc_ = ssrc;
      return 0;
    }
    return -1;
  }

  WEBRTC_FUNC_CONST(GetLocalSSRC, (const int channel,
                                   unsigned int& ssrc)) {
    // ssrcs_[0] is the default local ssrc.
    WEBRTC_CHECK_CHANNEL(channel);
    ssrc = channels_.find(channel)->second->ssrcs_[0];
    return 0;
  }
  WEBRTC_STUB_CONST(GetRemoteSSRC, (const int, unsigned int&));
  WEBRTC_STUB_CONST(GetRemoteCSRCs, (const int, unsigned int*));

  WEBRTC_FUNC(SetRtxSendPayloadType, (const int channel,
                                      const uint8 payload_type)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->rtx_send_payload_type = payload_type;
    return 0;
  }
  WEBRTC_STUB(SetRtxReceivePayloadType, (const int, const uint8));

  WEBRTC_STUB(SetStartSequenceNumber, (const int, unsigned short));
  WEBRTC_FUNC(SetRTCPStatus,
              (const int channel, const webrtc::ViERTCPMode mode)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->rtcp_status_ = mode;
    return 0;
  }
  WEBRTC_STUB_CONST(GetRTCPStatus, (const int, webrtc::ViERTCPMode&));
  WEBRTC_FUNC(SetRTCPCName, (const int channel,
                             const char rtcp_cname[KMaxRTCPCNameLength])) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->cname_.assign(rtcp_cname);
    return 0;
  }
  WEBRTC_FUNC_CONST(GetRTCPCName, (const int channel,
                                   char rtcp_cname[KMaxRTCPCNameLength])) {
    WEBRTC_CHECK_CHANNEL(channel);
    talk_base::strcpyn(rtcp_cname, KMaxRTCPCNameLength,
                       channels_.find(channel)->second->cname_.c_str());
    return 0;
  }
  WEBRTC_STUB_CONST(GetRemoteRTCPCName, (const int, char*));
  WEBRTC_STUB(SendApplicationDefinedRTCPPacket, (const int, const unsigned char,
      unsigned int, const char*, unsigned short));
  WEBRTC_FUNC(SetNACKStatus, (const int channel, const bool enable)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->nack_ = enable;
    channels_[channel]->hybrid_nack_fec_ = false;
    return 0;
  }
  WEBRTC_STUB(SetFECStatus, (const int, const bool, const unsigned char,
      const unsigned char));
  WEBRTC_FUNC(SetHybridNACKFECStatus, (const int channel, const bool enable,
      const unsigned char red_type, const unsigned char fec_type)) {
    WEBRTC_CHECK_CHANNEL(channel);
    if (red_type == fec_type ||
        red_type == channels_[channel]->send_codec.plType ||
        fec_type == channels_[channel]->send_codec.plType) {
      return -1;
    }
    channels_[channel]->nack_ = false;
    channels_[channel]->hybrid_nack_fec_ = enable;
    return 0;
  }
  WEBRTC_FUNC(SetKeyFrameRequestMethod,
              (const int channel,
               const webrtc::ViEKeyFrameRequestMethod method)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->key_frame_request_method_ = method;
    return 0;
  }
  WEBRTC_FUNC(SetSenderBufferingMode, (int channel, int target_delay)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->sender_target_delay_ = target_delay;
    return 0;
  }
  WEBRTC_FUNC(SetReceiverBufferingMode, (int channel, int target_delay)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->receiver_target_delay_ = target_delay;
    return 0;
  }
  // |Send| and |receive| are stored locally in variables that more clearly
  // explain what they mean.
  WEBRTC_FUNC(SetRembStatus, (int channel, bool send, bool receive)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->remb_contribute_ = receive;
    channels_[channel]->remb_bw_partition_ = send;
    return 0;
  }
  WEBRTC_FUNC(SetTMMBRStatus, (const int channel, const bool enable)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->tmmbr_ = enable;
    return 0;
  }
  WEBRTC_FUNC(SetSendTimestampOffsetStatus, (int channel, bool enable,
      int id)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->rtp_offset_send_id_ = (enable) ? id : 0;
    return 0;
  }
  WEBRTC_FUNC(SetReceiveTimestampOffsetStatus, (int channel, bool enable,
      int id)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->rtp_offset_receive_id_ = (enable) ? id : 0;
    return 0;
  }
  WEBRTC_FUNC(SetSendAbsoluteSendTimeStatus, (int channel, bool enable,
      int id)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->rtp_absolute_send_time_send_id_ = (enable) ? id : 0;
    return 0;
  }
  WEBRTC_FUNC(SetReceiveAbsoluteSendTimeStatus, (int channel, bool enable,
      int id)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->rtp_absolute_send_time_receive_id_ = (enable) ? id : 0;
    return 0;
  }
#ifdef USE_WEBRTC_DEV_BRANCH
  WEBRTC_STUB(SetRtcpXrRrtrStatus, (int, bool));
#endif
  WEBRTC_FUNC(SetTransmissionSmoothingStatus, (int channel, bool enable)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->transmission_smoothing_ = enable;
    return 0;
  }
  WEBRTC_STUB_CONST(GetReceivedRTCPStatistics, (const int, unsigned short&,
      unsigned int&, unsigned int&, unsigned int&, int&));
  WEBRTC_STUB_CONST(GetSentRTCPStatistics, (const int, unsigned short&,
      unsigned int&, unsigned int&, unsigned int&, int&));
  WEBRTC_STUB_CONST(GetRTPStatistics, (const int, unsigned int&, unsigned int&,
      unsigned int&, unsigned int&));
  WEBRTC_STUB_CONST(GetReceiveChannelRtcpStatistics, (const int,
      webrtc::RtcpStatistics&, int&));
  WEBRTC_STUB_CONST(GetSendChannelRtcpStatistics, (const int,
      webrtc::RtcpStatistics&, int&));
  WEBRTC_STUB_CONST(GetRtpStatistics, (const int, webrtc::StreamDataCounters&,
      webrtc::StreamDataCounters&));
  WEBRTC_FUNC_CONST(GetBandwidthUsage, (const int channel,
      unsigned int& total_bitrate, unsigned int& video_bitrate,
      unsigned int& fec_bitrate, unsigned int& nack_bitrate)) {
    WEBRTC_CHECK_CHANNEL(channel);
    std::map<int, Channel*>::const_iterator it = channels_.find(channel);
    if (it->second->send) {
      video_bitrate = it->second->send_video_bitrate_;
      fec_bitrate = it->second->send_fec_bitrate_;
      nack_bitrate = it->second->send_nack_bitrate_;
      total_bitrate = video_bitrate + fec_bitrate + nack_bitrate;
    } else {
      total_bitrate = 0;
      video_bitrate = 0;
      fec_bitrate = 0;
      nack_bitrate = 0;
    }
    return 0;
  }
  WEBRTC_FUNC_CONST(GetEstimatedSendBandwidth, (const int channel,
      unsigned int* send_bandwidth_estimate)) {
    WEBRTC_CHECK_CHANNEL(channel);
    std::map<int, Channel*>::const_iterator it = channels_.find(channel);
    // Assume the current video, fec and nack bitrate sums up to our estimate.
    if (it->second->send) {
      *send_bandwidth_estimate = it->second->send_bandwidth_;
    } else {
      *send_bandwidth_estimate = 0;
    }
    return 0;
  }
  WEBRTC_FUNC_CONST(GetEstimatedReceiveBandwidth, (const int channel,
      unsigned int* receive_bandwidth_estimate)) {
    WEBRTC_CHECK_CHANNEL(channel);
    std::map<int, Channel*>::const_iterator it = channels_.find(channel);
    if (it->second->receive_) {
    // For simplicity, assume all channels receive half of max send rate.
      *receive_bandwidth_estimate = it->second->receive_bandwidth_;
    } else {
      *receive_bandwidth_estimate = 0;
    }
    return 0;
  }
  WEBRTC_STUB(RegisterSendChannelRtcpStatisticsCallback,
                    (int, webrtc::RtcpStatisticsCallback*));
  WEBRTC_STUB(DeregisterSendChannelRtcpStatisticsCallback,
                    (int, webrtc::RtcpStatisticsCallback*));
  WEBRTC_STUB(RegisterReceiveChannelRtcpStatisticsCallback,
                    (int, webrtc::RtcpStatisticsCallback*));
  WEBRTC_STUB(DeregisterReceiveChannelRtcpStatisticsCallback,
                    (int, webrtc::RtcpStatisticsCallback*));
  WEBRTC_STUB(RegisterSendChannelRtpStatisticsCallback,
                    (int, webrtc::StreamDataCountersCallback*));
  WEBRTC_STUB(DeregisterSendChannelRtpStatisticsCallback,
                    (int, webrtc::StreamDataCountersCallback*));
  WEBRTC_STUB(RegisterReceiveChannelRtpStatisticsCallback,
                    (int, webrtc::StreamDataCountersCallback*));
  WEBRTC_STUB(DeregisterReceiveChannelRtpStatisticsCallback,
                    (int, webrtc::StreamDataCountersCallback*));
  WEBRTC_STUB(RegisterSendBitrateObserver,
                    (int, webrtc::BitrateStatisticsObserver*));
  WEBRTC_STUB(DeregisterSendBitrateObserver,
                    (int, webrtc::BitrateStatisticsObserver*));
  WEBRTC_STUB(RegisterSendFrameCountObserver,
                    (int, webrtc::FrameCountObserver*));
  WEBRTC_STUB(DeregisterSendFrameCountObserver,
                    (int, webrtc::FrameCountObserver*));

  WEBRTC_STUB(StartRTPDump, (const int, const char*, webrtc::RTPDirections));
  WEBRTC_STUB(StopRTPDump, (const int, webrtc::RTPDirections));
  WEBRTC_STUB(RegisterRTPObserver, (const int, webrtc::ViERTPObserver&));
  WEBRTC_STUB(DeregisterRTPObserver, (const int));
  WEBRTC_STUB(RegisterRTCPObserver, (const int, webrtc::ViERTCPObserver&));
  WEBRTC_STUB(DeregisterRTCPObserver, (const int));

  // webrtc::ViEImageProcess
  WEBRTC_STUB(RegisterCaptureEffectFilter, (const int,
      webrtc::ViEEffectFilter&));
  WEBRTC_STUB(DeregisterCaptureEffectFilter, (const int));
  WEBRTC_STUB(RegisterSendEffectFilter, (const int,
      webrtc::ViEEffectFilter&));
  WEBRTC_STUB(DeregisterSendEffectFilter, (const int));
  WEBRTC_STUB(RegisterRenderEffectFilter, (const int,
      webrtc::ViEEffectFilter&));
  WEBRTC_STUB(DeregisterRenderEffectFilter, (const int));
  WEBRTC_STUB(EnableDeflickering, (const int, const bool));
  WEBRTC_FUNC(EnableDenoising, (const int capture_id, const bool denoising)) {
    WEBRTC_CHECK_CAPTURER(capture_id);
    capturers_[capture_id]->set_denoising(denoising);
    return 0;
  }
  WEBRTC_STUB(EnableColorEnhancement, (const int, const bool));
  WEBRTC_VOID_STUB(RegisterPreEncodeCallback,
                   (int, webrtc::I420FrameCallback*));
  WEBRTC_VOID_STUB(DeRegisterPreEncodeCallback, (int));
  WEBRTC_VOID_STUB(RegisterPreRenderCallback,
                   (int, webrtc::I420FrameCallback*));
  WEBRTC_VOID_STUB(DeRegisterPreRenderCallback, (int));
  // webrtc::ViEExternalCodec
  WEBRTC_FUNC(RegisterExternalSendCodec,
      (const int channel, const unsigned char pl_type, webrtc::VideoEncoder*,
          bool)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->ext_encoder_pl_types_.insert(pl_type);
    return 0;
  }
  WEBRTC_FUNC(DeRegisterExternalSendCodec,
      (const int channel, const unsigned char pl_type)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->ext_encoder_pl_types_.erase(pl_type);
    return 0;
  }
  WEBRTC_FUNC(RegisterExternalReceiveCodec,
      (const int channel, const unsigned int pl_type, webrtc::VideoDecoder*,
       bool, int)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->ext_decoder_pl_types_.insert(pl_type);
    return 0;
  }
  WEBRTC_FUNC(DeRegisterExternalReceiveCodec,
      (const int channel, const unsigned char pl_type)) {
    WEBRTC_CHECK_CHANNEL(channel);
    channels_[channel]->ext_decoder_pl_types_.erase(pl_type);
    return 0;
  }

 private:
  bool IsChannelId(int id) const {
    return (id >= kViEChannelIdBase && id <= kViEChannelIdMax);
  }
  bool IsCapturerId(int id) const {
    return (id >= kViECaptureIdBase && id <= kViECaptureIdMax);
  }

  bool inited_;
  int last_channel_;
  std::map<int, Channel*> channels_;
  bool fail_create_channel_;
  int last_capturer_;
  std::map<int, Capturer*> capturers_;
  bool fail_alloc_capturer_;
  const cricket::VideoCodec* const* codecs_;
  int num_codecs_;
  int num_set_send_codecs_;  // how many times we call SetSendCodec().
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTC_FAKEWEBRTCVIDEOENGINE_H_
