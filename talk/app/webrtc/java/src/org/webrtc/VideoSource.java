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

/**
 * Java version of VideoSourceInterface, extended with stop/restart
 * functionality to allow explicit control of the camera device on android,
 * where there is no support for multiple open capture devices and the cost of
 * holding a camera open (even if MediaStreamTrack.setEnabled(false) is muting
 * its output to the encoder) can be too high to bear.
 */
public class VideoSource extends MediaSource {
  private long nativeVideoFormatAtStop;

  public VideoSource(long nativeSource) {
    super(nativeSource);
  }

  // Stop capture feeding this source.
  public void stop() {
    nativeVideoFormatAtStop = stop(nativeSource);
  }

  // Restart capture feeding this source.  stop() must have been called since
  // the last call to restart() (if any).  Note that this isn't "start()";
  // sources are started by default at birth.
  public void restart() {
    restart(nativeSource, nativeVideoFormatAtStop);
    nativeVideoFormatAtStop = 0;
  }

  @Override
  public void dispose() {
    if (nativeVideoFormatAtStop != 0) {
      freeNativeVideoFormat(nativeVideoFormatAtStop);
      nativeVideoFormatAtStop = 0;
    }
    super.dispose();
  }

  // This stop() returns an owned C++ VideoFormat pointer for use in restart()
  // and dispose().
  private static native long stop(long nativeSource);
  private static native void restart(
      long nativeSource, long nativeVideoFormatAtStop);
  private static native void freeNativeVideoFormat(long nativeVideoFormat);
}
