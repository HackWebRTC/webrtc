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

#include "talk/p2p/base/asyncstuntcpsocket.h"

#include <cstring>

#include "talk/base/common.h"
#include "talk/base/logging.h"
#include "talk/p2p/base/stun.h"

namespace cricket {

static const size_t kMaxPacketSize = 64 * 1024;

typedef uint16 PacketLength;
static const size_t kPacketLenSize = sizeof(PacketLength);
static const size_t kPacketLenOffset = 2;
static const size_t kBufSize = kMaxPacketSize + kStunHeaderSize;
static const size_t kTurnChannelDataHdrSize = 4;

inline bool IsStunMessage(uint16 msg_type) {
  // The first two bits of a channel data message are 0b01.
  return (msg_type & 0xC000) ? false : true;
}

// AsyncStunTCPSocket
// Binds and connects |socket| and creates AsyncTCPSocket for
// it. Takes ownership of |socket|. Returns NULL if bind() or
// connect() fail (|socket| is destroyed in that case).
AsyncStunTCPSocket* AsyncStunTCPSocket::Create(
    talk_base::AsyncSocket* socket,
    const talk_base::SocketAddress& bind_address,
    const talk_base::SocketAddress& remote_address) {
  return new AsyncStunTCPSocket(AsyncTCPSocketBase::ConnectSocket(
      socket, bind_address, remote_address), false);
}

AsyncStunTCPSocket::AsyncStunTCPSocket(
    talk_base::AsyncSocket* socket, bool listen)
    : talk_base::AsyncTCPSocketBase(socket, listen, kBufSize) {
}

// TODO(mallinath) - Add support of setting DSCP code on AsyncSocket.
int AsyncStunTCPSocket::Send(const void *pv, size_t cb,
                             talk_base::DiffServCodePoint dscp) {
  if (cb > kBufSize || cb < kPacketLenSize + kPacketLenOffset) {
    SetError(EMSGSIZE);
    return -1;
  }

  // If we are blocking on send, then silently drop this packet
  if (!IsOutBufferEmpty())
    return static_cast<int>(cb);

  int pad_bytes;
  size_t expected_pkt_len = GetExpectedLength(pv, cb, &pad_bytes);

  // Accepts only complete STUN/ChannelData packets.
  if (cb != expected_pkt_len)
    return -1;

  AppendToOutBuffer(pv, cb);

  ASSERT(pad_bytes < 4);
  char padding[4] = {0};
  AppendToOutBuffer(padding, pad_bytes);

  int res = FlushOutBuffer();
  if (res <= 0) {
    // drop packet if we made no progress
    ClearOutBuffer();
    return res;
  }

  // We claim to have sent the whole thing, even if we only sent partial
  return static_cast<int>(cb);
}

void AsyncStunTCPSocket::ProcessInput(char* data, size_t* len) {
  talk_base::SocketAddress remote_addr(GetRemoteAddress());
  // STUN packet - First 4 bytes. Total header size is 20 bytes.
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |0 0|     STUN Message Type     |         Message Length        |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  // TURN ChannelData
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |         Channel Number        |            Length             |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  while (true) {
    // We need at least 4 bytes to read the STUN or ChannelData packet length.
    if (*len < kPacketLenOffset + kPacketLenSize)
      return;

    int pad_bytes;
    size_t expected_pkt_len = GetExpectedLength(data, *len, &pad_bytes);
    size_t actual_length = expected_pkt_len + pad_bytes;

    if (*len < actual_length) {
      return;
    }

    SignalReadPacket(this, data, expected_pkt_len, remote_addr);

    *len -= actual_length;
    if (*len > 0) {
      memmove(data, data + actual_length, *len);
    }
  }
}

void AsyncStunTCPSocket::HandleIncomingConnection(
    talk_base::AsyncSocket* socket) {
  SignalNewConnection(this, new AsyncStunTCPSocket(socket, false));
}

size_t AsyncStunTCPSocket::GetExpectedLength(const void* data, size_t len,
                                             int* pad_bytes) {
  *pad_bytes = 0;
  PacketLength pkt_len =
      talk_base::GetBE16(static_cast<const char*>(data) + kPacketLenOffset);
  size_t expected_pkt_len;
  uint16 msg_type = talk_base::GetBE16(data);
  if (IsStunMessage(msg_type)) {
    // STUN message.
    expected_pkt_len = kStunHeaderSize + pkt_len;
  } else {
    // TURN ChannelData message.
    expected_pkt_len = kTurnChannelDataHdrSize + pkt_len;
    // From RFC 5766 section 11.5
    // Over TCP and TLS-over-TCP, the ChannelData message MUST be padded to
    // a multiple of four bytes in order to ensure the alignment of
    // subsequent messages.  The padding is not reflected in the length
    // field of the ChannelData message, so the actual size of a ChannelData
    // message (including padding) is (4 + Length) rounded up to the nearest
    // multiple of 4.  Over UDP, the padding is not required but MAY be
    // included.
    if (expected_pkt_len % 4)
      *pad_bytes = 4 - (expected_pkt_len % 4);
  }
  return expected_pkt_len;
}

}  // namespace cricket
