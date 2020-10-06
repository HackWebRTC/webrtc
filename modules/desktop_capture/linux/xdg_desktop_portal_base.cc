/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/desktop_capture/linux/xdg_desktop_portal_base.h"

#include <gio/gunixfdlist.h>

#include <utility>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

const char kDesktopBusName[] = "org.freedesktop.portal.Desktop";
const char kDesktopObjectPath[] = "/org/freedesktop/portal/desktop";
const char kDesktopRequestObjectPath[] =
    "/org/freedesktop/portal/desktop/request";
const char kSessionInterfaceName[] = "org.freedesktop.portal.Session";
const char kRequestInterfaceName[] = "org.freedesktop.portal.Request";
const char kScreenCastInterfaceName[] = "org.freedesktop.portal.ScreenCast";

template <class T>
class Scoped {
 public:
  Scoped() {}
  explicit Scoped(T* val) { ptr_ = val; }
  ~Scoped() { RTC_NOTREACHED(); }

  T* operator->() { return ptr_; }

  bool operator!() { return ptr_ == nullptr; }

  T* get() { return ptr_; }

  T** receive() {
    RTC_CHECK(!ptr_);
    return &ptr_;
  }

  Scoped& operator=(T* val) {
    ptr_ = val;
    return *this;
  }

 protected:
  T* ptr_ = nullptr;
};

template <>
Scoped<GError>::~Scoped() {
  if (ptr_) {
    g_error_free(ptr_);
  }
}

template <>
Scoped<gchar>::~Scoped() {
  if (ptr_) {
    g_free(ptr_);
  }
}

template <>
Scoped<GVariant>::~Scoped() {
  if (ptr_) {
    g_variant_unref(ptr_);
  }
}

template <>
Scoped<GVariantIter>::~Scoped() {
  if (ptr_) {
    g_variant_iter_free(ptr_);
  }
}

template <>
Scoped<GDBusMessage>::~Scoped() {
  if (ptr_) {
    g_object_unref(ptr_);
  }
}

template <>
Scoped<GUnixFDList>::~Scoped() {
  if (ptr_) {
    g_object_unref(ptr_);
  }
}

ConnectionData::ConnectionData(int32_t id) : id_(id) {}

ConnectionData::~ConnectionData() {
  if (start_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_, start_request_signal_id_);
    start_request_signal_id_ = 0;
  }
  if (sources_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         sources_request_signal_id_);
    sources_request_signal_id_ = 0;
  }
  if (session_request_signal_id_) {
    g_dbus_connection_signal_unsubscribe(connection_,
                                         session_request_signal_id_);
    session_request_signal_id_ = 0;
  }

  if (session_handle_) {
    Scoped<GDBusMessage> message(g_dbus_message_new_method_call(
        kDesktopBusName, session_handle_, kSessionInterfaceName, "Close"));
    if (message.get()) {
      Scoped<GError> error;
      g_dbus_connection_send_message(connection_, message.get(),
                                     G_DBUS_SEND_MESSAGE_FLAGS_NONE,
                                     /*out_serial=*/nullptr, error.receive());
      if (error.get()) {
        RTC_LOG(LS_ERROR) << "Failed to close the session: " << error->message;
      }
    }
  }

  g_free(start_handle_);
  start_handle_ = nullptr;
  g_free(sources_handle_);
  sources_handle_ = nullptr;
  g_free(session_handle_);
  session_handle_ = nullptr;
  g_free(portal_handle_);
  portal_handle_ = nullptr;

  if (proxy_) {
    g_clear_object(&proxy_);
    proxy_ = nullptr;
  }

  g_object_unref(connection_);
  connection_ = nullptr;

  // Restore to initial values
  id_ = 0;
  stream_id_ = 0;
  pw_fd_ = -1;
  portal_init_failed_ = false;
}

XdgDesktopPortalBase::XdgDesktopPortalBase() {}

XdgDesktopPortalBase::~XdgDesktopPortalBase() {
  connection_data_map_.clear();
}

// static
rtc::scoped_refptr<XdgDesktopPortalBase> XdgDesktopPortalBase::CreateDefault() {
  return new XdgDesktopPortalBase();
}

