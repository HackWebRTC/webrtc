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
#include "webrtc/base/logging.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/stream.h"

NSString *const kDefaultLogFileName = @"webrtc.log";
NSUInteger const kDefaultMaxFileSize = 10 * 1024 * 1024; // 10MB.

namespace rtc {

class CircularFileStreamLogSink : public LogSink {
 public:
  // Creates a log sink that writes to the given stream. This log sink takes
  // ownership of |stream|.
  CircularFileStreamLogSink(CircularFileStream *stream) {
    DCHECK(stream);
    _stream.reset(stream);
  }

  ~CircularFileStreamLogSink() override {}

  void OnLogMessage(const std::string &message) override {
    if (_stream) {
      _stream->WriteAll(message.data(), message.size(), nullptr, nullptr);
    }
  }

  CircularFileStream *GetStream() { return _stream.get(); }

 private:
  scoped_ptr<CircularFileStream> _stream;
};

} // namespace rtc

@implementation RTCFileLogger {
  BOOL _hasStarted;
  NSString *_filePath;
  NSUInteger _maxFileSize;
  rtc::scoped_ptr<rtc::CircularFileStreamLogSink> _logSink;
}

@synthesize severity = _severity;

- (instancetype)init {
  NSArray *paths = NSSearchPathForDirectoriesInDomains(
      NSDocumentDirectory, NSUserDomainMask, YES);
  NSString *documentsDirPath = [paths firstObject];
  NSString *defaultFilePath =
      [documentsDirPath stringByAppendingPathComponent:kDefaultLogFileName];
  return [self initWithFilePath:defaultFilePath
                    maxFileSize:kDefaultMaxFileSize];
}

- (instancetype)initWithFilePath:(NSString *)filePath
                     maxFileSize:(NSUInteger)maxFileSize {
  NSParameterAssert(filePath.length);
  NSParameterAssert(maxFileSize);
  if (self = [super init]) {
    _filePath = filePath;
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
  rtc::scoped_ptr<rtc::CircularFileStream> stream;
  stream.reset(new rtc::CircularFileStream(_maxFileSize));
  _logSink.reset(new rtc::CircularFileStreamLogSink(stream.release()));
  int error = 0;
  if (!_logSink->GetStream()->Open(_filePath.UTF8String, "wb", &error)) {
    LOG(LS_ERROR) << "Failed to open log file at path: "
                  << _filePath.UTF8String
                  << " Error: "
                  << error;
    _logSink.reset();
    return;
  }
  // TODO(tkchin): Log thead info on iOS, currently this doesn't do anything.
  rtc::LogMessage::LogThreads(true);
  rtc::LogMessage::LogTimestamps(true);
  rtc::LogMessage::AddLogToStream(_logSink.get(), [self rtcSeverity]);
  _hasStarted = YES;
}

- (void)stop {
  if (!_hasStarted) {
    return;
  }
  DCHECK(_logSink);
  rtc::LogMessage::RemoveLogToStream(_logSink.get());
  _hasStarted = NO;

  // Read the ordered version of the log.
  NSData *logData = [self reorderedLogData];
  NSError *error = nil;
  // Write the ordered version back to disk.
  if (![logData writeToFile:_filePath
                    options:NSDataWritingAtomic
                      error:&error]) {
    LOG(LS_ERROR) << "Failed to rewrite log to disk at path: "
                  << _filePath.UTF8String;
    if (error) {
      LOG(LS_ERROR) << "Error: " << error.localizedDescription.UTF8String;
    }
  } else {
    // If we succeeded in writing to disk we don't need to hold on to the
    // stream anymore.
    _logSink.reset();
  }
}

- (NSData *)logData {
  if (_hasStarted) {
    return nil;
  }
  if (!_logSink.get()) {
    // If there isn't a previously used stream just return contents of file.
    return [[self class] contentsOfFileAtPath:_filePath];
  }
  return [self reorderedLogData];
}

#pragma mark - Private

+ (NSData *)contentsOfFileAtPath:(NSString *)path {
  NSError *error = nil;
  NSData *contents = [NSData dataWithContentsOfFile:path
                                            options:0
                                              error:&error];
  if (error) {
    LOG(LS_ERROR) << "Failed to read contents of file at path: "
                  << path.UTF8String
                  << " Error: "
                  << error.localizedDescription.UTF8String;
    return nil;
  }
  return contents;
}

- (NSData *)reorderedLogData {
  if (_hasStarted || !_logSink.get()) {
    return nil;
  }
  // We have a stream we used for writing in memory and we're not writing. The
  // stream has a pointer to where the log boundary is so it can reorder the
  // log correctly. We just need to reopen the file in read mode.
  int error = 0;
  rtc::CircularFileStream *stream = _logSink->GetStream();
  if (!stream->Open(_filePath.UTF8String, "r", &error)) {
    LOG(LS_ERROR) << "Failed to open log file at path: "
                  << _filePath.UTF8String
                  << " Error: "
                  << error;
    return nil;
  }
  size_t logSize = 0;
  size_t bytesRead = 0;
  error = 0;
  if (!stream->GetSize(&logSize)) {
    LOG(LS_ERROR) << "Failed to get log file size.";
    return nil;
  }
  // Allocate memory using malloc so we can pass it direcly to NSData without
  // copying.
  rtc::scoped_ptr<uint8_t[]> buffer(static_cast<uint8_t*>(malloc(logSize)));
  if (stream->ReadAll(buffer.get(), logSize, &bytesRead, &error)
      != rtc::SR_SUCCESS) {
    LOG(LS_ERROR) << "Failed to read log file at path: "
                  << _filePath.UTF8String
                  << " Error: "
                  << error;
  }
  DCHECK_LE(bytesRead, logSize);
  // NSData takes ownership of the bytes and frees it on dealloc.
  return [NSData dataWithBytesNoCopy:buffer.release()
                              length:bytesRead];
}

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
