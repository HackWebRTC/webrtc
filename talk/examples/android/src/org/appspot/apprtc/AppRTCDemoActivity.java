/*
 * libjingle
 * Copyright 2013, Google Inc.
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
import android.content.res.Configuration;
import android.graphics.Color;
import android.graphics.Point;
import android.media.AudioManager;
import android.os.Bundle;
import android.util.Log;
import android.util.TypedValue;
import android.view.View;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import org.appspot.apprtc.AppRTCClient.AppRTCSignalingParameters;
import org.webrtc.IceCandidate;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.SessionDescription;
import org.webrtc.StatsObserver;
import org.webrtc.StatsReport;
import org.webrtc.VideoRenderer;
import org.webrtc.VideoRendererGui;

/**
 * Main Activity of the AppRTCDemo Android app demonstrating interoperability
 * between the Android/Java implementation of PeerConnection and the
 * apprtc.appspot.com demo webapp.
 */
public class AppRTCDemoActivity extends Activity
    implements AppRTCClient.AppRTCSignalingEvents,
      PeerConnectionClient.PeerConnectionEvents {
  private static final String TAG = "AppRTCClient";
  private PeerConnectionClient pc;
  private AppRTCClient appRtcClient = new GAERTCClient(this, this);
  private AppRTCSignalingParameters appRtcParameters;
  private AppRTCGLView vsv;
  private VideoRenderer.Callbacks localRender;
  private VideoRenderer.Callbacks remoteRender;
  private Toast logToast;
  private final LayoutParams hudLayout =
      new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
  private TextView hudView;
  // Synchronize on quit[0] to avoid teardown-related crashes.
  private final Boolean[] quit = new Boolean[] { false };

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    Thread.setDefaultUncaughtExceptionHandler(
        new UnhandledExceptionHandler(this));

    getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

    Point displaySize = new Point();
    getWindowManager().getDefaultDisplay().getRealSize(displaySize);

    vsv = new AppRTCGLView(this, displaySize);
    VideoRendererGui.setView(vsv);
    remoteRender = VideoRendererGui.create(0, 0, 100, 100,
        VideoRendererGui.ScalingType.SCALE_ASPECT_FIT);
    localRender = VideoRendererGui.create(0, 0, 100, 100,
        VideoRendererGui.ScalingType.SCALE_ASPECT_FIT);

    vsv.setOnClickListener(new View.OnClickListener() {
        @Override public void onClick(View v) {
          toggleHUD();
        }
      });
    setContentView(vsv);
    logAndToast("Tap the screen to toggle stats visibility");

    hudView = new TextView(this);
    hudView.setTextColor(Color.BLACK);
    hudView.setBackgroundColor(Color.WHITE);
    hudView.setAlpha(0.4f);
    hudView.setTextSize(TypedValue.COMPLEX_UNIT_PT, 5);
    hudView.setVisibility(View.INVISIBLE);
    addContentView(hudView, hudLayout);

    AudioManager audioManager =
        ((AudioManager) getSystemService(AUDIO_SERVICE));
    // TODO(fischman): figure out how to do this Right(tm) and remove the
    // suppression.
    @SuppressWarnings("deprecation")
    boolean isWiredHeadsetOn = audioManager.isWiredHeadsetOn();
    audioManager.setMode(isWiredHeadsetOn ?
        AudioManager.MODE_IN_CALL : AudioManager.MODE_IN_COMMUNICATION);
    audioManager.setSpeakerphoneOn(!isWiredHeadsetOn);

    final Intent intent = getIntent();
    if ("android.intent.action.VIEW".equals(intent.getAction())) {
      connectToRoom(intent.getData().toString());
      return;
    }
    showGetRoomUI();
  }

  @Override
  public void onPause() {
    super.onPause();
    vsv.onPause();
    if (pc != null) {
      pc.stopVideoSource();
    }
  }

  @Override
  public void onResume() {
    super.onResume();
    vsv.onResume();
    if (pc != null) {
      pc.startVideoSource();
    }
  }

  @Override
  public void onConfigurationChanged (Configuration newConfig) {
    Point displaySize = new Point();
    getWindowManager().getDefaultDisplay().getSize(displaySize);
    vsv.updateDisplaySize(displaySize);
    super.onConfigurationChanged(newConfig);
  }

  @Override
  protected void onDestroy() {
    disconnectAndExit();
    super.onDestroy();
  }

  private void showGetRoomUI() {
    final EditText roomInput = new EditText(this);
    roomInput.setText("https://apprtc.appspot.com/?r=");
    roomInput.setSelection(roomInput.getText().length());
    DialogInterface.OnClickListener listener =
        new DialogInterface.OnClickListener() {
          @Override
          public void onClick(DialogInterface dialog, int which) {
            abortUnless(which == DialogInterface.BUTTON_POSITIVE, "lolwat?");
            dialog.dismiss();
            connectToRoom(roomInput.getText().toString());
          }
        };
    AlertDialog.Builder builder = new AlertDialog.Builder(this);
    builder
        .setMessage("Enter room URL").setView(roomInput)
        .setPositiveButton("Go!", listener).show();
  }

  private void connectToRoom(String roomUrl) {
    logAndToast("Connecting to room...");
    appRtcClient.connectToRoom(roomUrl);
  }

  // Toggle visibility of the heads-up display.
  private void toggleHUD() {
    if (hudView.getVisibility() == View.VISIBLE) {
      hudView.setVisibility(View.INVISIBLE);
    } else {
      hudView.setVisibility(View.VISIBLE);
    }
  }

  // Update the heads-up display with information from |reports|.
  private void updateHUD(StatsReport[] reports) {
    StringBuilder builder = new StringBuilder();
    for (StatsReport report : reports) {
      // bweforvideo to show statistics for video Bandwidth Estimation,
      // which is global per-session.
      if (report.id.equals("bweforvideo")) {
        for (StatsReport.Value value : report.values) {
          String name = value.name.replace("goog", "")
              .replace("Available", "").replace("Bandwidth", "")
              .replace("Bitrate", "").replace("Enc", "");

          builder.append(name).append("=").append(value.value)
              .append(" ");
        }
        builder.append("\n");
      } else if (report.type.equals("googCandidatePair")) {
        String activeConnectionStats = getActiveConnectionStats(report);
        if (activeConnectionStats == null) {
          continue;
        }
        builder.append(activeConnectionStats);
      } else {
        continue;
      }
      builder.append("\n");
    }
    hudView.setText(builder.toString() + hudView.getText());
  }

  // Return the active connection stats else return null
  private String getActiveConnectionStats(StatsReport report) {
    StringBuilder activeConnectionbuilder = new StringBuilder();
    // googCandidatePair to show information about the active
    // connection.
    for (StatsReport.Value value : report.values) {
      if (value.name.equals("googActiveConnection")
          && value.value.equals("false")) {
        return null;
      }
      String name = value.name.replace("goog", "");
      activeConnectionbuilder.append(name).append("=")
          .append(value.value).append("\n");
    }
    return activeConnectionbuilder.toString();
  }

  // Disconnect from remote resources, dispose of local resources, and exit.
  private void disconnectAndExit() {
    synchronized (quit[0]) {
      if (quit[0]) {
        return;
      }
      quit[0] = true;
      if (pc != null) {
        pc.close();
        pc = null;
      }
      if (appRtcClient != null) {
        appRtcClient.disconnect();
        appRtcClient = null;
      }
      finish();
    }
  }

  // Poor-man's assert(): die with |msg| unless |condition| is true.
  private static void abortUnless(boolean condition, String msg) {
    if (!condition) {
      throw new RuntimeException(msg);
    }
  }

  // Log |msg| and Toast about it.
  private void logAndToast(String msg) {
    Log.d(TAG, msg);
    if (logToast != null) {
      logToast.cancel();
    }
    logToast = Toast.makeText(this, msg, Toast.LENGTH_SHORT);
    logToast.show();
  }

  // -----Implementation of AppRTCClient.AppRTCSignalingEvents ---------------
  // All events are called from UI thread.
  @Override
  public void onConnectedToRoom(final AppRTCSignalingParameters params) {
    appRtcParameters = params;
    abortUnless(PeerConnectionFactory.initializeAndroidGlobals(
      this, true, true, VideoRendererGui.getEGLContext()),
        "Failed to initializeAndroidGlobals");
    logAndToast("Creating peer connection...");
    pc = new PeerConnectionClient(
        this, localRender, remoteRender, appRtcParameters, this);

    {
      final PeerConnectionClient finalPC = pc;
      final Runnable repeatedStatsLogger = new Runnable() {
          public void run() {
            synchronized (quit[0]) {
              if (quit[0]) {
                return;
              }
              final Runnable runnableThis = this;
              if (hudView.getVisibility() == View.INVISIBLE) {
                vsv.postDelayed(runnableThis, 1000);
                return;
              }
              boolean success = finalPC.getStats(new StatsObserver() {
                  public void onComplete(final StatsReport[] reports) {
                    runOnUiThread(new Runnable() {
                        public void run() {
                          updateHUD(reports);
                        }
                      });
                    for (StatsReport report : reports) {
                      Log.d(TAG, "Stats: " + report.toString());
                    }
                    vsv.postDelayed(runnableThis, 1000);
                  }
                }, null);
              if (!success) {
                throw new RuntimeException("getStats() return false!");
              }
            }
          }
        };
      vsv.postDelayed(repeatedStatsLogger, 1000);
    }

    logAndToast("Waiting for remote connection...");
  }

  @Override
  public void onChannelOpen() {
    if (appRtcParameters.initiator) {
      logAndToast("Creating OFFER...");
      // Create offer. Offer SDP will be sent to answering client in
      // PeerConnectionEvents.onLocalDescription event.
      pc.createOffer();
    }
  }

  @Override
  public void onRemoteDescription(final SessionDescription sdp) {
    logAndToast("Received remote " + sdp.type + " ...");
    pc.setRemoteDescription(sdp);
    if (!appRtcParameters.initiator) {
      logAndToast("Creating ANSWER...");
      // Create answer. Answer SDP will be sent to offering client in
      // PeerConnectionEvents.onLocalDescription event.
      pc.createAnswer();
    }
  }

  @Override
  public void onRemoteIceCandidate(final IceCandidate candidate) {
    pc.addRemoteIceCandidate(candidate);
  }

  @Override
  public void onChannelClose() {
    logAndToast("Remote end hung up; dropping PeerConnection");
    disconnectAndExit();
  }

  @Override
  public void onChannelError(int code, String description) {
    logAndToast("Channel error: " + code + ". " + description);
    disconnectAndExit();
  }

  // -----Implementation of PeerConnectionClient.PeerConnectionEvents.---------
  // Send local peer connection SDP and ICE candidates to remote party.
  // All callbacks are invoked from UI thread.
  @Override
  public void onLocalDescription(final SessionDescription sdp) {
    logAndToast("Sending " + sdp.type + " ...");
    appRtcClient.sendLocalDescription(sdp);
  }

  @Override
  public void onIceCandidate(final IceCandidate candidate) {
    appRtcClient.sendLocalIceCandidate(candidate);
  }

  @Override
  public void onIceConnected() {
    logAndToast("ICE connected");
    VideoRendererGui.update(localRender, 70, 70, 28, 28,
        VideoRendererGui.ScalingType.SCALE_ASPECT_FIT);
  }
}
