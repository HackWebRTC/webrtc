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


package org.webrtc;

import java.util.List;

/**
 * Java wrapper for a C++ PeerConnectionFactoryInterface.  Main entry point to
 * the PeerConnection API for clients.
 */
public class PeerConnectionFactory {
  static {
    System.loadLibrary("jingle_peerconnection_so");
  }

  private static final String TAG = "PeerConnectionFactory";
  private final long nativeFactory;
  private static Thread workerThread;
  private static Thread signalingThread;

  public static class Options {
    // Keep in sync with webrtc/base/network.h!
    static final int ADAPTER_TYPE_UNKNOWN = 0;
    static final int ADAPTER_TYPE_ETHERNET = 1 << 0;
    static final int ADAPTER_TYPE_WIFI = 1 << 1;
    static final int ADAPTER_TYPE_CELLULAR = 1 << 2;
    static final int ADAPTER_TYPE_VPN = 1 << 3;
    static final int ADAPTER_TYPE_LOOPBACK = 1 << 4;

    public int networkIgnoreMask;
    public boolean disableEncryption;
    public boolean disableNetworkMonitor;
  }

  // |context| is an android.content.Context object, but we keep it untyped here
  // to allow building on non-Android platforms.
  // Callers may specify either |initializeAudio| or |initializeVideo| as false
  // to skip initializing the respective engine (and avoid the need for the
  // respective permissions).
  // |renderEGLContext| can be provided to suport HW video decoding to
  // texture and will be used to create a shared EGL context on video
  // decoding thread.
  public static native boolean initializeAndroidGlobals(
      Object context, boolean initializeAudio, boolean initializeVideo,
      boolean videoHwAcceleration);

  // Field trial initialization. Must be called before PeerConnectionFactory
  // is created.
  public static native void initializeFieldTrials(String fieldTrialsInitString);
  // Internal tracing initialization. Must be called before PeerConnectionFactory is created to
  // prevent racing with tracing code.
  public static native void initializeInternalTracer();
  // Internal tracing shutdown, called to prevent resource leaks. Must be called after
  // PeerConnectionFactory is gone to prevent races with code performing tracing.
  public static native void shutdownInternalTracer();
  // Start/stop internal capturing of internal tracing.
  public static native boolean startInternalTracingCapture(String tracing_filename);
  public static native void stopInternalTracingCapture();

  public PeerConnectionFactory() {
    nativeFactory = nativeCreatePeerConnectionFactory();
    if (nativeFactory == 0) {
      throw new RuntimeException("Failed to initialize PeerConnectionFactory!");
    }
  }

  public PeerConnection createPeerConnection(
      PeerConnection.RTCConfiguration rtcConfig,
      MediaConstraints constraints,
      PeerConnection.Observer observer) {
    long nativeObserver = nativeCreateObserver(observer);
    if (nativeObserver == 0) {
      return null;
    }
    long nativePeerConnection = nativeCreatePeerConnection(
        nativeFactory, rtcConfig, constraints, nativeObserver);
    if (nativePeerConnection == 0) {
      return null;
    }
    return new PeerConnection(nativePeerConnection, nativeObserver);
  }

  public PeerConnection createPeerConnection(
      List<PeerConnection.IceServer> iceServers,
      MediaConstraints constraints,
      PeerConnection.Observer observer) {
    PeerConnection.RTCConfiguration rtcConfig =
        new PeerConnection.RTCConfiguration(iceServers);
    return createPeerConnection(rtcConfig, constraints, observer);
  }

  public MediaStream createLocalMediaStream(String label) {
    return new MediaStream(
        nativeCreateLocalMediaStream(nativeFactory, label));
  }

  public VideoSource createVideoSource(
      VideoCapturer capturer, MediaConstraints constraints) {
    return new VideoSource(nativeCreateVideoSource(
        nativeFactory, capturer.takeNativeVideoCapturer(), constraints));
  }

  public VideoTrack createVideoTrack(String id, VideoSource source) {
    return new VideoTrack(nativeCreateVideoTrack(
        nativeFactory, id, source.nativeSource));
  }

  public AudioSource createAudioSource(MediaConstraints constraints) {
    return new AudioSource(nativeCreateAudioSource(nativeFactory, constraints));
  }

  public AudioTrack createAudioTrack(String id, AudioSource source) {
    return new AudioTrack(nativeCreateAudioTrack(
        nativeFactory, id, source.nativeSource));
  }

