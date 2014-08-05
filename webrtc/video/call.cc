/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <string.h>

#include <map>
#include <vector>

#include "webrtc/call.h"
#include "webrtc/common.h"
#include "webrtc/config.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_header_parser.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/thread_annotations.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/video/video_receive_stream.h"
#include "webrtc/video/video_send_stream.h"
#include "webrtc/video_engine/include/vie_base.h"
#include "webrtc/video_engine/include/vie_codec.h"
#include "webrtc/video_engine/include/vie_rtp_rtcp.h"

namespace webrtc {
const char* RtpExtension::kTOffset = "urn:ietf:params:rtp-hdrext:toffset";
const char* RtpExtension::kAbsSendTime =
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";

bool RtpExtension::IsSupported(const std::string& name) {
  return name == webrtc::RtpExtension::kTOffset ||
         name == webrtc::RtpExtension::kAbsSendTime;
}

namespace internal {

class CpuOveruseObserverProxy : public webrtc::CpuOveruseObserver {
 public:
  explicit CpuOveruseObserverProxy(OveruseCallback* overuse_callback)
      : crit_(CriticalSectionWrapper::CreateCriticalSection()),
        overuse_callback_(overuse_callback) {
    assert(overuse_callback != NULL);
  }

  virtual ~CpuOveruseObserverProxy() {}

  virtual void OveruseDetected() OVERRIDE {
    CriticalSectionScoped lock(crit_.get());
    overuse_callback_->OnOveruse();
  }

  virtual void NormalUsage() OVERRIDE {
    CriticalSectionScoped lock(crit_.get());
    overuse_callback_->OnNormalUse();
  }

 private:
  const scoped_ptr<CriticalSectionWrapper> crit_;
  OveruseCallback* overuse_callback_ GUARDED_BY(crit_);
};

class Call : public webrtc::Call, public PacketReceiver {
 public:
  Call(webrtc::VideoEngine* video_engine, const Call::Config& config);
  virtual ~Call();

  virtual PacketReceiver* Receiver() OVERRIDE;

  virtual VideoSendStream* CreateVideoSendStream(
      const VideoSendStream::Config& config,
      const std::vector<VideoStream>& video_streams,
      const void* encoder_settings) OVERRIDE;

  virtual void DestroyVideoSendStream(webrtc::VideoSendStream* send_stream)
      OVERRIDE;

  virtual VideoReceiveStream* CreateVideoReceiveStream(
      const VideoReceiveStream::Config& config) OVERRIDE;

  virtual void DestroyVideoReceiveStream(
      webrtc::VideoReceiveStream* receive_stream) OVERRIDE;

  virtual uint32_t SendBitrateEstimate() OVERRIDE;
  virtual uint32_t ReceiveBitrateEstimate() OVERRIDE;

  virtual DeliveryStatus DeliverPacket(const uint8_t* packet,
                                       size_t length) OVERRIDE;

 private:
  DeliveryStatus DeliverRtcp(const uint8_t* packet, size_t length);
  DeliveryStatus DeliverRtp(const uint8_t* packet, size_t length);

  Call::Config config_;

  std::map<uint32_t, VideoReceiveStream*> receive_ssrcs_
      GUARDED_BY(receive_lock_);
  scoped_ptr<RWLockWrapper> receive_lock_;

  std::map<uint32_t, VideoSendStream*> send_ssrcs_ GUARDED_BY(send_lock_);
  scoped_ptr<RWLockWrapper> send_lock_;

  scoped_ptr<CpuOveruseObserverProxy> overuse_observer_proxy_;

  VideoSendStream::RtpStateMap suspended_send_ssrcs_;

  VideoEngine* video_engine_;
  ViERTP_RTCP* rtp_rtcp_;
  ViECodec* codec_;
  ViEBase* base_;
  int base_channel_id_;

