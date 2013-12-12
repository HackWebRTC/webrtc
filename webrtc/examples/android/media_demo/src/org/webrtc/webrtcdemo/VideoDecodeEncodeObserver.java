/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.webrtcdemo;

public interface VideoDecodeEncodeObserver {
  void incomingRate(int videoChannel, int framerate, int bitrate);

  // VideoCodecInst.dispose must be called for |videoCodec| before all
  // references to it are lost as it will free memory allocated in the native
  // layer.
  void incomingCodecChanged(int videoChannel, VideoCodecInst videoCodec);

  void requestNewKeyFrame(int videoChannel);

  void outgoingRate(int videoChannel, int framerate, int bitrate);
}
