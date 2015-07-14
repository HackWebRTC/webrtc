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

#import <Foundation/Foundation.h>

// We route all logging through the WebRTC logger. By doing this we will get
// both app and WebRTC logs in the same place, which we can then route to a
// file if we need to. A side effect of this is that we get severity for free.
typedef NS_ENUM(NSInteger, ARDLogSeverity) {
  kARDLogSeverityVerbose,
  kARDLogSeverityInfo,
  kARDLogSeverityWarning,
  kARDLogSeverityError,
};

#if defined(__cplusplus)
extern "C" void ARDLogToWebRTCLogger(ARDLogSeverity severity,
                                     NSString *logString);
extern "C" NSString *ARDFileName(const char *filePath);
extern "C" void ARDLogInit();
#else
// Logs |logString| to the WebRTC logger at the given severity.
extern void ARDLogToWebRTCLogger(ARDLogSeverity severity, NSString *logString);
// Returns the filename with the path prefix removed.
extern NSString *ARDFileName(const char *filePath);
// Initializes the correct logging levels. This should be called once on app
// startup.
extern void ARDLogInit();
#endif

#define ARDLogString(format, ...)                    \
  [NSString stringWithFormat:@"(%@:%d %s): " format, \
      ARDFileName(__FILE__),                         \
      __LINE__,                                      \
      __FUNCTION__,                                  \
      ##__VA_ARGS__]

#define ARDLogEx(severity, format, ...)                        \
  do {                                                         \
    NSString *logString = ARDLogString(format, ##__VA_ARGS__); \
    ARDLogToWebRTCLogger(severity, logString);                 \
  } while (false)

#define ARDLogVerbose(format, ...)                        \
  ARDLogEx(kARDLogSeverityVerbose, format, ##__VA_ARGS__) \

#define ARDLogInfo(format, ...)                           \
  ARDLogEx(kARDLogSeverityInfo, format, ##__VA_ARGS__)    \

#define ARDLogWarning(format, ...)                        \
  ARDLogEx(kARDLogSeverityWarning, format, ##__VA_ARGS__) \

#define ARDLogError(format, ...)                          \
  ARDLogEx(kARDLogSeverityError, format, ##__VA_ARGS__)   \

#ifdef _DEBUG
#define ARDLogDebug(format, ...) ARDLogInfo(format, ##__VA_ARGS__)
#else
#define ARDLogDebug(format, ...) \
  do {                           \
  } while (false)
#endif

#define ARDLog(format, ...) ARDLogInfo(format, ##__VA_ARGS__)
