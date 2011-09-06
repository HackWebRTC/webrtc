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
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/p2p/client/basicportallocator.h"
#include "talk/session/phone/videorendererfactory.h"

namespace {
// Used when passing stream information from callback threads to the UI thread.
struct StreamInfo {
  StreamInfo(const std::string& id, bool video) : id_(id), video_(video) {}

  std::string id_;
  bool video_;
};
}  // end anonymous.

Conductor::Conductor(PeerConnectionClient* client, MainWindow* main_wnd)
  : peer_id_(-1),
    client_(client),
    main_wnd_(main_wnd) {
  client_->RegisterObserver(this);
  main_wnd->RegisterObserver(this);
}

Conductor::~Conductor() {
  ASSERT(peer_connection_.get() == NULL);
}

bool Conductor::connection_active() const {
  return peer_connection_.get() != NULL;
}

void Conductor::Close() {
  client_->SignOut();
  DeletePeerConnection();
}

bool Conductor::InitializePeerConnection() {
  ASSERT(peer_connection_factory_.get() == NULL);
  ASSERT(peer_connection_.get() == NULL);
  ASSERT(worker_thread_.get() == NULL);

  worker_thread_.reset(new talk_base::Thread());
  if (!worker_thread_->SetName("ConductorWT", this) ||
      !worker_thread_->Start()) {
    LOG(LS_ERROR) << "Failed to start libjingle worker thread";
    worker_thread_.reset();
    return false;
  }

  cricket::PortAllocator* port_allocator =
      new cricket::BasicPortAllocator(
          new talk_base::BasicNetworkManager(),
          talk_base::SocketAddress("stun.l.google.com", 19302),
          talk_base::SocketAddress(),
          talk_base::SocketAddress(),
          talk_base::SocketAddress());

  peer_connection_factory_.reset(
      new webrtc::PeerConnectionFactory(GetPeerConnectionString(),
                                        port_allocator,
                                        worker_thread_.get()));
  if (!peer_connection_factory_->Initialize()) {
    main_wnd_->MessageBox("Error",
        "Failed to initialize PeerConnectionFactory", true);
    DeletePeerConnection();
    return false;
  }

  // Since we only ever use a single PeerConnection instance, we share
  // the worker thread between the factory and the PC instance.
  peer_connection_.reset(peer_connection_factory_->CreatePeerConnection(
      worker_thread_.get()));
  if (!peer_connection_.get()) {
    main_wnd_->MessageBox("Error",
        "CreatePeerConnection failed", true);
    DeletePeerConnection();
  } else {
    peer_connection_->RegisterObserver(this);
    bool audio = peer_connection_->SetAudioDevice("", "", 0);
    LOG(INFO) << "SetAudioDevice " << (audio ? "succeeded." : "failed.");
  }
  return peer_connection_.get() != NULL;
}

void Conductor::DeletePeerConnection() {
  peer_connection_.reset();
  worker_thread_.reset();
  active_streams_.clear();
  peer_connection_factory_.reset();
  peer_id_ = -1;
}

void Conductor::StartCaptureDevice() {
  ASSERT(peer_connection_.get() != NULL);
  if (main_wnd_->IsWindow()) {
    if (main_wnd_->current_ui() != MainWindow::STREAMING)
      main_wnd_->SwitchToStreamingUI();

    if (peer_connection_->SetVideoCapture("")) {
      peer_connection_->SetLocalVideoRenderer(main_wnd_->local_renderer());
    }
  }
}

//
// PeerConnectionObserver implementation.
//

void Conductor::OnError() {
  LOG(LS_ERROR) << __FUNCTION__;
  main_wnd_->QueueUIThreadCallback(PEER_CONNECTION_ERROR, NULL);
}

void Conductor::OnSignalingMessage(const std::string& msg) {
  LOG(INFO) << __FUNCTION__;

  std::string* msg_copy = new std::string(msg);
  main_wnd_->QueueUIThreadCallback(SEND_MESSAGE_TO_PEER, msg_copy);
}

