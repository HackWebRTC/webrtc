/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#ifdef HAVE_DBUS_GLIB

#include "talk/base/dbus.h"
#include "talk/base/gunit.h"
#include "talk/base/thread.h"

namespace talk_base {

#define SIG_NAME "NameAcquired"

static const uint32 kTimeoutMs = 5000U;

class DBusSigFilterTest : public DBusSigFilter {
 public:
  // DBusSigFilterTest listens on DBus service itself for "NameAcquired" signal.
  // This signal should be received when the application connects to DBus
  // service and gains ownership of a name.
  // http://dbus.freedesktop.org/doc/dbus-specification.html
  DBusSigFilterTest()
      : DBusSigFilter(GetFilter()),
        message_received_(false) {
  }

  bool MessageReceived() {
    return message_received_;
  }

 private:
  static std::string GetFilter() {
    return talk_base::DBusSigFilter::BuildFilterString("", "", SIG_NAME);
  }

  // Implement virtual method of DBusSigFilter. On caller thread.
  virtual void ProcessSignal(DBusMessage *message) {
    EXPECT_TRUE(message != NULL);
    message_received_ = true;
  }

  bool message_received_;
};

TEST(DBusMonitorTest, StartStopStartStop) {
  DBusSigFilterTest filter;
  talk_base::scoped_ptr<talk_base::DBusMonitor> monitor;
  monitor.reset(talk_base::DBusMonitor::Create(DBUS_BUS_SYSTEM));
  if (monitor) {
    EXPECT_TRUE(monitor->AddFilter(&filter));

    EXPECT_TRUE(monitor->StopMonitoring());
    EXPECT_EQ(monitor->GetStatus(), DBusMonitor::DMS_NOT_INITIALIZED);

    EXPECT_TRUE(monitor->StartMonitoring());
    EXPECT_EQ_WAIT(DBusMonitor::DMS_RUNNING, monitor->GetStatus(), kTimeoutMs);
    EXPECT_TRUE(monitor->StopMonitoring());
    EXPECT_EQ(monitor->GetStatus(), DBusMonitor::DMS_STOPPED);
    EXPECT_TRUE(monitor->StopMonitoring());
    EXPECT_EQ(monitor->GetStatus(), DBusMonitor::DMS_STOPPED);

    EXPECT_TRUE(monitor->StartMonitoring());
    EXPECT_EQ_WAIT(DBusMonitor::DMS_RUNNING, monitor->GetStatus(), kTimeoutMs);
    EXPECT_TRUE(monitor->StartMonitoring());
    EXPECT_EQ(monitor->GetStatus(), DBusMonitor::DMS_RUNNING);
    EXPECT_TRUE(monitor->StopMonitoring());
    EXPECT_EQ(monitor->GetStatus(), DBusMonitor::DMS_STOPPED);
  } else {
    LOG(LS_WARNING) << "DBus Monitor not started. Skipping test.";
  }
}

// DBusMonitorTest listens on DBus service itself for "NameAcquired" signal.
// This signal should be received when the application connects to DBus
// service and gains ownership of a name.
// This test is to make sure that we capture the "NameAcquired" signal.
TEST(DBusMonitorTest, ReceivedNameAcquiredSignal) {
  DBusSigFilterTest filter;
  talk_base::scoped_ptr<talk_base::DBusMonitor> monitor;
  monitor.reset(talk_base::DBusMonitor::Create(DBUS_BUS_SYSTEM));
  if (monitor) {
    EXPECT_TRUE(monitor->AddFilter(&filter));

    EXPECT_TRUE(monitor->StartMonitoring());
    EXPECT_EQ_WAIT(DBusMonitor::DMS_RUNNING, monitor->GetStatus(), kTimeoutMs);
    EXPECT_TRUE_WAIT(filter.MessageReceived(), kTimeoutMs);
    EXPECT_TRUE(monitor->StopMonitoring());
    EXPECT_EQ(monitor->GetStatus(), DBusMonitor::DMS_STOPPED);
  } else {
    LOG(LS_WARNING) << "DBus Monitor not started. Skipping test.";
  }
}

TEST(DBusMonitorTest, ConcurrentMonitors) {
  DBusSigFilterTest filter1;
  talk_base::scoped_ptr<talk_base::DBusMonitor> monitor1;
  monitor1.reset(talk_base::DBusMonitor::Create(DBUS_BUS_SYSTEM));
  if (monitor1) {
    EXPECT_TRUE(monitor1->AddFilter(&filter1));
    DBusSigFilterTest filter2;
    talk_base::scoped_ptr<talk_base::DBusMonitor> monitor2;
    monitor2.reset(talk_base::DBusMonitor::Create(DBUS_BUS_SYSTEM));
    EXPECT_TRUE(monitor2->AddFilter(&filter2));

    EXPECT_TRUE(monitor1->StartMonitoring());
    EXPECT_EQ_WAIT(DBusMonitor::DMS_RUNNING, monitor1->GetStatus(), kTimeoutMs);
    EXPECT_TRUE(monitor2->StartMonitoring());
    EXPECT_EQ_WAIT(DBusMonitor::DMS_RUNNING, monitor2->GetStatus(), kTimeoutMs);

    EXPECT_TRUE_WAIT(filter2.MessageReceived(), kTimeoutMs);
    EXPECT_TRUE(monitor2->StopMonitoring());
    EXPECT_EQ(monitor2->GetStatus(), DBusMonitor::DMS_STOPPED);

    EXPECT_TRUE_WAIT(filter1.MessageReceived(), kTimeoutMs);
    EXPECT_TRUE(monitor1->StopMonitoring());
    EXPECT_EQ(monitor1->GetStatus(), DBusMonitor::DMS_STOPPED);
  } else {
    LOG(LS_WARNING) << "DBus Monitor not started. Skipping test.";
  }
}

TEST(DBusMonitorTest, ConcurrentFilters) {
  DBusSigFilterTest filter1;
  DBusSigFilterTest filter2;
  talk_base::scoped_ptr<talk_base::DBusMonitor> monitor;
  monitor.reset(talk_base::DBusMonitor::Create(DBUS_BUS_SYSTEM));
  if (monitor) {
    EXPECT_TRUE(monitor->AddFilter(&filter1));
    EXPECT_TRUE(monitor->AddFilter(&filter2));

    EXPECT_TRUE(monitor->StartMonitoring());
    EXPECT_EQ_WAIT(DBusMonitor::DMS_RUNNING, monitor->GetStatus(), kTimeoutMs);

    EXPECT_TRUE_WAIT(filter1.MessageReceived(), kTimeoutMs);
    EXPECT_TRUE_WAIT(filter2.MessageReceived(), kTimeoutMs);

    EXPECT_TRUE(monitor->StopMonitoring());
    EXPECT_EQ(monitor->GetStatus(), DBusMonitor::DMS_STOPPED);
  } else {
    LOG(LS_WARNING) << "DBus Monitor not started. Skipping test.";
  }
}

TEST(DBusMonitorTest, NoAddFilterIfRunning) {
  DBusSigFilterTest filter1;
  DBusSigFilterTest filter2;
  talk_base::scoped_ptr<talk_base::DBusMonitor> monitor;
  monitor.reset(talk_base::DBusMonitor::Create(DBUS_BUS_SYSTEM));
  if (monitor) {
    EXPECT_TRUE(monitor->AddFilter(&filter1));

    EXPECT_TRUE(monitor->StartMonitoring());
    EXPECT_EQ_WAIT(DBusMonitor::DMS_RUNNING, monitor->GetStatus(), kTimeoutMs);
    EXPECT_FALSE(monitor->AddFilter(&filter2));

    EXPECT_TRUE(monitor->StopMonitoring());
    EXPECT_EQ(monitor->GetStatus(), DBusMonitor::DMS_STOPPED);
  } else {
    LOG(LS_WARNING) << "DBus Monitor not started. Skipping test.";
  }
}

TEST(DBusMonitorTest, AddFilterAfterStop) {
  DBusSigFilterTest filter1;
  DBusSigFilterTest filter2;
  talk_base::scoped_ptr<talk_base::DBusMonitor> monitor;
  monitor.reset(talk_base::DBusMonitor::Create(DBUS_BUS_SYSTEM));
  if (monitor) {
    EXPECT_TRUE(monitor->AddFilter(&filter1));
    EXPECT_TRUE(monitor->StartMonitoring());
    EXPECT_EQ_WAIT(DBusMonitor::DMS_RUNNING, monitor->GetStatus(), kTimeoutMs);
    EXPECT_TRUE_WAIT(filter1.MessageReceived(), kTimeoutMs);
    EXPECT_TRUE(monitor->StopMonitoring());
    EXPECT_EQ(monitor->GetStatus(), DBusMonitor::DMS_STOPPED);

    EXPECT_TRUE(monitor->AddFilter(&filter2));
    EXPECT_TRUE(monitor->StartMonitoring());
    EXPECT_EQ_WAIT(DBusMonitor::DMS_RUNNING, monitor->GetStatus(), kTimeoutMs);
    EXPECT_TRUE_WAIT(filter1.MessageReceived(), kTimeoutMs);
    EXPECT_TRUE_WAIT(filter2.MessageReceived(), kTimeoutMs);
    EXPECT_TRUE(monitor->StopMonitoring());
    EXPECT_EQ(monitor->GetStatus(), DBusMonitor::DMS_STOPPED);
  } else {
    LOG(LS_WARNING) << "DBus Monitor not started. Skipping test.";
  }
}

TEST(DBusMonitorTest, StopRightAfterStart) {
  DBusSigFilterTest filter;
  talk_base::scoped_ptr<talk_base::DBusMonitor> monitor;
  monitor.reset(talk_base::DBusMonitor::Create(DBUS_BUS_SYSTEM));
  if (monitor) {
    EXPECT_TRUE(monitor->AddFilter(&filter));

    EXPECT_TRUE(monitor->StartMonitoring());
    EXPECT_TRUE(monitor->StopMonitoring());

    // Stop the monitoring thread right after it had been started.
    // If the monitoring thread got a chance to receive a DBus signal, it would
    // post a message to the main thread and signal the main thread wakeup.
    // This message will be cleaned out automatically when the filter get
    // destructed. Here we also consume the wakeup signal (if there is one) so
    // that the testing (main) thread is reset to a clean state.
    talk_base::Thread::Current()->ProcessMessages(1);
  } else {
    LOG(LS_WARNING) << "DBus Monitor not started.";
  }
}

TEST(DBusSigFilter, BuildFilterString) {
  EXPECT_EQ(DBusSigFilter::BuildFilterString("", "", ""),
      (DBUS_TYPE "='" DBUS_SIGNAL "'"));
  EXPECT_EQ(DBusSigFilter::BuildFilterString("p", "", ""),
      (DBUS_TYPE "='" DBUS_SIGNAL "'," DBUS_PATH "='p'"));
  EXPECT_EQ(DBusSigFilter::BuildFilterString("p","i", ""),
      (DBUS_TYPE "='" DBUS_SIGNAL "'," DBUS_PATH "='p',"
          DBUS_INTERFACE "='i'"));
  EXPECT_EQ(DBusSigFilter::BuildFilterString("p","i","m"),
      (DBUS_TYPE "='" DBUS_SIGNAL "'," DBUS_PATH "='p',"
          DBUS_INTERFACE "='i'," DBUS_MEMBER "='m'"));
}

}  // namespace talk_base

#endif  // HAVE_DBUS_GLIB
