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

#ifndef TALK_MEDIA_BASE_HYBRIDVIDEOENGINE_H_
#define TALK_MEDIA_BASE_HYBRIDVIDEOENGINE_H_

#include <string>
#include <vector>

#include "talk/base/logging.h"
#include "talk/base/sigslotrepeater.h"
#include "talk/media/base/codec.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/videocapturer.h"
#include "talk/media/base/videocommon.h"

namespace cricket {

struct Device;
struct VideoFormat;
class HybridVideoEngineInterface;
class VideoCapturer;
class VideoFrame;
class VideoRenderer;

// HybridVideoMediaChannels work with a HybridVideoEngine to combine
// two unrelated VideoMediaChannel implementations into a single class.
class HybridVideoMediaChannel : public VideoMediaChannel {
 public:
  HybridVideoMediaChannel(HybridVideoEngineInterface* engine,
                          VideoMediaChannel* channel1,
                          VideoMediaChannel* channel2);
  virtual ~HybridVideoMediaChannel();

  // VideoMediaChannel methods
  virtual void SetInterface(NetworkInterface* iface);
  virtual bool SetOptions(const VideoOptions& options);
  virtual bool GetOptions(VideoOptions* options) const;
  virtual bool AddSendStream(const StreamParams& sp);
  virtual bool RemoveSendStream(uint32 ssrc);
  virtual bool SetRenderer(uint32 ssrc, VideoRenderer* renderer);
  virtual bool SetRender(bool render);
  virtual bool MuteStream(uint32 ssrc, bool muted);

  virtual bool SetRecvCodecs(const std::vector<VideoCodec>& codecs);
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions);

  virtual bool SetSendCodecs(const std::vector<VideoCodec>& codecs);
  virtual bool GetSendCodec(VideoCodec* codec);
  virtual bool SetSendStreamFormat(uint32 ssrc, const VideoFormat& format);
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions);
  virtual bool SetStartSendBandwidth(int bps);
  virtual bool SetMaxSendBandwidth(int bps);
  virtual bool SetSend(bool send);

  virtual bool AddRecvStream(const StreamParams& sp);
  virtual bool RemoveRecvStream(uint32 ssrc);
  virtual bool SetCapturer(uint32 ssrc, VideoCapturer* capturer);

  virtual bool SendIntraFrame();
  virtual bool RequestIntraFrame();

  virtual bool GetStats(const StatsOptions& options, VideoMediaInfo* info);

  virtual void OnPacketReceived(talk_base::Buffer* packet,
                                const talk_base::PacketTime& packet_time);
  virtual void OnRtcpReceived(talk_base::Buffer* packet,
                              const talk_base::PacketTime& packet_time);
  virtual void OnReadyToSend(bool ready);

  virtual void UpdateAspectRatio(int ratio_w, int ratio_h);

  void OnLocalFrame(VideoCapturer*, const VideoFrame*);
  void OnLocalFrameFormat(VideoCapturer*, const VideoFormat*);

  bool sending() const { return sending_; }

 private:
  bool SelectActiveChannel(const std::vector<VideoCodec>& codecs);
  void SplitCodecs(const std::vector<VideoCodec>& codecs,
                   std::vector<VideoCodec>* codecs1,
                   std::vector<VideoCodec>* codecs2);

  void OnMediaError(uint32 ssrc, Error error);

  HybridVideoEngineInterface* engine_;
  talk_base::scoped_ptr<VideoMediaChannel> channel1_;
  talk_base::scoped_ptr<VideoMediaChannel> channel2_;
  VideoMediaChannel* active_channel_;
  bool sending_;
};

// Interface class for HybridVideoChannels to talk to the engine.
class HybridVideoEngineInterface {
 public:
  virtual ~HybridVideoEngineInterface() {}
  virtual bool HasCodec1(const VideoCodec& codec) = 0;
  virtual bool HasCodec2(const VideoCodec& codec) = 0;
  virtual void OnSendChange1(VideoMediaChannel* channel1, bool send) = 0;
  virtual void OnSendChange2(VideoMediaChannel* channel1, bool send) = 0;
  virtual void OnNewSendResolution(int width, int height) = 0;
};

// The HybridVideoEngine class combines two unrelated VideoEngine impls
// into a single class. It creates HybridVideoMediaChannels that also contain
// a VideoMediaChannel implementation from each engine. Policy is then used
// during call setup to determine which VideoMediaChannel should be used.
// Currently, this policy is based on what codec the remote side wants to use.
template<class VIDEO1, class VIDEO2>
class HybridVideoEngine : public HybridVideoEngineInterface {
 public:
  HybridVideoEngine() {
    // Unify the codec lists.
    codecs_ = video1_.codecs();
    codecs_.insert(codecs_.end(), video2_.codecs().begin(),
                   video2_.codecs().end());

    rtp_header_extensions_ = video1_.rtp_header_extensions();
    rtp_header_extensions_.insert(rtp_header_extensions_.end(),
                                  video2_.rtp_header_extensions().begin(),
                                  video2_.rtp_header_extensions().end());

    SignalCaptureStateChange.repeat(video2_.SignalCaptureStateChange);
  }

