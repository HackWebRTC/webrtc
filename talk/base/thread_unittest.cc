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

#include "talk/base/asyncudpsocket.h"
#include "talk/base/event.h"
#include "talk/base/gunit.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/socketaddress.h"
#include "talk/base/thread.h"

#ifdef WIN32
#include <comdef.h>  // NOLINT
#endif

using namespace talk_base;

const int MAX = 65536;

// Generates a sequence of numbers (collaboratively).
class TestGenerator {
 public:
  TestGenerator() : last(0), count(0) {}

  int Next(int prev) {
    int result = prev + last;
    last = result;
    count += 1;
    return result;
  }

  int last;
  int count;
};

struct TestMessage : public MessageData {
  explicit TestMessage(int v) : value(v) {}
  virtual ~TestMessage() {}

  int value;
};

// Receives on a socket and sends by posting messages.
class SocketClient : public TestGenerator, public sigslot::has_slots<> {
 public:
  SocketClient(AsyncSocket* socket, const SocketAddress& addr,
               Thread* post_thread, MessageHandler* phandler)
      : socket_(AsyncUDPSocket::Create(socket, addr)),
        post_thread_(post_thread),
        post_handler_(phandler) {
    socket_->SignalReadPacket.connect(this, &SocketClient::OnPacket);
  }

  ~SocketClient() {
    delete socket_;
  }

  SocketAddress address() const { return socket_->GetLocalAddress(); }

  void OnPacket(AsyncPacketSocket* socket, const char* buf, size_t size,
                const SocketAddress& remote_addr,
                const PacketTime& packet_time) {
    EXPECT_EQ(size, sizeof(uint32));
    uint32 prev = reinterpret_cast<const uint32*>(buf)[0];
    uint32 result = Next(prev);

    //socket_->set_readable(last < MAX);
    post_thread_->PostDelayed(200, post_handler_, 0, new TestMessage(result));
  }

 private:
  AsyncUDPSocket* socket_;
  Thread* post_thread_;
  MessageHandler* post_handler_;
};

// Receives messages and sends on a socket.
class MessageClient : public MessageHandler, public TestGenerator {
 public:
  MessageClient(Thread* pth, Socket* socket)
      : thread_(pth), socket_(socket) {
  }

  virtual ~MessageClient() {
    delete socket_;
  }

  virtual void OnMessage(Message *pmsg) {
    TestMessage* msg = static_cast<TestMessage*>(pmsg->pdata);
    int result = Next(msg->value);
    EXPECT_GE(socket_->Send(&result, sizeof(result)), 0);
    delete msg;
  }

 private:
  Thread* thread_;
  Socket* socket_;
};

class CustomThread : public talk_base::Thread {
 public:
  CustomThread() {}
  virtual ~CustomThread() { Stop(); }
  bool Start() { return false; }
};


// A thread that does nothing when it runs and signals an event
// when it is destroyed.
class SignalWhenDestroyedThread : public Thread {
 public:
  SignalWhenDestroyedThread(Event* event)
      : event_(event) {
  }

  virtual ~SignalWhenDestroyedThread() {
    Stop();
    event_->Set();
  }

  virtual void Run() {
    // Do nothing.
  }

 private:
  Event* event_;
};

// Function objects to test Thread::Invoke.
struct Functor1 {
  int operator()() { return 42; }
};
class Functor2 {
 public:
  explicit Functor2(bool* flag) : flag_(flag) {}
  void operator()() { if (flag_) *flag_ = true; }
 private:
  bool* flag_;
};

// See: https://code.google.com/p/webrtc/issues/detail?id=2409
TEST(ThreadTest, DISABLED_Main) {
  const SocketAddress addr("127.0.0.1", 0);

  // Create the messaging client on its own thread.
  Thread th1;
  Socket* socket = th1.socketserver()->CreateAsyncSocket(addr.family(),
                                                         SOCK_DGRAM);
  MessageClient msg_client(&th1, socket);

  // Create the socket client on its own thread.
  Thread th2;
  AsyncSocket* asocket =
      th2.socketserver()->CreateAsyncSocket(addr.family(), SOCK_DGRAM);
  SocketClient sock_client(asocket, addr, &th1, &msg_client);

  socket->Connect(sock_client.address());

  th1.Start();
  th2.Start();

  // Get the messages started.
  th1.PostDelayed(100, &msg_client, 0, new TestMessage(1));

  // Give the clients a little while to run.
  // Messages will be processed at 100, 300, 500, 700, 900.
  Thread* th_main = Thread::Current();
  th_main->ProcessMessages(1000);

  // Stop the sending client. Give the receiver a bit longer to run, in case
  // it is running on a machine that is under load (e.g. the build machine).
  th1.Stop();
  th_main->ProcessMessages(200);
  th2.Stop();

  // Make sure the results were correct
  EXPECT_EQ(5, msg_client.count);
  EXPECT_EQ(34, msg_client.last);
  EXPECT_EQ(5, sock_client.count);
  EXPECT_EQ(55, sock_client.last);
}

