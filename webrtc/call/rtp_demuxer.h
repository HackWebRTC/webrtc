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

namespace webrtc {

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

  // Registers a sink. The same sink can be registered for multiple ssrcs, and
  // the same ssrc can have multiple sinks. Null pointer is not allowed.
  void AddSink(uint32_t ssrc, RtpPacketSinkInterface* sink);
  // Removes a sink. Returns deletion count (a sink may be registered
  // for multiple ssrcs). Null pointer is not allowed.
  size_t RemoveSink(const RtpPacketSinkInterface* sink);

  // Returns true if at least one matching sink was found, otherwise false.
  bool OnRtpPacket(const RtpPacketReceived& packet);

 private:
  std::multimap<uint32_t, RtpPacketSinkInterface*> sinks_;
};

}  // namespace webrtc

#endif  // WEBRTC_CALL_RTP_DEMUXER_H_
