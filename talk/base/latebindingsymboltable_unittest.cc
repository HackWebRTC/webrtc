/*
 * libjingle
 * Copyright 2010, Google Inc.
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

#ifdef LINUX
#include <math.h>
#endif

#include "talk/base/gunit.h"
#include "talk/base/latebindingsymboltable.h"

namespace talk_base {

#ifdef LINUX

#define LIBM_SYMBOLS_CLASS_NAME LibmTestSymbolTable
#define LIBM_SYMBOLS_LIST \
  X(acos) \
  X(sin) \
  X(tan)

#define LATE_BINDING_SYMBOL_TABLE_CLASS_NAME LIBM_SYMBOLS_CLASS_NAME
#define LATE_BINDING_SYMBOL_TABLE_SYMBOLS_LIST LIBM_SYMBOLS_LIST
#include "talk/base/latebindingsymboltable.h.def"

#define LATE_BINDING_SYMBOL_TABLE_CLASS_NAME LIBM_SYMBOLS_CLASS_NAME
#define LATE_BINDING_SYMBOL_TABLE_SYMBOLS_LIST LIBM_SYMBOLS_LIST
#define LATE_BINDING_SYMBOL_TABLE_DLL_NAME "libm.so.6"
#include "talk/base/latebindingsymboltable.cc.def"

TEST(LateBindingSymbolTable, libm) {
  LibmTestSymbolTable table;
  EXPECT_FALSE(table.IsLoaded());
  ASSERT_TRUE(table.Load());
  EXPECT_TRUE(table.IsLoaded());
  EXPECT_EQ(table.acos()(0.5), acos(0.5));
  EXPECT_EQ(table.sin()(0.5), sin(0.5));
  EXPECT_EQ(table.tan()(0.5), tan(0.5));
  // It would be nice to check that the addresses are the same, but the nature
  // of dynamic linking and relocation makes them actually be different.
  table.Unload();
  EXPECT_FALSE(table.IsLoaded());
}

#else
#error Not implemented
#endif

}  // namespace talk_base
