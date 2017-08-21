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
  private final int width;
  private final int height;
  private final ByteBuffer dataY;
  private final ByteBuffer dataU;
  private final ByteBuffer dataV;
  private final int strideY;
  private final int strideU;
  private final int strideV;
  private final Runnable releaseCallback;
  private final Object refCountLock = new Object();

  private int refCount;

  /** Constructs an I420Buffer backed by existing data. */
  I420BufferImpl(int width, int height, ByteBuffer dataY, int strideY, ByteBuffer dataU,
      int strideU, ByteBuffer dataV, int strideV, Runnable releaseCallback) {
    this.width = width;
    this.height = height;
    this.dataY = dataY;
    this.dataU = dataU;
    this.dataV = dataV;
    this.strideY = strideY;
    this.strideU = strideU;
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

    buffer.position(yPos);
    buffer.limit(uPos);
    ByteBuffer dataY = buffer.slice();

    buffer.position(uPos);
    buffer.limit(vPos);
    ByteBuffer dataU = buffer.slice();

    buffer.position(vPos);
    buffer.limit(vPos + strideUV * chromaHeight);
    ByteBuffer dataV = buffer.slice();

    return new I420BufferImpl(width, height, dataY, width, dataU, strideUV, dataV, strideUV, null);
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
    return dataY;
  }

  @Override
  public ByteBuffer getDataU() {
    return dataU;
  }

  @Override
  public ByteBuffer getDataV() {
    return dataV;
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
    retain();
    return this;
  }

  @Override
  public void retain() {
    synchronized (refCountLock) {
      ++refCount;
    }
  }

  @Override
  public void release() {
    synchronized (refCountLock) {
      if (--refCount == 0 && releaseCallback != null) {
        releaseCallback.run();
      }
    }
  }

  @Override
  public VideoFrame.Buffer cropAndScale(
      int cropX, int cropY, int cropWidth, int cropHeight, int scaleWidth, int scaleHeight) {
    return VideoFrame.cropAndScaleI420(
        this, cropX, cropY, cropWidth, cropHeight, scaleWidth, scaleHeight);
  }
}
