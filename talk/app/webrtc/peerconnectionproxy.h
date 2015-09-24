/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#ifndef TALK_APP_WEBRTC_PEERCONNECTIONPROXY_H_
#define TALK_APP_WEBRTC_PEERCONNECTIONPROXY_H_

#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/proxy.h"

namespace webrtc {

// Define proxy for PeerConnectionInterface.
BEGIN_PROXY_MAP(PeerConnection)
  PROXY_METHOD0(rtc::scoped_refptr<StreamCollectionInterface>,
                local_streams)
  PROXY_METHOD0(rtc::scoped_refptr<StreamCollectionInterface>,
                remote_streams)
  PROXY_METHOD1(bool, AddStream, MediaStreamInterface*)
  PROXY_METHOD1(void, RemoveStream, MediaStreamInterface*)
  PROXY_METHOD1(rtc::scoped_refptr<DtmfSenderInterface>,
                CreateDtmfSender, AudioTrackInterface*)
  PROXY_METHOD3(bool, GetStats, StatsObserver*,
                MediaStreamTrackInterface*,
                StatsOutputLevel)
  PROXY_METHOD2(rtc::scoped_refptr<DataChannelInterface>,
                CreateDataChannel, const std::string&, const DataChannelInit*)
  PROXY_CONSTMETHOD0(const SessionDescriptionInterface*, local_description)
  PROXY_CONSTMETHOD0(const SessionDescriptionInterface*, remote_description)
  PROXY_METHOD2(void, CreateOffer, CreateSessionDescriptionObserver*,
                const MediaConstraintsInterface*)
  PROXY_METHOD2(void, CreateAnswer, CreateSessionDescriptionObserver*,
                const MediaConstraintsInterface*)
  PROXY_METHOD2(void, SetLocalDescription, SetSessionDescriptionObserver*,
                SessionDescriptionInterface*)
  PROXY_METHOD2(void, SetRemoteDescription, SetSessionDescriptionObserver*,
                SessionDescriptionInterface*)
  PROXY_METHOD2(bool, UpdateIce, const IceServers&,
                const MediaConstraintsInterface*)
  PROXY_METHOD1(bool, AddIceCandidate, const IceCandidateInterface*)
  PROXY_METHOD1(void, RegisterUMAObserver, UMAObserver*)
  PROXY_METHOD0(SignalingState, signaling_state)
  PROXY_METHOD0(IceState, ice_state)
  PROXY_METHOD0(IceConnectionState, ice_connection_state)
  PROXY_METHOD0(IceGatheringState, ice_gathering_state)
  PROXY_METHOD0(void, Close)
END_PROXY()

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTIONPROXY_H_
