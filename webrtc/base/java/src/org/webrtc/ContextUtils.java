/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import android.content.Context;

/**
 * Class for storing the application context and retrieving it in a static context. Similar to
 * org.chromium.base.ContextUtils.
 */
public class ContextUtils {
  private static Context applicationContext;

  /**
   * Stores the application context that will be returned by getApplicationContext. This is called
   * by PeerConnectionFactory.initializeAndroidGlobals.
   */
  public static void initialize(Context applicationContext) {
    if (ContextUtils.applicationContext != null) {
      throw new RuntimeException("Multiple ContextUtils.initialize calls.");
    }
    if (applicationContext == null) {
      throw new RuntimeException("Application context cannot be null for ContextUtils.initialize.");
    }
    ContextUtils.applicationContext = applicationContext;
  }

  /**
   * Returns the stored application context.
   */
  public static Context getApplicationContext() {
    return applicationContext;
  }
}