void XdgDesktopPortalBase::InitPortal(
    rtc::Callback1<void, bool> callback,
    XdgDesktopPortalBase::CaptureSourceType requested_type,
    int32_t id) {
  if (!id) {
    callback(false);
    return;
  }

  auto data = GetConnectionData(id);

  if (data) {
    data->callbacks_.push_back(callback);
    return;
  }

  std::unique_ptr<ConnectionData> connection_data(new ConnectionData(id));
  connection_data->callbacks_.push_back(callback);
  connection_data->requested_source_type_ = requested_type;

  connection_data_map_.insert({id, std::move(connection_data)});

  g_dbus_proxy_new_for_bus(
      G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, /*info=*/nullptr,
      kDesktopBusName, kDesktopObjectPath, kScreenCastInterfaceName,
      /*cancellable=*/nullptr,
      reinterpret_cast<GAsyncReadyCallback>(OnProxyRequested),
      new UserData(id, this));
}

bool XdgDesktopPortalBase::IsConnectionInitialized(
    const absl::optional<int32_t>& id) const {
  auto connection_data = GetConnectionData(id);

  if (!connection_data) {
    return false;
  }

  if (connection_data->portal_init_failed_) {
    return false;
  }

  if (connection_data->pw_fd_ == -1) {
    return false;
  }

  return true;
}

bool XdgDesktopPortalBase::IsConnectionStreamingOnWeb(
    const absl::optional<int32_t>& id) const {
  auto connection_data = GetConnectionData(id);

  if (!connection_data) {
    return false;
  }

  return connection_data->web_streaming_;
}

PipeWireBase* XdgDesktopPortalBase::GetPipeWireBase(
    const absl::optional<int32_t>& id) const {
  int32_t valid_id = id.value_or(current_connection_id_.value_or(0));

  auto connection_data = GetConnectionData(valid_id);
  RTC_CHECK(connection_data);

  if (!connection_data->pw_base_) {
    return nullptr;
  }

  auto pwBase = connection_data->pw_base_.get();

  // Assume this call/connection has been already used when someone asks for
  // PipeWireBase which we use to guess we already stream to the web page itself
  // and not to the preview dialog
  if (!connection_data->already_used_ && pwBase && pwBase->Frame()) {
    connection_data->already_used_ = true;
  }

  return pwBase;
}

ConnectionData* XdgDesktopPortalBase::GetConnectionData(
    const absl::optional<int32_t>& id) const {
  int32_t valid_id = id.value_or(current_connection_id_.value_or(0));

  auto search = connection_data_map_.find(valid_id);
  if (search != connection_data_map_.end()) {
    return search->second.get();
  }

  return nullptr;
}

absl::optional<int32_t> XdgDesktopPortalBase::CurrentConnectionId() const {
  return current_connection_id_;
}

void XdgDesktopPortalBase::SetCurrentConnectionId(
    const absl::optional<int32_t>& id) {
  current_connection_id_ = id;
}

guint XdgDesktopPortalBase::SetupRequestResponseSignal(
    const gchar* object_path,
    GDBusSignalCallback callback,
    UserData* data) {
  auto connection_data = GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  return g_dbus_connection_signal_subscribe(
      connection_data->connection_, kDesktopBusName, kRequestInterfaceName,
      "Response", object_path, /*arg0=*/nullptr,
      G_DBUS_SIGNAL_FLAGS_NO_MATCH_RULE, callback, data,
      /*user_data_free_func=*/nullptr);
}

// static
void XdgDesktopPortalBase::OnProxyRequested(GObject* /*object*/,
                                            GAsyncResult* result,
                                            gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* portal_base = data->GetXdgDesktopPortalBase();
  RTC_CHECK(portal_base);

  auto connection_data = portal_base->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  Scoped<GError> error;
  connection_data->proxy_ = g_dbus_proxy_new_finish(result, error.receive());
  if (!connection_data->proxy_) {
    RTC_LOG(LS_ERROR) << "Failed to create a proxy for the screen cast portal: "
                      << error->message;
    connection_data->portal_init_failed_ = true;
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    portal_base->CloseConnection(connection_data->id_);
    return;
  }
  connection_data->connection_ =
      g_dbus_proxy_get_connection(connection_data->proxy_);

  RTC_LOG(LS_INFO) << "Created proxy for the screen cast portal.";
  portal_base->SessionRequest(data);
}

// static
gchar* XdgDesktopPortalBase::PrepareSignalHandle(GDBusConnection* connection,
                                                 const gchar* token) {
  Scoped<gchar> sender(
      g_strdup(g_dbus_connection_get_unique_name(connection) + 1));
  for (int i = 0; sender.get()[i]; i++) {
    if (sender.get()[i] == '.') {
      sender.get()[i] = '_';
    }
  }

  gchar* handle = g_strconcat(kDesktopRequestObjectPath, "/", sender.get(), "/",
                              token, /*end of varargs*/ nullptr);

  return handle;
}

