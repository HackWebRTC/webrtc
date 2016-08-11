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

import org.webrtc.CameraEnumerationAndroid.CaptureFormat;

import android.content.Context;
import android.os.Handler;
import android.os.SystemClock;

import java.util.Arrays;
import java.util.List;

@SuppressWarnings("deprecation")
public abstract class CameraCapturer implements CameraVideoCapturer {
  private static final String TAG = "CameraCapturer";
  private final static int MAX_OPEN_CAMERA_ATTEMPTS = 3;
  private final static int OPEN_CAMERA_DELAY_MS = 500;

  private final CameraEnumerator cameraEnumerator;
  private final CameraEventsHandler eventsHandler;

  private final CameraSession.CreateSessionCallback createSessionCallback =
      new CameraSession.CreateSessionCallback() {
        @Override
        public void onDone(CameraSession session) {
          Logging.d(TAG, "Create session done");

          synchronized (stateLock) {
            sessionOpening = false;
            currentSession = session;
            stateLock.notifyAll();

            if (switchEventsHandler != null) {
              switchEventsHandler.onCameraSwitchDone(
                  cameraEnumerator.isFrontFacing(cameraName));
              switchEventsHandler = null;
            }
            switchInProgress = false;
          }
        }

        @Override
        public void onFailure(String error) {
          synchronized (stateLock) {
            openAttemptsRemaining--;

            if (openAttemptsRemaining <= 0) {
              Logging.w(TAG, "Opening camera failed, passing: " + error);
              sessionOpening = false;
              stateLock.notifyAll();

              if (switchEventsHandler != null) {
                switchEventsHandler.onCameraSwitchError(error);
                switchEventsHandler = null;
              }
              switchInProgress = false;

              eventsHandler.onCameraError(error);
            } else {
              Logging.w(TAG, "Opening camera failed, retry: " + error);

              createSessionInternal(OPEN_CAMERA_DELAY_MS);
            }
          }
        }
      };

  // Initialized on initialize
  // -------------------------
  // Use postOnCameraThread() instead of posting directly to the handler - this way all
  // callbacks with a specifed token can be removed at once.
  private Handler cameraThreadHandler;
  private Context applicationContext;
  private CapturerObserver capturerObserver;
  private SurfaceTextureHelper surfaceHelper;

  private final Object stateLock = new Object();
  private boolean sessionOpening;                  /* guarded by stateLock */
  private CameraSession currentSession;            /* guarded by stateLock */
  private String cameraName;                       /* guarded by stateLock */
  private int width;                               /* guarded by stateLock */
  private int height;                              /* guarded by stateLock */
  private int framerate;                           /* guarded by stateLock */
  private int openAttemptsRemaining;               /* guarded by stateLock */
  private boolean switchInProgress;                /* guarded by stateLock */
  private CameraSwitchHandler switchEventsHandler; /* guarded by stateLock */

  public CameraCapturer(
      String cameraName, CameraEventsHandler eventsHandler, CameraEnumerator cameraEnumerator) {
    if (eventsHandler == null) {
      eventsHandler = new CameraEventsHandler() {
        @Override
        public void onCameraError(String errorDescription) {}
        @Override
        public void onCameraFreezed(String errorDescription) {}
        @Override
        public void onCameraOpening(int cameraId) {}
        @Override
        public void onFirstFrameAvailable() {}
        @Override
        public void onCameraClosed() {}
      };
    }

    this.eventsHandler = eventsHandler;
    this.cameraEnumerator = cameraEnumerator;
    this.cameraName = cameraName;

    final String[] deviceNames = cameraEnumerator.getDeviceNames();

    if (deviceNames.length == 0) {
      throw new RuntimeException("No cameras attached.");
    }
    if (!Arrays.asList(deviceNames).contains(this.cameraName)) {
      throw new IllegalArgumentException(
          "Camera name " + this.cameraName + " does not match any known camera device.");
    }
  }

  @Override
  public void initialize(SurfaceTextureHelper surfaceTextureHelper, Context applicationContext,
      CapturerObserver capturerObserver) {
    this.applicationContext = applicationContext;
    this.capturerObserver = capturerObserver;
    this.surfaceHelper = surfaceTextureHelper;
    this.cameraThreadHandler =
        surfaceTextureHelper == null ? null : surfaceTextureHelper.getHandler();
  }

