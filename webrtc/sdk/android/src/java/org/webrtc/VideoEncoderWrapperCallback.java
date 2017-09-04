/*
 * Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 * Use of this source code is governed by a BSD-style license
 * that can be found in the LICENSE file in the root of the source
 * tree. An additional intellectual property rights grant can be found
 * in the file PATENTS.  All contributing project authors may
 * be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import java.nio.ByteBuffer;

/**
 * VideoEncoder callback that calls VideoEncoderWrapper.OnEncodedFrame for the Encoded frames.
 */
class VideoEncoderWrapperCallback implements VideoEncoder.Callback {
  private final long nativeEncoder;

  public VideoEncoderWrapperCallback(long nativeEncoder) {
    this.nativeEncoder = nativeEncoder;
  }

  @Override
  public void onEncodedFrame(EncodedImage frame, VideoEncoder.CodecSpecificInfo info) {
    nativeOnEncodedFrame(nativeEncoder, frame.buffer, frame.encodedWidth, frame.encodedHeight,
        frame.captureTimeNs, frame.frameType.getNative(), frame.rotation, frame.completeFrame,
        frame.qp);
  }

  private native static void nativeOnEncodedFrame(long nativeEncoder, ByteBuffer buffer,
      int encodedWidth, int encodedHeight, long captureTimeNs, int frameType, int rotation,
      boolean completeFrame, Integer qp);
}
