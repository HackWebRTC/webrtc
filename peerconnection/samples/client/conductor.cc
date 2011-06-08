/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "peerconnection/samples/client/conductor.h"

#include "peerconnection/samples/client/defaults.h"
#include "talk/base/logging.h"
#include "talk/session/phone/videorendererfactory.h"

Conductor::Conductor(PeerConnectionClient* client, MainWnd* main_wnd)
  : handshake_(NONE),
    waiting_for_audio_(false),
    waiting_for_video_(false),
    peer_id_(-1),
    video_channel_(-1),
    audio_channel_(-1),
    client_(client),
    main_wnd_(main_wnd) {
  // Create a window for posting notifications back to from other threads.
  bool ok = Create(HWND_MESSAGE, L"Conductor", 0, 0, 0, 0, 0, 0);
  ASSERT(ok);
  client_->RegisterObserver(this);
  main_wnd->RegisterObserver(this);
}

Conductor::~Conductor() {
  ASSERT(peer_connection_.get() == NULL);
  Destroy();
  DeletePeerConnection();
}

bool Conductor::has_video() const {
  return video_channel_ != -1;
}

bool Conductor::has_audio() const {
  return audio_channel_ != -1;
}

bool Conductor::connection_active() const {
  return peer_connection_.get() != NULL;
}

void Conductor::Close() {
  if (peer_connection_.get()) {
    peer_connection_->Close();
  } else {
    client_->SignOut();
  }
}

bool Conductor::InitializePeerConnection() {
  ASSERT(peer_connection_.get() == NULL);
  peer_connection_.reset(new webrtc::PeerConnection(GetPeerConnectionString()));
  peer_connection_->RegisterObserver(this);
  if (!peer_connection_->Init()) {
    DeletePeerConnection();
  } else {
    bool audio = peer_connection_->SetAudioDevice("", "", 0);
    LOG(INFO) << "SetAudioDevice " << (audio ? "succeeded." : "failed.");
  }
  return peer_connection_.get() != NULL;
}

void Conductor::DeletePeerConnection() {
  peer_connection_.reset();
  local_renderer_.reset();
  remote_renderer_.reset();
  handshake_ = NONE;
}

void Conductor::StartCaptureDevice() {
  ASSERT(peer_connection_.get());
  if (main_wnd_->IsWindow()) {
    main_wnd_->SwitchToStreamingUI();

    if (peer_connection_->SetVideoCapture("")) {
      if (!local_renderer_.get()) {
        // The window will be resized according to the stream properties
        // when streaming starts.
        local_renderer_.reset(
            cricket::VideoRendererFactory::CreateGuiVideoRenderer(100, 100));
      }
      if (local_renderer_.get())
        peer_connection_->SetLocalVideoRenderer(local_renderer_.get());
    } else {
      ASSERT(false);
    }
  }
}

//
// PeerConnectionObserver implementation.
//

void Conductor::OnError() {
  LOG(INFO) << __FUNCTION__;
  ASSERT(false);
}

void Conductor::OnSignalingMessage(const std::string& msg) {
  LOG(INFO) << __FUNCTION__;

  bool shutting_down = (video_channel_ == -1 && audio_channel_ == -1);

  if (handshake_ == OFFER_RECEIVED && !shutting_down)
    StartCaptureDevice();

  // Send our answer/offer/shutting down message.
  // If we're the initiator, this will be our offer.  If we just received
  // an offer, this will be an answer.  If PeerConnection::Close has been
  // called, then this is our signal to the other end that we're shutting
  // down.
  if (handshake_ != QUIT_SENT) {
    SendMessage(handle(), SEND_MESSAGE_TO_PEER, 0,
                reinterpret_cast<LPARAM>(&msg));
  }

  if (shutting_down) {
    handshake_ = QUIT_SENT;
    PostMessage(handle(), PEER_CONNECTION_CLOSED, 0, 0);
  }
}

// Called when a remote stream is added
void Conductor::OnAddStream(const std::string& stream_id, int channel_id,
                            bool video) {
  LOG(INFO) << __FUNCTION__ << " " << stream_id;
  bool send_notification = (waiting_for_video_ || waiting_for_audio_);
  if (video) {
    ASSERT(video_channel_ == -1);
    video_channel_ = channel_id;
    waiting_for_video_ = false;
    LOG(INFO) << "Setting video renderer for channel: " << channel_id;
    if (!remote_renderer_.get()) {
      // The window size will be automatically corrected.
      remote_renderer_.reset(
          cricket::VideoRendererFactory::CreateGuiVideoRenderer(100, 100));
    }
    bool ok = peer_connection_->SetVideoRenderer(stream_id,
                                                 remote_renderer_.get());
    ASSERT(ok);
  } else {
    ASSERT(audio_channel_ == -1);
    audio_channel_ = channel_id;
    waiting_for_audio_ = false;
  }

  if (send_notification && !waiting_for_audio_ && !waiting_for_video_)
    PostMessage(handle(), MEDIA_CHANNELS_INITIALIZED, 0, 0);
}

void Conductor::OnRemoveStream(const std::string& stream_id, int channel_id,
                               bool video) {
  LOG(INFO) << __FUNCTION__;
  if (video) {
    ASSERT(channel_id == video_channel_);
    video_channel_ = -1;
  } else {
    ASSERT(channel_id == audio_channel_);
    audio_channel_ = -1;
  }
}

//
// PeerConnectionClientObserver implementation.
//

