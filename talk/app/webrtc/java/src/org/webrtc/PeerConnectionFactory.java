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

  public void setOptions(Options options) {
    nativeSetOptions(nativeFactory, options);
  }

  public void setVideoHwAccelerationOptions(Object renderEGLContext) {
    nativeSetVideoHwAccelerationOptions(nativeFactory, renderEGLContext);
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

  public native void nativeSetOptions(long nativeFactory, Options options);

  private static native void nativeSetVideoHwAccelerationOptions(
      long nativeFactory, Object renderEGLContext);

  private static native void nativeThreadsCallbacks(long nativeFactory);

  private static native void nativeFreeFactory(long nativeFactory);
}
