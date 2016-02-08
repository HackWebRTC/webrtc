/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/base/mediaengine.h"

#if !defined(DISABLE_MEDIA_ENGINE_FACTORY)

#if defined(HAVE_WEBRTC_VOICE) && defined(HAVE_WEBRTC_VIDEO)
#include "webrtc/media/webrtc/webrtcmediaengine.h"
#endif  // HAVE_WEBRTC_VOICE && HAVE_WEBRTC_VIDEO

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG

namespace cricket {

MediaEngineFactory::MediaEngineCreateFunction
    MediaEngineFactory::create_function_ = NULL;

MediaEngineFactory::MediaEngineCreateFunction
    MediaEngineFactory::SetCreateFunction(MediaEngineCreateFunction function) {
  MediaEngineCreateFunction old_function = create_function_;
  create_function_ = function;
  return old_function;
}

};  // namespace cricket

#endif  // DISABLE_MEDIA_ENGINE_FACTORY
