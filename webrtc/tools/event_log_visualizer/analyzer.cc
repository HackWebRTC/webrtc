/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/tools/event_log_visualizer/analyzer.h"

#include <algorithm>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>

#include "webrtc/audio_receive_stream.h"
#include "webrtc/audio_send_stream.h"
#include "webrtc/base/checks.h"
#include "webrtc/call.h"
#include "webrtc/common_types.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp.h"
#include "webrtc/modules/rtp_rtcp/include/rtp_rtcp_defines.h"
#include "webrtc/modules/rtp_rtcp/source/rtp_utility.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace {

std::string SsrcToString(uint32_t ssrc) {
  std::stringstream ss;
  ss << "SSRC " << ssrc;
  return ss.str();
}

// Checks whether an SSRC is contained in the list of desired SSRCs.
// Note that an empty SSRC list matches every SSRC.
bool MatchingSsrc(uint32_t ssrc, const std::vector<uint32_t>& desired_ssrc) {
  if (desired_ssrc.size() == 0)
    return true;
  return std::find(desired_ssrc.begin(), desired_ssrc.end(), ssrc) !=
         desired_ssrc.end();
}

double AbsSendTimeToMicroseconds(int64_t abs_send_time) {
  // The timestamp is a fixed point representation with 6 bits for seconds
  // and 18 bits for fractions of a second. Thus, we divide by 2^18 to get the
  // time in seconds and then multiply by 1000000 to convert to microseconds.
  static constexpr double kTimestampToMicroSec =
      1000000.0 / static_cast<double>(1 << 18);
  return abs_send_time * kTimestampToMicroSec;
}

// Computes the difference |later| - |earlier| where |later| and |earlier|
// are counters that wrap at |modulus|. The difference is chosen to have the
// least absolute value. For example if |modulus| is 8, then the difference will
// be chosen in the range [-3, 4]. If |modulus| is 9, then the difference will
// be in [-4, 4].
int64_t WrappingDifference(uint32_t later, uint32_t earlier, int64_t modulus) {
  RTC_DCHECK_LE(1, modulus);
  RTC_DCHECK_LT(later, modulus);
  RTC_DCHECK_LT(earlier, modulus);
  int64_t difference =
      static_cast<int64_t>(later) - static_cast<int64_t>(earlier);
  int64_t max_difference = modulus / 2;
  int64_t min_difference = max_difference - modulus + 1;
  if (difference > max_difference) {
    difference -= modulus;
  }
  if (difference < min_difference) {
    difference += modulus;
  }
  return difference;
}

const double kXMargin = 1.02;
const double kYMargin = 1.1;
const double kDefaultXMin = -1;
const double kDefaultYMin = -1;

}  // namespace

namespace webrtc {
namespace plotting {


bool EventLogAnalyzer::StreamId::operator<(const StreamId& other) const {
  if (ssrc_ < other.ssrc_) {
    return true;
  }
  if (ssrc_ == other.ssrc_) {
    if (media_type_ < other.media_type_) {
      return true;
    }
    if (media_type_ == other.media_type_) {
      if (direction_ < other.direction_) {
        return true;
      }
    }
  }
  return false;
}

bool EventLogAnalyzer::StreamId::operator==(const StreamId& other) const {
  return ssrc_ == other.ssrc_ && direction_ == other.direction_ &&
         media_type_ == other.media_type_;
}


EventLogAnalyzer::EventLogAnalyzer(const ParsedRtcEventLog& log)
    : parsed_log_(log), window_duration_(250000), step_(10000) {
  uint64_t first_timestamp = std::numeric_limits<uint64_t>::max();
  uint64_t last_timestamp = std::numeric_limits<uint64_t>::min();

  // Maps a stream identifier consisting of ssrc, direction and MediaType
  // to the header extensions used by that stream,
  std::map<StreamId, RtpHeaderExtensionMap> extension_maps;

  PacketDirection direction;
  MediaType media_type;
  uint8_t header[IP_PACKET_SIZE];
  size_t header_length;
  size_t total_length;

  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type != ParsedRtcEventLog::VIDEO_RECEIVER_CONFIG_EVENT &&
        event_type != ParsedRtcEventLog::VIDEO_SENDER_CONFIG_EVENT &&
        event_type != ParsedRtcEventLog::AUDIO_RECEIVER_CONFIG_EVENT &&
        event_type != ParsedRtcEventLog::AUDIO_SENDER_CONFIG_EVENT) {
      uint64_t timestamp = parsed_log_.GetTimestamp(i);
      first_timestamp = std::min(first_timestamp, timestamp);
      last_timestamp = std::max(last_timestamp, timestamp);
    }

