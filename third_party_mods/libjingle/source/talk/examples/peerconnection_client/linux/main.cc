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

#include "talk/examples/peerconnection_client/conductor.h"
#include "talk/examples/peerconnection_client/linux/main_wnd.h"
#include "talk/examples/peerconnection_client/peer_connection_client.h"

#include "talk/base/thread.h"

class CustomSocketServer : public talk_base::PhysicalSocketServer {
 public:
  CustomSocketServer(talk_base::Thread* thread, GtkMainWnd* wnd)
      : thread_(thread), wnd_(wnd), conductor_(NULL), client_(NULL) {}
  virtual ~CustomSocketServer() {}
  
  void set_client(PeerConnectionClient* client) { client_ = client; }
  void set_conductor(Conductor* conductor) { conductor_ = conductor; }

  // Override so that we can also pump the GTK message loop.
  virtual bool Wait(int cms, bool process_io) {
    // Pump GTK events.
    // TODO(tommi): We really should move either the socket server or UI to a
    // different thread.  Alternatively we could look at merging the two loops
    // by implementing a dispatcher for the socket server and/or use
    // g_main_context_set_poll_func.
      while (gtk_events_pending())
        gtk_main_iteration();
    
    if (!wnd_->IsWindow() && !conductor_->connection_active() &&
        client_ != NULL && !client_->is_connected()) {
      thread_->Quit();
    }
    return talk_base::PhysicalSocketServer::Wait(0/*cms == -1 ? 1 : cms*/,
                                                 process_io);
  }

 protected:
  talk_base::Thread* thread_;
  GtkMainWnd* wnd_;
  Conductor* conductor_;
  PeerConnectionClient* client_;
};

int main(int argc, char* argv[]) {
  gtk_init(&argc, &argv);
  g_type_init();
  g_thread_init(NULL);

  GtkMainWnd wnd;
  wnd.Create();

  talk_base::AutoThread auto_thread;
  talk_base::Thread* thread = talk_base::Thread::Current();
  CustomSocketServer socket_server(thread, &wnd);
  thread->set_socketserver(&socket_server);

  // Must be constructed after we set the socketserver.
  PeerConnectionClient client;
  Conductor conductor(&client, &wnd);
  socket_server.set_client(&client);
  socket_server.set_conductor(&conductor);

  thread->Run();

  // gtk_main();
  wnd.Destroy();

  thread->set_socketserver(NULL);
  // TODO(tommi): Run the Gtk main loop to tear down the connection.
  //while (gtk_events_pending()) {
  //  gtk_main_iteration();
  //}

  return 0;
}

