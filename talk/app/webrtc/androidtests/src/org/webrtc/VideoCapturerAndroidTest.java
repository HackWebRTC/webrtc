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

import android.hardware.Camera;
import android.test.ActivityTestCase;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Size;

import org.webrtc.CameraEnumerationAndroid.CaptureFormat;
import org.webrtc.VideoRenderer.I420Frame;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

@SuppressWarnings("deprecation")
public class VideoCapturerAndroidTest extends ActivityTestCase {
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

  static class AsyncRenderer implements VideoRenderer.Callbacks {
    private final List<I420Frame> pendingFrames = new ArrayList<I420Frame>();

    @Override
    public void renderFrame(I420Frame frame) {
      synchronized (pendingFrames) {
        pendingFrames.add(frame);
        pendingFrames.notifyAll();
      }
    }

    // Wait until at least one frame have been received, before returning them.
    public List<I420Frame> WaitForFrames() {
      synchronized (pendingFrames) {
        while (pendingFrames.isEmpty()) {
          try {
            pendingFrames.wait();
          } catch (InterruptedException e) {
            // Ignore.
          }
        }
        final List<I420Frame> frames = new ArrayList<I420Frame>(pendingFrames);
        pendingFrames.clear();
        return frames;
      }
    }
  }

  static class FakeCapturerObserver implements
      VideoCapturerAndroid.CapturerObserver {
    private int framesCaptured = 0;
    private int frameSize = 0;
    private Object frameLock = 0;
    private Object capturerStartLock = 0;
    private boolean captureStartResult = false;
    private List<Long> timestamps = new ArrayList<Long>();

    @Override
    public void OnCapturerStarted(boolean success) {
      synchronized (capturerStartLock) {
        captureStartResult = success;
        capturerStartLock.notify();
      }
    }

    @Override
    public void OnFrameCaptured(byte[] frame, int length, int width, int height,
        int rotation, long timeStamp) {
      synchronized (frameLock) {
        ++framesCaptured;
        frameSize = length;
        timestamps.add(timeStamp);
        frameLock.notify();
      }
    }

    @Override
    public void OnOutputFormatRequest(int width, int height, int fps) {}

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

    List<Long> getCopyAndResetListOftimeStamps() {
      synchronized (frameLock) {
        ArrayList<Long> list = new ArrayList<Long>(timestamps);
        timestamps.clear();
        return list;
      }
    }
  }

  // Return true if the device under test have at least two cameras.
  @SuppressWarnings("deprecation")
  boolean HaveTwoCameras() {
    return (Camera.getNumberOfCameras() >= 2);
  }

  void startCapturerAndRender(String deviceName) throws InterruptedException {
    PeerConnectionFactory factory = new PeerConnectionFactory();
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create(deviceName, null);
    VideoSource source =
        factory.createVideoSource(capturer, new MediaConstraints());
    VideoTrack track = factory.createVideoTrack("dummy", source);
    RendererCallbacks callbacks = new RendererCallbacks();
    track.addRenderer(new VideoRenderer(callbacks));
    assertTrue(callbacks.WaitForNextFrameToRender() > 0);
    track.dispose();
    source.dispose();
    factory.dispose();
  }

  @Override
  protected void setUp() {
    assertTrue(PeerConnectionFactory.initializeAndroidGlobals(
        getInstrumentation().getContext(), true, true, true));
  }

  @SmallTest
  // Test that enumerating formats using android.hardware.camera2 will give the same formats as
  // android.hardware.camera in the range 320x240 to 1280x720. Often the camera2 API may contain
  // some high resolutions that are not supported in camera1, but it may also be the other way
  // around in some cases. Supported framerates may also differ, so don't compare those.
  public void testCamera2Enumerator() {
    if (!Camera2Enumerator.isSupported()) {
      return;
    }
    final CameraEnumerationAndroid.Enumerator camera1Enumerator = new CameraEnumerator();
    final CameraEnumerationAndroid.Enumerator camera2Enumerator =
        new Camera2Enumerator(getInstrumentation().getContext());

    for (int i = 0; i < CameraEnumerationAndroid.getDeviceCount(); ++i) {
      final Set<Size> resolutions1 = new HashSet<Size>();
      for (CaptureFormat format : camera1Enumerator.getSupportedFormats(i)) {
        resolutions1.add(new Size(format.width, format.height));
      }
      final Set<Size> resolutions2 = new HashSet<Size>();
      for (CaptureFormat format : camera2Enumerator.getSupportedFormats(i)) {
        resolutions2.add(new Size(format.width, format.height));
      }
      for (Size size : resolutions1) {
        if (size.getWidth() >= 320 && size.getHeight() >= 240
            && size.getWidth() <= 1280 && size.getHeight() <= 720) {
          assertTrue(resolutions2.contains(size));
        }
      }
    }
  }

  @SmallTest
  public void testCreateAndRelease() throws Exception {
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create("", null);
    assertNotNull(capturer);
    capturer.dispose();
  }

  @SmallTest
  public void testCreateNonExistingCamera() throws Exception {
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create(
        "non-existing camera", null);
    assertNull(capturer);
  }

  @SmallTest
  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using a "default" capturer.
  // It tests both the Java and the C++ layer.
  public void testStartVideoCapturer() throws Exception {
    startCapturerAndRender("");
  }

  @SmallTest
  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using the front facing video capturer.
  // It tests both the Java and the C++ layer.
  public void testStartFrontFacingVideoCapturer() throws Exception {
    startCapturerAndRender(CameraEnumerationAndroid.getNameOfFrontFacingDevice());
  }

