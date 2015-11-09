/*
 * libjingle
 * Copyright 2015 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

    @Override
    public void renderFrame(I420Frame frame) {
      synchronized (frameLock) {
        ++framesRendered;
        frameLock.notify();
      }
      VideoRenderer.renderFrameDone(frame);
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

  static class FakeCapturerObserver implements
      VideoCapturerAndroid.CapturerObserver {
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
    public void onByteBufferFrameCaptured(byte[] frame, int length, int width, int height,
        int rotation, long timeStamp) {
      synchronized (frameLock) {
        ++framesCaptured;
        frameSize = length;
        frameWidth = width;
        frameHeight = height;
        timestamps.add(timeStamp);
        frameLock.notify();
      }
    }
    @Override
    public void onTextureFrameCaptured(
        int width, int height, int oesTextureId, float[] transformMatrix, long timeStamp) {
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
    public final Object onCameraErrorLock = new Object();
    private String onCameraErrorDescription;

    @Override
    public void onCameraError(String errorDescription) {
      synchronized (onCameraErrorLock) {
        onCameraErrorDescription = errorDescription;
        onCameraErrorLock.notifyAll();
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

    public String WaitForCameraError() throws InterruptedException {
      synchronized (onCameraErrorLock) {
        onCameraErrorLock.wait();
        return onCameraErrorDescription;
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
    assertTrue(capturer.isReleased());
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
    assertTrue(capturer.isReleased());
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
    assertTrue(capturer.isReleased());
  }

  static public void cameraEventsInvoked(VideoCapturerAndroid capturer, CameraEvents events,
      Context appContext) throws InterruptedException {
    final List<CaptureFormat> formats = capturer.getSupportedFormats();
    final CameraEnumerationAndroid.CaptureFormat format = formats.get(0);

    final FakeCapturerObserver observer = new FakeCapturerObserver();
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        appContext, observer);
    // Make sure camera is started and first frame is received and then stop it.
    assertTrue(observer.WaitForCapturerToStart());
    observer.WaitForNextCapturedFrame();
    capturer.stopCapture();
    for (long timeStamp : observer.getCopyAndResetListOftimeStamps()) {
      capturer.returnBuffer(timeStamp);
    }
    capturer.dispose();

    assertTrue(capturer.isReleased());
    assertTrue(events.onCameraOpeningCalled);
    assertTrue(events.onFirstFrameAvailableCalled);
  }

  static public void cameraCallsAfterStop(
      VideoCapturerAndroid capturer, Context appContext) throws InterruptedException {
    final List<CaptureFormat> formats = capturer.getSupportedFormats();
    final CameraEnumerationAndroid.CaptureFormat format = formats.get(0);

    final FakeCapturerObserver observer = new FakeCapturerObserver();
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        appContext, observer);
    // Make sure camera is started and then stop it.
    assertTrue(observer.WaitForCapturerToStart());
    capturer.stopCapture();
    for (long timeStamp : observer.getCopyAndResetListOftimeStamps()) {
      capturer.returnBuffer(timeStamp);
    }
    // We can't change |capturer| at this point, but we should not crash.
    capturer.switchCamera(null);
    capturer.onOutputFormatRequest(640, 480, 15);
    capturer.changeCaptureFormat(640, 480, 15);

    capturer.dispose();
    assertTrue(capturer.isReleased());
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
    assertTrue(capturer.isReleased());
  }

  static public void startStopWithDifferentResolutions(VideoCapturerAndroid capturer,
      Context appContext) throws InterruptedException {
    FakeCapturerObserver observer = new FakeCapturerObserver();
    List<CaptureFormat> formats = capturer.getSupportedFormats();

    for(int i = 0; i < 3 ; ++i) {
      CameraEnumerationAndroid.CaptureFormat format = formats.get(i);
      capturer.startCapture(format.width, format.height, format.maxFramerate,
          appContext, observer);
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
        assertEquals(format.frameSize(), observer.frameSize());
      }
      capturer.stopCapture();
      for (long timestamp : observer.getCopyAndResetListOftimeStamps()) {
        capturer.returnBuffer(timestamp);
      }
    }
    capturer.dispose();
    assertTrue(capturer.isReleased());
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
    Camera camera = Camera.open(capturer.getCurrentCameraId());
    final List<CaptureFormat> formats = capturer.getSupportedFormats();
    final CameraEnumerationAndroid.CaptureFormat format = formats.get(0);

    final FakeCapturerObserver observer = new FakeCapturerObserver();
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        appContext, observer);

    assertFalse(observer.WaitForCapturerToStart());
    capturer.dispose();
    camera.release();
  }

  static public void startWhileCameraIsAlreadyOpenAndCloseCamera(
      VideoCapturerAndroid capturer, Context appContext) throws InterruptedException {
    Camera camera = Camera.open(capturer.getCurrentCameraId());

    final List<CaptureFormat> formats = capturer.getSupportedFormats();
    final CameraEnumerationAndroid.CaptureFormat format = formats.get(0);

    final FakeCapturerObserver observer = new FakeCapturerObserver();
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        appContext, observer);
    waitUntilIdle(capturer);

    camera.release();

    // Make sure camera is started and first frame is received and then stop it.
    assertTrue(observer.WaitForCapturerToStart());
    observer.WaitForNextCapturedFrame();
    capturer.stopCapture();
    for (long timeStamp : observer.getCopyAndResetListOftimeStamps()) {
      capturer.returnBuffer(timeStamp);
    }
    capturer.dispose();
    assertTrue(capturer.isReleased());
  }

  static public void startWhileCameraIsAlreadyOpenAndStop(
      VideoCapturerAndroid capturer, Context appContext) throws InterruptedException {
    Camera camera = Camera.open(capturer.getCurrentCameraId());
    final List<CaptureFormat> formats = capturer.getSupportedFormats();
    final CameraEnumerationAndroid.CaptureFormat format = formats.get(0);

    final FakeCapturerObserver observer = new FakeCapturerObserver();
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        appContext, observer);
    capturer.stopCapture();
    capturer.dispose();
    assertTrue(capturer.isReleased());
    camera.release();
  }

  static public void returnBufferLate(VideoCapturerAndroid capturer,
      Context appContext) throws InterruptedException {
    FakeCapturerObserver observer = new FakeCapturerObserver();

    List<CaptureFormat> formats = capturer.getSupportedFormats();
    CameraEnumerationAndroid.CaptureFormat format = formats.get(0);
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        appContext, observer);
    assertTrue(observer.WaitForCapturerToStart());

    observer.WaitForNextCapturedFrame();
    capturer.stopCapture();
    List<Long> listOftimestamps = observer.getCopyAndResetListOftimeStamps();
    assertTrue(listOftimestamps.size() >= 1);

    format = formats.get(1);
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        appContext, observer);
    observer.WaitForCapturerToStart();

    for (Long timeStamp : listOftimestamps) {
      capturer.returnBuffer(timeStamp);
    }

    observer.WaitForNextCapturedFrame();
    capturer.stopCapture();

    listOftimestamps = observer.getCopyAndResetListOftimeStamps();
    assertTrue(listOftimestamps.size() >= 1);
    for (Long timeStamp : listOftimestamps) {
      capturer.returnBuffer(timeStamp);
    }
    capturer.dispose();
    assertTrue(capturer.isReleased());
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

    // The pending frames should keep the JNI parts and |capturer| alive.
    assertFalse(capturer.isReleased());

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

    // Check that frames have successfully returned. This will cause |capturer| to be released.
    assertTrue(capturer.isReleased());
  }

  static public void cameraErrorEventOnBufferStarvation(VideoCapturerAndroid capturer,
      CameraEvents events, Context appContext) throws InterruptedException {
    final List<CaptureFormat> formats = capturer.getSupportedFormats();
    final CameraEnumerationAndroid.CaptureFormat format = formats.get(0);

    final FakeCapturerObserver observer = new FakeCapturerObserver();
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        appContext, observer);
    // Make sure camera is started.
    assertTrue(observer.WaitForCapturerToStart());
    // Since we don't call returnBuffer, we should get a starvation message.
    assertEquals("Camera failure. Client must return video buffers.", events.WaitForCameraError());

    capturer.stopCapture();
    for (long timeStamp : observer.getCopyAndResetListOftimeStamps()) {
      capturer.returnBuffer(timeStamp);
    }
    capturer.dispose();
    assertTrue(capturer.isReleased());
  }
}
