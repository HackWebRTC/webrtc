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

  private final long nativeFactory;

  // |context| is an android.content.Context object, but we keep it untyped here
  // to allow building on non-Android platforms.
  public static native boolean initializeAndroidGlobals(Object context);

  public PeerConnectionFactory() {
    nativeFactory = nativeCreatePeerConnectionFactory();
    if (nativeFactory == 0) {
      throw new RuntimeException("Failed to initialize PeerConnectionFactory!");
    }
  }


  public PeerConnection createPeerConnection(
      List<PeerConnection.IceServer> iceServers,
      MediaConstraints constraints,
      PeerConnection.Observer observer) {
    long nativeObserver = nativeCreateObserver(observer);
    if (nativeObserver == 0) {
      return null;
    }
    long nativePeerConnection = nativeCreatePeerConnection(
        nativeFactory, iceServers, constraints, nativeObserver);
    if (nativePeerConnection == 0) {
      return null;
    }
    return new PeerConnection(nativePeerConnection, nativeObserver);
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

  public void dispose() {
    freeFactory(nativeFactory);
  }

  private static native long nativeCreatePeerConnectionFactory();

  private static native long nativeCreateObserver(
      PeerConnection.Observer observer);

  private static native long nativeCreatePeerConnection(
      long nativeFactory, List<PeerConnection.IceServer> iceServers,
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

  private static native void freeFactory(long nativeFactory);
}
