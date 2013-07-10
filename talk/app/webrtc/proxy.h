/*
 * libjingle
 * Copyright 2013, Google Inc.
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

// This file contains Macros for creating proxies for webrtc MediaStream and
// PeerConnection classes.

//
// Example usage:
//
// class TestInterface : public talk_base::RefCountInterface {
//  public:
//   std::string FooA() = 0;
//   std::string FooB(bool arg1) const = 0;
//   std::string FooC(bool arg1)= 0;
//  };
//
// Note that return types can not be a const reference.
//
// class Test : public TestInterface {
// ... implementation of the interface.
// };
//
// BEGIN_PROXY_MAP(Test)
//   PROXY_METHOD0(std::string, FooA)
//   PROXY_CONSTMETHOD1(std::string, FooB, arg1)
//   PROXY_METHOD1(std::string, FooC, arg1)
// END_PROXY()
//
// The proxy can be created using TestProxy::Create(Thread*, TestInterface*).

#ifndef TALK_APP_WEBRTC_PROXY_H_
#define TALK_APP_WEBRTC_PROXY_H_

#include "talk/base/thread.h"

namespace webrtc {

template <typename R>
class ReturnType {
 public:
  template<typename C, typename M>
  void Invoke(C* c, M m) { r_ = (c->*m)(); }
  template<typename C, typename M, typename T1>
  void Invoke(C* c, M m, T1 a1) { r_ = (c->*m)(a1); }
  template<typename C, typename M, typename T1, typename T2>
  void Invoke(C* c, M m, T1 a1, T2 a2) { r_ = (c->*m)(a1, a2); }
  template<typename C, typename M, typename T1, typename T2, typename T3>
  void Invoke(C* c, M m, T1 a1, T2 a2, T3 a3) { r_ = (c->*m)(a1, a2, a3); }

  R value() { return r_; }

 private:
  R r_;
};

template <>
class ReturnType<void> {
 public:
  template<typename C, typename M>
  void Invoke(C* c, M m) { (c->*m)(); }
  template<typename C, typename M, typename T1>
  void Invoke(C* c, M m, T1 a1) { (c->*m)(a1); }
  template<typename C, typename M, typename T1, typename T2>
  void Invoke(C* c, M m, T1 a1, T2 a2) { (c->*m)(a1, a2); }
  template<typename C, typename M, typename T1, typename T2, typename T3>
  void Invoke(C* c, M m, T1 a1, T2 a2, T3 a3) { (c->*m)(a1, a2, a3); }

  void value() {}
};

template <typename C, typename R>
class MethodCall0 : public talk_base::Message,
                    public talk_base::MessageHandler {
 public:
  typedef R (C::*Method)();
  MethodCall0(C* c, Method m) : c_(c), m_(m) {}

  R Marshal(talk_base::Thread* t) {
    t->Send(this, 0);
    return r_.value();
  }

 private:
  void OnMessage(talk_base::Message*) {  r_.Invoke(c_, m_);}

  C* c_;
  Method m_;
  ReturnType<R> r_;
};

template <typename C, typename R>
class ConstMethodCall0 : public talk_base::Message,
                         public talk_base::MessageHandler {
 public:
  typedef R (C::*Method)() const;
  ConstMethodCall0(C* c, Method m) : c_(c), m_(m) {}

  R Marshal(talk_base::Thread* t) {
    t->Send(this, 0);
    return r_.value();
  }

 private:
  void OnMessage(talk_base::Message*) { r_.Invoke(c_, m_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
};

template <typename C, typename R,  typename T1>
class MethodCall1 : public talk_base::Message,
                    public talk_base::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1);
  MethodCall1(C* c, Method m, T1 a1) : c_(c), m_(m), a1_(a1) {}

  R Marshal(talk_base::Thread* t) {
    t->Send(this, 0);
    return r_.value();
  }

 private:
  void OnMessage(talk_base::Message*) { r_.Invoke(c_, m_, a1_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
};

template <typename C, typename R,  typename T1>
class ConstMethodCall1 : public talk_base::Message,
                         public talk_base::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1) const;
  ConstMethodCall1(C* c, Method m, T1 a1) : c_(c), m_(m), a1_(a1) {}

  R Marshal(talk_base::Thread* t) {
    t->Send(this, 0);
    return r_.value();
  }

 private:
  void OnMessage(talk_base::Message*) { r_.Invoke(c_, m_, a1_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
};

template <typename C, typename R, typename T1, typename T2>
class MethodCall2 : public talk_base::Message,
                    public talk_base::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1, T2 a2);
  MethodCall2(C* c, Method m, T1 a1, T2 a2) : c_(c), m_(m), a1_(a1), a2_(a2) {}

  R Marshal(talk_base::Thread* t) {
    t->Send(this, 0);
    return r_.value();
  }

 private:
  void OnMessage(talk_base::Message*) { r_.Invoke(c_, m_, a1_, a2_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
  T2 a2_;
};

template <typename C, typename R, typename T1, typename T2, typename T3>
class MethodCall3 : public talk_base::Message,
                    public talk_base::MessageHandler {
 public:
  typedef R (C::*Method)(T1 a1, T2 a2, T3 a3);
  MethodCall3(C* c, Method m, T1 a1, T2 a2, T3 a3)
      : c_(c), m_(m), a1_(a1), a2_(a2), a3_(a3) {}

  R Marshal(talk_base::Thread* t) {
    t->Send(this, 0);
    return r_.value();
  }

 private:
  void OnMessage(talk_base::Message*) { r_.Invoke(c_, m_, a1_, a2_, a3_); }

  C* c_;
  Method m_;
  ReturnType<R> r_;
  T1 a1_;
  T2 a2_;
  T3 a3_;
};

#define BEGIN_PROXY_MAP(c) \
  class c##Proxy : public c##Interface {\
   protected:\
    typedef c##Interface C;\
    c##Proxy(talk_base::Thread* thread, C* c)\
      : owner_thread_(thread), \
        c_(c)  {}\
    ~c##Proxy() {\
      MethodCall0<c##Proxy, void> call(this, &c##Proxy::Release_s);\
      call.Marshal(owner_thread_);\
    }\
   public:\
    static talk_base::scoped_refptr<C> Create(talk_base::Thread* thread, \
                                              C* c) {\
      return new talk_base::RefCountedObject<c##Proxy>(thread, c);\
    }\

#define PROXY_METHOD0(r, method)\
    r method() OVERRIDE {\
      MethodCall0<C, r> call(c_.get(), &C::method);\
      return call.Marshal(owner_thread_);\
    }\

#define PROXY_CONSTMETHOD0(r, method)\
    r method() const OVERRIDE {\
      ConstMethodCall0<C, r> call(c_.get(), &C::method);\
      return call.Marshal(owner_thread_);\
     }\

#define PROXY_METHOD1(r, method, t1)\
    r method(t1 a1) OVERRIDE {\
      MethodCall1<C, r, t1> call(c_.get(), &C::method, a1);\
      return call.Marshal(owner_thread_);\
    }\

#define PROXY_CONSTMETHOD1(r, method, t1)\
    r method(t1 a1) const OVERRIDE {\
      ConstMethodCall1<C, r, t1> call(c_.get(), &C::method, a1);\
      return call.Marshal(owner_thread_);\
    }\

#define PROXY_METHOD2(r, method, t1, t2)\
    r method(t1 a1, t2 a2) OVERRIDE {\
      MethodCall2<C, r, t1, t2> call(c_.get(), &C::method, a1, a2);\
      return call.Marshal(owner_thread_);\
    }\

#define PROXY_METHOD3(r, method, t1, t2, t3)\
    r method(t1 a1, t2 a2, t3 a3) OVERRIDE {\
      MethodCall3<C, r, t1, t2, t3> call(c_.get(), &C::method, a1, a2, a3);\
      return call.Marshal(owner_thread_);\
    }\

#define END_PROXY() \
   private:\
    void Release_s() {\
      c_ = NULL;\
    }\
    mutable talk_base::Thread* owner_thread_;\
    talk_base::scoped_refptr<C> c_;\
  };\

}  // namespace webrtc

#endif  //  TALK_APP_WEBRTC_PROXY_H_