// Called when a remote stream is added
void Conductor::OnAddStream(const std::string& stream_id, bool video) {
  LOG(INFO) << __FUNCTION__ << " " << stream_id;

  main_wnd_->QueueUIThreadCallback(NEW_STREAM_ADDED,
                                   new StreamInfo(stream_id, video));
}

void Conductor::OnRemoveStream(const std::string& stream_id, bool video) {
  LOG(INFO) << __FUNCTION__ << (video ? " video: " : " audio: ") << stream_id;

  main_wnd_->QueueUIThreadCallback(STREAM_REMOVED,
                                   new StreamInfo(stream_id, video));
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

  DeletePeerConnection();

  if (main_wnd_->IsWindow())
    main_wnd_->SwitchToConnectUI();
}

void Conductor::OnPeerConnected(int id, const std::string& name) {
  LOG(INFO) << __FUNCTION__;
  // Refresh the list if we're showing it.
  if (main_wnd_->current_ui() == MainWindow::LIST_PEERS)
    main_wnd_->SwitchToPeerList(client_->peers());
}

void Conductor::OnPeerDisconnected(int id) {
  LOG(INFO) << __FUNCTION__;
  if (id == peer_id_) {
    LOG(INFO) << "Our peer disconnected";
    main_wnd_->QueueUIThreadCallback(PEER_CONNECTION_CLOSED, NULL);
  } else {
    // Refresh the list if we're showing it.
    if (main_wnd_->current_ui() == MainWindow::LIST_PEERS)
      main_wnd_->SwitchToPeerList(client_->peers());
  }
}

void Conductor::OnMessageFromPeer(int peer_id, const std::string& message) {
  ASSERT(peer_id_ == peer_id || peer_id_ == -1);
  ASSERT(!message.empty());

  if (!peer_connection_.get()) {
    ASSERT(peer_id_ == -1);
    peer_id_ = peer_id;

    // Got an offer.  Give it to the PeerConnection instance.
    // Once processed, we will get a callback to OnSignalingMessage with
    // our 'answer' which we'll send to the peer.
    LOG(INFO) << "Got an offer from our peer: " << peer_id;
    if (!InitializePeerConnection()) {
      LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
      client_->SignOut();
      return;
    }
  } else if (peer_id != peer_id_) {
    ASSERT(peer_id_ != -1);
    LOG(WARNING) << "Received an offer from a peer while already in a "
                    "conversation with a different peer.";
    return;
  }

  peer_connection_->SignalingMessage(message);
}

void Conductor::OnMessageSent(int err) {
  // Process the next pending message if any.
  main_wnd_->QueueUIThreadCallback(SEND_MESSAGE_TO_PEER, NULL);
}

//
// MainWndCallback implementation.
//

bool Conductor::StartLogin(const std::string& server, int port) {
  if (client_->is_connected())
    return false;

  if (!client_->Connect(server, port, GetPeerName())) {
    main_wnd_->MessageBox("Error", ("Failed to connect to " + server).c_str(),
                          true);
    return false;
  }

  return true;
}

void Conductor::DisconnectFromServer() {
  if (client_->is_connected())
    client_->SignOut();
}

void Conductor::ConnectToPeer(int peer_id) {
  ASSERT(peer_id_ == -1);
  ASSERT(peer_id != -1);

  if (peer_connection_.get()) {
    main_wnd_->MessageBox("Error",
        "We only support connecting to one peer at a time", true);
    return;
  }

  if (InitializePeerConnection()) {
    peer_id_ = peer_id;
    main_wnd_->SwitchToStreamingUI();
    StartCaptureDevice();
    AddStreams();
  } else {
    main_wnd_->MessageBox("Error", "Failed to initialize PeerConnection", true);
  }
}

bool Conductor::AddStream(const std::string& id, bool video) {
  // NOTE: Must be called from the UI thread.
  if (active_streams_.find(id) != active_streams_.end())
    return false;  // Already added.

  active_streams_.insert(id);
  bool ret = peer_connection_->AddStream(id, video);
  if (!ret) {
    active_streams_.erase(id);
  } else if (video) {
    LOG(INFO) << "Setting video renderer for stream: " << id;
    bool ok = peer_connection_->SetVideoRenderer(id,
        main_wnd_->remote_renderer());
    ASSERT(ok);
    UNUSED(ok);
  }
  return ret;
}

