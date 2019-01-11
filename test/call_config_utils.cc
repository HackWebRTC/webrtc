/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/call_config_utils.h"

#include <string>
#include <vector>

namespace webrtc {
namespace test {

// Deserializes a JSON representation of the VideoReceiveStream::Config back
// into a valid object. This will not initialize the decoders or the renderer.
VideoReceiveStream::Config ParseVideoReceiveStreamJsonConfig(
    webrtc::Transport* transport,
    const Json::Value& json) {
  auto receive_config = VideoReceiveStream::Config(transport);
  for (const auto decoder_json : json["decoders"]) {
    VideoReceiveStream::Decoder decoder;
    decoder.video_format =
        SdpVideoFormat(decoder_json["payload_name"].asString());
    decoder.payload_type = decoder_json["payload_type"].asInt64();
    for (const auto& params_json : decoder_json["codec_params"]) {
      std::vector<std::string> members = params_json.getMemberNames();
      RTC_CHECK_EQ(members.size(), 1);
      decoder.video_format.parameters[members[0]] =
          params_json[members[0]].asString();
    }
    receive_config.decoders.push_back(decoder);
  }
  receive_config.render_delay_ms = json["render_delay_ms"].asInt64();
  receive_config.target_delay_ms = json["target_delay_ms"].asInt64();
  receive_config.rtp.remote_ssrc = json["rtp"]["remote_ssrc"].asInt64();
  receive_config.rtp.local_ssrc = json["rtp"]["local_ssrc"].asInt64();
  receive_config.rtp.rtcp_mode =
      json["rtp"]["rtcp_mode"].asString() == "RtcpMode::kCompound"
          ? RtcpMode::kCompound
          : RtcpMode::kReducedSize;
  receive_config.rtp.remb = json["rtp"]["remb"].asBool();
  receive_config.rtp.transport_cc = json["rtp"]["transport_cc"].asBool();
  receive_config.rtp.nack.rtp_history_ms =
      json["rtp"]["nack"]["rtp_history_ms"].asInt64();
  receive_config.rtp.ulpfec_payload_type =
      json["rtp"]["ulpfec_payload_type"].asInt64();
  receive_config.rtp.red_payload_type =
      json["rtp"]["red_payload_type"].asInt64();
  receive_config.rtp.rtx_ssrc = json["rtp"]["rtx_ssrc"].asInt64();

  for (const auto& pl_json : json["rtp"]["rtx_payload_types"]) {
    std::vector<std::string> members = pl_json.getMemberNames();
    RTC_CHECK_EQ(members.size(), 1);
    Json::Value rtx_payload_type = pl_json[members[0]];
    receive_config.rtp.rtx_associated_payload_types[std::stoi(members[0])] =
        rtx_payload_type.asInt64();
  }
  for (const auto& ext_json : json["rtp"]["extensions"]) {
    receive_config.rtp.extensions.emplace_back(ext_json["uri"].asString(),
                                               ext_json["id"].asInt64(),
                                               ext_json["encrypt"].asBool());
  }
  return receive_config;
}

}  // namespace test.
}  // namespace webrtc.
