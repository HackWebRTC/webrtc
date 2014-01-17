/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.app;

import android.content.Context;

public class OpenSlRunner {
  public OpenSlRunner() {
    System.loadLibrary("opensl-demo-jni");
  }

  public static native void RegisterApplicationContext(Context context);
  public static native void Start();
  public static native void Stop();

}