/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.io.PrintWriter;
import java.io.StringWriter;
import java.util.EnumSet;
import java.util.logging.ConsoleHandler;
import java.util.logging.SimpleFormatter;
import java.util.logging.Handler;
import java.util.logging.Logger;
import java.util.logging.Level;

/** Java wrapper for WebRTC logging. */
public class Logging {
  private static final Logger fallbackLogger = Logger.getLogger("org.webrtc.Logging");

  static {
    try {
      System.loadLibrary("jingle_peerconnection_so");
    } catch (Throwable t) {
      // If native logging is unavailable, log to system log.
      ConsoleHandler consolehandler = new ConsoleHandler();
      consolehandler.setFormatter(new SimpleFormatter());
      consolehandler.setLevel(Level.ALL);
      fallbackLogger.addHandler(consolehandler);
      fallbackLogger.setLevel(Level.FINE);
    }
  }

  // Keep in sync with webrtc/common_types.h:TraceLevel.
  public enum TraceLevel {
    TRACE_NONE(0x0000),
    TRACE_STATEINFO(0x0001),
    TRACE_WARNING(0x0002),
    TRACE_ERROR(0x0004),
    TRACE_CRITICAL(0x0008),
    TRACE_APICALL(0x0010),
    TRACE_DEFAULT(0x00ff),
    TRACE_MODULECALL(0x0020),
    TRACE_MEMORY(0x0100),
    TRACE_TIMER(0x0200),
    TRACE_STREAM(0x0400),
    TRACE_DEBUG(0x0800),
    TRACE_INFO(0x1000),
    TRACE_TERSEINFO(0x2000),
    TRACE_ALL(0xffff);

    public final int level;
    TraceLevel(int level) {
      this.level = level;
    }
  };

  // Keep in sync with webrtc/base/logging.h:LoggingSeverity.
  public enum Severity {
    LS_SENSITIVE, LS_VERBOSE, LS_INFO, LS_WARNING, LS_ERROR,
  };

  public static void enableLogThreads() {
    nativeEnableLogThreads();
  }

  public static void enableLogTimeStamps() {
    nativeEnableLogTimeStamps();
  }

  // Enable tracing to |path| of messages of |levels| and |severity|.
  // On Android, use "logcat:" for |path| to send output there.
  public static void enableTracing(
      String path, EnumSet<TraceLevel> levels, Severity severity) {
    int nativeLevel = 0;
    for (TraceLevel level : levels) {
      nativeLevel |= level.level;
    }
    nativeEnableTracing(path, nativeLevel, severity.ordinal());
  }

  public static void log(Severity severity, String tag, String message) {
    try {
      nativeLog(severity.ordinal(), tag, message);
    } catch (Throwable t) {
      Level level;
      switch (severity) {
        case LS_ERROR:
          level = Level.SEVERE;
          break;
        case LS_WARNING:
          level = Level.WARNING;
          break;
        case LS_INFO:
          level = Level.INFO;
          break;
        default:
          level = Level.FINE;
          break;
      }
      fallbackLogger.log(level, tag + ": " + message);
    }
  }

  public static void d(String tag, String message) {
    log(Severity.LS_INFO, tag, message);
  }

  public static void e(String tag, String message) {
    log(Severity.LS_ERROR, tag, message);
  }

  public static void w(String tag, String message) {
    log(Severity.LS_WARNING, tag, message);
  }

  public static void e(String tag, String message, Throwable e) {
    log(Severity.LS_ERROR, tag, message);
    log(Severity.LS_ERROR, tag, e.toString());
    log(Severity.LS_ERROR, tag, getStackTraceString(e));
  }

  public static void w(String tag, String message, Throwable e) {
    log(Severity.LS_WARNING, tag, message);
    log(Severity.LS_WARNING, tag, e.toString());
    log(Severity.LS_WARNING, tag, getStackTraceString(e));
  }

  public static void v(String tag, String message) {
    log(Severity.LS_VERBOSE, tag, message);
  }

  private static String getStackTraceString(Throwable e) {
    if (e == null) {
      return "";
    }

    StringWriter sw = new StringWriter();
    PrintWriter pw = new PrintWriter(sw);
    e.printStackTrace(pw);
    return sw.toString();
  }

  private static native void nativeEnableTracing(
      String path, int nativeLevels, int nativeSeverity);
  private static native void nativeEnableLogThreads();
  private static native void nativeEnableLogTimeStamps();
  private static native void nativeLog(int severity, String tag, String message);
}
