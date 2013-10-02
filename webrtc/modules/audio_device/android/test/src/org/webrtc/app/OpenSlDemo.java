/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.app;

import android.app.Activity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.util.Log;
import android.view.View;
import android.widget.Button;

public class OpenSlDemo extends Activity implements View.OnClickListener {
  private static final String TAG = "WEBRTC";

  private Button btStartStopCall;
  private boolean isRunning = false;

  private WakeLock wakeLock;

  private OpenSlRunner runner;

  // Called when activity is created.
  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    PowerManager pm = (PowerManager)this.getSystemService(
        Context.POWER_SERVICE);
    wakeLock = pm.newWakeLock(
        PowerManager.SCREEN_DIM_WAKE_LOCK, TAG);
    wakeLock.acquire(); // Keep screen on until app terminates.

    setContentView(R.layout.open_sl_demo);

    // Direct hardware volume controls to affect the voice call audio stream.
    setVolumeControlStream(AudioManager.STREAM_VOICE_CALL);

    btStartStopCall = (Button) findViewById(R.id.btStartStopCall);
    btStartStopCall.setOnClickListener(this);
    findViewById(R.id.btExit).setOnClickListener(this);

    runner = new OpenSlRunner();
    // Native code calls back into JVM to be able to configure OpenSL to low
    // latency mode. Provide the context needed to do this.
    runner.RegisterApplicationContext(getApplicationContext());
  }

  // Called before activity is destroyed.
  @Override
  public void onDestroy() {
    Log.d(TAG, "onDestroy");
    wakeLock.release();
    super.onDestroy();
  }

  private void startOrStop() {
    if (isRunning) {
      runner.Stop();
      btStartStopCall.setText(R.string.startCall);
      isRunning = false;
    } else if (!isRunning){
      runner.Start();
      btStartStopCall.setText(R.string.stopCall);
      isRunning = true;
    }
  }

  public void onClick(View arg0) {
    switch (arg0.getId()) {
      case R.id.btStartStopCall:
        startOrStop();
        break;
      case R.id.btExit:
        finish();
        break;
    }
  }

}
