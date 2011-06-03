/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.vieautotest;

import org.webrtc.vieautotest.R;

import android.app.Activity;
import android.os.Bundle;
import android.util.Log;
import android.widget.Button;
import android.view.SurfaceView;
import android.view.View;
import android.view.SurfaceHolder;
import android.widget.LinearLayout;
import android.opengl.GLSurfaceView;
import android.widget.Spinner;
import android.widget.ArrayAdapter;
import android.widget.AdapterView;

public class ViEAutotest extends Activity
    implements
      AdapterView.OnItemSelectedListener,
      View.OnClickListener {

  private Thread _testThread;
  private Spinner _testSpinner;
  private Spinner _subtestSpinner;
  private int _testSelection;
  private int _subTestSelection;

  // View for remote video
  private LinearLayout _remoteSurface = null;
  private GLSurfaceView _glSurfaceView = null;
  private SurfaceView _surfaceView = null;

  private LinearLayout _localSurface = null;
  private GLSurfaceView _glLocalSurfaceView = null;
  private SurfaceView _localSurfaceView = null;

  /** Called when the activity is first created. */
  @Override
  public void onCreate(Bundle savedInstanceState) {

    Log.d("*WEBRTC*", "onCreate called");

    super.onCreate(savedInstanceState);
    setContentView(R.layout.main);

    // Set the Start button action
    final Button buttonStart = (Button) findViewById(R.id.Button01);
    buttonStart.setOnClickListener(this);

    // Set test spinner
    _testSpinner = (Spinner) findViewById(R.id.testSpinner);
    ArrayAdapter<CharSequence> adapter =
        ArrayAdapter.createFromResource(this, R.array.test_array,
                                        android.R.layout.simple_spinner_item);

    int resource = android.R.layout.simple_spinner_dropdown_item;
    adapter.setDropDownViewResource(resource);
    _testSpinner.setAdapter(adapter);
    _testSpinner.setOnItemSelectedListener(this);

    // Set sub test spinner
    _subtestSpinner = (Spinner) findViewById(R.id.subtestSpinner);
    ArrayAdapter<CharSequence> subtestAdapter =
        ArrayAdapter.createFromResource(this, R.array.subtest_array,
                                        android.R.layout.simple_spinner_item);

    subtestAdapter.setDropDownViewResource(resource);
    _subtestSpinner.setAdapter(subtestAdapter);
    _subtestSpinner.setOnItemSelectedListener(this);

    _remoteSurface = (LinearLayout) findViewById(R.id.RemoteView);
    _surfaceView = new SurfaceView(this);
    _remoteSurface.addView(_surfaceView);

    _localSurface = (LinearLayout) findViewById(R.id.LocalView);
    _localSurfaceView = new SurfaceView(this);
    _localSurfaceView.setZOrderMediaOverlay(true);
    _localSurface.addView(_localSurfaceView);

    // Set members
    _testSelection = 0;
    _subTestSelection = 0;
  }

  public void onClick(View v) {
    Log.d("*WEBRTC*", "Button clicked...");
    switch (v.getId()) {
      case R.id.Button01:
        new Thread(new Runnable() {
            public void run() {
              Log.d("*WEBRTC*", "Calling RunTest...");
              RunTest(_testSelection, _subTestSelection,
                      _localSurfaceView, _surfaceView);
              Log.d("*WEBRTC*", "RunTest done");
            }
          }).start();
    }
  }

  public void onItemSelected(AdapterView<?> parent, View v,
                             int position, long id) {

    if (parent == (Spinner) findViewById(R.id.testSpinner)) {
      _testSelection = position;
    } else {
      _subTestSelection = position;
    }
  }

  public void onNothingSelected(AdapterView<?> parent) {
  }

  @Override
  protected void onStart() {
    super.onStart();
  }

  @Override
  protected void onResume() {
    super.onResume();
  }

  @Override
  protected void onPause() {
    super.onPause();
  }

  @Override
  protected void onStop() {
    super.onStop();
  }

  @Override
  protected void onDestroy() {

    super.onDestroy();
  }

  // C++ function performing the chosen test
  // private native int RunTest(int testSelection, int subtestSelection,
  // GLSurfaceView window1, GLSurfaceView window2);
  private native int RunTest(int testSelection, int subtestSelection,
                             SurfaceView window1, SurfaceView window2);

  // this is used to load the 'ViEAutotestJNIAPI' library on application
  // startup.
  static {
    Log.d("*WEBRTC*", "Loading ViEAutotest...");
    System.loadLibrary("webrtc-video-autotest-jni");
  }
}
