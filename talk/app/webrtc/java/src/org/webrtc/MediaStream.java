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

import java.util.LinkedList;

/** Java wrapper for a C++ MediaStreamInterface. */
public class MediaStream {
  public final LinkedList<AudioTrack> audioTracks;
  public final LinkedList<VideoTrack> videoTracks;
  // Package-protected for PeerConnection.
  final long nativeStream;

  public MediaStream(long nativeStream) {
    audioTracks = new LinkedList<AudioTrack>();
    videoTracks = new LinkedList<VideoTrack>();
    this.nativeStream = nativeStream;
  }

  public boolean addTrack(AudioTrack track) {
    if (nativeAddAudioTrack(nativeStream, track.nativeTrack)) {
      audioTracks.add(track);
      return true;
    }
    return false;
  }

  public boolean addTrack(VideoTrack track) {
    if (nativeAddVideoTrack(nativeStream, track.nativeTrack)) {
      videoTracks.add(track);
      return true;
    }
    return false;
  }

  public boolean removeTrack(AudioTrack track) {
    if (nativeRemoveAudioTrack(nativeStream, track.nativeTrack)) {
      audioTracks.remove(track);
      return true;
    }
    return false;
  }

  public boolean removeTrack(VideoTrack track) {
    if (nativeRemoveVideoTrack(nativeStream, track.nativeTrack)) {
      videoTracks.remove(track);
      return true;
    }
    return false;
  }

  public void dispose() {
    while (!audioTracks.isEmpty()) {
      AudioTrack track = audioTracks.getFirst();
      removeTrack(track);
      track.dispose();
    }
    while (!videoTracks.isEmpty()) {
      VideoTrack track = videoTracks.getFirst();
      removeTrack(track);
      track.dispose();
    }
    free(nativeStream);
  }

  public String label() {
    return nativeLabel(nativeStream);
  }

  public String toString() {
    return "[" + label() + ":A=" + audioTracks.size() +
        ":V=" + videoTracks.size() + "]";
  }

  private static native boolean nativeAddAudioTrack(
      long nativeStream, long nativeAudioTrack);

  private static native boolean nativeAddVideoTrack(
      long nativeStream, long nativeVideoTrack);

  private static native boolean nativeRemoveAudioTrack(
      long nativeStream, long nativeAudioTrack);

  private static native boolean nativeRemoveVideoTrack(
      long nativeStream, long nativeVideoTrack);

  private static native String nativeLabel(long nativeStream);

  private static native void free(long nativeStream);
}
