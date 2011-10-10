/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "talk/examples/peerconnection_client/conductor.h"

#include <utility>

#include "modules/video_capture/main/source/video_capture_impl.h"
#include "talk/examples/peerconnection_client/defaults.h"
#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/p2p/client/basicportallocator.h"
#include "talk/session/phone/videorendererfactory.h"

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

  peer_connection_factory_  = webrtc::PeerConnectionManager::Create();

  if (!peer_connection_factory_.get()) {
    main_wnd_->MessageBox("Error",
        "Failed to initialize PeerConnectionFactory", true);
    DeletePeerConnection();
    return false;
  }

  peer_connection_ = peer_connection_factory_->CreatePeerConnection(
      GetPeerConnectionString(), this);

  if (!peer_connection_.get()) {
    main_wnd_->MessageBox("Error",
        "CreatePeerConnection failed", true);
    DeletePeerConnection();
  }
  return peer_connection_.get() != NULL;
}

void Conductor::DeletePeerConnection() {
  peer_connection_.release();
  active_streams_.clear();
  peer_connection_factory_.release();
  peer_id_ = -1;
}

void Conductor::EnsureStreamingUI() {
  ASSERT(peer_connection_.get() != NULL);
  if (main_wnd_->IsWindow()) {
    if (main_wnd_->current_ui() != MainWindow::STREAMING)
      main_wnd_->SwitchToStreamingUI();
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
void Conductor::OnAddStream(webrtc::MediaStream* stream) {
  LOG(INFO) << __FUNCTION__ << " " << stream->label();

  stream->AddRef();
  main_wnd_->QueueUIThreadCallback(NEW_STREAM_ADDED,
                                   stream);
}

void Conductor::OnRemoveStream(webrtc::MediaStream* stream) {
  LOG(INFO) << __FUNCTION__ << " " << stream->label();
  stream->AddRef();
  main_wnd_->QueueUIThreadCallback(STREAM_REMOVED,
                                   stream);
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

  peer_connection_->ProcessSignalingMessage(message);
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
    AddStreams();
  } else {
    main_wnd_->MessageBox("Error", "Failed to initialize PeerConnection", true);
  }
}

scoped_refptr<webrtc::VideoCaptureModule> Conductor::OpenVideoCaptureDevice() {
  webrtc::VideoCaptureModule::DeviceInfo* device_info(
      webrtc::videocapturemodule::VideoCaptureImpl::CreateDeviceInfo(0));
  scoped_refptr<webrtc::VideoCaptureModule> video_device;

  const size_t kMaxDeviceNameLength = 128;
  const size_t kMaxUniqueIdLength = 256;
  uint8 device_name[kMaxDeviceNameLength];
  uint8 unique_id[kMaxUniqueIdLength];

  const size_t device_count = device_info->NumberOfDevices();
  for (size_t i = 0; i < device_count; ++i) {
    // Get the name of the video capture device.
    device_info->GetDeviceName(i, device_name, kMaxDeviceNameLength, unique_id,
        kMaxUniqueIdLength);
    // Try to open this device.
    video_device =
        webrtc::videocapturemodule::VideoCaptureImpl::Create(0, unique_id);
    if (video_device.get())
      break;
  }
  webrtc::videocapturemodule::VideoCaptureImpl::DestroyDeviceInfo(device_info);
  return video_device;
}

void Conductor::AddStreams() {
  if (active_streams_.find(kStreamLabel) != active_streams_.end())
    return;  // Already added.

  scoped_refptr<webrtc::LocalAudioTrack> audio_track(
      webrtc::CreateLocalAudioTrack(kAudioLabel, NULL));

  scoped_refptr<webrtc::LocalVideoTrack> video_track(
      webrtc::CreateLocalVideoTrack(kVideoLabel, OpenVideoCaptureDevice()));

  scoped_refptr<webrtc::VideoRenderer> renderer(webrtc::CreateVideoRenderer(
      main_wnd_->local_renderer()));
  video_track->SetRenderer(renderer);

  scoped_refptr<webrtc::LocalMediaStream> stream =
      webrtc::CreateLocalMediaStream(kStreamLabel);

  stream->AddTrack(audio_track);
  stream->AddTrack(video_track);
  peer_connection_->AddStream(stream);
  peer_connection_->CommitStreamChanges();
  typedef std::pair<std::string, scoped_refptr<webrtc::MediaStream> >
      MediaStreamPair;
  active_streams_.insert(MediaStreamPair(stream->label(), stream));
  main_wnd_->SwitchToStreamingUI();
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
      if (msg) {
        // For convenience, we always run the message through the queue.
        // This way we can be sure that messages are sent to the server
        // in the same order they were signaled without much hassle.
        pending_messages_.push_back(msg);
      }

      if (!pending_messages_.empty() && !client_->IsSendingMessage()) {
        msg = pending_messages_.front();
        pending_messages_.pop_front();

        if (!client_->SendToPeer(peer_id_, *msg) && peer_id_ != -1) {
          LOG(LS_ERROR) << "SendToPeer failed";
          DisconnectFromServer();
        }
        delete msg;
      }

      if (!peer_connection_.get())
        peer_id_ = -1;

      break;
    }

    case PEER_CONNECTION_ADDSTREAMS:
      AddStreams();
      break;

    case PEER_CONNECTION_ERROR:
      main_wnd_->MessageBox("Error", "an unknown error occurred", true);
      break;

    case NEW_STREAM_ADDED: {
      webrtc::MediaStream* stream = reinterpret_cast<webrtc::MediaStream*>(
          data);
      scoped_refptr<webrtc::MediaStreamTrackList> tracks =
          stream->tracks();
      for (size_t i = 0; i < tracks->count(); ++i) {
        if (tracks->at(i)->kind().compare(webrtc::kVideoTrackKind) == 0) {
          webrtc::VideoTrack* track =
              reinterpret_cast<webrtc::VideoTrack*>(tracks->at(i).get());
          LOG(INFO) << "Setting video renderer for track: " << track->label();
          scoped_refptr<webrtc::VideoRenderer> renderer(
              webrtc::CreateVideoRenderer(main_wnd_->remote_renderer()));
          track->SetRenderer(renderer);
        }
      }
      // If we haven't shared any streams with this peer (we're the receiver)
      // then do so now.
      if (active_streams_.empty())
        AddStreams();
      stream->Release();
      break;
    }

    case STREAM_REMOVED: {
      webrtc::MediaStream* stream = reinterpret_cast<webrtc::MediaStream*>(
          data);
      active_streams_.erase(stream->label());
      stream->Release();
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
