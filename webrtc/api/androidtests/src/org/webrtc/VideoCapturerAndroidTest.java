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

import android.test.ActivityTestCase;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;
import android.util.Size;

import org.webrtc.CameraEnumerationAndroid.CaptureFormat;

import java.util.HashSet;
import java.util.Set;

@SuppressWarnings("deprecation")
public class VideoCapturerAndroidTest extends ActivityTestCase {
  static final String TAG = "VideoCapturerAndroidTest";

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
  public void testCreateAndRelease() {
    VideoCapturerAndroidTestFixtures.release(VideoCapturerAndroid.create("", null));
  }

  @SmallTest
  public void testCreateAndReleaseUsingTextures() {
    VideoCapturerAndroidTestFixtures.release(
        VideoCapturerAndroid.create("", null, true /* captureToTexture */));
  }

  @SmallTest
  public void testCreateNonExistingCamera() {
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create(
        "non-existing camera", null);
    assertNull(capturer);
  }

  @SmallTest
  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using a "default" capturer.
  // It tests both the Java and the C++ layer.
  public void testStartVideoCapturer() throws InterruptedException {
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create("", null);
    VideoCapturerAndroidTestFixtures.startCapturerAndRender(capturer);
  }

  @SmallTest
  public void testStartVideoCapturerUsingTexturesDeprecated() throws InterruptedException {
    EglBase eglBase = EglBase.create();
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create("", null, eglBase.getEglBaseContext());
    VideoCapturerAndroidTestFixtures.startCapturerAndRender(capturer);
    eglBase.release();
  }

  @SmallTest
  public void testStartVideoCapturerUsingTextures() throws InterruptedException {
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create("", null, true /* captureToTexture */);
    VideoCapturerAndroidTestFixtures.startCapturerAndRender(capturer);
  }

  @SmallTest
  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using the front facing video capturer.
  // It tests both the Java and the C++ layer.
  public void testStartFrontFacingVideoCapturer() throws InterruptedException {
    String deviceName = CameraEnumerationAndroid.getNameOfFrontFacingDevice();
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create(deviceName, null);
    VideoCapturerAndroidTestFixtures.startCapturerAndRender(capturer);
  }

  @SmallTest
  // This test that the camera can be started and that the frames are forwarded
  // to a Java video renderer using the back facing video capturer.
  // It tests both the Java and the C++ layer.
  public void testStartBackFacingVideoCapturer() throws InterruptedException {
    if (!VideoCapturerAndroidTestFixtures.HaveTwoCameras()) {
      return;
    }

    String deviceName = CameraEnumerationAndroid.getNameOfBackFacingDevice();
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create(deviceName, null);
    VideoCapturerAndroidTestFixtures.startCapturerAndRender(capturer);
  }

  @SmallTest
  // This test that the default camera can be started and that the camera can
  // later be switched to another camera.
  // It tests both the Java and the C++ layer.
  public void testSwitchVideoCapturer() throws InterruptedException {
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create("", null);
    VideoCapturerAndroidTestFixtures.switchCamera(capturer);
  }

  @SmallTest
  public void testSwitchVideoCapturerUsingTextures() throws InterruptedException {
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create("", null, true /* captureToTexture */);
    VideoCapturerAndroidTestFixtures.switchCamera(capturer);
  }

  @MediumTest
  public void testCameraEvents() throws InterruptedException {
    VideoCapturerAndroidTestFixtures.CameraEvents cameraEvents =
        VideoCapturerAndroidTestFixtures.createCameraEvents();
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create("", cameraEvents);
    VideoCapturerAndroidTestFixtures.cameraEventsInvoked(
        capturer, cameraEvents, getInstrumentation().getContext());
  }

  @MediumTest
  public void testCameraEventsUsingTextures() throws InterruptedException {
    VideoCapturerAndroidTestFixtures.CameraEvents cameraEvents =
        VideoCapturerAndroidTestFixtures.createCameraEvents();
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create("", cameraEvents, true /* captureToTexture */);
    VideoCapturerAndroidTestFixtures.cameraEventsInvoked(
        capturer, cameraEvents, getInstrumentation().getContext());
  }

  @MediumTest
  // Test what happens when attempting to call e.g. switchCamera() after camera has been stopped.
  public void testCameraCallsAfterStop() throws InterruptedException {
    final String deviceName = CameraEnumerationAndroid.getDeviceName(0);
    final VideoCapturerAndroid capturer = VideoCapturerAndroid.create(deviceName, null);

    VideoCapturerAndroidTestFixtures.cameraCallsAfterStop(capturer,
        getInstrumentation().getContext());
  }

  @MediumTest
  public void testCameraCallsAfterStopUsingTextures() throws InterruptedException {
    final String deviceName = CameraEnumerationAndroid.getDeviceName(0);
    final VideoCapturerAndroid capturer = VideoCapturerAndroid.create(deviceName, null,
        true /* captureToTexture */);

    VideoCapturerAndroidTestFixtures.cameraCallsAfterStop(capturer,
        getInstrumentation().getContext());
  }

  @SmallTest
  // This test that the VideoSource that the VideoCapturer is connected to can
  // be stopped and restarted. It tests both the Java and the C++ layer.
  public void testStopRestartVideoSource() throws InterruptedException {
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create("", null);
    VideoCapturerAndroidTestFixtures.stopRestartVideoSource(capturer);
  }