  @SmallTest
  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using the back facing video capturer.
  // It tests both the Java and the C++ layer.
  public void testStartBackFacingVideoCapturer() throws Exception {
    if (!HaveTwoCameras()) {
      return;
    }
    startCapturerAndRender(CameraEnumerationAndroid.getNameOfBackFacingDevice());
  }

  @SmallTest
  // This test that the default camera can be started and but the camera can
  // later be switched to another camera.
  // It tests both the Java and the C++ layer.
  public void testSwitchVideoCapturer() throws Exception {
    PeerConnectionFactory factory = new PeerConnectionFactory();
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create("", null);
    VideoSource source =
        factory.createVideoSource(capturer, new MediaConstraints());
    VideoTrack track = factory.createVideoTrack("dummy", source);

    if (HaveTwoCameras())
      assertTrue(capturer.switchCamera(null));
    else
      assertFalse(capturer.switchCamera(null));

    // Wait until the camera have been switched.
    capturer.runCameraThreadUntilIdle();

    // Ensure that frames are received.
    RendererCallbacks callbacks = new RendererCallbacks();
    track.addRenderer(new VideoRenderer(callbacks));
    assertTrue(callbacks.WaitForNextFrameToRender() > 0);
    track.dispose();
    source.dispose();
    factory.dispose();
  }

  @SmallTest
  // This test that the VideoSource that the VideoCapturer is connected to can
  // be stopped and restarted. It tests both the Java and the C++ layer.
  public void testStopRestartVideoSource() throws Exception {
    PeerConnectionFactory factory = new PeerConnectionFactory();
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create("", null);
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
  }

  @SmallTest
  // This test that the camera can be started at different resolutions.
  // It does not test or use the C++ layer.
  public void testStartStopWithDifferentResolutions() throws Exception {
    FakeCapturerObserver observer = new FakeCapturerObserver();

    String deviceName = CameraEnumerationAndroid.getDeviceName(0);
    List<CaptureFormat> formats = CameraEnumerationAndroid.getSupportedFormats(0);
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create(deviceName, null);

    for(int i = 0; i < 3 ; ++i) {
      CameraEnumerationAndroid.CaptureFormat format = formats.get(i);
      capturer.startCapture(format.width, format.height, format.maxFramerate,
          getInstrumentation().getContext(), observer);
      assertTrue(observer.WaitForCapturerToStart());
      observer.WaitForNextCapturedFrame();
      // Check the frame size.
      assertEquals(format.frameSize(), observer.frameSize());
      capturer.stopCapture();
    }
    capturer.dispose();
  }

  @SmallTest
  // This test what happens if buffers are returned after the capturer have
  // been stopped and restarted. It does not test or use the C++ layer.
  public void testReturnBufferLate() throws Exception {
    FakeCapturerObserver observer = new FakeCapturerObserver();

    String deviceName = CameraEnumerationAndroid.getDeviceName(0);
    List<CaptureFormat> formats = CameraEnumerationAndroid.getSupportedFormats(0);
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create(deviceName, null);

    CameraEnumerationAndroid.CaptureFormat format = formats.get(0);
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        getInstrumentation().getContext(), observer);
    assertTrue(observer.WaitForCapturerToStart());

    observer.WaitForNextCapturedFrame();
    capturer.stopCapture();
    List<Long> listOftimestamps = observer.getCopyAndResetListOftimeStamps();
    assertTrue(listOftimestamps.size() >= 1);

    format = formats.get(1);
    capturer.startCapture(format.width, format.height, format.maxFramerate,
        getInstrumentation().getContext(), observer);
    observer.WaitForCapturerToStart();
    observer.WaitForNextCapturedFrame();

    for (Long timeStamp : listOftimestamps) {
      capturer.returnBuffer(timeStamp);
    }

    observer.WaitForNextCapturedFrame();
    capturer.stopCapture();

    listOftimestamps = observer.getCopyAndResetListOftimeStamps();
    assertTrue(listOftimestamps.size() >= 2);
    for (Long timeStamp : listOftimestamps) {
      capturer.returnBuffer(timeStamp);
    }
  }

  @SmallTest
  // This test that we can capture frames, stop capturing, keep the frames for rendering, and then
  // return the frames. It tests both the Java and the C++ layer.
  public void testCaptureAndAsyncRender() {
    final VideoCapturerAndroid capturer = VideoCapturerAndroid.create("", null);
    // Helper class that sets everything up, captures at least one frame, and then shuts
    // everything down.
    class CaptureFramesRunnable implements Runnable {
      public List<I420Frame> frames;

      @Override
      public void run() {
        PeerConnectionFactory factory = new PeerConnectionFactory();
        VideoSource source = factory.createVideoSource(capturer, new MediaConstraints());
        VideoTrack track = factory.createVideoTrack("dummy", source);
        AsyncRenderer renderer = new AsyncRenderer();
        track.addRenderer(new VideoRenderer(renderer));

        // Wait until we get at least one frame.
        frames = renderer.WaitForFrames();

        // Stop everything.
        track.dispose();
        source.dispose();
        factory.dispose();
      }
    }

    // Capture frames on a separate thread.
    CaptureFramesRunnable captureFramesRunnable = new CaptureFramesRunnable();
    Thread captureThread = new Thread(captureFramesRunnable);
    captureThread.start();

    // Wait until frames are captured, and then kill the thread.
    try {
      captureThread.join();
    } catch (InterruptedException e) {
      fail("Capture thread was interrupted");
    }
    captureThread = null;

    // Assert that we have frames that have not been returned.
    assertTrue(!captureFramesRunnable.frames.isEmpty());
    // Return the frame(s).
    for (I420Frame frame : captureFramesRunnable.frames) {
      VideoRenderer.renderFrameDone(frame);
    }
    assertEquals(capturer.pendingFramesTimeStamps(), "[]");
  }
}
