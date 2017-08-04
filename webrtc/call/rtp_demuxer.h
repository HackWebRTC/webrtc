/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_CALL_RTP_DEMUXER_H_
#define WEBRTC_CALL_RTP_DEMUXER_H_

#include <map>
#include <string>
#include <vector>

namespace webrtc {

class RsidResolutionObserver;
class RtpPacketReceived;
class RtpPacketSinkInterface;

// This class represents the RTP demuxing, for a single RTP session (i.e., one
// ssrc space, see RFC 7656). It isn't thread aware, leaving responsibility of
// multithreading issues to the user of this class.
// TODO(nisse): Should be extended to also do MID-based demux and payload-type
// demux.
class RtpDemuxer {
 public:
  RtpDemuxer();
  ~RtpDemuxer();

  // Registers a sink. Multiple SSRCs may be mapped to the same sink, but
  // each SSRC may only be mapped to one sink. The return value reports
  // whether the association has been recorded or rejected. Rejection may occur
  // if the SSRC has already been associated with a sink. The previously added
  // sink is *not* forgotten.
  bool AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink);

  // Registers a sink's association to an RSID. Only one sink may be associated
  // with a given RSID. Null pointer is not allowed.
  void AddSink(const std::string& rsid, RtpPacketSinkInterface* sink);

  // Removes a sink. Return value reports if anything was actually removed.
  // Null pointer is not allowed.
  bool RemoveSink(const RtpPacketSinkInterface* sink);

  // Handles RTP packets. Returns true if at least one matching sink was found.
  bool OnRtpPacket(const RtpPacketReceived& packet);

  // Allows other objects to be notified when RSID-SSRC associations are
  // resolved by this object.
  void RegisterRsidResolutionObserver(RsidResolutionObserver* observer);

  // Undo a previous RegisterRsidResolutionObserver().
  void DeregisterRsidResolutionObserver(const RsidResolutionObserver* observer);

 private:
  // Find the associations of RSID to SSRCs.
  void ResolveRsidToSsrcAssociations(const RtpPacketReceived& packet);

  // Notify observers of the resolution of an RSID to an SSRC.
  void NotifyObserversOfRsidResolution(const std::string& rsid, uint32_t ssrc);

  // This records the association SSRCs to sinks. Other associations, such
  // as by RSID, also end up here once the RSID, etc., is resolved to an SSRC.
  std::map<uint32_t, RtpPacketSinkInterface*> ssrc_sinks_;

  // A sink may be associated with an RSID - RTP Stream ID. This tag has a
  // one-to-one association with an SSRC, but that SSRC is not yet known.
  // When it becomes known, the association of the sink to the RSID is deleted
  // from this container, and moved into |ssrc_sinks_|.
  std::map<std::string, RtpPacketSinkInterface*> rsid_sinks_;

  // Observers which will be notified when an RSID association to an SSRC is
  // resolved by this object.
  std::vector<RsidResolutionObserver*> rsid_resolution_observers_;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_RTP_DEMUXER_H_