  DISALLOW_COPY_AND_ASSIGN(Call);
};
}  // namespace internal

Call* Call::Create(const Call::Config& config) {
  VideoEngine* video_engine = config.webrtc_config != NULL
                                  ? VideoEngine::Create(*config.webrtc_config)
                                  : VideoEngine::Create();
  assert(video_engine != NULL);

  return new internal::Call(video_engine, config);
}

namespace internal {

const int kDefaultVideoStreamBitrateBps = 300000;

Call::Call(webrtc::VideoEngine* video_engine, const Call::Config& config)
    : config_(config),
      receive_lock_(RWLockWrapper::CreateRWLock()),
      send_lock_(RWLockWrapper::CreateRWLock()),
      video_engine_(video_engine),
      base_channel_id_(-1) {
  assert(video_engine != NULL);
  assert(config.send_transport != NULL);

  if (config.overuse_callback) {
    overuse_observer_proxy_.reset(
        new CpuOveruseObserverProxy(config.overuse_callback));
  }

  rtp_rtcp_ = ViERTP_RTCP::GetInterface(video_engine_);
  assert(rtp_rtcp_ != NULL);

  codec_ = ViECodec::GetInterface(video_engine_);
  assert(codec_ != NULL);

  // As a workaround for non-existing calls in the old API, create a base
  // channel used as default channel when creating send and receive streams.
  base_ = ViEBase::GetInterface(video_engine_);
  assert(base_ != NULL);

  base_->CreateChannel(base_channel_id_);
  assert(base_channel_id_ != -1);
}

Call::~Call() {
  base_->DeleteChannel(base_channel_id_);
  base_->Release();
  codec_->Release();
  rtp_rtcp_->Release();
  webrtc::VideoEngine::Delete(video_engine_);
}

PacketReceiver* Call::Receiver() { return this; }

VideoSendStream* Call::CreateVideoSendStream(
    const VideoSendStream::Config& config,
    const std::vector<VideoStream>& video_streams,
    const void* encoder_settings) {
  assert(config.rtp.ssrcs.size() > 0);

  // TODO(mflodman): Base the start bitrate on a current bandwidth estimate, if
  // the call has already started.
  VideoSendStream* send_stream = new VideoSendStream(
      config_.send_transport,
      overuse_observer_proxy_.get(),
      video_engine_,
      config,
      video_streams,
      encoder_settings,
      suspended_send_ssrcs_,
      base_channel_id_,
      config_.start_bitrate_bps != -1 ? config_.start_bitrate_bps
                                      : kDefaultVideoStreamBitrateBps);

  WriteLockScoped write_lock(*send_lock_);
  for (size_t i = 0; i < config.rtp.ssrcs.size(); ++i) {
    assert(send_ssrcs_.find(config.rtp.ssrcs[i]) == send_ssrcs_.end());
    send_ssrcs_[config.rtp.ssrcs[i]] = send_stream;
  }
  return send_stream;
}

void Call::DestroyVideoSendStream(webrtc::VideoSendStream* send_stream) {
  assert(send_stream != NULL);

  send_stream->Stop();

  VideoSendStream* send_stream_impl = NULL;
  {
    WriteLockScoped write_lock(*send_lock_);
    std::map<uint32_t, VideoSendStream*>::iterator it = send_ssrcs_.begin();
    while (it != send_ssrcs_.end()) {
      if (it->second == static_cast<VideoSendStream*>(send_stream)) {
        send_stream_impl = it->second;
        send_ssrcs_.erase(it++);
      } else {
        ++it;
      }
    }
  }

  VideoSendStream::RtpStateMap rtp_state = send_stream_impl->GetRtpStates();

  for (VideoSendStream::RtpStateMap::iterator it = rtp_state.begin();
       it != rtp_state.end();
       ++it) {
    suspended_send_ssrcs_[it->first] = it->second;
  }

  assert(send_stream_impl != NULL);
  delete send_stream_impl;
}

VideoReceiveStream* Call::CreateVideoReceiveStream(
    const VideoReceiveStream::Config& config) {
  VideoReceiveStream* receive_stream =
      new VideoReceiveStream(video_engine_,
                             config,
                             config_.send_transport,
                             config_.voice_engine,
                             base_channel_id_);

  WriteLockScoped write_lock(*receive_lock_);
  assert(receive_ssrcs_.find(config.rtp.remote_ssrc) == receive_ssrcs_.end());
  receive_ssrcs_[config.rtp.remote_ssrc] = receive_stream;
  // TODO(pbos): Configure different RTX payloads per receive payload.
  VideoReceiveStream::Config::Rtp::RtxMap::const_iterator it =
      config.rtp.rtx.begin();
  if (it != config.rtp.rtx.end())
    receive_ssrcs_[it->second.ssrc] = receive_stream;

  return receive_stream;
}

void Call::DestroyVideoReceiveStream(
    webrtc::VideoReceiveStream* receive_stream) {
  assert(receive_stream != NULL);

  VideoReceiveStream* receive_stream_impl = NULL;
  {
    WriteLockScoped write_lock(*receive_lock_);
    // Remove all ssrcs pointing to a receive stream. As RTX retransmits on a
    // separate SSRC there can be either one or two.
    std::map<uint32_t, VideoReceiveStream*>::iterator it =
        receive_ssrcs_.begin();
    while (it != receive_ssrcs_.end()) {
      if (it->second == static_cast<VideoReceiveStream*>(receive_stream)) {
        assert(receive_stream_impl == NULL ||
            receive_stream_impl == it->second);
        receive_stream_impl = it->second;
        receive_ssrcs_.erase(it++);
      } else {
        ++it;
      }
    }
  }

  assert(receive_stream_impl != NULL);
  delete receive_stream_impl;
}

uint32_t Call::SendBitrateEstimate() {
  // TODO(pbos): Return send-bitrate estimate
  return 0;
}

uint32_t Call::ReceiveBitrateEstimate() {
  // TODO(pbos): Return receive-bitrate estimate
  return 0;
}

PacketReceiver::DeliveryStatus Call::DeliverRtcp(const uint8_t* packet,
                                                       size_t length) {
  // TODO(pbos): Figure out what channel needs it actually.
  //             Do NOT broadcast! Also make sure it's a valid packet.
  //             Return DELIVERY_UNKNOWN_SSRC if it can be determined that
  //             there's no receiver of the packet.
  bool rtcp_delivered = false;
  {
    ReadLockScoped read_lock(*receive_lock_);
    for (std::map<uint32_t, VideoReceiveStream*>::iterator it =
             receive_ssrcs_.begin();
         it != receive_ssrcs_.end();
         ++it) {
      if (it->second->DeliverRtcp(packet, length))
        rtcp_delivered = true;
    }
  }

  {
    ReadLockScoped read_lock(*send_lock_);
    for (std::map<uint32_t, VideoSendStream*>::iterator it =
             send_ssrcs_.begin();
         it != send_ssrcs_.end();
         ++it) {
      if (it->second->DeliverRtcp(packet, length))
        rtcp_delivered = true;
    }
  }
  return rtcp_delivered ? DELIVERY_OK : DELIVERY_PACKET_ERROR;
}

PacketReceiver::DeliveryStatus Call::DeliverRtp(const uint8_t* packet,
                                                size_t length) {
  // Minimum RTP header size.
  if (length < 12)
    return DELIVERY_PACKET_ERROR;

  const uint8_t* ptr = &packet[8];
  uint32_t ssrc = ptr[0] << 24 | ptr[1] << 16 | ptr[2] << 8 | ptr[3];

  ReadLockScoped read_lock(*receive_lock_);
  std::map<uint32_t, VideoReceiveStream*>::iterator it =
      receive_ssrcs_.find(ssrc);

  if (it == receive_ssrcs_.end())
    return DELIVERY_UNKNOWN_SSRC;

  return it->second->DeliverRtp(packet, length) ? DELIVERY_OK
                                                : DELIVERY_PACKET_ERROR;
}

PacketReceiver::DeliveryStatus Call::DeliverPacket(const uint8_t* packet,
                                                   size_t length) {
  if (RtpHeaderParser::IsRtcp(packet, length))
    return DeliverRtcp(packet, length);

  return DeliverRtp(packet, length);
}

}  // namespace internal
}  // namespace webrtc
