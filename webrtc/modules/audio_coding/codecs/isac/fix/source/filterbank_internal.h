/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_FILTERBANK_INTERNAL_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_FILTERBANK_INTERNAL_H_

#include "typedefs.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif

/* Arguments:
 *   io:  Input/output, in Q0.
 *   len: Input, sample length.
 *   coefficient: Input.
 *   state: Input/output, filter state, in Q4.
 */
void WebRtcIsacfix_HighpassFilterFixDec32(int16_t *io,
                                          int16_t len,
                                          const int16_t *coefficient,
                                          int32_t *state);

#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif
/* WEBRTC_MODULES_AUDIO_CODING_CODECS_ISAC_FIX_SOURCE_FILTERBANK_INTERNAL_H_ */
