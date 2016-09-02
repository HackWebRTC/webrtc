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
import android.hardware.camera2.CameraManager;
import android.os.Handler;

import java.util.List;

@TargetApi(21)
public class Camera2Capturer extends CameraCapturer {
  private final Context context;
  private final CameraManager cameraManager;

  public Camera2Capturer(Context context, String cameraName, CameraEventsHandler eventsHandler) {
    super(cameraName, eventsHandler, new Camera2Enumerator(context));

    this.context = context;
    cameraManager = (CameraManager) context.getSystemService(Context.CAMERA_SERVICE);
  }

  @Override
  protected void createCameraSession(
      CameraSession.CreateSessionCallback createSessionCallback,
      CameraEventsHandler eventsHandler, Context applicationContext,
      CameraVideoCapturer.CapturerObserver capturerObserver,
      SurfaceTextureHelper surfaceTextureHelper,
      String cameraName, int width, int height, int framerate) {
    Camera2Session.create(
      cameraManager,
      createSessionCallback,
      eventsHandler, applicationContext,
      capturerObserver,
      surfaceTextureHelper,
      cameraName, width, height, framerate);
  }
}
