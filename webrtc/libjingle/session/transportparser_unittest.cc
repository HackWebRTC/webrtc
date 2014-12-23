/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/base/gunit.h"
#include "webrtc/libjingle/session/p2ptransportparser.h"
#include "webrtc/libjingle/session/parsing.h"
#include "webrtc/libjingle/session/sessionmessages.h"
#include "webrtc/libjingle/xmllite/xmlelement.h"
#include "webrtc/libjingle/xmpp/constants.h"
#include "webrtc/p2p/base/constants.h"

using cricket::Candidate;
using cricket::Candidates;
using cricket::ParseError;
using cricket::WriteError;

class TransportParserTest : public testing::Test {
};

class FakeCandidateTranslator : public cricket::CandidateTranslator {
 public:
  void AddMapping(int component, const std::string& channel_name) {
    name_to_component[channel_name] = component;
    component_to_name[component] = channel_name;
  }

  bool GetChannelNameFromComponent(
      int component, std::string* channel_name) const {
    if (component_to_name.find(component) == component_to_name.end()) {
      return false;
    }
    *channel_name = component_to_name.find(component)->second;
    return true;
  }
  bool GetComponentFromChannelName(
      const std::string& channel_name, int* component) const {
    if (name_to_component.find(channel_name) == name_to_component.end()) {
      return false;
    }
    *component = name_to_component.find(channel_name)->second;
    return true;
  }

  std::map<std::string, int> name_to_component;
  std::map<int, std::string> component_to_name;
};

// Tests that we can properly serialize/deserialize candidates.
TEST_F(TransportParserTest, TestP2PTransportWriteAndParseCandidate) {
  Candidate test_candidate(
      "", 1, "udp",
      rtc::SocketAddress("2001:db8:fefe::1", 9999),
      738197504, "abcdef", "ghijkl", "foo", 50, "");
  test_candidate.set_network_name("testnet");
  Candidate test_candidate2(
      "", 2, "tcp",
      rtc::SocketAddress("192.168.7.1", 9999),
      1107296256, "mnopqr", "stuvwx", "bar", 100, "");
  test_candidate2.set_network_name("testnet2");
  rtc::SocketAddress host_address("www.google.com", 24601);
  host_address.SetResolvedIP(rtc::IPAddress(0x0A000001));
  Candidate test_candidate3(
      "", 3, "spdy", host_address, 1476395008, "yzabcd",
      "efghij", "baz", 150, "");
  test_candidate3.set_network_name("testnet3");
  WriteError write_error;
  ParseError parse_error;
  rtc::scoped_ptr<buzz::XmlElement> elem;
  cricket::Candidate parsed_candidate;
  cricket::P2PTransportParser parser;

  FakeCandidateTranslator translator;
  translator.AddMapping(1, "test");
  translator.AddMapping(2, "test2");
  translator.AddMapping(3, "test3");

  EXPECT_TRUE(parser.WriteGingleCandidate(test_candidate, &translator,
                                          elem.accept(), &write_error));
  EXPECT_EQ("", write_error.text);
  EXPECT_EQ("test", elem->Attr(buzz::QN_NAME));
  EXPECT_EQ("udp", elem->Attr(cricket::QN_PROTOCOL));
  EXPECT_EQ("2001:db8:fefe::1", elem->Attr(cricket::QN_ADDRESS));
  EXPECT_EQ("9999", elem->Attr(cricket::QN_PORT));
  EXPECT_EQ("0.34", elem->Attr(cricket::QN_PREFERENCE));
  EXPECT_EQ("abcdef", elem->Attr(cricket::QN_USERNAME));
  EXPECT_EQ("ghijkl", elem->Attr(cricket::QN_PASSWORD));
  EXPECT_EQ("foo", elem->Attr(cricket::QN_TYPE));
  EXPECT_EQ("testnet", elem->Attr(cricket::QN_NETWORK));
  EXPECT_EQ("50", elem->Attr(cricket::QN_GENERATION));

  EXPECT_TRUE(parser.ParseGingleCandidate(elem.get(), &translator,
                                          &parsed_candidate, &parse_error));
  EXPECT_TRUE(test_candidate.IsEquivalent(parsed_candidate));

  EXPECT_TRUE(parser.WriteGingleCandidate(test_candidate2, &translator,
                                          elem.accept(), &write_error));
  EXPECT_EQ("test2", elem->Attr(buzz::QN_NAME));
  EXPECT_EQ("tcp", elem->Attr(cricket::QN_PROTOCOL));
  EXPECT_EQ("192.168.7.1", elem->Attr(cricket::QN_ADDRESS));
  EXPECT_EQ("9999", elem->Attr(cricket::QN_PORT));
  EXPECT_EQ("0.51", elem->Attr(cricket::QN_PREFERENCE));
  EXPECT_EQ("mnopqr", elem->Attr(cricket::QN_USERNAME));
  EXPECT_EQ("stuvwx", elem->Attr(cricket::QN_PASSWORD));
  EXPECT_EQ("bar", elem->Attr(cricket::QN_TYPE));
  EXPECT_EQ("testnet2", elem->Attr(cricket::QN_NETWORK));
  EXPECT_EQ("100", elem->Attr(cricket::QN_GENERATION));

  EXPECT_TRUE(parser.ParseGingleCandidate(elem.get(), &translator,
                                          &parsed_candidate, &parse_error));
  EXPECT_TRUE(test_candidate2.IsEquivalent(parsed_candidate));

  // Check that an ip is preferred over hostname.
  EXPECT_TRUE(parser.WriteGingleCandidate(test_candidate3, &translator,
                                          elem.accept(), &write_error));
  EXPECT_EQ("test3", elem->Attr(cricket::QN_NAME));
  EXPECT_EQ("spdy", elem->Attr(cricket::QN_PROTOCOL));
  EXPECT_EQ("10.0.0.1", elem->Attr(cricket::QN_ADDRESS));
  EXPECT_EQ("24601", elem->Attr(cricket::QN_PORT));
  EXPECT_EQ("0.69", elem->Attr(cricket::QN_PREFERENCE));
  EXPECT_EQ("yzabcd", elem->Attr(cricket::QN_USERNAME));
  EXPECT_EQ("efghij", elem->Attr(cricket::QN_PASSWORD));
  EXPECT_EQ("baz", elem->Attr(cricket::QN_TYPE));
  EXPECT_EQ("testnet3", elem->Attr(cricket::QN_NETWORK));
  EXPECT_EQ("150", elem->Attr(cricket::QN_GENERATION));

  EXPECT_TRUE(parser.ParseGingleCandidate(elem.get(), &translator,
                                          &parsed_candidate, &parse_error));
  EXPECT_TRUE(test_candidate3.IsEquivalent(parsed_candidate));
}
