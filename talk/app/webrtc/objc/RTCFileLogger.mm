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

#import "RTCFileLogger.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/filerotatingstream.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/logsinks.h"
#include "webrtc/base/scoped_ptr.h"

NSString *const kDefaultLogDirName = @"webrtc_logs";
NSUInteger const kDefaultMaxFileSize = 10 * 1024 * 1024; // 10MB.
const char *kRTCFileLoggerRotatingLogPrefix = "rotating_log";

@implementation RTCFileLogger {
  BOOL _hasStarted;
  NSString *_dirPath;
  NSUInteger _maxFileSize;
  rtc::scoped_ptr<rtc::FileRotatingLogSink> _logSink;
}

@synthesize severity = _severity;
@synthesize rotationType = _rotationType;

- (instancetype)init {
  NSArray *paths = NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES);
  NSString *documentsDirPath = [paths firstObject];
  NSString *defaultDirPath =
      [documentsDirPath stringByAppendingPathComponent:kDefaultLogDirName];
  return [self initWithDirPath:defaultDirPath
                   maxFileSize:kDefaultMaxFileSize];
}

- (instancetype)initWithDirPath:(NSString *)dirPath
                    maxFileSize:(NSUInteger)maxFileSize {
  return [self initWithDirPath:dirPath
                   maxFileSize:maxFileSize
                  rotationType:kRTCFileLoggerTypeCall];
}

- (instancetype)initWithDirPath:(NSString *)dirPath
                    maxFileSize:(NSUInteger)maxFileSize
                   rotationType:(RTCFileLoggerRotationType)rotationType {
  NSParameterAssert(dirPath.length);
  NSParameterAssert(maxFileSize);
  if (self = [super init]) {
    BOOL isDir = NO;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    if ([fileManager fileExistsAtPath:dirPath isDirectory:&isDir]) {
      if (!isDir) {
        // Bail if something already exists there.
        return nil;
      }
    } else {
      if (![fileManager createDirectoryAtPath:dirPath
                  withIntermediateDirectories:NO
                                   attributes:nil
                                        error:nil]) {
        // Bail if we failed to create a directory.
        return nil;
      }
    }
    _dirPath = dirPath;
    _maxFileSize = maxFileSize;
    _severity = kRTCFileLoggerSeverityInfo;
  }
  return self;
}

- (void)dealloc {
  [self stop];
}

- (void)start {
  if (_hasStarted) {
    return;
  }
  switch (_rotationType) {
    case kRTCFileLoggerTypeApp:
      _logSink.reset(
          new rtc::FileRotatingLogSink(_dirPath.UTF8String,
                                       kRTCFileLoggerRotatingLogPrefix,
                                       _maxFileSize,
                                       _maxFileSize / 10));
      break;
    case kRTCFileLoggerTypeCall:
      _logSink.reset(
          new rtc::CallSessionFileRotatingLogSink(_dirPath.UTF8String,
                                                  _maxFileSize));
      break;
  }
  if (!_logSink->Init()) {
    LOG(LS_ERROR) << "Failed to open log files at path: "
                  << _dirPath.UTF8String;
    _logSink.reset();
    return;
  }
  rtc::LogMessage::LogThreads(true);
  rtc::LogMessage::LogTimestamps(true);
  rtc::LogMessage::AddLogToStream(_logSink.get(), [self rtcSeverity]);
  _hasStarted = YES;
}

- (void)stop {
  if (!_hasStarted) {
    return;
  }
  RTC_DCHECK(_logSink);
  rtc::LogMessage::RemoveLogToStream(_logSink.get());
  _hasStarted = NO;
  _logSink.reset();
}

- (NSData *)logData {
  if (_hasStarted) {
    return nil;
  }
  NSMutableData* logData = [NSMutableData data];
  rtc::scoped_ptr<rtc::FileRotatingStream> stream;
  switch(_rotationType) {
    case kRTCFileLoggerTypeApp:
      stream.reset(
          new rtc::FileRotatingStream(_dirPath.UTF8String,
                                      kRTCFileLoggerRotatingLogPrefix));
      break;
    case kRTCFileLoggerTypeCall:
      stream.reset(new rtc::CallSessionFileRotatingStream(_dirPath.UTF8String));
      break;
  }
  if (!stream->Open()) {
    return logData;
  }
  size_t bufferSize = 0;
  if (!stream->GetSize(&bufferSize) || bufferSize == 0) {
    return logData;
  }
  size_t read = 0;
  // Allocate memory using malloc so we can pass it direcly to NSData without
  // copying.
  rtc::scoped_ptr<uint8_t[]> buffer(static_cast<uint8_t*>(malloc(bufferSize)));
  stream->ReadAll(buffer.get(), bufferSize, &read, nullptr);
  logData = [[NSMutableData alloc] initWithBytesNoCopy:buffer.release()
                                                length:read];
  return logData;
}

#pragma mark - Private

- (rtc::LoggingSeverity)rtcSeverity {
  switch (_severity) {
    case kRTCFileLoggerSeverityVerbose:
      return rtc::LS_VERBOSE;
    case kRTCFileLoggerSeverityInfo:
      return rtc::LS_INFO;
    case kRTCFileLoggerSeverityWarning:
      return rtc::LS_WARNING;
    case kRTCFileLoggerSeverityError:
      return rtc::LS_ERROR;
  }
}

@end
