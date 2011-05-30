/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <windows.h>

#include "peerconnection/samples/client/conductor.h"
#include "peerconnection/samples/client/main_wnd.h"
#include "peerconnection/samples/client/peer_connection_client.h"
#include "system_wrappers/source/trace_impl.h"
#include "talk/base/win32socketinit.h"


int PASCAL wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* cmd_line, int cmd_show) {
  talk_base::EnsureWinsockInit();

  webrtc::Trace::CreateTrace();
  webrtc::Trace::SetTraceFile("peerconnection_client.log");
  webrtc::Trace::SetLevelFilter(webrtc::kTraceWarning);

  MainWnd wnd;
  if (!wnd.Create()) {
    ASSERT(false);
    return -1;
  }

  PeerConnectionClient client;
  Conductor conductor(&client, &wnd);

  // Main loop.
  MSG msg;
  BOOL gm;
  while ((gm = ::GetMessage(&msg, NULL, 0, 0)) && gm != -1) {
    if (!wnd.PreTranslateMessage(&msg)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }

  if (conductor.connection_active() || client.is_connected()) {
    conductor.Close();
    while ((conductor.connection_active() || client.is_connected()) &&
           (gm = ::GetMessage(&msg, NULL, 0, 0)) && gm != -1) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }

  return 0;
}
