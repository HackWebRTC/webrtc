/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.webrtcdemo;

import android.app.Activity;
import android.app.Fragment;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.TextView;

public class MainMenuFragment extends Fragment implements MediaEngineObserver {

  private String TAG;
  private MenuStateProvider stateProvider;

  private Button btStartStopCall;
  private TextView tvStats;

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container,
      Bundle savedInstanceState) {
    View v = inflater.inflate(R.layout.mainmenu, container, false);

    TAG = getResources().getString(R.string.tag);

    Button btStats = (Button) v.findViewById(R.id.btStats);
    boolean stats = getResources().getBoolean(R.bool.stats_enabled_default);
    enableStats(btStats, stats);
    btStats.setOnClickListener(new View.OnClickListener() {
        public void onClick(View button) {
          boolean turnOnStats = ((Button) button).getText().equals(
              getResources().getString(R.string.statsOn));
          enableStats((Button) button, turnOnStats);
        }
    });
    tvStats = (TextView) v.findViewById(R.id.tvStats);

    btStartStopCall = (Button) v.findViewById(R.id.btStartStopCall);
    btStartStopCall.setText(getEngine().isRunning() ?
        R.string.stopCall :
        R.string.startCall);
    btStartStopCall.setOnClickListener(new View.OnClickListener() {
        public void onClick(View button) {
          toggleStart();
        }
      });
    return v;
  }

  @Override
  public void onAttach(Activity activity) {
    super.onAttach(activity);

    // This makes sure that the container activity has implemented
    // the callback interface. If not, it throws an exception.
    try {
      stateProvider = (MenuStateProvider) activity;
    } catch (ClassCastException e) {
      throw new ClassCastException(activity +
          " must implement MenuStateProvider");
    }
  }

  // tvStats need to be updated on the UI thread.
  public void newStats(final String stats) {
    getActivity().runOnUiThread(new Runnable() {
        public void run() {
          tvStats.setText(stats);
        }
      });
  }

  private MediaEngine getEngine() {
    return stateProvider.getEngine();
  }

  private void enableStats(Button btStats, boolean enable) {
    if (enable) {
      getEngine().setObserver(this);
    } else {
      getEngine().setObserver(null);
      // Clear old stats text by posting empty stats.
      newStats("");
    }
    // If stats was true it was just turned on. This means that
    // clicking the button again should turn off stats.
    btStats.setText(enable ? R.string.statsOff : R.string.statsOn);
  }


  public void toggleStart() {
    if (getEngine().isRunning()) {
      stopAll();
    } else {
      startCall();
    }
    btStartStopCall.setText(getEngine().isRunning() ?
        R.string.stopCall :
        R.string.startCall);
  }

  public void stopAll() {
    getEngine().stop();
  }

  private void startCall() {
    getEngine().start();
  }
}