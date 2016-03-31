/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
package org.webrtc;

import android.content.Context;
import android.hardware.Camera;

import org.webrtc.VideoCapturerAndroidTestFixtures;
import org.webrtc.CameraEnumerationAndroid.CaptureFormat;
import org.webrtc.VideoRenderer.I420Frame;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;

import static junit.framework.Assert.*;

public class VideoCapturerAndroidTestFixtures {
  static class RendererCallbacks implements VideoRenderer.Callbacks {
    private int framesRendered = 0;
    private Object frameLock = 0;
    private int width = 0;
    private int height = 0;

    @Override
    public void renderFrame(I420Frame frame) {
      synchronized (frameLock) {
        ++framesRendered;
        width = frame.rotatedWidth();
        height = frame.rotatedHeight();
        frameLock.notify();
      }
      VideoRenderer.renderFrameDone(frame);
    }

    public int frameWidth() {
      synchronized (frameLock) {
        return width;
      }
    }

    public int frameHeight() {
      synchronized (frameLock) {
        return height;
      }
    }

    public int WaitForNextFrameToRender() throws InterruptedException {
      synchronized (frameLock) {
        frameLock.wait();
        return framesRendered;
      }
    }
  }

  static class FakeAsyncRenderer implements VideoRenderer.Callbacks {
    private final List<I420Frame> pendingFrames = new ArrayList<I420Frame>();

    @Override
    public void renderFrame(I420Frame frame) {
      synchronized (pendingFrames) {
        pendingFrames.add(frame);
        pendingFrames.notifyAll();
      }
    }

    // Wait until at least one frame have been received, before returning them.
    public List<I420Frame> waitForPendingFrames() throws InterruptedException {
      synchronized (pendingFrames) {
        while (pendingFrames.isEmpty()) {
          pendingFrames.wait();
        }
        return new ArrayList<I420Frame>(pendingFrames);
      }
    }
  }

  static class FakeCapturerObserver implements VideoCapturer.CapturerObserver {
    private int framesCaptured = 0;
    private int frameSize = 0;
    private int frameWidth = 0;
    private int frameHeight = 0;
    private Object frameLock = 0;
    private Object capturerStartLock = 0;
    private boolean captureStartResult = false;
    private List<Long> timestamps = new ArrayList<Long>();

    @Override
    public void onCapturerStarted(boolean success) {
      synchronized (capturerStartLock) {
        captureStartResult = success;
        capturerStartLock.notify();
      }
    }

    @Override
    public void onByteBufferFrameCaptured(byte[] frame, int width, int height, int rotation,
        long timeStamp) {
      synchronized (frameLock) {
        ++framesCaptured;
        frameSize = frame.length;
        frameWidth = width;
        frameHeight = height;
        timestamps.add(timeStamp);
        frameLock.notify();
      }
    }
    @Override
    public void onTextureFrameCaptured(
        int width, int height, int oesTextureId, float[] transformMatrix, int rotation,
        long timeStamp) {
      synchronized (frameLock) {
        ++framesCaptured;
        frameWidth = width;
        frameHeight = height;
        frameSize = 0;
        timestamps.add(timeStamp);
        frameLock.notify();
      }
    }

    @Override
    public void onOutputFormatRequest(int width, int height, int fps) {}

    public boolean WaitForCapturerToStart() throws InterruptedException {
      synchronized (capturerStartLock) {
        capturerStartLock.wait();
        return captureStartResult;
      }
    }

    public int WaitForNextCapturedFrame() throws InterruptedException {
      synchronized (frameLock) {
        frameLock.wait();
        return framesCaptured;
      }
    }

    int frameSize() {
      synchronized (frameLock) {
        return frameSize;
      }
    }

    int frameWidth() {
      synchronized (frameLock) {
        return frameWidth;
      }
    }

    int frameHeight() {
      synchronized (frameLock) {
        return frameHeight;
      }
    }

    List<Long> getCopyAndResetListOftimeStamps() {
      synchronized (frameLock) {
        ArrayList<Long> list = new ArrayList<Long>(timestamps);
        timestamps.clear();
        return list;
      }
    }
  }

