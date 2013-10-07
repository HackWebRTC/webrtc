/*
 * libjingle SCTP
 * Copyright 2012 Google Inc, and Robin Seggelmann
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

#include "talk/media/sctp/sctpdataengine.h"

#include <stdarg.h>
#include <stdio.h>
#include <vector>

#include "talk/base/buffer.h"
#include "talk/base/helpers.h"
#include "talk/base/logging.h"
#include "talk/media/base/codec.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/streamparams.h"
#include "usrsctplib/usrsctp.h"

namespace cricket {

// This is the SCTP port to use. It is passed along the wire and the listener
// and connector must be using the same port. It is not related to the ports at
// the IP level. (Corresponds to: sockaddr_conn.sconn_port in usrsctp.h)
//
// TODO(ldixon): Allow port to be set from higher level code.
static const int kSctpDefaultPort = 5001;
// TODO(ldixon): Find where this is defined, and also check is Sctp really
// respects this.
static const size_t kSctpMtu = 1280;

enum {
  MSG_SCTPINBOUNDPACKET = 1,   // MessageData is SctpInboundPacket
  MSG_SCTPOUTBOUNDPACKET = 2,  // MessageData is talk_base:Buffer
};

struct SctpInboundPacket {
  talk_base::Buffer buffer;
  ReceiveDataParams params;
  // The |flags| parameter is used by SCTP to distinguish notification packets
  // from other types of packets.
  int flags;
};

// Helper for logging SCTP messages.
static void debug_sctp_printf(const char *format, ...) {
  char s[255];
  va_list ap;
  va_start(ap, format);
  vsnprintf(s, sizeof(s), format, ap);
  LOG(LS_INFO) << "SCTP: " << s;
  va_end(ap);
}

// Get the PPID to use for the terminating fragment of this type.
static SctpDataMediaChannel::PayloadProtocolIdentifier GetPpid(
    cricket::DataMessageType type) {
  switch (type) {
  default:
  case cricket::DMT_NONE:
    return SctpDataMediaChannel::PPID_NONE;
  case cricket::DMT_CONTROL:
    return SctpDataMediaChannel::PPID_CONTROL;
  case cricket::DMT_BINARY:
    return SctpDataMediaChannel::PPID_BINARY_LAST;
  case cricket::DMT_TEXT:
    return SctpDataMediaChannel::PPID_TEXT_LAST;
  };
}

static bool GetDataMediaType(
    SctpDataMediaChannel::PayloadProtocolIdentifier ppid,
    cricket::DataMessageType *dest) {
  ASSERT(dest != NULL);
  switch (ppid) {
    case SctpDataMediaChannel::PPID_BINARY_PARTIAL:
    case SctpDataMediaChannel::PPID_BINARY_LAST:
      *dest = cricket::DMT_BINARY;
      return true;

    case SctpDataMediaChannel::PPID_TEXT_PARTIAL:
    case SctpDataMediaChannel::PPID_TEXT_LAST:
      *dest = cricket::DMT_TEXT;
      return true;

    case SctpDataMediaChannel::PPID_CONTROL:
      *dest = cricket::DMT_CONTROL;
      return true;

    case SctpDataMediaChannel::PPID_NONE:
      *dest = cricket::DMT_NONE;
      return true;

    default:
      return false;
  }
}

// This is the callback usrsctp uses when there's data to send on the network
// that has been wrapped appropriatly for the SCTP protocol.
static int OnSctpOutboundPacket(void* addr, void* data, size_t length,
                                uint8_t tos, uint8_t set_df) {
  SctpDataMediaChannel* channel = static_cast<SctpDataMediaChannel*>(addr);
  LOG(LS_VERBOSE) << "global OnSctpOutboundPacket():"
                  << "addr: " << addr << "; length: " << length
                  << "; tos: " << std::hex << static_cast<int>(tos)
                  << "; set_df: " << std::hex << static_cast<int>(set_df);
  // Note: We have to copy the data; the caller will delete it.
  talk_base::Buffer* buffer = new talk_base::Buffer(data, length);
  channel->worker_thread()->Post(channel, MSG_SCTPOUTBOUNDPACKET,
                                 talk_base::WrapMessageData(buffer));
  return 0;
}

// This is the callback called from usrsctp when data has been received, after
// a packet has been interpreted and parsed by usrsctp and found to contain
// payload data. It is called by a usrsctp thread. It is assumed this function
// will free the memory used by 'data'.
static int OnSctpInboundPacket(struct socket* sock, union sctp_sockstore addr,
                               void* data, size_t length,
                               struct sctp_rcvinfo rcv, int flags,
                               void* ulp_info) {
  SctpDataMediaChannel* channel = static_cast<SctpDataMediaChannel*>(ulp_info);
  // Post data to the channel's receiver thread (copying it).
  // TODO(ldixon): Unclear if copy is needed as this method is responsible for
  // memory cleanup. But this does simplify code.
  const SctpDataMediaChannel::PayloadProtocolIdentifier ppid =
      static_cast<SctpDataMediaChannel::PayloadProtocolIdentifier>(
          talk_base::HostToNetwork32(rcv.rcv_ppid));
  cricket::DataMessageType type = cricket::DMT_NONE;
  if (!GetDataMediaType(ppid, &type) && !(flags & MSG_NOTIFICATION)) {
    // It's neither a notification nor a recognized data packet.  Drop it.
    LOG(LS_ERROR) << "Received an unknown PPID " << ppid
                  << " on an SCTP packet.  Dropping.";
  } else {
    SctpInboundPacket* packet = new SctpInboundPacket;
    packet->buffer.SetData(data, length);
    packet->params.ssrc = rcv.rcv_sid;
    packet->params.seq_num = rcv.rcv_ssn;
    packet->params.timestamp = rcv.rcv_tsn;
    packet->params.type = type;
    packet->flags = flags;
    channel->worker_thread()->Post(channel, MSG_SCTPINBOUNDPACKET,
                                   talk_base::WrapMessageData(packet));
  }
  free(data);
  return 1;
}

// Set the initial value of the static SCTP Data Engines reference count.
int SctpDataEngine::usrsctp_engines_count = 0;

SctpDataEngine::SctpDataEngine() {
  if (usrsctp_engines_count == 0) {
    // First argument is udp_encapsulation_port, which is not releveant for our
    // AF_CONN use of sctp.
    usrsctp_init(0, cricket::OnSctpOutboundPacket, debug_sctp_printf);

    // To turn on/off detailed SCTP debugging. You will also need to have the
    // SCTP_DEBUG cpp defines flag.
    // usrsctp_sysctl_set_sctp_debug_on(SCTP_DEBUG_ALL);

    // TODO(ldixon): Consider turning this on/off.
    usrsctp_sysctl_set_sctp_ecn_enable(0);

    // TODO(ldixon): Consider turning this on/off.
    // This is not needed right now (we don't do dynamic address changes):
    // If SCTP Auto-ASCONF is enabled, the peer is informed automatically
    // when a new address is added or removed. This feature is enabled by
    // default.
    // usrsctp_sysctl_set_sctp_auto_asconf(0);

    // TODO(ldixon): Consider turning this on/off.
    // Add a blackhole sysctl. Setting it to 1 results in no ABORTs
    // being sent in response to INITs, setting it to 2 results
    // in no ABORTs being sent for received OOTB packets.
    // This is similar to the TCP sysctl.
    //
    // See: http://lakerest.net/pipermail/sctp-coders/2012-January/009438.html
    // See: http://svnweb.freebsd.org/base?view=revision&revision=229805
    // usrsctp_sysctl_set_sctp_blackhole(2);

    // Set the number of default outgoing streams.  This is the number we'll
    // send in the SCTP INIT message.  The 'appropriate default' in the
    // second paragraph of
    // http://tools.ietf.org/html/draft-ietf-rtcweb-data-channel-05#section-6.2
    // is cricket::kMaxSctpSid.
    usrsctp_sysctl_set_sctp_nr_outgoing_streams_default(
        cricket::kMaxSctpSid);
  }
  usrsctp_engines_count++;

  // We don't put in a codec because we don't want one offered when we
  // use the hybrid data engine.
  // codecs_.push_back(cricket::DataCodec( kGoogleSctpDataCodecId,
  // kGoogleSctpDataCodecName, 0));
}

SctpDataEngine::~SctpDataEngine() {
  // TODO(ldixon): There is currently a bug in teardown of usrsctp that blocks
  // indefintely if a finish call made too soon after close calls. So teardown
  // has been skipped. Once the bug is fixed, retest and enable teardown.
  //
  // usrsctp_engines_count--;
  // LOG(LS_VERBOSE) << "usrsctp_engines_count:" << usrsctp_engines_count;
  // if (usrsctp_engines_count == 0) {
  //   if (usrsctp_finish() != 0) {
  //     LOG(LS_WARNING) << "usrsctp_finish.";
  //   }
  // }
}

DataMediaChannel* SctpDataEngine::CreateChannel(
    DataChannelType data_channel_type) {
  if (data_channel_type != DCT_SCTP) {
    return NULL;
  }
  return new SctpDataMediaChannel(talk_base::Thread::Current());
}

SctpDataMediaChannel::SctpDataMediaChannel(talk_base::Thread* thread)
    : worker_thread_(thread),
      local_port_(-1),
      remote_port_(-1),
      sock_(NULL),
      sending_(false),
      receiving_(false),
      debug_name_("SctpDataMediaChannel") {
}

SctpDataMediaChannel::~SctpDataMediaChannel() {
  CloseSctpSocket();
}

sockaddr_conn SctpDataMediaChannel::GetSctpSockAddr(int port) {
  sockaddr_conn sconn = {0};
  sconn.sconn_family = AF_CONN;
#ifdef HAVE_SCONN_LEN
  sconn.sconn_len = sizeof(sockaddr_conn);
#endif
  // Note: conversion from int to uint16_t happens here.
  sconn.sconn_port = talk_base::HostToNetwork16(port);
  sconn.sconn_addr = this;
  return sconn;
}

bool SctpDataMediaChannel::OpenSctpSocket() {
  if (sock_) {
    LOG(LS_VERBOSE) << debug_name_
                    << "->Ignoring attempt to re-create existing socket.";
    return false;
  }
  sock_ = usrsctp_socket(AF_CONN, SOCK_STREAM, IPPROTO_SCTP,
                         cricket::OnSctpInboundPacket, NULL, 0, this);
  if (!sock_) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed to create SCTP socket.";
    return false;
  }

  // Make the socket non-blocking. Connect, close, shutdown etc will not block
  // the thread waiting for the socket operation to complete.
  if (usrsctp_set_non_blocking(sock_, 1) < 0) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed to set SCTP to non blocking.";
    return false;
  }

  // This ensures that the usrsctp close call deletes the association. This
  // prevents usrsctp from calling OnSctpOutboundPacket with references to
  // this class as the address.
  linger linger_opt;
  linger_opt.l_onoff = 1;
  linger_opt.l_linger = 0;
  if (usrsctp_setsockopt(sock_, SOL_SOCKET, SO_LINGER, &linger_opt,
                         sizeof(linger_opt))) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed to set SO_LINGER.";
    return false;
  }

  uint32_t nodelay = 1;
  if (usrsctp_setsockopt(sock_, IPPROTO_SCTP, SCTP_NODELAY, &nodelay,
                         sizeof(nodelay))) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed to set SCTP_NODELAY.";
    return false;
  }

  // Subscribe to SCTP event notifications.
  int event_types[] = {SCTP_ASSOC_CHANGE,
                       SCTP_PEER_ADDR_CHANGE,
                       SCTP_SEND_FAILED_EVENT,
                       SCTP_SENDER_DRY_EVENT};
  struct sctp_event event = {0};
  event.se_assoc_id = SCTP_ALL_ASSOC;
  event.se_on = 1;
  for (size_t i = 0; i < ARRAY_SIZE(event_types); i++) {
    event.se_type = event_types[i];
    if (usrsctp_setsockopt(sock_, IPPROTO_SCTP, SCTP_EVENT, &event,
                           sizeof(event)) < 0) {
      LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed to set SCTP_EVENT type: "
                          << event.se_type;
      return false;
    }
  }

  // Register this class as an address for usrsctp. This is used by SCTP to
  // direct the packets received (by the created socket) to this class.
  usrsctp_register_address(this);
  sending_ = true;
  return true;
}

void SctpDataMediaChannel::CloseSctpSocket() {
  sending_ = false;
  if (sock_) {
    // We assume that SO_LINGER option is set to close the association when
    // close is called. This means that any pending packets in usrsctp will be
    // discarded instead of being sent.
    usrsctp_close(sock_);
    sock_ = NULL;
    usrsctp_deregister_address(this);
  }
}

bool SctpDataMediaChannel::Connect() {
  LOG(LS_VERBOSE) << debug_name_ << "->Connect().";
  if (remote_port_ < 0) {
    remote_port_ = kSctpDefaultPort;
  }
  if (local_port_ < 0) {
    local_port_ = kSctpDefaultPort;
  }

  // If we already have a socket connection, just return.
  if (sock_) {
    LOG(LS_WARNING) << debug_name_ << "->Connect(): Ignored as socket "
                                      "is already established.";
    return true;
  }

  // If no socket (it was closed) try to start it again. This can happen when
  // the socket we are connecting to closes, does an sctp shutdown handshake,
  // or behaves unexpectedly causing us to perform a CloseSctpSocket.
  if (!sock_ && !OpenSctpSocket()) {
    return false;
  }

  // Note: conversion from int to uint16_t happens on assignment.
  sockaddr_conn local_sconn = GetSctpSockAddr(local_port_);
  if (usrsctp_bind(sock_, reinterpret_cast<sockaddr *>(&local_sconn),
                   sizeof(local_sconn)) < 0) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "->Connect(): "
                        << ("Failed usrsctp_bind");
    CloseSctpSocket();
    return false;
  }

  // Note: conversion from int to uint16_t happens on assignment.
  sockaddr_conn remote_sconn = GetSctpSockAddr(remote_port_);
  int connect_result = usrsctp_connect(
      sock_, reinterpret_cast<sockaddr *>(&remote_sconn), sizeof(remote_sconn));
  if (connect_result < 0 && errno != SCTP_EINPROGRESS) {
    LOG_ERRNO(LS_ERROR) << debug_name_ << "Failed usrsctp_connect. got errno="
                        << errno << ", but wanted " << SCTP_EINPROGRESS;
    CloseSctpSocket();
    return false;
  }
  return true;
}

void SctpDataMediaChannel::Disconnect() {
  // TODO(ldixon): Consider calling |usrsctp_shutdown(sock_, ...)| to do a
  // shutdown handshake and remove the association.
  CloseSctpSocket();
}

bool SctpDataMediaChannel::SetSend(bool send) {
  if (!sending_ && send) {
    return Connect();
  }
  if (sending_ && !send) {
    Disconnect();
  }
  return true;
}

bool SctpDataMediaChannel::SetReceive(bool receive) {
  receiving_ = receive;
  return true;
}

bool SctpDataMediaChannel::AddSendStream(const StreamParams& stream) {
  if (!stream.has_ssrcs()) {
    return false;
  }

  StreamParams found_stream;
  // TODO(lally): Consider keeping this sorted.
  if (GetStreamBySsrc(streams_, stream.first_ssrc(), &found_stream)) {
    LOG(LS_WARNING) << debug_name_ << "->AddSendStream(...): "
                    << "Not adding data send stream '" << stream.id
                    << "' with ssrc=" << stream.first_ssrc()
                    << " because stream already exists.";
    return false;
  }

  streams_.push_back(stream);
  return true;
}

bool SctpDataMediaChannel::RemoveSendStream(uint32 ssrc) {
  StreamParams found_stream;
  if (!GetStreamBySsrc(streams_, ssrc, &found_stream)) {
    return false;
  }

  RemoveStreamBySsrc(&streams_, ssrc);
  return true;
}

// Note: expects exactly one ssrc.  If none are given, it will fail.  If more
// than one are given, it will use the first.
bool SctpDataMediaChannel::AddRecvStream(const StreamParams& stream) {
  if (!stream.has_ssrcs()) {
    return false;
  }

  StreamParams found_stream;
  if (GetStreamBySsrc(streams_, stream.first_ssrc(), &found_stream)) {
    LOG(LS_WARNING) << debug_name_ << "->AddRecvStream(...): "
                    << "Not adding data recv stream '" << stream.id
                    << "' with ssrc=" << stream.first_ssrc()
                    << " because stream already exists.";
    return false;
  }

  streams_.push_back(stream);
  LOG(LS_VERBOSE) << debug_name_ << "->AddRecvStream(...): "
                  << "Added data recv stream '" << stream.id
                  << "' with ssrc=" << stream.first_ssrc();
  return true;
}

bool SctpDataMediaChannel::RemoveRecvStream(uint32 ssrc) {
  RemoveStreamBySsrc(&streams_, ssrc);
  return true;
}

bool SctpDataMediaChannel::SendData(
    const SendDataParams& params,
    const talk_base::Buffer& payload,
    SendDataResult* result) {
  if (result) {
    // Preset |result| to assume an error.  If SendData succeeds, we'll
    // overwrite |*result| once more at the end.
    *result = SDR_ERROR;
  }

  if (!sending_) {
    LOG(LS_WARNING) << debug_name_ << "->SendData(...): "
                    << "Not sending packet with ssrc=" << params.ssrc
                    << " len=" << payload.length() << " before SetSend(true).";
    return false;
  }

  StreamParams found_stream;
  if (params.type != cricket::DMT_CONTROL &&
      !GetStreamBySsrc(streams_, params.ssrc, &found_stream)) {
    LOG(LS_WARNING) << debug_name_ << "->SendData(...): "
                    << "Not sending data because ssrc is unknown: "
                    << params.ssrc;
    return false;
  }

  //
  // Send data using SCTP.
  ssize_t send_res = 0;  // result from usrsctp_sendv.
  struct sctp_sendv_spa spa = {0};
  spa.sendv_flags |= SCTP_SEND_SNDINFO_VALID;
  spa.sendv_sndinfo.snd_sid = params.ssrc;
  spa.sendv_sndinfo.snd_ppid = talk_base::HostToNetwork32(
      GetPpid(params.type));

  // Ordered implies reliable.
  if (!params.ordered) {
    spa.sendv_sndinfo.snd_flags |= SCTP_UNORDERED;
    if (params.max_rtx_count >= 0 || params.max_rtx_ms == 0) {
      spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
      spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_RTX;
      spa.sendv_prinfo.pr_value = params.max_rtx_count;
    } else {
      spa.sendv_flags |= SCTP_SEND_PRINFO_VALID;
      spa.sendv_prinfo.pr_policy = SCTP_PR_SCTP_TTL;
      spa.sendv_prinfo.pr_value = params.max_rtx_ms;
    }
  }

  // We don't fragment.
  send_res = usrsctp_sendv(sock_, payload.data(),
                           static_cast<size_t>(payload.length()),
                           NULL, 0, &spa,
                           static_cast<socklen_t>(sizeof(spa)),
                           SCTP_SENDV_SPA, 0);
  if (send_res < 0) {
    if (errno == EWOULDBLOCK) {
      *result = SDR_BLOCK;
      LOG(LS_INFO) << debug_name_ << "->SendData(...): EWOULDBLOCK returned";
    } else {
      LOG_ERRNO(LS_ERROR) << "ERROR:" << debug_name_
                          << "->SendData(...): "
                          << " usrsctp_sendv: ";
    }
    return false;
  }
  if (result) {
    // Only way out now is success.
    *result = SDR_SUCCESS;
  }
  return true;
}

// Called by network interface when a packet has been received.
void SctpDataMediaChannel::OnPacketReceived(talk_base::Buffer* packet) {
  LOG(LS_VERBOSE) << debug_name_ << "->OnPacketReceived(...): " << " length="
                  << packet->length() << ", sending: " << sending_;
  // Only give receiving packets to usrsctp after if connected. This enables two
  // peers to each make a connect call, but for them not to receive an INIT
  // packet before they have called connect; least the last receiver of the INIT
  // packet will have called connect, and a connection will be established.
  if (sending_) {
    // Pass received packet to SCTP stack. Once processed by usrsctp, the data
    // will be will be given to the global OnSctpInboundData, and then,
    // marshalled by a Post and handled with OnMessage.
    usrsctp_conninput(this, packet->data(), packet->length(), 0);
  } else {
    // TODO(ldixon): Consider caching the packet for very slightly better
    // reliability.
  }
}

void SctpDataMediaChannel::OnInboundPacketFromSctpToChannel(
    SctpInboundPacket* packet) {
  LOG(LS_VERBOSE) << debug_name_ << "->OnInboundPacketFromSctpToChannel(...): "
                  << "Received SCTP data:"
                  << " ssrc=" << packet->params.ssrc
                  << " notification: " << (packet->flags & MSG_NOTIFICATION)
                  << " length=" << packet->buffer.length();
  // Sending a packet with data == NULL (no data) is SCTPs "close the
  // connection" message. This sets sock_ = NULL;
  if (!packet->buffer.length() || !packet->buffer.data()) {
    LOG(LS_INFO) << debug_name_ << "->OnInboundPacketFromSctpToChannel(...): "
                                   "No data, closing.";
    return;
  }
  if (packet->flags & MSG_NOTIFICATION) {
    OnNotificationFromSctp(&packet->buffer);
  } else {
    OnDataFromSctpToChannel(packet->params, &packet->buffer);
  }
}

void SctpDataMediaChannel::OnDataFromSctpToChannel(
    const ReceiveDataParams& params, talk_base::Buffer* buffer) {
  StreamParams found_stream;
  if (!GetStreamBySsrc(streams_, params.ssrc, &found_stream)) {
    if (params.type == DMT_CONTROL) {
      SignalDataReceived(params, buffer->data(), buffer->length());
    } else {
      LOG(LS_WARNING) << debug_name_ << "->OnDataFromSctpToChannel(...): "
                      << "Received packet for unknown ssrc: " << params.ssrc;
    }
    return;
  }

  if (receiving_) {
    LOG(LS_VERBOSE) << debug_name_ << "->OnDataFromSctpToChannel(...): "
                    << "Posting with length: " << buffer->length();
    SignalDataReceived(params, buffer->data(), buffer->length());
  } else {
    LOG(LS_WARNING) << debug_name_ << "->OnDataFromSctpToChannel(...): "
                    << "Not receiving packet with sid=" << params.ssrc
                    << " len=" <<  buffer->length()
                    << " before SetReceive(true).";
  }
}

void SctpDataMediaChannel::OnNotificationFromSctp(talk_base::Buffer* buffer) {
  const sctp_notification& notification =
      reinterpret_cast<const sctp_notification&>(*buffer->data());
  ASSERT(notification.sn_header.sn_length == buffer->length());

  // TODO(ldixon): handle notifications appropriately.
  switch (notification.sn_header.sn_type) {
    case SCTP_ASSOC_CHANGE:
      LOG(LS_VERBOSE) << "SCTP_ASSOC_CHANGE";
      OnNotificationAssocChange(notification.sn_assoc_change);
      break;
    case SCTP_REMOTE_ERROR:
      LOG(LS_INFO) << "SCTP_REMOTE_ERROR";
      break;
    case SCTP_SHUTDOWN_EVENT:
      LOG(LS_INFO) << "SCTP_SHUTDOWN_EVENT";
      break;
    case SCTP_ADAPTATION_INDICATION:
      LOG(LS_INFO) << "SCTP_ADAPTATION_INIDICATION";
      break;
    case SCTP_PARTIAL_DELIVERY_EVENT:
      LOG(LS_INFO) << "SCTP_PARTIAL_DELIVERY_EVENT";
      break;
    case SCTP_AUTHENTICATION_EVENT:
      LOG(LS_INFO) << "SCTP_AUTHENTICATION_EVENT";
      break;
    case SCTP_SENDER_DRY_EVENT:
      LOG(LS_INFO) << "SCTP_SENDER_DRY_EVENT";
      SignalReadyToSend(true);
      break;
    // TODO(ldixon): Unblock after congestion.
    case SCTP_NOTIFICATIONS_STOPPED_EVENT:
      LOG(LS_INFO) << "SCTP_NOTIFICATIONS_STOPPED_EVENT";
      break;
    case SCTP_SEND_FAILED_EVENT:
      LOG(LS_INFO) << "SCTP_SEND_FAILED_EVENT";
      break;
    case SCTP_STREAM_RESET_EVENT:
      LOG(LS_INFO) << "SCTP_STREAM_RESET_EVENT";
      // TODO(ldixon): Notify up to channel that stream resent has happened,
      // and write unit test for this case.
      break;
    case SCTP_ASSOC_RESET_EVENT:
      LOG(LS_INFO) << "SCTP_ASSOC_RESET_EVENT";
      break;
    case SCTP_STREAM_CHANGE_EVENT:
      LOG(LS_INFO) << "SCTP_STREAM_CHANGE_EVENT";
      break;
    default:
      LOG(LS_WARNING) << "Unknown SCTP event: "
                      << notification.sn_header.sn_type;
      break;
  }
}

void SctpDataMediaChannel::OnNotificationAssocChange(
    const sctp_assoc_change& change) {
  switch (change.sac_state) {
    case SCTP_COMM_UP:
      LOG(LS_VERBOSE) << "Association change SCTP_COMM_UP";
      break;
    case SCTP_COMM_LOST:
      LOG(LS_INFO) << "Association change SCTP_COMM_LOST";
      break;
    case SCTP_RESTART:
      LOG(LS_INFO) << "Association change SCTP_RESTART";
      break;
    case SCTP_SHUTDOWN_COMP:
      LOG(LS_INFO) << "Association change SCTP_SHUTDOWN_COMP";
      break;
    case SCTP_CANT_STR_ASSOC:
      LOG(LS_INFO) << "Association change SCTP_CANT_STR_ASSOC";
      break;
    default:
      LOG(LS_INFO) << "Association change UNKNOWN";
      break;
  }
}

// Puts the specified |param| from the codec identified by |id| into |dest|
// and returns true.  Or returns false if it wasn't there, leaving |dest|
// untouched.
static bool GetCodecIntParameter(const std::vector<DataCodec>& codecs,
                                 int id, const std::string& name,
                                 const std::string& param, int* dest) {
  std::string value;
  Codec match_pattern;
  match_pattern.id = id;
  match_pattern.name = name;
  for (size_t i = 0; i < codecs.size(); ++i) {
    if (codecs[i].Matches(match_pattern)) {
      if (codecs[i].GetParam(param, &value)) {
        *dest = talk_base::FromString<int>(value);
        return true;
      }
    }
  }
  return false;
}

bool SctpDataMediaChannel::SetSendCodecs(const std::vector<DataCodec>& codecs) {
  return GetCodecIntParameter(
      codecs, kGoogleSctpDataCodecId, kGoogleSctpDataCodecName, kCodecParamPort,
      &remote_port_);
}

bool SctpDataMediaChannel::SetRecvCodecs(const std::vector<DataCodec>& codecs) {
  return GetCodecIntParameter(
      codecs, kGoogleSctpDataCodecId, kGoogleSctpDataCodecName, kCodecParamPort,
      &local_port_);
}

void SctpDataMediaChannel::OnPacketFromSctpToNetwork(
    talk_base::Buffer* buffer) {
  if (buffer->length() > kSctpMtu) {
    LOG(LS_ERROR) << debug_name_ << "->OnPacketFromSctpToNetwork(...): "
                  << "SCTP seems to have made a poacket that is bigger "
                     "than its official MTU.";
  }
  MediaChannel::SendPacket(buffer);
}

void SctpDataMediaChannel::OnMessage(talk_base::Message* msg) {
  switch (msg->message_id) {
    case MSG_SCTPINBOUNDPACKET: {
      SctpInboundPacket* packet =
          static_cast<talk_base::TypedMessageData<SctpInboundPacket*>*>(
              msg->pdata)->data();
      OnInboundPacketFromSctpToChannel(packet);
      delete packet;
      break;
    }
    case MSG_SCTPOUTBOUNDPACKET: {
      talk_base::Buffer* buffer =
          static_cast<talk_base::TypedMessageData<talk_base::Buffer*>*>(
              msg->pdata)->data();
      OnPacketFromSctpToNetwork(buffer);
      delete buffer;
      break;
    }
  }
}

}  // namespace cricket
