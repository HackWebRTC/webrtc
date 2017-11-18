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

/** This class contains the Java glue code for JNI generation of Integer handling. */
class IntegerWrapper {
  @CalledByNative
  static Integer create(int i) {
    return Integer.valueOf(i);
  }

  @CalledByNative
  static int getIntValue(Integer i) {
    return i;
  }
}