  static class CameraEvents implements
      VideoCapturerAndroid.CameraEventsHandler {
    public boolean onCameraOpeningCalled;
    public boolean onFirstFrameAvailableCalled;
    public final Object onCameraFreezedLock = new Object();
    private String onCameraFreezedDescription;

    @Override
    public void onCameraError(String errorDescription) {
    }

    @Override
    public void onCameraFreezed(String errorDescription) {
      synchronized (onCameraFreezedLock) {
        onCameraFreezedDescription = errorDescription;
        onCameraFreezedLock.notifyAll();
      }
    }

    @Override
    public void onCameraOpening(int cameraId) {
      onCameraOpeningCalled = true;
    }

    @Override
    public void onFirstFrameAvailable() {
      onFirstFrameAvailableCalled = true;
    }

    @Override
    public void onCameraClosed() { }

    public String WaitForCameraFreezed() throws InterruptedException {
      synchronized (onCameraFreezedLock) {
        onCameraFreezedLock.wait();
        return onCameraFreezedDescription;
      }
    }
  }

  static public CameraEvents createCameraEvents() {
    return new CameraEvents();
  }

  // Return true if the device under test have at least two cameras.
  @SuppressWarnings("deprecation")
  static public boolean HaveTwoCameras() {
    return (Camera.getNumberOfCameras() >= 2);
  }

  static public void release(VideoCapturerAndroid capturer) {
    assertNotNull(capturer);
    capturer.dispose();
    assertTrue(capturer.isDisposed());
  }

  static public void startCapturerAndRender(VideoCapturerAndroid capturer)
      throws InterruptedException {
    PeerConnectionFactory factory = new PeerConnectionFactory();
    VideoSource source =
        factory.createVideoSource(capturer, new MediaConstraints());
    VideoTrack track = factory.createVideoTrack("dummy", source);
    RendererCallbacks callbacks = new RendererCallbacks();
    track.addRenderer(new VideoRenderer(callbacks));
    assertTrue(callbacks.WaitForNextFrameToRender() > 0);
    track.dispose();
    source.dispose();
    factory.dispose();
    assertTrue(capturer.isDisposed());
  }

  static public void switchCamera(VideoCapturerAndroid capturer) throws InterruptedException {
    PeerConnectionFactory factory = new PeerConnectionFactory();
    VideoSource source =
        factory.createVideoSource(capturer, new MediaConstraints());
    VideoTrack track = factory.createVideoTrack("dummy", source);

    // Array with one element to avoid final problem in nested classes.
    final boolean[] cameraSwitchSuccessful = new boolean[1];
    final CountDownLatch barrier = new CountDownLatch(1);
    capturer.switchCamera(new VideoCapturerAndroid.CameraSwitchHandler() {
      @Override
      public void onCameraSwitchDone(boolean isFrontCamera) {
        cameraSwitchSuccessful[0] = true;
        barrier.countDown();
      }
      @Override
      public void onCameraSwitchError(String errorDescription) {
        cameraSwitchSuccessful[0] = false;
        barrier.countDown();
      }
    });
    // Wait until the camera has been switched.
    barrier.await();

    // Check result.
    if (HaveTwoCameras()) {
      assertTrue(cameraSwitchSuccessful[0]);
    } else {
      assertFalse(cameraSwitchSuccessful[0]);
    }
    // Ensure that frames are received.
    RendererCallbacks callbacks = new RendererCallbacks();
    track.addRenderer(new VideoRenderer(callbacks));
    assertTrue(callbacks.WaitForNextFrameToRender() > 0);
    track.dispose();
    source.dispose();
    factory.dispose();
    assertTrue(capturer.isDisposed());
  }

