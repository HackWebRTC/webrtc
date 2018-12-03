/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>
#include <vector>

#include "pc/sdpserializer.h"
#include "rtc_base/gunit.h"

using ::testing::ValuesIn;
using cricket::SimulcastDescription;
using cricket::SimulcastLayer;
using cricket::SimulcastLayerList;

namespace webrtc {

class SdpSerializerTest : public ::testing::TestWithParam<const char*> {
 public:
  // Runs a test for deserializing Simulcast.
  // |str| - The serialized Simulcast to parse.
  // |expected| - The expected output Simulcast to compare to.
  void TestSimulcastDeserialization(
      const std::string& str,
      const SimulcastDescription& expected) const {
    SdpSerializer deserializer;
    auto result = deserializer.DeserializeSimulcastDescription(str);
    EXPECT_TRUE(result.ok());
    ExpectEqual(expected, result.value());
  }

  // Runs a test for serializing Simulcast.
  // |simulcast| - The Simulcast to serialize.
  // |expected| - The expected output string to compare to.
  void TestSimulcastSerialization(const SimulcastDescription& simulcast,
                                  const std::string& expected) const {
    SdpSerializer serializer;
    auto result = serializer.SerializeSimulcastDescription(simulcast);
    EXPECT_EQ(expected, result);
  }

  // Checks that the two vectors of SimulcastLayer objects are equal.
  void ExpectEqual(const std::vector<SimulcastLayer>& expected,
                   const std::vector<SimulcastLayer>& actual) const {
    EXPECT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); i++) {
      EXPECT_EQ(expected[i].rid, actual[i].rid);
      EXPECT_EQ(expected[i].is_paused, actual[i].is_paused);
    }
  }

  // Checks that the two SimulcastLayerLists are equal.
  void ExpectEqual(const SimulcastLayerList& expected,
                   const SimulcastLayerList& actual) const {
    EXPECT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < expected.size(); i++) {
      ExpectEqual(expected[i], actual[i]);
    }
  }

  // Checks that the two SimulcastDescriptions are equal.
  void ExpectEqual(const SimulcastDescription& expected,
                   const SimulcastDescription& actual) const {
    ExpectEqual(expected.send_layers(), actual.send_layers());
    ExpectEqual(expected.receive_layers(), actual.receive_layers());
  }
};

// Test Cases

// Test simple deserialization with no alternative streams.
TEST_F(SdpSerializerTest, DeserializeSimulcast_SimpleCaseNoAlternatives) {
  std::string simulcast_str = "send 1;2 recv 3;4";
  SimulcastDescription expected;
  expected.send_layers().AddLayer(SimulcastLayer("1", false));
  expected.send_layers().AddLayer(SimulcastLayer("2", false));
  expected.receive_layers().AddLayer(SimulcastLayer("3", false));
  expected.receive_layers().AddLayer(SimulcastLayer("4", false));
  TestSimulcastDeserialization(simulcast_str, expected);
}

// Test simulcast deserialization with alternative streams.
TEST_F(SdpSerializerTest, DeserializeSimulcast_SimpleCaseWithAlternatives) {
  std::string simulcast_str = "send 1,5;2,6 recv 3,7;4,8";
  SimulcastDescription expected;
  expected.send_layers().AddLayerWithAlternatives(
      {SimulcastLayer("1", false), SimulcastLayer("5", false)});
  expected.send_layers().AddLayerWithAlternatives(
      {SimulcastLayer("2", false), SimulcastLayer("6", false)});
  expected.receive_layers().AddLayerWithAlternatives(
      {SimulcastLayer("3", false), SimulcastLayer("7", false)});
  expected.receive_layers().AddLayerWithAlternatives(
      {SimulcastLayer("4", false), SimulcastLayer("8", false)});
  TestSimulcastDeserialization(simulcast_str, expected);
}

// Test simulcast deserialization when only some streams have alternatives.
TEST_F(SdpSerializerTest, DeserializeSimulcast_WithSomeAlternatives) {
  std::string simulcast_str = "send 1;2,6 recv 3,7;4";
  SimulcastDescription expected;
  expected.send_layers().AddLayer(SimulcastLayer("1", false));
  expected.send_layers().AddLayerWithAlternatives(
      {SimulcastLayer("2", false), SimulcastLayer("6", false)});
  expected.receive_layers().AddLayerWithAlternatives(
      {SimulcastLayer("3", false), SimulcastLayer("7", false)});
  expected.receive_layers().AddLayer(SimulcastLayer("4", false));
  TestSimulcastDeserialization(simulcast_str, expected);
}

// Test simulcast deserialization when only send streams are specified.
TEST_F(SdpSerializerTest, DeserializeSimulcast_OnlySendStreams) {
  std::string simulcast_str = "send 1;2,6;3,7;4";
  SimulcastDescription expected;
  expected.send_layers().AddLayer(SimulcastLayer("1", false));
  expected.send_layers().AddLayerWithAlternatives(
      {SimulcastLayer("2", false), SimulcastLayer("6", false)});
  expected.send_layers().AddLayerWithAlternatives(
      {SimulcastLayer("3", false), SimulcastLayer("7", false)});
  expected.send_layers().AddLayer(SimulcastLayer("4", false));
  TestSimulcastDeserialization(simulcast_str, expected);
}

