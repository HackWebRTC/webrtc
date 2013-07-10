/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#ifndef TALK_BASE_LIBDBUSGLIBSYMBOLTABLE_H_
#define TALK_BASE_LIBDBUSGLIBSYMBOLTABLE_H_

#ifdef HAVE_DBUS_GLIB

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#include "talk/base/latebindingsymboltable.h"

namespace talk_base {

#define LIBDBUS_GLIB_CLASS_NAME LibDBusGlibSymbolTable
// The libdbus-glib symbols we need, as an X-Macro list.
// This list must contain precisely every libdbus-glib function that is used in
// dbus.cc.
#define LIBDBUS_GLIB_SYMBOLS_LIST \
  X(dbus_bus_add_match) \
  X(dbus_connection_add_filter) \
  X(dbus_connection_close) \
  X(dbus_connection_remove_filter) \
  X(dbus_connection_set_exit_on_disconnect) \
  X(dbus_g_bus_get) \
  X(dbus_g_bus_get_private) \
  X(dbus_g_connection_get_connection) \
  X(dbus_g_connection_unref) \
  X(dbus_g_thread_init) \
  X(dbus_message_get_interface) \
  X(dbus_message_get_member) \
  X(dbus_message_get_path) \
  X(dbus_message_get_type) \
  X(dbus_message_iter_get_arg_type) \
  X(dbus_message_iter_get_basic) \
  X(dbus_message_iter_init) \
  X(dbus_message_ref) \
  X(dbus_message_unref)

#define LATE_BINDING_SYMBOL_TABLE_CLASS_NAME LIBDBUS_GLIB_CLASS_NAME
#define LATE_BINDING_SYMBOL_TABLE_SYMBOLS_LIST LIBDBUS_GLIB_SYMBOLS_LIST
#include "talk/base/latebindingsymboltable.h.def"

}  // namespace talk_base

#endif  // HAVE_DBUS_GLIB

#endif  // TALK_BASE_LIBDBUSGLIBSYMBOLTABLE_H_
