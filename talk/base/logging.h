/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

//   LOG(...) an ostream target that can be used to send formatted
// output to a variety of logging targets, such as debugger console, stderr,
// file, or any StreamInterface.
//   The severity level passed as the first argument to the LOGging
// functions is used as a filter, to limit the verbosity of the logging.
//   Static members of LogMessage documented below are used to control the
// verbosity and target of the output.
//   There are several variations on the LOG macro which facilitate logging
// of common error conditions, detailed below.

// LOG(sev) logs the given stream at severity "sev", which must be a
//     compile-time constant of the LoggingSeverity type, without the namespace
//     prefix.
// LOG_V(sev) Like LOG(), but sev is a run-time variable of the LoggingSeverity
//     type (basically, it just doesn't prepend the namespace).
// LOG_F(sev) Like LOG(), but includes the name of the current function.
// LOG_T(sev) Like LOG(), but includes the this pointer.
// LOG_T_F(sev) Like LOG_F(), but includes the this pointer.
// LOG_GLE(M)(sev [, mod]) attempt to add a string description of the
//     HRESULT returned by GetLastError.  The "M" variant allows searching of a
//     DLL's string table for the error description.
// LOG_ERRNO(sev) attempts to add a string description of an errno-derived
//     error. errno and associated facilities exist on both Windows and POSIX,
//     but on Windows they only apply to the C/C++ runtime.
// LOG_ERR(sev) is an alias for the platform's normal error system, i.e. _GLE on
//     Windows and _ERRNO on POSIX.
// (The above three also all have _EX versions that let you specify the error
// code, rather than using the last one.)
// LOG_E(sev, ctx, err, ...) logs a detailed error interpreted using the
//     specified context.
// LOG_CHECK_LEVEL(sev) (and LOG_CHECK_LEVEL_V(sev)) can be used as a test
//     before performing expensive or sensitive operations whose sole purpose is
//     to output logging data at the desired level.
// Lastly, PLOG(sev, err) is an alias for LOG_ERR_EX.

#ifndef TALK_BASE_LOGGING_H_
#define TALK_BASE_LOGGING_H_

#ifdef HAVE_CONFIG_H
#include "config.h"  // NOLINT
#endif

#include <list>
#include <sstream>
#include <string>
#include <utility>
#include "talk/base/basictypes.h"
#include "talk/base/criticalsection.h"

namespace talk_base {

class StreamInterface;

///////////////////////////////////////////////////////////////////////////////
// ConstantLabel can be used to easily generate string names from constant
// values.  This can be useful for logging descriptive names of error messages.
// Usage:
//   const ConstantLabel LIBRARY_ERRORS[] = {
//     KLABEL(SOME_ERROR),
//     KLABEL(SOME_OTHER_ERROR),
//     ...
//     LASTLABEL
//   }
//
//   int err = LibraryFunc();
//   LOG(LS_ERROR) << "LibraryFunc returned: "
//                 << ErrorName(err, LIBRARY_ERRORS);

struct ConstantLabel { int value; const char * label; };
#define KLABEL(x) { x, #x }
#define TLABEL(x, y) { x, y }
#define LASTLABEL { 0, 0 }

const char * FindLabel(int value, const ConstantLabel entries[]);
std::string ErrorName(int err, const ConstantLabel* err_table);

//////////////////////////////////////////////////////////////////////

// Note that the non-standard LoggingSeverity aliases exist because they are
// still in broad use.  The meanings of the levels are:
//  LS_SENSITIVE: Information which should only be logged with the consent
//   of the user, due to privacy concerns.
//  LS_VERBOSE: This level is for data which we do not want to appear in the
//   normal debug log, but should appear in diagnostic logs.
//  LS_INFO: Chatty level used in debugging for all sorts of things, the default
//   in debug builds.
//  LS_WARNING: Something that may warrant investigation.
//  LS_ERROR: Something that should not have occurred.
enum LoggingSeverity { LS_SENSITIVE, LS_VERBOSE, LS_INFO, LS_WARNING, LS_ERROR,
                       INFO = LS_INFO,
                       WARNING = LS_WARNING,
                       LERROR = LS_ERROR };

// LogErrorContext assists in interpreting the meaning of an error value.
enum LogErrorContext {
  ERRCTX_NONE,
  ERRCTX_ERRNO,     // System-local errno
  ERRCTX_HRESULT,   // Windows HRESULT
  ERRCTX_OSSTATUS,  // MacOS OSStatus

  // Abbreviations for LOG_E macro
  ERRCTX_EN = ERRCTX_ERRNO,     // LOG_E(sev, EN, x)
  ERRCTX_HR = ERRCTX_HRESULT,   // LOG_E(sev, HR, x)
  ERRCTX_OS = ERRCTX_OSSTATUS,  // LOG_E(sev, OS, x)
};

class LogMessage {
 public:
  static const int NO_LOGGING;
  static const uint32 WARN_SLOW_LOGS_DELAY = 50;  // ms

  LogMessage(const char* file, int line, LoggingSeverity sev,
             LogErrorContext err_ctx = ERRCTX_NONE, int err = 0,
             const char* module = NULL);
  ~LogMessage();

