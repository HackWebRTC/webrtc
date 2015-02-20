/*
 * libjingle
 * Copyright 2015 Google Inc.
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
import android.app.Fragment;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.TextView;

import org.webrtc.StatsReport;
import org.webrtc.VideoRendererGui.ScalingType;

import java.util.HashMap;
import java.util.Map;

/**
 * Fragment for call control.
 */
public class CallFragment extends Fragment {
  private View controlView;
  private TextView encoderStatView;
  private TextView roomIdView;
  private ImageButton disconnectButton;
  private ImageButton cameraSwitchButton;
  private ImageButton videoScalingButton;
  private ImageButton toggleDebugButton;
  private OnCallEvents callEvents;
  private ScalingType scalingType;
  private boolean displayHud;
  private volatile boolean isRunning;
  private TextView hudView;
  private final CpuMonitor cpuMonitor = new CpuMonitor();

  /**
   * Call control interface for container activity.
   */
  public interface OnCallEvents {
    public void onCallHangUp();
    public void onCameraSwitch();
    public void onVideoScalingSwitch(ScalingType scalingType);
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container,
      Bundle savedInstanceState) {
    controlView =
        inflater.inflate(R.layout.fragment_call, container, false);

    // Create UI controls.
    encoderStatView =
        (TextView) controlView.findViewById(R.id.encoder_stat_call);
    roomIdView =
        (TextView) controlView.findViewById(R.id.contact_name_call);
    hudView =
        (TextView) controlView.findViewById(R.id.hud_stat_call);
    disconnectButton =
        (ImageButton) controlView.findViewById(R.id.button_call_disconnect);
    cameraSwitchButton =
        (ImageButton) controlView.findViewById(R.id.button_call_switch_camera);
    videoScalingButton =
        (ImageButton) controlView.findViewById(R.id.button_call_scaling_mode);
    toggleDebugButton =
        (ImageButton) controlView.findViewById(R.id.button_toggle_debug);

    // Add buttons click events.
    disconnectButton.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View view) {
        callEvents.onCallHangUp();
      }
    });

    cameraSwitchButton.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View view) {
        callEvents.onCameraSwitch();
      }
    });

    videoScalingButton.setOnClickListener(new View.OnClickListener() {
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
        callEvents.onVideoScalingSwitch(scalingType);
      }
    });
    scalingType = ScalingType.SCALE_ASPECT_FILL;

    toggleDebugButton.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View view) {
        if (displayHud) {
          int visibility = (hudView.getVisibility() == View.VISIBLE)
              ? View.INVISIBLE : View.VISIBLE;
          hudView.setVisibility(visibility);
        }
      }
    });

    return controlView;
  }

  @Override
  public void onStart() {
    super.onStart();

    Bundle args = getArguments();
    if (args != null) {
      String roomId = args.getString(CallActivity.EXTRA_ROOMID);
      roomIdView.setText(roomId);
      displayHud = args.getBoolean(CallActivity.EXTRA_DISPLAY_HUD, false);
    }
    int visibility = displayHud ? View.VISIBLE : View.INVISIBLE;
    encoderStatView.setVisibility(visibility);
    toggleDebugButton.setVisibility(visibility);
    hudView.setVisibility(View.INVISIBLE);
    hudView.setTextSize(TypedValue.COMPLEX_UNIT_PT, 5);
    isRunning = true;
  }

  @Override
  public void onStop() {
    isRunning = false;
    super.onStop();
  }

  @Override
  public void onAttach(Activity activity) {
    super.onAttach(activity);
    callEvents = (OnCallEvents) activity;
  }

  private Map<String, String> getReportMap(StatsReport report) {
    Map<String, String> reportMap = new HashMap<String, String>();
    for (StatsReport.Value value : report.values) {
      reportMap.put(value.name, value.value);
    }
    return reportMap;
  }

  public void updateEncoderStatistics(final StatsReport[] reports) {
    if (!isRunning || !displayHud) {
      return;
    }
    String fps = null;
    String targetBitrate = null;
    String actualBitrate = null;
    StringBuilder bweBuilder = new StringBuilder();
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

        for (StatsReport.Value value : report.values) {
          String name = value.name.replace("goog", "")
              .replace("Available", "").replace("Bandwidth", "")
              .replace("Bitrate", "").replace("Enc", "");
          bweBuilder.append(name).append("=").append(value.value)
              .append(" ");
        }
        bweBuilder.append("\n");
      }
    }

    StringBuilder stat = new StringBuilder(128);
    if (fps != null) {
      stat.append("Fps:  ")
          .append(fps)
          .append("\n");
    }
    if (targetBitrate != null) {
      stat.append("Target BR: ")
          .append(targetBitrate)
          .append("\n");
    }
    if (actualBitrate != null) {
      stat.append("Actual BR: ")
          .append(actualBitrate)
          .append("\n");
    }

    if (cpuMonitor.sampleCpuUtilization()) {
      stat.append("CPU%: ")
          .append(cpuMonitor.getCpuCurrent())
          .append("/")
          .append(cpuMonitor.getCpuAvg3())
          .append("/")
          .append(cpuMonitor.getCpuAvgAll());
    }
    encoderStatView.setText(stat.toString());
    hudView.setText(bweBuilder.toString() + hudView.getText());
  }
}
