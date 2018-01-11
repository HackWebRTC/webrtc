/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VOICE_ENGINE_VOICE_ENGINE_IMPL_H_
#define VOICE_ENGINE_VOICE_ENGINE_IMPL_H_

#include <memory>

#include "system_wrappers/include/atomic32.h"
#include "typedefs.h"  // NOLINT(build/include)
#include "voice_engine/voe_base_impl.h"

namespace webrtc {

class VoiceEngineImpl : public VoiceEngine,
                        public VoEBaseImpl {
 public:
  VoiceEngineImpl()
      : VoEBaseImpl(),
        _ref_count(0) {}
  ~VoiceEngineImpl() override { assert(_ref_count.Value() == 0); }

  int AddRef();

  // This implements the Release() method for all the inherited interfaces.
  int Release() override;

 // This is *protected* so that FakeVoiceEngine can inherit from the class and
 // manipulate the reference count. See: fake_voice_engine.h.
 protected:
  Atomic32 _ref_count;
};

}  // namespace webrtc

#endif  // VOICE_ENGINE_VOICE_ENGINE_IMPL_H_
