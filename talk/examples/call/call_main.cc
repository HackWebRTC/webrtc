/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#include <cstdio>
#include <cstring>
#include <time.h>
#include <iomanip>
#include <iostream>
#include <vector>

#include "talk/base/flags.h"
#include "talk/base/logging.h"
#ifdef OSX
#include "talk/base/maccocoasocketserver.h"
#endif
#include "talk/base/pathutils.h"
#include "talk/base/ssladapter.h"
#include "talk/base/stream.h"
#include "talk/base/win32socketserver.h"
#include "talk/examples/call/callclient.h"
#include "talk/examples/call/console.h"
#include "talk/examples/call/mediaenginefactory.h"
#include "talk/p2p/base/constants.h"
#include "talk/session/media/mediasessionclient.h"
#include "talk/session/media/srtpfilter.h"
#include "talk/xmpp/xmppauth.h"
#include "talk/xmpp/xmppclientsettings.h"
#include "talk/xmpp/xmpppump.h"
#include "talk/xmpp/xmppsocket.h"

class DebugLog : public sigslot::has_slots<> {
 public:
  DebugLog() :
    debug_input_buf_(NULL), debug_input_len_(0), debug_input_alloc_(0),
    debug_output_buf_(NULL), debug_output_len_(0), debug_output_alloc_(0),
    censor_password_(false)
      {}
  char * debug_input_buf_;
  int debug_input_len_;
  int debug_input_alloc_;
  char * debug_output_buf_;
  int debug_output_len_;
  int debug_output_alloc_;
  bool censor_password_;

  void Input(const char * data, int len) {
    if (debug_input_len_ + len > debug_input_alloc_) {
      char * old_buf = debug_input_buf_;
      debug_input_alloc_ = 4096;
      while (debug_input_alloc_ < debug_input_len_ + len) {
        debug_input_alloc_ *= 2;
      }
      debug_input_buf_ = new char[debug_input_alloc_];
      memcpy(debug_input_buf_, old_buf, debug_input_len_);
      delete[] old_buf;
    }
    memcpy(debug_input_buf_ + debug_input_len_, data, len);
    debug_input_len_ += len;
    DebugPrint(debug_input_buf_, &debug_input_len_, false);
  }

  void Output(const char * data, int len) {
    if (debug_output_len_ + len > debug_output_alloc_) {
      char * old_buf = debug_output_buf_;
      debug_output_alloc_ = 4096;
      while (debug_output_alloc_ < debug_output_len_ + len) {
        debug_output_alloc_ *= 2;
      }
      debug_output_buf_ = new char[debug_output_alloc_];
      memcpy(debug_output_buf_, old_buf, debug_output_len_);
      delete[] old_buf;
    }
    memcpy(debug_output_buf_ + debug_output_len_, data, len);
    debug_output_len_ += len;
    DebugPrint(debug_output_buf_, &debug_output_len_, true);
  }

  static bool IsAuthTag(const char * str, size_t len) {
    if (str[0] == '<' && str[1] == 'a' &&
                         str[2] == 'u' &&
                         str[3] == 't' &&
                         str[4] == 'h' &&
                         str[5] <= ' ') {
      std::string tag(str, len);

      if (tag.find("mechanism") != std::string::npos)
        return true;
    }
    return false;
  }

  void DebugPrint(char * buf, int * plen, bool output) {
    int len = *plen;
    if (len > 0) {
      time_t tim = time(NULL);
      struct tm * now = localtime(&tim);
      char *time_string = asctime(now);
      if (time_string) {
        size_t time_len = strlen(time_string);
        if (time_len > 0) {
          time_string[time_len-1] = 0;    // trim off terminating \n
        }
      }
      LOG(INFO) << (output ? "SEND >>>>>>>>>>>>>>>>" : "RECV <<<<<<<<<<<<<<<<")
                << " : " << time_string;

      bool indent;
      int start = 0, nest = 3;
      for (int i = 0; i < len; i += 1) {
        if (buf[i] == '>') {
          if ((i > 0) && (buf[i-1] == '/')) {
            indent = false;
          } else if ((start + 1 < len) && (buf[start + 1] == '/')) {
            indent = false;
            nest -= 2;
          } else {
            indent = true;
          }

          // Output a tag
          LOG(INFO) << std::setw(nest) << " "
                    << std::string(buf + start, i + 1 - start);

          if (indent)
            nest += 2;

          // Note if it's a PLAIN auth tag
          if (IsAuthTag(buf + start, i + 1 - start)) {
            censor_password_ = true;
          }

          // incr
          start = i + 1;
        }

        if (buf[i] == '<' && start < i) {
          if (censor_password_) {
            LOG(INFO) << std::setw(nest) << " " << "## TEXT REMOVED ##";
            censor_password_ = false;
          } else {
            LOG(INFO) << std::setw(nest) << " "
                      << std::string(buf + start, i - start);
          }
          start = i;
        }
      }
      len = len - start;
      memcpy(buf, buf + start, len);
      *plen = len;
    }
  }
};

