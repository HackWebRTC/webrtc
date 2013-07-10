/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#ifndef TALK_BASE_NETWORK_H_
#define TALK_BASE_NETWORK_H_

#include <deque>
#include <map>
#include <string>
#include <vector>

#include "talk/base/basictypes.h"
#include "talk/base/ipaddress.h"
#include "talk/base/messagehandler.h"
#include "talk/base/sigslot.h"

#if defined(POSIX)
struct ifaddrs;
#endif  // defined(POSIX)

namespace talk_base {

class Network;
class NetworkSession;
class Thread;

// Generic network manager interface. It provides list of local
// networks.
class NetworkManager {
 public:
  typedef std::vector<Network*> NetworkList;

  NetworkManager();
  virtual ~NetworkManager();

  // Called when network list is updated.
  sigslot::signal0<> SignalNetworksChanged;

  // Indicates a failure when getting list of network interfaces.
  sigslot::signal0<> SignalError;

  // Start/Stop monitoring of network interfaces
  // list. SignalNetworksChanged or SignalError is emitted immidiately
  // after StartUpdating() is called. After that SignalNetworksChanged
  // is emitted wheneven list of networks changes.
  virtual void StartUpdating() = 0;
  virtual void StopUpdating() = 0;

  // Returns the current list of networks available on this machine.
  // UpdateNetworks() must be called before this method is called.
  // It makes sure that repeated calls return the same object for a
  // given network, so that quality is tracked appropriately. Does not
  // include ignored networks.
  virtual void GetNetworks(NetworkList* networks) const = 0;

  // Dumps a list of networks available to LS_INFO.
  virtual void DumpNetworks(bool include_ignored) {}
};

// Base class for NetworkManager implementations.
class NetworkManagerBase : public NetworkManager {
 public:
  NetworkManagerBase();
  virtual ~NetworkManagerBase();

  virtual void GetNetworks(std::vector<Network*>* networks) const;
  bool ipv6_enabled() const { return ipv6_enabled_; }
  void set_ipv6_enabled(bool enabled) { ipv6_enabled_ = enabled; }

 protected:
  typedef std::map<std::string, Network*> NetworkMap;
  // Updates |networks_| with the networks listed in |list|. If
  // |network_map_| already has a Network object for a network listed
  // in the |list| then it is reused. Accept ownership of the Network
  // objects in the |list|. |changed| will be set to true if there is
  // any change in the network list.
  void MergeNetworkList(const NetworkList& list, bool* changed);

 private:
  friend class NetworkTest;
  void DoUpdateNetworks();

  NetworkList networks_;
  NetworkMap networks_map_;
  bool ipv6_enabled_;
};

// Basic implementation of the NetworkManager interface that gets list
// of networks using OS APIs.
class BasicNetworkManager : public NetworkManagerBase,
                            public MessageHandler {
 public:
  BasicNetworkManager();
  virtual ~BasicNetworkManager();

  virtual void StartUpdating();
  virtual void StopUpdating();

  // Logs the available networks.
  virtual void DumpNetworks(bool include_ignored);

  // MessageHandler interface.
  virtual void OnMessage(Message* msg);
  bool started() { return start_count_ > 0; }

 protected:
#if defined(POSIX)
  // Separated from CreateNetworks for tests.
  void ConvertIfAddrs(ifaddrs* interfaces,
                      bool include_ignored,
                      NetworkList* networks) const;
#endif  // defined(POSIX)

  // Creates a network object for each network available on the machine.
  bool CreateNetworks(bool include_ignored, NetworkList* networks) const;

  // Determines if a network should be ignored.
  static bool IsIgnoredNetwork(const Network& network);

 private:
  friend class NetworkTest;

  void DoUpdateNetworks();

  Thread* thread_;
  bool sent_first_update_;
  int start_count_;
};

// Represents a Unix-type network interface, with a name and single address.
class Network {
 public:
  Network() : prefix_(INADDR_ANY), scope_id_(0) {}
  Network(const std::string& name, const std::string& description,
          const IPAddress& prefix, int prefix_length);

  // Returns the name of the interface this network is associated wtih.
  const std::string& name() const { return name_; }

  // Returns the OS-assigned name for this network. This is useful for
  // debugging but should not be sent over the wire (for privacy reasons).
  const std::string& description() const { return description_; }

  // Returns the prefix for this network.
  const IPAddress& prefix() const { return prefix_; }
  // Returns the length, in bits, of this network's prefix.
  int prefix_length() const { return prefix_length_; }

  // Returns the Network's current idea of the 'best' IP it has.
  // 'Best' currently means the first one added.
  // TODO: We should be preferring temporary addresses.
  // Returns an unset IP if this network has no active addresses.
  IPAddress ip() const {
    if (ips_.size() == 0) {
      return IPAddress();
    }
    return ips_.at(0);
  }
  // Adds an active IP address to this network. Does not check for duplicates.
  void AddIP(const IPAddress& ip) { ips_.push_back(ip); }

  // Sets the network's IP address list. Returns true if new IP addresses were
  // detected. Passing true to already_changed skips this check.
  bool SetIPs(const std::vector<IPAddress>& ips, bool already_changed);
  // Get the list of IP Addresses associated with this network.
  const std::vector<IPAddress>& GetIPs() { return ips_;}
  // Clear the network's list of addresses.
  void ClearIPs() { ips_.clear(); }

  // Returns the scope-id of the network's address.
  // Should only be relevant for link-local IPv6 addresses.
  int scope_id() const { return scope_id_; }
  void set_scope_id(int id) { scope_id_ = id; }

  // Indicates whether this network should be ignored, perhaps because
  // the IP is 0, or the interface is one we know is invalid.
  bool ignored() const { return ignored_; }
  void set_ignored(bool ignored) { ignored_ = ignored; }

  // Debugging description of this network
  std::string ToString() const;

 private:
  typedef std::vector<NetworkSession*> SessionList;

  std::string name_;
  std::string description_;
  IPAddress prefix_;
  int prefix_length_;
  std::vector<IPAddress> ips_;
  int scope_id_;
  bool ignored_;
  SessionList sessions_;
  double uniform_numerator_;
  double uniform_denominator_;
  double exponential_numerator_;
  double exponential_denominator_;

  friend class NetworkManager;
};
}  // namespace talk_base

#endif  // TALK_BASE_NETWORK_H_