    switch (parsed_log_.GetEventType(i)) {
      case ParsedRtcEventLog::VIDEO_RECEIVER_CONFIG_EVENT: {
        VideoReceiveStream::Config config(nullptr);
        parsed_log_.GetVideoReceiveConfig(i, &config);
        StreamId stream(config.rtp.remote_ssrc, kIncomingPacket,
                        MediaType::VIDEO);
        extension_maps[stream].Erase();
        for (size_t j = 0; j < config.rtp.extensions.size(); ++j) {
          const std::string& extension = config.rtp.extensions[j].uri;
          int id = config.rtp.extensions[j].id;
          extension_maps[stream].Register(StringToRtpExtensionType(extension),
                                          id);
        }
        break;
      }
      case ParsedRtcEventLog::VIDEO_SENDER_CONFIG_EVENT: {
        VideoSendStream::Config config(nullptr);
        parsed_log_.GetVideoSendConfig(i, &config);
        for (auto ssrc : config.rtp.ssrcs) {
          StreamId stream(ssrc, kOutgoingPacket, MediaType::VIDEO);
          extension_maps[stream].Erase();
          for (size_t j = 0; j < config.rtp.extensions.size(); ++j) {
            const std::string& extension = config.rtp.extensions[j].uri;
            int id = config.rtp.extensions[j].id;
            extension_maps[stream].Register(StringToRtpExtensionType(extension),
                                            id);
          }
        }
        break;
      }
      case ParsedRtcEventLog::AUDIO_RECEIVER_CONFIG_EVENT: {
        AudioReceiveStream::Config config;
        // TODO(terelius): Parse the audio configs once we have them.
        break;
      }
      case ParsedRtcEventLog::AUDIO_SENDER_CONFIG_EVENT: {
        AudioSendStream::Config config(nullptr);
        // TODO(terelius): Parse the audio configs once we have them.
        break;
      }
      case ParsedRtcEventLog::RTP_EVENT: {
        parsed_log_.GetRtpHeader(i, &direction, &media_type, header,
                                 &header_length, &total_length);
        // Parse header to get SSRC.
        RtpUtility::RtpHeaderParser rtp_parser(header, header_length);
        RTPHeader parsed_header;
        rtp_parser.Parse(&parsed_header);
        StreamId stream(parsed_header.ssrc, direction, media_type);
        // Look up the extension_map and parse it again to get the extensions.
        if (extension_maps.count(stream) == 1) {
          RtpHeaderExtensionMap* extension_map = &extension_maps[stream];
          rtp_parser.Parse(&parsed_header, extension_map);
        }
        uint64_t timestamp = parsed_log_.GetTimestamp(i);
        rtp_packets_[stream].push_back(
            LoggedRtpPacket(timestamp, parsed_header));
        break;
      }
      case ParsedRtcEventLog::RTCP_EVENT: {
        break;
      }
      case ParsedRtcEventLog::LOG_START: {
        break;
      }
      case ParsedRtcEventLog::LOG_END: {
        break;
      }
      case ParsedRtcEventLog::BWE_PACKET_LOSS_EVENT: {
        BwePacketLossEvent bwe_update;
        bwe_update.timestamp = parsed_log_.GetTimestamp(i);
        parsed_log_.GetBwePacketLossEvent(i, &bwe_update.new_bitrate,
                                             &bwe_update.fraction_loss,
                                             &bwe_update.expected_packets);
        bwe_loss_updates_.push_back(bwe_update);
        break;
      }
      case ParsedRtcEventLog::BWE_PACKET_DELAY_EVENT: {
        break;
      }
      case ParsedRtcEventLog::AUDIO_PLAYOUT_EVENT: {
        break;
      }
      case ParsedRtcEventLog::UNKNOWN_EVENT: {
        break;
      }
    }
  }

