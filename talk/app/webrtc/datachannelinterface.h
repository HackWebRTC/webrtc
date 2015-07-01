/*
 * libjingle
 * Copyright 2012 Google Inc.
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

// This file contains interfaces for DataChannels
// http://dev.w3.org/2011/webrtc/editor/webrtc.html#rtcdatachannel

#ifndef TALK_APP_WEBRTC_DATACHANNELINTERFACE_H_
#define TALK_APP_WEBRTC_DATACHANNELINTERFACE_H_

#include <string>

#include "webrtc/base/basictypes.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/refcount.h"


namespace webrtc {

struct DataChannelInit {
  DataChannelInit()
      : reliable(false),
        ordered(true),
        maxRetransmitTime(-1),
        maxRetransmits(-1),
        negotiated(false),
        id(-1) {
  }

  bool reliable;           // Deprecated.
  bool ordered;            // True if ordered delivery is required.
  int maxRetransmitTime;   // The max period of time in milliseconds in which
                           // retransmissions will be sent.  After this time, no
                           // more retransmissions will be sent. -1 if unset.
  int maxRetransmits;      // The max number of retransmissions. -1 if unset.
  std::string protocol;    // This is set by the application and opaque to the
                           // WebRTC implementation.
  bool negotiated;         // True if the channel has been externally negotiated
                           // and we do not send an in-band signalling in the
                           // form of an "open" message.
  int id;                  // The stream id, or SID, for SCTP data channels. -1
                           // if unset.
};

struct DataBuffer {
  DataBuffer(const rtc::Buffer& data, bool binary)
      : data(data),
        binary(binary) {
  }
  // For convenience for unit tests.
  explicit DataBuffer(const std::string& text)
      : data(text.data(), text.length()),
        binary(false) {
  }
  size_t size() const { return data.size(); }

  rtc::Buffer data;
  // Indicates if the received data contains UTF-8 or binary data.
  // Note that the upper layers are left to verify the UTF-8 encoding.
  // TODO(jiayl): prefer to use an enum instead of a bool.
  bool binary;
};

class DataChannelObserver {
 public:
  // The data channel state have changed.
  virtual void OnStateChange() = 0;
  //  A data buffer was successfully received.
  virtual void OnMessage(const DataBuffer& buffer) = 0;
  // The data channel's buffered_amount has changed.
  virtual void OnBufferedAmountChange(uint64 previous_amount){};

 protected:
  virtual ~DataChannelObserver() {}
};

class DataChannelInterface : public rtc::RefCountInterface {
 public:
  // Keep in sync with DataChannel.java:State and
  // RTCDataChannel.h:RTCDataChannelState.
  enum DataState {
    kConnecting,
    kOpen,  // The DataChannel is ready to send data.
    kClosing,
    kClosed
  };

  static const char* DataStateString(DataState state) {
    switch (state) {
      case kConnecting:
        return "connecting";
      case kOpen:
        return "open";
      case kClosing:
        return "closing";
      case kClosed:
        return "closed";
    }
    CHECK(false) << "Unknown DataChannel state: " << state;
    return "";
  }

  virtual void RegisterObserver(DataChannelObserver* observer) = 0;
  virtual void UnregisterObserver() = 0;
  // The label attribute represents a label that can be used to distinguish this
  // DataChannel object from other DataChannel objects.
  virtual std::string label() const = 0;
  virtual bool reliable() const = 0;

  // TODO(tommyw): Remove these dummy implementations when all classes have
  // implemented these APIs. They should all just return the values the
  // DataChannel was created with.
  virtual bool ordered() const { return false; }
  virtual uint16 maxRetransmitTime() const { return 0; }
  virtual uint16 maxRetransmits() const { return 0; }
  virtual std::string protocol() const { return std::string(); }
  virtual bool negotiated() const { return false; }

  virtual int id() const = 0;
  virtual DataState state() const = 0;
  // The buffered_amount returns the number of bytes of application data
  // (UTF-8 text and binary data) that have been queued using SendBuffer but
  // have not yet been transmitted to the network.
  virtual uint64 buffered_amount() const = 0;
  virtual void Close() = 0;
  // Sends |data| to the remote peer.
  virtual bool Send(const DataBuffer& buffer) = 0;

 protected:
  virtual ~DataChannelInterface() {}
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_DATACHANNELINTERFACE_H_
