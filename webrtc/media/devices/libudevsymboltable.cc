/*
 *  Copyright (c) 2004 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/media/devices/libudevsymboltable.h"

#include <dlfcn.h>

#include "webrtc/base/logging.h"

namespace cricket {

#define LATE_BINDING_SYMBOL_TABLE_CLASS_NAME LIBUDEV_SYMBOLS_CLASS_NAME
#define LATE_BINDING_SYMBOL_TABLE_SYMBOLS_LIST LIBUDEV_SYMBOLS_LIST
#define LATE_BINDING_SYMBOL_TABLE_DLL_NAME "libudev.so.0"
#include "webrtc/base/latebindingsymboltable.cc.def"
#undef LATE_BINDING_SYMBOL_TABLE_CLASS_NAME
#undef LATE_BINDING_SYMBOL_TABLE_SYMBOLS_LIST
#undef LATE_BINDING_SYMBOL_TABLE_DLL_NAME

bool IsWrongLibUDevAbiVersion(rtc::DllHandle libudev_0) {
  rtc::DllHandle libudev_1 = dlopen("libudev.so.1",
                                          RTLD_NOW|RTLD_LOCAL|RTLD_NOLOAD);
  bool unsafe_symlink = (libudev_0 == libudev_1);
  if (unsafe_symlink) {
    // .0 and .1 are distinct ABIs, so if they point to the same thing then one
    // of them must be wrong. Probably the old has been symlinked to the new in
    // a misguided attempt at backwards compatibility.
    LOG(LS_ERROR) << "libudev.so.0 and libudev.so.1 unsafely point to the"
                     " same thing; not using libudev";
  } else if (libudev_1) {
    // If libudev.so.1 is resident but distinct from libudev.so.0, then some
    // system library loaded the new ABI separately. This is not a problem for
    // LateBindingSymbolTable because its symbol look-ups are restricted to its
    // DllHandle, but having libudev.so.0 resident may cause problems for that
    // system library because symbol names are not namespaced by DLL. (Although
    // our use of RTLD_LOCAL should avoid most problems.)
    LOG(LS_WARNING)
        << "libudev.so.1 is resident but distinct from libudev.so.0";
  }
  if (libudev_1) {
    // Release the refcount that we acquired above. (Does not unload the DLL;
    // whoever loaded it still needs it.)
    dlclose(libudev_1);
  }
  return unsafe_symlink;
}

}  // namespace cricket
