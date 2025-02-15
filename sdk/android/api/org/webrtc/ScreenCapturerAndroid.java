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

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.hardware.display.DisplayManager;
import android.hardware.display.VirtualDisplay;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build.VERSION;
import android.os.Build.VERSION_CODES;
import android.view.Surface;
import androidx.annotation.Nullable;

/**
 * An implementation of VideoCapturer to capture the screen content as a video stream.
 * Capturing is done by {@code MediaProjection} on a {@code SurfaceTexture}. We interact with this
 * {@code SurfaceTexture} using a {@code SurfaceTextureHelper}.
 * The {@code SurfaceTextureHelper} is created by the native code and passed to this capturer in
 * {@code VideoCapturer.initialize()}. On receiving a new frame, this capturer passes it
 * as a texture to the native code via {@code CapturerObserver.onFrameCaptured()}. This takes
 * place on the HandlerThread of the given {@code SurfaceTextureHelper}. When done with each frame,
 * the native code returns the buffer to the  {@code SurfaceTextureHelper} to be used for new
 * frames. At any time, at most one frame is being processed.
 */
public class ScreenCapturerAndroid implements VideoCapturer, VideoSink {
  private static final int DISPLAY_FLAGS =
      DisplayManager.VIRTUAL_DISPLAY_FLAG_PUBLIC | DisplayManager.VIRTUAL_DISPLAY_FLAG_PRESENTATION;
  // DPI for VirtualDisplay, does not seem to matter for us.
  private static final int VIRTUAL_DISPLAY_DPI = 400;
  private static final String TAG = "ScreenCaptureAndroid";

  private Intent mediaProjectionPermissionResultData;
  private final MediaProjection.Callback mediaProjectionCallback;
  private final RateLimiter fpsLimiter = new RateLimiter(1000 / 20);

  private int width;
  private int height;
  @Nullable private VirtualDisplay virtualDisplay;
  @Nullable private SurfaceTextureHelper surfaceTextureHelper;
  @Nullable private CapturerObserver capturerObserver;
  private long numCapturedFrames;
  @Nullable private MediaProjection mediaProjection;
  private boolean isDisposed;
  @Nullable private MediaProjectionManager mediaProjectionManager;

  private long lastLogStatisticsTime;
  private int receivedFrameCount;
  private int compensateFrameCount;
  private int droppedFrameCount;

  private long feedOldFrameInterval;
  private long lastCapturedFrameTime;
  private final Runnable feedOldFrame = new Runnable() {
    @Override
    public void run() {
      long now = System.currentTimeMillis();
      long noFrameDuration = now - lastCapturedFrameTime;
      if (noFrameDuration > feedOldFrameInterval && lastCapturedFrameTime > 0) {
        surfaceTextureHelper.reuseLastFrame();
        lastCapturedFrameTime = now;
        compensateFrameCount++;
        logStatistics();
      }

      surfaceTextureHelper.getHandler().postDelayed(this, feedOldFrameInterval >> 1);
    }
  };

  /**
   * Constructs a new Screen Capturer.
   *
   * @param mediaProjectionPermissionResultData the result data of MediaProjection permission
   *     activity; the calling app must validate that result code is Activity.RESULT_OK before
   *     calling this method.
   * @param mediaProjectionCallback MediaProjection callback to implement application specific
   *     logic in events such as when the user revokes a previously granted capture permission.
  **/
  public ScreenCapturerAndroid(Intent mediaProjectionPermissionResultData,
      MediaProjection.Callback mediaProjectionCallback) {
    Logging.d(TAG, "new ScreenCapturerAndroid " + mediaProjectionPermissionResultData + " " + mediaProjectionCallback);
    this.mediaProjectionPermissionResultData = mediaProjectionPermissionResultData;
    this.mediaProjectionCallback = mediaProjectionCallback;
  }

  private boolean checkNotDisposed() {
    if (isDisposed) {
      Logging.e(TAG, "capturer is disposed.");
      return true;
    }

    return false;
  }

  @Nullable
  public MediaProjection getMediaProjection() {
    return mediaProjection;
  }

  @Override
  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized void initialize(final SurfaceTextureHelper surfaceTextureHelper,
      final Context applicationContext, final CapturerObserver capturerObserver) {
    Logging.d(TAG, "initialize " + surfaceTextureHelper + ", " + applicationContext + ", " + capturerObserver);

    if (checkNotDisposed()) {
      return;
    }

    if (capturerObserver == null) {
      throw new RuntimeException("capturerObserver not set.");
    }
    this.capturerObserver = capturerObserver;

    if (surfaceTextureHelper == null) {
      throw new RuntimeException("surfaceTextureHelper not set.");
    }
    this.surfaceTextureHelper = surfaceTextureHelper;

    mediaProjectionManager = (MediaProjectionManager) applicationContext.getSystemService(
        Context.MEDIA_PROJECTION_SERVICE);
  }

  public synchronized void setMediaProjectionPermissionResultData(Intent mediaProjectionPermissionResultData) {
    Logging.d(TAG, "setMediaProjectionPermissionResultData " + mediaProjectionPermissionResultData);
    this.mediaProjectionPermissionResultData = mediaProjectionPermissionResultData;
  }

