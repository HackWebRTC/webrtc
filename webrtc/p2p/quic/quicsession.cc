/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/quic/quicsession.h"

#include <string>
#include <utility>

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/messagehandler.h"
#include "webrtc/base/messagequeue.h"

namespace cricket {

QuicSession::QuicSession(rtc::scoped_ptr<net::QuicConnection> connection,
                         const net::QuicConfig& config)
    : net::QuicSession(connection.release(), config) {}

QuicSession::~QuicSession() {}

void QuicSession::StartClientHandshake(
    net::QuicCryptoClientStream* crypto_stream) {
  SetCryptoStream(crypto_stream);
  net::QuicSession::Initialize();
  crypto_stream->CryptoConnect();
}

void QuicSession::StartServerHandshake(
    net::QuicCryptoServerStream* crypto_stream) {
  SetCryptoStream(crypto_stream);
  net::QuicSession::Initialize();
}

void QuicSession::SetCryptoStream(net::QuicCryptoStream* crypto_stream) {
  crypto_stream_.reset(crypto_stream);
}

bool QuicSession::ExportKeyingMaterial(base::StringPiece label,
                                       base::StringPiece context,
                                       size_t result_len,
                                       string* result) {
  return crypto_stream_->ExportKeyingMaterial(label, context, result_len,
                                              result);
}

void QuicSession::OnCryptoHandshakeEvent(CryptoHandshakeEvent event) {
  net::QuicSession::OnCryptoHandshakeEvent(event);
  if (event == HANDSHAKE_CONFIRMED) {
    LOG(LS_INFO) << "QuicSession handshake complete";
    RTC_DCHECK(IsEncryptionEstablished());
    RTC_DCHECK(IsCryptoHandshakeConfirmed());

    SignalHandshakeComplete();
  }
}

ReliableQuicStream* QuicSession::CreateIncomingDynamicStream(
    net::QuicStreamId id) {
  ReliableQuicStream* stream = CreateDataStream(id);
  if (stream) {
    SignalIncomingStream(stream);
  }
  return stream;
}

ReliableQuicStream* QuicSession::CreateOutgoingDynamicStream(
    net::SpdyPriority priority) {
  ReliableQuicStream* stream = CreateDataStream(GetNextOutgoingStreamId());
  if (stream) {
    ActivateStream(stream);  // QuicSession owns the stream.
  }
  return stream;
}

ReliableQuicStream* QuicSession::CreateDataStream(net::QuicStreamId id) {
  if (crypto_stream_ == nullptr || !crypto_stream_->encryption_established()) {
    // Encryption not active so no stream created
    return nullptr;
  }
  return new ReliableQuicStream(id, this);
}

void QuicSession::OnConnectionClosed(net::QuicErrorCode error,
                                     net::ConnectionCloseSource source) {
  net::QuicSession::OnConnectionClosed(error, source);
  SignalConnectionClosed(error,
                         source == net::ConnectionCloseSource::FROM_PEER);
}

bool QuicSession::OnReadPacket(const char* data, size_t data_len) {
  net::QuicEncryptedPacket packet(data, data_len);
  connection()->ProcessUdpPacket(connection()->self_address(),
                                 connection()->peer_address(), packet);
  return true;
}

}  // namespace cricket
