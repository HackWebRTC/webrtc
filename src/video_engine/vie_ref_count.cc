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
 * ViERefCount.cpp
 */


#include "vie_ref_count.h"
#include "critical_section_wrapper.h"


ViERefCount::ViERefCount() :
    _count(0),
    _crit(*webrtc::CriticalSectionWrapper::CreateCriticalSection())
{
}

ViERefCount::~ViERefCount()
{
    delete &_crit;
}

ViERefCount&
ViERefCount::operator++(int)
{
    webrtc::CriticalSectionScoped lock(_crit);
    _count++;
    return *this;
}
    
ViERefCount&
ViERefCount::operator--(int)
{
    webrtc::CriticalSectionScoped lock(_crit);
    _count--;
    return *this;
}
  
void 
ViERefCount::Reset()
{
    webrtc::CriticalSectionScoped lock(_crit);
    _count = 0;
}

int 
ViERefCount::GetCount() const
{
    return _count;
}
