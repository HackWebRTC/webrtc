/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.util.logging.Level;
import java.util.logging.Logger;
import org.webrtc.Loggable;
import org.webrtc.Logging.Severity;

class FallbackLogger implements Loggable {
  final Logger fallbackLogger;

  public FallbackLogger() {
    fallbackLogger = Logger.getLogger("org.webrtc.FallbackLogger");
    fallbackLogger.setLevel(Level.ALL);
  }

  @Override
  public void onLogMessage(String message, Severity severity, String tag) {
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
