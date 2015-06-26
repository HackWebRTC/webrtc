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

#include "talk/media/base/codec.h"
#include "talk/media/webrtc/fakewebrtccommon.h"
#include "talk/media/webrtc/webrtcvideodecoderfactory.h"
#include "talk/media/webrtc/webrtcvideoencoderfactory.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/video_coding/codecs/interface/video_error_codes.h"
#include "webrtc/video_decoder.h"
#include "webrtc/video_encoder.h"

namespace cricket {

static const int kMinVideoBitrate = 100;
static const int kStartVideoBitrate = 300;
static const int kMaxVideoBitrate = 1000;

// WebRtc channel id and capture id share the same number space.
// This is how AddRenderer(renderId, ...) is able to tell if it is adding a
// renderer for a channel or it is adding a renderer for a capturer.
static const int kViEChannelIdBase = 0;
static const int kViEChannelIdMax = 1000;

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
      const webrtc::CodecSpecificInfo*, int64_t) {
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
  FakeWebRtcVideoEncoder() : num_frames_encoded_(0) {}

  virtual int32 InitEncode(const webrtc::VideoCodec* codecSettings,
                           int32 numberOfCores,
                           size_t maxPayloadSize) {
    rtc::CritScope lock(&crit_);
    codec_settings_ = *codecSettings;
    return WEBRTC_VIDEO_CODEC_OK;
  }

  webrtc::VideoCodec GetCodecSettings() {
    rtc::CritScope lock(&crit_);
    return codec_settings_;
  }

  virtual int32 Encode(const webrtc::VideoFrame& inputImage,
                       const webrtc::CodecSpecificInfo* codecSpecificInfo,
                       const std::vector<webrtc::VideoFrameType>* frame_types) {
    rtc::CritScope lock(&crit_);
    ++num_frames_encoded_;
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
                                     int64_t rtt) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  virtual int32 SetRates(uint32 newBitRate,
                         uint32 frameRate) {
    return WEBRTC_VIDEO_CODEC_OK;
  }

  int GetNumEncodedFrames() {
    rtc::CritScope lock(&crit_);
    return num_frames_encoded_;
  }

 private:
  rtc::CriticalSection crit_;
  int num_frames_encoded_ GUARDED_BY(crit_);
  webrtc::VideoCodec codec_settings_ GUARDED_BY(crit_);
};

// Fake class for mocking out WebRtcVideoEncoderFactory.
class FakeWebRtcVideoEncoderFactory : public WebRtcVideoEncoderFactory {
 public:
  FakeWebRtcVideoEncoderFactory()
      : num_created_encoders_(0), encoders_have_internal_sources_(false) {}

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

  virtual const std::vector<WebRtcVideoEncoderFactory::VideoCodec>& codecs()
      const {
    return codecs_;
  }

  virtual bool EncoderTypeHasInternalSource(
      webrtc::VideoCodecType type) const override {
    return encoders_have_internal_sources_;
  }

  void set_encoders_have_internal_sources(bool internal_source) {
    encoders_have_internal_sources_ = internal_source;
  }

  void AddSupportedVideoCodecType(webrtc::VideoCodecType type,
                                  const std::string& name) {
    supported_codec_types_.insert(type);
    codecs_.push_back(
        WebRtcVideoEncoderFactory::VideoCodec(type, name, 1280, 720, 30));
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
  int num_created_encoders_;
  bool encoders_have_internal_sources_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTC_FAKEWEBRTCVIDEOENGINE_H_