  if (last_timestamp < first_timestamp) {
    // No useful events in the log.
    first_timestamp = last_timestamp = 0;
  }
  begin_time_ = first_timestamp;
  end_time_ = last_timestamp;
}

void EventLogAnalyzer::CreatePacketGraph(PacketDirection desired_direction,
                                         Plot* plot) {
  std::map<uint32_t, TimeSeries> time_series;

  PacketDirection direction;
  MediaType media_type;
  uint8_t header[IP_PACKET_SIZE];
  size_t header_length, total_length;
  float max_y = 0;

  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type == ParsedRtcEventLog::RTP_EVENT) {
      parsed_log_.GetRtpHeader(i, &direction, &media_type, header,
                               &header_length, &total_length);
      if (direction == desired_direction) {
        // Parse header to get SSRC.
        RtpUtility::RtpHeaderParser rtp_parser(header, header_length);
        RTPHeader parsed_header;
        rtp_parser.Parse(&parsed_header);
        // Filter on SSRC.
        if (MatchingSsrc(parsed_header.ssrc, desired_ssrc_)) {
          uint64_t timestamp = parsed_log_.GetTimestamp(i);
          float x = static_cast<float>(timestamp - begin_time_) / 1000000;
          float y = total_length;
          max_y = std::max(max_y, y);
          time_series[parsed_header.ssrc].points.push_back(
              TimeSeriesPoint(x, y));
        }
      }
    }
  }

  // Set labels and put in graph.
  for (auto& kv : time_series) {
    kv.second.label = SsrcToString(kv.first);
    kv.second.style = BAR_GRAPH;
    plot->series.push_back(std::move(kv.second));
  }

  plot->xaxis_min = kDefaultXMin;
  plot->xaxis_max = (end_time_ - begin_time_) / 1000000 * kXMargin;
  plot->xaxis_label = "Time (s)";
  plot->yaxis_min = kDefaultYMin;
  plot->yaxis_max = max_y * kYMargin;
  plot->yaxis_label = "Packet size (bytes)";
  if (desired_direction == webrtc::PacketDirection::kIncomingPacket) {
    plot->title = "Incoming RTP packets";
  } else if (desired_direction == webrtc::PacketDirection::kOutgoingPacket) {
    plot->title = "Outgoing RTP packets";
  }
}

// For each SSRC, plot the time between the consecutive playouts.
void EventLogAnalyzer::CreatePlayoutGraph(Plot* plot) {
  std::map<uint32_t, TimeSeries> time_series;
  std::map<uint32_t, uint64_t> last_playout;

  uint32_t ssrc;
  float max_y = 0;

  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type == ParsedRtcEventLog::AUDIO_PLAYOUT_EVENT) {
      parsed_log_.GetAudioPlayout(i, &ssrc);
      uint64_t timestamp = parsed_log_.GetTimestamp(i);
      if (MatchingSsrc(ssrc, desired_ssrc_)) {
        float x = static_cast<float>(timestamp - begin_time_) / 1000000;
        float y = static_cast<float>(timestamp - last_playout[ssrc]) / 1000;
        if (time_series[ssrc].points.size() == 0) {
          // There were no previusly logged playout for this SSRC.
          // Generate a point, but place it on the x-axis.
          y = 0;
        }
        max_y = std::max(max_y, y);
        time_series[ssrc].points.push_back(TimeSeriesPoint(x, y));
        last_playout[ssrc] = timestamp;
      }
    }
  }

  // Set labels and put in graph.
  for (auto& kv : time_series) {
    kv.second.label = SsrcToString(kv.first);
    kv.second.style = BAR_GRAPH;
    plot->series.push_back(std::move(kv.second));
  }

  plot->xaxis_min = kDefaultXMin;
  plot->xaxis_max = (end_time_ - begin_time_) / 1000000 * kXMargin;
  plot->xaxis_label = "Time (s)";
  plot->yaxis_min = kDefaultYMin;
  plot->yaxis_max = max_y * kYMargin;
  plot->yaxis_label = "Time since last playout (ms)";
  plot->title = "Audio playout";
}

