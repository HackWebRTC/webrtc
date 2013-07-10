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

#ifndef TALK_EXAMPLES_PEERCONNECTION_SERVER_PEER_CHANNEL_H_
#define TALK_EXAMPLES_PEERCONNECTION_SERVER_PEER_CHANNEL_H_
#pragma once

#include <time.h>

#include <queue>
#include <string>
#include <vector>

class DataSocket;

// Represents a single peer connected to the server.
class ChannelMember {
 public:
  explicit ChannelMember(DataSocket* socket);
  ~ChannelMember();

  bool connected() const { return connected_; }
  int id() const { return id_; }
  void set_disconnected() { connected_ = false; }
  bool is_wait_request(DataSocket* ds) const;
  const std::string& name() const { return name_; }

  bool TimedOut();

  std::string GetPeerIdHeader() const;

  bool NotifyOfOtherMember(const ChannelMember& other);

  // Returns a string in the form "name,id\n".
  std::string GetEntry() const;

  void ForwardRequestToPeer(DataSocket* ds, ChannelMember* peer);

  void OnClosing(DataSocket* ds);

  void QueueResponse(const std::string& status, const std::string& content_type,
                     const std::string& extra_headers, const std::string& data);

  void SetWaitingSocket(DataSocket* ds);

 protected:
  struct QueuedResponse {
    std::string status, content_type, extra_headers, data;
  };

  DataSocket* waiting_socket_;
  int id_;
  bool connected_;
  time_t timestamp_;
  std::string name_;
  std::queue<QueuedResponse> queue_;
  static int s_member_id_;
};

// Manages all currently connected peers.
class PeerChannel {
 public:
  typedef std::vector<ChannelMember*> Members;

  PeerChannel() {
  }

  ~PeerChannel() {
    DeleteAll();
  }

  const Members& members() const { return members_; }

  // Returns true if the request should be treated as a new ChannelMember
  // request.  Otherwise the request is not peerconnection related.
  static bool IsPeerConnection(const DataSocket* ds);

  // Finds a connected peer that's associated with the |ds| socket.
  ChannelMember* Lookup(DataSocket* ds) const;

  // Checks if the request has a "peer_id" parameter and if so, looks up the
  // peer for which the request is targeted at.
  ChannelMember* IsTargetedRequest(const DataSocket* ds) const;

  // Adds a new ChannelMember instance to the list of connected peers and
  // associates it with the socket.
  bool AddMember(DataSocket* ds);

  // Closes all connections and sends a "shutting down" message to all
  // connected peers.
  void CloseAll();

  // Called when a socket was determined to be closing by the peer (or if the
  // connection went dead).
  void OnClosing(DataSocket* ds);

  void CheckForTimeout();

 protected:
  void DeleteAll();
  void BroadcastChangedState(const ChannelMember& member,
                             Members* delivery_failures);
  void HandleDeliveryFailures(Members* failures);

  // Builds a simple list of "name,id\n" entries for each member.
  std::string BuildResponseForNewMember(const ChannelMember& member,
                                        std::string* content_type);

 protected:
  Members members_;
};

#endif  // TALK_EXAMPLES_PEERCONNECTION_SERVER_PEER_CHANNEL_H_