void XdgDesktopPortalBase::SessionRequest(UserData* data) {
  auto connection_data = GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  GVariantBuilder builder;
  Scoped<gchar> variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string =
      g_strdup_printf("webrtc_session%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "session_handle_token",
                        g_variant_new_string(variant_string.get()));

  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  connection_data->portal_handle_ =
      PrepareSignalHandle(connection_data->connection_, variant_string.get());
  connection_data->session_request_signal_id_ = SetupRequestResponseSignal(
      connection_data->portal_handle_, OnSessionRequestResponseSignal, data);

  RTC_LOG(LS_INFO) << "Screen cast session requested.";
  g_dbus_proxy_call(connection_data->proxy_, "CreateSession",
                    g_variant_new("(a{sv})", &builder), G_DBUS_CALL_FLAGS_NONE,
                    /*timeout=*/-1, /*cancellable=*/nullptr,
                    reinterpret_cast<GAsyncReadyCallback>(OnSessionRequested),
                    data);
}

// static
void XdgDesktopPortalBase::OnSessionRequested(GDBusConnection* connection,
                                              GAsyncResult* result,
                                              gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* portal_base = data->GetXdgDesktopPortalBase();
  RTC_CHECK(portal_base);

  auto connection_data = portal_base->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  Scoped<GError> error;
  Scoped<GVariant> variant(g_dbus_proxy_call_finish(connection_data->proxy_,
                                                    result, error.receive()));
  if (!variant) {
    RTC_LOG(LS_ERROR) << "Failed to create a screen cast session: "
                      << error->message;
    connection_data->portal_init_failed_ = true;
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    portal_base->CloseConnection(connection_data->id_);
    return;
  }
  RTC_LOG(LS_INFO) << "Initializing the screen cast session.";

  Scoped<gchar> handle;
  g_variant_get_child(variant.get(), 0, "o", handle.receive());
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the screen cast session.";
    if (connection_data->session_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(
          connection, connection_data->session_request_signal_id_);
      connection_data->session_request_signal_id_ = 0;
    }
    connection_data->portal_init_failed_ = true;
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    portal_base->CloseConnection(connection_data->id_);
    return;
  }

  RTC_LOG(LS_INFO) << "Subscribing to the screen cast session.";
}

// static
void XdgDesktopPortalBase::OnSessionRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* portal_base = data->GetXdgDesktopPortalBase();
  RTC_CHECK(portal_base);

  auto connection_data = portal_base->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  RTC_LOG(LS_INFO)
      << "Received response for the screen cast session subscription.";

  guint32 portal_response;
  Scoped<GVariant> response_data;
  g_variant_get(parameters, "(u@a{sv})", &portal_response,
                response_data.receive());
  g_variant_lookup(response_data.get(), "session_handle", "s",
                   &connection_data->session_handle_);

  if (!connection_data->session_handle_ || portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to request the screen cast session subscription.";
    connection_data->portal_init_failed_ = true;
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    return;
  }

  portal_base->SourcesRequest(data);
}

void XdgDesktopPortalBase::SourcesRequest(UserData* data) {
  auto connection_data = GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  GVariantBuilder builder;
  Scoped<gchar> variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  // We want to record monitor content.
  g_variant_builder_add(&builder, "{sv}", "types",
                        g_variant_new_uint32(static_cast<uint32_t>(
                            connection_data->requested_source_type_)));
  // We don't want to allow selection of multiple sources.
  g_variant_builder_add(&builder, "{sv}", "multiple",
                        g_variant_new_boolean(false));
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  connection_data->sources_handle_ =
      PrepareSignalHandle(connection_data->connection_, variant_string.get());
  connection_data->sources_request_signal_id_ = SetupRequestResponseSignal(
      connection_data->sources_handle_, OnSourcesRequestResponseSignal, data);

  RTC_LOG(LS_INFO) << "Requesting sources from the screen cast session.";
  g_dbus_proxy_call(
      connection_data->proxy_, "SelectSources",
      g_variant_new("(oa{sv})", connection_data->session_handle_, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, /*cancellable=*/nullptr,
      reinterpret_cast<GAsyncReadyCallback>(OnSourcesRequested), data);
}

// static
void XdgDesktopPortalBase::OnSourcesRequested(GDBusConnection* connection,
                                              GAsyncResult* result,
                                              gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* portal_base = data->GetXdgDesktopPortalBase();
  RTC_CHECK(portal_base);

  auto connection_data = portal_base->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  Scoped<GError> error;
  Scoped<GVariant> variant(g_dbus_proxy_call_finish(connection_data->proxy_,
                                                    result, error.receive()));
  if (!variant) {
    RTC_LOG(LS_ERROR) << "Failed to request the sources: " << error->message;
    connection_data->portal_init_failed_ = true;
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    portal_base->CloseConnection(connection_data->id_);
    return;
  }

  RTC_LOG(LS_INFO) << "Sources requested from the screen cast session.";

  Scoped<gchar> handle;
  g_variant_get_child(variant.get(), 0, "o", handle.receive());
  if (!handle) {
    RTC_LOG(LS_ERROR) << "Failed to initialize the screen cast session.";
    if (connection_data->sources_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(
          connection, connection_data->sources_request_signal_id_);
      connection_data->sources_request_signal_id_ = 0;
    }
    connection_data->portal_init_failed_ = true;
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    portal_base->CloseConnection(connection_data->id_);
    return;
  }

  RTC_LOG(LS_INFO) << "Subscribed to sources signal.";
}

// static
void XdgDesktopPortalBase::OnSourcesRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* portal_base = data->GetXdgDesktopPortalBase();
  RTC_CHECK(portal_base);

  auto connection_data = portal_base->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  RTC_LOG(LS_INFO) << "Received sources signal from session.";

  guint32 portal_response;
  g_variant_get(parameters, "(u@a{sv})", &portal_response, nullptr);
  if (portal_response) {
    RTC_LOG(LS_ERROR)
        << "Failed to select sources for the screen cast session.";
    connection_data->portal_init_failed_ = true;
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    portal_base->CloseConnection(connection_data->id_);
    return;
  }

  portal_base->StartRequest(data);
}

