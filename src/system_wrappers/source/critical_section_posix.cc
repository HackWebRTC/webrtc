/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "critical_section_posix.h"

namespace webrtc {
CriticalSectionPosix::CriticalSectionPosix()
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_mutex, &attr);
}

CriticalSectionPosix::~CriticalSectionPosix()
{
    pthread_mutex_destroy(&_mutex);
}

void
CriticalSectionPosix::Enter()
{
    pthread_mutex_lock(&_mutex);
}

void
CriticalSectionPosix::Leave()
{
    pthread_mutex_unlock(&_mutex);
}
} // namespace webrtc
