/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

/** Java wrapper for a C++ MediaStreamTrackInterface. */
@JNINamespace("webrtc::jni")
public class MediaStreamTrack {
  /** Tracks MediaStreamTrackInterface.TrackState */
  public enum State {
    LIVE,
    ENDED;

    @CalledByNative("State")
    static State fromNativeIndex(int nativeIndex) {
      return values()[nativeIndex];
    }
  }

  // Must be kept in sync with cricket::MediaType.
  public enum MediaType {
    MEDIA_TYPE_AUDIO(0),
    MEDIA_TYPE_VIDEO(1);

    private final int nativeIndex;

    private MediaType(int nativeIndex) {
      this.nativeIndex = nativeIndex;
    }

    @CalledByNative("MediaType")
    int getNative() {
      return nativeIndex;
    }

    @CalledByNative("MediaType")
    static MediaType fromNativeIndex(int nativeIndex) {
      for (MediaType type : MediaType.values()) {
        if (type.getNative() == nativeIndex) {
          return type;
        }
      }
      throw new IllegalArgumentException("Unknown native media type: " + nativeIndex);
    }
  }

  final long nativeTrack;

  public MediaStreamTrack(long nativeTrack) {
    this.nativeTrack = nativeTrack;
  }

  public String id() {
    return nativeGetId(nativeTrack);
  }

  public String kind() {
    return nativeGetKind(nativeTrack);
  }

  public boolean enabled() {
    return nativeGetEnabled(nativeTrack);
  }

  public boolean setEnabled(boolean enable) {
    return nativeSetEnabled(nativeTrack, enable);
  }

  public State state() {
    return nativeGetState(nativeTrack);
  }

  public void dispose() {
    JniCommon.nativeReleaseRef(nativeTrack);
  }

  private static native String nativeGetId(long track);
  private static native String nativeGetKind(long track);
  private static native boolean nativeGetEnabled(long track);
  private static native boolean nativeSetEnabled(long track, boolean enabled);
  private static native State nativeGetState(long track);
}
