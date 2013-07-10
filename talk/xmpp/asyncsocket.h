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

#ifndef _ASYNCSOCKET_H_
#define _ASYNCSOCKET_H_

#include "talk/base/sigslot.h"

namespace talk_base {
  class SocketAddress;
}

namespace buzz {

class AsyncSocket {
public:
  enum State {
    STATE_CLOSED = 0,      //!< Socket is not open.
    STATE_CLOSING,         //!< Socket is closing but can have buffered data
    STATE_CONNECTING,      //!< In the process of
    STATE_OPEN,            //!< Socket is connected
#if defined(FEATURE_ENABLE_SSL)
    STATE_TLS_CONNECTING,  //!< Establishing TLS connection
    STATE_TLS_OPEN,        //!< TLS connected
#endif
  };

  enum Error {
    ERROR_NONE = 0,         //!< No error
    ERROR_WINSOCK,          //!< Winsock error
    ERROR_DNS,              //!< Couldn't resolve host name
    ERROR_WRONGSTATE,       //!< Call made while socket is in the wrong state
#if defined(FEATURE_ENABLE_SSL)
    ERROR_SSL,              //!< Something went wrong with OpenSSL
#endif
  };

  virtual ~AsyncSocket() {}
  virtual State state() = 0;
  virtual Error error() = 0;
  virtual int GetError() = 0;    // winsock error code

  virtual bool Connect(const talk_base::SocketAddress& addr) = 0;
  virtual bool Read(char * data, size_t len, size_t* len_read) = 0;
  virtual bool Write(const char * data, size_t len) = 0;
  virtual bool Close() = 0;
#if defined(FEATURE_ENABLE_SSL)
  // We allow matching any passed domain.  This allows us to avoid
  // handling the valuable certificates for logins into proxies.  If
  // both names are passed as empty, we do not require a match.
  virtual bool StartTls(const std::string & domainname) = 0;
#endif

  sigslot::signal0<> SignalConnected;
  sigslot::signal0<> SignalSSLConnected;
  sigslot::signal0<> SignalClosed;
  sigslot::signal0<> SignalRead;
  sigslot::signal0<> SignalError;
};

}

#endif
