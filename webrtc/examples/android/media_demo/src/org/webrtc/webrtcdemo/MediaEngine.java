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

import android.app.AlertDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.os.Environment;
import android.util.Log;
import android.view.OrientationEventListener;
import java.io.File;

public class MediaEngine {
  private static final String LOG_DIR = "webrtc";

  // Checks for and communicate failures to user (logcat and popup).
  private void check(boolean value, String message) {
    if (value) {
      return;
    }
    Log.e("WEBRTC-CHECK", message);
    AlertDialog alertDialog = new AlertDialog.Builder(context).create();
    alertDialog.setTitle("WebRTC Error");
    alertDialog.setMessage(message);
    alertDialog.setButton(DialogInterface.BUTTON_POSITIVE,
        "OK",
        new DialogInterface.OnClickListener() {
          public void onClick(DialogInterface dialog, int which) {
            dialog.dismiss();
            return;
          }
        }
                          );
    alertDialog.show();
  }


  // Shared Audio/Video members.
  private final Context context;
  private String remoteIp;
  private boolean enableTrace;

    // Audio
  private VoiceEngine voe;
  private int audioChannel;
  private boolean audioEnabled;
  private boolean voeRunning;
  private int audioCodecIndex;
  private int audioTxPort;
  private int audioRxPort;

  private boolean speakerEnabled;
  private boolean headsetPluggedIn;
  private boolean enableAgc;
  private boolean enableNs;
  private boolean enableAecm;

  private BroadcastReceiver headsetListener;

  private boolean audioRtpDump;
  private boolean apmRecord;

  private int inFps;
  private int inKbps;
  private int outFps;
  private int outKbps;
  private int inWidth;
  private int inHeight;

