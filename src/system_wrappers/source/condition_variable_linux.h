/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_CONDITION_VARIABLE_LINUX_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_CONDITION_VARIABLE_LINUX_H_

#include "condition_variable_wrapper.h"

#include <pthread.h>

namespace webrtc {
class ConditionVariableLinux : public ConditionVariableWrapper
{
public:
    static ConditionVariableWrapper* Create();
    ~ConditionVariableLinux();

    void SleepCS(CriticalSectionWrapper& critSect);
    bool SleepCS(CriticalSectionWrapper& critSect, unsigned long maxTimeInMS);
    void Wake();
    void WakeAll();

private:
    ConditionVariableLinux();
    int Construct();

private:
    pthread_cond_t _cond;
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_CONDITION_VARIABLE_LINUX_H_
