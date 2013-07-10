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

#include "talk/base/latebindingsymboltable.h"

#ifdef POSIX
#include <dlfcn.h>
#endif

#include "talk/base/logging.h"

namespace talk_base {

#ifdef POSIX
static const DllHandle kInvalidDllHandle = NULL;
#else
#error Not implemented
#endif

static const char *GetDllError() {
#ifdef POSIX
  const char *err = dlerror();
  if (err) {
    return err;
  } else {
    return "No error";
  }
#else
#error Not implemented
#endif
}

static bool LoadSymbol(DllHandle handle,
                       const char *symbol_name,
                       void **symbol) {
#ifdef POSIX
  *symbol = dlsym(handle, symbol_name);
  const char *err = dlerror();
  if (err) {
    LOG(LS_ERROR) << "Error loading symbol " << symbol_name << ": " << err;
    return false;
  } else if (!*symbol) {
    // ELF allows for symbols to be NULL, but that should never happen for our
    // usage.
    LOG(LS_ERROR) << "Symbol " << symbol_name << " is NULL";
    return false;
  }
  return true;
#else
#error Not implemented
#endif
}

LateBindingSymbolTable::LateBindingSymbolTable(const TableInfo *info,
    void **table)
    : info_(info),
      table_(table),
      handle_(kInvalidDllHandle),
      undefined_symbols_(false) {
  ClearSymbols();
}

LateBindingSymbolTable::~LateBindingSymbolTable() {
  Unload();
}

bool LateBindingSymbolTable::IsLoaded() const {
  return handle_ != kInvalidDllHandle;
}

bool LateBindingSymbolTable::Load() {
  ASSERT(info_->dll_name != NULL);
  return LoadFromPath(info_->dll_name);
}

bool LateBindingSymbolTable::LoadFromPath(const char *dll_path) {
  if (IsLoaded()) {
    return true;
  }
  if (undefined_symbols_) {
    // We do not attempt to load again because repeated attempts are not
    // likely to succeed and DLL loading is costly.
    LOG(LS_ERROR) << "We know there are undefined symbols";
    return false;
  }

#ifdef POSIX
  handle_ = dlopen(dll_path, RTLD_NOW);
#else
#error Not implemented
#endif

  if (handle_ == kInvalidDllHandle) {
    LOG(LS_WARNING) << "Can't load " << dll_path << ": "
                    << GetDllError();
    return false;
  }
#ifdef POSIX
  // Clear any old errors.
  dlerror();
#endif
  for (int i = 0; i < info_->num_symbols; ++i) {
    if (!LoadSymbol(handle_, info_->symbol_names[i], &table_[i])) {
      undefined_symbols_ = true;
      Unload();
      return false;
    }
  }
  return true;
}

void LateBindingSymbolTable::Unload() {
  if (!IsLoaded()) {
    return;
  }

#ifdef POSIX
  if (dlclose(handle_) != 0) {
    LOG(LS_ERROR) << GetDllError();
  }
#else
#error Not implemented
#endif

  handle_ = kInvalidDllHandle;
  ClearSymbols();
}

void LateBindingSymbolTable::ClearSymbols() {
  memset(table_, 0, sizeof(void *) * info_->num_symbols);
}

}  // namespace talk_base