  bool Init(talk_base::Thread* worker_thread) {
    if (!video1_.Init(worker_thread)) {
      LOG(LS_ERROR) << "Failed to init VideoEngine1";
      return false;
    }
    if (!video2_.Init(worker_thread)) {
      LOG(LS_ERROR) << "Failed to init VideoEngine2";
      video1_.Terminate();
      return false;
    }
    return true;
  }
  void Terminate() {
    video1_.Terminate();
    video2_.Terminate();
  }

  int GetCapabilities() {
    return (video1_.GetCapabilities() | video2_.GetCapabilities());
  }
  HybridVideoMediaChannel* CreateChannel(VoiceMediaChannel* channel) {
    talk_base::scoped_ptr<VideoMediaChannel> channel1(
        video1_.CreateChannel(channel));
    if (!channel1) {
      LOG(LS_ERROR) << "Failed to create VideoMediaChannel1";
      return NULL;
    }
    talk_base::scoped_ptr<VideoMediaChannel> channel2(
        video2_.CreateChannel(channel));
    if (!channel2) {
      LOG(LS_ERROR) << "Failed to create VideoMediaChannel2";
      return NULL;
    }
    return new HybridVideoMediaChannel(this,
        channel1.release(), channel2.release());
  }

  bool SetOptions(const VideoOptions& options) {
    return video1_.SetOptions(options) && video2_.SetOptions(options);
  }
  bool SetDefaultEncoderConfig(const VideoEncoderConfig& config) {
    VideoEncoderConfig conf = config;
    if (video1_.codecs().size() > 0) {
      conf.max_codec.name = video1_.codecs()[0].name;
      if (!video1_.SetDefaultEncoderConfig(conf)) {
        LOG(LS_ERROR) << "Failed to SetDefaultEncoderConfig for video1";
        return false;
      }
    }
    if (video2_.codecs().size() > 0) {
      conf.max_codec.name = video2_.codecs()[0].name;
      if (!video2_.SetDefaultEncoderConfig(conf)) {
        LOG(LS_ERROR) << "Failed to SetDefaultEncoderConfig for video2";
        return false;
      }
    }
    return true;
  }
  VideoEncoderConfig GetDefaultEncoderConfig() const {
    // This looks pretty strange, but, in practice, it'll do sane things if
    // GetDefaultEncoderConfig is only called after SetDefaultEncoderConfig,
    // since both engines should be essentially equivalent at that point. If it
    // hasn't been called, though, we'll use the first meaningful encoder
    // config, or the config from the second video engine if neither are
    // meaningful.
    VideoEncoderConfig config = video1_.GetDefaultEncoderConfig();
    if (config.max_codec.width != 0) {
      return config;
    } else {
      return video2_.GetDefaultEncoderConfig();
    }
  }
  const std::vector<VideoCodec>& codecs() const {
    return codecs_;
  }
  const std::vector<RtpHeaderExtension>& rtp_header_extensions() const {
    return rtp_header_extensions_;
  }
  void SetLogging(int min_sev, const char* filter) {
    video1_.SetLogging(min_sev, filter);
    video2_.SetLogging(min_sev, filter);
  }

  VideoFormat GetStartCaptureFormat() const {
    return video2_.GetStartCaptureFormat();
  }

  // TODO(juberti): Remove these functions after we do the capturer refactoring.
  // For now they are set to always use the second engine for capturing, which
  // is convenient given our intended use case.
  bool SetCaptureDevice(const Device* device) {
    return video2_.SetCaptureDevice(device);
  }
  VideoCapturer* GetVideoCapturer() const {
    return video2_.GetVideoCapturer();
  }
  bool SetLocalRenderer(VideoRenderer* renderer) {
    return video2_.SetLocalRenderer(renderer);
  }
  sigslot::repeater2<VideoCapturer*, CaptureState> SignalCaptureStateChange;

  virtual bool HasCodec1(const VideoCodec& codec) {
    return HasCodec(video1_, codec);
  }
  virtual bool HasCodec2(const VideoCodec& codec) {
    return HasCodec(video2_, codec);
  }
  template<typename VIDEO>
  bool HasCodec(const VIDEO& engine, const VideoCodec& codec) const {
    for (std::vector<VideoCodec>::const_iterator i = engine.codecs().begin();
         i != engine.codecs().end();
         ++i) {
      if (i->Matches(codec)) {
        return true;
      }
    }
    return false;
  }
  virtual void OnSendChange1(VideoMediaChannel* channel1, bool send) {
  }
  virtual void OnSendChange2(VideoMediaChannel* channel2, bool send) {
  }
  virtual void OnNewSendResolution(int width, int height) {
  }

 protected:
  VIDEO1 video1_;
  VIDEO2 video2_;
  std::vector<VideoCodec> codecs_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_HYBRIDVIDEOENGINE_H_