// For each SSRC, plot the time between the consecutive playouts.
void EventLogAnalyzer::CreateSequenceNumberGraph(Plot* plot) {
  std::map<uint32_t, TimeSeries> time_series;
  std::map<uint32_t, uint16_t> last_seqno;

  PacketDirection direction;
  MediaType media_type;
  uint8_t header[IP_PACKET_SIZE];
  size_t header_length, total_length;

  int max_y = 1;
  int min_y = 0;

  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type == ParsedRtcEventLog::RTP_EVENT) {
      parsed_log_.GetRtpHeader(i, &direction, &media_type, header,
                               &header_length, &total_length);
      uint64_t timestamp = parsed_log_.GetTimestamp(i);
      if (direction == PacketDirection::kIncomingPacket) {
        // Parse header to get SSRC.
        RtpUtility::RtpHeaderParser rtp_parser(header, header_length);
        RTPHeader parsed_header;
        rtp_parser.Parse(&parsed_header);
        // Filter on SSRC.
        if (MatchingSsrc(parsed_header.ssrc, desired_ssrc_)) {
          float x = static_cast<float>(timestamp - begin_time_) / 1000000;
          int y = WrappingDifference(parsed_header.sequenceNumber,
                                     last_seqno[parsed_header.ssrc], 1ul << 16);
          if (time_series[parsed_header.ssrc].points.size() == 0) {
            // There were no previusly logged playout for this SSRC.
            // Generate a point, but place it on the x-axis.
            y = 0;
          }
          max_y = std::max(max_y, y);
          min_y = std::min(min_y, y);
          time_series[parsed_header.ssrc].points.push_back(
              TimeSeriesPoint(x, y));
          last_seqno[parsed_header.ssrc] = parsed_header.sequenceNumber;
        }
      }
    }
  }

  // Set labels and put in graph.
  for (auto& kv : time_series) {
    kv.second.label = SsrcToString(kv.first);
    kv.second.style = BAR_GRAPH;
    plot->series.push_back(std::move(kv.second));
  }

  plot->xaxis_min = kDefaultXMin;
  plot->xaxis_max = (end_time_ - begin_time_) / 1000000 * kXMargin;
  plot->xaxis_label = "Time (s)";
  plot->yaxis_min = min_y - (kYMargin - 1) / 2 * (max_y - min_y);
  plot->yaxis_max = max_y + (kYMargin - 1) / 2 * (max_y - min_y);
  plot->yaxis_label = "Difference since last packet";
  plot->title = "Sequence number";
}

