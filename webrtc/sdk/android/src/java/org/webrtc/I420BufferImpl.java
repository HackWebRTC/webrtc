/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.nio.ByteBuffer;
import org.webrtc.VideoFrame.I420Buffer;

/** Implementation of an I420 VideoFrame buffer. */
class I420BufferImpl implements VideoFrame.I420Buffer {
  private final ByteBuffer buffer;
  private final int width;
  private final int height;
  private final int chromaHeight;
  private final int yPos;
  private final int strideY;
  private final int uPos;
  private final int strideU;
  private final int vPos;
  private final int strideV;
  private final ReleaseCallback releaseCallback;

  private int refCount;

  /** Allocates an I420Buffer backed by existing data. */
  I420BufferImpl(ByteBuffer buffer, int width, int height, int yPos, int strideY, int uPos,
      int strideU, int vPos, int strideV, ReleaseCallback releaseCallback) {
    this.buffer = buffer;
    this.width = width;
    this.height = height;
    this.chromaHeight = (height + 1) / 2;
    this.yPos = yPos;
    this.strideY = strideY;
    this.uPos = uPos;
    this.strideU = strideU;
    this.vPos = vPos;
    this.strideV = strideV;
    this.releaseCallback = releaseCallback;

    this.refCount = 1;
  }

  /** Allocates an empty I420Buffer suitable for an image of the given dimensions. */
  static I420BufferImpl allocate(int width, int height) {
    int chromaHeight = (height + 1) / 2;
    int strideUV = (width + 1) / 2;
    int yPos = 0;
    int uPos = yPos + width * height;
    int vPos = uPos + strideUV * chromaHeight;
    ByteBuffer buffer = ByteBuffer.allocateDirect(width * height + 2 * strideUV * chromaHeight);
    return new I420BufferImpl(
        buffer, width, height, yPos, width, uPos, strideUV, vPos, strideUV, null);
  }

  @Override
  public int getWidth() {
    return width;
  }

  @Override
  public int getHeight() {
    return height;
  }

  @Override
  public ByteBuffer getDataY() {
    ByteBuffer data = buffer.slice();
    data.position(yPos);
    data.limit(yPos + getStrideY() * height);
    return data;
  }

  @Override
  public ByteBuffer getDataU() {
    ByteBuffer data = buffer.slice();
    data.position(uPos);
    data.limit(uPos + strideU * chromaHeight);
    return data;
  }

  @Override
  public ByteBuffer getDataV() {
    ByteBuffer data = buffer.slice();
    data.position(vPos);
    data.limit(vPos + strideV * chromaHeight);
    return data;
  }

  @Override
  public int getStrideY() {
    return strideY;
  }

  @Override
  public int getStrideU() {
    return strideU;
  }

  @Override
  public int getStrideV() {
    return strideV;
  }

  @Override
  public I420Buffer toI420() {
    return this;
  }

  @Override
  public void retain() {
    ++refCount;
  }

  @Override
  public void release() {
    if (--refCount == 0 && releaseCallback != null) {
      releaseCallback.onRelease();
    }
  }

  // Callback called when the frame is no longer referenced.
  interface ReleaseCallback {
    // Called when the frame is no longer referenced.
    void onRelease();
  }
}
