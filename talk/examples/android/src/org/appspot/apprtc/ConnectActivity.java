/*
 * libjingle
 * Copyright 2014, Google Inc.
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

package org.appspot.apprtc;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.inputmethod.EditorInfo;
import android.webkit.URLUtil;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ListView;
import android.widget.TextView;

import java.util.ArrayList;
import java.util.Random;

import org.json.JSONArray;
import org.json.JSONException;
import org.webrtc.MediaCodecVideoEncoder;


/**
 * Handles the initial setup where the user selects which room to join.
 */
public class ConnectActivity extends Activity {

  public static final String EXTRA_ROOMNAME = "org.appspot.apprtc.ROOMNAME";
  public static final String EXTRA_LOOPBACK = "org.appspot.apprtc.LOOPBACK";
  public static final String EXTRA_CMDLINE = "org.appspot.apprtc.CMDLINE";
  public static final String EXTRA_RUNTIME = "org.appspot.apprtc.RUNTIME";
  public static final String EXTRA_BITRATE = "org.appspot.apprtc.BITRATE";
  public static final String EXTRA_HWCODEC = "org.appspot.apprtc.HWCODEC";
  public static final String EXTRA_WEBSOCKET = "org.appspot.apprtc.WEBSOCKET";
  private static final String TAG = "ConnectRTCClient";
  private final String APPRTC_SERVER = "https://apprtc.appspot.com";
  private final int CONNECTION_REQUEST = 1;
  private static boolean commandLineRun = false;

  private ImageButton addRoomButton;
  private ImageButton removeRoomButton;
  private ImageButton connectButton;
  private ImageButton connectLoopbackButton;
  private EditText roomEditText;
  private ListView roomListView;
  private SharedPreferences sharedPref;
  private String keyprefResolution;
  private String keyprefFps;
  private String keyprefBitrateType;
  private String keyprefBitrateValue;
  private String keyprefHwCodec;
  private String keyprefCpuUsageDetection;
  private String keyprefRoom;
  private String keyprefRoomList;
  private ArrayList<String> roomList;
  private ArrayAdapter<String> adapter;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // Get setting keys.
    PreferenceManager.setDefaultValues(this, R.xml.preferences, false);
    sharedPref = PreferenceManager.getDefaultSharedPreferences(this);
    keyprefResolution = getString(R.string.pref_resolution_key);
    keyprefFps = getString(R.string.pref_fps_key);
    keyprefBitrateType = getString(R.string.pref_startbitrate_key);
    keyprefBitrateValue = getString(R.string.pref_startbitratevalue_key);
    keyprefHwCodec = getString(R.string.pref_hwcodec_key);
    keyprefCpuUsageDetection = getString(R.string.pref_cpu_usage_detection_key);
    keyprefRoom = getString(R.string.pref_room_key);
    keyprefRoomList = getString(R.string.pref_room_list_key);

    setContentView(R.layout.activity_connect);

    roomEditText = (EditText) findViewById(R.id.room_edittext);
    roomEditText.setOnEditorActionListener(
      new TextView.OnEditorActionListener() {
        @Override
        public boolean onEditorAction(
            TextView textView, int i, KeyEvent keyEvent) {
          if (i == EditorInfo.IME_ACTION_DONE) {
            addRoomButton.performClick();
            return true;
          }
          return false;
        }
    });
    roomEditText.requestFocus();

    roomListView = (ListView) findViewById(R.id.room_listview);
    roomListView.setChoiceMode(ListView.CHOICE_MODE_SINGLE);

    addRoomButton = (ImageButton) findViewById(R.id.add_room_button);
    addRoomButton.setOnClickListener(addRoomListener);
    removeRoomButton = (ImageButton) findViewById(R.id.remove_room_button);
    removeRoomButton.setOnClickListener(removeRoomListener);
    connectButton = (ImageButton) findViewById(R.id.connect_button);
    connectButton.setOnClickListener(connectListener);
    connectLoopbackButton =
        (ImageButton) findViewById(R.id.connect_loopback_button);
    connectLoopbackButton.setOnClickListener(connectListener);

