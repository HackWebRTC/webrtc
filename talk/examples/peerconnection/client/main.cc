/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#include "talk/examples/peerconnection/client/conductor.h"
#include "talk/examples/peerconnection/client/main_wnd.h"
#include "talk/examples/peerconnection/client/peer_connection_client.h"
#include "talk/base/ssladapter.h"
#include "talk/base/win32socketinit.h"
#include "talk/base/win32socketserver.h"


int PASCAL wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* cmd_line, int cmd_show) {
  talk_base::EnsureWinsockInit();
  talk_base::Win32Thread w32_thread;
  talk_base::ThreadManager::Instance()->SetCurrentThread(&w32_thread);

  MainWnd wnd;
  if (!wnd.Create()) {
    ASSERT(false);
    return -1;
  }

  talk_base::InitializeSSL();
  PeerConnectionClient client;
  talk_base::scoped_refptr<Conductor> conductor(
        new talk_base::RefCountedObject<Conductor>(&client, &wnd));

  // Main loop.
  MSG msg;
  BOOL gm;
  while ((gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
    if (!wnd.PreTranslateMessage(&msg)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }

  if (conductor->connection_active() || client.is_connected()) {
    while ((conductor->connection_active() || client.is_connected()) &&
           (gm = ::GetMessage(&msg, NULL, 0, 0)) != 0 && gm != -1) {
      if (!wnd.PreTranslateMessage(&msg)) {
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
      }
    }
  }

  talk_base::CleanupSSL();
  return 0;
}
