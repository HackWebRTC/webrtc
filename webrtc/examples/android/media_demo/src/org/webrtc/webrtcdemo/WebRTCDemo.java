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

import android.app.ActionBar.Tab;
import android.app.ActionBar.TabListener;
import android.app.ActionBar;
import android.app.Activity;
import android.app.Fragment;
import android.app.FragmentTransaction;
import android.content.pm.ActivityInfo;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.WindowManager;

public class WebRTCDemo extends Activity implements MenuStateProvider {

  // From http://developer.android.com/guide/topics/ui/actionbar.html
  public static class TabListener<T extends Fragment>
      implements ActionBar.TabListener {
    private Fragment fragment;
    private final Activity activity;
    private final String tag;
    private final Class<T> instance;
    private final Bundle args;

    public TabListener(Activity activity, String tag, Class<T> clz) {
      this(activity, tag, clz, null);
    }

    public TabListener(Activity activity, String tag, Class<T> clz,
        Bundle args) {
      this.activity = activity;
      this.tag = tag;
      this.instance = clz;
      this.args = args;
    }

    public void onTabSelected(Tab tab, FragmentTransaction ft) {
      // Check if the fragment is already initialized
      if (fragment == null) {
        // If not, instantiate and add it to the activity
        fragment = Fragment.instantiate(activity, instance.getName(), args);
        ft.add(android.R.id.content, fragment, tag);
      } else {
        // If it exists, simply attach it in order to show it
        ft.attach(fragment);
      }
    }

    public void onTabUnselected(Tab tab, FragmentTransaction ft) {
      if (fragment != null) {
        // Detach the fragment, because another one is being attached
        ft.detach(fragment);
      }
    }

    public void onTabReselected(Tab tab, FragmentTransaction ft) {
      // User selected the already selected tab. Do nothing.
    }
  }

  private NativeWebRtcContextRegistry contextRegistry = null;
  private MediaEngine mediaEngine = null;
  private Handler handler;
  public MediaEngine getEngine() { return mediaEngine; }

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // Global settings.
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

    // State.
    // Must be instantiated before MediaEngine.
    contextRegistry = new NativeWebRtcContextRegistry();
    contextRegistry.register(this);

    // Load all settings dictated in xml.
    mediaEngine = new MediaEngine(this);
    mediaEngine.setRemoteIp(getResources().getString(R.string.loopbackIp));

    mediaEngine.setAudio(getResources().getBoolean(
        R.bool.audio_enabled_default));
    mediaEngine.setAudioCodec(mediaEngine.getIsacIndex());
    mediaEngine.setAudioRxPort(getResources().getInteger(
        R.integer.aRxPortDefault));
    mediaEngine.setAudioTxPort(getResources().getInteger(
        R.integer.aTxPortDefault));
    mediaEngine.setSpeaker(getResources().getBoolean(
        R.bool.speaker_enabled_default));
    mediaEngine.setDebuging(getResources().getBoolean(
        R.bool.apm_debug_enabled_default));

    // Create action bar with all tabs.
    ActionBar actionBar = getActionBar();
    actionBar.setNavigationMode(ActionBar.NAVIGATION_MODE_TABS);
    actionBar.setDisplayShowTitleEnabled(false);

    Tab tab = actionBar.newTab()
        .setText("Main")
        .setTabListener(new TabListener<MainMenuFragment>(
            this, "main", MainMenuFragment.class));
    actionBar.addTab(tab);

    tab = actionBar.newTab()
        .setText("Settings")
        .setTabListener(new TabListener<SettingsMenuFragment>(
            this, "Settings", SettingsMenuFragment.class));
    actionBar.addTab(tab);

    tab = actionBar.newTab()
        .setText("Audio")
        .setTabListener(new TabListener<AudioMenuFragment>(
            this, "Audio", AudioMenuFragment.class));
    actionBar.addTab(tab);

    enableTimedStartStop();

    // Hint that voice call audio stream should be used for hardware volume
    // controls.
    setVolumeControlStream(AudioManager.STREAM_VOICE_CALL);
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu) {
    MenuInflater inflater = getMenuInflater();
    inflater.inflate(R.menu.main_activity_actions, menu);
    return super.onCreateOptionsMenu(menu);
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item) {
    // Handle presses on the action bar items
    switch (item.getItemId()) {
      case R.id.action_exit:
        MainMenuFragment main = (MainMenuFragment)getFragmentManager()
            .findFragmentByTag("main");
        main.stopAll();
        finish();
        return true;
      default:
        return super.onOptionsItemSelected(item);
    }
  }

  @Override
  public void onDestroy() {
    disableTimedStartStop();
    mediaEngine.dispose();
    contextRegistry.unRegister();
    super.onDestroy();
  }

  @Override
  public boolean onKeyDown(int keyCode, KeyEvent event) {
    if (keyCode == KeyEvent.KEYCODE_BACK) {
      // Prevent app from running in the background.
      MainMenuFragment main = (MainMenuFragment)getFragmentManager()
            .findFragmentByTag("main");
      main.stopAll();
      finish();
      return true;
    }
    return super.onKeyDown(keyCode, event);
  }

  private int getCallRestartPeriodicity() {
    return getResources().getInteger(R.integer.call_restart_periodicity_ms);
  }

  // Thread repeatedly calling start/stop.
  void enableTimedStartStop() {
    if (getCallRestartPeriodicity() > 0) {
      // Periodicity == 0 <-> Disabled.
      handler = new Handler();
      handler.postDelayed(startOrStopCallback, getCallRestartPeriodicity());
    }
  }

  void disableTimedStartStop() {
    if (handler != null) {
      handler.removeCallbacks(startOrStopCallback);
    }
  }

  private Runnable startOrStopCallback = new Runnable() {
      public void run() {
        MainMenuFragment main = (MainMenuFragment)getFragmentManager()
            .findFragmentByTag("main");
        main.toggleStart();
        handler.postDelayed(startOrStopCallback, getCallRestartPeriodicity());
      }
  };
}