    // If an implicit VIEW intent is launching the app, go directly to that URL.
    final Intent intent = getIntent();
    if ("android.intent.action.VIEW".equals(intent.getAction()) &&
        !commandLineRun) {
      commandLineRun = true;
      boolean loopback = intent.getBooleanExtra(EXTRA_LOOPBACK, false);
      int runTimeMs = intent.getIntExtra(EXTRA_RUNTIME, 0);
      String room = sharedPref.getString(keyprefRoom, "");
      roomEditText.setText(room);
      connectToRoom(loopback, runTimeMs);
      return;
    }
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu) {
    getMenuInflater().inflate(R.menu.connect_menu, menu);
    return true;
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item) {
    // Handle presses on the action bar items.
    if (item.getItemId() == R.id.action_settings) {
      Intent intent = new Intent(this, SettingsActivity.class);
      startActivity(intent);
      return true;
    } else {
      return super.onOptionsItemSelected(item);
    }
  }

  @Override
  public void onPause() {
    super.onPause();
    String room = roomEditText.getText().toString();
    String roomListJson = new JSONArray(roomList).toString();
    SharedPreferences.Editor editor = sharedPref.edit();
    editor.putString(keyprefRoom, room);
    editor.putString(keyprefRoomList, roomListJson);
    editor.commit();
  }

  @Override
  public void onResume() {
    super.onResume();
    String room = sharedPref.getString(keyprefRoom, "");
    roomEditText.setText(room);
    roomList = new ArrayList<String>();
    String roomListJson = sharedPref.getString(keyprefRoomList, null);
    if (roomListJson != null) {
      try {
        JSONArray jsonArray = new JSONArray(roomListJson);
        for (int i = 0; i < jsonArray.length(); i++) {
          roomList.add(jsonArray.get(i).toString());
        }
      } catch (JSONException e) {
        Log.e(TAG, "Failed to load room list: " + e.toString());
      }
    }
    adapter = new ArrayAdapter<String>(
        this, android.R.layout.simple_list_item_1, roomList);
    roomListView.setAdapter(adapter);
    if (adapter.getCount() > 0) {
      roomListView.requestFocus();
      roomListView.setItemChecked(0, true);
    }
  }

  @Override
  protected void onActivityResult(
      int requestCode, int resultCode, Intent data) {
    if (requestCode == CONNECTION_REQUEST && commandLineRun) {
      Log.d(TAG, "Return: " + resultCode);
      setResult(resultCode);
      finish();
    }
  }

  private final OnClickListener connectListener = new OnClickListener() {
    @Override
    public void onClick(View view) {
      boolean loopback = false;
      if (view.getId() == R.id.connect_loopback_button) {
        loopback = true;
      }
      commandLineRun = false;
      connectToRoom(loopback, 0);
    }
  };

  private String appendQueryParameter(String url, String parameter) {
    String newUrl = url;
    if (newUrl.contains("?")) {
      newUrl += "&" + parameter;
    } else {
      newUrl += "?" + parameter;
    }
    return newUrl;
  }

  private void connectToRoom(boolean loopback, int runTimeMs) {
    // Get room name (random for loopback).
    String roomName;
    if (loopback) {
      roomName = Integer.toString((new Random()).nextInt(100000000));
    } else {
      roomName = getSelectedItem();
      if (roomName == null) {
        roomName = roomEditText.getText().toString();
      }
    }

    String url;
    url = APPRTC_SERVER + "/register/" + roomName;

    // Check HW codec flag.
    boolean hwCodec = sharedPref.getBoolean(keyprefHwCodec,
        Boolean.valueOf(getString(R.string.pref_hwcodec_default)));

    // Add video resolution constraints.
    String parametersResolution = null;
    String parametersFps = null;
    String resolution = sharedPref.getString(keyprefResolution,
        getString(R.string.pref_resolution_default));
    String[] dimensions = resolution.split("[ x]+");
    if (dimensions.length == 2) {
      try {
        int maxWidth = Integer.parseInt(dimensions[0]);
        int maxHeight = Integer.parseInt(dimensions[1]);
        if (maxWidth > 0 && maxHeight > 0) {
          parametersResolution = "minHeight=" + maxHeight + ",maxHeight=" +
              maxHeight + ",minWidth=" + maxWidth + ",maxWidth=" + maxWidth;
        }
      } catch (NumberFormatException e) {
        Log.e(TAG, "Wrong video resolution setting: " + resolution);
      }
    }

    // Add camera fps constraints.
    String fps = sharedPref.getString(keyprefFps,
        getString(R.string.pref_fps_default));
    String[] fpsValues = fps.split("[ x]+");
    if (fpsValues.length == 2) {
      try {
        int cameraFps = Integer.parseInt(fpsValues[0]);
        if (cameraFps > 0) {
          parametersFps = "minFrameRate=" + cameraFps +
              ",maxFrameRate=" + cameraFps;
        }
      } catch (NumberFormatException e) {
        Log.e(TAG, "Wrong camera fps setting: " + fps);
      }
    }

    // Modify connection URL.
    if (parametersResolution != null || parametersFps != null) {
      String urlVideoParameters = "video=";
      if (parametersResolution != null) {
        urlVideoParameters += parametersResolution;
        if (parametersFps != null) {
          urlVideoParameters += ",";
        }
      }
      if (parametersFps != null) {
        urlVideoParameters += parametersFps;
      }
      url = appendQueryParameter(url, urlVideoParameters);
    } else {
      if (hwCodec && MediaCodecVideoEncoder.isPlatformSupported()) {
        url = appendQueryParameter(url, "hd=true");
      }
    }

    // Get start bitrate.
    int startBitrate = 0;
    String bitrateTypeDefault = getString(R.string.pref_startbitrate_default);
    String bitrateType = sharedPref.getString(
        keyprefBitrateType, bitrateTypeDefault);
    if (!bitrateType.equals(bitrateTypeDefault)) {
      String bitrateValue = sharedPref.getString(keyprefBitrateValue,
          getString(R.string.pref_startbitratevalue_default));
      startBitrate = Integer.parseInt(bitrateValue);
    }

    // Test if CpuOveruseDetection should be disabled. By default is on.
    boolean cpuOveruseDetection = sharedPref.getBoolean(
        keyprefCpuUsageDetection,
        Boolean.valueOf(
            getString(R.string.pref_cpu_usage_detection_default)));
    if (!cpuOveruseDetection) {
      url = appendQueryParameter(url, "googCpuOveruseDetection=false");
    }

    // Start AppRTCDemo activity.
    Log.d(TAG, "Connecting to room " + roomName + " at URL " + url);
    if (validateUrl(url)) {
      Uri uri = Uri.parse(url);
      Intent intent = new Intent(this, AppRTCDemoActivity.class);
      intent.setData(uri);
      intent.putExtra(EXTRA_ROOMNAME, roomName);
      intent.putExtra(EXTRA_LOOPBACK, loopback);
      intent.putExtra(EXTRA_CMDLINE, commandLineRun);
      intent.putExtra(EXTRA_RUNTIME, runTimeMs);
      intent.putExtra(EXTRA_BITRATE, startBitrate);
      intent.putExtra(EXTRA_HWCODEC, hwCodec);
      startActivityForResult(intent, CONNECTION_REQUEST);
    }
  }

  private boolean validateUrl(String url) {
    if (URLUtil.isHttpsUrl(url) || URLUtil.isHttpUrl(url))
      return true;

    new AlertDialog.Builder(this)
        .setTitle(getText(R.string.invalid_url_title))
        .setMessage(getString(R.string.invalid_url_text, url))
        .setCancelable(false)
        .setNeutralButton(R.string.ok, new DialogInterface.OnClickListener() {
            public void onClick(DialogInterface dialog, int id) {
              dialog.cancel();
            }
          }).create().show();
    return false;
  }

  private final OnClickListener addRoomListener = new OnClickListener() {
    @Override
    public void onClick(View view) {
      String newRoom = roomEditText.getText().toString();
      if (newRoom.length() > 0 && !roomList.contains(newRoom)) {
        adapter.add(newRoom);
        adapter.notifyDataSetChanged();
      }
    }
  };

  private final OnClickListener removeRoomListener = new OnClickListener() {
    @Override
    public void onClick(View view) {
      String selectedRoom = getSelectedItem();
      if (selectedRoom != null) {
        adapter.remove(selectedRoom);
        adapter.notifyDataSetChanged();
      }
    }
  };

  private String getSelectedItem() {
    int position = AdapterView.INVALID_POSITION;
    if (roomListView.getCheckedItemCount() > 0 && adapter.getCount() > 0) {
      position = roomListView.getCheckedItemPosition();
      if (position >= adapter.getCount()) {
        position = AdapterView.INVALID_POSITION;
      }
    }
    if (position != AdapterView.INVALID_POSITION) {
      return adapter.getItem(position);
    } else {
      return null;
    }
  }

}
