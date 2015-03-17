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

import android.app.Fragment;
import android.os.Bundle;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageButton;
import android.widget.TextView;

import org.webrtc.StatsReport;

import java.util.HashMap;
import java.util.Map;

/**
 * Fragment for HUD statistics display.
 */
public class HudFragment extends Fragment {
  private View controlView;
  private TextView encoderStatView;
  private TextView hudViewBwe;
  private TextView hudViewConnection;
  private TextView hudViewVideoSend;
  private TextView hudViewVideoRecv;
  private ImageButton toggleDebugButton;
  private boolean videoCallEnabled;
  private boolean displayHud;
  private volatile boolean isRunning;
  private final CpuMonitor cpuMonitor = new CpuMonitor();

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container,
      Bundle savedInstanceState) {
    controlView = inflater.inflate(R.layout.fragment_hud, container, false);

    // Create UI controls.
    encoderStatView = (TextView) controlView.findViewById(R.id.encoder_stat_call);
    hudViewBwe = (TextView) controlView.findViewById(R.id.hud_stat_bwe);
    hudViewConnection = (TextView) controlView.findViewById(R.id.hud_stat_connection);
    hudViewVideoSend = (TextView) controlView.findViewById(R.id.hud_stat_video_send);
    hudViewVideoRecv = (TextView) controlView.findViewById(R.id.hud_stat_video_recv);
    toggleDebugButton = (ImageButton) controlView.findViewById(R.id.button_toggle_debug);

    toggleDebugButton.setOnClickListener(new View.OnClickListener() {
      @Override
      public void onClick(View view) {
        if (displayHud) {
          int visibility = (hudViewBwe.getVisibility() == View.VISIBLE)
              ? View.INVISIBLE : View.VISIBLE;
          hudViewsSetProperties(visibility);
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
      videoCallEnabled = args.getBoolean(CallActivity.EXTRA_VIDEO_CALL, true);
      displayHud = args.getBoolean(CallActivity.EXTRA_DISPLAY_HUD, false);
    }
    int visibility = displayHud ? View.VISIBLE : View.INVISIBLE;
    encoderStatView.setVisibility(visibility);
    toggleDebugButton.setVisibility(visibility);
    hudViewsSetProperties(View.INVISIBLE);
    isRunning = true;
  }

  @Override
  public void onStop() {
    isRunning = false;
    super.onStop();
  }

  private void hudViewsSetProperties(int visibility) {
    hudViewBwe.setVisibility(visibility);
    hudViewConnection.setVisibility(visibility);
    hudViewVideoSend.setVisibility(visibility);
    hudViewVideoRecv.setVisibility(visibility);
    hudViewBwe.setTextSize(TypedValue.COMPLEX_UNIT_PT, 5);
    hudViewConnection.setTextSize(TypedValue.COMPLEX_UNIT_PT, 5);
    hudViewVideoSend.setTextSize(TypedValue.COMPLEX_UNIT_PT, 5);
    hudViewVideoRecv.setTextSize(TypedValue.COMPLEX_UNIT_PT, 5);
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
    StringBuilder encoderStat = new StringBuilder(128);
    StringBuilder bweStat = new StringBuilder();
    StringBuilder connectionStat = new StringBuilder();
    StringBuilder videoSendStat = new StringBuilder();
    StringBuilder videoRecvStat = new StringBuilder();
    String fps = null;
    String targetBitrate = null;
    String actualBitrate = null;

    for (StatsReport report : reports) {
      if (report.type.equals("ssrc") && report.id.contains("ssrc")
          && report.id.contains("send")) {
        // Send video statistics.
        Map<String, String> reportMap = getReportMap(report);
        String trackId = reportMap.get("googTrackId");
        if (trackId != null && trackId.contains(PeerConnectionClient.VIDEO_TRACK_ID)) {
          fps = reportMap.get("googFrameRateSent");
          videoSendStat.append(report.id).append("\n");
          for (StatsReport.Value value : report.values) {
            String name = value.name.replace("goog", "");
            videoSendStat.append(name).append("=").append(value.value).append("\n");
          }
        }
      } else if (report.type.equals("ssrc") && report.id.contains("ssrc")
          && report.id.contains("recv")) {
        // Receive video statistics.
        Map<String, String> reportMap = getReportMap(report);
        // Check if this stat is for video track.
        String frameWidth = reportMap.get("googFrameWidthReceived");
        if (frameWidth != null) {
          videoRecvStat.append(report.id).append("\n");
          for (StatsReport.Value value : report.values) {
            String name = value.name.replace("goog", "");
            videoRecvStat.append(name).append("=").append(value.value).append("\n");
          }
        }
      } else if (report.id.equals("bweforvideo")) {
        // BWE statistics.
        Map<String, String> reportMap = getReportMap(report);
        targetBitrate = reportMap.get("googTargetEncBitrate");
        actualBitrate = reportMap.get("googActualEncBitrate");

        bweStat.append(report.id).append("\n");
        for (StatsReport.Value value : report.values) {
          String name = value.name.replace("goog", "").replace("Available", "");
          bweStat.append(name).append("=").append(value.value).append("\n");
        }
      } else if (report.type.equals("googCandidatePair")) {
        // Connection statistics.
        Map<String, String> reportMap = getReportMap(report);
        String activeConnection = reportMap.get("googActiveConnection");
        if (activeConnection != null && activeConnection.equals("true")) {
          connectionStat.append(report.id).append("\n");
          for (StatsReport.Value value : report.values) {
            String name = value.name.replace("goog", "");
            connectionStat.append(name).append("=").append(value.value).append("\n");
          }
        }
      }
    }
    hudViewBwe.setText(bweStat.toString());
    hudViewConnection.setText(connectionStat.toString());
    hudViewVideoSend.setText(videoSendStat.toString());
    hudViewVideoRecv.setText(videoRecvStat.toString());

    if (videoCallEnabled) {
      if (fps != null) {
        encoderStat.append("Fps:  ").append(fps).append("\n");
      }
      if (targetBitrate != null) {
        encoderStat.append("Target BR: ").append(targetBitrate).append("\n");
      }
      if (actualBitrate != null) {
        encoderStat.append("Actual BR: ").append(actualBitrate).append("\n");
      }
    }

    if (cpuMonitor.sampleCpuUtilization()) {
      encoderStat.append("CPU%: ")
          .append(cpuMonitor.getCpuCurrent()).append("/")
          .append(cpuMonitor.getCpuAvg3()).append("/")
          .append(cpuMonitor.getCpuAvgAll());
    }
    encoderStatView.setText(encoderStat.toString());
  }
}
