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

/**
 * Helper class to log framerate and detect if the camera freezes. It will run periodic callbacks
 * on the SurfaceTextureHelper thread passed in the ctor, and should only be operated from that
 * thread.
 */
class CameraStatistics {
  private final static String TAG = "CameraStatistics";
  private final static int CAMERA_OBSERVER_PERIOD_MS = 2000;
  private final static int CAMERA_FREEZE_REPORT_TIMOUT_MS = 4000;

  private final SurfaceTextureHelper surfaceTextureHelper;
  private final CameraVideoCapturer.CameraEventsHandler eventsHandler;
  private int frameCount;
  private int freezePeriodCount;
  // Camera observer - monitors camera framerate. Observer is executed on camera thread.
  private final Runnable cameraObserver = new Runnable() {
    @Override
    public void run() {
      final int cameraFps = Math.round(frameCount * 1000.0f / CAMERA_OBSERVER_PERIOD_MS);
      Logging.d(TAG, "Camera fps: " + cameraFps + ".");
      if (frameCount == 0) {
        ++freezePeriodCount;
        if (CAMERA_OBSERVER_PERIOD_MS * freezePeriodCount >= CAMERA_FREEZE_REPORT_TIMOUT_MS
            && eventsHandler != null) {
          Logging.e(TAG, "Camera freezed.");
          if (surfaceTextureHelper.isTextureInUse()) {
            // This can only happen if we are capturing to textures.
            eventsHandler.onCameraFreezed("Camera failure. Client must return video buffers.");
          } else {
            eventsHandler.onCameraFreezed("Camera failure.");
          }
          return;
        }
      } else {
        freezePeriodCount = 0;
      }
      frameCount = 0;
      surfaceTextureHelper.getHandler().postDelayed(this, CAMERA_OBSERVER_PERIOD_MS);
    }
  };

  public CameraStatistics(SurfaceTextureHelper surfaceTextureHelper,
      CameraVideoCapturer.CameraEventsHandler eventsHandler) {
    if (surfaceTextureHelper == null) {
      throw new IllegalArgumentException("SurfaceTextureHelper is null");
    }
    this.surfaceTextureHelper = surfaceTextureHelper;
    this.eventsHandler = eventsHandler;
    this.frameCount = 0;
    this.freezePeriodCount = 0;
    surfaceTextureHelper.getHandler().postDelayed(cameraObserver, CAMERA_OBSERVER_PERIOD_MS);
  }

  private void checkThread() {
    if (Thread.currentThread() != surfaceTextureHelper.getHandler().getLooper().getThread()) {
      throw new IllegalStateException("Wrong thread");
    }
  }

  public void addFrame() {
    checkThread();
    ++frameCount;
  }

  public void release() {
    surfaceTextureHelper.getHandler().removeCallbacks(cameraObserver);
  }
}
