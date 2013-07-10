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

#include "talk/media/base/hybridvideoengine.h"

#include "talk/base/logging.h"

namespace cricket {

HybridVideoMediaChannel::HybridVideoMediaChannel(
    HybridVideoEngineInterface* engine,
    VideoMediaChannel* channel1,
    VideoMediaChannel* channel2)
    : engine_(engine),
      channel1_(channel1),
      channel2_(channel2),
      active_channel_(NULL),
      sending_(false) {
}

HybridVideoMediaChannel::~HybridVideoMediaChannel() {
}

void HybridVideoMediaChannel::SetInterface(NetworkInterface* iface) {
  if (channel1_) {
    channel1_->SetInterface(iface);
  }
  if (channel2_) {
    channel2_->SetInterface(iface);
  }
}

bool HybridVideoMediaChannel::SetOptions(const VideoOptions &options) {
  bool ret = true;
  if (channel1_) {
    ret = channel1_->SetOptions(options);
  }
  if (channel2_ && ret) {
    ret = channel2_->SetOptions(options);
  }
  return ret;
}

bool HybridVideoMediaChannel::GetOptions(VideoOptions *options) const {
  if (active_channel_) {
    return active_channel_->GetOptions(options);
  }
  if (channel1_) {
    return channel1_->GetOptions(options);
  }
  if (channel2_) {
    return channel2_->GetOptions(options);
  }
  return false;
}

bool HybridVideoMediaChannel::SetRecvCodecs(
    const std::vector<VideoCodec>& codecs) {
  // Only give each channel the codecs it knows about.
  bool ret = true;
  std::vector<VideoCodec> codecs1, codecs2;
  SplitCodecs(codecs, &codecs1, &codecs2);
  if (channel1_) {
    ret = channel1_->SetRecvCodecs(codecs1);
  }
  if (channel2_ && ret) {
    ret = channel2_->SetRecvCodecs(codecs2);
  }
  return ret;
}

bool HybridVideoMediaChannel::SetRecvRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  bool ret = true;
  if (channel1_) {
    ret = channel1_->SetRecvRtpHeaderExtensions(extensions);
  }
  if (channel2_ && ret) {
    ret = channel2_->SetRecvRtpHeaderExtensions(extensions);
  }
  return ret;
}

bool HybridVideoMediaChannel::SetRenderer(uint32 ssrc,
                                          VideoRenderer* renderer) {
  bool ret = true;
  if (channel1_) {
    ret = channel1_->SetRenderer(ssrc, renderer);
  }
  if (channel2_ && ret) {
    ret = channel2_->SetRenderer(ssrc, renderer);
  }
  return ret;
}

bool HybridVideoMediaChannel::SetRender(bool render) {
  bool ret = true;
  if (channel1_) {
    ret = channel1_->SetRender(render);
  }
  if (channel2_ && ret) {
    ret = channel2_->SetRender(render);
  }
  return ret;
}

bool HybridVideoMediaChannel::MuteStream(uint32 ssrc, bool muted) {
  bool ret = true;
  if (channel1_) {
    ret = channel1_->MuteStream(ssrc, muted);
  }
  if (channel2_ && ret) {
    ret = channel2_->MuteStream(ssrc, muted);
  }
  return ret;
}

bool HybridVideoMediaChannel::SetSendCodecs(
    const std::vector<VideoCodec>& codecs) {
  // Use the input to this function to decide what impl we're going to use.
  if (!active_channel_ && !SelectActiveChannel(codecs)) {
    LOG(LS_WARNING) << "Failed to select active channel";
    return false;
  }
  // Only give the active channel the codecs it knows about.
  std::vector<VideoCodec> codecs1, codecs2;
  SplitCodecs(codecs, &codecs1, &codecs2);
  const std::vector<VideoCodec>& codecs_to_set =
      (active_channel_ == channel1_.get()) ? codecs1 : codecs2;
  bool return_value = active_channel_->SetSendCodecs(codecs_to_set);
  if (!return_value) {
    return false;
  }
  VideoCodec send_codec;
  return_value = active_channel_->GetSendCodec(&send_codec);
  if (!return_value) {
    return false;
  }
  engine_->OnNewSendResolution(send_codec.width, send_codec.height);
  active_channel_->UpdateAspectRatio(send_codec.width, send_codec.height);
  return true;
}

bool HybridVideoMediaChannel::GetSendCodec(VideoCodec* send_codec) {
  if (!active_channel_) {
    return false;
  }
  return active_channel_->GetSendCodec(send_codec);
}

bool HybridVideoMediaChannel::SetSendStreamFormat(uint32 ssrc,
                                                  const VideoFormat& format) {
  return active_channel_ && active_channel_->SetSendStreamFormat(ssrc, format);
}

bool HybridVideoMediaChannel::SetSendRtpHeaderExtensions(
    const std::vector<RtpHeaderExtension>& extensions) {
  return active_channel_ &&
      active_channel_->SetSendRtpHeaderExtensions(extensions);
}

bool HybridVideoMediaChannel::SetSendBandwidth(bool autobw, int bps) {
  return active_channel_ &&
      active_channel_->SetSendBandwidth(autobw, bps);
}

