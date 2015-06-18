/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_DUMP_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_DUMP_H_

#include <string>

#include "webrtc/base/scoped_ptr.h"

namespace webrtc {

// Forward declaration of storage class that is automatically generated from
// the protobuf file.
class ACMDumpEventStream;

class AcmDumpImpl;

class AcmDump {
 public:
  // The types of debug events that are currently supported for logging.
  enum class DebugEvent { kLogStart, kLogEnd, kAudioPlayout };

  virtual ~AcmDump() {}

  static rtc::scoped_ptr<AcmDump> Create();

  // Starts logging for the specified duration to the specified file.
  // The logging will stop automatically after the specified duration.
  // If the file already exists it will be overwritten.
  // The function will return false on failure.
  virtual void StartLogging(const std::string& file_name, int duration_ms) = 0;

  // Logs an incoming or outgoing RTP packet.
  virtual void LogRtpPacket(bool incoming,
                            const uint8_t* packet,
                            size_t length) = 0;

  // Logs a debug event, with optional message.
  virtual void LogDebugEvent(DebugEvent event_type,
                             const std::string& event_message) = 0;
  virtual void LogDebugEvent(DebugEvent event_type) = 0;

  // Reads an AcmDump file and returns true when reading was successful.
  // The result is stored in the given ACMDumpEventStream object.
  static bool ParseAcmDump(const std::string& file_name,
                           ACMDumpEventStream* result);
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_DUMP_H_
