/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
#define PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
#pragma once

#include <string>

#include "peerconnection/samples/client/main_wnd.h"
#include "peerconnection/samples/client/peer_connection_client.h"
#include "talk/app/peerconnection.h"
#include "talk/base/scoped_ptr.h"

namespace cricket {
class VideoRenderer;
}  // namespace cricket

class Conductor
  : public webrtc::PeerConnectionObserver,
    public PeerConnectionClientObserver,
    public MainWndCallback,
    public talk_base::Win32Window {
 public:
  enum WindowMessages {
    MEDIA_CHANNELS_INITIALIZED = WM_APP + 1,
    PEER_CONNECTION_CLOSED,
    SEND_MESSAGE_TO_PEER,
  };

  enum HandshakeState {
    NONE,
    INITIATOR,
    ANSWER_RECEIVED,
    OFFER_RECEIVED,
    QUIT_SENT,
  };

  Conductor(PeerConnectionClient* client, MainWnd* main_wnd);
  ~Conductor();

  bool has_video() const;
  bool has_audio() const;
  bool connection_active() const;

  void Close();

 protected:
  bool InitializePeerConnection();
  void DeletePeerConnection();
  void StartCaptureDevice();

  //
  // PeerConnectionObserver implementation.
  //

  virtual void OnError();
  virtual void OnSignalingMessage(const std::string& msg);

  // Called when a remote stream is added
  virtual void OnAddStream(const std::string& stream_id, int channel_id,
                           bool video);

  virtual void OnRemoveStream(const std::string& stream_id,
                              int channel_id,
                              bool video);

  //
  // PeerConnectionClientObserver implementation.
  //

  virtual void OnSignedIn();

  virtual void OnDisconnected();

  virtual void OnPeerConnected(int id, const std::string& name);

  virtual void OnPeerDisconnected(int id, const std::string& name);

  virtual void OnMessageFromPeer(int peer_id, const std::string& message);

  //
  // MainWndCallback implementation.
  //

  virtual void StartLogin(const std::string& server, int port);

  virtual void DisconnectFromServer();

  virtual void ConnectToPeer(int peer_id);

  virtual void DisconnectFromCurrentPeer();

  //
  // Win32Window implementation.
  //

  virtual bool OnMessage(UINT msg, WPARAM wp, LPARAM lp,
                         LRESULT& result); // NOLINT

 protected:
  HandshakeState handshake_;
  bool waiting_for_audio_;
  bool waiting_for_video_;
  int peer_id_;
  talk_base::scoped_ptr<webrtc::PeerConnection> peer_connection_;
  PeerConnectionClient* client_;
  MainWnd* main_wnd_;
  int video_channel_;
  int audio_channel_;
  talk_base::scoped_ptr<cricket::VideoRenderer> local_renderer_;
  talk_base::scoped_ptr<cricket::VideoRenderer> remote_renderer_;
};

#endif  // PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
