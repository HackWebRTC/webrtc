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

import org.webrtc.videoengine.ViERenderer;

import android.app.AlertDialog;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.Camera;
import android.hardware.Camera.CameraInfo;
import android.hardware.SensorManager;
import android.os.Environment;
import android.util.Log;
import android.view.OrientationEventListener;
import android.view.SurfaceView;
import java.io.File;

public class MediaEngine implements VideoDecodeEncodeObserver {
  // TODO(henrike): Most of these should be moved to xml (since static).
  private static final int VCM_VP8_PAYLOAD_TYPE = 100;
  private static final int SEND_CODEC_FPS = 30;
  // TODO(henrike): increase INIT_BITRATE_KBPS to 2000 and ensure that
  // 720p30fps can be acheived (on hardware that can handle it). Note that
  // setting 2000 currently leads to failure, so that has to be resolved first.
  private static final int INIT_BITRATE_KBPS = 500;
  private static final int MAX_BITRATE_KBPS = 3000;
  private static final String LOG_DIR = "webrtc";
  private static final int WIDTH_IDX = 0;
  private static final int HEIGHT_IDX = 1;
  private static final int[][] RESOLUTIONS = {
    {176,144}, {320,240}, {352,288}, {640,480}, {1280,720}
  };
  // Arbitrary choice of 4/5 volume (204/256).
  private static final int volumeLevel = 204;

  public static int numberOfResolutions() { return RESOLUTIONS.length; }

  public static String[] resolutionsAsString() {
    String[] retVal = new String[numberOfResolutions()];
    for (int i = 0; i < numberOfResolutions(); ++i) {
      retVal[i] = RESOLUTIONS[i][0] + "x" + RESOLUTIONS[i][1];
    }
    return retVal;
  }

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

