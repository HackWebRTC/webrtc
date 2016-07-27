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

import android.annotation.TargetApi;
import android.content.Context;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CameraMetadata;
import android.hardware.camera2.CaptureFailure;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.TotalCaptureResult;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Range;
import android.view.Surface;
import android.view.WindowManager;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.atomic.AtomicBoolean;

@TargetApi(21)
public class Camera2Capturer implements
    CameraVideoCapturer,
    SurfaceTextureHelper.OnTextureFrameAvailableListener {
  private final static String TAG = "Camera2Capturer";

  private final static int MAX_OPEN_CAMERA_ATTEMPTS = 3;
  private final static int OPEN_CAMERA_DELAY_MS = 500;
  private final static int STOP_TIMEOUT = 10000;
  private final static int START_TIMEOUT = 10000;
  private final static Object STOP_TIMEOUT_RUNNABLE_TOKEN = new Object();

  // In the Camera2 API, starting a camera is inherently asynchronous, and this state is
  // represented with 'STARTING'. Stopping is also asynchronous and this state is 'STOPPING'.
  private static enum CameraState { IDLE, STARTING, RUNNING, STOPPING }

  // Thread safe objects.
  // --------------------
  private final CameraManager cameraManager;
  private final CameraEventsHandler eventsHandler;

  // Set once in initialization(), before any other calls, so therefore thread safe.
  // ---------------------------------------------------------------------------------------------
  private SurfaceTextureHelper surfaceTextureHelper;
  private Context applicationContext;
  private CapturerObserver capturerObserver;
  // Use postOnCameraThread() instead of posting directly to the handler - this way all callbacks
  // with a specifed token can be removed at once.
  private Handler cameraThreadHandler;

  // Shared state - guarded by cameraStateLock. Will only be edited from camera thread (when it is
  // running).
  // ---------------------------------------------------------------------------------------------
  private final Object cameraStateLock = new Object();
  private volatile CameraState cameraState = CameraState.IDLE;
  // Remember the requested format in case we want to switch cameras.
  private int requestedWidth;
  private int requestedHeight;
  private int requestedFramerate;

  // Will only be edited while camera state is IDLE and cameraStateLock is acquired.
  private String cameraName;
  private boolean isFrontCamera;
  private int cameraOrientation;

  // Atomic boolean for allowing only one switch at a time.
  private final AtomicBoolean isPendingCameraSwitch = new AtomicBoolean();
  // Guarded by isPendingCameraSwitch.
  private CameraSwitchHandler switchEventsHandler;

  // Internal state - must only be modified from camera thread
  // ---------------------------------------------------------
  private CaptureFormat captureFormat;
  private CameraStatistics cameraStatistics;
  private CameraCaptureSession captureSession;
  private Surface surface;
  private CameraDevice cameraDevice;

  // Factor to convert between Android framerates and CaptureFormat.FramerateRange. It will be
  // either 1 or 1000.
  private int fpsUnitFactor;
  private boolean firstFrameReported;
  private int consecutiveCameraOpenFailures;

  public Camera2Capturer(
      Context context, String cameraName, CameraEventsHandler eventsHandler) {
    Logging.d(TAG, "Camera2Capturer ctor, camera name: " + cameraName);
    this.cameraManager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
    this.eventsHandler = eventsHandler;

    setCameraName(cameraName);
  }

  private boolean isOnCameraThread() {
    return Thread.currentThread() == cameraThreadHandler.getLooper().getThread();
  }

  /**
   * Helper method for checking method is executed on camera thread.
   */
  private void checkIsOnCameraThread() {
    if (!isOnCameraThread()) {
      throw new IllegalStateException("Not on camera thread");
    }
  }

  /**
   * Checks method is not invoked on the camera thread. Used in functions waiting for the camera
   * state to change since executing them on the camera thread would cause a deadlock.
   */
  private void checkNotOnCameraThread() {
    if (cameraThreadHandler == null) {
      return;
    }

    if (Thread.currentThread() == cameraThreadHandler.getLooper().getThread()) {
      throw new IllegalStateException(
          "Method waiting for camera state to change executed on camera thread");
    }
  }

  private void waitForCameraToExitTransitionalState(
      CameraState transitionalState, long timeoutMs) {
    checkNotOnCameraThread();

    // We probably should already have the lock when this is called but acquire it in case
    // we don't have it.
    synchronized (cameraStateLock) {
      long timeoutAt = SystemClock.uptimeMillis() + timeoutMs;

      while (cameraState == transitionalState) {
        Logging.d(TAG, "waitForCameraToExitTransitionalState waiting: "
            + cameraState);

        long timeLeft = timeoutAt - SystemClock.uptimeMillis();

        if (timeLeft <= 0) {
          Logging.e(TAG, "Camera failed to exit transitional state " + transitionalState
              + " within the time limit.");
          break;
        }

        try {
          cameraStateLock.wait(timeLeft);
        } catch (InterruptedException e) {
          Logging.w(TAG, "Trying to interrupt while waiting to exit transitional state "
            + transitionalState + ", ignoring: " + e);
        }
      }
    }
  }

  /**
   * Waits until camera state is not STOPPING.
   */
  private void waitForCameraToStopIfStopping() {
    waitForCameraToExitTransitionalState(CameraState.STOPPING, STOP_TIMEOUT);
  }

  /**
   * Wait until camera state is not STARTING.
   */
  private void waitForCameraToStartIfStarting() {
    waitForCameraToExitTransitionalState(CameraState.STARTING, START_TIMEOUT);
  }

  /**
   * Sets the name of the camera. Camera must be stopped or stopping when this is called.
   */
  private void setCameraName(String cameraName) {
    final CameraCharacteristics characteristics;
    try {
      final String[] cameraIds = cameraManager.getCameraIdList();

      if (cameraName.isEmpty() && cameraIds.length != 0) {
        cameraName = cameraIds[0];
      }

      if (!Arrays.asList(cameraIds).contains(cameraName)) {
        throw new IllegalArgumentException(
            "Camera name: " + cameraName + " does not match any known camera device:");
      }

      characteristics = cameraManager.getCameraCharacteristics(cameraName);
    } catch (CameraAccessException e) {
      throw new RuntimeException("Camera access exception: " + e);
    }

    synchronized (cameraStateLock) {
      waitForCameraToStopIfStopping();

      if (cameraState != CameraState.IDLE) {
        throw new RuntimeException("Changing camera name on running camera.");
      }

      // Note: Usually changing camera state from outside camera thread is not allowed. It is
      // allowed here because camera is not running.
      this.cameraName = cameraName;
      isFrontCamera = characteristics.get(CameraCharacteristics.LENS_FACING)
          == CameraMetadata.LENS_FACING_FRONT;

      /*
       * Clockwise angle through which the output image needs to be rotated to be upright on the
       * device screen in its native orientation.
       * Also defines the direction of rolling shutter readout, which is from top to bottom in the
       * sensor's coordinate system.
       * Units: Degrees of clockwise rotation; always a multiple of 90
       */
      cameraOrientation = characteristics.get(CameraCharacteristics.SENSOR_ORIENTATION);
    }
  }

  /**
   * Triggers appropriate error handlers based on the camera state. Must be called on the camera
   * thread and camera must not be stopped.
   */
  private void reportError(String errorDescription) {
    checkIsOnCameraThread();
    Logging.e(TAG, "Error in camera at state " + cameraState + ": " + errorDescription);

    if (switchEventsHandler != null) {
      switchEventsHandler.onCameraSwitchError(errorDescription);
      switchEventsHandler = null;
    }
    isPendingCameraSwitch.set(false);

    switch (cameraState) {
      case STARTING:
        capturerObserver.onCapturerStarted(false /* success */);
        // fall through
      case RUNNING:
        if (eventsHandler != null) {
          eventsHandler.onCameraError(errorDescription);
        }
        break;
      case STOPPING:
        setCameraState(CameraState.IDLE);
        Logging.e(TAG, "Closing camera failed: " + errorDescription);
        return; // We don't want to call closeAndRelease in this case.
      default:
        throw new RuntimeException("Unknown camera state: " + cameraState);
    }
    closeAndRelease();
  }

  private void closeAndRelease() {
    checkIsOnCameraThread();

    Logging.d(TAG, "Close and release.");
    setCameraState(CameraState.STOPPING);
    capturerObserver.onCapturerStopped();

    // Remove all pending Runnables posted from |this|.
    cameraThreadHandler.removeCallbacksAndMessages(this /* token */);
    if (cameraStatistics != null) {
      cameraStatistics.release();
      cameraStatistics = null;
    }
    if (surfaceTextureHelper != null) {
      surfaceTextureHelper.stopListening();
    }
    if (captureSession != null) {
      captureSession.close();
      captureSession = null;
    }
    if (surface != null) {
      surface.release();
      surface = null;
    }
    if (cameraDevice != null) {
      // Add a timeout for stopping the camera.
      cameraThreadHandler.postAtTime(new Runnable() {
        @Override
        public void run() {
          Logging.e(TAG, "Camera failed to stop within the timeout. Force stopping.");
          setCameraState(CameraState.IDLE);
          if (eventsHandler != null) {
            eventsHandler.onCameraError("Camera failed to stop (timeout).");
          }
        }
      }, STOP_TIMEOUT_RUNNABLE_TOKEN, SystemClock.uptimeMillis() + STOP_TIMEOUT);

      cameraDevice.close();
      cameraDevice = null;
    } else {
      Logging.w(TAG, "closeAndRelease called while cameraDevice is null");
      setCameraState(CameraState.IDLE);
    }
  }

  /**
   * Sets the camera state while ensuring constraints are followed.
   */
  private void setCameraState(CameraState newState) {
    // State must only be modified on the camera thread. It can be edited from other threads
    // if cameraState is IDLE since the camera thread is idle and not modifying the state.
    if (cameraState != CameraState.IDLE) {
      checkIsOnCameraThread();
    }

    switch (newState) {
      case STARTING:
        if (cameraState != CameraState.IDLE) {
          throw new IllegalStateException("Only stopped camera can start.");
        }
        break;
      case RUNNING:
        if (cameraState != CameraState.STARTING) {
          throw new IllegalStateException("Only starting camera can go to running state.");
        }
        break;
      case STOPPING:
        if (cameraState != CameraState.STARTING && cameraState != CameraState.RUNNING) {
          throw new IllegalStateException("Only starting or running camera can stop.");
        }
        break;
      case IDLE:
        if (cameraState != CameraState.STOPPING) {
          throw new IllegalStateException("Only stopping camera can go to idle state.");
        }
        break;
      default:
        throw new RuntimeException("Unknown camera state: " + newState);
    }

    synchronized (cameraStateLock) {
      cameraState = newState;
      cameraStateLock.notifyAll();
    }
  }

  /**
   * Internal method for opening the camera. Must be called on the camera thread.
   */
  private void openCamera() {
    try {
      checkIsOnCameraThread();

      if (cameraState != CameraState.STARTING) {
        throw new IllegalStateException("Camera should be in state STARTING in openCamera.");
      }

      // Camera is in state STARTING so cameraName will not be edited.
      cameraManager.openCamera(cameraName, new CameraStateCallback(), cameraThreadHandler);
    } catch (CameraAccessException e) {
      reportError("Failed to open camera: " + e);
    }
  }

  private boolean isInitialized() {
    return applicationContext != null && capturerObserver != null;
  }

  @Override
  public void initialize(SurfaceTextureHelper surfaceTextureHelper, Context applicationContext,
      CapturerObserver capturerObserver) {
    Logging.d(TAG, "initialize");
    if (applicationContext == null) {
      throw new IllegalArgumentException("applicationContext not set.");
    }
    if (capturerObserver == null) {
      throw new IllegalArgumentException("capturerObserver not set.");
    }
    if (isInitialized()) {
      throw new IllegalStateException("Already initialized");
    }
    this.applicationContext = applicationContext;
    this.capturerObserver = capturerObserver;
    this.surfaceTextureHelper = surfaceTextureHelper;
    this.cameraThreadHandler =
        surfaceTextureHelper == null ? null : surfaceTextureHelper.getHandler();
  }

  private void startCaptureOnCameraThread(
      final int requestedWidth, final int requestedHeight, final int requestedFramerate) {
    checkIsOnCameraThread();

    firstFrameReported = false;
    consecutiveCameraOpenFailures = 0;

    synchronized (cameraStateLock) {
      // Remember the requested format in case we want to switch cameras.
      this.requestedWidth = requestedWidth;
      this.requestedHeight = requestedHeight;
      this.requestedFramerate = requestedFramerate;
    }

    final CameraCharacteristics cameraCharacteristics;
    try {
      // Camera is in state STARTING so cameraName will not be edited.
      cameraCharacteristics = cameraManager.getCameraCharacteristics(cameraName);
    } catch (CameraAccessException e) {
      reportError("getCameraCharacteristics(): " + e.getMessage());
      return;
    }

    Range<Integer>[] fpsRanges =
        cameraCharacteristics.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
    fpsUnitFactor = Camera2Enumerator.getFpsUnitFactor(fpsRanges);
    List<CaptureFormat.FramerateRange> framerateRanges =
        Camera2Enumerator.convertFramerates(fpsRanges, fpsUnitFactor);
    List<Size> sizes = Camera2Enumerator.getSupportedSizes(cameraCharacteristics);

    if (framerateRanges.isEmpty() || sizes.isEmpty()) {
      reportError("No supported capture formats.");
    }
    final CaptureFormat.FramerateRange bestFpsRange =
        CameraEnumerationAndroid.getClosestSupportedFramerateRange(
            framerateRanges, requestedFramerate);

    final Size bestSize = CameraEnumerationAndroid.getClosestSupportedSize(
        sizes, requestedWidth, requestedHeight);

    this.captureFormat = new CaptureFormat(bestSize.width, bestSize.height, bestFpsRange);
    Logging.d(TAG, "Using capture format: " + captureFormat);

    Logging.d(TAG, "Opening camera " + cameraName);
    if (eventsHandler != null) {
      int cameraIndex = -1;
      try {
        cameraIndex = Integer.parseInt(cameraName);
      } catch (NumberFormatException e) {
        Logging.d(TAG, "External camera with non-int identifier: " + cameraName);
      }
      eventsHandler.onCameraOpening(cameraIndex);
    }

    openCamera();
  }

  /**
   * Starts capture using specified settings. This is automatically called for you by
   * VideoCapturerTrackSource if you are just using the camera as source for video track.
   */
  @Override
  public void startCapture(
      final int requestedWidth, final int requestedHeight, final int requestedFramerate) {
    Logging.d(TAG, "startCapture requested: " + requestedWidth + "x" + requestedHeight
        + "@" + requestedFramerate);
    if (!isInitialized()) {
      throw new IllegalStateException("startCapture called in uninitialized state");
    }
    if (surfaceTextureHelper == null) {
      capturerObserver.onCapturerStarted(false /* success */);
      if (eventsHandler != null) {
        eventsHandler.onCameraError("No SurfaceTexture created.");
      }
      return;
    }
    synchronized (cameraStateLock) {
      waitForCameraToStopIfStopping();
      if (cameraState != CameraState.IDLE) {
        Logging.e(TAG, "Unexpected camera state for startCapture: " + cameraState);
        return;
      }
      setCameraState(CameraState.STARTING);
    }

    postOnCameraThread(new Runnable() {
      @Override
      public void run() {
        startCaptureOnCameraThread(requestedWidth, requestedHeight, requestedFramerate);
      }
    });
  }

  final class CameraStateCallback extends CameraDevice.StateCallback {
    private String getErrorDescription(int errorCode) {
      switch (errorCode) {
        case CameraDevice.StateCallback.ERROR_CAMERA_DEVICE:
          return "Camera device has encountered a fatal error.";
        case CameraDevice.StateCallback.ERROR_CAMERA_DISABLED:
          return "Camera device could not be opened due to a device policy.";
        case CameraDevice.StateCallback.ERROR_CAMERA_IN_USE:
          return "Camera device is in use already.";
        case CameraDevice.StateCallback.ERROR_CAMERA_SERVICE:
          return "Camera service has encountered a fatal error.";
        case CameraDevice.StateCallback.ERROR_MAX_CAMERAS_IN_USE:
          return "Camera device could not be opened because"
              + " there are too many other open camera devices.";
        default:
          return "Unknown camera error: " + errorCode;
      }
    }

    @Override
    public void onDisconnected(CameraDevice camera) {
      checkIsOnCameraThread();
      cameraDevice = camera;
      reportError("Camera disconnected.");
    }

    @Override
    public void onError(CameraDevice camera, int errorCode) {
      checkIsOnCameraThread();
      cameraDevice = camera;

      if (cameraState == CameraState.STARTING && (
          errorCode == CameraDevice.StateCallback.ERROR_CAMERA_IN_USE ||
          errorCode == CameraDevice.StateCallback.ERROR_MAX_CAMERAS_IN_USE)) {
        consecutiveCameraOpenFailures++;

        if (consecutiveCameraOpenFailures < MAX_OPEN_CAMERA_ATTEMPTS) {
          Logging.w(TAG, "Opening camera failed, trying again: " + getErrorDescription(errorCode));

          postDelayedOnCameraThread(OPEN_CAMERA_DELAY_MS, new Runnable() {
            @Override
            public void run() {
              openCamera();
            }
          });
          return;
        } else {
          Logging.e(TAG, "Opening camera failed too many times. Passing the error.");
        }
      }

      reportError(getErrorDescription(errorCode));
    }

    @Override
    public void onOpened(CameraDevice camera) {
      checkIsOnCameraThread();

      Logging.d(TAG, "Camera opened.");
      if (cameraState != CameraState.STARTING) {
        throw new IllegalStateException("Unexpected state when camera opened: " + cameraState);
      }

      cameraDevice = camera;
      final SurfaceTexture surfaceTexture = surfaceTextureHelper.getSurfaceTexture();
      surfaceTexture.setDefaultBufferSize(captureFormat.width, captureFormat.height);
      surface = new Surface(surfaceTexture);
      try {
        camera.createCaptureSession(
            Arrays.asList(surface), new CaptureSessionCallback(), cameraThreadHandler);
      } catch (CameraAccessException e) {
        reportError("Failed to create capture session. " + e);
      }
    }

    @Override
    public void onClosed(CameraDevice camera) {
      checkIsOnCameraThread();

      Logging.d(TAG, "Camera device closed.");

      if (cameraState != CameraState.STOPPING) {
        Logging.e(TAG, "Camera state was not STOPPING in onClosed. Most likely camera didn't stop "
            + "within timelimit and this method was invoked twice.");
        return;
      }

      cameraThreadHandler.removeCallbacksAndMessages(STOP_TIMEOUT_RUNNABLE_TOKEN);
      setCameraState(CameraState.IDLE);
      if (eventsHandler != null) {
        eventsHandler.onCameraClosed();
      }
    }
  }

  final class CaptureSessionCallback extends CameraCaptureSession.StateCallback {
    @Override
    public void onConfigureFailed(CameraCaptureSession session) {
      checkIsOnCameraThread();
      captureSession = session;
      reportError("Failed to configure capture session.");
    }

    @Override
    public void onConfigured(CameraCaptureSession session) {
      checkIsOnCameraThread();
      Logging.d(TAG, "Camera capture session configured.");
      captureSession = session;
      try {
        /*
         * The viable options for video capture requests are:
         * TEMPLATE_PREVIEW: High frame rate is given priority over the highest-quality
         *   post-processing.
         * TEMPLATE_RECORD: Stable frame rate is used, and post-processing is set for recording
         *   quality.
         */
        final CaptureRequest.Builder captureRequestBuilder =
            cameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_RECORD);
        // Set auto exposure fps range.
        captureRequestBuilder.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE, new Range<Integer>(
            captureFormat.framerate.min / fpsUnitFactor,
            captureFormat.framerate.max / fpsUnitFactor));
        captureRequestBuilder.set(CaptureRequest.CONTROL_AE_MODE,
            CaptureRequest.CONTROL_AE_MODE_ON);
        captureRequestBuilder.set(CaptureRequest.CONTROL_AE_LOCK, false);

        captureRequestBuilder.addTarget(surface);
        session.setRepeatingRequest(
            captureRequestBuilder.build(), new CameraCaptureCallback(), cameraThreadHandler);
      } catch (CameraAccessException e) {
        reportError("Failed to start capture request. " + e);
        return;
      }

      Logging.d(TAG, "Camera device successfully started.");
      surfaceTextureHelper.startListening(Camera2Capturer.this);
      capturerObserver.onCapturerStarted(true /* success */);
      cameraStatistics = new CameraStatistics(surfaceTextureHelper, eventsHandler);
      setCameraState(CameraState.RUNNING);

      if (switchEventsHandler != null) {
        switchEventsHandler.onCameraSwitchDone(isFrontCamera);
        switchEventsHandler = null;
      }
      isPendingCameraSwitch.set(false);
    }
  }

  final class CameraCaptureCallback extends CameraCaptureSession.CaptureCallback {
    static final int MAX_CONSECUTIVE_CAMERA_CAPTURE_FAILURES = 10;
    int consecutiveCameraCaptureFailures;

    @Override
    public void onCaptureFailed(
        CameraCaptureSession session, CaptureRequest request, CaptureFailure failure) {
      checkIsOnCameraThread();
      ++consecutiveCameraCaptureFailures;
      if (consecutiveCameraCaptureFailures > MAX_CONSECUTIVE_CAMERA_CAPTURE_FAILURES) {
        reportError("Capture failed " + consecutiveCameraCaptureFailures + " consecutive times.");
      }
    }

    @Override
    public void onCaptureCompleted(
          CameraCaptureSession session, CaptureRequest request, TotalCaptureResult result) {
      // TODO(sakal): This sometimes gets called after camera has stopped, investigate
      checkIsOnCameraThread();
      consecutiveCameraCaptureFailures = 0;
    }
  }



  // Switch camera to the next valid camera id. This can only be called while
  // the camera is running.
  @Override
  public void switchCamera(final CameraSwitchHandler switchEventsHandler) {
    final String[] cameraIds;
    try {
      cameraIds = cameraManager.getCameraIdList();
    } catch (CameraAccessException e) {
      if (switchEventsHandler != null) {
        switchEventsHandler.onCameraSwitchError("Could not get camera names: " + e);
      }
      return;
    }
    if (cameraIds.length < 2) {
      if (switchEventsHandler != null) {
        switchEventsHandler.onCameraSwitchError("No camera to switch to.");
      }
      return;
    }
    // Do not handle multiple camera switch request to avoid blocking camera thread by handling too
    // many switch request from a queue. We have to be careful to always release
    // |isPendingCameraSwitch| by setting it to false when done.
    if (isPendingCameraSwitch.getAndSet(true)) {
      Logging.w(TAG, "Ignoring camera switch request.");
      if (switchEventsHandler != null) {
        switchEventsHandler.onCameraSwitchError("Pending camera switch already in progress.");
      }
      return;
    }

    final String newCameraId;
    final int requestedWidth;
    final int requestedHeight;
    final int requestedFramerate;

    synchronized (cameraStateLock) {
      waitForCameraToStartIfStarting();

      if (cameraState != CameraState.RUNNING) {
        Logging.e(TAG, "Calling swithCamera() on stopped camera.");
        if (switchEventsHandler != null) {
          switchEventsHandler.onCameraSwitchError("Camera is stopped.");
        }
        isPendingCameraSwitch.set(false);
        return;
      }

      // Calculate new camera index and camera id. Camera is in state RUNNING so cameraName will
      // not be edited.
      final int currentCameraIndex = Arrays.asList(cameraIds).indexOf(cameraName);
      if (currentCameraIndex == -1) {
        Logging.e(TAG, "Couldn't find current camera id " + cameraName
            + " in list of camera ids: " + Arrays.toString(cameraIds));
      }
      final int newCameraIndex = (currentCameraIndex + 1) % cameraIds.length;
      newCameraId = cameraIds[newCameraIndex];

      requestedWidth = this.requestedWidth;
      requestedHeight = this.requestedHeight;
      requestedFramerate = this.requestedFramerate;
      this.switchEventsHandler = switchEventsHandler;
    }

    // Make the switch.
    stopCapture();
    setCameraName(newCameraId);
    startCapture(requestedWidth, requestedHeight, requestedFramerate);

    // Note: switchEventsHandler will be called from onConfigured / reportError.
  }

  // Requests a new output format from the video capturer. Captured frames
  // by the camera will be scaled/or dropped by the video capturer.
  // It does not matter if width and height are flipped. I.E, |width| = 640, |height| = 480 produce
  // the same result as |width| = 480, |height| = 640.
  // TODO(magjed/perkj): Document what this function does. Change name?
  @Override
  public void onOutputFormatRequest(final int width, final int height, final int framerate) {
    postOnCameraThread(new Runnable() {
      @Override
      public void run() {
        Logging.d(TAG,
            "onOutputFormatRequestOnCameraThread: " + width + "x" + height + "@" + framerate);
        capturerObserver.onOutputFormatRequest(width, height, framerate);
      }
    });
  }

  // Reconfigure the camera to capture in a new format. This should only be called while the camera
  // is running.
  @Override
  public void changeCaptureFormat(final int width, final int height, final int framerate) {
    synchronized (cameraStateLock) {
      waitForCameraToStartIfStarting();

      if (cameraState != CameraState.RUNNING) {
        Logging.e(TAG, "Calling changeCaptureFormat() on stopped camera.");
        return;
      }

      requestedWidth = width;
      requestedHeight = height;
      requestedFramerate = framerate;
    }

    // Make the switch.
    stopCapture();
    // TODO(magjed/sakal): Just recreate session.
    startCapture(width, height, framerate);
  }

  @Override
  public List<CaptureFormat> getSupportedFormats() {
    synchronized (cameraState) {
      return Camera2Enumerator.getSupportedFormats(this.cameraManager, cameraName);
    }
  }

  @Override
  public void dispose() {
    synchronized (cameraStateLock) {
      waitForCameraToStopIfStopping();

      if (cameraState != CameraState.IDLE) {
        throw new IllegalStateException("Unexpected camera state for dispose: " + cameraState);
      }
    }
  }

  // Blocks until camera is known to be stopped.
  @Override
  public void stopCapture() {
    final CountDownLatch cameraStoppingLatch = new CountDownLatch(1);

    Logging.d(TAG, "stopCapture");
    checkNotOnCameraThread();

    synchronized (cameraStateLock) {
      waitForCameraToStartIfStarting();

      if (cameraState != CameraState.RUNNING) {
        Logging.w(TAG, "stopCapture called for already stopped camera.");
        return;
      }

      postOnCameraThread(new Runnable() {
        @Override
        public void run() {
          Logging.d(TAG, "stopCaptureOnCameraThread");

          // Stop capture.
          closeAndRelease();
          cameraStoppingLatch.countDown();
        }
      });
    }

    // Wait for the stopping to start
    ThreadUtils.awaitUninterruptibly(cameraStoppingLatch);

    Logging.d(TAG, "stopCapture done");
  }

  private void postOnCameraThread(Runnable runnable) {
    postDelayedOnCameraThread(0 /* delayMs */, runnable);
  }

  private void postDelayedOnCameraThread(int delayMs, Runnable runnable) {
    synchronized (cameraStateLock) {
      if ((cameraState != CameraState.STARTING && cameraState != CameraState.RUNNING)
          || !cameraThreadHandler.postAtTime(
              runnable, this /* token */, SystemClock.uptimeMillis() + delayMs)) {
        Logging.w(TAG, "Runnable not scheduled even though it was requested.");
      }
    }
  }

  private int getDeviceOrientation() {
    int orientation = 0;

    WindowManager wm = (WindowManager) applicationContext.getSystemService(
        Context.WINDOW_SERVICE);
    switch(wm.getDefaultDisplay().getRotation()) {
      case Surface.ROTATION_90:
        orientation = 90;
        break;
      case Surface.ROTATION_180:
        orientation = 180;
        break;
      case Surface.ROTATION_270:
        orientation = 270;
        break;
      case Surface.ROTATION_0:
      default:
        orientation = 0;
        break;
    }
    return orientation;
  }

  @Override
  public void onTextureFrameAvailable(
      int oesTextureId, float[] transformMatrix, long timestampNs) {
    checkIsOnCameraThread();

    if (cameraState != CameraState.RUNNING) {
      Logging.d(TAG, "Texture frame received while camera was not running.");
      return;
    }

    if (eventsHandler != null && !firstFrameReported) {
      eventsHandler.onFirstFrameAvailable();
      firstFrameReported = true;
    }

    int rotation;
    if (isFrontCamera) {
      // Undo the mirror that the OS "helps" us with.
      // http://developer.android.com/reference/android/hardware/Camera.html#setDisplayOrientation(int)
      rotation = cameraOrientation + getDeviceOrientation();
      transformMatrix =
          RendererCommon.multiplyMatrices(transformMatrix, RendererCommon.horizontalFlipMatrix());
    } else {
      rotation = cameraOrientation - getDeviceOrientation();
    }
    // Make sure |rotation| is between 0 and 360.
    rotation = (360 + rotation % 360) % 360;

    // Undo camera orientation - we report it as rotation instead.
    transformMatrix = RendererCommon.rotateTextureMatrix(transformMatrix, -cameraOrientation);

    cameraStatistics.addFrame();
    capturerObserver.onTextureFrameCaptured(captureFormat.width, captureFormat.height, oesTextureId,
        transformMatrix, rotation, timestampNs);
  }
}
