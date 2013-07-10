/*
 * libjingle
 * Copyright 2004--2013, Google Inc.
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

//
// A simple text chat application, largely copied from examples/call.
//

#include <iostream>

#include "talk/base/logging.h"
#include "talk/base/ssladapter.h"

#ifdef OSX
#include "talk/base/maccocoasocketserver.h"
#elif defined(WIN32)
#include "talk/base/win32socketserver.h"
#else
#include "talk/base/physicalsocketserver.h"
#endif

#include "talk/xmpp/constants.h"
#include "talk/xmpp/xmppauth.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmpppump.h"
#include "talk/xmpp/xmppsocket.h"

#include "talk/examples/chat/chatapp.h"
#include "talk/examples/chat/consoletask.h"

static const int kDefaultPort = 5222;

int main(int argc, char* argv[]) {
  // TODO(pmclean): Remove duplication of code with examples/call.
  // Set up debugging.
  bool debug = true;
  if (debug) {
    talk_base::LogMessage::LogToDebug(talk_base::LS_VERBOSE);
  }

  // Set up the crypto subsystem.
  talk_base::InitializeSSL();

  // Parse username and password, if present.
  buzz::Jid jid;
  std::string username;
  talk_base::InsecureCryptStringImpl pass;
  if (argc > 1) {
    username = argv[1];
    if (argc > 2) {
      pass.password() = argv[2];
    }
  }

  // ... else prompt for them
  if (username.empty()) {
    printf("JID: ");
    std::cin >> username;
  }
  if (username.find('@') == std::string::npos) {
    username.append("@localhost");
  }

  jid = buzz::Jid(username);
  if (!jid.IsValid() || jid.node() == "") {
    printf("Invalid JID. JIDs should be in the form user@domain\n");
    return 1;
  }

  if (pass.password().empty()) {
    buzz::ConsoleTask::SetEcho(false);
    printf("Password: ");
    std::cin >> pass.password();
    buzz::ConsoleTask::SetEcho(true);
    printf("\n");
  }

  // OTP (this can be skipped)
  std::string otp_token;
  printf("OTP: ");
  fflush(stdin);
  std::getline(std::cin, otp_token);

  // Setup the connection settings.
  buzz::XmppClientSettings xcs;
  xcs.set_user(jid.node());
  xcs.set_resource("chat");
  xcs.set_host(jid.domain());
  bool allow_plain = false;
  xcs.set_allow_plain(allow_plain);
  xcs.set_use_tls(buzz::TLS_REQUIRED);
  xcs.set_pass(talk_base::CryptString(pass));
  if (!otp_token.empty() && *otp_token.c_str() != '\n') {
    xcs.set_auth_token(buzz::AUTH_MECHANISM_OAUTH2, otp_token);
  }

  // Build the server spec
  std::string host;
  int port;

  std::string server = "talk.google.com";
  int colon = server.find(':');
  if (colon == -1) {
    host = server;
    port = kDefaultPort;
  } else {
    host = server.substr(0, colon);
    port = atoi(server.substr(colon + 1).c_str());
  }
  xcs.set_server(talk_base::SocketAddress(host, port));

  talk_base::Thread* main_thread = talk_base::Thread::Current();
#if WIN32
  // Need to pump messages on our main thread on Windows.
  talk_base::Win32Thread w32_thread;
  talk_base::ThreadManager::Instance()->SetCurrentThread(&w32_thread);
#elif defined(OSX)
  talk_base::MacCocoaSocketServer ss;
  talk_base::SocketServerScope ss_scope(&ss);
#else
  talk_base::PhysicalSocketServer ss;
#endif

  buzz::XmppPump* pump = new buzz::XmppPump();
  ChatApp *client = new ChatApp(pump->client(), main_thread);

  // Start pumping messages!
  pump->DoLogin(xcs, new buzz::XmppSocket(buzz::TLS_REQUIRED), new XmppAuth());

  main_thread->Run();
  pump->DoDisconnect();

  delete client;

  return 0;
}
