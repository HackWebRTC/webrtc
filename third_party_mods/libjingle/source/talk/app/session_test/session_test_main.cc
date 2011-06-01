// Copyright 2011 Google Inc. All Rights Reserved.
// Author: tommi@google.com (Tomas Gunnarsson)

// This may not look like much but it has already uncovered several issues.
// In the future this will be a p2p reference app for the webrtc API along
// with a separate simple server implementation.

#include "talk/base/win32.h"  // Must be first

#include <map>

#include "talk/base/scoped_ptr.h"
#include "talk/base/win32socketinit.cc"
#include "talk/base/win32socketserver.h"  // For Win32Socket
#include "talk/base/win32socketserver.cc"  // For Win32Socket

#include "modules/audio_device/main/interface/audio_device.h"
#include "modules/video_capture/main/interface/video_capture.h"
#include "system_wrappers/source/trace_impl.h"
#include "talk/app/peerconnection.h"
#include "talk/app/session_test/main_wnd.h"
#include "talk/base/logging.h"
#include "talk/session/phone/videorendererfactory.h"

static const char kAudioLabel[] = "audio_label";
static const char kVideoLabel[] = "video_label";
const unsigned short kDefaultServerPort = 8888;

using talk_base::scoped_ptr;
using webrtc::AudioDeviceModule;
using webrtc::PeerConnection;
using webrtc::PeerConnectionObserver;

std::string GetEnvVarOrDefault(const char* env_var_name,
                               const char* default_value) {
  std::string value;
  const char* env_var = getenv(env_var_name);
  if (env_var)
    value = env_var;

  if (value.empty())
    value = default_value;

  return value;
}

std::string GetPeerConnectionString() {
  return GetEnvVarOrDefault("WEBRTC_CONNECT", "STUN stun.l.google.com:19302");
}

std::string GetDefaultServerName() {
  return GetEnvVarOrDefault("WEBRTC_SERVER", "localhost");
}

std::string GetPeerName() {
  char computer_name[MAX_PATH] = {0}, user_name[MAX_PATH] = {0};
  DWORD size = ARRAYSIZE(computer_name);
  ::GetComputerNameA(computer_name, &size);
  size = ARRAYSIZE(user_name);
  ::GetUserNameA(user_name, &size);
  std::string ret(user_name);
  ret += '@';
  ret += computer_name;
  return ret;
}

struct PeerConnectionClientObserver {
  virtual void OnSignedIn() = 0;  // Called when we're "logged" on.
  virtual void OnDisconnected() = 0;
  virtual void OnPeerConnected(int id, const std::string& name) = 0;
  virtual void OnPeerDisconnected(int id, const std::string& name) = 0;
  virtual void OnMessageFromPeer(int peer_id, const std::string& message) = 0;
};

class PeerConnectionClient : public sigslot::has_slots<> {
 public:
  enum State {
    NOT_CONNECTED,
    SIGNING_IN,
    CONNECTED,
    SIGNING_OUT_WAITING,
    SIGNING_OUT,
  };

  PeerConnectionClient() : callback_(NULL), my_id_(-1), state_(NOT_CONNECTED) {
    control_socket_.SignalCloseEvent.connect(this,
        &PeerConnectionClient::OnClose);
    hanging_get_.SignalCloseEvent.connect(this,
        &PeerConnectionClient::OnClose);
    control_socket_.SignalConnectEvent.connect(this,
        &PeerConnectionClient::OnConnect);
    hanging_get_.SignalConnectEvent.connect(this,
        &PeerConnectionClient::OnHangingGetConnect);
    control_socket_.SignalReadEvent.connect(this,
        &PeerConnectionClient::OnRead);
    hanging_get_.SignalReadEvent.connect(this,
        &PeerConnectionClient::OnHangingGetRead);
  }

  ~PeerConnectionClient() {
  }

  int id() const {
    return my_id_;
  }

  bool is_connected() const {
    return my_id_ != -1;
  }

  const Peers& peers() const {
    return peers_;
  }

