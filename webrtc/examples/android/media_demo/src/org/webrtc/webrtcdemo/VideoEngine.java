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

public class VideoEngine {
  private final long nativeVideoEngine;

  // Keep in sync (including this comment) with webrtc/common_types.h:TraceLevel
  public enum TraceLevel {
    TRACE_NONE(0x0000),
    TRACE_STATE_INFO(0x0001),
    TRACE_WARNING(0x0002),
    TRACE_ERROR(0x0004),
    TRACE_CRITICAL(0x0008),
    TRACE_API_CALL(0x0010),
    TRACE_DEFAULT(0x00ff),
    TRACE_MODULE_CALL(0x0020),
    TRACE_MEMORY(0x0100),
    TRACE_TIMER(0x0200),
    TRACE_STREAM(0x0400),
    TRACE_DEBUG(0x0800),
    TRACE_INFO(0x1000),
    TRACE_TERSE_INFO(0x2000),
    TRACE_ALL(0xfff);

    public final int level;
    TraceLevel(int level) {
      this.level = level;
    }
  };

  // Keep in sync (including this comment) with
  // webrtc/video_engine/include/vie_rtp_rtcp.h:ViEKeyFrameRequestMethod
  public enum VieKeyFrameRequestMethod {
    KEY_FRAME_REQUEST_NONE, KEY_FRAME_REQUEST_PLI_RTCP,
    KEY_FRAME_REQUEST_FIR_RTP, KEY_FRAME_REQUEST_FIR_RTCP
  }

  // Keep in sync (including this comment) with
  // webrtc/common_types.h:RtpDirections
  public enum RtpDirections { INCOMING, OUTGOING }

  public VideoEngine() {
    nativeVideoEngine = create();
  }

  // API comments can be found in VideoEngine's native APIs. Not all native
  // APIs are available.
  private static native long create();
  public native int init();
  public native int setVoiceEngine(VoiceEngine voe);
  public native void dispose();
  public native int startSend(int channel);
  public native int stopRender(int channel);
  public native int stopSend(int channel);
  public native int startReceive(int channel);
  public native int stopReceive(int channel);
  public native int createChannel();
  public native int deleteChannel(int channel);
  public native int connectAudioChannel(int videoChannel, int voiceChannel);
  public native int setLocalReceiver(int channel, int port);
  public native int setSendDestination(int channel, int port, String ipAddr);
  public native int numberOfCodecs();
  public native VideoCodecInst getCodec(int index);
  public native int setReceiveCodec(int channel, VideoCodecInst codec);
  public native int setSendCodec(int channel, VideoCodecInst codec);
  public native int addRenderer(int channel, Object glSurface, int zOrder,
      float left, float top,
      float right, float bottom);
  public native int removeRenderer(int channel);
  public native int registerExternalReceiveCodec(int channel, int plType,
      MediaCodecVideoDecoder decoder, boolean internal_source);
  public native int deRegisterExternalReceiveCodec(int channel, int plType);
  public native int startRender(int channel);
  public native int numberOfCaptureDevices();
  public native CameraDesc getCaptureDevice(int index);
  public native int allocateCaptureDevice(CameraDesc camera);
  public native int connectCaptureDevice(int cameraId, int channel);
  public native int startCapture(int cameraId);
  public native int stopCapture(int cameraId);
  public native int releaseCaptureDevice(int cameraId);
  public native int getOrientation(CameraDesc camera);
  public native int setRotateCapturedFrames(int cameraId, int degrees);
  public native int setNackStatus(int channel, boolean enable);
  public int setKeyFrameRequestMethod(int channel,
      VieKeyFrameRequestMethod requestMethod) {
    return setKeyFrameRequestMethod(channel, requestMethod.ordinal());
  }
  private native int setKeyFrameRequestMethod(int channel,
      int requestMethod);
  public native RtcpStatistics getReceivedRtcpStatistics(int channel);
  public native int registerObserver(int channel,
      VideoDecodeEncodeObserver callback);
  public native int deregisterObserver(int channel);
  public native int setTraceFile(String fileName,
      boolean fileCounter);
  public int setTraceFilter(TraceLevel filter) {
    return filter.level;
  }
  private native int setTraceFilter(int filter);
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