  // Converts device rotation to camera rotation. Rotation depends on if the
  // camera is back facing and rotate with the device or front facing and
  // rotating in the opposite direction of the device.
  private static int rotationFromRealWorldUp(CameraInfo info,
                                             int deviceRotation) {
    int coarseDeviceOrientation =
        (int)(Math.round((double)deviceRotation / 90) * 90) % 360;
    if (info.facing == CameraInfo.CAMERA_FACING_FRONT) {
      // The front camera rotates in the opposite direction of the
      // device.
      int inverseDeviceOrientation = 360 - coarseDeviceOrientation;
      return (inverseDeviceOrientation + info.orientation) % 360;
    }
    return (coarseDeviceOrientation + info.orientation) % 360;
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

  // Video
  private VideoEngine vie;
  private int videoChannel;
  private boolean receiveVideo;
  private boolean sendVideo;
  private boolean vieRunning;
  private int videoCodecIndex;
  private int resolutionIndex;
  private int videoTxPort;
  private int videoRxPort;

  // Indexed by CameraInfo.CAMERA_FACING_{BACK,FRONT}.
  private CameraInfo cameras[];
  private boolean useFrontCamera;
  private int currentCameraHandle;
  private boolean enableNack;
  // openGl, surfaceView or mediaCodec (integers.xml)
  private int viewSelection;
  private boolean videoRtpDump;

  private SurfaceView svLocal;
  private SurfaceView svRemote;
  MediaCodecVideoDecoder externalCodec;

  private int inFps;
  private int inKbps;
  private int outFps;
  private int outKbps;
  private int inWidth;
  private int inHeight;

  private OrientationEventListener orientationListener;
  private int deviceOrientation = OrientationEventListener.ORIENTATION_UNKNOWN;

  public MediaEngine(Context context) {
    this.context = context;
    voe = new VoiceEngine();
    check(voe.init() == 0, "Failed voe Init");
    audioChannel = voe.createChannel();
    check(audioChannel >= 0, "Failed voe CreateChannel");
    vie = new VideoEngine();
    check(vie.init() == 0, "Failed voe Init");
    check(vie.setVoiceEngine(voe) == 0, "Failed setVoiceEngine");
    videoChannel = vie.createChannel();
    check(audioChannel >= 0, "Failed voe CreateChannel");
    check(vie.connectAudioChannel(videoChannel, audioChannel) == 0,
        "Failed ConnectAudioChannel");

    cameras = new CameraInfo[2];
    CameraInfo info = new CameraInfo();
    for (int i = 0; i < Camera.getNumberOfCameras(); ++i) {
      Camera.getCameraInfo(i, info);
      cameras[info.facing] = info;
    }
    setDefaultCamera();
    check(voe.setSpeakerVolume(volumeLevel) == 0,
        "Failed setSpeakerVolume");
    check(voe.setAecmMode(VoiceEngine.AecmModes.SPEAKERPHONE, false) == 0,
        "VoE set Aecm speakerphone mode failed");
    check(vie.setKeyFrameRequestMethod(videoChannel,
            VideoEngine.VieKeyFrameRequestMethod.
            KEY_FRAME_REQUEST_PLI_RTCP) == 0,
        "Failed setKeyFrameRequestMethod");
    check(vie.registerObserver(videoChannel, this) == 0,
        "Failed registerObserver");

    // TODO(hellner): SENSOR_DELAY_NORMAL?
    // Listen to changes in device orientation.
    orientationListener =
        new OrientationEventListener(context, SensorManager.SENSOR_DELAY_UI) {
          public void onOrientationChanged (int orientation) {
            deviceOrientation = orientation;
            compensateRotation();
          }
        };
    orientationListener.enable();
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
    orientationListener.disable();
    check(vie.deregisterObserver(videoChannel) == 0,
        "Failed deregisterObserver");
    if (externalCodec != null) {
      check(vie.deRegisterExternalReceiveCodec(videoChannel,
              VCM_VP8_PAYLOAD_TYPE) == 0,
          "Failed to deregister external decoder");
      externalCodec = null;
    }
    check(vie.deleteChannel(videoChannel) == 0, "DeleteChannel");
    vie.dispose();
    check(voe.deleteChannel(audioChannel) == 0, "VoE delete channel failed");
    voe.dispose();
  }

  public void start() {
    if (audioEnabled) {
      startVoE();
    }
    if (receiveVideo || sendVideo) {
      startViE();
    }
  }

  public void stop() {
    stopVoe();
    stopVie();
  }

  public boolean isRunning() {
    return voeRunning || vieRunning;
  }

  public void setRemoteIp(String remoteIp) { this.remoteIp = remoteIp; }

  public String remoteIp() { return remoteIp; }

  public void setTrace(boolean enable) {
    if (enable) {
      vie.setTraceFile("/sdcard/trace.txt", false);
      vie.setTraceFilter(VideoEngine.TraceLevel.TRACE_ERROR);
      return;
    }
    vie.setTraceFilter(VideoEngine.TraceLevel.TRACE_NONE);
  }

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
    check(remoteIp != null,
        "remoteIP must have been set before setting audio send port");
    check(voe.setSendDestination(audioChannel, audioTxPort,
            remoteIp) == 0, "VoE set send destination failed");
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
      check(voe.stopRtpDump(videoChannel,
              VoiceEngine.RtpDirections.INCOMING) == 0,
          "voe stopping rtp dump");
      return;
    }
    String debugDirectory = getDebugDirectory();
    check(voe.startRtpDump(videoChannel, debugDirectory +
            String.format("/voe_%d.rtp", System.currentTimeMillis()),
            VoiceEngine.RtpDirections.INCOMING) == 0,
        "voe starting rtp dump");
  }

  private void updateAudioOutput() {
    boolean useSpeaker = !headsetPluggedIn && speakerEnabled;
    check(voe.setLoudspeakerStatus(useSpeaker) == 0,
        "Failed updating loudspeaker");
  }

  public void startViE() {
    check(!vieRunning, "ViE already started");

    if (receiveVideo) {
      if (viewSelection ==
          context.getResources().getInteger(R.integer.openGl)) {
        svRemote = ViERenderer.CreateRenderer(context, true);
      } else if (viewSelection ==
          context.getResources().getInteger(R.integer.surfaceView)) {
        svRemote = ViERenderer.CreateRenderer(context, false);
      } else {
        externalCodec = new MediaCodecVideoDecoder(context);
        svRemote = externalCodec.getView();
      }
      if (externalCodec != null) {
        check(vie.registerExternalReceiveCodec(videoChannel,
                VCM_VP8_PAYLOAD_TYPE, externalCodec, true) == 0,
            "Failed to register external decoder");
      } else {
        check(vie.addRenderer(videoChannel, svRemote,
                0, 0, 0, 1, 1) == 0, "Failed AddRenderer");
        check(vie.startRender(videoChannel) == 0, "Failed StartRender");
      }
      check(vie.startReceive(videoChannel) == 0, "Failed StartReceive");
    }
    if (sendVideo) {
      startCamera();
      check(vie.startSend(videoChannel) == 0, "Failed StartSend");
    }
    vieRunning = true;
  }

  private void stopVie() {
    if (!vieRunning) {
      return;
    }
    check(vie.stopSend(videoChannel) == 0, "StopSend");
    stopCamera();
    check(vie.stopReceive(videoChannel) == 0, "StopReceive");
    if (externalCodec != null) {
      check(vie.deRegisterExternalReceiveCodec(videoChannel,
              VCM_VP8_PAYLOAD_TYPE) == 0,
              "Failed to deregister external decoder");
      externalCodec.dispose();
      externalCodec = null;
    } else {
      check(vie.stopRender(videoChannel) == 0, "StopRender");
      check(vie.removeRenderer(videoChannel) == 0, "RemoveRenderer");
    }
    svRemote = null;
    vieRunning = false;
  }

  public void setReceiveVideo(boolean receiveVideo) {
    this.receiveVideo = receiveVideo;
  }

  public boolean receiveVideo() { return receiveVideo; }

  public void setSendVideo(boolean sendVideo) { this.sendVideo = sendVideo; }

  public boolean sendVideo() { return sendVideo; }

  public int videoCodecIndex() { return videoCodecIndex; }

  public void setVideoCodec(int codecNumber) {
    videoCodecIndex = codecNumber;
    updateVideoCodec();
  }

  public String[] videoCodecsAsString() {
    String[] retVal = new String[vie.numberOfCodecs()];
    for (int i = 0; i < vie.numberOfCodecs(); ++i) {
      VideoCodecInst codec = vie.getCodec(i);
      retVal[i] = codec.toString();
      codec.dispose();
    }
    return retVal;
  }

  public int resolutionIndex() { return resolutionIndex; }

  public void setResolutionIndex(int resolution) {
    resolutionIndex = resolution;
    updateVideoCodec();
  }

  private void updateVideoCodec() {
    VideoCodecInst codec = getVideoCodec(videoCodecIndex, resolutionIndex);
    check(vie.setSendCodec(videoChannel, codec) == 0, "Failed setReceiveCodec");
    codec.dispose();
  }

  private VideoCodecInst getVideoCodec(int codecNumber, int resolution) {
    VideoCodecInst retVal = vie.getCodec(codecNumber);
    retVal.setStartBitRate(INIT_BITRATE_KBPS);
    retVal.setMaxBitRate(MAX_BITRATE_KBPS);
    retVal.setWidth(RESOLUTIONS[resolution][WIDTH_IDX]);
    retVal.setHeight(RESOLUTIONS[resolution][HEIGHT_IDX]);
    retVal.setMaxFrameRate(SEND_CODEC_FPS);
    return retVal;
  }

  public void setVideoRxPort(int videoRxPort) {
    this.videoRxPort = videoRxPort;
    check(vie.setLocalReceiver(videoChannel, videoRxPort) == 0,
        "Failed setLocalReceiver");
  }

  public int videoRxPort() { return videoRxPort; }

  public void setVideoTxPort(int videoTxPort) {
    this.videoTxPort = videoTxPort;
    check(remoteIp != null,
        "remoteIP must have been set before setting audio send port");
    check(vie.setSendDestination(videoChannel, videoTxPort, remoteIp) == 0,
        "Failed setSendDestination");
  }

  public int videoTxPort() {
    return videoTxPort;
  }

  public boolean hasMultipleCameras() {
    return cameras.length > 1;
  }

  public boolean frontCameraIsSet() {
    return useFrontCamera;
  }

  // Set camera to front if one exists otherwise use back camera.
  private void setDefaultCamera() {
    useFrontCamera = hasFrontCamera();
  }

  public void toggleCamera() {
    if (vieRunning) {
      stopCamera();
    }
    useFrontCamera = !useFrontCamera;
    if (vieRunning) {
      startCamera();
    }
  }

  private void startCamera() {
    CameraDesc cameraInfo = vie.getCaptureDevice(getCameraId());
    currentCameraHandle = vie.allocateCaptureDevice(cameraInfo);
    cameraInfo.dispose();
    check(vie.connectCaptureDevice(currentCameraHandle, videoChannel) == 0,
        "Failed to connect capture device");
    // Camera and preview surface. Note, renderer must be created before
    // calling StartCapture or |svLocal| won't be able to render.
    svLocal = ViERenderer.CreateLocalRenderer(context);
    check(vie.startCapture(currentCameraHandle) == 0, "Failed StartCapture");
    compensateRotation();
  }

  private void stopCamera() {
    check(vie.stopCapture(currentCameraHandle) == 0, "Failed StopCapture");
    svLocal = null;
    check(vie.releaseCaptureDevice(currentCameraHandle) == 0,
        "Failed ReleaseCaptureDevice");
  }

  private boolean hasFrontCamera() {
    return cameras[CameraInfo.CAMERA_FACING_FRONT] != null;
  }

  public SurfaceView getRemoteSurfaceView() {
    return svRemote;
  }

  public SurfaceView getLocalSurfaceView() {
    return svLocal;
  }

  public void setViewSelection(int viewSelection) {
    this.viewSelection = viewSelection;
  }

  public int viewSelection() { return viewSelection; }

  public boolean nackEnabled() { return enableNack; }

  public void setNack(boolean enable) {
    enableNack = enable;
    check(vie.setNackStatus(videoChannel, enableNack) == 0,
        "Failed setNackStatus");
  }

  // Collates current state into a multiline string.
  public String sendReceiveState() {
    int packetLoss = 0;
    if (vieRunning) {
      RtcpStatistics stats = vie.getReceivedRtcpStatistics(videoChannel);
      if (stats != null) {
        // Calculate % lost from fraction lost.
        // Definition of fraction lost can be found in RFC3550.
        packetLoss = (stats.fractionLost * 100) >> 8;
      }
    }
    String retVal =
        "fps in/out: " + inFps + "/" + outFps + "\n" +
        "kBps in/out: " + inKbps / 1024 + "/ " + outKbps / 1024 + "\n" +
        "resolution: " + inWidth + "x" + inHeight + "\n" +
        "loss: " + packetLoss + "%";
    return retVal;
  }

  MediaEngineObserver observer;
  public void setObserver(MediaEngineObserver observer) {
    this.observer = observer;
  }

  // Callbacks from the VideoDecodeEncodeObserver interface.
  public void incomingRate(int videoChannel, int framerate, int bitrate) {
    inFps = framerate;
    inKbps = bitrate;
    newStats();
  }

  public void incomingCodecChanged(int videoChannel,
      VideoCodecInst videoCodec) {
    inWidth = videoCodec.width();
    inHeight = videoCodec.height();
    videoCodec.dispose();
    newStats();
  }

  public void requestNewKeyFrame(int videoChannel) {}

  public void outgoingRate(int videoChannel, int framerate, int bitrate) {
    outFps = framerate;
    outKbps = bitrate;
    newStats();
  }

  private void newStats() {
    if (observer != null) {
      observer.newStats(sendReceiveState());
    }
  }

  // Debug helpers.
  public boolean videoRtpDump() { return videoRtpDump; }

  public void setIncomingVieRtpDump(boolean enable) {
    videoRtpDump = enable;
    if (!enable) {
      check(vie.stopRtpDump(videoChannel,
              VideoEngine.RtpDirections.INCOMING) == 0,
          "vie StopRTPDump");
      return;
    }
    String debugDirectory = getDebugDirectory();
    check(vie.startRtpDump(videoChannel, debugDirectory +
            String.format("/vie_%d.rtp", System.currentTimeMillis()),
            VideoEngine.RtpDirections.INCOMING) == 0,
        "vie StartRtpDump");
  }

  private int getCameraId() {
    return useFrontCamera ? Camera.CameraInfo.CAMERA_FACING_FRONT :
        Camera.CameraInfo.CAMERA_FACING_BACK;
  }

  private void compensateRotation() {
    if (svLocal == null) {
      // Not rendering (or sending).
      return;
    }
    if (deviceOrientation == OrientationEventListener.ORIENTATION_UNKNOWN) {
      return;
    }
    int cameraRotation = rotationFromRealWorldUp(
        cameras[getCameraId()], deviceOrientation);
    // Egress streams should have real world up as up.
    check(
        vie.setRotateCapturedFrames(currentCameraHandle, cameraRotation) == 0,
        "Failed setRotateCapturedFrames: camera " + currentCameraHandle +
        "rotation " + cameraRotation);
  }
}