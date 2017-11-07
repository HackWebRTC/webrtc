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

// Explicit imports necessary for JNI generation.
import org.webrtc.EncodedImage;
import org.webrtc.VideoEncoder;
import java.nio.ByteBuffer;

/**
 * This class contains the Java glue code for JNI generation of VideoEncoder.
 */
class VideoEncoderWrapper {
  @CalledByNative
  static VideoEncoder.Settings createSettings(int numberOfCores, int width, int height,
      int startBitrate, int maxFramerate, boolean automaticResizeOn) {
    return new VideoEncoder.Settings(
        numberOfCores, width, height, startBitrate, maxFramerate, automaticResizeOn);
  }

  @CalledByNative
  static VideoEncoder.EncodeInfo createEncodeInfo(EncodedImage.FrameType[] frameTypes) {
    return new VideoEncoder.EncodeInfo(frameTypes);
  }

  @CalledByNative
  static VideoEncoder.BitrateAllocation createBitrateAllocation(int[][] bitratesBbs) {
    return new VideoEncoder.BitrateAllocation(bitratesBbs);
  }

  @CalledByNative
  static EncodedImage.FrameType createFrameType(int nativeIndex) {
    for (EncodedImage.FrameType type : EncodedImage.FrameType.values()) {
      if (type.getNative() == nativeIndex) {
        return type;
      }
    }
    throw new IllegalArgumentException("Unknown native frame type: " + nativeIndex);
  }

  @CalledByNative
  static boolean getScalingSettingsOn(VideoEncoder.ScalingSettings scalingSettings) {
    return scalingSettings.on;
  }

  @CalledByNative
  static Integer getScalingSettingsLow(VideoEncoder.ScalingSettings scalingSettings) {
    return scalingSettings.low;
  }

  @CalledByNative
  static Integer getScalingSettingsHigh(VideoEncoder.ScalingSettings scalingSettings) {
    return scalingSettings.high;
  }

  @CalledByNative
  static int getIntValue(Integer i) {
    return i.intValue();
  }

  @CalledByNative
  static VideoEncoder.Callback createEncoderCallback(final long nativeEncoder) {
    return (EncodedImage frame, VideoEncoder.CodecSpecificInfo info)
               -> onEncodedFrame(nativeEncoder, frame.buffer, frame.encodedWidth,
                   frame.encodedHeight, frame.captureTimeNs, frame.frameType.getNative(),
                   frame.rotation, frame.completeFrame, frame.qp);
  }

  private static native void onEncodedFrame(long nativeEncoder, ByteBuffer buffer, int encodedWidth,
      int encodedHeight, long captureTimeNs, int frameType, int rotation, boolean completeFrame,
      Integer qp);
}
