/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#include "talk/base/network.h"

#include <vector>
#if defined(POSIX)
#include <sys/types.h>
#ifndef ANDROID
#include <ifaddrs.h>
#else
#include "talk/base/ifaddrs-android.h"
#endif
#endif
#include "talk/base/gunit.h"

namespace talk_base {

class NetworkTest : public testing::Test, public sigslot::has_slots<>  {
 public:
  NetworkTest() : callback_called_(false) {}

  void OnNetworksChanged() {
    callback_called_ = true;
  }

  void MergeNetworkList(BasicNetworkManager& network_manager,
                        const NetworkManager::NetworkList& list,
                        bool* changed ) {
    network_manager.MergeNetworkList(list, changed);
  }

  bool IsIgnoredNetwork(const Network& network) {
    return BasicNetworkManager::IsIgnoredNetwork(network);
  }

  NetworkManager::NetworkList GetNetworks(
      const BasicNetworkManager& network_manager, bool include_ignored) {
    NetworkManager::NetworkList list;
    network_manager.CreateNetworks(include_ignored, &list);
    return list;
  }

#if defined(POSIX)
  // Separated from CreateNetworks for tests.
  static void CallConvertIfAddrs(const BasicNetworkManager& network_manager,
                                 struct ifaddrs* interfaces,
                                 bool include_ignored,
                                 NetworkManager::NetworkList* networks) {
    network_manager.ConvertIfAddrs(interfaces, include_ignored, networks);
  }
#endif  // defined(POSIX)

 protected:
  bool callback_called_;
};

// Test that the Network ctor works properly.
TEST_F(NetworkTest, TestNetworkConstruct) {
  Network ipv4_network1("test_eth0", "Test Network Adapter 1",
                        IPAddress(0x12345600U), 24);
  EXPECT_EQ("test_eth0", ipv4_network1.name());
  EXPECT_EQ("Test Network Adapter 1", ipv4_network1.description());
  EXPECT_EQ(IPAddress(0x12345600U), ipv4_network1.prefix());
  EXPECT_EQ(24, ipv4_network1.prefix_length());
  EXPECT_FALSE(ipv4_network1.ignored());
}

// Tests that our ignore function works properly.
TEST_F(NetworkTest, TestNetworkIgnore) {
  Network ipv4_network1("test_eth0", "Test Network Adapter 1",
                        IPAddress(0x12345600U), 24);
  Network ipv4_network2("test_eth1", "Test Network Adapter 2",
                        IPAddress(0x00010000U), 16);
  EXPECT_FALSE(IsIgnoredNetwork(ipv4_network1));
  EXPECT_TRUE(IsIgnoredNetwork(ipv4_network2));
}

TEST_F(NetworkTest, TestCreateNetworks) {
  BasicNetworkManager manager;
  NetworkManager::NetworkList result = GetNetworks(manager, true);
  // We should be able to bind to any addresses we find.
  NetworkManager::NetworkList::iterator it;
  for (it = result.begin();
       it != result.end();
       ++it) {
    sockaddr_storage storage;
    memset(&storage, 0, sizeof(storage));
    IPAddress ip = (*it)->ip();
    SocketAddress bindaddress(ip, 0);
    bindaddress.SetScopeID((*it)->scope_id());
    // TODO: Make this use talk_base::AsyncSocket once it supports IPv6.
    int fd = static_cast<int>(socket(ip.family(), SOCK_STREAM, IPPROTO_TCP));
    if (fd > 0) {
      size_t ipsize = bindaddress.ToSockAddrStorage(&storage);
      EXPECT_GE(ipsize, 0U);
      int success = ::bind(fd,
                           reinterpret_cast<sockaddr*>(&storage),
                           static_cast<int>(ipsize));
      EXPECT_EQ(0, success);
#ifdef WIN32
      closesocket(fd);
#else
      close(fd);
#endif
    }
  }
}

// Test that UpdateNetworks succeeds.
TEST_F(NetworkTest, TestUpdateNetworks) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(
      static_cast<NetworkTest*>(this), &NetworkTest::OnNetworksChanged);
  manager.StartUpdating();
  Thread::Current()->ProcessMessages(0);
  EXPECT_TRUE(callback_called_);
  callback_called_ = false;
  // Callback should be triggered immediately when StartUpdating
  // is called, after network update signal is already sent.
  manager.StartUpdating();
  EXPECT_TRUE(manager.started());
  Thread::Current()->ProcessMessages(0);
  EXPECT_TRUE(callback_called_);
  manager.StopUpdating();
  EXPECT_TRUE(manager.started());
  manager.StopUpdating();
  EXPECT_FALSE(manager.started());
  manager.StopUpdating();
  EXPECT_FALSE(manager.started());
  callback_called_ = false;
  // Callback should be triggered immediately after StartUpdating is called
  // when start_count_ is reset to 0.
  manager.StartUpdating();
  Thread::Current()->ProcessMessages(0);
  EXPECT_TRUE(callback_called_);
}