void EventLogAnalyzer::CreateDelayChangeGraph(Plot* plot) {
  double max_y = 10;
  double min_y = 0;

  for (auto& kv : rtp_packets_) {
    StreamId stream_id = kv.first;
    // Filter on direction and SSRC.
    if (stream_id.GetDirection() != kIncomingPacket ||
        !MatchingSsrc(stream_id.GetSsrc(), desired_ssrc_)) {
      continue;
    }

    TimeSeries time_series;
    time_series.label = SsrcToString(stream_id.GetSsrc());
    time_series.style = BAR_GRAPH;
    const std::vector<LoggedRtpPacket>& packet_stream = kv.second;
    int64_t last_abs_send_time = 0;
    int64_t last_timestamp = 0;
    for (const LoggedRtpPacket& packet : packet_stream) {
      if (packet.header.extension.hasAbsoluteSendTime) {
        int64_t send_time_diff =
            WrappingDifference(packet.header.extension.absoluteSendTime,
                               last_abs_send_time, 1ul << 24);
        int64_t recv_time_diff = packet.timestamp - last_timestamp;

        last_abs_send_time = packet.header.extension.absoluteSendTime;
        last_timestamp = packet.timestamp;

        float x = static_cast<float>(packet.timestamp - begin_time_) / 1000000;
        double y =
            static_cast<double>(recv_time_diff -
                                AbsSendTimeToMicroseconds(send_time_diff)) /
            1000;
        if (time_series.points.size() == 0) {
          // There were no previously logged packets for this SSRC.
          // Generate a point, but place it on the x-axis.
          y = 0;
        }
        max_y = std::max(max_y, y);
        min_y = std::min(min_y, y);
        time_series.points.emplace_back(x, y);
      }
    }
    // Add the data set to the plot.
    plot->series.push_back(std::move(time_series));
  }

  plot->xaxis_min = kDefaultXMin;
  plot->xaxis_max = (end_time_ - begin_time_) / 1000000 * kXMargin;
  plot->xaxis_label = "Time (s)";
  plot->yaxis_min = min_y - (kYMargin - 1) / 2 * (max_y - min_y);
  plot->yaxis_max = max_y + (kYMargin - 1) / 2 * (max_y - min_y);
  plot->yaxis_label = "Latency change (ms)";
  plot->title = "Network latency change between consecutive packets";
}

void EventLogAnalyzer::CreateAccumulatedDelayChangeGraph(Plot* plot) {
  double max_y = 10;
  double min_y = 0;

  for (auto& kv : rtp_packets_) {
    StreamId stream_id = kv.first;
    // Filter on direction and SSRC.
    if (stream_id.GetDirection() != kIncomingPacket ||
        !MatchingSsrc(stream_id.GetSsrc(), desired_ssrc_)) {
      continue;
    }
    TimeSeries time_series;
    time_series.label = SsrcToString(stream_id.GetSsrc());
    time_series.style = LINE_GRAPH;
    const std::vector<LoggedRtpPacket>& packet_stream = kv.second;
    int64_t last_abs_send_time = 0;
    int64_t last_timestamp = 0;
    double accumulated_delay_ms = 0;
    for (const LoggedRtpPacket& packet : packet_stream) {
      if (packet.header.extension.hasAbsoluteSendTime) {
        int64_t send_time_diff =
            WrappingDifference(packet.header.extension.absoluteSendTime,
                               last_abs_send_time, 1ul << 24);
        int64_t recv_time_diff = packet.timestamp - last_timestamp;

        last_abs_send_time = packet.header.extension.absoluteSendTime;
        last_timestamp = packet.timestamp;

        float x = static_cast<float>(packet.timestamp - begin_time_) / 1000000;
        accumulated_delay_ms +=
            static_cast<double>(recv_time_diff -
                                AbsSendTimeToMicroseconds(send_time_diff)) /
            1000;
        if (time_series.points.size() == 0) {
          // There were no previously logged packets for this SSRC.
          // Generate a point, but place it on the x-axis.
          accumulated_delay_ms = 0;
        }
        max_y = std::max(max_y, accumulated_delay_ms);
        min_y = std::min(min_y, accumulated_delay_ms);
        time_series.points.emplace_back(x, accumulated_delay_ms);
      }
    }
    // Add the data set to the plot.
    plot->series.push_back(std::move(time_series));
  }

  plot->xaxis_min = kDefaultXMin;
  plot->xaxis_max = (end_time_ - begin_time_) / 1000000 * kXMargin;
  plot->xaxis_label = "Time (s)";
  plot->yaxis_min = min_y - (kYMargin - 1) / 2 * (max_y - min_y);
  plot->yaxis_max = max_y + (kYMargin - 1) / 2 * (max_y - min_y);
  plot->yaxis_label = "Latency change (ms)";
  plot->title = "Accumulated network latency change";
}

