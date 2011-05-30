/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_CRITICAL_SECTION_LINUX_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_CRITICAL_SECTION_LINUX_H_

#include "critical_section_wrapper.h"

#include <pthread.h>

namespace webrtc {
class CriticalSectionLinux : public CriticalSectionWrapper
{
public:
    CriticalSectionLinux();

    virtual ~CriticalSectionLinux();

    virtual void Enter();
    virtual void Leave();

private:
    pthread_mutex_t _mutex;
    friend class ConditionVariableLinux;
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_CRITICAL_SECTION_LINUX_H_