  void RegisterObserver(PeerConnectionClientObserver* callback) {
    ASSERT(!callback_);
    callback_ = callback;
  }

  bool Connect(const std::string& server, int port,
               const std::string& client_name) {
    ASSERT(!server.empty());
    ASSERT(!client_name.empty());
    ASSERT(state_ == NOT_CONNECTED);

    if (server.empty() || client_name.empty())
      return false;

    if (port <= 0)
      port = kDefaultServerPort;

    server_address_.SetIP(server);
    server_address_.SetPort(port);

    if (server_address_.IsUnresolved()) {
      hostent* h = gethostbyname(server_address_.IPAsString().c_str());
      if (!h) {
        LOG(LS_ERROR) << "Failed to resolve host name: "
                      << server_address_.IPAsString();
        return false;
      } else {
        server_address_.SetResolvedIP(
            ntohl(*reinterpret_cast<uint32*>(h->h_addr_list[0])));
      }
    }

    char buffer[1024];
    wsprintfA(buffer, "GET /sign_in?%s HTTP/1.0\r\n\r\n", client_name.c_str());
    onconnect_data_ = buffer;

    bool ret = ConnectControlSocket();
    if (ret)
      state_ = SIGNING_IN;

    return ret;
  }

  bool SendToPeer(int peer_id, const std::string& message) {
    if (state_ != CONNECTED)
      return false;

    ASSERT(is_connected());
    ASSERT(control_socket_.GetState() == talk_base::Socket::CS_CLOSED);
    if (!is_connected() || peer_id == -1)
      return false;

    char headers[1024];
    wsprintfA(headers, "POST /message?peer_id=%i&to=%i HTTP/1.0\r\n"
                       "Content-Length: %i\r\n"
                       "Content-Type: text/plain\r\n"
                       "\r\n",
        my_id_, peer_id, message.length());
    onconnect_data_ = headers;
    onconnect_data_ += message;
    return ConnectControlSocket();
  }

  bool SignOut() {
    if (state_ == NOT_CONNECTED || state_ == SIGNING_OUT)
      return true;

    if (hanging_get_.GetState() != talk_base::Socket::CS_CLOSED)
      hanging_get_.Close();

    if (control_socket_.GetState() == talk_base::Socket::CS_CLOSED) {
      ASSERT(my_id_ != -1);
      state_ = SIGNING_OUT;

      char buffer[1024];
      wsprintfA(buffer, "GET /sign_out?peer_id=%i HTTP/1.0\r\n\r\n", my_id_);
      onconnect_data_ = buffer;
      return ConnectControlSocket();
    } else {
      state_ = SIGNING_OUT_WAITING;
    }

    return true;
  }

 protected:
  void Close() {
    control_socket_.Close();
    hanging_get_.Close();
    onconnect_data_.clear();
    peers_.clear();
    my_id_ = -1;
    state_ = NOT_CONNECTED;
  }

  bool ConnectControlSocket() {
    ASSERT(control_socket_.GetState() == talk_base::Socket::CS_CLOSED);
    int err = control_socket_.Connect(server_address_);
    if (err == SOCKET_ERROR) {
      Close();
      return false;
    }
    return true;
  }

  void OnConnect(talk_base::AsyncSocket* socket) {
    ASSERT(!onconnect_data_.empty());
    int sent = socket->Send(onconnect_data_.c_str(), onconnect_data_.length());
    ASSERT(sent == onconnect_data_.length());
    onconnect_data_.clear();
  }

  void OnHangingGetConnect(talk_base::AsyncSocket* socket) {
    char buffer[1024];
    wsprintfA(buffer, "GET /wait?peer_id=%i HTTP/1.0\r\n\r\n", my_id_);
    int len = lstrlenA(buffer);
    int sent = socket->Send(buffer, len);
    ASSERT(sent == len);    
  }

  // Quick and dirty support for parsing HTTP header values.
  bool GetHeaderValue(const std::string& data, size_t eoh,
                      const char* header_pattern, size_t* value) {
    ASSERT(value);
    size_t found = data.find(header_pattern);
    if (found != std::string::npos && found < eoh) {
      *value = atoi(&data[found + lstrlenA(header_pattern)]);
      return true;
    }
    return false;
  }

