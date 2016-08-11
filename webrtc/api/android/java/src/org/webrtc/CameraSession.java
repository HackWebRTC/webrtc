/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

public interface CameraSession {
  public interface CreateSessionCallback {
    void onDone(CameraSession session);
    void onFailure(String error);
  }

  /**
   * Stops the capture. Waits until no more calls to capture observer will be made.
   * If waitCameraStop is true, also waits for the camera to stop.
   */
  void stop();
}