  // Starts recording an AEC dump. Ownership of the file is transfered to the
  // native code. If an AEC dump is already in progress, it will be stopped and
  // a new one will start using the provided file.
  public boolean startAecDump(int file_descriptor) {
    return nativeStartAecDump(nativeFactory, file_descriptor);
  }

  // Stops recording an AEC dump. If no AEC dump is currently being recorded,
  // this call will have no effect.
  public void stopAecDump() {
    nativeStopAecDump(nativeFactory);
  }

  // Starts recording an RTC event log. Ownership of the file is transfered to
  // the native code. If an RTC event log is already being recorded, it will be
  // stopped and a new one will start using the provided file.
  public boolean startRtcEventLog(int file_descriptor) {
    return nativeStartRtcEventLog(nativeFactory, file_descriptor);
  }

  // Stops recording an RTC event log. If no RTC event log is currently being
  // recorded, this call will have no effect.
  public void StopRtcEventLog() {
    nativeStopRtcEventLog(nativeFactory);
  }

  public void setOptions(Options options) {
    nativeSetOptions(nativeFactory, options);
  }

  @Deprecated
  public void setVideoHwAccelerationOptions(Object renderEGLContext) {
    nativeSetVideoHwAccelerationOptions(nativeFactory, renderEGLContext, renderEGLContext);
  }

  /** Set the EGL context used by HW Video encoding and decoding.
   *
   *
   * @param localEGLContext   An instance of javax.microedition.khronos.egl.EGLContext.
   *                          Must be the same as used by VideoCapturerAndroid and any local
   *                          video renderer.
   * @param remoteEGLContext  An instance of javax.microedition.khronos.egl.EGLContext.
   *                          Must be the same as used by any remote video renderer.
   */
  public void setVideoHwAccelerationOptions(Object localEGLContext, Object remoteEGLContext) {
    nativeSetVideoHwAccelerationOptions(nativeFactory, localEGLContext, remoteEGLContext);
  }

  public void dispose() {
    nativeFreeFactory(nativeFactory);
    signalingThread = null;
    workerThread = null;
  }

  public void threadsCallbacks() {
    nativeThreadsCallbacks(nativeFactory);
  }

  private static void printStackTrace(Thread thread, String threadName) {
    if (thread != null) {
      StackTraceElement[] stackTraces = thread.getStackTrace();
      if (stackTraces.length > 0) {
        Logging.d(TAG, threadName + " stacks trace:");
        for (StackTraceElement stackTrace : stackTraces) {
          Logging.d(TAG, stackTrace.toString());
        }
      }
    }
  }

  public static void printStackTraces() {
    printStackTrace(workerThread, "Worker thread");
    printStackTrace(signalingThread, "Signaling thread");
  }

  private static void onWorkerThreadReady() {
    workerThread = Thread.currentThread();
    Logging.d(TAG, "onWorkerThreadReady");
  }

  private static void onSignalingThreadReady() {
    signalingThread = Thread.currentThread();
    Logging.d(TAG, "onSignalingThreadReady");
  }

  private static native long nativeCreatePeerConnectionFactory();

  private static native long nativeCreateObserver(
      PeerConnection.Observer observer);

  private static native long nativeCreatePeerConnection(
      long nativeFactory, PeerConnection.RTCConfiguration rtcConfig,
      MediaConstraints constraints, long nativeObserver);

  private static native long nativeCreateLocalMediaStream(
      long nativeFactory, String label);

  private static native long nativeCreateVideoSource(
      long nativeFactory, long nativeVideoCapturer,
      MediaConstraints constraints);

  private static native long nativeCreateVideoTrack(
      long nativeFactory, String id, long nativeVideoSource);

  private static native long nativeCreateAudioSource(
      long nativeFactory, MediaConstraints constraints);

  private static native long nativeCreateAudioTrack(
      long nativeFactory, String id, long nativeSource);

  private static native boolean nativeStartAecDump(long nativeFactory, int file_descriptor);

  private static native void nativeStopAecDump(long nativeFactory);

  private static native boolean nativeStartRtcEventLog(long nativeFactory, int file_descriptor);

  private static native void nativeStopRtcEventLog(long nativeFactory);

  public native void nativeSetOptions(long nativeFactory, Options options);

  private static native void nativeSetVideoHwAccelerationOptions(
      long nativeFactory, Object localEGLContext, Object remoteEGLContext);

  private static native void nativeThreadsCallbacks(long nativeFactory);

  private static native void nativeFreeFactory(long nativeFactory);
}