  bool GetHeaderValue(const std::string& data, size_t eoh,
                      const char* header_pattern, std::string* value) {
    ASSERT(value);
    size_t found = data.find(header_pattern);
    if (found != std::string::npos && found < eoh) {
      size_t begin = found + lstrlenA(header_pattern);
      size_t end = data.find("\r\n", begin);
      if (end == std::string::npos)
        end = eoh;
      value->assign(data.substr(begin, end - begin));
      return true;
    }
    return false;
  }

  // Returns true if the whole response has been read.
  bool ReadIntoBuffer(talk_base::AsyncSocket* socket, std::string* data,
                      size_t* content_length) {
    LOG(INFO) << __FUNCTION__;

    char buffer[0xffff];
    do {
      int bytes = socket->Recv(buffer, sizeof(buffer));
      if (bytes <= 0)
        break;
      data->append(buffer, bytes);
    } while (true);

    bool ret = false;
    size_t i = data->find("\r\n\r\n");
    if (i != std::string::npos) {
      LOG(INFO) << "Headers received";
      const char kContentLengthHeader[] = "\r\nContent-Length: ";
      if (GetHeaderValue(*data, i, "\r\nContent-Length: ", content_length)) {
        LOG(INFO) << "Expecting " << *content_length << " bytes.";
        size_t total_response_size = (i + 4) + *content_length;
        if (data->length() >= total_response_size) {
          ret = true;
          std::string should_close;
          const char kConnection[] = "\r\nConnection: ";
          if (GetHeaderValue(*data, i, kConnection, &should_close) &&
              should_close.compare("close") == 0) {
            socket->Close();
          }
        } else {
          // We haven't received everything.  Just continue to accept data.
        }
      } else {
        LOG(LS_ERROR) << "No content length field specified by the server.";
      }
    }
    return ret;
  }

  void OnRead(talk_base::AsyncSocket* socket) {
    LOG(INFO) << __FUNCTION__;
    size_t content_length = 0;
    if (ReadIntoBuffer(socket, &control_data_, &content_length)) {
      size_t peer_id = 0, eoh = 0;
      bool ok = ParseServerResponse(control_data_, content_length, &peer_id,
                                    &eoh);
      if (ok) {
        if (my_id_ == -1) {
          // First response.  Let's store our server assigned ID.
          ASSERT(state_ == SIGNING_IN);
          my_id_ = peer_id;
          ASSERT(my_id_ != -1);

          // The body of the response will be a list of already connected peers.
          if (content_length) {
            size_t pos = eoh + 4;
            while (pos < control_data_.size()) {
              size_t eol = control_data_.find('\n', pos);
              if (eol == std::string::npos)
                break;
              int id = 0;
              std::string name;
              bool connected;
              if (ParseEntry(control_data_.substr(pos, eol - pos), &name, &id,
                             &connected) && id != my_id_) {
                peers_[id] = name;
                callback_->OnPeerConnected(id, name);
              }
              pos = eol + 1;
            }
          }
          ASSERT(is_connected());
          callback_->OnSignedIn();
        } else if (state_ == SIGNING_OUT) {
          Close();
          callback_->OnDisconnected();
        } else if (state_ == SIGNING_OUT_WAITING) {
          SignOut();
        }
      }

      control_data_.clear();

      if (state_ == SIGNING_IN) {
        ASSERT(hanging_get_.GetState() == talk_base::Socket::CS_CLOSED);
        state_ = CONNECTED;
        hanging_get_.Connect(server_address_);
      }
    }
  }