  @Override
  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized void startCapture(
      final int width, final int height, final int frameRate) {
    Logging.d(TAG, "startCapture " + width + "x" + height + "@" + frameRate + " " + mediaProjectionPermissionResultData);

    if (checkNotDisposed()) {
      return;
    }
    if (mediaProjectionPermissionResultData == null) {
      Logging.e(TAG, "startCapture mediaProjectionPermissionResultData is null");
      return;
    }

    this.width = width;
    this.height = height;
    feedOldFrameInterval = 1000 / frameRate;
    fpsLimiter.updateInterval(1000 / frameRate);

    mediaProjection = mediaProjectionManager.getMediaProjection(
        Activity.RESULT_OK, mediaProjectionPermissionResultData);

    // Let MediaProjection callback use the SurfaceTextureHelper thread.
    mediaProjection.registerCallback(mediaProjectionCallback, surfaceTextureHelper.getHandler());

    updateVirtualDisplay();
    capturerObserver.onCapturerStarted(true);
    surfaceTextureHelper.startListening(ScreenCapturerAndroid.this);
    surfaceTextureHelper.getHandler().postDelayed(feedOldFrame, feedOldFrameInterval >> 1);

    Logging.d(TAG, "startCapture finish, " + mediaProjection + ", " + virtualDisplay);
  }

  @Override
  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized void stopCapture() {
    Logging.d(TAG, "stopCapture");

    if (checkNotDisposed()) {
      return;
    }
    surfaceTextureHelper.getHandler().removeCallbacks(feedOldFrame);
    ThreadUtils.invokeAtFrontUninterruptibly(surfaceTextureHelper.getHandler(), new Runnable() {
      @Override
      public void run() {
        surfaceTextureHelper.stopListening();
        capturerObserver.onCapturerStopped();

        if (virtualDisplay != null) {
          Logging.d(TAG, "stop virtualDisplay");
          virtualDisplay.release();
          virtualDisplay = null;
        }

        if (mediaProjection != null) {
          Logging.d(TAG, "stop mediaProjection");
          // Unregister the callback before stopping, otherwise the callback recursively
          // calls this method.
          mediaProjection.unregisterCallback(mediaProjectionCallback);
          mediaProjection.stop();
          mediaProjection = null;
        }

        Logging.d(TAG, "stopCapture finish");
      }
    });
  }

  @Override
  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized void dispose() {
    isDisposed = true;
  }

  /**
   * Changes output video format. This method can be used to scale the output
   * video, or to change orientation when the captured screen is rotated for example.
   *
   * @param width new output video width
   * @param height new output video height
   * @param ignoredFramerate ignored
   */
  @Override
  // TODO(bugs.webrtc.org/8491): Remove NoSynchronizedMethodCheck suppression.
  @SuppressWarnings("NoSynchronizedMethodCheck")
  public synchronized void changeCaptureFormat(
      final int width, final int height, final int ignoredFramerate) {
    if (checkNotDisposed()) {
      return;
    }

    this.width = width;
    this.height = height;

    if (virtualDisplay == null) {
      // Capturer is stopped, the virtual display will be created in startCapture().
      return;
    }

    // Create a new virtual display on the surfaceTextureHelper thread to avoid interference
    // with frame processing, which happens on the same thread (we serialize events by running
    // them on the same thread).
    ThreadUtils.invokeAtFrontUninterruptibly(
        surfaceTextureHelper.getHandler(), this::updateVirtualDisplay);
  }

  private void updateVirtualDisplay() {
    surfaceTextureHelper.setTextureSize(width, height);
    // Before Android S (12), resizing the virtual display can cause the captured screen to be
    // scaled incorrectly, so keep the behavior of recreating the virtual display prior to Android
    // S.
    if (virtualDisplay == null || VERSION.SDK_INT < VERSION_CODES.S) {
      createVirtualDisplay();
    } else {
      virtualDisplay.resize(width, height, VIRTUAL_DISPLAY_DPI);
      virtualDisplay.setSurface(new Surface(surfaceTextureHelper.getSurfaceTexture()));
    }
  }
  private void createVirtualDisplay() {
    if (virtualDisplay != null) {
      virtualDisplay.release();
    }
    virtualDisplay = mediaProjection.createVirtualDisplay("WebRTC_ScreenCapture", width, height,
        VIRTUAL_DISPLAY_DPI, DISPLAY_FLAGS, new Surface(surfaceTextureHelper.getSurfaceTexture()),
        null /* callback */, null /* callback handler */);
  }

  // This is called on the internal looper thread of {@Code SurfaceTextureHelper}.
  @Override
  public void onFrame(VideoFrame frame) {
    lastCapturedFrameTime = System.currentTimeMillis();
    numCapturedFrames++;
    if (!fpsLimiter.check(lastCapturedFrameTime)) {
      droppedFrameCount++;
      return;
    }
    capturerObserver.onFrameCaptured(new VideoFrame(frame.getBuffer(), frame.getRotation(),
        lastCapturedFrameTime * 1_000_000));
    receivedFrameCount++;
    logStatistics();
  }

  @Override
  public boolean isScreencast() {
    return true;
  }

  public long getNumCapturedFrames() {
    return numCapturedFrames;
  }

  private void logStatistics() {
    long ts = System.currentTimeMillis();
    if (ts - lastLogStatisticsTime > 5000) {
      if (lastLogStatisticsTime != 0) {
        float duration = (ts - lastLogStatisticsTime) / (float) 1000;
        Logging.d(TAG, "screen capture input statistics in " + duration
                     + "s, receive frame rate " + (receivedFrameCount / duration)
                     + ", compensate frame rate " + (compensateFrameCount / duration)
                     + ", drop frame rate " + (droppedFrameCount / duration));
      } else {
        Logging.d(TAG, "screen capture input statistics start");
      }
      receivedFrameCount = 0;
      compensateFrameCount = 0;
      droppedFrameCount = 0;
      lastLogStatisticsTime = ts;
    }
  }
}