  @SmallTest
  public void testStopRestartVideoSourceUsingTextures() throws InterruptedException {
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create("", null, true /* captureToTexture */);
    VideoCapturerAndroidTestFixtures.stopRestartVideoSource(capturer);
  }

  @SmallTest
  // This test that the camera can be started at different resolutions.
  // It does not test or use the C++ layer.
  public void testStartStopWithDifferentResolutions() throws InterruptedException {
    String deviceName = CameraEnumerationAndroid.getDeviceName(0);
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create(deviceName, null);
    VideoCapturerAndroidTestFixtures.startStopWithDifferentResolutions(capturer,
        getInstrumentation().getContext());
  }

  @SmallTest
  public void testStartStopWithDifferentResolutionsUsingTextures() throws InterruptedException {
    String deviceName = CameraEnumerationAndroid.getDeviceName(0);
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create(deviceName, null, true /* captureToTexture */);
    VideoCapturerAndroidTestFixtures.startStopWithDifferentResolutions(capturer,
        getInstrumentation().getContext());
  }

  @SmallTest
  // This test that an error is reported if the camera is already opened
  // when VideoCapturerAndroid is started.
  public void testStartWhileCameraAlreadyOpened() throws InterruptedException {
    String deviceName = CameraEnumerationAndroid.getDeviceName(0);
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create(deviceName, null);
    VideoCapturerAndroidTestFixtures.startWhileCameraIsAlreadyOpen(
        capturer, getInstrumentation().getContext());
  }

  @SmallTest
  // This test that VideoCapturerAndroid can be started, even if the camera is already opened
  // if the camera is closed while VideoCapturerAndroid is re-trying to start.
  public void testStartWhileCameraIsAlreadyOpenAndCloseCamera() throws InterruptedException {
    String deviceName = CameraEnumerationAndroid.getDeviceName(0);
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create(deviceName, null);
    VideoCapturerAndroidTestFixtures.startWhileCameraIsAlreadyOpenAndCloseCamera(
        capturer, getInstrumentation().getContext());
  }

  @SmallTest
  // This test that VideoCapturerAndroid.stop can be called while VideoCapturerAndroid is
  // re-trying to start.
  public void startWhileCameraIsAlreadyOpenAndStop() throws InterruptedException {
    String deviceName = CameraEnumerationAndroid.getDeviceName(0);
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create(deviceName, null);
    VideoCapturerAndroidTestFixtures.startWhileCameraIsAlreadyOpenAndStop(
        capturer, getInstrumentation().getContext());
  }



  @SmallTest
  // This test what happens if buffers are returned after the capturer have
  // been stopped and restarted. It does not test or use the C++ layer.
  public void testReturnBufferLate() throws InterruptedException {
    String deviceName = CameraEnumerationAndroid.getDeviceName(0);
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create(deviceName, null);
    VideoCapturerAndroidTestFixtures.returnBufferLate(capturer,
        getInstrumentation().getContext());
  }

  @SmallTest
  public void testReturnBufferLateUsingTextures() throws InterruptedException {
    String deviceName = CameraEnumerationAndroid.getDeviceName(0);
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create(deviceName, null, true /* captureToTexture */);
    VideoCapturerAndroidTestFixtures.returnBufferLate(capturer,
        getInstrumentation().getContext());
  }

  @MediumTest
  // This test that we can capture frames, keep the frames in a local renderer, stop capturing,
  // and then return the frames. The difference between the test testReturnBufferLate() is that we
  // also test the JNI and C++ AndroidVideoCapturer parts.
  public void testReturnBufferLateEndToEnd() throws InterruptedException {
    final VideoCapturerAndroid capturer = VideoCapturerAndroid.create("", null);
    VideoCapturerAndroidTestFixtures.returnBufferLateEndToEnd(capturer);
  }

  @MediumTest
  public void testReturnBufferLateEndToEndUsingTextures() throws InterruptedException {
    final VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create("", null, true /* captureToTexture */);
    VideoCapturerAndroidTestFixtures.returnBufferLateEndToEnd(capturer);
  }

  @MediumTest
  // This test that CameraEventsHandler.onError is triggered if video buffers are not returned to
  // the capturer.
  public void testCameraFreezedEventOnBufferStarvationUsingTextures() throws InterruptedException {
    VideoCapturerAndroidTestFixtures.CameraEvents cameraEvents =
        VideoCapturerAndroidTestFixtures.createCameraEvents();
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create("", cameraEvents,
        true /* captureToTexture */);
    VideoCapturerAndroidTestFixtures.cameraFreezedEventOnBufferStarvationUsingTextures(capturer,
        cameraEvents, getInstrumentation().getContext());
  }

  @MediumTest
  // This test that frames forwarded to a renderer is scaled if onOutputFormatRequest is
  // called. This test both Java and C++ parts of of the stack.
  public void testScaleCameraOutput() throws InterruptedException {
    VideoCapturerAndroid capturer = VideoCapturerAndroid.create("", null);
    VideoCapturerAndroidTestFixtures.scaleCameraOutput(capturer);
  }

  @MediumTest
  // This test that frames forwarded to a renderer is scaled if onOutputFormatRequest is
  // called. This test both Java and C++ parts of of the stack.
  public void testScaleCameraOutputUsingTextures() throws InterruptedException {
    VideoCapturerAndroid capturer =
        VideoCapturerAndroid.create("", null, true /* captureToTexture */);
    VideoCapturerAndroidTestFixtures.scaleCameraOutput(capturer);
  }
}
