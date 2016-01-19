/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/include/condition_variable_wrapper.h"

// TODO(tommi): Remove completely.  As is there is still some code for Windows
// that relies on ConditionVariableWrapper, but code has been removed on other
// platforms.
#if defined(WEBRTC_WIN)

#include <windows.h>
#include "webrtc/system_wrappers/source/condition_variable_event_win.h"
#include "webrtc/system_wrappers/source/condition_variable_native_win.h"

namespace webrtc {
ConditionVariableWrapper* ConditionVariableWrapper::CreateConditionVariable() {
  // Try to create native condition variable implementation.
  ConditionVariableWrapper* ret_val = ConditionVariableNativeWin::Create();
  if (!ret_val) {
    // Native condition variable implementation does not exist. Create generic
    // condition variable based on events.
    ret_val = new ConditionVariableEventWin();
  }
  return ret_val;
}
}  // namespace webrtc

#endif  // defined(WEBRTC_WIN)
