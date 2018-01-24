/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef PC_PEERCONNECTIONINTERNAL_H_
#define PC_PEERCONNECTIONINTERNAL_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "api/peerconnectioninterface.h"
#include "pc/datachannel.h"
#include "pc/rtptransceiver.h"

namespace webrtc {

// Statistics for all the transports of the session.
// TODO(pthatcher): Think of a better name for this.  We already have
// a TransportStats in transport.h.  Perhaps TransportsStats?
struct SessionStats {
  std::map<std::string, cricket::TransportStats> transport_stats;
};

struct ChannelNamePair {
  ChannelNamePair(const std::string& content_name,
                  const std::string& transport_name)
      : content_name(content_name), transport_name(transport_name) {}
  std::string content_name;
  std::string transport_name;
};

struct ChannelNamePairs {
  rtc::Optional<ChannelNamePair> voice;
  rtc::Optional<ChannelNamePair> video;
  rtc::Optional<ChannelNamePair> data;
};

// Internal interface for extra PeerConnection methods.
class PeerConnectionInternal : public PeerConnectionInterface {
 public:
  virtual rtc::Thread* network_thread() const = 0;
  virtual rtc::Thread* worker_thread() const = 0;
  virtual rtc::Thread* signaling_thread() const = 0;

  // The SDP session ID as defined by RFC 3264.
  virtual const std::string& session_id() const = 0;

  // Returns true if we were the initial offerer.
  virtual bool initial_offerer() const = 0;

  // TODO(steveanton): Remove these.
  virtual cricket::VoiceChannel* voice_channel() const = 0;
  virtual cricket::VideoChannel* video_channel() const = 0;

  // Exposed for tests.
  virtual std::vector<
      rtc::scoped_refptr<RtpTransceiverProxyWithInternal<RtpTransceiver>>>
  GetTransceiversForTesting() const = 0;

  // Get the id used as a media stream track's "id" field from ssrc.
  virtual bool GetLocalTrackIdBySsrc(uint32_t ssrc, std::string* track_id) = 0;
  virtual bool GetRemoteTrackIdBySsrc(uint32_t ssrc, std::string* track_id) = 0;

  virtual sigslot::signal1<DataChannel*>& SignalDataChannelCreated() = 0;

  // Only valid when using deprecated RTP data channels.
  virtual cricket::RtpDataChannel* rtp_data_channel() const = 0;

  virtual const std::vector<rtc::scoped_refptr<DataChannel>>&
  sctp_data_channels() const = 0;

  virtual rtc::Optional<std::string> sctp_content_name() const = 0;
  virtual rtc::Optional<std::string> sctp_transport_name() const = 0;

  // Returns stats for all channels of all transports.
  // This avoids exposing the internal structures used to track them.
  // The parameterless version creates |ChannelNamePairs| from |voice_channel|,
  // |video_channel| and |voice_channel| if available - this requires it to be
  // called on the signaling thread - and invokes the other |GetStats|. The
  // other |GetStats| can be invoked on any thread; if not invoked on the
  // network thread a thread hop will happen.
  virtual std::unique_ptr<SessionStats> GetSessionStats_s() = 0;
  virtual std::unique_ptr<SessionStats> GetSessionStats(
      const ChannelNamePairs& channel_name_pairs) = 0;

  virtual Call::Stats GetCallStats() = 0;

  virtual bool GetLocalCertificate(
      const std::string& transport_name,
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) = 0;
  virtual std::unique_ptr<rtc::SSLCertificate> GetRemoteSSLCertificate(
      const std::string& transport_name) = 0;

  // Returns true if there was an ICE restart initiated by the remote offer.
  virtual bool IceRestartPending(const std::string& content_name) const = 0;

  // Returns true if the ICE restart flag above was set, and no ICE restart has
  // occurred yet for this transport (by applying a local description with
  // changed ufrag/password). If the transport has been deleted as a result of
  // bundling, returns false.
  virtual bool NeedsIceRestart(const std::string& content_name) const = 0;

  // Get SSL role for an arbitrary m= section (handles bundling correctly).
  virtual bool GetSslRole(const std::string& content_name,
                          rtc::SSLRole* role) = 0;
};

}  // namespace webrtc

#endif  // PC_PEERCONNECTIONINTERNAL_H_