static DebugLog debug_log_;
static const int DEFAULT_PORT = 5222;

#ifdef ANDROID
static std::vector<cricket::AudioCodec> codecs;
static const cricket::AudioCodec ISAC(103, "ISAC", 40000, 16000, 1, 0);

cricket::MediaEngineInterface *CreateAndroidMediaEngine() {
    cricket::FakeMediaEngine *engine = new cricket::FakeMediaEngine();

    codecs.push_back(ISAC);
    engine->SetAudioCodecs(codecs);
    return engine;
}
#endif

// TODO: Move this into Console.
void Print(const char* chars) {
  printf("%s", chars);
  fflush(stdout);
}

bool GetSecurePolicy(const std::string& in, cricket::SecurePolicy* out) {
  if (in == "disable") {
    *out = cricket::SEC_DISABLED;
  } else if (in == "enable") {
    *out = cricket::SEC_ENABLED;
  } else if (in == "require") {
    *out = cricket::SEC_REQUIRED;
  } else {
    return false;
  }
  return true;
}

int main(int argc, char **argv) {
  // This app has three threads. The main thread will run the XMPP client,
  // which will print to the screen in its own thread. A second thread
  // will get input from the console, parse it, and pass the appropriate
  // message back to the XMPP client's thread. A third thread is used
  // by MediaSessionClient as its worker thread.

  // define options
  DEFINE_string(s, "talk.google.com", "The connection server to use.");
  DEFINE_string(tls, "require",
      "Select connection encryption: disable, enable, require.");
  DEFINE_bool(allowplain, false, "Allow plain authentication.");
  DEFINE_bool(testserver, false, "Use test server.");
  DEFINE_string(oauth, "", "OAuth2 access token.");
  DEFINE_bool(a, false, "Turn on auto accept for incoming calls.");
  DEFINE_string(signaling, "hybrid",
      "Initial signaling protocol to use: jingle, gingle, or hybrid.");
  DEFINE_string(transport, "hybrid",
      "Initial transport protocol to use: ice, gice, or hybrid.");
  DEFINE_string(sdes, "enable",
      "Select SDES media encryption: disable, enable, require.");
  DEFINE_string(dtls, "disable",
      "Select DTLS transport encryption: disable, enable, require.");
  DEFINE_int(portallocator, 0, "Filter out unwanted connection types.");
  DEFINE_string(pmuc, "groupchat.google.com", "The persistant muc domain.");
  DEFINE_string(capsnode, "http://code.google.com/p/libjingle/call",
                "Caps node: A URI identifying the app.");
  DEFINE_string(capsver, "0.6",
                "Caps ver: A string identifying the version of the app.");
  DEFINE_string(voiceinput, NULL, "RTP dump file for voice input.");
  DEFINE_string(voiceoutput, NULL, "RTP dump file for voice output.");
  DEFINE_string(videoinput, NULL, "RTP dump file for video input.");
  DEFINE_string(videooutput, NULL, "RTP dump file for video output.");
  DEFINE_bool(render, true, "Renders the video.");
  DEFINE_string(datachannel, "",
                "Enable a data channel, and choose the type: rtp or sctp.");
  DEFINE_bool(d, false, "Turn on debugging.");
  DEFINE_string(log, "", "Turn on debugging to a file.");
  DEFINE_bool(debugsrtp, false, "Enable debugging for srtp.");
  DEFINE_bool(help, false, "Prints this message");
  DEFINE_bool(multisession, false,
              "Enable support for multiple sessions in calls.");
  DEFINE_bool(roster, false,
      "Enable roster messages printed in console.");

  // parse options
  FlagList::SetFlagsFromCommandLine(&argc, argv, true);
  if (FLAG_help) {
    FlagList::Print(NULL, false);
    return 0;
  }

  bool auto_accept = FLAG_a;
  bool debug = FLAG_d;
  std::string log = FLAG_log;
  std::string signaling = FLAG_signaling;
  std::string transport = FLAG_transport;
  bool test_server = FLAG_testserver;
  bool allow_plain = FLAG_allowplain;
  std::string tls = FLAG_tls;
  std::string oauth_token = FLAG_oauth;
  int32 portallocator_flags = FLAG_portallocator;
  std::string pmuc_domain = FLAG_pmuc;
  std::string server = FLAG_s;
  std::string sdes = FLAG_sdes;
  std::string dtls = FLAG_dtls;
  std::string caps_node = FLAG_capsnode;
  std::string caps_ver = FLAG_capsver;
  bool debugsrtp = FLAG_debugsrtp;
  bool render = FLAG_render;
  std::string data_channel = FLAG_datachannel;
  bool multisession_enabled = FLAG_multisession;
  talk_base::SSLIdentity* ssl_identity = NULL;
  bool show_roster_messages = FLAG_roster;

  // Set up debugging.
  if (debug) {
    talk_base::LogMessage::LogToDebug(talk_base::LS_VERBOSE);
  }

  if (!log.empty()) {
    talk_base::StreamInterface* stream =
        talk_base::Filesystem::OpenFile(log, "a");
    if (stream) {
      talk_base::LogMessage::LogToStream(stream, talk_base::LS_VERBOSE);
    } else {
      Print(("Cannot open debug log " + log + "\n").c_str());
      return 1;
    }
  }

  if (debugsrtp) {
    cricket::EnableSrtpDebugging();
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

  if (username.empty()) {
    Print("JID: ");
    std::cin >> username;
  }
  if (username.find('@') == std::string::npos) {
    username.append("@localhost");
  }
  jid = buzz::Jid(username);
  if (!jid.IsValid() || jid.node() == "") {
    Print("Invalid JID. JIDs should be in the form user@domain\n");
    return 1;
  }
  if (pass.password().empty() && !test_server && oauth_token.empty()) {
    Console::SetEcho(false);
    Print("Password: ");
    std::cin >> pass.password();
    Console::SetEcho(true);
    Print("\n");
  }

  // Decide on the connection settings.
  buzz::XmppClientSettings xcs;
  xcs.set_user(jid.node());
  xcs.set_resource("call");
  xcs.set_host(jid.domain());
  xcs.set_allow_plain(allow_plain);

  if (tls == "disable") {
    xcs.set_use_tls(buzz::TLS_DISABLED);
  } else if (tls == "enable") {
    xcs.set_use_tls(buzz::TLS_ENABLED);
  } else if (tls == "require") {
    xcs.set_use_tls(buzz::TLS_REQUIRED);
  } else {
    Print("Invalid TLS option, must be enable, disable, or require.\n");
    return 1;
  }

  if (test_server) {
    pass.password() = jid.node();
    xcs.set_allow_plain(true);
    xcs.set_use_tls(buzz::TLS_DISABLED);
    xcs.set_test_server_domain("google.com");
  }
  xcs.set_pass(talk_base::CryptString(pass));
  if (!oauth_token.empty()) {
    xcs.set_auth_token(buzz::AUTH_MECHANISM_OAUTH2, oauth_token);
  }

  std::string host;
  int port;

  int colon = server.find(':');
  if (colon == -1) {
    host = server;
    port = DEFAULT_PORT;
  } else {
    host = server.substr(0, colon);
    port = atoi(server.substr(colon + 1).c_str());
  }

  xcs.set_server(talk_base::SocketAddress(host, port));

  // Decide on the signaling and crypto settings.
  cricket::SignalingProtocol signaling_protocol = cricket::PROTOCOL_HYBRID;
  if (signaling == "jingle") {
    signaling_protocol = cricket::PROTOCOL_JINGLE;
  } else if (signaling == "gingle") {
    signaling_protocol = cricket::PROTOCOL_GINGLE;
  } else if (signaling == "hybrid") {
    signaling_protocol = cricket::PROTOCOL_HYBRID;
  } else {
    Print("Invalid signaling protocol.  Must be jingle, gingle, or hybrid.\n");
    return 1;
  }

  cricket::TransportProtocol transport_protocol = cricket::ICEPROTO_HYBRID;
  if (transport == "ice") {
    transport_protocol = cricket::ICEPROTO_RFC5245;
  } else if (transport == "gice") {
    transport_protocol = cricket::ICEPROTO_GOOGLE;
  } else if (transport == "hybrid") {
    transport_protocol = cricket::ICEPROTO_HYBRID;
  } else {
    Print("Invalid transport protocol.  Must be ice, gice, or hybrid.\n");
    return 1;
  }

  cricket::DataChannelType data_channel_type = cricket::DCT_NONE;
  if (data_channel == "rtp") {
    data_channel_type = cricket::DCT_RTP;
  } else if (data_channel == "sctp") {
    data_channel_type = cricket::DCT_SCTP;
  } else if (!data_channel.empty()) {
    Print("Invalid data channel type.  Must be rtp or sctp.\n");
    return 1;
  }

  cricket::SecurePolicy sdes_policy, dtls_policy;
  if (!GetSecurePolicy(sdes, &sdes_policy)) {
    Print("Invalid SDES policy. Must be enable, disable, or require.\n");
    return 1;
  }
  if (!GetSecurePolicy(dtls, &dtls_policy)) {
    Print("Invalid DTLS policy. Must be enable, disable, or require.\n");
    return 1;
  }
  if (dtls_policy != cricket::SEC_DISABLED) {
    ssl_identity = talk_base::SSLIdentity::Generate(jid.Str());
    if (!ssl_identity) {
      Print("Failed to generate identity for DTLS.\n");
      return 1;
    }
  }

#ifdef ANDROID
  MediaEngineFactory::SetCreateFunction(&CreateAndroidMediaEngine);
#endif

#if WIN32
  // Need to pump messages on our main thread on Windows.
  talk_base::Win32Thread w32_thread;
  talk_base::ThreadManager::Instance()->SetCurrentThread(&w32_thread);
#endif
  talk_base::Thread* main_thread = talk_base::Thread::Current();
#ifdef OSX
  talk_base::MacCocoaSocketServer ss;
  talk_base::SocketServerScope ss_scope(&ss);
#endif

  buzz::XmppPump pump;
  CallClient *client = new CallClient(pump.client(), caps_node, caps_ver);

  if (FLAG_voiceinput || FLAG_voiceoutput ||
      FLAG_videoinput || FLAG_videooutput) {
    // If any dump file is specified, we use a FileMediaEngine.
    cricket::MediaEngineInterface* engine =
        MediaEngineFactory::CreateFileMediaEngine(
            FLAG_voiceinput, FLAG_voiceoutput,
            FLAG_videoinput, FLAG_videooutput);
    client->SetMediaEngine(engine);
  }

  Console *console = new Console(main_thread, client);
  client->SetConsole(console);
  client->SetAutoAccept(auto_accept);
  client->SetPmucDomain(pmuc_domain);
  client->SetPortAllocatorFlags(portallocator_flags);
  client->SetAllowLocalIps(true);
  client->SetSignalingProtocol(signaling_protocol);
  client->SetTransportProtocol(transport_protocol);
  client->SetSecurePolicy(sdes_policy, dtls_policy);
  client->SetSslIdentity(ssl_identity);
  client->SetRender(render);
  client->SetDataChannelType(data_channel_type);
  client->SetMultiSessionEnabled(multisession_enabled);
  client->SetShowRosterMessages(show_roster_messages);
  console->Start();

  if (debug) {
    pump.client()->SignalLogInput.connect(&debug_log_, &DebugLog::Input);
    pump.client()->SignalLogOutput.connect(&debug_log_, &DebugLog::Output);
  }

  Print(("Logging in to " + server + " as " + jid.Str() + "\n").c_str());
  pump.DoLogin(xcs, new buzz::XmppSocket(buzz::TLS_REQUIRED), new XmppAuth());
  main_thread->Run();
  pump.DoDisconnect();

  console->Stop();
  delete console;
  delete client;

  return 0;
}
