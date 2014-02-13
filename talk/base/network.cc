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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "talk/base/network.h"

#ifdef POSIX
// linux/if.h can't be included at the same time as the posix sys/if.h, and
// it's transitively required by linux/route.h, so include that version on
// linux instead of the standard posix one.
#if defined(ANDROID) || defined(LINUX)
#include <linux/if.h>
#include <linux/route.h>
#elif !defined(__native_client__)
#include <net/if.h>
#endif
#include <sys/socket.h>
#include <sys/utsname.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>

#ifdef ANDROID
#include "talk/base/ifaddrs-android.h"
#elif !defined(__native_client__)
#include <ifaddrs.h>
#endif

#endif  // POSIX

#ifdef WIN32
#include "talk/base/win32.h"
#include <Iphlpapi.h>
#endif

#include <algorithm>
#include <cstdio>

#include "talk/base/logging.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/socket.h"  // includes something that makes windows happy
#include "talk/base/stream.h"
#include "talk/base/stringencode.h"
#include "talk/base/thread.h"

namespace talk_base {
namespace {

const uint32 kUpdateNetworksMessage = 1;
const uint32 kSignalNetworksMessage = 2;

// Fetch list of networks every two seconds.
const int kNetworksUpdateIntervalMs = 2000;

const int kHighestNetworkPreference = 127;

bool CompareNetworks(const Network* a, const Network* b) {
  if (a->prefix_length() == b->prefix_length()) {
    if (a->name() == b->name()) {
      return a->prefix() < b->prefix();
    }
  }
  return a->name() < b->name();
}

bool SortNetworks(const Network* a, const Network* b) {
  // Network types will be preferred above everything else while sorting
  // Networks.

  // Networks are sorted first by type.
  if (a->type() != b->type()) {
    return a->type() < b->type();
  }

  // After type, networks are sorted by IP address precedence values
  // from RFC 3484-bis
  if (IPAddressPrecedence(a->ip()) != IPAddressPrecedence(b->ip())) {
    return IPAddressPrecedence(a->ip()) > IPAddressPrecedence(b->ip());
  }

  // TODO(mallinath) - Add VPN and Link speed conditions while sorting.

  // Networks are sorted last by key.
  return a->key() > b->key();
}

}  // namespace

std::string MakeNetworkKey(const std::string& name, const IPAddress& prefix,
                           int prefix_length) {
  std::ostringstream ost;
  ost << name << "%" << prefix.ToString() << "/" << prefix_length;
  return ost.str();
}

NetworkManager::NetworkManager() {
}

NetworkManager::~NetworkManager() {
}

NetworkManagerBase::NetworkManagerBase() : ipv6_enabled_(true) {
}

NetworkManagerBase::~NetworkManagerBase() {
  for (NetworkMap::iterator i = networks_map_.begin();
       i != networks_map_.end(); ++i) {
    delete i->second;
  }
}

void NetworkManagerBase::GetNetworks(NetworkList* result) const {
  *result = networks_;
}

void NetworkManagerBase::MergeNetworkList(const NetworkList& new_networks,
                                          bool* changed) {
  // Sort the list so that we can detect when it changes.
  typedef std::pair<Network*, std::vector<IPAddress> > address_list;
  std::map<std::string, address_list> address_map;
  NetworkList list(new_networks);
  NetworkList merged_list;
  std::sort(list.begin(), list.end(), CompareNetworks);

  *changed = false;

  if (networks_.size() != list.size())
    *changed = true;

  // First, build a set of network-keys to the ipaddresses.
  for (uint32 i = 0; i < list.size(); ++i) {
    bool might_add_to_merged_list = false;
    std::string key = MakeNetworkKey(list[i]->name(),
                                     list[i]->prefix(),
                                     list[i]->prefix_length());
    if (address_map.find(key) == address_map.end()) {
      address_map[key] = address_list(list[i], std::vector<IPAddress>());
      might_add_to_merged_list = true;
    }
    const std::vector<IPAddress>& addresses = list[i]->GetIPs();
    address_list& current_list = address_map[key];
    for (std::vector<IPAddress>::const_iterator it = addresses.begin();
         it != addresses.end();
         ++it) {
      current_list.second.push_back(*it);
    }
    if (!might_add_to_merged_list) {
      delete list[i];
    }
  }

  // Next, look for existing network objects to re-use.
  for (std::map<std::string, address_list >::iterator it = address_map.begin();
       it != address_map.end();
       ++it) {
    const std::string& key = it->first;
    Network* net = it->second.first;
    NetworkMap::iterator existing = networks_map_.find(key);
    if (existing == networks_map_.end()) {
      // This network is new. Place it in the network map.
      merged_list.push_back(net);
      networks_map_[key] = net;
      *changed = true;
    } else {
      // This network exists in the map already. Reset its IP addresses.
      *changed = existing->second->SetIPs(it->second.second, *changed);
      merged_list.push_back(existing->second);
      if (existing->second != net) {
        delete net;
      }
    }
  }
  networks_ = merged_list;

  // If the network lists changes, we resort it.
  if (changed) {
    std::sort(networks_.begin(), networks_.end(), SortNetworks);
    // Now network interfaces are sorted, we should set the preference value
    // for each of the interfaces we are planning to use.
    // Preference order of network interfaces might have changed from previous
    // sorting due to addition of higher preference network interface.
    // Since we have already sorted the network interfaces based on our
    // requirements, we will just assign a preference value starting with 127,
    // in decreasing order.
    int pref = kHighestNetworkPreference;
    for (NetworkList::const_iterator iter = networks_.begin();
         iter != networks_.end(); ++iter) {
      (*iter)->set_preference(pref);
      if (pref > 0) {
        --pref;
      } else {
        LOG(LS_ERROR) << "Too many network interfaces to handle!";
        break;
      }
    }
  }
}

BasicNetworkManager::BasicNetworkManager()
    : thread_(NULL), sent_first_update_(false), start_count_(0),
      ignore_non_default_routes_(false) {
}

BasicNetworkManager::~BasicNetworkManager() {
}

#if defined(__native_client__)

bool BasicNetworkManager::CreateNetworks(bool include_ignored,
                                         NetworkList* networks) const {
  ASSERT(false);
  LOG(LS_WARNING) << "BasicNetworkManager doesn't work on NaCl yet";
  return false;
}

#elif defined(POSIX)
void BasicNetworkManager::ConvertIfAddrs(struct ifaddrs* interfaces,
                                         bool include_ignored,
                                         NetworkList* networks) const {
  NetworkMap current_networks;
  for (struct ifaddrs* cursor = interfaces;
       cursor != NULL; cursor = cursor->ifa_next) {
    IPAddress prefix;
    IPAddress mask;
    IPAddress ip;
    int scope_id = 0;

    // Some interfaces may not have address assigned.
    if (!cursor->ifa_addr || !cursor->ifa_netmask)
      continue;

    switch (cursor->ifa_addr->sa_family) {
      case AF_INET: {
        ip = IPAddress(
            reinterpret_cast<sockaddr_in*>(cursor->ifa_addr)->sin_addr);
        mask = IPAddress(
            reinterpret_cast<sockaddr_in*>(cursor->ifa_netmask)->sin_addr);
        break;
      }
      case AF_INET6: {
        if (ipv6_enabled()) {
          ip = IPAddress(
              reinterpret_cast<sockaddr_in6*>(cursor->ifa_addr)->sin6_addr);
          mask = IPAddress(
              reinterpret_cast<sockaddr_in6*>(cursor->ifa_netmask)->sin6_addr);
          scope_id =
              reinterpret_cast<sockaddr_in6*>(cursor->ifa_addr)->sin6_scope_id;
          break;
        } else {
          continue;
        }
      }
      default: {
        continue;
      }
    }

    int prefix_length = CountIPMaskBits(mask);
    prefix = TruncateIP(ip, prefix_length);
    std::string key = MakeNetworkKey(std::string(cursor->ifa_name),
                                     prefix, prefix_length);
    NetworkMap::iterator existing_network = current_networks.find(key);
    if (existing_network == current_networks.end()) {
      scoped_ptr<Network> network(new Network(cursor->ifa_name,
                                              cursor->ifa_name,
                                              prefix,
                                              prefix_length,
                                              key));
      network->set_scope_id(scope_id);
      network->AddIP(ip);
      bool ignored = ((cursor->ifa_flags & IFF_LOOPBACK) ||
                      IsIgnoredNetwork(*network));
      network->set_ignored(ignored);
      if (include_ignored || !network->ignored()) {
        networks->push_back(network.release());
      }
    } else {
      (*existing_network).second->AddIP(ip);
    }
  }
}

bool BasicNetworkManager::CreateNetworks(bool include_ignored,
                                         NetworkList* networks) const {
  struct ifaddrs* interfaces;
  int error = getifaddrs(&interfaces);
  if (error != 0) {
    LOG_ERR(LERROR) << "getifaddrs failed to gather interface data: " << error;
    return false;
  }

  ConvertIfAddrs(interfaces, include_ignored, networks);

  freeifaddrs(interfaces);
  return true;
}

#elif defined(WIN32)

unsigned int GetPrefix(PIP_ADAPTER_PREFIX prefixlist,
              const IPAddress& ip, IPAddress* prefix) {
  IPAddress current_prefix;
  IPAddress best_prefix;
  unsigned int best_length = 0;
  while (prefixlist) {
    // Look for the longest matching prefix in the prefixlist.
    if (prefixlist->Address.lpSockaddr == NULL ||
        prefixlist->Address.lpSockaddr->sa_family != ip.family()) {
      prefixlist = prefixlist->Next;
      continue;
    }
    switch (prefixlist->Address.lpSockaddr->sa_family) {
      case AF_INET: {
        sockaddr_in* v4_addr =
            reinterpret_cast<sockaddr_in*>(prefixlist->Address.lpSockaddr);
        current_prefix = IPAddress(v4_addr->sin_addr);
        break;
      }
      case AF_INET6: {
          sockaddr_in6* v6_addr =
              reinterpret_cast<sockaddr_in6*>(prefixlist->Address.lpSockaddr);
          current_prefix = IPAddress(v6_addr->sin6_addr);
          break;
      }
      default: {
        prefixlist = prefixlist->Next;
        continue;
      }
    }
    if (TruncateIP(ip, prefixlist->PrefixLength) == current_prefix &&
        prefixlist->PrefixLength > best_length) {
      best_prefix = current_prefix;
      best_length = prefixlist->PrefixLength;
    }
    prefixlist = prefixlist->Next;
  }
  *prefix = best_prefix;
  return best_length;
}

bool BasicNetworkManager::CreateNetworks(bool include_ignored,
                                         NetworkList* networks) const {
  NetworkMap current_networks;
  // MSDN recommends a 15KB buffer for the first try at GetAdaptersAddresses.
  size_t buffer_size = 16384;
  scoped_ptr<char[]> adapter_info(new char[buffer_size]);
  PIP_ADAPTER_ADDRESSES adapter_addrs =
      reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapter_info.get());
  int adapter_flags = (GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_ANYCAST |
                       GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_INCLUDE_PREFIX);
  int ret = 0;
  do {
    adapter_info.reset(new char[buffer_size]);
    adapter_addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(adapter_info.get());
    ret = GetAdaptersAddresses(AF_UNSPEC, adapter_flags,
                               0, adapter_addrs,
                               reinterpret_cast<PULONG>(&buffer_size));
  } while (ret == ERROR_BUFFER_OVERFLOW);
  if (ret != ERROR_SUCCESS) {
    return false;
  }
  int count = 0;
  while (adapter_addrs) {
    if (adapter_addrs->OperStatus == IfOperStatusUp) {
      PIP_ADAPTER_UNICAST_ADDRESS address = adapter_addrs->FirstUnicastAddress;
      PIP_ADAPTER_PREFIX prefixlist = adapter_addrs->FirstPrefix;
      std::string name;
      std::string description;
#ifdef _DEBUG
      name = ToUtf8(adapter_addrs->FriendlyName,
                    wcslen(adapter_addrs->FriendlyName));
#endif
      description = ToUtf8(adapter_addrs->Description,
                           wcslen(adapter_addrs->Description));
      for (; address; address = address->Next) {
#ifndef _DEBUG
        name = talk_base::ToString(count);
#endif

        IPAddress ip;
        int scope_id = 0;
        scoped_ptr<Network> network;
        switch (address->Address.lpSockaddr->sa_family) {
          case AF_INET: {
            sockaddr_in* v4_addr =
                reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
            ip = IPAddress(v4_addr->sin_addr);
            break;
          }
          case AF_INET6: {
            if (ipv6_enabled()) {
              sockaddr_in6* v6_addr =
                  reinterpret_cast<sockaddr_in6*>(address->Address.lpSockaddr);
              scope_id = v6_addr->sin6_scope_id;
              ip = IPAddress(v6_addr->sin6_addr);
              break;
            } else {
              continue;
            }
          }
          default: {
            continue;
          }
        }

        IPAddress prefix;
        int prefix_length = GetPrefix(prefixlist, ip, &prefix);
        std::string key = MakeNetworkKey(name, prefix, prefix_length);
        NetworkMap::iterator existing_network = current_networks.find(key);
        if (existing_network == current_networks.end()) {
          scoped_ptr<Network> network(new Network(name,
                                                  description,
                                                  prefix,
                                                  prefix_length,
                                                  key));
          network->set_scope_id(scope_id);
          network->AddIP(ip);
          bool ignore = ((adapter_addrs->IfType == IF_TYPE_SOFTWARE_LOOPBACK) ||
                         IsIgnoredNetwork(*network));
          network->set_ignored(ignore);
          if (include_ignored || !network->ignored()) {
            networks->push_back(network.release());
          }
        } else {
          (*existing_network).second->AddIP(ip);
        }
      }
      // Count is per-adapter - all 'Networks' created from the same
      // adapter need to have the same name.
      ++count;
    }
    adapter_addrs = adapter_addrs->Next;
  }
  return true;
}
#endif  // WIN32

