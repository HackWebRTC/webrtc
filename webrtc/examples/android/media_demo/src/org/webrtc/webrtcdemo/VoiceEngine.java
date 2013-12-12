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

public class VoiceEngine {
  private final long nativeVoiceEngine;

  // Keep in sync (including this comment) with
  // webrtc/common_types.h:NsModes
  public enum NsModes {
    UNCHANGED, DEFAULT, CONFERENCE, LOW_SUPPRESSION,
    MODERATE_SUPPRESSION, HIGH_SUPPRESSION, VERY_HIGH_SUPPRESSION
  }

  // Keep in sync (including this comment) with
  // webrtc/common_types.h:AgcModes
  public enum AgcModes {
    UNCHANGED, DEFAULT, ADAPTIVE_ANALOG, ADAPTIVE_DIGITAL,
    FIXED_DIGITAL
  }

  // Keep in sync (including this comment) with
  // webrtc/common_types.h:AecmModes
  public enum AecmModes {
    QUIET_EARPIECE_OR_HEADSET, EARPIECE, LOUD_EARPIECE,
    SPEAKERPHONE, LOUD_SPEAKERPHONE
  }

  // Keep in sync (including this comment) with
  // webrtc/common_types.h:EcModes
  public enum EcModes { UNCHANGED, DEFAULT, CONFERENCE, AEC, AECM }

  // Keep in sync (including this comment) with
  // webrtc/common_types.h:RtpDirections
  public enum RtpDirections { INCOMING, OUTGOING }

  public static class AgcConfig {
    AgcConfig(int targetLevelDbOv, int digitalCompressionGaindB,
        boolean limiterEnable) {
      this.targetLevelDbOv = targetLevelDbOv;
      this.digitalCompressionGaindB = digitalCompressionGaindB;
      this.limiterEnable = limiterEnable;
    }
    private final int targetLevelDbOv;
    private final int digitalCompressionGaindB;
    private final boolean limiterEnable;
  }

  public VoiceEngine() {
    nativeVoiceEngine = create();
  }
  private static native long create();
  public native int init();
  public native void dispose();
  public native int createChannel();
  public native int deleteChannel(int channel);
  public native int setLocalReceiver(int channel, int port);
  public native int setSendDestination(int channel, int port, String ipaddr);
  public native int startListen(int channel);
  public native int startPlayout(int channel);
  public native int startSend(int channel);
  public native int stopListen(int channel);
  public native int stopPlayout(int channel);
  public native int stopSend(int channel);
  public native int setSpeakerVolume(int volume);
  public native int setLoudspeakerStatus(boolean enable);
  public native int startPlayingFileLocally(
      int channel,
      String fileName,
      boolean loop);
  public native int stopPlayingFileLocally(int channel);
  public native int startPlayingFileAsMicrophone(
      int channel,
      String fileName,
      boolean loop);
  public native int stopPlayingFileAsMicrophone(int channel);
  public native int numOfCodecs();
  public native CodecInst getCodec(int index);
  public native int setSendCodec(int channel, CodecInst codec);
  public int setEcStatus(boolean enable, EcModes mode) {
    return setEcStatus(enable, mode.ordinal());
  }
  private native int setEcStatus(boolean enable, int ec_mode);
  public int setAecmMode(AecmModes aecm_mode, boolean cng) {
    return setAecmMode(aecm_mode.ordinal(), cng);
  }
  private native int setAecmMode(int aecm_mode, boolean cng);
  public int setAgcStatus(boolean enable, AgcModes agc_mode) {
    return setAgcStatus(enable, agc_mode.ordinal());
  }
  private native int setAgcStatus(boolean enable, int agc_mode);
  public native int setAgcConfig(AgcConfig agc_config);
  public int setNsStatus(boolean enable, NsModes ns_mode) {
    return setNsStatus(enable, ns_mode.ordinal());
  }
  private native int setNsStatus(boolean enable, int ns_mode);
  public native int startDebugRecording(String file);
  public native int stopDebugRecording();
  public int startRtpDump(int channel, String file,
      RtpDirections direction) {
    return startRtpDump(channel, file, direction.ordinal());
  }
  private native int startRtpDump(int channel, String file,
      int direction);
  public int stopRtpDump(int channel, RtpDirections direction) {
    return stopRtpDump(channel, direction.ordinal());
  }
  private native int stopRtpDump(int channel, int direction);
}