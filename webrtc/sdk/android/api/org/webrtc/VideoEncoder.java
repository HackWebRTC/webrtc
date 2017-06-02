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

/**
 * Interface for a video encoder that can be used with WebRTC. All calls will be made on the
 * encoding thread.
 */
public interface VideoEncoder {
  /** Settings passed to the encoder by WebRTC. */
  public class Settings {
    public final int numberOfCores;

    public Settings(int numberOfCores) {
      this.numberOfCores = numberOfCores;
    }
  }

  /** Additional info for encoding. */
  public class EncodeInfo {
    public final EncodedImage.FrameType[] frameTypes;

    public EncodeInfo(EncodedImage.FrameType[] frameTypes) {
      this.frameTypes = frameTypes;
    }
  }

  // TODO(sakal): Add values to these classes as necessary.
  /** Codec specific information about the encoded frame. */
  public class CodecSpecificInfo {}

  public class CodecSpecificInfoVP8 extends CodecSpecificInfo {}

  public class CodecSpecificInfoVP9 extends CodecSpecificInfo {}

  public class CodecSpecificInfoH264 extends CodecSpecificInfo {}

  /**
   * Represents bitrate allocated for an encoder to produce frames. Bitrate can be divided between
   * spatial and temporal layers.
   */
  public class BitrateAllocation {
    // First index is the spatial layer and second the temporal layer.
    public final long[][] bitratesBbs;

    /**
     * Initializes the allocation with a two dimensional array of bitrates. The first index of the
     * array is the spatial layer and the second index in the temporal layer.
     */
    public BitrateAllocation(long[][] bitratesBbs) {
      this.bitratesBbs = bitratesBbs;
    }

    /**
     * Gets the total bitrate allocated for all layers.
     */
    public long getSum() {
      long sum = 0;
      for (long[] spatialLayer : bitratesBbs) {
        for (long bitrate : spatialLayer) {
          sum += bitrate;
        }
      }
      return sum;
    }
  }

  /** Settings for WebRTC quality based scaling. */
  public class ScalingSettings {
    public final boolean on;
    public final int low;
    public final int high;

    /**
     * Creates quality based scaling settings.
     *
     * @param on True if quality scaling is turned on.
     * @param low Average QP at which to scale up the resolution.
     * @param high Average QP at which to scale down the resolution.
     */
    public ScalingSettings(boolean on, int low, int high) {
      this.on = on;
      this.low = low;
      this.high = high;
    }
  }

  public interface Callback {
    /** Call to return an encoded frame. */
    void onEncodedFrame(EncodedImage frame, CodecSpecificInfo info);
  }

  /**
   * Initializes the encoding process. Call before any calls to encode.
   */
  void initEncode(Settings settings, Callback encodeCallback);
  /**
   * Releases the encoder. No more calls to encode will be made after this call.
   */
  void release();
  /**
   * Requests the encoder to encode a frame.
   */
  void encode(VideoFrame frame, EncodeInfo info);
  /**
   * Informs the encoder of the packet loss and the round-trip time of the network.
   *
   * @param packetLoss How many packets are lost on average per 255 packets.
   * @param roundTripTimeMs Round-trip time of the network in milliseconds.
   */
  void setChannelParameters(short packetLoss, long roundTripTimeMs);
  /** Sets the bitrate allocation and the target framerate for the encoder. */
  void setRateAllocation(BitrateAllocation allocation, long framerate);
  /** Any encoder that wants to use WebRTC provided quality scaler must implement this method. */
  ScalingSettings getScalingSettings();
  /** Should return a descriptive name for the implementation. */
  String getImplementationName();
}