// Verify that MergeNetworkList() merges network lists properly.
TEST_F(NetworkTest, TestBasicMergeNetworkList) {
  Network ipv4_network1("test_eth0", "Test Network Adapter 1",
                        IPAddress(0x12345600U), 24);
  Network ipv4_network2("test_eth1", "Test Network Adapter 2",
                        IPAddress(0x00010000U), 16);
  ipv4_network1.AddIP(IPAddress(0x12345678));
  ipv4_network2.AddIP(IPAddress(0x00010004));
  BasicNetworkManager manager;

  // Add ipv4_network1 to the list of networks.
  NetworkManager::NetworkList list;
  list.push_back(new Network(ipv4_network1));
  bool changed;
  MergeNetworkList(manager, list, &changed);
  EXPECT_TRUE(changed);
  list.clear();

  manager.GetNetworks(&list);
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(ipv4_network1.ToString(), list[0]->ToString());
  Network* net1 = list[0];
  list.clear();

  // Replace ipv4_network1 with ipv4_network2.
  list.push_back(new Network(ipv4_network2));
  MergeNetworkList(manager, list, &changed);
  EXPECT_TRUE(changed);
  list.clear();

  manager.GetNetworks(&list);
  EXPECT_EQ(1U, list.size());
  EXPECT_EQ(ipv4_network2.ToString(), list[0]->ToString());
  Network* net2 = list[0];
  list.clear();

  // Add Network2 back.
  list.push_back(new Network(ipv4_network1));
  list.push_back(new Network(ipv4_network2));
  MergeNetworkList(manager, list, &changed);
  EXPECT_TRUE(changed);
  list.clear();

  // Verify that we get previous instances of Network objects.
  manager.GetNetworks(&list);
  EXPECT_EQ(2U, list.size());
  EXPECT_TRUE((net1 == list[0] && net2 == list[1]) ||
              (net1 == list[1] && net2 == list[0]));
  list.clear();

  // Call MergeNetworkList() again and verify that we don't get update
  // notification.
  list.push_back(new Network(ipv4_network2));
  list.push_back(new Network(ipv4_network1));
  MergeNetworkList(manager, list, &changed);
  EXPECT_FALSE(changed);
  list.clear();

  // Verify that we get previous instances of Network objects.
  manager.GetNetworks(&list);
  EXPECT_EQ(2U, list.size());
  EXPECT_TRUE((net1 == list[0] && net2 == list[1]) ||
              (net1 == list[1] && net2 == list[0]));
  list.clear();
}

