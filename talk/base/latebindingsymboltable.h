/*
 * libjingle
 * Copyright 2004--2010, Google Inc.
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

#ifndef TALK_BASE_LATEBINDINGSYMBOLTABLE_H_
#define TALK_BASE_LATEBINDINGSYMBOLTABLE_H_

#include <string.h>

#include "talk/base/common.h"

namespace talk_base {

#ifdef POSIX
typedef void *DllHandle;
#else
#error Not implemented for this platform
#endif

// This is the base class for "symbol table" classes to simplify the dynamic
// loading of symbols from DLLs. Currently the implementation only supports
// Linux and OS X, and pure C symbols (or extern "C" symbols that wrap C++
// functions).  Sub-classes for specific DLLs are generated via the "supermacro"
// files latebindingsymboltable.h.def and latebindingsymboltable.cc.def. See
// talk/sound/pulseaudiosymboltable.(h|cc) for an example.
class LateBindingSymbolTable {
 public:
  struct TableInfo {
    const char *dll_name;
    int num_symbols;
    // Array of size num_symbols.
    const char *const *symbol_names;
  };

  LateBindingSymbolTable(const TableInfo *info, void **table);
  ~LateBindingSymbolTable();

  bool IsLoaded() const;
  // Loads the DLL and the symbol table. Returns true iff the DLL and symbol
  // table loaded successfully.
  bool Load();
  // Like load, but allows overriding the dll path for when the dll path is
  // dynamic.
  bool LoadFromPath(const char *dll_path);
  void Unload();

  // Gets the raw OS handle to the DLL. Be careful what you do with it.
  DllHandle GetDllHandle() const { return handle_; }

 private:
  void ClearSymbols();

  const TableInfo *info_;
  void **table_;
  DllHandle handle_;
  bool undefined_symbols_;

  DISALLOW_COPY_AND_ASSIGN(LateBindingSymbolTable);
};

}  // namespace talk_base

#endif  // TALK_BASE_LATEBINDINGSYMBOLTABLE_H_
