/*
 *  Copyright 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MODULES_DESKTOP_CAPTURE_LINUX_XDG_DESKTOP_PORTAL_BASE_H_
#define MODULES_DESKTOP_CAPTURE_LINUX_XDG_DESKTOP_PORTAL_BASE_H_

#include <gio/gio.h>

#include <map>
#include <memory>
#include <vector>

#include "modules/desktop_capture/desktop_geometry.h"
#include "modules/desktop_capture/linux/pipewire_base.h"

#include "api/ref_counted_base.h"
#include "api/scoped_refptr.h"
#include "rtc_base/callback.h"
#include "rtc_base/constructor_magic.h"

namespace webrtc {

struct ConnectionData;
class UserData;

class RTC_EXPORT XdgDesktopPortalBase : public rtc::RefCountedBase {
 public:
  enum class CaptureSourceType : uint32_t {
    kScreen = 0b01,
    kWindow = 0b10,
    kAny = 0b11
  };

  XdgDesktopPortalBase();
  ~XdgDesktopPortalBase();

  static rtc::scoped_refptr<XdgDesktopPortalBase> CreateDefault();

  // Initializes a screen sharing request for a web page identified with a
  // id. This id is later used to associate it with ConnectionData structure
  // where we keep all the necessary information about the DBus communication.
  //
  // This starts a series of DBus calls:
  //   1) SessionRequest - requests a session, which will be associated with
  //   this screen sharing request, passing a handle for identification so
  //   we can watch for success/failure.
  //   2) SourceRequest - requests what content we want to share on given
  //   session (either monitor, screen or both)
  //   3) StartRequest - requests to
  //   start sharing, which in return will give us information about stream
  //   (stream id and resolution).
  //   4) OpenPipeWireRemote - requests a file descriptor which we can use
  //   for initialization of PipeWire on the client side and start receiving
  //   content.
  void InitPortal(rtc::Callback1<void, bool> callback,
                  XdgDesktopPortalBase::CaptureSourceType requested_type,
                  int32_t id);

  void CloseConnection(const absl::optional<int32_t>& id);
  void SetConnectionStreamingOnWeb(const absl::optional<int32_t>& id);

  bool IsConnectionInitialized(const absl::optional<int32_t>& id) const;
  bool IsConnectionStreamingOnWeb(const absl::optional<int32_t>& id) const;

  PipeWireBase* GetPipeWireBase(const absl::optional<int32_t>& id) const;

  // Current ID serves for the DesktopCapturerOption to identify portal call
  // from the client itself so we can skip an additional call which would be
  // made upon preview dialog confirmation (e.g. Chromium).
  absl::optional<int32_t> CurrentConnectionId() const;
  void SetCurrentConnectionId(const absl::optional<int32_t>& id);

  ConnectionData* GetConnectionData(const absl::optional<int32_t>& id) const;

 private:
  // Methods are defined in the same order in which they are being called
  guint SetupRequestResponseSignal(const gchar* object_path,
                                   GDBusSignalCallback callback,
                                   UserData* data);

  static void OnProxyRequested(GObject* object,
                               GAsyncResult* result,
                               gpointer user_data);

  static gchar* PrepareSignalHandle(GDBusConnection* connection,
                                    const gchar* token);

  void SessionRequest(UserData* data);
  static void OnSessionRequested(GDBusConnection* connection,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnSessionRequestResponseSignal(GDBusConnection* connection,
                                             const gchar* sender_name,
                                             const gchar* object_path,
                                             const gchar* interface_name,
                                             const gchar* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);

  void SourcesRequest(UserData* data);
  static void OnSourcesRequested(GDBusConnection* connection,
                                 GAsyncResult* result,
                                 gpointer user_data);
  static void OnSourcesRequestResponseSignal(GDBusConnection* connection,
                                             const gchar* sender_name,
                                             const gchar* object_path,
                                             const gchar* interface_name,
                                             const gchar* signal_name,
                                             GVariant* parameters,
                                             gpointer user_data);

  void StartRequest(UserData* data);
  static void OnStartRequested(GDBusConnection* connection,
                               GAsyncResult* result,
                               gpointer user_data);
  static void OnStartRequestResponseSignal(GDBusConnection* connection,
                                           const gchar* sender_name,
                                           const gchar* object_path,
                                           const gchar* interface_name,
                                           const gchar* signal_name,
                                           GVariant* parameters,
                                           gpointer user_data);
  void OpenPipeWireRemote(UserData* data);
  static void OnOpenPipeWireRemoteRequested(GDBusConnection* connection,
                                            GAsyncResult* result,
                                            gpointer user_data);

  absl::optional<int32_t> current_connection_id_;
  std::map<int32_t, std::unique_ptr<ConnectionData>> connection_data_map_;

  RTC_DISALLOW_COPY_AND_ASSIGN(XdgDesktopPortalBase);
};

// This class represents each screen sharing request, which consists from a
// series of DBus calls, where we need to remember the session handle and
// parameters of the returned stream (id, resolution).
struct RTC_EXPORT ConnectionData {
  explicit ConnectionData(int32_t id);
  ~ConnectionData();

  bool operator=(const int32_t id) { return id_ == id; }

  gint32 pw_fd_ = -1;

  XdgDesktopPortalBase::CaptureSourceType capture_source_type_ =
      XdgDesktopPortalBase::CaptureSourceType::kAny;
  XdgDesktopPortalBase::CaptureSourceType requested_source_type_ =
      XdgDesktopPortalBase::CaptureSourceType::kAny;

  GDBusConnection* connection_ = nullptr;
  GDBusProxy* proxy_ = nullptr;
  gchar* portal_handle_ = nullptr;
  gchar* session_handle_ = nullptr;
  gchar* sources_handle_ = nullptr;
  gchar* start_handle_ = nullptr;
  guint session_request_signal_id_ = 0;
  guint sources_request_signal_id_ = 0;
  guint start_request_signal_id_ = 0;

  DesktopSize desktop_size_ = {};
  guint32 stream_id_ = 0;

  int32_t id_;

  bool already_used_ = false;
  bool portal_init_failed_ = false;
  bool web_streaming_ = false;
  std::vector<rtc::Callback1<void, bool>> callbacks_;
  rtc::Callback2<void, bool, int32_t> pw_callback_;

  std::unique_ptr<PipeWireBase> pw_base_;
};

// Structure which is used as user_data property in GLib async calls, where we
// need to pass two values. One is ID of the web page requesting screen sharing
// and the other is pointer to the XdgDesktopPortalBase object.
class UserData {
 public:
  UserData(int32_t id, XdgDesktopPortalBase* xdp) {
    data_id_ = id;
    xdp_ = xdp;
  }

  int32_t GetDataId() const { return data_id_; }
  XdgDesktopPortalBase* GetXdgDesktopPortalBase() const { return xdp_; }

 private:
  int32_t data_id_ = 0;
  XdgDesktopPortalBase* xdp_ = nullptr;
};

}  // namespace webrtc

#endif  // MODULES_DESKTOP_CAPTURE_LINUX_XDG_DESKTOP_PORTAL_BASE_H_
