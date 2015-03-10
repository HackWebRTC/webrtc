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

import org.webrtc.VideoCapturerAndroid.CaptureFormat;
import org.webrtc.VideoRenderer.I420Frame;

import java.util.ArrayList;

@SuppressWarnings("deprecation")
public class VideoCapturerAndroidTest extends ActivityTestCase {
  static class RendererCallbacks implements VideoRenderer.Callbacks {
    private int framesRendered = 0;
    private Object frameLock = 0;

    @Override
    public void setSize(int width, int height) {
    }

    @Override
    public void renderFrame(I420Frame frame) {
      synchronized (frameLock) {
        ++framesRendered;
        frameLock.notify();
      }
    }

    public int WaitForNextFrameToRender() throws InterruptedException {
      synchronized (frameLock) {
        frameLock.wait();
        return framesRendered;
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

    @Override
    public void OnCapturerStarted(boolean success) {
      synchronized (capturerStartLock) {
        captureStartResult = success;
        capturerStartLock.notify();
      }
    }

    @Override
    public void OnFrameCaptured(byte[] frame, int length, int rotation,
        long timeStamp) {
      synchronized (frameLock) {
        ++framesCaptured;
        frameSize = length;
        frameLock.notify();
      }
    }

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
  }

  // Return true if the device under test have at least two cameras.
  @SuppressWarnings("deprecation")
  boolean HaveTwoCameras() {
    return (Camera.getNumberOfCameras() >= 2);
  }

  void starCapturerAndRender(String deviceName) throws InterruptedException {
    PeerConnectionFactory factory = new PeerConnectionFactory();
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create(deviceName);
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
        getInstrumentation().getContext(), true,
        true, true, null));
  }

  @SmallTest
  public void testCreateAndRelease() throws Exception {
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create("");
    assertNotNull(capturer);
    capturer.dispose();
  }

  @SmallTest
  public void testCreateNoneExistingCamera() throws Exception {
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create(
        "none existing camera");
    assertNull(capturer);
  }

  @SmallTest
  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using a "default" capturer.
  // It tests both the Java and the C++ layer.
  public void testStartVideoCapturer() throws Exception {
    starCapturerAndRender("");
  }

  @SmallTest
  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using the front facing video capturer.
  // It tests both the Java and the C++ layer.
  public void testStartFrontFacingVideoCapturer() throws Exception {
    starCapturerAndRender(VideoCapturerAndroid.getNameOfFrontFacingDevice());
  }

  @SmallTest
  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using the back facing video capturer.
  // It tests both the Java and the C++ layer.
  public void testStartBackFacingVideoCapturer() throws Exception {
    if (!HaveTwoCameras()) {
      return;
    }
    starCapturerAndRender(VideoCapturerAndroid.getNameOfBackFacingDevice());
  }

  @SmallTest
  // This test that the default camera can be started and but the camera can
  // later be switched to another camera.
  // It tests both the Java and the C++ layer.
  public void testSwitchVideoCapturer() throws Exception {
    PeerConnectionFactory factory = new PeerConnectionFactory();
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create("");
    VideoSource source =
        factory.createVideoSource(capturer, new MediaConstraints());
    VideoTrack track = factory.createVideoTrack("dummy", source);

    if (HaveTwoCameras())
      assertTrue(capturer.switchCamera());
    else
      assertFalse(capturer.switchCamera());

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
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create("");
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

    String deviceName = VideoCapturerAndroid.getDeviceName(0);
    ArrayList<CaptureFormat> formats =
        VideoCapturerAndroid.getSupportedFormats(0);
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create(deviceName);

    for(int i = 0; i < 3 ; ++i) {
      VideoCapturerAndroid.CaptureFormat format = formats.get(i);
      capturer.startCapture(format.width, format.height, format.maxFramerate,
          getInstrumentation().getContext(), observer);
      assertTrue(observer.WaitForCapturerToStart());
      observer.WaitForNextCapturedFrame();
      // Check the frame size.
      assertEquals((format.width*format.height*3)/2, observer.frameSize());
      capturer.stopCapture();
    }
    capturer.dispose();
  }
}