#if defined(ANDROID) || defined(LINUX)
bool IsDefaultRoute(const std::string& network_name) {
  FileStream fs;
  if (!fs.Open("/proc/net/route", "r", NULL)) {
    LOG(LS_WARNING) << "Couldn't read /proc/net/route, skipping default "
                    << "route check (assuming everything is a default route).";
    return true;
  } else {
    std::string line;
    while (fs.ReadLine(&line) == SR_SUCCESS) {
      char iface_name[256];
      unsigned int iface_ip, iface_gw, iface_mask, iface_flags;
      if (sscanf(line.c_str(),
                 "%255s %8X %8X %4X %*d %*u %*d %8X",
                 iface_name, &iface_ip, &iface_gw,
                 &iface_flags, &iface_mask) == 5 &&
          network_name == iface_name &&
          iface_mask == 0 &&
          (iface_flags & (RTF_UP | RTF_HOST)) == RTF_UP) {
        return true;
      }
    }
  }
  return false;
}
#endif

bool BasicNetworkManager::IsIgnoredNetwork(const Network& network) const {
  // Ignore networks on the explicit ignore list.
  for (size_t i = 0; i < network_ignore_list_.size(); ++i) {
    if (network.name() == network_ignore_list_[i]) {
      return true;
    }
  }
#ifdef POSIX
  // Filter out VMware interfaces, typically named vmnet1 and vmnet8
  if (strncmp(network.name().c_str(), "vmnet", 5) == 0 ||
      strncmp(network.name().c_str(), "vnic", 4) == 0) {
    return true;
  }
#if defined(ANDROID) || defined(LINUX)
  // Make sure this is a default route, if we're ignoring non-defaults.
  if (ignore_non_default_routes_ && !IsDefaultRoute(network.name())) {
    return true;
  }
#endif
#elif defined(WIN32)
  // Ignore any HOST side vmware adapters with a description like:
  // VMware Virtual Ethernet Adapter for VMnet1
  // but don't ignore any GUEST side adapters with a description like:
  // VMware Accelerated AMD PCNet Adapter #2
  if (strstr(network.description().c_str(), "VMnet") != NULL) {
    return true;
  }
#endif

  // Ignore any networks with a 0.x.y.z IP
  if (network.prefix().family() == AF_INET) {
    return (network.prefix().v4AddressAsHostOrderInteger() < 0x01000000);
  }
  return false;
}

