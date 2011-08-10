/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <gtk/gtk.h>

#include "peerconnection/samples/client/linux/main_wnd.h"

int main(int argc, char* argv[]) {
  gtk_init(&argc, &argv);
  g_type_init();

  GtkMainWnd wnd;
  wnd.Create();
  gtk_main();
  wnd.Destroy();

  // TODO(tommi): Run the Gtk main loop to tear down the connection.
  //while (gtk_events_pending()) {
  //  gtk_main_iteration();
  //}

  return 0;
}

