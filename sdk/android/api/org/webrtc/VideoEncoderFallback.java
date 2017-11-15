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
 * A combined video encoder that falls back on a secondary encoder if the primary encoder fails.
 */
public class VideoEncoderFallback extends WrappedNativeVideoEncoder {
  public VideoEncoderFallback(VideoEncoder fallback, VideoEncoder primary) {
    super(createNativeEncoder(fallback, primary));
  }

  private static native long createNativeEncoder(VideoEncoder fallback, VideoEncoder primary);
}
