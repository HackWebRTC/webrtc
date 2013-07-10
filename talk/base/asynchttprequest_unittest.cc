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

#include <string>
#include "talk/base/asynchttprequest.h"
#include "talk/base/gunit.h"
#include "talk/base/httpserver.h"
#include "talk/base/socketstream.h"
#include "talk/base/thread.h"

namespace talk_base {

static const SocketAddress kServerAddr("127.0.0.1", 0);
static const SocketAddress kServerHostnameAddr("localhost", 0);
static const char kServerGetPath[] = "/get";
static const char kServerPostPath[] = "/post";
static const char kServerResponse[] = "This is a test";

class TestHttpServer : public HttpServer, public sigslot::has_slots<> {
 public:
  TestHttpServer(Thread* thread, const SocketAddress& addr) :
      socket_(thread->socketserver()->CreateAsyncSocket(addr.family(),
                                                        SOCK_STREAM)) {
    socket_->Bind(addr);
    socket_->Listen(5);
    socket_->SignalReadEvent.connect(this, &TestHttpServer::OnAccept);
  }

  SocketAddress address() const { return socket_->GetLocalAddress(); }
  void Close() const { socket_->Close(); }

 private:
  void OnAccept(AsyncSocket* socket) {
    AsyncSocket* new_socket = socket_->Accept(NULL);
    if (new_socket) {
      HandleConnection(new SocketStream(new_socket));
    }
  }
  talk_base::scoped_ptr<AsyncSocket> socket_;
};

class AsyncHttpRequestTest : public testing::Test,
                             public sigslot::has_slots<> {
 public:
  AsyncHttpRequestTest()
      : started_(false),
        done_(false),
        server_(Thread::Current(), kServerAddr) {
    server_.SignalHttpRequest.connect(this, &AsyncHttpRequestTest::OnRequest);
  }

  bool started() const { return started_; }
  bool done() const { return done_; }

  AsyncHttpRequest* CreateGetRequest(const std::string& host, int port,
                                     const std::string& path) {
    talk_base::AsyncHttpRequest* request =
        new talk_base::AsyncHttpRequest("unittest");
    request->SignalWorkDone.connect(this,
        &AsyncHttpRequestTest::OnRequestDone);
    request->request().verb = talk_base::HV_GET;
    request->set_host(host);
    request->set_port(port);
    request->request().path = path;
    request->response().document.reset(new MemoryStream());
    return request;
  }
  AsyncHttpRequest* CreatePostRequest(const std::string& host, int port,
                                      const std::string& path,
                                      const std::string content_type,
                                      StreamInterface* content) {
    talk_base::AsyncHttpRequest* request =
        new talk_base::AsyncHttpRequest("unittest");
    request->SignalWorkDone.connect(this,
        &AsyncHttpRequestTest::OnRequestDone);
    request->request().verb = talk_base::HV_POST;
    request->set_host(host);
    request->set_port(port);
    request->request().path = path;
    request->request().setContent(content_type, content);
    request->response().document.reset(new MemoryStream());
    return request;
  }

  const TestHttpServer& server() const { return server_; }

 protected:
  void OnRequest(HttpServer* server, HttpServerTransaction* t) {
    started_ = true;

    if (t->request.path == kServerGetPath) {
      t->response.set_success("text/plain", new MemoryStream(kServerResponse));
    } else if (t->request.path == kServerPostPath) {
      // reverse the data and reply
      size_t size;
      StreamInterface* in = t->request.document.get();
      StreamInterface* out = new MemoryStream();
      in->GetSize(&size);
      for (size_t i = 0; i < size; ++i) {
        char ch;
        in->SetPosition(size - i - 1);
        in->Read(&ch, 1, NULL, NULL);
        out->Write(&ch, 1, NULL, NULL);
      }
      out->Rewind();
      t->response.set_success("text/plain", out);
    } else {
      t->response.set_error(404);
    }
    server_.Respond(t);
  }
  void OnRequestDone(SignalThread* thread) {
    done_ = true;
  }

 private:
  bool started_;
  bool done_;
  TestHttpServer server_;
};

TEST_F(AsyncHttpRequestTest, TestGetSuccess) {
  AsyncHttpRequest* req = CreateGetRequest(
      kServerHostnameAddr.hostname(), server().address().port(),
      kServerGetPath);
  EXPECT_FALSE(started());
  req->Start();
  EXPECT_TRUE_WAIT(started(), 5000);  // Should have started by now.
  EXPECT_TRUE_WAIT(done(), 5000);
  std::string response;
  EXPECT_EQ(200U, req->response().scode);
  ASSERT_TRUE(req->response().document);
  req->response().document->Rewind();
  req->response().document->ReadLine(&response);
  EXPECT_EQ(kServerResponse, response);
  req->Release();
}

TEST_F(AsyncHttpRequestTest, TestGetNotFound) {
  AsyncHttpRequest* req = CreateGetRequest(
      kServerHostnameAddr.hostname(), server().address().port(),
      "/bad");
  req->Start();
  EXPECT_TRUE_WAIT(done(), 5000);
  size_t size;
  EXPECT_EQ(404U, req->response().scode);
  ASSERT_TRUE(req->response().document);
  req->response().document->GetSize(&size);
  EXPECT_EQ(0U, size);
  req->Release();
}

TEST_F(AsyncHttpRequestTest, TestGetToNonServer) {
  AsyncHttpRequest* req = CreateGetRequest(
      "127.0.0.1", server().address().port(),
      kServerGetPath);
  // Stop the server before we send the request.
  server().Close();
  req->Start();
  EXPECT_TRUE_WAIT(done(), 10000);
  size_t size;
  EXPECT_EQ(500U, req->response().scode);
  ASSERT_TRUE(req->response().document);
  req->response().document->GetSize(&size);
  EXPECT_EQ(0U, size);
  req->Release();
}

TEST_F(AsyncHttpRequestTest, DISABLED_TestGetToInvalidHostname) {
  AsyncHttpRequest* req = CreateGetRequest(
      "invalid", server().address().port(),
      kServerGetPath);
  req->Start();
  EXPECT_TRUE_WAIT(done(), 5000);
  size_t size;
  EXPECT_EQ(500U, req->response().scode);
  ASSERT_TRUE(req->response().document);
  req->response().document->GetSize(&size);
  EXPECT_EQ(0U, size);
  req->Release();
}

TEST_F(AsyncHttpRequestTest, TestPostSuccess) {
  AsyncHttpRequest* req = CreatePostRequest(
      kServerHostnameAddr.hostname(), server().address().port(),
      kServerPostPath, "text/plain", new MemoryStream("abcd1234"));
  req->Start();
  EXPECT_TRUE_WAIT(done(), 5000);
  std::string response;
  EXPECT_EQ(200U, req->response().scode);
  ASSERT_TRUE(req->response().document);
  req->response().document->Rewind();
  req->response().document->ReadLine(&response);
  EXPECT_EQ("4321dcba", response);
  req->Release();
}

// Ensure that we shut down properly even if work is outstanding.
TEST_F(AsyncHttpRequestTest, TestCancel) {
  AsyncHttpRequest* req = CreateGetRequest(
      kServerHostnameAddr.hostname(), server().address().port(),
      kServerGetPath);
  req->Start();
  req->Destroy(true);
}

TEST_F(AsyncHttpRequestTest, TestGetSuccessDelay) {
  AsyncHttpRequest* req = CreateGetRequest(
      kServerHostnameAddr.hostname(), server().address().port(),
      kServerGetPath);
  req->set_start_delay(10);  // Delay 10ms.
  req->Start();
  Thread::SleepMs(5);
  EXPECT_FALSE(started());  // Should not have started immediately.
  EXPECT_TRUE_WAIT(started(), 5000);  // Should have started by now.
  EXPECT_TRUE_WAIT(done(), 5000);
  std::string response;
  EXPECT_EQ(200U, req->response().scode);
  ASSERT_TRUE(req->response().document);
  req->response().document->Rewind();
  req->response().document->ReadLine(&response);
  EXPECT_EQ(kServerResponse, response);
  req->Release();
}

}  // namespace talk_base
