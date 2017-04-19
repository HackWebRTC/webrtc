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

/**
 * Base interface for camera1 and camera2 implementations. Extends VideoCapturer with a
 * switchCamera() function. Also provides subinterfaces for handling camera events, and a helper
 * class for detecting camera freezes.
 */
public interface CameraVideoCapturer extends VideoCapturer {
  /**
   * Camera events handler - can be used to be notifed about camera events. The callbacks are
   * executed from an arbitrary thread.
   */
  public interface CameraEventsHandler {
    // Camera error handler - invoked when camera can not be opened
    // or any camera exception happens on camera thread.
    void onCameraError(String errorDescription);

    // Called when camera is disconnected.
    void onCameraDisconnected();

    // Invoked when camera stops receiving frames.
    void onCameraFreezed(String errorDescription);

    // Callback invoked when camera is opening.
    void onCameraOpening(String cameraName);

    // Callback invoked when first camera frame is available after camera is started.
    void onFirstFrameAvailable();

    // Callback invoked when camera is closed.
    void onCameraClosed();
  }

  /**
   * Camera switch handler - one of these functions are invoked with the result of switchCamera().
   * The callback may be called on an arbitrary thread.
   */
  public interface CameraSwitchHandler {
    // Invoked on success. |isFrontCamera| is true if the new camera is front facing.
    void onCameraSwitchDone(boolean isFrontCamera);

    // Invoked on failure, e.g. camera is stopped or only one camera available.
    void onCameraSwitchError(String errorDescription);
  }

  /**
   * Switch camera to the next valid camera id. This can only be called while the camera is running.
   * This function can be called from any thread.
   */
  void switchCamera(CameraSwitchHandler switchEventsHandler);
}
