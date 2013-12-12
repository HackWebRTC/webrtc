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

public class RtcpStatistics {
  // Definition of fraction lost can be found in RFC3550.
  // It is equivalent to taking the integer part after multiplying the loss
  // fraction by 256.
  public final int fractionLost;
  public final int cumulativeLost;
  public final int extendedMax;
  public final int jitter;
  public final int rttMs;

  // Only allowed to be created by the native layer.
  private RtcpStatistics(int fractionLost, int cumulativeLost, int extendedMax,
      int jitter, int rttMs) {
    this.fractionLost = fractionLost;
    this.cumulativeLost = cumulativeLost;
    this.extendedMax = extendedMax;
    this.jitter = jitter;
    this.rttMs = rttMs;
  }
}