void Conductor::AddStreams() {
  int streams = 0;
  if (AddStream(kVideoLabel, true))
    ++streams;

  if (AddStream(kAudioLabel, false))
    ++streams;

  // At the initiator of the call, after adding streams we need
  // kick start the ICE candidates discovery process, which
  // is done by the Connect method. Earlier this was done after
  // getting the OnLocalStreamInitialized callback which is removed
  // now. Connect will trigger OnSignalingMessage callback when
  // ICE candidates are available.
  if (streams)
    peer_connection_->Connect();
}

void Conductor::DisconnectFromCurrentPeer() {
  LOG(INFO) << __FUNCTION__;
  if (peer_connection_.get()) {
    client_->SendHangUp(peer_id_);
    DeletePeerConnection();
  }

  if (main_wnd_->IsWindow())
    main_wnd_->SwitchToPeerList(client_->peers());
}

void Conductor::UIThreadCallback(int msg_id, void* data) {
  switch (msg_id) {
    case PEER_CONNECTION_CLOSED:
      LOG(INFO) << "PEER_CONNECTION_CLOSED";
      DeletePeerConnection();

      ASSERT(active_streams_.empty());

      if (main_wnd_->IsWindow()) {
        if (client_->is_connected()) {
          main_wnd_->SwitchToPeerList(client_->peers());
        } else {
          main_wnd_->SwitchToConnectUI();
        }
      } else {
        DisconnectFromServer();
      }
      break;

    case SEND_MESSAGE_TO_PEER: {
      LOG(INFO) << "SEND_MESSAGE_TO_PEER";
      std::string* msg = reinterpret_cast<std::string*>(data);
      if (client_->IsSendingMessage()) {
        ASSERT(msg != NULL);
        pending_messages_.push_back(msg);
      } else {
        if (!msg && !pending_messages_.empty()) {
          msg = pending_messages_.front();
          pending_messages_.pop_front();
        }
        if (msg) {
          bool ok = client_->SendToPeer(peer_id_, *msg);
          if (!ok && peer_id_ != -1) {
            LOG(LS_ERROR) << "SendToPeer failed";
            DisconnectFromServer();
          }
          delete msg;
        }

        if (!peer_connection_.get())
          peer_id_ = -1;
      }
      break;
    }

    case PEER_CONNECTION_ADDSTREAMS:
      AddStreams();
      break;

    case PEER_CONNECTION_ERROR:
      main_wnd_->MessageBox("Error", "an unknown error occurred", true);
      break;

    case NEW_STREAM_ADDED: {
      talk_base::scoped_ptr<StreamInfo> info(
          reinterpret_cast<StreamInfo*>(data));
      if (info->video_) {
        LOG(INFO) << "Setting video renderer for stream: " << info->id_;
        bool ok = peer_connection_->SetVideoRenderer(info->id_,
            main_wnd_->remote_renderer());
        ASSERT(ok);
        if (!ok)
          LOG(LS_ERROR) << "SetVideoRenderer failed for : " << info->id_;

        // TODO(tommi): For the initiator, we shouldn't have to make this call
        // here (which is actually the second time this is called for the
        // initiator).  Look into why this is needed.
        StartCaptureDevice();
      }

      // If we haven't shared any streams with this peer (we're the receiver)
      // then do so now.
      if (active_streams_.empty())
        AddStreams();
      break;
    }

    case STREAM_REMOVED: {
      talk_base::scoped_ptr<StreamInfo> info(
          reinterpret_cast<StreamInfo*>(data));
      active_streams_.erase(info->id_);
      if (active_streams_.empty()) {
        LOG(INFO) << "All streams have been closed.";
        main_wnd_->QueueUIThreadCallback(PEER_CONNECTION_CLOSED, NULL);
      }
      break;
    }

    default:
      ASSERT(false);
      break;
  }
}
