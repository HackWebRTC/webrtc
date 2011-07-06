/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// constants shared by all paths (C, SSE2).
extern float rdft_w[64];

// code path selection function pointers
typedef void (*rft_sub_128_t)(float *a);
extern rft_sub_128_t rftfsub_128;
extern rft_sub_128_t rftbsub_128;

// entry points
void aec_rdft_init(void);
void aec_rdft_init_sse2(void);
void aec_rdft_128(int isgn, float *a);