  static inline bool Loggable(LoggingSeverity sev) { return (sev >= min_sev_); }
  std::ostream& stream() { return print_stream_; }

  // Returns the time at which this function was called for the first time.
  // The time will be used as the logging start time.
  // If this is not called externally, the LogMessage ctor also calls it, in
  // which case the logging start time will be the time of the first LogMessage
  // instance is created.
  static uint32 LogStartTime();

  // Returns the wall clock equivalent of |LogStartTime|, in seconds from the
  // epoch.
  static uint32 WallClockStartTime();

  // These are attributes which apply to all logging channels
  //  LogContext: Display the file and line number of the message
  static void LogContext(int min_sev);
  //  LogThreads: Display the thread identifier of the current thread
  static void LogThreads(bool on = true);
  //  LogTimestamps: Display the elapsed time of the program
  static void LogTimestamps(bool on = true);

  // These are the available logging channels
  //  Debug: Debug console on Windows, otherwise stderr
  static void LogToDebug(int min_sev);
  static int GetLogToDebug() { return dbg_sev_; }

  //  Stream: Any non-blocking stream interface.  LogMessage takes ownership of
  //   the stream. Multiple streams may be specified by using AddLogToStream.
  //   LogToStream is retained for backwards compatibility; when invoked, it
  //   will discard any previously set streams and install the specified stream.
  //   GetLogToStream gets the severity for the specified stream, of if none
  //   is specified, the minimum stream severity.
  //   RemoveLogToStream removes the specified stream, without destroying it.
  static void LogToStream(StreamInterface* stream, int min_sev);
  static int GetLogToStream(StreamInterface* stream = NULL);
  static void AddLogToStream(StreamInterface* stream, int min_sev);
  static void RemoveLogToStream(StreamInterface* stream);

  // Testing against MinLogSeverity allows code to avoid potentially expensive
  // logging operations by pre-checking the logging level.
  static int GetMinLogSeverity() { return min_sev_; }

  static void SetDiagnosticMode(bool f) { is_diagnostic_mode_ = f; }
  static bool IsDiagnosticMode() { return is_diagnostic_mode_; }

  // Parses the provided parameter stream to configure the options above.
  // Useful for configuring logging from the command line.  If file logging
  // is enabled, it is output to the specified filename.
  static void ConfigureLogging(const char* params, const char* filename);

  // Convert the string to a LS_ value; also accept numeric values.
  static int ParseLogSeverity(const std::string& value);

 private:
  typedef std::list<std::pair<StreamInterface*, int> > StreamList;

  // Updates min_sev_ appropriately when debug sinks change.
  static void UpdateMinLogSeverity();

  // These assist in formatting some parts of the debug output.
  static const char* Describe(LoggingSeverity sev);
  static const char* DescribeFile(const char* file);

  // These write out the actual log messages.
  static void OutputToDebug(const std::string& msg, LoggingSeverity severity_);
  static void OutputToStream(StreamInterface* stream, const std::string& msg);

  // The ostream that buffers the formatted message before output
  std::ostringstream print_stream_;

  // The severity level of this message
  LoggingSeverity severity_;

  // String data generated in the constructor, that should be appended to
  // the message before output.
  std::string extra_;

  // If time it takes to write to stream is more than this, log one
  // additional warning about it.
  uint32 warn_slow_logs_delay_;

  // Global lock for the logging subsystem
  static CriticalSection crit_;

  // dbg_sev_ is the thresholds for those output targets
  // min_sev_ is the minimum (most verbose) of those levels, and is used
  //  as a short-circuit in the logging macros to identify messages that won't
  //  be logged.
  // ctx_sev_ is the minimum level at which file context is displayed
  static int min_sev_, dbg_sev_, ctx_sev_;

  // The output streams and their associated severities
  static StreamList streams_;

  // Flags for formatting options
  static bool thread_, timestamp_;

  // are we in diagnostic mode (as defined by the app)?
  static bool is_diagnostic_mode_;