void Conductor::OnSignedIn() {
  LOG(INFO) << __FUNCTION__;
  main_wnd_->SwitchToPeerList(client_->peers());
}

void Conductor::OnDisconnected() {
  LOG(INFO) << __FUNCTION__;
  if (peer_connection_.get()) {
    peer_connection_->Close();
  } else if (main_wnd_->IsWindow()) {
    main_wnd_->SwitchToConnectUI();
  }
}

void Conductor::OnPeerConnected(int id, const std::string& name) {
  LOG(INFO) << __FUNCTION__;
  // Refresh the list if we're showing it.
  if (main_wnd_->current_ui() == MainWnd::LIST_PEERS)
    main_wnd_->SwitchToPeerList(client_->peers());
}

void Conductor::OnPeerDisconnected(int id, const std::string& name) {
  LOG(INFO) << __FUNCTION__;
  if (id == peer_id_) {
    LOG(INFO) << "Our peer disconnected";
    peer_id_ = -1;
    if (peer_connection_.get())
      peer_connection_->Close();
  }

  // Refresh the list if we're showing it.
  if (main_wnd_->current_ui() == MainWnd::LIST_PEERS)
    main_wnd_->SwitchToPeerList(client_->peers());
}

void Conductor::OnMessageFromPeer(int peer_id, const std::string& message) {
  ASSERT(peer_id_ == peer_id || peer_id_ == -1);

  if (handshake_ == NONE) {
    handshake_ = OFFER_RECEIVED;
    peer_id_ = peer_id;
    if (!peer_connection_.get()) {
      // Got an offer.  Give it to the PeerConnection instance.
      // Once processed, we will get a callback to OnSignalingMessage with
      // our 'answer' which we'll send to the peer.
      LOG(INFO) << "Got an offer from our peer: " << peer_id;
      if (!InitializePeerConnection()) {
        LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
        client_->SignOut();
        return;
      }
    }
  } else if (handshake_ == INITIATOR) {
    LOG(INFO) << "Remote peer sent us an answer";
    handshake_ = ANSWER_RECEIVED;
  } else {
    LOG(INFO) << "Remote peer is disconnecting";
    handshake_ = QUIT_SENT;
  }

  peer_connection_->SignalingMessage(message);

  if (handshake_ == QUIT_SENT) {
    DisconnectFromCurrentPeer();
  }
}

//
// MainWndCallback implementation.
//

void Conductor::StartLogin(const std::string& server, int port) {
  ASSERT(!client_->is_connected());
  if (!client_->Connect(server, port, GetPeerName())) {
    MessageBoxA(main_wnd_->handle(),
        ("Failed to connect to " + server).c_str(),
        "Error", MB_OK | MB_ICONERROR);
  }
}

void Conductor::DisconnectFromServer() {
  if (!client_->is_connected())
    return;
  client_->SignOut();
}

void Conductor::ConnectToPeer(int peer_id) {
  ASSERT(peer_id_ == -1);
  ASSERT(peer_id != -1);
  ASSERT(handshake_ == NONE);

  if (handshake_ != NONE)
    return;

  if (InitializePeerConnection()) {
    peer_id_ = peer_id;
    waiting_for_video_ = peer_connection_->AddStream(kVideoLabel, true);
    waiting_for_audio_ = peer_connection_->AddStream(kAudioLabel, false);
    if (waiting_for_video_ || waiting_for_audio_)
      handshake_ = INITIATOR;
    ASSERT(waiting_for_video_ || waiting_for_audio_);
  }

  if (handshake_ == NONE) {
    ::MessageBoxA(main_wnd_->handle(), "Failed to initialize PeerConnection",
                  "Error", MB_OK | MB_ICONERROR);
  }
}

void Conductor::DisconnectFromCurrentPeer() {
  if (peer_connection_.get())
    peer_connection_->Close();
}

//
// Win32Window implementation.
//

bool Conductor::OnMessage(UINT msg, WPARAM wp, LPARAM lp,
                          LRESULT& result) {  // NOLINT
  bool ret = true;
  if (msg == MEDIA_CHANNELS_INITIALIZED) {
      ASSERT(handshake_ == INITIATOR);
      bool ok = peer_connection_->Connect();
      ASSERT(ok);
      StartCaptureDevice();
      // When we get an OnSignalingMessage notification, we'll send our
      // json encoded signaling message to the peer, which is the first step
      // of establishing a connection.
  } else if (msg == PEER_CONNECTION_CLOSED) {
    LOG(INFO) << "PEER_CONNECTION_CLOSED";
    DeletePeerConnection();
    ::InvalidateRect(main_wnd_->handle(), NULL, TRUE);
    waiting_for_audio_ = false;
    waiting_for_video_ = false;
    peer_id_ = -1;
    ASSERT(video_channel_ == -1);
    ASSERT(audio_channel_ == -1);
    if (main_wnd_->IsWindow()) {
      if (client_->is_connected()) {
        main_wnd_->SwitchToPeerList(client_->peers());
      } else {
        main_wnd_->SwitchToConnectUI();
      }
    } else {
      DisconnectFromServer();
    }
  } else if (msg == SEND_MESSAGE_TO_PEER) {
    bool ok = client_->SendToPeer(peer_id_,
                                  *reinterpret_cast<std::string*>(lp));
    if (!ok) {
      LOG(LS_ERROR) << "SendToPeer failed";
      DisconnectFromServer();
    }
  } else {
    ret = false;
  }

  return ret;
}
