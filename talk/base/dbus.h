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

#ifndef TALK_BASE_DBUS_H_
#define TALK_BASE_DBUS_H_

#ifdef HAVE_DBUS_GLIB

#include <dbus/dbus.h>

#include <string>
#include <vector>

#include "talk/base/libdbusglibsymboltable.h"
#include "talk/base/messagehandler.h"
#include "talk/base/thread.h"

namespace talk_base {

#define DBUS_TYPE                   "type"
#define DBUS_SIGNAL                 "signal"
#define DBUS_PATH                   "path"
#define DBUS_INTERFACE              "interface"
#define DBUS_MEMBER                 "member"

#ifdef CHROMEOS
#define CROS_PM_PATH                "/"
#define CROS_PM_INTERFACE           "org.chromium.PowerManager"
#define CROS_SIG_POWERCHANGED       "PowerStateChanged"
#define CROS_VALUE_SLEEP            "mem"
#define CROS_VALUE_RESUME           "on"
#else
#define UP_PATH                     "/org/freedesktop/UPower"
#define UP_INTERFACE                "org.freedesktop.UPower"
#define UP_SIG_SLEEPING             "Sleeping"
#define UP_SIG_RESUMING             "Resuming"
#endif  // CHROMEOS

// Wraps a DBus messages.
class DBusSigMessageData : public TypedMessageData<DBusMessage *> {
 public:
  explicit DBusSigMessageData(DBusMessage *message);
  ~DBusSigMessageData();
};

// DBusSigFilter is an abstract class that defines the interface of DBus
// signal handling.
// The subclasses implement ProcessSignal() for various purposes.
// When a DBus signal comes, a DSM_SIGNAL message is posted to the caller thread
// which will then invokes ProcessSignal().
class DBusSigFilter : protected MessageHandler {
 public:
  enum DBusSigMessage { DSM_SIGNAL };

  // This filter string should ususally come from BuildFilterString()
  explicit DBusSigFilter(const std::string &filter)
      : caller_thread_(Thread::Current()), filter_(filter) {
  }

  // Builds a DBus monitor filter string from given DBus path, interface, and
  // member.
  // See http://dbus.freedesktop.org/doc/api/html/group__DBusConnection.html
  static std::string BuildFilterString(const std::string &path,
                                       const std::string &interface,
                                       const std::string &member);

  // Handles callback on DBus messages by DBus system.
  static DBusHandlerResult DBusCallback(DBusConnection *dbus_conn,
                                        DBusMessage *message,
                                        void *instance);

  // Handles callback on DBus messages to each DBusSigFilter instance.
  DBusHandlerResult Callback(DBusMessage *message);

  // From MessageHandler.
  virtual void OnMessage(Message *message);

  // Returns the DBus monitor filter string.
  const std::string &filter() const { return filter_; }

 private:
  // On caller thread.
  virtual void ProcessSignal(DBusMessage *message) = 0;

  Thread *caller_thread_;
  const std::string filter_;
};

// DBusMonitor is a class for DBus signal monitoring.
//
// The caller-thread calls AddFilter() first to add the signals that it wants to
// monitor and then calls StartMonitoring() to start the monitoring.
// This will create a worker-thread which listens on DBus connection and sends
// DBus signals back through the callback.
// The worker-thread will be running forever until either StopMonitoring() is
// called from the caller-thread or the worker-thread hit some error.
//
// Programming model:
//   1. Caller-thread: Creates an object of DBusMonitor.
//   2. Caller-thread: Calls DBusMonitor::AddFilter() one or several times.
//   3. Caller-thread: StartMonitoring().
//      ...
//   4. Worker-thread: DBus signal recieved. Post a message to caller-thread.
//   5. Caller-thread: DBusFilterBase::ProcessSignal() is invoked.
//      ...
//   6. Caller-thread: StopMonitoring().
//
// Assumption:
//   AddFilter(), StartMonitoring(), and StopMonitoring() methods are called by
//   a single thread. Hence, there is no need to make them thread safe.
class DBusMonitor {
 public:
  // Status of DBus monitoring.
  enum DBusMonitorStatus {
    DMS_NOT_INITIALIZED,  // Not initialized.
    DMS_INITIALIZING,     // Initializing the monitoring thread.
    DMS_RUNNING,          // Monitoring.
    DMS_STOPPED,          // Not monitoring. Stopped normally.
    DMS_FAILED,           // Not monitoring. Failed.
  };

  // Returns the DBus-Glib symbol table.
  // We should only use this function to access DBus-Glib symbols.
  static LibDBusGlibSymbolTable *GetDBusGlibSymbolTable();

  // Creates an instance of DBusMonitor.
  static DBusMonitor *Create(DBusBusType type);
  ~DBusMonitor();

  // Adds a filter to DBusMonitor.
  bool AddFilter(DBusSigFilter *filter);

  // Starts DBus message monitoring.
  bool StartMonitoring();

  // Stops DBus message monitoring.
  bool StopMonitoring();

  // Gets the status of DBus monitoring.
  DBusMonitorStatus GetStatus();

 private:
  // Forward declaration. Defined in the .cc file.
  class DBusMonitoringThread;

  explicit DBusMonitor(DBusBusType type);

  // Updates status_ when monitoring status has changed.
  void OnMonitoringStatusChanged(DBusMonitorStatus status);

  DBusBusType type_;
  DBusMonitorStatus status_;
  DBusMonitoringThread *monitoring_thread_;
  std::vector<DBusSigFilter *> filter_list_;
};

}  // namespace talk_base

#endif  // HAVE_DBUS_GLIB

#endif  // TALK_BASE_DBUS_H_
