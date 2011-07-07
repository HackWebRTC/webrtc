/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * This file contains the function WebRtcSpl_Sqrt().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#define iter1(N)                                \
  try1 = root + (1 << (N));                     \
  if (value >= try1 << (N))                     \
  {                                             \
    value -= try1 << (N);                       \
    root |= 2 << (N);                           \
  }

// (out) Square root of input parameter
WebRtc_Word32 WebRtcSpl_Sqrt(WebRtc_Word32 value) {
  // new routine for performance, 4 cycles/bit in ARM
  // output precision is 16 bits

  WebRtc_Word32 root = 0, try1;
  iter1 (15);    iter1 (14);    iter1 (13);    iter1 (12);
  iter1 (11);    iter1 (10);    iter1 ( 9);    iter1 ( 8);
  iter1 ( 7);    iter1 ( 6);    iter1 ( 5);    iter1 ( 4);
  iter1 ( 3);    iter1 ( 2);    iter1 ( 1);    iter1 ( 0);
  return root >> 1;
}
