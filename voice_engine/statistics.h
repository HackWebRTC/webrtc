/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VOICE_ENGINE_STATISTICS_H_
#define VOICE_ENGINE_STATISTICS_H_

#include "common_types.h"  // NOLINT(build/include)
#include "rtc_base/criticalsection.h"
#include "typedefs.h"  // NOLINT(build/include)
#include "voice_engine/include/voe_errors.h"
#include "voice_engine/voice_engine_defines.h"

namespace webrtc {
namespace voe {

class Statistics
{
 public:
    enum {KTraceMaxMessageSize = 256};
 public:
    Statistics(uint32_t instanceId);
    ~Statistics();

    int32_t SetInitialized();
    int32_t SetUnInitialized();
    bool Initialized() const;
    int32_t SetLastError(int32_t error) const;
    int32_t SetLastError(int32_t error, TraceLevel level) const;
    int32_t SetLastError(int32_t error,
                         TraceLevel level,
                         const char* msg) const;

 private:
    rtc::CriticalSection lock_;
    const uint32_t _instanceId;
    mutable int32_t _lastError;
    bool _isInitialized;
};

}  // namespace voe

}  // namespace webrtc

#endif // VOICE_ENGINE_STATISTICS_H_