  @Override
  public void startCapture(int width, int height, int framerate) {
    Logging.d(TAG, "startCapture: " + width + "x" + height + "@" + framerate);

    synchronized (stateLock) {
      if (sessionOpening || currentSession != null) {
        Logging.w(TAG, "Session already open");
        return;
      }

      this.width = width;
      this.height = height;
      this.framerate = framerate;

      sessionOpening = true;
      openAttemptsRemaining = MAX_OPEN_CAMERA_ATTEMPTS;
      createSessionInternal(0);
    }
  }

  private void createSessionInternal(int delayMs) {
    cameraThreadHandler.postDelayed(new Runnable() {
      @Override
      public void run() {
        createCameraSession(
            createSessionCallback,
            eventsHandler, applicationContext, capturerObserver, surfaceHelper,
            cameraName, width, height, framerate);
      }
    }, delayMs);
  }

  @Override
  public void stopCapture() {
    Logging.d(TAG, "Stop capture");

    synchronized (stateLock) {
      while (sessionOpening) {
        Logging.d(TAG, "Stop capture: Waiting for session to open");
        ThreadUtils.waitUninterruptibly(stateLock);
      }

      if (currentSession != null) {
        Logging.d(TAG, "Stop capture: Stopping session");
        currentSession.stop();
        currentSession = null;
      } else {
        Logging.d(TAG, "Stop capture: No session open");
      }
    }

    Logging.d(TAG, "Stop capture done");
  }

  @Override
  public void onOutputFormatRequest(final int width, final int height, final int framerate) {
    cameraThreadHandler.post(new Runnable() {
      @Override public void run() {
        Logging.d(TAG, "onOutputFormatRequestOnCameraThread: " + width + "x" + height +
        "@" + framerate);
        capturerObserver.onOutputFormatRequest(width, height, framerate);
      }
    });
  }

  @Override
  public void changeCaptureFormat(int width, int height, int framerate) {
    Logging.d(TAG, "changeCaptureFormat: " + width + "x" + height + "@" + framerate);
    synchronized (stateLock) {
      stopCapture();
      startCapture(width, height, framerate);
    }
  }

  @Override
  public void dispose() {
    Logging.d(TAG, "dispose");
    stopCapture();
  }

  @Override
  public void switchCamera(final CameraSwitchHandler switchEventsHandler) {
    Logging.d(TAG, "switchCamera");

    final String[] deviceNames = cameraEnumerator.getDeviceNames();

    if (deviceNames.length < 2) {
      if (switchEventsHandler != null) {
        switchEventsHandler.onCameraSwitchError("No camera to switch to.");
      }
      return;
    }

    synchronized (stateLock) {
      if (switchInProgress) {
        Logging.d(TAG, "switchCamera switchInProgress");
        if (switchEventsHandler != null) {
          switchEventsHandler.onCameraSwitchError("Camera switch already in progress.");
        }
        return;
      }

      if (sessionOpening) {
        Logging.d(TAG, "switchCamera sessionOpening");
        if (switchEventsHandler != null) {
          switchEventsHandler.onCameraSwitchError("Session is still opening.");
        }
        return;
      }

      if (currentSession == null) {
        Logging.d(TAG, "switchCamera: No session open");
        if (switchEventsHandler != null) {
          switchEventsHandler.onCameraSwitchError("Camera is not running.");
        }
        return;
      }

      Logging.d(TAG, "switchCamera: Stopping session");
      currentSession.stop();
      currentSession = null;

      int cameraNameIndex = Arrays.asList(deviceNames).indexOf(cameraName);
      cameraName = deviceNames[(cameraNameIndex + 1) % deviceNames.length];

      switchInProgress = true;
      this.switchEventsHandler = switchEventsHandler;
      sessionOpening = true;
      openAttemptsRemaining = 1;
      createSessionInternal(0);
    }
    Logging.d(TAG, "switchCamera done");
  }

  protected String getCameraName() {
    synchronized (stateLock) {
      return cameraName;
    }
  }

  abstract protected void createCameraSession(
      CameraSession.CreateSessionCallback createSessionCallback,
      CameraEventsHandler eventsHandler, Context applicationContext,
      CameraVideoCapturer.CapturerObserver capturerObserver,
      SurfaceTextureHelper surfaceTextureHelper,
      String cameraName, int width, int height, int framerate);
}
