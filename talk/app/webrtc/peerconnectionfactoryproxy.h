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

#ifndef TALK_APP_WEBRTC_PEERCONNECTIONFACTORYPROXY_H_
#define TALK_APP_WEBRTC_PEERCONNECTIONFACTORYPROXY_H_

#include <string>

#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/proxy.h"
#include "webrtc/base/bind.h"

namespace webrtc {

BEGIN_PROXY_MAP(PeerConnectionFactory)
  PROXY_METHOD1(void, SetOptions, const Options&)
  // Can't use PROXY_METHOD5 because scoped_ptr must be Pass()ed.
  // TODO(tommi,hbos): Use of templates to support scoped_ptr?
  rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection(
      const PeerConnectionInterface::RTCConfiguration& a1,
      const MediaConstraintsInterface* a2,
      PortAllocatorFactoryInterface* a3,
      rtc::scoped_ptr<DtlsIdentityStoreInterface> a4,
      PeerConnectionObserver* a5) override {
    return owner_thread_->Invoke<rtc::scoped_refptr<PeerConnectionInterface>>(
        rtc::Bind(&PeerConnectionFactoryProxy::CreatePeerConnection_ot, this,
                  a1, a2, a3, a4.release(), a5));
  }
  PROXY_METHOD1(rtc::scoped_refptr<MediaStreamInterface>,
                CreateLocalMediaStream, const std::string&)
  PROXY_METHOD1(rtc::scoped_refptr<AudioSourceInterface>,
                CreateAudioSource, const MediaConstraintsInterface*)
  PROXY_METHOD2(rtc::scoped_refptr<VideoSourceInterface>,
                CreateVideoSource, cricket::VideoCapturer*,
                const MediaConstraintsInterface*)
  PROXY_METHOD2(rtc::scoped_refptr<VideoTrackInterface>,
                CreateVideoTrack, const std::string&,  VideoSourceInterface*)
  PROXY_METHOD2(rtc::scoped_refptr<AudioTrackInterface>,
                CreateAudioTrack, const std::string&,  AudioSourceInterface*)
  PROXY_METHOD1(bool, StartAecDump, rtc::PlatformFile)
  PROXY_METHOD0(void, StopAecDump)
  PROXY_METHOD1(bool, StartRtcEventLog, rtc::PlatformFile)
  PROXY_METHOD0(void, StopRtcEventLog)

 private:
  rtc::scoped_refptr<PeerConnectionInterface> CreatePeerConnection_ot(
      const PeerConnectionInterface::RTCConfiguration& a1,
      const MediaConstraintsInterface* a2,
      PortAllocatorFactoryInterface* a3,
      DtlsIdentityStoreInterface* a4,
      PeerConnectionObserver* a5) {
    rtc::scoped_ptr<DtlsIdentityStoreInterface> ptr_a4(a4);
    return c_->CreatePeerConnection(a1, a2, a3, ptr_a4.Pass(), a5);
  }
END_PROXY()

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTIONFACTORYPROXY_H_