// Sets up some test IPv6 networks and appends them to list.
// Four networks are added - public and link local, for two interfaces.
void SetupNetworks(NetworkManager::NetworkList* list) {
  IPAddress ip;
  IPAddress prefix;
  EXPECT_TRUE(IPFromString("fe80::1234:5678:abcd:ef12", &ip));
  EXPECT_TRUE(IPFromString("fe80::", &prefix));
  // First, fake link-locals.
  Network ipv6_eth0_linklocalnetwork("test_eth0", "Test NetworkAdapter 1",
                                     prefix, 64);
  ipv6_eth0_linklocalnetwork.AddIP(ip);
  EXPECT_TRUE(IPFromString("fe80::5678:abcd:ef12:3456", &ip));
  Network ipv6_eth1_linklocalnetwork("test_eth1", "Test NetworkAdapter 2",
                                     prefix, 64);
  ipv6_eth1_linklocalnetwork.AddIP(ip);
  // Public networks:
  EXPECT_TRUE(IPFromString("2401:fa00:4:1000:be30:5bff:fee5:c3", &ip));
  prefix = TruncateIP(ip, 64);
  Network ipv6_eth0_publicnetwork1_ip1("test_eth0", "Test NetworkAdapter 1",
                                       prefix, 64);
  ipv6_eth0_publicnetwork1_ip1.AddIP(ip);
  EXPECT_TRUE(IPFromString("2400:4030:1:2c00:be30:abcd:efab:cdef", &ip));
  prefix = TruncateIP(ip, 64);
  Network ipv6_eth1_publicnetwork1_ip1("test_eth1", "Test NetworkAdapter 1",
                                       prefix, 64);
  ipv6_eth1_publicnetwork1_ip1.AddIP(ip);
  list->push_back(new Network(ipv6_eth0_linklocalnetwork));
  list->push_back(new Network(ipv6_eth1_linklocalnetwork));
  list->push_back(new Network(ipv6_eth0_publicnetwork1_ip1));
  list->push_back(new Network(ipv6_eth1_publicnetwork1_ip1));
}

// Test that the basic network merging case works.
TEST_F(NetworkTest, TestIPv6MergeNetworkList) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(
      static_cast<NetworkTest*>(this), &NetworkTest::OnNetworksChanged);
  NetworkManager::NetworkList original_list;
  SetupNetworks(&original_list);
  bool changed = false;
  MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  NetworkManager::NetworkList list;
  manager.GetNetworks(&list);
  EXPECT_EQ(original_list.size(), list.size());
  // Verify that the original members are in the merged list.
  for (NetworkManager::NetworkList::iterator it = original_list.begin();
       it != original_list.end(); ++it) {
    EXPECT_NE(list.end(), std::find(list.begin(), list.end(), *it));
  }
}

// Tests that when two network lists that describe the same set of networks are
// merged, that the changed callback is not called, and that the original
// objects remain in the result list.
TEST_F(NetworkTest, TestNoChangeMerge) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(
      static_cast<NetworkTest*>(this), &NetworkTest::OnNetworksChanged);
  NetworkManager::NetworkList original_list;
  SetupNetworks(&original_list);
  bool changed = false;
  MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  // Second list that describes the same networks but with new objects.
  NetworkManager::NetworkList second_list;
  SetupNetworks(&second_list);
  changed = false;
  MergeNetworkList(manager, second_list, &changed);
  EXPECT_FALSE(changed);
  NetworkManager::NetworkList resulting_list;
  manager.GetNetworks(&resulting_list);
  EXPECT_EQ(original_list.size(), resulting_list.size());
  // Verify that the original members are in the merged list.
  for (NetworkManager::NetworkList::iterator it = original_list.begin();
       it != original_list.end(); ++it) {
    EXPECT_NE(resulting_list.end(),
              std::find(resulting_list.begin(), resulting_list.end(), *it));
  }
  // Doublecheck that the new networks aren't in the list.
  for (NetworkManager::NetworkList::iterator it = second_list.begin();
       it != second_list.end(); ++it) {
    EXPECT_EQ(resulting_list.end(),
              std::find(resulting_list.begin(), resulting_list.end(), *it));
  }
}