bool HybridVideoMediaChannel::SetSend(bool send) {
  if (send == sending()) {
    return true;  // no action required if already set.
  }

  bool ret = active_channel_ &&
      active_channel_->SetSend(send);

  // Returns error and don't connect the signal if starting up.
  // Disconnects the signal anyway if shutting down.
  if (ret || !send) {
    // TODO(juberti): Remove this hack that connects the WebRTC channel
    // to the capturer.
    if (active_channel_ == channel1_.get()) {
      engine_->OnSendChange1(channel1_.get(), send);
    } else {
      engine_->OnSendChange2(channel2_.get(), send);
    }
    // If succeeded, remember the state as is.
    // If failed to open, sending_ should be false.
    // If failed to stop, sending_ should also be false, as we disconnect the
    // capture anyway.
    // The failure on SetSend(false) is a known issue in webrtc.
    sending_ = send;
  }
  return ret;
}

bool HybridVideoMediaChannel::SetCapturer(uint32 ssrc,
                                          VideoCapturer* capturer) {
  bool ret = true;
  if (channel1_.get()) {
    ret = channel1_->SetCapturer(ssrc, capturer);
  }
  if (channel2_.get() && ret) {
    ret = channel2_->SetCapturer(ssrc, capturer);
  }
  return ret;
}

bool HybridVideoMediaChannel::AddSendStream(const StreamParams& sp) {
  bool ret = true;
  if (channel1_) {
    ret = channel1_->AddSendStream(sp);
  }
  if (channel2_ && ret) {
    ret = channel2_->AddSendStream(sp);
  }
  return ret;
}

bool HybridVideoMediaChannel::RemoveSendStream(uint32 ssrc) {
  bool ret = true;
  if (channel1_) {
    ret = channel1_->RemoveSendStream(ssrc);
  }
  if (channel2_ && ret) {
    ret = channel2_->RemoveSendStream(ssrc);
  }
  return ret;
}

bool HybridVideoMediaChannel::AddRecvStream(const StreamParams& sp) {
  return active_channel_ &&
      active_channel_->AddRecvStream(sp);
}

bool HybridVideoMediaChannel::RemoveRecvStream(uint32 ssrc) {
  return active_channel_ &&
      active_channel_->RemoveRecvStream(ssrc);
}

bool HybridVideoMediaChannel::SendIntraFrame() {
  return active_channel_ &&
      active_channel_->SendIntraFrame();
}

bool HybridVideoMediaChannel::RequestIntraFrame() {
  return active_channel_ &&
      active_channel_->RequestIntraFrame();
}

bool HybridVideoMediaChannel::GetStats(VideoMediaInfo* info) {
  // TODO(juberti): Ensure that returning no stats until SetSendCodecs is OK.
  return active_channel_ &&
      active_channel_->GetStats(info);
}

void HybridVideoMediaChannel::OnPacketReceived(talk_base::Buffer* packet) {
  // Eat packets until we have an active channel;
  if (active_channel_) {
    active_channel_->OnPacketReceived(packet);
  } else {
    LOG(LS_INFO) << "HybridVideoChannel: Eating early RTP packet";
  }
}

void HybridVideoMediaChannel::OnRtcpReceived(talk_base::Buffer* packet) {
  // Eat packets until we have an active channel;
  if (active_channel_) {
    active_channel_->OnRtcpReceived(packet);
  } else {
    LOG(LS_INFO) << "HybridVideoChannel: Eating early RTCP packet";
  }
}

void HybridVideoMediaChannel::OnReadyToSend(bool ready) {
  if (channel1_) {
    channel1_->OnReadyToSend(ready);
  }
  if (channel2_) {
    channel2_->OnReadyToSend(ready);
  }
}

void HybridVideoMediaChannel::UpdateAspectRatio(int ratio_w, int ratio_h) {
  if (active_channel_) active_channel_->UpdateAspectRatio(ratio_w, ratio_h);
}

bool HybridVideoMediaChannel::SelectActiveChannel(
    const std::vector<VideoCodec>& codecs) {
  if (!active_channel_ && !codecs.empty()) {
    if (engine_->HasCodec1(codecs[0])) {
      channel2_.reset();
      active_channel_ = channel1_.get();
    } else if (engine_->HasCodec2(codecs[0])) {
      channel1_.reset();
      active_channel_ = channel2_.get();
    }
  }
  if (NULL == active_channel_) {
    return false;
  }
  // Connect signals from the active channel.
  active_channel_->SignalMediaError.connect(
      this,
      &HybridVideoMediaChannel::OnMediaError);
  return true;
}

void HybridVideoMediaChannel::SplitCodecs(
    const std::vector<VideoCodec>& codecs,
    std::vector<VideoCodec>* codecs1, std::vector<VideoCodec>* codecs2) {
  codecs1->clear();
  codecs2->clear();
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (engine_->HasCodec1(codecs[i])) {
      codecs1->push_back(codecs[i]);
    }
    if (engine_->HasCodec2(codecs[i])) {
      codecs2->push_back(codecs[i]);
    }
  }
}

void HybridVideoMediaChannel::OnMediaError(uint32 ssrc, Error error) {
  SignalMediaError(ssrc, error);
}

}  // namespace cricket
