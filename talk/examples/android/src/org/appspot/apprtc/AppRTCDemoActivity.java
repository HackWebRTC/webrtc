/*
 * libjingle
 * Copyright 2013 Google Inc.
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

import org.appspot.apprtc.AppRTCClient.SignalingParameters;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.Fragment;
import android.content.DialogInterface;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.util.Log;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.WindowManager;
import android.widget.ImageButton;
import android.widget.TextView;
import android.widget.Toast;

import org.webrtc.IceCandidate;
import org.webrtc.SessionDescription;
import org.webrtc.StatsObserver;
import org.webrtc.StatsReport;
import org.webrtc.VideoRenderer;
import org.webrtc.VideoRendererGui;
import org.webrtc.VideoRendererGui.ScalingType;

import java.util.HashMap;
import java.util.Map;

/**
 * Activity of the AppRTCDemo Android app demonstrating interoperability
 * between the Android/Java implementation of PeerConnection and the
 * apprtc.appspot.com demo webapp.
 */
public class AppRTCDemoActivity extends Activity
    implements AppRTCClient.SignalingEvents,
      PeerConnectionClient.PeerConnectionEvents {
  private static final String TAG = "AppRTCClient";
  private PeerConnectionClient pc = null;
  private AppRTCClient appRtcClient;
  private SignalingParameters signalingParameters;
  private AppRTCAudioManager audioManager = null;
  private View rootView;
  private View menuBar;
  private GLSurfaceView videoView;
  private VideoRenderer.Callbacks localRender;
  private VideoRenderer.Callbacks remoteRender;
  private ScalingType scalingType;
  private Toast logToast;
  private final LayoutParams hudLayout =
      new LayoutParams(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
  private TextView hudView;
  private TextView encoderStatView;
  private TextView roomNameView;
  private ImageButton videoScalingButton;
  private String roomName;
  private boolean commandLineRun;
  private boolean activityRunning;
  private int runTimeMs;
  private int startBitrate;
  private String videoCodec;
  private boolean hwCodecAcceleration;
  private boolean iceConnected;
  private boolean isError;

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // Set window styles for fullscreen-window size. Needs to be done before
    // adding content.
    requestWindowFeature(Window.FEATURE_NO_TITLE);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
    getWindow().getDecorView().setSystemUiVisibility(
        View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
        | View.SYSTEM_UI_FLAG_FULLSCREEN
        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);

    setContentView(R.layout.activity_fullscreen);

    Thread.setDefaultUncaughtExceptionHandler(
        new UnhandledExceptionHandler(this));
    iceConnected = false;
    signalingParameters = null;

    rootView = findViewById(android.R.id.content);
    encoderStatView = (TextView) findViewById(R.id.encoder_stat);
    menuBar = findViewById(R.id.menubar_fragment);
    roomNameView = (TextView) findViewById(R.id.room_name);
    videoView = (GLSurfaceView) findViewById(R.id.glview);

    VideoRendererGui.setView(videoView, new Runnable() {
      @Override
      public void run() {
        createPeerConnectionFactory();
      }
    });
    scalingType = ScalingType.SCALE_ASPECT_FILL;
    remoteRender = VideoRendererGui.create(0, 0, 100, 100, scalingType, false);
    localRender = VideoRendererGui.create(0, 0, 100, 100, scalingType, true);

    videoView.setOnClickListener(
        new View.OnClickListener() {
          @Override
          public void onClick(View view) {
            int visibility = menuBar.getVisibility() == View.VISIBLE
                    ? View.INVISIBLE : View.VISIBLE;
            encoderStatView.setVisibility(visibility);
            menuBar.setVisibility(visibility);
            roomNameView.setVisibility(visibility);
            if (visibility == View.VISIBLE) {
              encoderStatView.bringToFront();
              menuBar.bringToFront();
              roomNameView.bringToFront();
              rootView.invalidate();
            }
          }
        });

    ((ImageButton) findViewById(R.id.button_disconnect)).setOnClickListener(
        new View.OnClickListener() {
          @Override
          public void onClick(View view) {
            logAndToast("Disconnecting call.");
            disconnect();
          }
        });

    ((ImageButton) findViewById(R.id.button_switch_camera)).setOnClickListener(
        new View.OnClickListener() {
          @Override
          public void onClick(View view) {
            if (pc != null) {
              pc.switchCamera();
            }
          }
        });

    ((ImageButton) findViewById(R.id.button_toggle_debug)).setOnClickListener(
        new View.OnClickListener() {
          @Override
          public void onClick(View view) {
            int visibility = hudView.getVisibility() == View.VISIBLE
                ? View.INVISIBLE : View.VISIBLE;
            hudView.setVisibility(visibility);
          }
        });

    videoScalingButton = (ImageButton) findViewById(R.id.button_scaling_mode);
    videoScalingButton.setOnClickListener(
        new View.OnClickListener() {
          @Override
          public void onClick(View view) {
            if (scalingType == ScalingType.SCALE_ASPECT_FILL) {
              videoScalingButton.setBackgroundResource(
                  R.drawable.ic_action_full_screen);
              scalingType = ScalingType.SCALE_ASPECT_FIT;
            } else {
              videoScalingButton.setBackgroundResource(
                  R.drawable.ic_action_return_from_full_screen);
              scalingType = ScalingType.SCALE_ASPECT_FILL;
            }
            updateVideoView();
          }
        });

    hudView = new TextView(this);
    hudView.setTextColor(Color.BLACK);
    hudView.setBackgroundColor(Color.WHITE);
    hudView.setAlpha(0.4f);
    hudView.setTextSize(TypedValue.COMPLEX_UNIT_PT, 5);
    hudView.setVisibility(View.INVISIBLE);
    addContentView(hudView, hudLayout);

    final Intent intent = getIntent();
    Uri url = intent.getData();
    roomName = intent.getStringExtra(ConnectActivity.EXTRA_ROOMNAME);
    boolean loopback = intent.getBooleanExtra(
        ConnectActivity.EXTRA_LOOPBACK, false);
    commandLineRun = intent.getBooleanExtra(
        ConnectActivity.EXTRA_CMDLINE, false);
    runTimeMs = intent.getIntExtra(ConnectActivity.EXTRA_RUNTIME, 0);
    startBitrate = intent.getIntExtra(ConnectActivity.EXTRA_BITRATE, 0);
    if (intent.hasExtra(ConnectActivity.EXTRA_VIDEOCODEC)) {
      videoCodec = intent.getStringExtra(ConnectActivity.EXTRA_VIDEOCODEC);
    } else {
      videoCodec = PeerConnectionClient.VIDEO_CODEC_VP8; // use VP8 by default.
    }
    hwCodecAcceleration = intent.getBooleanExtra(
        ConnectActivity.EXTRA_HWCODEC, true);

    if (url != null) {
      if (loopback || (roomName != null && !roomName.equals(""))) {
        // Start room connection.
        logAndToast(getString(R.string.connecting_to, url));
        appRtcClient = new WebSocketRTCClient(this);
        appRtcClient.connectToRoom(url.toString(), loopback);
        if (loopback) {
          roomNameView.setText("loopback");
        } else {
          roomNameView.setText(roomName);
        }

        // Create and audio manager that will take care of audio routing,
        // audio modes, audio device enumeration etc.
        audioManager = AppRTCAudioManager.create(this, new Runnable() {
            // This method will be called each time the audio state (number and
            // type of devices) has been changed.
            @Override
            public void run() {
              onAudioManagerChangedState();
            }
          }
        );
        // Store existing audio settings and change audio mode to
        // MODE_IN_COMMUNICATION for best possible VoIP performance.
        Log.d(TAG, "Initializing the audio manager...");
        audioManager.init();

        // For command line execution run connection for <runTimeMs> and exit.
        if (commandLineRun && runTimeMs > 0) {
          videoView.postDelayed(new Runnable() {
            public void run() {
              disconnect();
            }
          }, runTimeMs);
        }
      } else {
        logAndToast("Empty or missing room name!");
        setResult(RESULT_CANCELED);
        finish();
      }
    } else {
      logAndToast(getString(R.string.missing_url));
      Log.e(TAG, "Didn't get any URL in intent!");
      setResult(RESULT_CANCELED);
      finish();
    }
  }

  // Create peer connection factory when EGL context is ready.
  private void createPeerConnectionFactory() {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        if (pc == null) {
          pc = new PeerConnectionClient();
          pc.createPeerConnectionFactory(AppRTCDemoActivity.this,
              videoCodec, hwCodecAcceleration,
              VideoRendererGui.getEGLContext(), AppRTCDemoActivity.this);
        }
        if (signalingParameters != null) {
          Log.w(TAG, "EGL context is ready after room connection.");
          onConnectedToRoomInternal(signalingParameters);
        }
      }
    });
  }

  /**
   * MenuBar fragment for AppRTC.
   */
  public static class MenuBarFragment extends Fragment {
    @Override
    public View onCreateView(
        LayoutInflater inflater,
        ViewGroup container,
        Bundle savedInstanceState) {
      return inflater.inflate(R.layout.fragment_menubar, container, false);
    }
  }

  @Override
  public void onPause() {
    super.onPause();
    videoView.onPause();
    activityRunning = false;
    if (pc != null) {
      pc.stopVideoSource();
    }
  }

  @Override
  public void onResume() {
    super.onResume();
    videoView.onResume();
    activityRunning = true;
    if (pc != null) {
      pc.startVideoSource();
    }
  }

  @Override
  protected void onDestroy() {
    disconnect();
    super.onDestroy();
    if (logToast != null) {
      logToast.cancel();
    }
    activityRunning = false;
  }

  private void updateVideoView() {
    VideoRendererGui.update(remoteRender, 0, 0, 100, 100, scalingType);
    if (iceConnected) {
      VideoRendererGui.update(localRender, 70, 70, 28, 28,
          ScalingType.SCALE_ASPECT_FIT);
    } else {
      VideoRendererGui.update(localRender, 0, 0, 100, 100, scalingType);
    }
  }

  private void onAudioManagerChangedState() {
    // TODO(henrika): disable video if AppRTCAudioManager.AudioDevice.EARPIECE
    // is active.
  }

  // Disconnect from remote resources, dispose of local resources, and exit.
  private void disconnect() {
    if (appRtcClient != null) {
      appRtcClient.disconnectFromRoom();
      appRtcClient = null;
    }
    if (pc != null) {
      pc.close();
      pc = null;
    }
    if (audioManager != null) {
      audioManager.close();
      audioManager = null;
    }
    if (iceConnected && !isError) {
      setResult(RESULT_OK);
    } else {
      setResult(RESULT_CANCELED);
    }
    finish();
  }

  private void disconnectWithErrorMessage(final String errorMessage) {
    if (commandLineRun || !activityRunning) {
      Log.e(TAG, "Critical error: " + errorMessage);
      disconnect();
    } else {
      new AlertDialog.Builder(this)
      .setTitle(getText(R.string.channel_error_title))
      .setMessage(errorMessage)
      .setCancelable(false)
      .setNeutralButton(R.string.ok, new DialogInterface.OnClickListener() {
          public void onClick(DialogInterface dialog, int id) {
            dialog.cancel();
            disconnect();
          }
        }).create().show();
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

  // Return the active connection stats,
  // or null if active connection is not found.
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

  // Update the heads-up display with information from |reports|.
  private void updateHUD(StatsReport[] reports) {
    StringBuilder builder = new StringBuilder();
    for (StatsReport report : reports) {
      Log.d(TAG, "Stats: " + report.toString());
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

  private Map<String, String> getReportMap(StatsReport report) {
    Map<String, String> reportMap = new HashMap<String, String>();
    for (StatsReport.Value value : report.values) {
      reportMap.put(value.name, value.value);
    }
    return reportMap;
  }

  // Update encoder statistics view with information from |reports|.
  private void updateEncoderStatistics(StatsReport[] reports) {
    if (!iceConnected) {
      return;
    }
    String fps = null;
    String targetBitrate = null;
    String actualBitrate = null;
    for (StatsReport report : reports) {
      if (report.type.equals("ssrc") && report.id.contains("ssrc")
          && report.id.contains("send")) {
        Map<String, String> reportMap = getReportMap(report);
        String trackId = reportMap.get("googTrackId");
        if (trackId != null
            && trackId.contains(PeerConnectionClient.VIDEO_TRACK_ID)) {
          fps = reportMap.get("googFrameRateSent");
        }
      } else if (report.id.equals("bweforvideo")) {
        Map<String, String> reportMap = getReportMap(report);
        targetBitrate = reportMap.get("googTargetEncBitrate");
        actualBitrate = reportMap.get("googActualEncBitrate");
      }
    }
    String stat = "";
    if (fps != null) {
      stat += "Fps:  " + fps + "\n";
    }
    if (targetBitrate != null) {
      stat += "Target BR: " + targetBitrate + "\n";
    }
    if (actualBitrate != null) {
      stat += "Actual BR: " + actualBitrate;
    }
    encoderStatView.setText(stat);
  }

  // -----Implementation of AppRTCClient.AppRTCSignalingEvents ---------------
  // All callbacks are invoked from websocket signaling looper thread and
  // are routed to UI thread.
  private void onConnectedToRoomInternal(final SignalingParameters params) {
    signalingParameters = params;
    if (pc == null) {
      Log.w(TAG, "Room is connected, but EGL context is not ready yet.");
      return;
    }
    logAndToast("Creating peer connection...");
    pc.createPeerConnection(
        localRender, remoteRender, signalingParameters, startBitrate);

    // Schedule statistics display.
    final Runnable repeatedStatsLogger = new Runnable() {
      public void run() {
        if (pc == null) {
          return;
        }
        final Runnable runnableThis = this;
        if (hudView.getVisibility() == View.INVISIBLE
            && encoderStatView.getVisibility() == View.INVISIBLE) {
          videoView.postDelayed(runnableThis, 1000);
          return;
        }
        boolean success = pc.getStats(new StatsObserver() {
            public void onComplete(final StatsReport[] reports) {
              runOnUiThread(new Runnable() {
                  public void run() {
                    if (hudView.getVisibility() == View.VISIBLE) {
                      updateHUD(reports);
                    }
                    if (encoderStatView.getVisibility() == View.VISIBLE) {
                      updateEncoderStatistics(reports);
                    }
                  }
                });
              videoView.postDelayed(runnableThis, 1000);
            }
          }, null);
        if (!success) {
          Log.w(TAG, "getStats() return false!");
          videoView.postDelayed(runnableThis, 1000);
        }
      }
    };
    videoView.postDelayed(repeatedStatsLogger, 1000);

    if (signalingParameters.initiator) {
      logAndToast("Creating OFFER...");
      // Create offer. Offer SDP will be sent to answering client in
      // PeerConnectionEvents.onLocalDescription event.
      pc.createOffer();
    }
  }

  @Override
  public void onConnectedToRoom(final SignalingParameters params) {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        onConnectedToRoomInternal(params);
      }
    });
  }

  @Override
  public void onRemoteDescription(final SessionDescription sdp) {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        if (pc == null) {
          return;
        }
        logAndToast("Received remote " + sdp.type + " ...");
        pc.setRemoteDescription(sdp);
        if (!signalingParameters.initiator) {
          logAndToast("Creating ANSWER...");
          // Create answer. Answer SDP will be sent to offering client in
          // PeerConnectionEvents.onLocalDescription event.
          pc.createAnswer();
        }
      }
    });
  }

  @Override
  public void onRemoteIceCandidate(final IceCandidate candidate) {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        if (pc != null) {
          pc.addRemoteIceCandidate(candidate);
        }
      }
    });
  }

  @Override
  public void onChannelClose() {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        logAndToast("Remote end hung up; dropping PeerConnection");
        disconnect();
      }
    });
  }

  @Override
  public void onChannelError(final String description) {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        if (!isError) {
          isError = true;
          disconnectWithErrorMessage(description);
        }
      }
    });
  }

  // -----Implementation of PeerConnectionClient.PeerConnectionEvents.---------
  // Send local peer connection SDP and ICE candidates to remote party.
  // All callbacks are invoked from peer connection client looper thread and
  // are routed to UI thread.
  @Override
  public void onLocalDescription(final SessionDescription sdp) {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        if (appRtcClient != null) {
          logAndToast("Sending " + sdp.type + " ...");
          if (signalingParameters.initiator) {
            appRtcClient.sendOfferSdp(sdp);
          } else {
            appRtcClient.sendAnswerSdp(sdp);
          }
        }
      }
    });
  }

  @Override
  public void onIceCandidate(final IceCandidate candidate) {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        if (appRtcClient != null) {
          appRtcClient.sendLocalIceCandidate(candidate);
        }
      }
    });
  }

  @Override
  public void onIceConnected() {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        logAndToast("ICE connected");
        iceConnected = true;
        updateVideoView();
      }
    });
  }

  @Override
  public void onIceDisconnected() {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        logAndToast("ICE disconnected");
        iceConnected = false;
        disconnect();
      }
    });
  }

  @Override
  public void onPeerConnectionClosed() {
  }

  @Override
  public void onPeerConnectionError(final String description) {
    runOnUiThread(new Runnable() {
      @Override
      public void run() {
        if (!isError) {
          isError = true;
          disconnectWithErrorMessage(description);
        }
      }
    });
  }
}
