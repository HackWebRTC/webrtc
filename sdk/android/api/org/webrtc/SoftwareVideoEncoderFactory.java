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

import java.util.HashMap;

public class SoftwareVideoEncoderFactory implements VideoEncoderFactory {
  @Override
  public VideoEncoder createEncoder(VideoCodecInfo info) {
    if (info.name.equalsIgnoreCase("VP8")) {
      return new VP8Encoder();
    }
    if (info.name.equalsIgnoreCase("VP9")) {
      return new VP9Encoder();
    }

    return null;
  }

  @Override
  public VideoCodecInfo[] getSupportedCodecs() {
    return supportedCodecs();
  }

  public static VideoCodecInfo[] supportedCodecs() {
    VideoCodecInfo vp8Info = new VideoCodecInfo("VP8", new HashMap<>());
    VideoCodecInfo vp9Info = new VideoCodecInfo("VP9", new HashMap<>());

    return new VideoCodecInfo[] {vp8Info, vp9Info};
  }
}