// Plot the total bandwidth used by all RTP streams.
void EventLogAnalyzer::CreateTotalBitrateGraph(
    PacketDirection desired_direction,
    Plot* plot) {
  struct TimestampSize {
    TimestampSize(uint64_t t, size_t s) : timestamp(t), size(s) {}
    uint64_t timestamp;
    size_t size;
  };
  std::vector<TimestampSize> packets;

  PacketDirection direction;
  size_t total_length;

  // Extract timestamps and sizes for the relevant packets.
  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type == ParsedRtcEventLog::RTP_EVENT) {
      parsed_log_.GetRtpHeader(i, &direction, nullptr, nullptr, nullptr,
                               &total_length);
      if (direction == desired_direction) {
        uint64_t timestamp = parsed_log_.GetTimestamp(i);
        packets.push_back(TimestampSize(timestamp, total_length));
      }
    }
  }

  size_t window_index_begin = 0;
  size_t window_index_end = 0;
  size_t bytes_in_window = 0;
  float max_y = 0;

  // Calculate a moving average of the bitrate and store in a TimeSeries.
  plot->series.push_back(TimeSeries());
  for (uint64_t time = begin_time_; time < end_time_ + step_; time += step_) {
    while (window_index_end < packets.size() &&
           packets[window_index_end].timestamp < time) {
      bytes_in_window += packets[window_index_end].size;
      window_index_end++;
    }
    while (window_index_begin < packets.size() &&
           packets[window_index_begin].timestamp < time - window_duration_) {
      RTC_DCHECK_LE(packets[window_index_begin].size, bytes_in_window);
      bytes_in_window -= packets[window_index_begin].size;
      window_index_begin++;
    }
    float window_duration_in_seconds =
        static_cast<float>(window_duration_) / 1000000;
    float x = static_cast<float>(time - begin_time_) / 1000000;
    float y = bytes_in_window * 8 / window_duration_in_seconds / 1000;
    max_y = std::max(max_y, y);
    plot->series.back().points.push_back(TimeSeriesPoint(x, y));
  }

  // Set labels.
  if (desired_direction == webrtc::PacketDirection::kIncomingPacket) {
    plot->series.back().label = "Incoming bitrate";
  } else if (desired_direction == webrtc::PacketDirection::kOutgoingPacket) {
    plot->series.back().label = "Outgoing bitrate";
  }
  plot->series.back().style = LINE_GRAPH;

  // Overlay the send-side bandwidth estimate over the outgoing bitrate.
  if (desired_direction == kOutgoingPacket) {
    plot->series.push_back(TimeSeries());
    for (auto& bwe_update : bwe_loss_updates_) {
      float x =
          static_cast<float>(bwe_update.timestamp - begin_time_) / 1000000;
      float y = static_cast<float>(bwe_update.new_bitrate) / 1000;
      max_y = std::max(max_y, y);
      plot->series.back().points.emplace_back(x, y);
    }
    plot->series.back().label = "Loss-based estimate";
    plot->series.back().style = LINE_GRAPH;
  }

  plot->xaxis_min = kDefaultXMin;
  plot->xaxis_max = (end_time_ - begin_time_) / 1000000 * kXMargin;
  plot->xaxis_label = "Time (s)";
  plot->yaxis_min = kDefaultYMin;
  plot->yaxis_max = max_y * kYMargin;
  plot->yaxis_label = "Bitrate (kbps)";
  if (desired_direction == webrtc::PacketDirection::kIncomingPacket) {
    plot->title = "Incoming RTP bitrate";
  } else if (desired_direction == webrtc::PacketDirection::kOutgoingPacket) {
    plot->title = "Outgoing RTP bitrate";
  }
}

