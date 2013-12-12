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

public class CodecInst {
  private final long nativeCodecInst;

  // CodecInst can only be created from the native layer.
  private CodecInst(long nativeCodecInst) {
    this.nativeCodecInst = nativeCodecInst;
  }

  public String toString() {
    return name() + " " +
        "PlType: " + plType() + " " +
        "PlFreq: " + plFrequency() + " " +
        "Size: " + pacSize() + " " +
        "Channels: " + channels() + " " +
        "Rate: " + rate();
  }

  // Dispose must be called before all references to CodecInst are lost as it
  // will free memory allocated in the native layer.
  public native void dispose();
  public native int plType();
  public native String name();
  public native int plFrequency();
  public native int pacSize();
  public native int channels();
  public native int rate();
}