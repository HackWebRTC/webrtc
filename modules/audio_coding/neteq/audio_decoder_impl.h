/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_AUDIO_CODING_NETEQ_AUDIO_DECODER_IMPL_H_
#define MODULES_AUDIO_CODING_NETEQ_AUDIO_DECODER_IMPL_H_

#include <assert.h>

#include "api/audio_codecs/audio_decoder.h"
#include "modules/audio_coding/neteq/neteq_decoder_enum.h"
#include "rtc_base/constructormagic.h"
#include "typedefs.h"  // NOLINT(build/include)

#ifdef WEBRTC_CODEC_G722
#include "modules/audio_coding/codecs/g722/g722_interface.h"
#endif

namespace webrtc {

// Returns true if |codec_type| is supported.
bool CodecSupported(NetEqDecoder codec_type);

}  // namespace webrtc
#endif  // MODULES_AUDIO_CODING_NETEQ_AUDIO_DECODER_IMPL_H_
