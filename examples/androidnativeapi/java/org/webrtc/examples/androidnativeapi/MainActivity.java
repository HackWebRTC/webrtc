/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.examples.androidnativeapi;

import android.app.Activity;
import android.os.Bundle;
import android.widget.Button;
import javax.annotation.Nullable;
import org.webrtc.ContextUtils;
import org.webrtc.EglBase;
import org.webrtc.GlRectDrawer;
import org.webrtc.SurfaceViewRenderer;

public class MainActivity extends Activity {
  private @Nullable CallClient callClient;
  private @Nullable EglBase eglBase;
  private @Nullable SurfaceViewRenderer localRenderer;
  private @Nullable SurfaceViewRenderer remoteRenderer;

  @Override
  protected void onCreate(Bundle savedInstance) {
    ContextUtils.initialize(getApplicationContext());

    super.onCreate(savedInstance);
    setContentView(R.layout.activity_main);

    System.loadLibrary("examples_androidnativeapi_jni");
    callClient = new CallClient();

    Button callButton = (Button) findViewById(R.id.call_button);
    callButton.setOnClickListener((view) -> { callClient.call(localRenderer, remoteRenderer); });

    Button hangupButton = (Button) findViewById(R.id.hangup_button);
    hangupButton.setOnClickListener((view) -> { callClient.hangup(); });
  }

  @Override
  protected void onStart() {
    super.onStart();

    eglBase = EglBase.create(null /* sharedContext */, EglBase.CONFIG_PLAIN);
    localRenderer = (SurfaceViewRenderer) findViewById(R.id.local_renderer);
    remoteRenderer = (SurfaceViewRenderer) findViewById(R.id.remote_renderer);

    localRenderer.init(eglBase.getEglBaseContext(), null /* rendererEvents */, EglBase.CONFIG_PLAIN,
        new GlRectDrawer());
    remoteRenderer.init(eglBase.getEglBaseContext(), null /* rendererEvents */,
        EglBase.CONFIG_PLAIN, new GlRectDrawer());
  }

  @Override
  protected void onStop() {
    callClient.hangup();

    localRenderer.release();
    remoteRenderer.release();
    eglBase.release();

    localRenderer = null;
    remoteRenderer = null;
    eglBase = null;

    super.onStop();
  }

  @Override
  protected void onDestroy() {
    callClient.close();
    callClient = null;

    super.onDestroy();
  }
}
