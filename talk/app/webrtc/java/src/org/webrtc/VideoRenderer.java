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

import java.nio.ByteBuffer;
import java.util.Arrays;

/**
 * Java version of VideoRendererInterface.  In addition to allowing clients to
 * define their own rendering behavior (by passing in a Callbacks object), this
 * class also provides a createGui() method for creating a GUI-rendering window
 * on various platforms.
 */
public class VideoRenderer {

  /** Java version of cricket::VideoFrame. */
  public static class I420Frame {
    public final int width;
    public final int height;
    public final int[] yuvStrides;
    public final ByteBuffer[] yuvPlanes;

    /**
     * Construct a frame of the given dimensions with the specified planar
     * data.  If |yuvPlanes| is null, new planes of the appropriate sizes are
     * allocated.
     */
    public I420Frame(
        int width, int height, int[] yuvStrides, ByteBuffer[] yuvPlanes) {
      this.width = width;
      this.height = height;
      this.yuvStrides = yuvStrides;
      if (yuvPlanes == null) {
        yuvPlanes = new ByteBuffer[3];
        yuvPlanes[0] = ByteBuffer.allocateDirect(yuvStrides[0] * height);
        yuvPlanes[1] = ByteBuffer.allocateDirect(yuvStrides[1] * height);
        yuvPlanes[2] = ByteBuffer.allocateDirect(yuvStrides[2] * height);
      }
      this.yuvPlanes = yuvPlanes;
    }

    /**
     * Copy the planes out of |source| into |this| and return |this|.  Calling
     * this with mismatched frame dimensions is a programming error and will
     * likely crash.
     */
    public I420Frame copyFrom(I420Frame source) {
      if (!Arrays.equals(yuvStrides, source.yuvStrides) ||
          width != source.width || height != source.height) {
        throw new RuntimeException("Mismatched dimensions!  Source: " +
            source.toString() + ", destination: " + toString());
      }
      copyPlane(source.yuvPlanes[0], yuvPlanes[0]);
      copyPlane(source.yuvPlanes[1], yuvPlanes[1]);
      copyPlane(source.yuvPlanes[2], yuvPlanes[2]);
      return this;
    }

    @Override
    public String toString() {
      return width + "x" + height + ":" + yuvStrides[0] + ":" + yuvStrides[1] +
          ":" + yuvStrides[2];
    }

    // Copy the bytes out of |src| and into |dst|, ignoring and overwriting
    // positon & limit in both buffers.
    private void copyPlane(ByteBuffer src, ByteBuffer dst) {
      src.position(0).limit(src.capacity());
      dst.put(src);
      dst.position(0).limit(dst.capacity());
    }
}

  /** The real meat of VideoRendererInterface. */
  public static interface Callbacks {
    public void setSize(int width, int height);
    public void renderFrame(I420Frame frame);
  }

  // |this| either wraps a native (GUI) renderer or a client-supplied Callbacks
  // (Java) implementation; so exactly one of these will be non-0/null.
  final long nativeVideoRenderer;
  private final Callbacks callbacks;

  public static VideoRenderer createGui(int x, int y) {
    long nativeVideoRenderer = nativeCreateGuiVideoRenderer(x, y);
    if (nativeVideoRenderer == 0) {
      return null;
    }
    return new VideoRenderer(nativeVideoRenderer);
  }

  public VideoRenderer(Callbacks callbacks) {
    nativeVideoRenderer = nativeWrapVideoRenderer(callbacks);
    this.callbacks = callbacks;
  }

  private VideoRenderer(long nativeVideoRenderer) {
    this.nativeVideoRenderer = nativeVideoRenderer;
    callbacks = null;
  }

  public void dispose() {
    free(nativeVideoRenderer);
  }

  private static native long nativeCreateGuiVideoRenderer(int x, int y);
  private static native long nativeWrapVideoRenderer(Callbacks callbacks);

  private static native void free(long nativeVideoRenderer);
}
