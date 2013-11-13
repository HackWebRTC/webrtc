/*
 * libjingle
 * Copyright 2004 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/media/devices/libudevsymboltable.h"

#include <dlfcn.h>

#include "talk/base/logging.h"

namespace cricket {

#define LATE_BINDING_SYMBOL_TABLE_CLASS_NAME LIBUDEV_SYMBOLS_CLASS_NAME
#define LATE_BINDING_SYMBOL_TABLE_SYMBOLS_LIST LIBUDEV_SYMBOLS_LIST
#define LATE_BINDING_SYMBOL_TABLE_DLL_NAME "libudev.so.0"
#include "talk/base/latebindingsymboltable.cc.def"
#undef LATE_BINDING_SYMBOL_TABLE_CLASS_NAME
#undef LATE_BINDING_SYMBOL_TABLE_SYMBOLS_LIST
#undef LATE_BINDING_SYMBOL_TABLE_DLL_NAME

bool IsWrongLibUDevAbiVersion(talk_base::DllHandle libudev_0) {
  talk_base::DllHandle libudev_1 = dlopen("libudev.so.1",
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
