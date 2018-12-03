/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/sdpserializer.h"

#include <string>
#include <utility>
#include <vector>

#include "api/jsep.h"
#include "rtc_base/strings/string_builder.h"

using cricket::SimulcastDescription;
using cricket::SimulcastLayer;
using cricket::SimulcastLayerList;

namespace webrtc {

namespace {

// delimiters
const char kDelimiterComma[] = ",";
const char kDelimiterCommaChar = ',';
const char kDelimiterSemicolon[] = ";";
const char kDelimiterSemicolonChar = ';';
const char kDelimiterSpace[] = " ";
const char kDelimiterSpaceChar = ' ';

// https://tools.ietf.org/html/draft-ietf-mmusic-sdp-simulcast-13#section-5.1
const char kSimulcastPausedStream[] = "~";
const char kSimulcastPausedStreamChar = '~';
const char kSimulcastSendStreams[] = "send";
const char kSimulcastReceiveStreams[] = "recv";

RTCError ParseError(const std::string& message) {
  return RTCError(RTCErrorType::SYNTAX_ERROR, message);
}

// These methods serialize simulcast according to the specification:
// https://tools.ietf.org/html/draft-ietf-mmusic-sdp-simulcast-13#section-5.1
rtc::StringBuilder& operator<<(rtc::StringBuilder& builder,
                               const SimulcastLayer& simulcast_layer) {
  if (simulcast_layer.is_paused) {
    builder << kSimulcastPausedStream;
  }
  builder << simulcast_layer.rid;
  return builder;
}

rtc::StringBuilder& operator<<(
    rtc::StringBuilder& builder,
    const std::vector<SimulcastLayer>& layer_alternatives) {
  bool first = true;
  for (const SimulcastLayer& rid : layer_alternatives) {
    if (!first) {
      builder << kDelimiterComma;
    }
    builder << rid;
    first = false;
  }
  return builder;
}

rtc::StringBuilder& operator<<(rtc::StringBuilder& builder,
                               const SimulcastLayerList& simulcast_layers) {
  bool first = true;
  for (auto alternatives : simulcast_layers) {
    if (!first) {
      builder << kDelimiterSemicolon;
    }
    builder << alternatives;
    first = false;
  }
  return builder;
}

// These methods deserialize simulcast according to the specification:
// https://tools.ietf.org/html/draft-ietf-mmusic-sdp-simulcast-13#section-5.1
// sc-str-list  = sc-alt-list *( ";" sc-alt-list )
// sc-alt-list  = sc-id *( "," sc-id )
// sc-id-paused = "~"
// sc-id        = [sc-id-paused] rid-id
// rid-id       = 1*(alpha-numeric / "-" / "_") ; see: I-D.ietf-mmusic-rid
RTCErrorOr<SimulcastLayerList> ParseSimulcastLayerList(const std::string& str) {
  std::vector<std::string> tokens;
  rtc::tokenize_with_empty_tokens(str, kDelimiterSemicolonChar, &tokens);
  if (tokens.empty()) {
    return ParseError("Layer list cannot be empty.");
  }

  SimulcastLayerList result;
  for (const std::string& token : tokens) {
    if (token.empty()) {
      return ParseError("Simulcast alternative layer list is empty.");
    }

    std::vector<std::string> rid_tokens;
    rtc::tokenize_with_empty_tokens(token, kDelimiterCommaChar, &rid_tokens);
    if (rid_tokens.empty()) {
      return ParseError("Simulcast alternative layer list is malformed.");
    }

    std::vector<SimulcastLayer> layers;
    for (const auto& rid_token : rid_tokens) {
      if (rid_token.empty() || rid_token == kSimulcastPausedStream) {
        return ParseError("Rid must not be empty.");
      }

      bool paused = rid_token[0] == kSimulcastPausedStreamChar;
      std::string rid = paused ? rid_token.substr(1) : rid_token;

      // TODO(amithi, bugs.webrtc.org/10073):
      // Validate the rid format.
      // See also: https://github.com/w3c/webrtc-pc/issues/2013
      layers.push_back(SimulcastLayer(rid, paused));
    }

    result.AddLayerWithAlternatives(layers);
  }

  return std::move(result);
}

}  // namespace

std::string SdpSerializer::SerializeSimulcastDescription(
    const cricket::SimulcastDescription& simulcast) const {
  rtc::StringBuilder sb;
  std::string delimiter;

  if (!simulcast.send_layers().empty()) {
    sb << kSimulcastSendStreams << kDelimiterSpace << simulcast.send_layers();
    delimiter = kDelimiterSpace;
  }

  if (!simulcast.receive_layers().empty()) {
    sb << delimiter << kSimulcastReceiveStreams << kDelimiterSpace
       << simulcast.receive_layers();
  }

  return sb.str();
}

// https://tools.ietf.org/html/draft-ietf-mmusic-sdp-simulcast-13#section-5.1
// a:simulcast:<send> <streams> <recv> <streams>
// Formal Grammar
// sc-value     = ( sc-send [SP sc-recv] ) / ( sc-recv [SP sc-send] )
// sc-send      = %s"send" SP sc-str-list
// sc-recv      = %s"recv" SP sc-str-list
// sc-str-list  = sc-alt-list *( ";" sc-alt-list )
// sc-alt-list  = sc-id *( "," sc-id )
// sc-id-paused = "~"
// sc-id        = [sc-id-paused] rid-id
// rid-id       = 1*(alpha-numeric / "-" / "_") ; see: I-D.ietf-mmusic-rid
RTCErrorOr<SimulcastDescription> SdpSerializer::DeserializeSimulcastDescription(
    absl::string_view string) const {
  std::vector<std::string> tokens;
  rtc::tokenize(std::string(string), kDelimiterSpaceChar, &tokens);

  if (tokens.size() != 2 && tokens.size() != 4) {
    return ParseError("Must have one or two <direction, streams> pairs.");
  }

  bool bidirectional = tokens.size() == 4;  // indicates both send and recv

  // Tokens 0, 2 (if exists) should be send / recv
  if ((tokens[0] != kSimulcastSendStreams &&
       tokens[0] != kSimulcastReceiveStreams) ||
      (bidirectional && tokens[2] != kSimulcastSendStreams &&
       tokens[2] != kSimulcastReceiveStreams) ||
      (bidirectional && tokens[0] == tokens[2])) {
    return ParseError("Valid values: send / recv.");
  }

  // Tokens 1, 3 (if exists) should be alternative layer lists
  RTCErrorOr<SimulcastLayerList> list1, list2;
  list1 = ParseSimulcastLayerList(tokens[1]);
  if (!list1.ok()) {
    return list1.MoveError();
  }

  if (bidirectional) {
    list2 = ParseSimulcastLayerList(tokens[3]);
    if (!list2.ok()) {
      return list2.MoveError();
    }
  }

  // Set the layers so that list1 is for send and list2 is for recv
  if (tokens[0] != kSimulcastSendStreams) {
    std::swap(list1, list2);
  }

  // Set the layers according to which pair is send and which is recv
  // At this point if the simulcast is unidirectional then
  // either |list1| or |list2| will be in 'error' state indicating that
  // the value should not be used.
  SimulcastDescription simulcast;
  if (list1.ok()) {
    simulcast.send_layers() = list1.MoveValue();
  }

  if (list2.ok()) {
    simulcast.receive_layers() = list2.MoveValue();
  }

  return std::move(simulcast);
}

}  // namespace webrtc
