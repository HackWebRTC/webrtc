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

/** Java version of cricket::VideoCapturer. */
// TODO(perkj): Merge VideoCapturer and VideoCapturerAndroid.
public class VideoCapturer {
  private long nativeVideoCapturer;

  protected VideoCapturer() {
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

  private static native void free(long nativeVideoCapturer);
}