// Test that we can merge a network that is the same as another network but with
// a different IP. The original network should remain in the list, but have its
// IP changed.
TEST_F(NetworkTest, MergeWithChangedIP) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(
      static_cast<NetworkTest*>(this), &NetworkTest::OnNetworksChanged);
  NetworkManager::NetworkList original_list;
  SetupNetworks(&original_list);
  // Make a network that we're going to change.
  IPAddress ip;
  EXPECT_TRUE(IPFromString("2401:fa01:4:1000:be30:faa:fee:faa", &ip));
  IPAddress prefix = TruncateIP(ip, 64);
  Network* network_to_change = new Network("test_eth0",
                                          "Test Network Adapter 1",
                                          prefix, 64);
  Network* changed_network = new Network(*network_to_change);
  network_to_change->AddIP(ip);
  IPAddress changed_ip;
  EXPECT_TRUE(IPFromString("2401:fa01:4:1000:be30:f00:f00:f00", &changed_ip));
  changed_network->AddIP(changed_ip);
  original_list.push_back(network_to_change);
  bool changed = false;
  MergeNetworkList(manager, original_list, &changed);
  NetworkManager::NetworkList second_list;
  SetupNetworks(&second_list);
  second_list.push_back(changed_network);
  changed = false;
  MergeNetworkList(manager, second_list, &changed);
  EXPECT_TRUE(changed);
  NetworkManager::NetworkList list;
  manager.GetNetworks(&list);
  EXPECT_EQ(original_list.size(), list.size());
  // Make sure the original network is still in the merged list.
  EXPECT_NE(list.end(),
            std::find(list.begin(), list.end(), network_to_change));
  EXPECT_EQ(changed_ip, network_to_change->GetIPs().at(0));
}

// Testing a similar case to above, but checking that a network can be updated
// with additional IPs (not just a replacement).
TEST_F(NetworkTest, TestMultipleIPMergeNetworkList) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(
      static_cast<NetworkTest*>(this), &NetworkTest::OnNetworksChanged);
  NetworkManager::NetworkList original_list;
  SetupNetworks(&original_list);
  bool changed = false;
  MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  IPAddress ip;
  IPAddress check_ip;
  IPAddress prefix;
  // Add a second IP to the public network on eth0 (2401:fa00:4:1000/64).
  EXPECT_TRUE(IPFromString("2401:fa00:4:1000:be30:5bff:fee5:c6", &ip));
  prefix = TruncateIP(ip, 64);
  Network ipv6_eth0_publicnetwork1_ip2("test_eth0", "Test NetworkAdapter 1",
                                       prefix, 64);
  // This is the IP that already existed in the public network on eth0.
  EXPECT_TRUE(IPFromString("2401:fa00:4:1000:be30:5bff:fee5:c3", &check_ip));
  ipv6_eth0_publicnetwork1_ip2.AddIP(ip);
  original_list.push_back(new Network(ipv6_eth0_publicnetwork1_ip2));
  changed = false;
  MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  // There should still be four networks.
  NetworkManager::NetworkList list;
  manager.GetNetworks(&list);
  EXPECT_EQ(4U, list.size());
  // Check the gathered IPs.
  int matchcount = 0;
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    if ((*it)->ToString() == original_list[2]->ToString()) {
      ++matchcount;
      EXPECT_EQ(1, matchcount);
      // This should be the same network object as before.
      EXPECT_EQ((*it), original_list[2]);
      // But with two addresses now.
      EXPECT_EQ(2U, (*it)->GetIPs().size());
      EXPECT_NE((*it)->GetIPs().end(),
                std::find((*it)->GetIPs().begin(),
                          (*it)->GetIPs().end(),
                          check_ip));
      EXPECT_NE((*it)->GetIPs().end(),
                std::find((*it)->GetIPs().begin(),
                          (*it)->GetIPs().end(),
                          ip));
    } else {
      // Check the IP didn't get added anywhere it wasn't supposed to.
      EXPECT_EQ((*it)->GetIPs().end(),
                std::find((*it)->GetIPs().begin(),
                          (*it)->GetIPs().end(),
                          ip));
    }
  }
}

