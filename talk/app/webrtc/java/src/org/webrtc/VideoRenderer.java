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

import java.nio.ByteBuffer;

/**
 * Java version of VideoRendererInterface.  In addition to allowing clients to
 * define their own rendering behavior (by passing in a Callbacks object), this
 * class also provides a createGui() method for creating a GUI-rendering window
 * on various platforms.
 */
public class VideoRenderer {
  /**
   * Java version of cricket::VideoFrame. Frames are only constructed from native code and test
   * code.
   */
  public static class I420Frame {
    public final int width;
    public final int height;
    public final int[] yuvStrides;
    public ByteBuffer[] yuvPlanes;
    public final boolean yuvFrame;
    public Object textureObject;
    public int textureId;
    // Frame pointer in C++.
    private long nativeFramePointer;

    // rotationDegree is the degree that the frame must be rotated clockwisely
    // to be rendered correctly.
    public int rotationDegree;

    /**
     * Construct a frame of the given dimensions with the specified planar data.
     */
    I420Frame(int width, int height, int rotationDegree, int[] yuvStrides, ByteBuffer[] yuvPlanes,
        long nativeFramePointer) {
      this.width = width;
      this.height = height;
      this.yuvStrides = yuvStrides;
      this.yuvPlanes = yuvPlanes;
      this.yuvFrame = true;
      this.rotationDegree = rotationDegree;
      this.nativeFramePointer = nativeFramePointer;
      if (rotationDegree % 90 != 0) {
        throw new IllegalArgumentException("Rotation degree not multiple of 90: " + rotationDegree);
      }
    }

    /**
     * Construct a texture frame of the given dimensions with data in SurfaceTexture
     */
    I420Frame(
        int width, int height, int rotationDegree,
        Object textureObject, int textureId, long nativeFramePointer) {
      this.width = width;
      this.height = height;
      this.yuvStrides = null;
      this.yuvPlanes = null;
      this.textureObject = textureObject;
      this.textureId = textureId;
      this.yuvFrame = false;
      this.rotationDegree = rotationDegree;
      this.nativeFramePointer = nativeFramePointer;
      if (rotationDegree % 90 != 0) {
        throw new IllegalArgumentException("Rotation degree not multiple of 90: " + rotationDegree);
      }
    }

    public int rotatedWidth() {
      return (rotationDegree % 180 == 0) ? width : height;
    }

    public int rotatedHeight() {
      return (rotationDegree % 180 == 0) ? height : width;
    }

    @Override
    public String toString() {
      return width + "x" + height + ":" + yuvStrides[0] + ":" + yuvStrides[1] +
          ":" + yuvStrides[2];
    }
  }

  // Helper native function to do a video frame plane copying.
  public static native void nativeCopyPlane(ByteBuffer src, int width,
      int height, int srcStride, ByteBuffer dst, int dstStride);

  /** The real meat of VideoRendererInterface. */
  public static interface Callbacks {
    // |frame| might have pending rotation and implementation of Callbacks
    // should handle that by applying rotation during rendering. The callee
    // is responsible for signaling when it is done with |frame| by calling
    // renderFrameDone(frame).
    public void renderFrame(I420Frame frame);
  }

   /**
    * This must be called after every renderFrame() to release the frame.
    */
   public static void renderFrameDone(I420Frame frame) {
     frame.yuvPlanes = null;
     frame.textureObject = null;
     frame.textureId = 0;
     if (frame.nativeFramePointer != 0) {
       releaseNativeFrame(frame.nativeFramePointer);
       frame.nativeFramePointer = 0;
     }
   }

  // |this| either wraps a native (GUI) renderer or a client-supplied Callbacks
  // (Java) implementation; this is indicated by |isWrappedVideoRenderer|.
  long nativeVideoRenderer;
  private final boolean isWrappedVideoRenderer;

  public static VideoRenderer createGui(int x, int y) {
    long nativeVideoRenderer = nativeCreateGuiVideoRenderer(x, y);
    if (nativeVideoRenderer == 0) {
      return null;
    }
    return new VideoRenderer(nativeVideoRenderer);
  }

  public VideoRenderer(Callbacks callbacks) {
    nativeVideoRenderer = nativeWrapVideoRenderer(callbacks);
    isWrappedVideoRenderer = true;
  }

  private VideoRenderer(long nativeVideoRenderer) {
    this.nativeVideoRenderer = nativeVideoRenderer;
    isWrappedVideoRenderer = false;
  }

  public void dispose() {
    if (nativeVideoRenderer == 0) {
      // Already disposed.
      return;
    }
    if (!isWrappedVideoRenderer) {
      freeGuiVideoRenderer(nativeVideoRenderer);
    } else {
      freeWrappedVideoRenderer(nativeVideoRenderer);
    }
    nativeVideoRenderer = 0;
  }

  private static native long nativeCreateGuiVideoRenderer(int x, int y);
  private static native long nativeWrapVideoRenderer(Callbacks callbacks);

  private static native void freeGuiVideoRenderer(long nativeVideoRenderer);
  private static native void freeWrappedVideoRenderer(long nativeVideoRenderer);

  private static native void releaseNativeFrame(long nativeFramePointer);
}
