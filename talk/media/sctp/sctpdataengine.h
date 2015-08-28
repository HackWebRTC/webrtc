/*
 * libjingle
 * Copyright 2012 Google Inc. and Robin Seggelmann
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

#ifndef TALK_MEDIA_SCTP_SCTPDATAENGINE_H_
#define TALK_MEDIA_SCTP_SCTPDATAENGINE_H_

#include <errno.h>
#include <string>
#include <vector>

namespace cricket {
// Some ERRNO values get re-#defined to WSA* equivalents in some talk/
// headers.  We save the original ones in an enum.
enum PreservedErrno {
  SCTP_EINPROGRESS = EINPROGRESS,
  SCTP_EWOULDBLOCK = EWOULDBLOCK
};
}  // namespace cricket

#include "talk/media/base/codec.h"
#include "talk/media/base/mediachannel.h"
#include "talk/media/base/mediaengine.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/scoped_ptr.h"

// Defined by "usrsctplib/usrsctp.h"
struct sockaddr_conn;
struct sctp_assoc_change;
struct sctp_stream_reset_event;
// Defined by <sys/socket.h>
struct socket;
namespace cricket {
// The highest stream ID (Sid) that SCTP allows, and the number of streams we
// tell SCTP we're going to use.
const uint32 kMaxSctpSid = 1023;

// This is the default SCTP port to use. It is passed along the wire and the
// connectee and connector must be using the same port. It is not related to the
// ports at the IP level. (Corresponds to: sockaddr_conn.sconn_port in
// usrsctp.h)
const int kSctpDefaultPort = 5000;

class SctpDataMediaChannel;

// A DataEngine that interacts with usrsctp.
//
// From channel calls, data flows like this:
// [worker thread (although it can in princple be another thread)]
//  1.  SctpDataMediaChannel::SendData(data)
//  2.  usrsctp_sendv(data)
// [worker thread returns; sctp thread then calls the following]
//  3.  OnSctpOutboundPacket(wrapped_data)
// [sctp thread returns having posted a message for the worker thread]
//  4.  SctpDataMediaChannel::OnMessage(wrapped_data)
//  5.  SctpDataMediaChannel::OnPacketFromSctpToNetwork(wrapped_data)
//  6.  NetworkInterface::SendPacket(wrapped_data)
//  7.  ... across network ... a packet is sent back ...
//  8.  SctpDataMediaChannel::OnPacketReceived(wrapped_data)
//  9.  usrsctp_conninput(wrapped_data)
// [worker thread returns; sctp thread then calls the following]
//  10.  OnSctpInboundData(data)
// [sctp thread returns having posted a message fot the worker thread]
//  11. SctpDataMediaChannel::OnMessage(inboundpacket)
//  12. SctpDataMediaChannel::OnInboundPacketFromSctpToChannel(inboundpacket)
//  13. SctpDataMediaChannel::OnDataFromSctpToChannel(data)
//  14. SctpDataMediaChannel::SignalDataReceived(data)
// [from the same thread, methods registered/connected to
//  SctpDataMediaChannel are called with the recieved data]
class SctpDataEngine : public DataEngineInterface, public sigslot::has_slots<> {
 public:
  SctpDataEngine();
  virtual ~SctpDataEngine();

  virtual DataMediaChannel* CreateChannel(DataChannelType data_channel_type);

  virtual const std::vector<DataCodec>& data_codecs() { return codecs_; }

  static int SendThresholdCallback(struct socket* sock, uint32_t sb_free);

 private:
  static int usrsctp_engines_count;
  std::vector<DataCodec> codecs_;

  static SctpDataMediaChannel* GetChannelFromSocket(struct socket* sock);
};

// TODO(ldixon): Make into a special type of TypedMessageData.
// Holds data to be passed on to a channel.
struct SctpInboundPacket;

class SctpDataMediaChannel : public DataMediaChannel,
                             public rtc::MessageHandler {
 public:
  // DataMessageType is used for the SCTP "Payload Protocol Identifier", as
  // defined in http://tools.ietf.org/html/rfc4960#section-14.4
  //
  // For the list of IANA approved values see:
  // http://www.iana.org/assignments/sctp-parameters/sctp-parameters.xml
  // The value is not used by SCTP itself. It indicates the protocol running
  // on top of SCTP.
  enum PayloadProtocolIdentifier {
    PPID_NONE = 0,  // No protocol is specified.
    // Matches the PPIDs in mozilla source and
    // https://datatracker.ietf.org/doc/draft-ietf-rtcweb-data-protocol Sec. 9
    // They're not yet assigned by IANA.
    PPID_CONTROL = 50,
    PPID_BINARY_PARTIAL = 52,
    PPID_BINARY_LAST = 53,
    PPID_TEXT_PARTIAL = 54,
    PPID_TEXT_LAST = 51
  };

  typedef std::set<uint32> StreamSet;

  // Given a thread which will be used to post messages (received data) to this
  // SctpDataMediaChannel instance.
  explicit SctpDataMediaChannel(rtc::Thread* thread);
  virtual ~SctpDataMediaChannel();

  // When SetSend is set to true, connects. When set to false, disconnects.
  // Calling: "SetSend(true); SetSend(false); SetSend(true);" will connect,
  // disconnect, and reconnect.
  virtual bool SetSend(bool send);
  // Unless SetReceive(true) is called, received packets will be discarded.
  virtual bool SetReceive(bool receive);

  virtual bool AddSendStream(const StreamParams& sp);
  virtual bool RemoveSendStream(uint32 ssrc);
  virtual bool AddRecvStream(const StreamParams& sp);
  virtual bool RemoveRecvStream(uint32 ssrc);

  // Called when Sctp gets data. The data may be a notification or data for
  // OnSctpInboundData. Called from the worker thread.
  virtual void OnMessage(rtc::Message* msg);
  // Send data down this channel (will be wrapped as SCTP packets then given to
  // sctp that will then post the network interface by OnMessage).
  // Returns true iff successful data somewhere on the send-queue/network.
  virtual bool SendData(const SendDataParams& params,
                        const rtc::Buffer& payload,
                        SendDataResult* result = NULL);
  // A packet is received from the network interface. Posted to OnMessage.
  virtual void OnPacketReceived(rtc::Buffer* packet,
                                const rtc::PacketTime& packet_time);

  // Exposed to allow Post call from c-callbacks.
  rtc::Thread* worker_thread() const { return worker_thread_; }

  // TODO(ldixon): add a DataOptions class to mediachannel.h
  virtual bool SetOptions(int options) { return false; }
  virtual int GetOptions() const { return 0; }

  // Many of these things are unused by SCTP, but are needed to fulfill
  // the MediaChannel interface.
  // TODO(pthatcher): Cleanup MediaChannel interface, or at least
  // don't try calling these and return false.  Right now, things
  // don't work if we return false.
  virtual bool SetMaxSendBandwidth(int bps) { return true; }
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) { return true; }
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) { return true; }
  virtual bool SetSendCodecs(const std::vector<DataCodec>& codecs);
  virtual bool SetRecvCodecs(const std::vector<DataCodec>& codecs);
  virtual void OnRtcpReceived(rtc::Buffer* packet,
                              const rtc::PacketTime& packet_time) {}
  virtual void OnReadyToSend(bool ready) {}

  void OnSendThresholdCallback();
  // Helper for debugging.
  void set_debug_name(const std::string& debug_name) {
    debug_name_ = debug_name;
  }
  const std::string& debug_name() const { return debug_name_; }
  const struct socket* socket() const { return sock_; }
 private:
  sockaddr_conn GetSctpSockAddr(int port);

  // Creates the socket and connects. Sets sending_ to true.
  bool Connect();
  // Closes the socket. Sets sending_ to false.
  void Disconnect();

  // Returns false when openning the socket failed; when successfull sets
  // sending_ to true
  bool OpenSctpSocket();
  // Sets sending_ to false and sock_ to NULL.
  void CloseSctpSocket();

  // Sends a SCTP_RESET_STREAM for all streams in closing_ssids_.
  bool SendQueuedStreamResets();

  // Adds a stream.
  bool AddStream(const StreamParams &sp);
  // Queues a stream for reset.
  bool ResetStream(uint32 ssrc);

  // Called by OnMessage to send packet on the network.
  void OnPacketFromSctpToNetwork(rtc::Buffer* buffer);
  // Called by OnMessage to decide what to do with the packet.
  void OnInboundPacketFromSctpToChannel(SctpInboundPacket* packet);
  void OnDataFromSctpToChannel(const ReceiveDataParams& params,
                               rtc::Buffer* buffer);
  void OnNotificationFromSctp(rtc::Buffer* buffer);
  void OnNotificationAssocChange(const sctp_assoc_change& change);

  void OnStreamResetEvent(const struct sctp_stream_reset_event* evt);

  // Responsible for marshalling incoming data to the channels listeners, and
  // outgoing data to the network interface.
  rtc::Thread* worker_thread_;
  // The local and remote SCTP port to use. These are passed along the wire
  // and the listener and connector must be using the same port. It is not
  // related to the ports at the IP level.  If set to -1, we default to
  // kSctpDefaultPort.
  int local_port_;
  int remote_port_;
  struct socket* sock_;  // The socket created by usrsctp_socket(...).

  // sending_ is true iff there is a connected socket.
  bool sending_;
  // receiving_ controls whether inbound packets are thrown away.
  bool receiving_;

  // When a data channel opens a stream, it goes into open_streams_.  When we
  // want to close it, the stream's ID goes into queued_reset_streams_.  When
  // we actually transmit a RE-CONFIG chunk with that stream ID, the ID goes
  // into sent_reset_streams_.  When we get a response RE-CONFIG chunk back
  // acknowledging the reset, we remove the stream ID from
  // sent_reset_streams_.  We use sent_reset_streams_ to differentiate
  // between acknowledgment RE-CONFIG and peer-initiated RE-CONFIGs.
  StreamSet open_streams_;
  StreamSet queued_reset_streams_;
  StreamSet sent_reset_streams_;

  // A human-readable name for debugging messages.
  std::string debug_name_;
};

}  // namespace cricket

#endif  // TALK_MEDIA_SCTP_SCTPDATAENGINE_H_
