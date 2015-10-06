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

package org.webrtc;

/** Java wrapper for a C++ RtpSenderInterface. */
public class RtpSender {
  final long nativeRtpSender;

  private MediaStreamTrack cachedTrack;

  public RtpSender(long nativeRtpSender) {
    this.nativeRtpSender = nativeRtpSender;
    long track = nativeGetTrack(nativeRtpSender);
    // It may be possible for an RtpSender to be created without a track.
    cachedTrack = (track == 0) ? null : new MediaStreamTrack(track);
  }

  // NOTE: This should not be called with a track that's already used by
  // another RtpSender, because then it would be double-disposed.
  public void setTrack(MediaStreamTrack track) {
    if (cachedTrack != null) {
      cachedTrack.dispose();
    }
    cachedTrack = track;
    nativeSetTrack(nativeRtpSender, (track == null) ? 0 : track.nativeTrack);
  }

  public MediaStreamTrack track() {
    return cachedTrack;
  }

  public String id() {
    return nativeId(nativeRtpSender);
  }

  public void dispose() {
    if (cachedTrack != null) {
      cachedTrack.dispose();
    }
    free(nativeRtpSender);
  }

  private static native void nativeSetTrack(long nativeRtpSender,
                                            long nativeTrack);

  // This should increment the reference count of the track.
  // Will be released in dispose() or setTrack().
  private static native long nativeGetTrack(long nativeRtpSender);

  private static native String nativeId(long nativeRtpSender);

  private static native void free(long nativeRtpSender);
}
;