// Test simulcast deserialization when only receive streams are specified.
TEST_F(SdpSerializerTest, DeserializeSimulcast_OnlyReceiveStreams) {
  std::string simulcast_str = "recv 1;2,6;3,7;4";
  SimulcastDescription expected;
  expected.receive_layers().AddLayer(SimulcastLayer("1", false));
  expected.receive_layers().AddLayerWithAlternatives(
      {SimulcastLayer("2", false), SimulcastLayer("6", false)});
  expected.receive_layers().AddLayerWithAlternatives(
      {SimulcastLayer("3", false), SimulcastLayer("7", false)});
  expected.receive_layers().AddLayer(SimulcastLayer("4", false));
  TestSimulcastDeserialization(simulcast_str, expected);
}

// Test simulcast deserialization with receive streams before send streams.
TEST_F(SdpSerializerTest, DeserializeSimulcast_SendReceiveReversed) {
  std::string simulcast_str = "recv 1;2,6 send 3,7;4";
  SimulcastDescription expected;
  expected.receive_layers().AddLayer(SimulcastLayer("1", false));
  expected.receive_layers().AddLayerWithAlternatives(
      {SimulcastLayer("2", false), SimulcastLayer("6", false)});
  expected.send_layers().AddLayerWithAlternatives(
      {SimulcastLayer("3", false), SimulcastLayer("7", false)});
  expected.send_layers().AddLayer(SimulcastLayer("4", false));
  TestSimulcastDeserialization(simulcast_str, expected);
}

// Test simulcast deserialization with some streams set to paused state.
TEST_F(SdpSerializerTest, DeserializeSimulcast_PausedStreams) {
  std::string simulcast_str = "recv 1;~2,6 send 3,7;~4";
  SimulcastDescription expected;
  expected.receive_layers().AddLayer(SimulcastLayer("1", false));
  expected.receive_layers().AddLayerWithAlternatives(
      {SimulcastLayer("2", true), SimulcastLayer("6", false)});
  expected.send_layers().AddLayerWithAlternatives(
      {SimulcastLayer("3", false), SimulcastLayer("7", false)});
  expected.send_layers().AddLayer(SimulcastLayer("4", true));
  TestSimulcastDeserialization(simulcast_str, expected);
}

// Parameterized negative test case for deserialization with invalid inputs.
TEST_P(SdpSerializerTest, SimulcastDeserializationFailed) {
  SdpSerializer deserializer;
  auto result = deserializer.DeserializeSimulcastDescription(GetParam());
  EXPECT_FALSE(result.ok());
}

// The malformed Simulcast inputs to use in the negative test case.
const char* kSimulcastMalformedStrings[] = {
    "send ",
    "recv ",
    "recv 1 send",
    "receive 1",
    "recv 1;~2,6 recv 3,7;~4",
    "send 1;~2,6 send 3,7;~4",
    "send ~;~2,6",
    "send 1; ;~2,6",
    "send 1,;~2,6",
    "recv 1 send 2 3",
    "",
};

INSTANTIATE_TEST_CASE_P(SimulcastDeserializationErrors,
                        SdpSerializerTest,
                        ValuesIn(kSimulcastMalformedStrings));

// Test a simple serialization scenario.
TEST_F(SdpSerializerTest, SerializeSimulcast_SimpleCase) {
  SimulcastDescription simulcast;
  simulcast.send_layers().AddLayer(SimulcastLayer("1", false));
  simulcast.receive_layers().AddLayer(SimulcastLayer("2", false));
  TestSimulcastSerialization(simulcast, "send 1 recv 2");
}

// Test serialization with only send streams.
TEST_F(SdpSerializerTest, SerializeSimulcast_OnlySend) {
  SimulcastDescription simulcast;
  simulcast.send_layers().AddLayer(SimulcastLayer("1", false));
  simulcast.send_layers().AddLayer(SimulcastLayer("2", false));
  TestSimulcastSerialization(simulcast, "send 1;2");
}

// Test serialization with only receive streams
TEST_F(SdpSerializerTest, SerializeSimulcast_OnlyReceive) {
  SimulcastDescription simulcast;
  simulcast.receive_layers().AddLayer(SimulcastLayer("1", false));
  simulcast.receive_layers().AddLayer(SimulcastLayer("2", false));
  TestSimulcastSerialization(simulcast, "recv 1;2");
}

// Test a complex serialization with multiple streams, alternatives and states.
TEST_F(SdpSerializerTest, SerializeSimulcast_ComplexSerialization) {
  SimulcastDescription simulcast;
  simulcast.send_layers().AddLayerWithAlternatives(
      {SimulcastLayer("2", false), SimulcastLayer("1", true)});
  simulcast.send_layers().AddLayerWithAlternatives(
      {SimulcastLayer("4", false), SimulcastLayer("3", false)});

  simulcast.receive_layers().AddLayerWithAlternatives(
      {SimulcastLayer("6", false), SimulcastLayer("7", false)});
  simulcast.receive_layers().AddLayer(SimulcastLayer("8", true));
  simulcast.receive_layers().AddLayerWithAlternatives(
      {SimulcastLayer("9", false), SimulcastLayer("10", true),
       SimulcastLayer("11", false)});
  TestSimulcastSerialization(simulcast, "send 2,~1;4,3 recv 6,7;~8;9,~10,11");
}

}  // namespace webrtc
