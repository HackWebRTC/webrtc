/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_TOOLS_EVENT_LOG_VISUALIZER_ANALYZER_H_
#define WEBRTC_TOOLS_EVENT_LOG_VISUALIZER_ANALYZER_H_

#include <vector>
#include <map>
#include <memory>
#include <utility>

#include "webrtc/call/rtc_event_log_parser.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtcp_packet.h"
#include "webrtc/tools/event_log_visualizer/plot_base.h"

namespace webrtc {
namespace plotting {

class EventLogAnalyzer {
 public:
  // The EventLogAnalyzer keeps a reference to the ParsedRtcEventLog for the
  // duration of its lifetime. The ParsedRtcEventLog must not be destroyed or
  // modified while the EventLogAnalyzer is being used.
  explicit EventLogAnalyzer(const ParsedRtcEventLog& log);

  void CreatePacketGraph(PacketDirection desired_direction, Plot* plot);

  void CreatePlayoutGraph(Plot* plot);

  void CreateSequenceNumberGraph(Plot* plot);

  void CreateDelayChangeGraph(Plot* plot);

  void CreateAccumulatedDelayChangeGraph(Plot* plot);

  void CreateFractionLossGraph(Plot* plot);

  void CreateTotalBitrateGraph(PacketDirection desired_direction, Plot* plot);

  void CreateStreamBitrateGraph(PacketDirection desired_direction, Plot* plot);

  void CreateBweGraph(Plot* plot);

  void CreateNetworkDelayFeebackGraph(Plot* plot);

 private:
  class StreamId {
   public:
    StreamId(uint32_t ssrc, webrtc::PacketDirection direction)
        : ssrc_(ssrc), direction_(direction) {}
    bool operator<(const StreamId& other) const;
    bool operator==(const StreamId& other) const;
    uint32_t GetSsrc() const { return ssrc_; }
    webrtc::PacketDirection GetDirection() const { return direction_; }

   private:
    uint32_t ssrc_;
    webrtc::PacketDirection direction_;
  };

  struct LoggedRtpPacket {
    LoggedRtpPacket(uint64_t timestamp, RTPHeader header, size_t total_length)
        : timestamp(timestamp), header(header), total_length(total_length) {}
    uint64_t timestamp;
    RTPHeader header;
    size_t total_length;
  };

  struct LoggedRtcpPacket {
    LoggedRtcpPacket(uint64_t timestamp,
                     RTCPPacketType rtcp_type,
                     std::unique_ptr<rtcp::RtcpPacket> rtcp_packet)
        : timestamp(timestamp),
          type(rtcp_type),
          packet(std::move(rtcp_packet)) {}
    uint64_t timestamp;
    RTCPPacketType type;
    std::unique_ptr<rtcp::RtcpPacket> packet;
  };

  struct BwePacketLossEvent {
    uint64_t timestamp;
    int32_t new_bitrate;
    uint8_t fraction_loss;
    int32_t expected_packets;
  };

  const ParsedRtcEventLog& parsed_log_;

  // A list of SSRCs we are interested in analysing.
  // If left empty, all SSRCs will be considered relevant.
  std::vector<uint32_t> desired_ssrc_;

  // Maps a stream identifier consisting of ssrc, direction and MediaType
  // to the parsed RTP headers in that stream. Header extensions are parsed
  // if the stream has been configured.
  std::map<StreamId, std::vector<LoggedRtpPacket>> rtp_packets_;

  std::map<StreamId, std::vector<LoggedRtcpPacket>> rtcp_packets_;

  // A list of all updates from the send-side loss-based bandwidth estimator.
  std::vector<BwePacketLossEvent> bwe_loss_updates_;

  // Window and step size used for calculating moving averages, e.g. bitrate.
  // The generated data points will be |step_| microseconds apart.
  // Only events occuring at most |window_duration_| microseconds before the
  // current data point will be part of the average.
  uint64_t window_duration_;
  uint64_t step_;

  // First and last events of the log.
  uint64_t begin_time_;
  uint64_t end_time_;

  // Duration (in seconds) of log file.
  float call_duration_s_;
};

}  // namespace plotting
}  // namespace webrtc

#endif  // WEBRTC_TOOLS_EVENT_LOG_VISUALIZER_ANALYZER_H_