  void OnHangingGetRead(talk_base::AsyncSocket* socket) {
    LOG(INFO) << __FUNCTION__;
    size_t content_length = 0;
    if (ReadIntoBuffer(socket, &notification_data_, &content_length)) {
      size_t peer_id = 0, eoh = 0;
      bool ok = ParseServerResponse(notification_data_, content_length,
                                    &peer_id, &eoh);

      if (ok) {
        // Store the position where the body begins.
        size_t pos = eoh + 4;

        if (my_id_ == peer_id) {
          // A notification about a new member or a member that just
          // disconnected.
          int id = 0;
          std::string name;
          bool connected = false;
          if (ParseEntry(notification_data_.substr(pos), &name, &id,
                         &connected)) {
            if (connected) {
              peers_[id] = name;
              callback_->OnPeerConnected(id, name);
            } else {
              peers_.erase(id);
              callback_->OnPeerDisconnected(id, name);
            }
          }
        } else {
          callback_->OnMessageFromPeer(peer_id,
                                       notification_data_.substr(pos));
        }
      }

      notification_data_.clear();
    }

    if (hanging_get_.GetState() == talk_base::Socket::CS_CLOSED &&
        state_ == CONNECTED) {
      hanging_get_.Connect(server_address_);
    }
  }

  // Parses a single line entry in the form "<name>,<id>,<connected>"
  bool ParseEntry(const std::string& entry, std::string* name, int* id,
                  bool* connected) {
    ASSERT(name);
    ASSERT(id);
    ASSERT(connected);
    ASSERT(entry.length());

    *connected = false;
    size_t separator = entry.find(',');
    if (separator != std::string::npos) {
      *id = atoi(&entry[separator + 1]);
      name->assign(entry.substr(0, separator));
      separator = entry.find(',', separator + 1);
      if (separator != std::string::npos) {
        *connected = atoi(&entry[separator + 1]) ? true : false;
      }
    }
    return !name->empty();
  }

  int GetResponseStatus(const std::string& response) {
    int status = -1;
    size_t pos = response.find(' ');
    if (pos != std::string::npos)
      status = atoi(&response[pos + 1]);
    return status;
  }

  bool ParseServerResponse(const std::string& response, size_t content_length,
                           size_t* peer_id, size_t* eoh) {
    LOG(INFO) << response;

    int status = GetResponseStatus(response.c_str());
    if (status != 200) {
      LOG(LS_ERROR) << "Received error from server";
      Close();
      callback_->OnDisconnected();
      return false;
    }

    *eoh = response.find("\r\n\r\n");
    ASSERT(*eoh != std::string::npos);
    if (*eoh == std::string::npos)
      return false;

    *peer_id = -1;

    // See comment in peer_channel.cc for why we use the Pragma header and
    // not e.g. "X-Peer-Id".
    GetHeaderValue(response, *eoh, "\r\nPragma: ", peer_id);

    return true;
  }

  void OnClose(talk_base::AsyncSocket* socket, int err) {
    LOG(INFO) << __FUNCTION__;

    socket->Close();

    if (err != WSAECONNREFUSED) {
      if (socket == &hanging_get_) {
        if (state_ == CONNECTED) {
          LOG(INFO) << "Issuing  a new hanging get";
          hanging_get_.Close();
          hanging_get_.Connect(server_address_);
        }
      }
    } else {
      // Failed to connect to the server.
      Close();
      callback_->OnDisconnected();
    }
  }

  PeerConnectionClientObserver* callback_;
  talk_base::SocketAddress server_address_;
  talk_base::Win32Socket control_socket_;
  talk_base::Win32Socket hanging_get_;
  std::string onconnect_data_;
  std::string control_data_;
  std::string notification_data_;
  Peers peers_;
  State state_;
  int my_id_;
};

