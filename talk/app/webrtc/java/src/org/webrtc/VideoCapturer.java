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

/** Java version of cricket::VideoCapturer. */
public class VideoCapturer {
  private long nativeVideoCapturer;

  protected VideoCapturer() {
  }

  public static VideoCapturer create(String deviceName) {
    Object capturer = nativeCreateVideoCapturer(deviceName);
    if (capturer != null)
      return (VideoCapturer) (capturer);
    return null;
  }

  // Sets |nativeCapturer| to be owned by VideoCapturer.
  protected void setNativeCapturer(long nativeCapturer) {
    this.nativeVideoCapturer = nativeCapturer;
  }

  // Package-visible for PeerConnectionFactory.
  long takeNativeVideoCapturer() {
    if (nativeVideoCapturer == 0) {
      throw new RuntimeException("Capturer can only be taken once!");
    }
    long ret = nativeVideoCapturer;
    nativeVideoCapturer = 0;
    return ret;
  }

  public void dispose() {
    // No-op iff this capturer is owned by a source (see comment on
    // PeerConnectionFactoryInterface::CreateVideoSource()).
    if (nativeVideoCapturer != 0) {
      free(nativeVideoCapturer);
    }
  }

  private static native Object nativeCreateVideoCapturer(String deviceName);

  private static native void free(long nativeVideoCapturer);
}
