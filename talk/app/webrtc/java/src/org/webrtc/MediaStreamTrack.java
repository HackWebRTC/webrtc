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

/** Java wrapper for a C++ MediaStreamTrackInterface. */
public class MediaStreamTrack {
  /** Tracks MediaStreamTrackInterface.TrackState */
  public enum State {
    INITIALIZING, LIVE, ENDED, FAILED
  }

  final long nativeTrack;

  public MediaStreamTrack(long nativeTrack) {
    this.nativeTrack = nativeTrack;
  }

  public String id() {
    return nativeId(nativeTrack);
  }

  public String kind() {
    return nativeKind(nativeTrack);
  }

  public boolean enabled() {
    return nativeEnabled(nativeTrack);
  }

  public boolean setEnabled(boolean enable) {
    return nativeSetEnabled(nativeTrack, enable);
  }

  public State state() {
    return nativeState(nativeTrack);
  }

  public boolean setState(State newState) {
    return nativeSetState(nativeTrack, newState.ordinal());
  }

  public void dispose() {
    free(nativeTrack);
  }

  private static native String nativeId(long nativeTrack);

  private static native String nativeKind(long nativeTrack);

  private static native boolean nativeEnabled(long nativeTrack);

  private static native boolean nativeSetEnabled(
      long nativeTrack, boolean enabled);

  private static native State nativeState(long nativeTrack);

  private static native boolean nativeSetState(
      long nativeTrack, int newState);

  private static native void free(long nativeTrack);
}