void XdgDesktopPortalBase::StartRequest(UserData* data) {
  auto connection_data = GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  GVariantBuilder builder;
  Scoped<gchar> variant_string;

  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
  variant_string = g_strdup_printf("webrtc%d", g_random_int_range(0, G_MAXINT));
  g_variant_builder_add(&builder, "{sv}", "handle_token",
                        g_variant_new_string(variant_string.get()));

  connection_data->start_handle_ =
      PrepareSignalHandle(connection_data->connection_, variant_string.get());
  connection_data->start_request_signal_id_ = SetupRequestResponseSignal(
      connection_data->start_handle_, OnStartRequestResponseSignal, data);

  // "Identifier for the application window", this is Wayland, so not "x11:...".
  const gchar parent_window[] = "";

  RTC_LOG(LS_INFO) << "Starting the screen cast session.";
  g_dbus_proxy_call(
      connection_data->proxy_, "Start",
      g_variant_new("(osa{sv})", connection_data->session_handle_,
                    parent_window, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, /*cancellable=*/nullptr,
      reinterpret_cast<GAsyncReadyCallback>(OnStartRequested), data);
}

// static
void XdgDesktopPortalBase::OnStartRequested(GDBusConnection* connection,
                                            GAsyncResult* result,
                                            gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* portal_base = data->GetXdgDesktopPortalBase();
  RTC_CHECK(portal_base);

  auto connection_data = portal_base->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  Scoped<GError> error;
  Scoped<GVariant> variant(g_dbus_proxy_call_finish(connection_data->proxy_,
                                                    result, error.receive()));
  if (!variant) {
    RTC_LOG(LS_ERROR) << "Failed to start the screen cast session: "
                      << error->message;
    connection_data->portal_init_failed_ = true;
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    portal_base->CloseConnection(connection_data->id_);
    return;
  }

  RTC_LOG(LS_INFO) << "Initializing the start of the screen cast session.";

  gchar* handle = nullptr;
  g_variant_get_child(variant.get(), 0, "o", &handle);
  if (!handle) {
    RTC_LOG(LS_ERROR)
        << "Failed to initialize the start of the screen cast session.";
    if (connection_data->start_request_signal_id_) {
      g_dbus_connection_signal_unsubscribe(
          connection, connection_data->start_request_signal_id_);
      connection_data->start_request_signal_id_ = 0;
    }
    connection_data->portal_init_failed_ = true;
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    portal_base->CloseConnection(connection_data->id_);
    return;
  }

  RTC_LOG(LS_INFO) << "Subscribed to the start signal.";
}

// static
void XdgDesktopPortalBase::OnStartRequestResponseSignal(
    GDBusConnection* connection,
    const gchar* sender_name,
    const gchar* object_path,
    const gchar* interface_name,
    const gchar* signal_name,
    GVariant* parameters,
    gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* portal_base = data->GetXdgDesktopPortalBase();
  RTC_CHECK(portal_base);

  auto connection_data = portal_base->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  RTC_LOG(LS_INFO) << "Start signal received.";
  guint32 portal_response;
  Scoped<GVariant> response_data;
  Scoped<GVariantIter> iter;
  g_variant_get(parameters, "(u@a{sv})", &portal_response,
                response_data.receive());
  if (portal_response || !response_data) {
    RTC_LOG(LS_ERROR) << "Failed to start the screen cast session.";
    connection_data->portal_init_failed_ = true;
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    portal_base->CloseConnection(connection_data->id_);
    return;
  }

  // Array of PipeWire streams. See
  // https://github.com/flatpak/xdg-desktop-portal/blob/master/data/org.freedesktop.portal.ScreenCast.xml
  // documentation for <method name="Start">.
  if (g_variant_lookup(response_data.get(), "streams", "a(ua{sv})",
                       iter.receive())) {
    Scoped<GVariant> variant;

    while (g_variant_iter_next(iter.get(), "@(ua{sv})", variant.receive())) {
      guint32 stream_id;
      gint32 width;
      gint32 height;
      guint32 type;
      Scoped<GVariant> options;

      g_variant_get(variant.get(), "(u@a{sv})", &stream_id, options.receive());
      RTC_DCHECK(options.get());

      g_variant_lookup(options.get(), "size", "(ii)", &width, &height);

      if (g_variant_lookup(options.get(), "source_type", "u", &type)) {
        connection_data->capture_source_type_ =
            static_cast<XdgDesktopPortalBase::CaptureSourceType>(type);
      }
      connection_data->desktop_size_.set(width, height);
      connection_data->stream_id_ = stream_id;

      break;
    }
  }

  portal_base->OpenPipeWireRemote(data);
}

void XdgDesktopPortalBase::OpenPipeWireRemote(UserData* data) {
  auto connection_data = GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  if (!connection_data) {
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    CloseConnection(connection_data->id_);
    return;
  }

  GVariantBuilder builder;
  g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);

  RTC_LOG(LS_INFO) << "Opening the PipeWire remote.";

  g_dbus_proxy_call_with_unix_fd_list(
      connection_data->proxy_, "OpenPipeWireRemote",
      g_variant_new("(oa{sv})", connection_data->session_handle_, &builder),
      G_DBUS_CALL_FLAGS_NONE, /*timeout=*/-1, /*fd_list=*/nullptr,
      /*cancellable=*/nullptr,
      reinterpret_cast<GAsyncReadyCallback>(OnOpenPipeWireRemoteRequested),
      data);
}