  static public void cameraEventsInvoked(VideoCapturerAndroid capturer, CameraEvents events,
      Context appContext) throws InterruptedException {
    final List<CaptureFormat> formats = capturer.getSupportedFormats();
    final CameraEnumerationAndroid.CaptureFormat format = formats.get(0);

    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, null /* sharedContext */);
    final FakeCapturerObserver observer = new FakeCapturerObserver();
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        surfaceTextureHelper, appContext, observer);
    // Make sure camera is started and first frame is received and then stop it.
    assertTrue(observer.WaitForCapturerToStart());
    observer.WaitForNextCapturedFrame();
    capturer.stopCapture();
    if (capturer.isCapturingToTexture()) {
      surfaceTextureHelper.returnTextureFrame();
    }
    release(capturer);
    surfaceTextureHelper.dispose();

    assertTrue(events.onCameraOpeningCalled);
    assertTrue(events.onFirstFrameAvailableCalled);
  }

  static public void cameraCallsAfterStop(
      VideoCapturerAndroid capturer, Context appContext) throws InterruptedException {
    final List<CaptureFormat> formats = capturer.getSupportedFormats();
    final CameraEnumerationAndroid.CaptureFormat format = formats.get(0);

    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, null /* sharedContext */);
    final FakeCapturerObserver observer = new FakeCapturerObserver();
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        surfaceTextureHelper, appContext, observer);
    // Make sure camera is started and then stop it.
    assertTrue(observer.WaitForCapturerToStart());
    capturer.stopCapture();
    if (capturer.isCapturingToTexture()) {
      surfaceTextureHelper.returnTextureFrame();
    }

    // We can't change |capturer| at this point, but we should not crash.
    capturer.switchCamera(null);
    capturer.onOutputFormatRequest(640, 480, 15);
    capturer.changeCaptureFormat(640, 480, 15);

    release(capturer);
    surfaceTextureHelper.dispose();
  }

  static public void stopRestartVideoSource(VideoCapturerAndroid capturer)
      throws InterruptedException {
    PeerConnectionFactory factory = new PeerConnectionFactory();
    VideoSource source =
        factory.createVideoSource(capturer, new MediaConstraints());
    VideoTrack track = factory.createVideoTrack("dummy", source);
    RendererCallbacks callbacks = new RendererCallbacks();
    track.addRenderer(new VideoRenderer(callbacks));
    assertTrue(callbacks.WaitForNextFrameToRender() > 0);
    assertEquals(MediaSource.State.LIVE, source.state());

    source.stop();
    assertEquals(MediaSource.State.ENDED, source.state());

    source.restart();
    assertTrue(callbacks.WaitForNextFrameToRender() > 0);
    assertEquals(MediaSource.State.LIVE, source.state());
    track.dispose();
    source.dispose();
    factory.dispose();
    assertTrue(capturer.isDisposed());
  }

  static public void startStopWithDifferentResolutions(VideoCapturerAndroid capturer,
      Context appContext) throws InterruptedException {
    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, null /* sharedContext */);
    FakeCapturerObserver observer = new FakeCapturerObserver();
    List<CaptureFormat> formats = capturer.getSupportedFormats();

    for(int i = 0; i < 3 ; ++i) {
      CameraEnumerationAndroid.CaptureFormat format = formats.get(i);
      capturer.startCapture(format.width, format.height, format.maxFramerate,
          surfaceTextureHelper, appContext, observer);
      assertTrue(observer.WaitForCapturerToStart());
      observer.WaitForNextCapturedFrame();

      // Check the frame size. The actual width and height depend on how the capturer is mounted.
      final boolean identicalResolution = (observer.frameWidth() == format.width
          &&  observer.frameHeight() == format.height);
      final boolean flippedResolution = (observer.frameWidth() == format.height
          && observer.frameHeight() == format.width);
      if (!identicalResolution && !flippedResolution) {
        fail("Wrong resolution, got: " + observer.frameWidth() + "x" + observer.frameHeight()
            + " expected: " + format.width + "x" + format.height + " or " + format.height + "x"
            + format.width);
      }

      if (capturer.isCapturingToTexture()) {
        assertEquals(0, observer.frameSize());
      } else {
        assertTrue(format.frameSize() <= observer.frameSize());
      }
      capturer.stopCapture();
      if (capturer.isCapturingToTexture()) {
        surfaceTextureHelper.returnTextureFrame();
      }
    }
    release(capturer);
    surfaceTextureHelper.dispose();
  }

  static void waitUntilIdle(VideoCapturerAndroid capturer) throws InterruptedException {
    final CountDownLatch barrier = new CountDownLatch(1);
    capturer.getCameraThreadHandler().post(new Runnable() {
        @Override public void run() {
          barrier.countDown();
        }
    });
    barrier.await();
  }

  static public void startWhileCameraIsAlreadyOpen(
      VideoCapturerAndroid capturer, Context appContext) throws InterruptedException {
    final List<CaptureFormat> formats = capturer.getSupportedFormats();
    final CameraEnumerationAndroid.CaptureFormat format = formats.get(0);
    Camera camera = Camera.open(capturer.getCurrentCameraId());

    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, null /* sharedContext */);
    final FakeCapturerObserver observer = new FakeCapturerObserver();
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        surfaceTextureHelper, appContext, observer);

    if (android.os.Build.VERSION.SDK_INT > android.os.Build.VERSION_CODES.LOLLIPOP_MR1) {
      // The first opened camera client will be evicted.
      assertTrue(observer.WaitForCapturerToStart());
      capturer.stopCapture();
    } else {
      assertFalse(observer.WaitForCapturerToStart());
    }

    release(capturer);
    camera.release();
    surfaceTextureHelper.dispose();
  }

  static public void startWhileCameraIsAlreadyOpenAndCloseCamera(
      VideoCapturerAndroid capturer, Context appContext) throws InterruptedException {
    final List<CaptureFormat> formats = capturer.getSupportedFormats();
    final CameraEnumerationAndroid.CaptureFormat format = formats.get(0);
    Camera camera = Camera.open(capturer.getCurrentCameraId());

    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, null /* sharedContext */);
    final FakeCapturerObserver observer = new FakeCapturerObserver();
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        surfaceTextureHelper, appContext, observer);
    waitUntilIdle(capturer);

    camera.release();

    // Make sure camera is started and first frame is received and then stop it.
    assertTrue(observer.WaitForCapturerToStart());
    observer.WaitForNextCapturedFrame();
    capturer.stopCapture();
    if (capturer.isCapturingToTexture()) {
      surfaceTextureHelper.returnTextureFrame();
    }
    release(capturer);
    surfaceTextureHelper.dispose();
  }

  static public void startWhileCameraIsAlreadyOpenAndStop(
      VideoCapturerAndroid capturer, Context appContext) throws InterruptedException {
    final List<CaptureFormat> formats = capturer.getSupportedFormats();
    final CameraEnumerationAndroid.CaptureFormat format = formats.get(0);
    Camera camera = Camera.open(capturer.getCurrentCameraId());

    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, null /* sharedContext */);
    final FakeCapturerObserver observer = new FakeCapturerObserver();
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        surfaceTextureHelper, appContext, observer);
    capturer.stopCapture();
    release(capturer);
    camera.release();
    surfaceTextureHelper.dispose();
  }

  static public void returnBufferLate(VideoCapturerAndroid capturer,
      Context appContext) throws InterruptedException {
    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, null /* sharedContext */);
    FakeCapturerObserver observer = new FakeCapturerObserver();

    List<CaptureFormat> formats = capturer.getSupportedFormats();
    CameraEnumerationAndroid.CaptureFormat format = formats.get(0);
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        surfaceTextureHelper, appContext, observer);
    assertTrue(observer.WaitForCapturerToStart());

    observer.WaitForNextCapturedFrame();
    capturer.stopCapture();
    List<Long> listOftimestamps = observer.getCopyAndResetListOftimeStamps();
    assertTrue(listOftimestamps.size() >= 1);

    format = formats.get(1);
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        surfaceTextureHelper, appContext, observer);
    observer.WaitForCapturerToStart();
    if (capturer.isCapturingToTexture()) {
      surfaceTextureHelper.returnTextureFrame();
    }

    observer.WaitForNextCapturedFrame();
    capturer.stopCapture();

    listOftimestamps = observer.getCopyAndResetListOftimeStamps();
    assertTrue(listOftimestamps.size() >= 1);
    if (capturer.isCapturingToTexture()) {
      surfaceTextureHelper.returnTextureFrame();
    }

    release(capturer);
    surfaceTextureHelper.dispose();
  }

  static public void returnBufferLateEndToEnd(VideoCapturerAndroid capturer)
      throws InterruptedException {
    final PeerConnectionFactory factory = new PeerConnectionFactory();
    final VideoSource source = factory.createVideoSource(capturer, new MediaConstraints());
    final VideoTrack track = factory.createVideoTrack("dummy", source);
    final FakeAsyncRenderer renderer = new FakeAsyncRenderer();

    track.addRenderer(new VideoRenderer(renderer));
    // Wait for at least one frame that has not been returned.
    assertFalse(renderer.waitForPendingFrames().isEmpty());

    capturer.stopCapture();

    // Dispose everything.
    track.dispose();
    source.dispose();
    factory.dispose();
    assertTrue(capturer.isDisposed());

    // Return the frame(s), on a different thread out of spite.
    final List<I420Frame> pendingFrames = renderer.waitForPendingFrames();
    final Thread returnThread = new Thread(new Runnable() {
      @Override
      public void run() {
        for (I420Frame frame : pendingFrames) {
          VideoRenderer.renderFrameDone(frame);
        }
      }
    });
    returnThread.start();
    returnThread.join();
  }

  static public void cameraFreezedEventOnBufferStarvationUsingTextures(
      VideoCapturerAndroid capturer,
      CameraEvents events, Context appContext) throws InterruptedException {
    assertTrue("Not capturing to textures.", capturer.isCapturingToTexture());

    final List<CaptureFormat> formats = capturer.getSupportedFormats();
    final CameraEnumerationAndroid.CaptureFormat format = formats.get(0);

    final SurfaceTextureHelper surfaceTextureHelper = SurfaceTextureHelper.create(
        "SurfaceTextureHelper test" /* threadName */, null /* sharedContext */);
    final FakeCapturerObserver observer = new FakeCapturerObserver();
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        surfaceTextureHelper, appContext, observer);
    // Make sure camera is started.
    assertTrue(observer.WaitForCapturerToStart());
    // Since we don't return the buffer, we should get a starvation message if we are
    // capturing to a texture.
    assertEquals("Camera failure. Client must return video buffers.",
        events.WaitForCameraFreezed());

    capturer.stopCapture();
    if (capturer.isCapturingToTexture()) {
      surfaceTextureHelper.returnTextureFrame();
    }

    release(capturer);
    surfaceTextureHelper.dispose();
  }

  static public void scaleCameraOutput(VideoCapturerAndroid capturer) throws InterruptedException {
    PeerConnectionFactory factory = new PeerConnectionFactory();
    VideoSource source =
        factory.createVideoSource(capturer, new MediaConstraints());
    VideoTrack track = factory.createVideoTrack("dummy", source);
    RendererCallbacks renderer = new RendererCallbacks();
    track.addRenderer(new VideoRenderer(renderer));
    assertTrue(renderer.WaitForNextFrameToRender() > 0);

    final int startWidth = renderer.frameWidth();
    final int startHeight = renderer.frameHeight();
    final int frameRate = 30;
    final int scaledWidth = startWidth / 2;
    final int scaledHeight = startHeight / 2;

    // Request the captured frames to be scaled.
    capturer.onOutputFormatRequest(scaledWidth, scaledHeight, frameRate);

    boolean gotExpectedResolution = false;
    int numberOfInspectedFrames = 0;

    do {
      renderer.WaitForNextFrameToRender();
      ++numberOfInspectedFrames;

      gotExpectedResolution = (renderer.frameWidth() == scaledWidth
          &&  renderer.frameHeight() == scaledHeight);
    } while (!gotExpectedResolution && numberOfInspectedFrames < 30);

    source.stop();
    track.dispose();
    source.dispose();
    factory.dispose();
    assertTrue(capturer.isDisposed());

    assertTrue(gotExpectedResolution);
  }

}
