/*
 *  Copyright 2006 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


// Originally comes from shared/commandlineflags/flags.h

// Flags are defined and declared using DEFINE_xxx and DECLARE_xxx macros,
// where xxx is the flag type. Flags are referred to via FLAG_yyy,
// where yyy is the flag name. For intialization and iteration of flags,
// see the FlagList class. For full programmatic access to any
// flag, see the Flag class.
//
// The implementation only relies and basic C++ functionality
// and needs no special library or STL support.

#ifndef WEBRTC_BASE_FLAGS_H_
#define WEBRTC_BASE_FLAGS_H_


// This header is deprecated and is just left here temporarily during
// refactoring. See https://bugs.webrtc.org/7634 for more details.
#include "webrtc/rtc_base/flags.h"

#endif  // SHARED_COMMANDLINEFLAGS_FLAGS_H_