class ConnectionObserver
  : public PeerConnectionObserver,
    public PeerConnectionClientObserver,
    public MainWndCallback,
    public talk_base::Win32Window {
 public:
  enum WindowMessages {
    MEDIA_CHANNELS_INITIALIZED = WM_APP + 1,
    PEER_CONNECTION_CLOSED,
    SEND_MESSAGE_TO_PEER,
  };

  enum HandshakeState {
    NONE,
    INITIATOR,
    ANSWER_RECEIVED,
    OFFER_RECEIVED,
    QUIT_SENT,
  };

  ConnectionObserver(PeerConnectionClient* client,
                     MainWnd* main_wnd)
    : handshake_(NONE),
      waiting_for_audio_(false),
      waiting_for_video_(false),
      peer_id_(-1),
      video_channel_(-1),
      audio_channel_(-1),
      client_(client),
      main_wnd_(main_wnd) {
    // Create a window for posting notifications back to from other threads.
    bool ok = Create(HWND_MESSAGE, L"ConnectionObserver", 0, 0, 0, 0, 0, 0);
    ASSERT(ok);
    client_->RegisterObserver(this);
    main_wnd->RegisterObserver(this);
  }

  ~ConnectionObserver() {
    ASSERT(peer_connection_.get() == NULL);
    Destroy();
    DeletePeerConnection();
  }

  bool has_video() const {
    return video_channel_ != -1;
  }

  bool has_audio() const {
    return audio_channel_ != -1;
  }

  bool connection_active() const {
    return peer_connection_.get() != NULL;
  }

  void Close() {
    if (peer_connection_.get()) {
      peer_connection_->Close();
    } else {
      client_->SignOut();
    }
  }

 protected:
  bool InitializePeerConnection() {
    ASSERT(peer_connection_.get() == NULL);
    peer_connection_.reset(new PeerConnection(GetPeerConnectionString()));
    peer_connection_->RegisterObserver(this);
    if (!peer_connection_->Init()) {
      DeletePeerConnection();
    } else {
      bool audio = peer_connection_->SetAudioDevice("", "", 0);
      LOG(INFO) << "SetAudioDevice " << (audio ? "succeeded." : "failed.");
    }
    return peer_connection_.get() != NULL;
  }

  void DeletePeerConnection() {
    peer_connection_.reset();
    handshake_ = NONE;
  }

  void StartCaptureDevice() {
    ASSERT(peer_connection_.get());
    if (main_wnd_->IsWindow()) {
      main_wnd_->SwitchToStreamingUI();

      if (peer_connection_->SetVideoCapture("")) {
        if (!local_renderer_.get()) {
          local_renderer_.reset(
              cricket::VideoRendererFactory::CreateGuiVideoRenderer(176, 144));
        }
        peer_connection_->SetLocalVideoRenderer(local_renderer_.get());
      } else {
        ASSERT(false);
      }
    }
  }

  //
  // PeerConnectionObserver implementation.
  //

  virtual void OnError() {
    LOG(INFO) << __FUNCTION__;
    ASSERT(false);
  }

  virtual void OnSignalingMessage(const std::string& msg) {
    LOG(INFO) << __FUNCTION__;

    bool shutting_down = (video_channel_ == -1 && audio_channel_ == -1);

    if (handshake_ == OFFER_RECEIVED && !shutting_down)
      StartCaptureDevice();

    // Send our answer/offer/shutting down message.
    // If we're the initiator, this will be our offer.  If we just received
    // an offer, this will be an answer.  If PeerConnection::Close has been
    // called, then this is our signal to the other end that we're shutting
    // down.
    if (handshake_ != QUIT_SENT) {
      SendMessage(handle(), SEND_MESSAGE_TO_PEER, 0,
                  reinterpret_cast<LPARAM>(&msg));
    }

    if (shutting_down) {
      handshake_ = QUIT_SENT;
      PostMessage(handle(), PEER_CONNECTION_CLOSED, 0, 0);
    }
  }

  // Called when a remote stream is added
  virtual void OnAddStream(const std::string& stream_id, int channel_id,
                           bool video) {
    LOG(INFO) << __FUNCTION__ << " " << stream_id;
    bool send_notification = (waiting_for_video_ || waiting_for_audio_);
    if (video) {
      ASSERT(video_channel_ == -1);
      video_channel_ = channel_id;
      waiting_for_video_ = false;
      LOG(INFO) << "Setting video renderer for channel: " << channel_id;
      if (!remote_renderer_.get()) {
          remote_renderer_.reset(
              cricket::VideoRendererFactory::CreateGuiVideoRenderer(352, 288));
      }
      bool ok = peer_connection_->SetVideoRenderer(stream_id,
                                                   remote_renderer_.get());
      ASSERT(ok);
    } else {
      ASSERT(audio_channel_ == -1);
      audio_channel_ = channel_id;
      waiting_for_audio_ = false;
    }

    if (send_notification && !waiting_for_audio_ && !waiting_for_video_)
      PostMessage(handle(), MEDIA_CHANNELS_INITIALIZED, 0, 0);
  }

  virtual void OnRemoveStream(const std::string& stream_id,
                              int channel_id,
                              bool video) {
    LOG(INFO) << __FUNCTION__;
    if (video) {
      ASSERT(channel_id == video_channel_);
      video_channel_ = -1;
    } else {
      ASSERT(channel_id == audio_channel_);
      audio_channel_ = -1;
    }
  }

  //
  // PeerConnectionClientObserver implementation.
  //

  virtual void OnSignedIn() {
    LOG(INFO) << __FUNCTION__;
    main_wnd_->SwitchToPeerList(client_->peers());
  }

  virtual void OnDisconnected() {
    LOG(INFO) << __FUNCTION__;
    if (peer_connection_.get()) {
      peer_connection_->Close();
    } else if (main_wnd_->IsWindow()) {
      main_wnd_->SwitchToConnectUI();
    }
  }

  virtual void OnPeerConnected(int id, const std::string& name) {
    LOG(INFO) << __FUNCTION__;
    // Refresh the list if we're showing it.
    if (main_wnd_->current_ui() == MainWnd::LIST_PEERS)
      main_wnd_->SwitchToPeerList(client_->peers());
  }

  virtual void OnPeerDisconnected(int id, const std::string& name) {
    LOG(INFO) << __FUNCTION__;
    if (id == peer_id_) {
      LOG(INFO) << "Our peer disconnected";
      peer_id_ = -1;
      // TODO: Somehow make sure that Close has been called?
      if (peer_connection_.get())
        peer_connection_->Close();
    }

    // Refresh the list if we're showing it.
    if (main_wnd_->current_ui() == MainWnd::LIST_PEERS)
      main_wnd_->SwitchToPeerList(client_->peers());
  }

  virtual void OnMessageFromPeer(int peer_id, const std::string& message) {
    ASSERT(peer_id_ == peer_id || peer_id_ == -1);

    if (handshake_ == NONE) {
      handshake_ = OFFER_RECEIVED;
      peer_id_ = peer_id;
      if (!peer_connection_.get()) {
        // Got an offer.  Give it to the PeerConnection instance.
        // Once processed, we will get a callback to OnSignalingMessage with
        // our 'answer' which we'll send to the peer.
        LOG(INFO) << "Got an offer from our peer: " << peer_id;
        if (!InitializePeerConnection()) {
          LOG(LS_ERROR) << "Failed to initialize our PeerConnection instance";
          client_->SignOut();
          return;
        }
      }
    } else if (handshake_ == INITIATOR) {
      LOG(INFO) << "Remote peer sent us an answer";
      handshake_ = ANSWER_RECEIVED;
    } else {
      LOG(INFO) << "Remote peer is disconnecting";
      handshake_ = QUIT_SENT;
    }

    peer_connection_->SignalingMessage(message);

    if (handshake_ == QUIT_SENT) {
      DisconnectFromCurrentPeer();
    }
  }

  //
  // MainWndCallback implementation.
  //
  virtual void StartLogin(const std::string& server, int port) {
    ASSERT(!client_->is_connected());
    if (!client_->Connect(server, port, GetPeerName())) {
      MessageBoxA(main_wnd_->handle(),
          ("Failed to connect to " + server).c_str(),
          "Error", MB_OK | MB_ICONERROR);
    }
  }

  virtual void DisconnectFromServer() {
    if (!client_->is_connected())
      return;
    client_->SignOut();
  }

  virtual void ConnectToPeer(int peer_id) {
    ASSERT(peer_id_ == -1);
    ASSERT(peer_id != -1);
    ASSERT(handshake_ == NONE);

    if (handshake_ != NONE)
      return;

    if (InitializePeerConnection()) {
      peer_id_ = peer_id;
      waiting_for_video_ = peer_connection_->AddStream(kVideoLabel, true);
      waiting_for_audio_ = peer_connection_->AddStream(kAudioLabel, false);
      if (waiting_for_video_ || waiting_for_audio_)
        handshake_ = INITIATOR;
      ASSERT(waiting_for_video_ || waiting_for_audio_);
    }

    if (handshake_ == NONE) {
      ::MessageBoxA(main_wnd_->handle(), "Failed to initialize PeerConnection",
                    "Error", MB_OK | MB_ICONERROR);
    }
  }

  virtual void DisconnectFromCurrentPeer() {
    if (peer_connection_.get())
      peer_connection_->Close();
  }

  //
  // Win32Window implementation.
  //

  virtual bool OnMessage(UINT msg, WPARAM wp, LPARAM lp, LRESULT& result) {
    bool ret = true;
    if (msg == MEDIA_CHANNELS_INITIALIZED) {
        ASSERT(handshake_ == INITIATOR);
        bool ok = peer_connection_->Connect();
        ASSERT(ok);
        StartCaptureDevice();
        // When we get an OnSignalingMessage notification, we'll send our
        // json encoded signaling message to the peer, which is the first step
        // of establishing a connection.
    } else if (msg == PEER_CONNECTION_CLOSED) {
      LOG(INFO) << "PEER_CONNECTION_CLOSED";
      DeletePeerConnection();
      ::InvalidateRect(main_wnd_->handle(), NULL, TRUE);
      waiting_for_audio_ = false;
      waiting_for_video_ = false;
      peer_id_ = -1;
      ASSERT(video_channel_ == -1);
      ASSERT(audio_channel_ == -1);
      if (main_wnd_->IsWindow()) {
        if (client_->is_connected()) {
          main_wnd_->SwitchToPeerList(client_->peers());
        } else {
          main_wnd_->SwitchToConnectUI();
        }
      } else {
        DisconnectFromServer();
      }
    } else if (msg == SEND_MESSAGE_TO_PEER) {
      bool ok = client_->SendToPeer(peer_id_,
                                    *reinterpret_cast<std::string*>(lp));
      if (!ok) {
        LOG(LS_ERROR) << "SendToPeer failed";
        DisconnectFromServer();
      }
    } else {
      ret = false;
    }

    return ret;
  }

 protected:
  HandshakeState handshake_;
  bool waiting_for_audio_;
  bool waiting_for_video_;
  int peer_id_;
  scoped_ptr<PeerConnection> peer_connection_;
  PeerConnectionClient* client_;
  MainWnd* main_wnd_;
  int video_channel_;
  int audio_channel_;
  scoped_ptr<cricket::VideoRenderer> local_renderer_;
  scoped_ptr<cricket::VideoRenderer> remote_renderer_;
};

int PASCAL wWinMain(HINSTANCE instance, HINSTANCE prev_instance,
                    wchar_t* cmd_line, int cmd_show) {
  talk_base::EnsureWinsockInit();

  webrtc::Trace::CreateTrace();
  webrtc::Trace::SetTraceFile("session_test_trace.txt");
  webrtc::Trace::SetLevelFilter(webrtc::kTraceWarning);

  MainWnd wnd;
  if (!wnd.Create()) {
    ASSERT(false);
    return -1;
  }

  PeerConnectionClient client;
  ConnectionObserver observer(&client, &wnd);

  // Main loop.
  MSG msg;
  BOOL gm;
  while ((gm = ::GetMessage(&msg, NULL, 0, 0)) && gm != -1) {
    if (!wnd.PreTranslateMessage(&msg)) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }

  if (observer.connection_active() || client.is_connected()) {
    observer.Close();
    while ((observer.connection_active() || client.is_connected()) &&
           (gm = ::GetMessage(&msg, NULL, 0, 0)) && gm != -1) {
      ::TranslateMessage(&msg);
      ::DispatchMessage(&msg);
    }
  }

  return 0;
}
