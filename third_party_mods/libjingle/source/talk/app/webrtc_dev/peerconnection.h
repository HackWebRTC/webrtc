/*
 * libjingle
 * Copyright 2011, Google Inc.
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

// This file contains the PeerConnection interface as defined in
// http://dev.w3.org/2011/webrtc/editor/webrtc.html#peer-to-peer-connections.
// Applications must use this interface to implement peerconnection.
// PeerConnectionFactory class provides factory methods to create
// peerconnection, mediastream and media tracks objects.
//
// The Following steps are needed to setup a typical call.
// 1. Create a PeerConnectionManager. Check constructors for more information
// about input parameters.
// 2. Create a PeerConnection object. Provide a configuration string which
// points either to stun or turn server to generate ICE candidates and provide
// an object that implements the PeerConnectionObserver interface.
// Now PeerConnection will startcollecting ICE candidates.
// 3. Create local MediaStream and MediaTracks using the PeerConnectionFactory
// and add it to PeerConnection by calling AddStream.
// 4. Once all mediastreams are added to peerconnection, call
// CommitStreamChanges. Now PeerConnection starts generating an offer based on
// the local mediastreams.
// 5. When PeerConnection have generated the ICE candidates it will call the
// observer OnSignalingMessage callback with the initial offer.
// 6. When an Answer from peer received it must be supplied to the
// PeerConnection by calling ProcessSignalingMessage.
// At this point PeerConnection knows remote capabilities and ICE candidates.
// Media will start flowing to the remote peer.

// The Receiver of a call can decide to accept or reject the call.
// This decision will be taken by the application not peerconnection.
// If application decides to accept the call
// 1. Create PeerConnectionManager if it doesn't exist.
// 2. Create new PeerConnection
// 3. Provide the remote offer to the new PeerConnection object by calling
// ProcessSignalingMessage.
// 4. PeerConnection will call the observer function OnAddStream with remote
// MediaStream and tracks information.
// 5. PeerConnection will call the observer function OnSignalingMessage with
// local ICE candidates in a answer message.
// 6. Application can add it's own MediaStreams by calling AddStream.
// When all streams have been added the application must call
// CommitStreamChanges. Streams can be added at any time after the
// PeerConnection object have been created.

#ifndef TALK_APP_WEBRTC_PEERCONNECTION_H_
#define TALK_APP_WEBRTC_PEERCONNECTION_H_

#include <string>

#include "talk/app/webrtc_dev/mediastream.h"

namespace talk_base {
  class Thread;
  class NetworkManager;
  class PacketSocketFactory;
}

namespace webrtc {
// MediaStream container interface.
class StreamCollectionInterface : public talk_base::RefCountInterface {
 public:
  virtual size_t count() = 0;
  virtual MediaStreamInterface* at(size_t index) = 0;
  virtual MediaStreamInterface* find(const std::string& label) = 0;
 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~StreamCollectionInterface() {}
};

// PeerConnection callback interface. Application should implement these
// methods.
class PeerConnectionObserver {
 public:
  enum Readiness {
    kNegotiating,
    kActive,
  };

  virtual void OnError() = 0;

  virtual void OnMessage(const std::string& msg) = 0;

  // Serialized signaling message
  virtual void OnSignalingMessage(const std::string& msg) = 0;

  virtual void OnStateChange(Readiness state) = 0;

  // Triggered when media is received on a new stream from remote peer.
  virtual void OnAddStream(MediaStreamInterface* stream) = 0;

  // Triggered when a remote peer close a stream.
  virtual void OnRemoveStream(MediaStreamInterface* stream) = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionObserver() {}
};


class PeerConnectionInterface : public talk_base::RefCountInterface {
 public:
  // SignalingMessage in json format
  virtual bool ProcessSignalingMessage(const std::string& msg) = 0;

  // Sends the msg over a data stream.
  virtual bool Send(const std::string& msg) = 0;

  // Accessor methods to active local streams.
  virtual talk_base::scoped_refptr<StreamCollectionInterface>
      local_streams() = 0;

  // Accessor methods to remote streams.
  virtual talk_base::scoped_refptr<StreamCollectionInterface>
      remote_streams() = 0;

  // Add a new local stream.
  // This function does not trigger any changes to the stream until
  // CommitStreamChanges is called.
  virtual void AddStream(LocalMediaStreamInterface* stream) = 0;

  // Remove a local stream and stop sending it.
  // This function does not trigger any changes to the stream until
  // CommitStreamChanges is called.
  virtual void RemoveStream(LocalMediaStreamInterface* stream) = 0;

  // Commit Stream changes. This will start sending media on new streams
  // and stop sending media on removed stream.
  virtual void CommitStreamChanges() = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionInterface() {}
};

// Reference counted wrapper for talk_base::NetworkManager.
class PcNetworkManager : public talk_base::RefCountInterface {
 public:
  static talk_base::scoped_refptr<PcNetworkManager> Create(
      talk_base::NetworkManager* network_manager);
  virtual talk_base::NetworkManager* network_manager() const;

 protected:
  explicit PcNetworkManager(talk_base::NetworkManager* network_manager);
  virtual ~PcNetworkManager();

  talk_base::NetworkManager* network_manager_;
};

// Reference counted wrapper for talk_base::PacketSocketFactory.
class PcPacketSocketFactory : public talk_base::RefCountInterface {
 public:
  static talk_base::scoped_refptr<PcPacketSocketFactory> Create(
      talk_base::PacketSocketFactory* socket_factory);
  virtual talk_base::PacketSocketFactory* socket_factory() const;

 protected:
  explicit PcPacketSocketFactory(
      talk_base::PacketSocketFactory* socket_factory);
  virtual ~PcPacketSocketFactory();

  talk_base::PacketSocketFactory* socket_factory_;
};

// PeerConnectionManager is the factory interface use for creating
// PeerConnection, MediaStream and media tracks.
// PeerConnectionManager will create required libjingle threads, socket and
// network manager factory classes for networking.
// If application decides to provide its own implementation of these classes
// it should use alternate create method which accepts these parameters
// as input.
class PeerConnectionManager : public talk_base::RefCountInterface {
 public:
  // Create a new instance of PeerConnectionManager.
  static talk_base::scoped_refptr<PeerConnectionManager> Create();

  // Create a new instance of PeerConnectionManager.
  // Ownership of the arguments are not transfered to this object and must
  // remain in scope for the lifetime of the PeerConnectionManager.
  static talk_base::scoped_refptr<PeerConnectionManager> Create(
      talk_base::Thread* worker_thread,
      talk_base::Thread* signaling_thread,
      PcNetworkManager* network_manager,
      PcPacketSocketFactory* packet_socket_factory,
      AudioDeviceModule* default_adm);

  virtual talk_base::scoped_refptr<PeerConnectionInterface>
      CreatePeerConnection(const std::string& config,
                           PeerConnectionObserver* observer) = 0;

  virtual talk_base::scoped_refptr<LocalMediaStreamInterface>
      CreateLocalMediaStream(const std::string& label) = 0;

  virtual talk_base::scoped_refptr<LocalVideoTrackInterface>
      CreateLocalVideoTrack(const std::string& label,
                            VideoCaptureModule* video_device) = 0;

  virtual talk_base::scoped_refptr<LocalAudioTrackInterface>
      CreateLocalAudioTrack(const std::string& label,
                            AudioDeviceModule* audio_device) = 0;

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface.
  ~PeerConnectionManager() {}
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_PEERCONNECTION_H_