// For each SSRC, plot the bandwidth used by that stream.
void EventLogAnalyzer::CreateStreamBitrateGraph(
    PacketDirection desired_direction,
    Plot* plot) {
  struct TimestampSize {
    TimestampSize(uint64_t t, size_t s) : timestamp(t), size(s) {}
    uint64_t timestamp;
    size_t size;
  };
  std::map<uint32_t, std::vector<TimestampSize>> packets;

  PacketDirection direction;
  MediaType media_type;
  uint8_t header[IP_PACKET_SIZE];
  size_t header_length, total_length;

  // Extract timestamps and sizes for the relevant packets.
  for (size_t i = 0; i < parsed_log_.GetNumberOfEvents(); i++) {
    ParsedRtcEventLog::EventType event_type = parsed_log_.GetEventType(i);
    if (event_type == ParsedRtcEventLog::RTP_EVENT) {
      parsed_log_.GetRtpHeader(i, &direction, &media_type, header,
                               &header_length, &total_length);
      if (direction == desired_direction) {
        // Parse header to get SSRC.
        RtpUtility::RtpHeaderParser rtp_parser(header, header_length);
        RTPHeader parsed_header;
        rtp_parser.Parse(&parsed_header);
        // Filter on SSRC.
        if (MatchingSsrc(parsed_header.ssrc, desired_ssrc_)) {
          uint64_t timestamp = parsed_log_.GetTimestamp(i);
          packets[parsed_header.ssrc].push_back(
              TimestampSize(timestamp, total_length));
        }
      }
    }
  }

  float max_y = 0;

  for (auto& kv : packets) {
    size_t window_index_begin = 0;
    size_t window_index_end = 0;
    size_t bytes_in_window = 0;

    // Calculate a moving average of the bitrate and store in a TimeSeries.
    plot->series.push_back(TimeSeries());
    for (uint64_t time = begin_time_; time < end_time_ + step_; time += step_) {
      while (window_index_end < kv.second.size() &&
             kv.second[window_index_end].timestamp < time) {
        bytes_in_window += kv.second[window_index_end].size;
        window_index_end++;
      }
      while (window_index_begin < kv.second.size() &&
             kv.second[window_index_begin].timestamp <
                 time - window_duration_) {
        RTC_DCHECK_LE(kv.second[window_index_begin].size, bytes_in_window);
        bytes_in_window -= kv.second[window_index_begin].size;
        window_index_begin++;
      }
      float window_duration_in_seconds =
          static_cast<float>(window_duration_) / 1000000;
      float x = static_cast<float>(time - begin_time_) / 1000000;
      float y = bytes_in_window * 8 / window_duration_in_seconds / 1000;
      max_y = std::max(max_y, y);
      plot->series.back().points.push_back(TimeSeriesPoint(x, y));
    }

    // Set labels.
    plot->series.back().label = SsrcToString(kv.first);
    plot->series.back().style = LINE_GRAPH;
  }

  plot->xaxis_min = kDefaultXMin;
  plot->xaxis_max = (end_time_ - begin_time_) / 1000000 * kXMargin;
  plot->xaxis_label = "Time (s)";
  plot->yaxis_min = kDefaultYMin;
  plot->yaxis_max = max_y * kYMargin;
  plot->yaxis_label = "Bitrate (kbps)";
  if (desired_direction == webrtc::PacketDirection::kIncomingPacket) {
    plot->title = "Incoming bitrate per stream";
  } else if (desired_direction == webrtc::PacketDirection::kOutgoingPacket) {
    plot->title = "Outgoing bitrate per stream";
  }
}

}  // namespace plotting
}  // namespace webrtc
