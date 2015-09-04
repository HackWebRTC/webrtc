/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_RTC_EVENT_LOG_H_
#define WEBRTC_VIDEO_RTC_EVENT_LOG_H_

#include <string>

#include "webrtc/base/scoped_ptr.h"
#include "webrtc/video_receive_stream.h"
#include "webrtc/video_send_stream.h"

namespace webrtc {

// Forward declaration of storage class that is automatically generated from
// the protobuf file.
namespace rtclog {
class EventStream;
}  // namespace rtclog

class RtcEventLogImpl;

enum class MediaType;

class RtcEventLog {
 public:
  // The types of debug events that are currently supported for logging.
  enum class DebugEvent { kLogStart, kLogEnd, kAudioPlayout };

  virtual ~RtcEventLog() {}

  static rtc::scoped_ptr<RtcEventLog> Create();

  // Starts logging for the specified duration to the specified file.
  // The logging will stop automatically after the specified duration.
  // If the file already exists it will be overwritten.
  // If the file cannot be opened, the RtcEventLog will not start logging.
  virtual void StartLogging(const std::string& file_name, int duration_ms) = 0;

  virtual void StopLogging() = 0;

  // Logs configuration information for webrtc::VideoReceiveStream
  virtual void LogVideoReceiveStreamConfig(
      const webrtc::VideoReceiveStream::Config& config) = 0;

  // Logs configuration information for webrtc::VideoSendStream
  virtual void LogVideoSendStreamConfig(
      const webrtc::VideoSendStream::Config& config) = 0;

  // Logs the header of an incoming or outgoing RTP packet. packet_length
  // is the total length of the packet, including both header and payload.
  virtual void LogRtpHeader(bool incoming,
                            MediaType media_type,
                            const uint8_t* header,
                            size_t packet_length) = 0;

  // Logs an incoming or outgoing RTCP packet.
  virtual void LogRtcpPacket(bool incoming,
                             MediaType media_type,
                             const uint8_t* packet,
                             size_t length) = 0;

  // Logs a debug event.
  virtual void LogDebugEvent(DebugEvent event_type) = 0;

  // Reads an RtcEventLog file and returns true when reading was successful.
  // The result is stored in the given EventStream object.
  static bool ParseRtcEventLog(const std::string& file_name,
                               rtclog::EventStream* result);
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_RTC_EVENT_LOG_H_
