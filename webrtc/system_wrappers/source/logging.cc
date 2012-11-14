/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/logging.h"

#include <string.h>
#include <iostream>

#include "webrtc/common_types.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

static TraceLevel WebRtcSeverity(LoggingSeverity sev) {
  switch (sev) {
    // TODO(andrew): SENSITIVE doesn't have a corresponding webrtc level.
    case LS_SENSITIVE:  return kTraceInfo;
    case LS_VERBOSE:    return kTraceDebug;
    case LS_INFO:       return kTraceInfo;
    case LS_WARNING:    return kTraceWarning;
    case LS_ERROR:      return kTraceError;
    default:            return kTraceNone;
  }
}

LogMessage::LogMessage(const char* file, int line, LoggingSeverity sev)
    : severity_(sev) {
  print_stream_ << "(" << DescribeFile(file) << ":" << line << "): ";
}

LogMessage::~LogMessage() {
  const std::string& str = print_stream_.str();
  WEBRTC_TRACE(WebRtcSeverity(severity_), kTraceUndefined, 0, str.c_str());
}

const char* LogMessage::DescribeFile(const char* file) {
  const char* end1 = ::strrchr(file, '/');
  const char* end2 = ::strrchr(file, '\\');
  if (!end1 && !end2)
    return file;
  else
    return (end1 > end2) ? end1 + 1 : end2 + 1;
}

}  // namespace webrtc
