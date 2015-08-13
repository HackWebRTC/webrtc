/*
 * libjingle
 * Copyright 2013 Google Inc.
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

#ifndef TALK_APP_WEBRTC_TEST_PEERCONNECTIONTESTWRAPPER_H_
#define TALK_APP_WEBRTC_TEST_PEERCONNECTIONTESTWRAPPER_H_

#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/test/fakeaudiocapturemodule.h"
#include "talk/app/webrtc/test/fakeconstraints.h"
#include "talk/app/webrtc/test/fakevideotrackrenderer.h"
#include "webrtc/base/sigslot.h"

namespace webrtc {
class DtlsIdentityStoreInterface;
class PortAllocatorFactoryInterface;
}

class PeerConnectionTestWrapper
    : public webrtc::PeerConnectionObserver,
      public webrtc::CreateSessionDescriptionObserver,
      public sigslot::has_slots<> {
 public:
  static void Connect(PeerConnectionTestWrapper* caller,
                      PeerConnectionTestWrapper* callee);

  explicit PeerConnectionTestWrapper(const std::string& name);
  virtual ~PeerConnectionTestWrapper();

  bool CreatePc(const webrtc::MediaConstraintsInterface* constraints);

  rtc::scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
      const std::string& label,
      const webrtc::DataChannelInit& init);

  // Implements PeerConnectionObserver.
  virtual void OnSignalingChange(
     webrtc::PeerConnectionInterface::SignalingState new_state) {}
  virtual void OnStateChange(
      webrtc::PeerConnectionObserver::StateType state_changed) {}
  virtual void OnAddStream(webrtc::MediaStreamInterface* stream);
  virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream) {}
  virtual void OnDataChannel(webrtc::DataChannelInterface* data_channel);
  virtual void OnRenegotiationNeeded() {}
  virtual void OnIceConnectionChange(
      webrtc::PeerConnectionInterface::IceConnectionState new_state) {}
  virtual void OnIceGatheringChange(
      webrtc::PeerConnectionInterface::IceGatheringState new_state) {}
  virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);
  virtual void OnIceComplete() {}

  // Implements CreateSessionDescriptionObserver.
  virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);
  virtual void OnFailure(const std::string& error) {}

  void CreateOffer(const webrtc::MediaConstraintsInterface* constraints);
  void CreateAnswer(const webrtc::MediaConstraintsInterface* constraints);
  void ReceiveOfferSdp(const std::string& sdp);
  void ReceiveAnswerSdp(const std::string& sdp);
  void AddIceCandidate(const std::string& sdp_mid, int sdp_mline_index,
                       const std::string& candidate);
  void WaitForCallEstablished();
  void WaitForConnection();
  void WaitForAudio();
  void WaitForVideo();
  void GetAndAddUserMedia(
    bool audio, const webrtc::FakeConstraints& audio_constraints,
    bool video, const webrtc::FakeConstraints& video_constraints);

  // sigslots
  sigslot::signal1<std::string*> SignalOnIceCandidateCreated;
  sigslot::signal3<const std::string&,
                   int,
                   const std::string&> SignalOnIceCandidateReady;
  sigslot::signal1<std::string*> SignalOnSdpCreated;
  sigslot::signal1<const std::string&> SignalOnSdpReady;
  sigslot::signal1<webrtc::DataChannelInterface*> SignalOnDataChannel;

 private:
  void SetLocalDescription(const std::string& type, const std::string& sdp);
  void SetRemoteDescription(const std::string& type, const std::string& sdp);
  bool CheckForConnection();
  bool CheckForAudio();
  bool CheckForVideo();
  rtc::scoped_refptr<webrtc::MediaStreamInterface> GetUserMedia(
      bool audio, const webrtc::FakeConstraints& audio_constraints,
      bool video, const webrtc::FakeConstraints& video_constraints);

  std::string name_;
  rtc::scoped_refptr<webrtc::PortAllocatorFactoryInterface>
      allocator_factory_;
  rtc::scoped_refptr<webrtc::PeerConnectionInterface> peer_connection_;
  rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>
      peer_connection_factory_;
  rtc::scoped_refptr<FakeAudioCaptureModule> fake_audio_capture_module_;
  rtc::scoped_ptr<webrtc::FakeVideoTrackRenderer> renderer_;
};

#endif  // TALK_APP_WEBRTC_TEST_PEERCONNECTIONTESTWRAPPER_H_
