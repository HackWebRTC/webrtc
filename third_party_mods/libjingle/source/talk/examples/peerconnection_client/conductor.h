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

#include <deque>
#include <map>
#include <set>
#include <string>

#include "talk/examples/peerconnection_client/main_wnd.h"
#include "talk/examples/peerconnection_client/peer_connection_client.h"
#include "talk/app/webrtc_dev/mediastream.h"
#include "talk/app/webrtc_dev/peerconnection.h"
#include "talk/base/scoped_ptr.h"

namespace webrtc {
class VideoCaptureModule;
}  // namespace webrtc

namespace cricket {
class VideoRenderer;
}  // namespace cricket

class Conductor
  : public webrtc::PeerConnectionObserver,
    public PeerConnectionClientObserver,
    public MainWndCallback {
 public:
  enum CallbackID {
    MEDIA_CHANNELS_INITIALIZED = 1,
    PEER_CONNECTION_CLOSED,
    SEND_MESSAGE_TO_PEER,
    PEER_CONNECTION_ADDSTREAMS,
    PEER_CONNECTION_ERROR,
    NEW_STREAM_ADDED,
    STREAM_REMOVED,
  };

  Conductor(PeerConnectionClient* client, MainWindow* main_wnd);
  ~Conductor();

  bool connection_active() const;

  virtual void Close();

 protected:
  bool InitializePeerConnection();
  void DeletePeerConnection();
  void EnsureStreamingUI();
  void AddStreams();
  scoped_refptr<webrtc::VideoCaptureModule> OpenVideoCaptureDevice();

  //
  // PeerConnectionObserver implementation.
  //
  virtual void OnError();
  virtual void OnMessage(const std::string& msg) {}
  virtual void OnSignalingMessage(const std::string& msg);
  virtual void OnStateChange(Readiness state) {}
  virtual void OnAddStream(webrtc::MediaStream* stream);
  virtual void OnRemoveStream(webrtc::MediaStream* stream);


  //
  // PeerConnectionClientObserver implementation.
  //

  virtual void OnSignedIn();

  virtual void OnDisconnected();

  virtual void OnPeerConnected(int id, const std::string& name);

  virtual void OnPeerDisconnected(int id);

  virtual void OnMessageFromPeer(int peer_id, const std::string& message);

  virtual void OnMessageSent(int err);

  //
  // MainWndCallback implementation.
  //

  virtual bool StartLogin(const std::string& server, int port);

  virtual void DisconnectFromServer();

  virtual void ConnectToPeer(int peer_id);

  virtual void DisconnectFromCurrentPeer();

  virtual void UIThreadCallback(int msg_id, void* data);

 protected:
  int peer_id_;
  scoped_refptr<webrtc::PeerConnection> peer_connection_;
  scoped_refptr<webrtc::PeerConnectionManager> peer_connection_factory_;
  PeerConnectionClient* client_;
  MainWindow* main_wnd_;
  std::deque<std::string*> pending_messages_;
  std::map<std::string, scoped_refptr<webrtc::MediaStream> > active_streams_;
};

#endif  // PEERCONNECTION_SAMPLES_CLIENT_CONDUCTOR_H_