void BasicNetworkManager::StartUpdating() {
  thread_ = Thread::Current();
  if (start_count_) {
    // If network interfaces are already discovered and signal is sent,
    // we should trigger network signal immediately for the new clients
    // to start allocating ports.
    if (sent_first_update_)
      thread_->Post(this, kSignalNetworksMessage);
  } else {
    thread_->Post(this, kUpdateNetworksMessage);
  }
  ++start_count_;
}

void BasicNetworkManager::StopUpdating() {
  ASSERT(Thread::Current() == thread_);
  if (!start_count_)
    return;

  --start_count_;
  if (!start_count_) {
    thread_->Clear(this);
    sent_first_update_ = false;
  }
}

void BasicNetworkManager::OnMessage(Message* msg) {
  switch (msg->message_id) {
    case kUpdateNetworksMessage:  {
      DoUpdateNetworks();
      break;
    }
    case kSignalNetworksMessage:  {
      SignalNetworksChanged();
      break;
    }
    default:
      ASSERT(false);
  }
}

void BasicNetworkManager::DoUpdateNetworks() {
  if (!start_count_)
    return;

  ASSERT(Thread::Current() == thread_);

  NetworkList list;
  if (!CreateNetworks(false, &list)) {
    SignalError();
  } else {
    bool changed;
    MergeNetworkList(list, &changed);
    if (changed || !sent_first_update_) {
      SignalNetworksChanged();
      sent_first_update_ = true;
    }
  }

  thread_->PostDelayed(kNetworksUpdateIntervalMs, this, kUpdateNetworksMessage);
}