// Test that setting thread names doesn't cause a malfunction.
// There's no easy way to verify the name was set properly at this time.
TEST(ThreadTest, Names) {
  // Default name
  Thread *thread;
  thread = new Thread();
  EXPECT_TRUE(thread->Start());
  thread->Stop();
  delete thread;
  thread = new Thread();
  // Name with no object parameter
  EXPECT_TRUE(thread->SetName("No object", NULL));
  EXPECT_TRUE(thread->Start());
  thread->Stop();
  delete thread;
  // Really long name
  thread = new Thread();
  EXPECT_TRUE(thread->SetName("Abcdefghijklmnopqrstuvwxyz1234567890", this));
  EXPECT_TRUE(thread->Start());
  thread->Stop();
  delete thread;
}

// Test that setting thread priorities doesn't cause a malfunction.
// There's no easy way to verify the priority was set properly at this time.
TEST(ThreadTest, Priorities) {
  Thread *thread;
  thread = new Thread();
  EXPECT_TRUE(thread->SetPriority(PRIORITY_HIGH));
  EXPECT_TRUE(thread->Start());
  thread->Stop();
  delete thread;
  thread = new Thread();
  EXPECT_TRUE(thread->SetPriority(PRIORITY_ABOVE_NORMAL));
  EXPECT_TRUE(thread->Start());
  thread->Stop();
  delete thread;

  thread = new Thread();
  EXPECT_TRUE(thread->Start());
#ifdef WIN32
  EXPECT_TRUE(thread->SetPriority(PRIORITY_ABOVE_NORMAL));
#else
  EXPECT_FALSE(thread->SetPriority(PRIORITY_ABOVE_NORMAL));
#endif
  thread->Stop();
  delete thread;

}

TEST(ThreadTest, Wrap) {
  Thread* current_thread = Thread::Current();
  current_thread->UnwrapCurrent();
  CustomThread* cthread = new CustomThread();
  EXPECT_TRUE(cthread->WrapCurrent());
  EXPECT_TRUE(cthread->started());
  EXPECT_FALSE(cthread->IsOwned());
  cthread->UnwrapCurrent();
  EXPECT_FALSE(cthread->started());
  delete cthread;
  current_thread->WrapCurrent();
}

// Test that calling Release on a thread causes it to self-destruct when
// it's finished running
TEST(ThreadTest, Release) {
  scoped_ptr<Event> event(new Event(true, false));
  // Ensure the event is initialized.
  event->Reset();

  Thread* thread = new SignalWhenDestroyedThread(event.get());
  thread->Start();
  thread->Release();

  // The event should get signaled when the thread completes, which should
  // be nearly instantaneous, since it doesn't do anything.  For safety,
  // give it 3 seconds in case the machine is under load.
  bool signaled = event->Wait(3000);
  EXPECT_TRUE(signaled);
}

TEST(ThreadTest, Invoke) {
  // Create and start the thread.
  Thread thread;
  thread.Start();
  // Try calling functors.
  EXPECT_EQ(42, thread.Invoke<int>(Functor1()));
  bool called = false;
  Functor2 f2(&called);
  thread.Invoke<void>(f2);
  EXPECT_TRUE(called);
  // Try calling bare functions.
  struct LocalFuncs {
    static int Func1() { return 999; }
    static void Func2() {}
  };
  EXPECT_EQ(999, thread.Invoke<int>(&LocalFuncs::Func1));
  thread.Invoke<void>(&LocalFuncs::Func2);
}

#ifdef WIN32
class ComThreadTest : public testing::Test, public MessageHandler {
 public:
  ComThreadTest() : done_(false) {}
 protected:
  virtual void OnMessage(Message* message) {
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    // S_FALSE means the thread was already inited for a multithread apartment.
    EXPECT_EQ(S_FALSE, hr);
    if (SUCCEEDED(hr)) {
      CoUninitialize();
    }
    done_ = true;
  }
  bool done_;
};

TEST_F(ComThreadTest, ComInited) {
  Thread* thread = new ComThread();
  EXPECT_TRUE(thread->Start());
  thread->Post(this, 0);
  EXPECT_TRUE_WAIT(done_, 1000);
  delete thread;
}
#endif