// Test that merge correctly distinguishes multiple networks on an interface.
TEST_F(NetworkTest, TestMultiplePublicNetworksOnOneInterfaceMerge) {
  BasicNetworkManager manager;
  manager.SignalNetworksChanged.connect(
      static_cast<NetworkTest*>(this), &NetworkTest::OnNetworksChanged);
  NetworkManager::NetworkList original_list;
  SetupNetworks(&original_list);
  bool changed = false;
  MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  IPAddress ip;
  IPAddress prefix;
  // A second network for eth0.
  EXPECT_TRUE(IPFromString("2400:4030:1:2c00:be30:5bff:fee5:c3", &ip));
  prefix = TruncateIP(ip, 64);
  Network ipv6_eth0_publicnetwork2_ip1("test_eth0", "Test NetworkAdapter 1",
                                       prefix, 64);
  ipv6_eth0_publicnetwork2_ip1.AddIP(ip);
  original_list.push_back(new Network(ipv6_eth0_publicnetwork2_ip1));
  changed = false;
  MergeNetworkList(manager, original_list, &changed);
  EXPECT_TRUE(changed);
  // There should be five networks now.
  NetworkManager::NetworkList list;
  manager.GetNetworks(&list);
  EXPECT_EQ(5U, list.size());
  // Check the resulting addresses.
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    if ((*it)->prefix() == ipv6_eth0_publicnetwork2_ip1.prefix() &&
        (*it)->name() == ipv6_eth0_publicnetwork2_ip1.name()) {
      // Check the new network has 1 IP and that it's the correct one.
      EXPECT_EQ(1U, (*it)->GetIPs().size());
      EXPECT_EQ(ip, (*it)->GetIPs().at(0));
    } else {
      // Check the IP didn't get added anywhere it wasn't supposed to.
      EXPECT_EQ((*it)->GetIPs().end(),
                std::find((*it)->GetIPs().begin(),
                          (*it)->GetIPs().end(),
                          ip));
    }
  }
}

// Test that DumpNetworks works.
TEST_F(NetworkTest, TestDumpNetworks) {
  BasicNetworkManager manager;
  manager.DumpNetworks(true);
}

// Test that we can toggle IPv6 on and off.
TEST_F(NetworkTest, TestIPv6Toggle) {
  BasicNetworkManager manager;
  bool ipv6_found = false;
  NetworkManager::NetworkList list;
#ifndef WIN32
  // There should be at least one IPv6 network (fe80::/64 should be in there).
  // TODO: Disabling this test on windows for the moment as the test
  // machines don't seem to have IPv6 installed on them at all.
  manager.set_ipv6_enabled(true);
  list = GetNetworks(manager, true);
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    if ((*it)->prefix().family() == AF_INET6) {
      ipv6_found = true;
      break;
    }
  }
  EXPECT_TRUE(ipv6_found);
#endif
  ipv6_found = false;
  manager.set_ipv6_enabled(false);
  list = GetNetworks(manager, true);
  for (NetworkManager::NetworkList::iterator it = list.begin();
       it != list.end(); ++it) {
    if ((*it)->prefix().family() == AF_INET6) {
      ipv6_found = true;
      break;
    }
  }
  EXPECT_FALSE(ipv6_found);
}

#if defined(POSIX)
// Verify that we correctly handle interfaces with no address.
TEST_F(NetworkTest, TestConvertIfAddrsNoAddress) {
  ifaddrs list;
  memset(&list, 0, sizeof(list));
  list.ifa_name = const_cast<char*>("test_iface");

  NetworkManager::NetworkList result;
  BasicNetworkManager manager;
  CallConvertIfAddrs(manager, &list, true, &result);
  EXPECT_TRUE(result.empty());
}
#endif  // defined(POSIX)


}  // namespace talk_base