  DISALLOW_EVIL_CONSTRUCTORS(LogMessage);
};

//////////////////////////////////////////////////////////////////////
// Logging Helpers
//////////////////////////////////////////////////////////////////////

class LogMultilineState {
 public:
  size_t unprintable_count_[2];
  LogMultilineState() {
    unprintable_count_[0] = unprintable_count_[1] = 0;
  }
};

// When possible, pass optional state variable to track various data across
// multiple calls to LogMultiline.  Otherwise, pass NULL.
void LogMultiline(LoggingSeverity level, const char* label, bool input,
                  const void* data, size_t len, bool hex_mode,
                  LogMultilineState* state);

//////////////////////////////////////////////////////////////////////
// Macros which automatically disable logging when LOGGING == 0
//////////////////////////////////////////////////////////////////////

// If LOGGING is not explicitly defined, default to enabled in debug mode
#if !defined(LOGGING)
#if defined(_DEBUG) && !defined(NDEBUG)
#define LOGGING 1
#else
#define LOGGING 0
#endif
#endif  // !defined(LOGGING)

#ifndef LOG
#if LOGGING

// The following non-obvious technique for implementation of a
// conditional log stream was stolen from google3/base/logging.h.

// This class is used to explicitly ignore values in the conditional
// logging macros.  This avoids compiler warnings like "value computed
// is not used" and "statement has no effect".

class LogMessageVoidify {
 public:
  LogMessageVoidify() { }
  // This has to be an operator with a precedence lower than << but
  // higher than ?:
  void operator&(std::ostream&) { }
};

#define LOG_SEVERITY_PRECONDITION(sev) \
  !(talk_base::LogMessage::Loggable(sev)) \
    ? (void) 0 \
    : talk_base::LogMessageVoidify() &

#define LOG(sev) \
  LOG_SEVERITY_PRECONDITION(talk_base::sev) \
    talk_base::LogMessage(__FILE__, __LINE__, talk_base::sev).stream()

// The _V version is for when a variable is passed in.  It doesn't do the
// namespace concatination.
#define LOG_V(sev) \
  LOG_SEVERITY_PRECONDITION(sev) \
    talk_base::LogMessage(__FILE__, __LINE__, sev).stream()

// The _F version prefixes the message with the current function name.
#if (defined(__GNUC__) && defined(_DEBUG)) || defined(WANT_PRETTY_LOG_F)
#define LOG_F(sev) LOG(sev) << __PRETTY_FUNCTION__ << ": "
#define LOG_T_F(sev) LOG(sev) << this << ": " << __PRETTY_FUNCTION__ << ": "
#else
#define LOG_F(sev) LOG(sev) << __FUNCTION__ << ": "
#define LOG_T_F(sev) LOG(sev) << this << ": " << __FUNCTION__ << ": "
#endif

#define LOG_CHECK_LEVEL(sev) \
  talk_base::LogCheckLevel(talk_base::sev)
#define LOG_CHECK_LEVEL_V(sev) \
  talk_base::LogCheckLevel(sev)
inline bool LogCheckLevel(LoggingSeverity sev) {
  return (LogMessage::GetMinLogSeverity() <= sev);
}

#define LOG_E(sev, ctx, err, ...) \
  LOG_SEVERITY_PRECONDITION(talk_base::sev) \
    talk_base::LogMessage(__FILE__, __LINE__, talk_base::sev, \
                          talk_base::ERRCTX_ ## ctx, err , ##__VA_ARGS__) \
        .stream()

#define LOG_T(sev) LOG(sev) << this << ": "

#else  // !LOGGING

// Hopefully, the compiler will optimize away some of this code.
// Note: syntax of "1 ? (void)0 : LogMessage" was causing errors in g++,
//   converted to "while (false)"
#define LOG(sev) \
  while (false)talk_base:: LogMessage(NULL, 0, talk_base::sev).stream()
#define LOG_V(sev) \
  while (false) talk_base::LogMessage(NULL, 0, sev).stream()
#define LOG_F(sev) LOG(sev) << __FUNCTION__ << ": "
#define LOG_CHECK_LEVEL(sev) \
  false
#define LOG_CHECK_LEVEL_V(sev) \
  false

#define LOG_E(sev, ctx, err, ...) \
  while (false) talk_base::LogMessage(__FILE__, __LINE__, talk_base::sev, \
                          talk_base::ERRCTX_ ## ctx, err , ##__VA_ARGS__) \
      .stream()

#define LOG_T(sev) LOG(sev) << this << ": "
#define LOG_T_F(sev) LOG(sev) << this << ": " << __FUNCTION__ <<
#endif  // !LOGGING

#define LOG_ERRNO_EX(sev, err) \
  LOG_E(sev, ERRNO, err)
#define LOG_ERRNO(sev) \
  LOG_ERRNO_EX(sev, errno)

#ifdef WIN32
#define LOG_GLE_EX(sev, err) \
  LOG_E(sev, HRESULT, err)
#define LOG_GLE(sev) \
  LOG_GLE_EX(sev, GetLastError())
#define LOG_GLEM(sev, mod) \
  LOG_E(sev, HRESULT, GetLastError(), mod)
#define LOG_ERR_EX(sev, err) \
  LOG_GLE_EX(sev, err)
#define LOG_ERR(sev) \
  LOG_GLE(sev)
#define LAST_SYSTEM_ERROR \
  (::GetLastError())
#elif __native_client__
#define LOG_ERR_EX(sev, err) \
  LOG(sev)
#define LOG_ERR(sev) \
  LOG(sev)
#define LAST_SYSTEM_ERROR \
  (0)
#elif POSIX
#define LOG_ERR_EX(sev, err) \
  LOG_ERRNO_EX(sev, err)
#define LOG_ERR(sev) \
  LOG_ERRNO(sev)
#define LAST_SYSTEM_ERROR \
  (errno)
#endif  // WIN32

#define PLOG(sev, err) \
  LOG_ERR_EX(sev, err)

// TODO(?): Add an "assert" wrapper that logs in the same manner.

#endif  // LOG

}  // namespace talk_base

#endif  // TALK_BASE_LOGGING_H_
