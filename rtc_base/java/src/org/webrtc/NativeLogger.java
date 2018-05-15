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

import org.webrtc.Loggable;
import org.webrtc.Logging.Severity;

class NativeLogger implements Loggable {
  public NativeLogger(Severity severity) {
    nativeEnableLogToDebugOutput(severity.ordinal());
  }

  @Override
  public void onLogMessage(String message, Severity severity, String tag) {
    nativeLog(severity.ordinal(), tag, message);
  }

  private static native void nativeEnableLogToDebugOutput(int nativeSeverity);
  private static native void nativeLog(int severity, String tag, String message);
}