void BasicNetworkManager::DumpNetworks(bool include_ignored) {
  NetworkList list;
  CreateNetworks(include_ignored, &list);
  LOG(LS_INFO) << "NetworkManager detected " << list.size() << " networks:";
  for (size_t i = 0; i < list.size(); ++i) {
    const Network* network = list[i];
    if (!network->ignored() || include_ignored) {
      LOG(LS_INFO) << network->ToString() << ": "
                   << network->description()
                   << ((network->ignored()) ? ", Ignored" : "");
    }
  }
  // Release the network list created previously.
  // Do this in a seperated for loop for better readability.
  for (size_t i = 0; i < list.size(); ++i) {
    delete list[i];
  }
}

Network::Network(const std::string& name, const std::string& desc,
                 const IPAddress& prefix, int prefix_length,
                 const std::string& key)
    : name_(name), description_(desc), prefix_(prefix),
      prefix_length_(prefix_length), key_(key), scope_id_(0), ignored_(false),
      uniform_numerator_(0), uniform_denominator_(0), exponential_numerator_(0),
      exponential_denominator_(0), type_(ADAPTER_TYPE_UNKNOWN), preference_(0) {
}

Network::Network(const std::string& name, const std::string& desc,
                 const IPAddress& prefix, int prefix_length)
    : name_(name), description_(desc), prefix_(prefix),
      prefix_length_(prefix_length), scope_id_(0), ignored_(false),
      uniform_numerator_(0), uniform_denominator_(0), exponential_numerator_(0),
      exponential_denominator_(0), type_(ADAPTER_TYPE_UNKNOWN), preference_(0) {
}

std::string Network::ToString() const {
  std::stringstream ss;
  // Print out the first space-terminated token of the network desc, plus
  // the IP address.
  ss << "Net[" << description_.substr(0, description_.find(' '))
     << ":" << prefix_.ToSensitiveString() << "/" << prefix_length_ << "]";
  return ss.str();
}

// Sets the addresses of this network. Returns true if the address set changed.
// Change detection is short circuited if the changed argument is true.
bool Network::SetIPs(const std::vector<IPAddress>& ips, bool changed) {
  changed = changed || ips.size() != ips_.size();
  // Detect changes with a nested loop; n-squared but we expect on the order
  // of 2-3 addresses per network.
  for (std::vector<IPAddress>::const_iterator it = ips.begin();
      !changed && it != ips.end();
      ++it) {
    bool found = false;
    for (std::vector<IPAddress>::iterator inner_it = ips_.begin();
         !found && inner_it != ips_.end();
         ++inner_it) {
      if (*it == *inner_it) {
        found = true;
      }
    }
    changed = !found;
  }
  ips_ = ips;
  return changed;
}

}  // namespace talk_base