// static
void XdgDesktopPortalBase::OnOpenPipeWireRemoteRequested(
    GDBusConnection* connection,
    GAsyncResult* result,
    gpointer user_data) {
  UserData* data = static_cast<UserData*>(user_data);
  RTC_DCHECK(data);

  auto* portal_base = data->GetXdgDesktopPortalBase();
  RTC_CHECK(portal_base);

  auto connection_data = portal_base->GetConnectionData(data->GetDataId());
  RTC_CHECK(connection_data);

  Scoped<GError> error;
  Scoped<GUnixFDList> outlist;
  Scoped<GVariant> variant(g_dbus_proxy_call_with_unix_fd_list_finish(
      connection_data->proxy_, outlist.receive(), result, error.receive()));
  if (!variant) {
    RTC_LOG(LS_ERROR) << "Failed to open the PipeWire remote: "
                      << error->message;
    connection_data->portal_init_failed_ = true;
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    portal_base->CloseConnection(connection_data->id_);
    return;
  }

  gint32 index;
  g_variant_get(variant.get(), "(h)", &index);

  if ((connection_data->pw_fd_ =
           g_unix_fd_list_get(outlist.get(), index, error.receive())) == -1) {
    RTC_LOG(LS_ERROR) << "Failed to get file descriptor from the list: "
                      << error->message;
    connection_data->portal_init_failed_ = true;
    for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
      callback(false);
    }
    portal_base->CloseConnection(connection_data->id_);
    return;
  }

  connection_data->pw_base_ =
      std::make_unique<PipeWireBase>(connection_data->pw_fd_);

  for (rtc::Callback1<void, bool>& callback : connection_data->callbacks_) {
    callback(true);
  }
}

void XdgDesktopPortalBase::CloseConnection(const absl::optional<int32_t>& id) {
  auto connection_data = GetConnectionData(id);

  if (!connection_data) {
    return;
  }

  connection_data_map_.erase(connection_data->id_);
}

void XdgDesktopPortalBase::SetConnectionStreamingOnWeb(
    const absl::optional<int32_t>& id) {
  auto connection_data = GetConnectionData(id);

  if (!connection_data) {
    return;
  }

  connection_data->web_streaming_ = true;
}

}  // namespace webrtc