  public MediaEngine(Context context) {
    this.context = context;
    voe = new VoiceEngine();
    check(voe.init() == 0, "Failed voe Init");
    audioChannel = voe.createChannel();
    check(audioChannel >= 0, "Failed voe CreateChannel");
    check(audioChannel >= 0, "Failed voe CreateChannel");

    check(voe.setAecmMode(VoiceEngine.AecmModes.SPEAKERPHONE, false) == 0,
        "VoE set Aecm speakerphone mode failed");

    // Set audio mode to communication
    AudioManager audioManager =
        ((AudioManager) context.getSystemService(Context.AUDIO_SERVICE));
    audioManager.setMode(AudioManager.MODE_IN_COMMUNICATION);
    // Listen to headset being plugged in/out.
    IntentFilter receiverFilter = new IntentFilter(Intent.ACTION_HEADSET_PLUG);
    headsetListener = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
          if (intent.getAction().compareTo(Intent.ACTION_HEADSET_PLUG) == 0) {
            headsetPluggedIn = intent.getIntExtra("state", 0) == 1;
            updateAudioOutput();
          }
        }
      };
    context.registerReceiver(headsetListener, receiverFilter);
  }

  public void dispose() {
    check(!voeRunning && !voeRunning, "Engines must be stopped before dispose");
    context.unregisterReceiver(headsetListener);
    check(voe.deleteChannel(audioChannel) == 0, "VoE delete channel failed");
    voe.dispose();
  }

  public void start() {
    if (audioEnabled) {
      startVoE();
    }
  }

  public void stop() {
    stopVoe();
  }

  public boolean isRunning() {
    return voeRunning;
  }

  public void setRemoteIp(String remoteIp) {
    this.remoteIp = remoteIp;
    UpdateSendDestination();
  }

  public String remoteIp() { return remoteIp; }

  private String getDebugDirectory() {
    // Should create a folder in /scard/|LOG_DIR|
    return Environment.getExternalStorageDirectory().toString() + "/" +
        LOG_DIR;
  }

  private boolean createDebugDirectory() {
    File webrtc_dir = new File(getDebugDirectory());
    if (!webrtc_dir.exists()) {
      return webrtc_dir.mkdir();
    }
    return webrtc_dir.isDirectory();
  }

  public void startVoE() {
    check(!voeRunning, "VoE already started");
    check(voe.startListen(audioChannel) == 0, "Failed StartListen");
    check(voe.startPlayout(audioChannel) == 0, "VoE start playout failed");
    check(voe.startSend(audioChannel) == 0, "VoE start send failed");
    voeRunning = true;
  }

  private void stopVoe() {
    check(voeRunning, "VoE not started");
    check(voe.stopSend(audioChannel) == 0, "VoE stop send failed");
    check(voe.stopPlayout(audioChannel) == 0, "VoE stop playout failed");
    check(voe.stopListen(audioChannel) == 0, "VoE stop listen failed");
    voeRunning = false;
  }

  public void setAudio(boolean audioEnabled) {
    this.audioEnabled = audioEnabled;
  }

  public boolean audioEnabled() { return audioEnabled; }

  public int audioCodecIndex() { return audioCodecIndex; }

  public void setAudioCodec(int codecNumber) {
    audioCodecIndex = codecNumber;
    CodecInst codec = voe.getCodec(codecNumber);
    check(voe.setSendCodec(audioChannel, codec) == 0, "Failed setSendCodec");
    codec.dispose();
  }

  public String[] audioCodecsAsString() {
    String[] retVal = new String[voe.numOfCodecs()];
    for (int i = 0; i < voe.numOfCodecs(); ++i) {
      CodecInst codec = voe.getCodec(i);
      retVal[i] = codec.toString();
      codec.dispose();
    }
    return retVal;
  }

  private CodecInst[] defaultAudioCodecs() {
    CodecInst[] retVal = new CodecInst[voe.numOfCodecs()];
     for (int i = 0; i < voe.numOfCodecs(); ++i) {
      retVal[i] = voe.getCodec(i);
    }
    return retVal;
  }

  public int getIsacIndex() {
    CodecInst[] codecs = defaultAudioCodecs();
    for (int i = 0; i < codecs.length; ++i) {
      if (codecs[i].name().contains("ISAC")) {
        return i;
      }
    }
    return 0;
  }

  public void setAudioTxPort(int audioTxPort) {
    this.audioTxPort = audioTxPort;
    UpdateSendDestination();
  }

  public int audioTxPort() { return audioTxPort; }

  public void setAudioRxPort(int audioRxPort) {
    check(voe.setLocalReceiver(audioChannel, audioRxPort) == 0,
        "Failed setLocalReceiver");
    this.audioRxPort = audioRxPort;
  }

  public int audioRxPort() { return audioRxPort; }

  public boolean agcEnabled() { return enableAgc; }

  public void setAgc(boolean enable) {
    enableAgc = enable;
    VoiceEngine.AgcConfig agc_config =
        new VoiceEngine.AgcConfig(3, 9, true);
    check(voe.setAgcConfig(agc_config) == 0, "VoE set AGC Config failed");
    check(voe.setAgcStatus(enableAgc, VoiceEngine.AgcModes.FIXED_DIGITAL) == 0,
        "VoE set AGC Status failed");
  }

  public boolean nsEnabled() { return enableNs; }

  public void setNs(boolean enable) {
    enableNs = enable;
    check(voe.setNsStatus(enableNs,
            VoiceEngine.NsModes.MODERATE_SUPPRESSION) == 0,
        "VoE set NS Status failed");
  }

  public boolean aecmEnabled() { return enableAecm; }

  public void setEc(boolean enable) {
    enableAecm = enable;
    check(voe.setEcStatus(enable, VoiceEngine.EcModes.AECM) == 0,
        "voe setEcStatus");
  }

  public boolean speakerEnabled() {
    return speakerEnabled;
  }

  public void setSpeaker(boolean enable) {
    speakerEnabled = enable;
    updateAudioOutput();
  }

  // Debug helpers.
  public boolean apmRecord() { return apmRecord; }

  public boolean audioRtpDump() { return audioRtpDump; }

  public void setDebuging(boolean enable) {
    apmRecord = enable;
    if (!enable) {
      check(voe.stopDebugRecording() == 0, "Failed stopping debug");
      return;
    }
    if (!createDebugDirectory()) {
      check(false, "Unable to create debug directory.");
      return;
    }
    String debugDirectory = getDebugDirectory();
    check(voe.startDebugRecording(debugDirectory +  String.format("/apm_%d.dat",
                System.currentTimeMillis())) == 0,
        "Failed starting debug");
  }

  public void setIncomingVoeRtpDump(boolean enable) {
    audioRtpDump = enable;
    if (!enable) {
      check(voe.stopRtpDump(audioChannel,
              VoiceEngine.RtpDirections.INCOMING) == 0,
          "voe stopping rtp dump");
      return;
    }
    String debugDirectory = getDebugDirectory();
    check(voe.startRtpDump(audioChannel, debugDirectory +
            String.format("/voe_%d.rtp", System.currentTimeMillis()),
            VoiceEngine.RtpDirections.INCOMING) == 0,
        "voe starting rtp dump");
  }

  private void updateAudioOutput() {
    boolean useSpeaker = !headsetPluggedIn && speakerEnabled;
    AudioManager audioManager =
        ((AudioManager) context.getSystemService(Context.AUDIO_SERVICE));
    audioManager.setSpeakerphoneOn(useSpeaker);
  }

  private void UpdateSendDestination() {
    if (remoteIp == null) {
      return;
    }
    if (audioTxPort != 0) {
      check(voe.setSendDestination(audioChannel, audioTxPort,
              remoteIp) == 0, "VoE set send destination failed");
    }
  }

  MediaEngineObserver observer;
  public void setObserver(MediaEngineObserver observer) {
    this.observer = observer;
  }
}
