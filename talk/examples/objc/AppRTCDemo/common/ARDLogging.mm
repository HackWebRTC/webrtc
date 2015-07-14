/*
 * libjingle
 * Copyright 2015 Google Inc.
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

#import "ARDLogging.h"

#include "webrtc/base/logging.h"

void ARDLogInit() {
#ifndef _DEBUG
  // In debug builds the default level is LS_INFO and in non-debug builds it is
  // disabled. Continue to log to console in non-debug builds, but only
  // warnings and errors.
  rtc::LogMessage::LogToDebug(rtc::LS_WARNING);
#endif
}

void ARDLogToWebRTCLogger(ARDLogSeverity severity, NSString *logString) {
  if (logString.length) {
    const char* utf8String = logString.UTF8String;
    switch (severity) {
      case kARDLogSeverityVerbose:
        LOG(LS_VERBOSE) << utf8String;
        break;
      case kARDLogSeverityInfo:
        LOG(LS_INFO) << utf8String;
        break;
      case kARDLogSeverityWarning:
        LOG(LS_WARNING) << utf8String;
        break;
      case kARDLogSeverityError:
        LOG(LS_ERROR) << utf8String;
        break;
    }
  }
}

NSString *ARDFileName(const char *filePath) {
  NSString *nsFilePath =
      [[NSString alloc] initWithBytesNoCopy:const_cast<char *>(filePath)
                                     length:strlen(filePath)
                                   encoding:NSUTF8StringEncoding
                               freeWhenDone:NO];
  return nsFilePath.lastPathComponent;
}
