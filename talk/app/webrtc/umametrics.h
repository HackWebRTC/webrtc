/*
 * libjingle
 * Copyright 2014 Google Inc.
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

// This file contains enums related to IPv4/IPv6 metrics.

#ifndef TALK_APP_WEBRTC_UMAMETRICS_H_
#define TALK_APP_WEBRTC_UMAMETRICS_H_

namespace webrtc {

// Currently this contains information related to WebRTC network/transport
// information.

// The difference between PeerConnectionMetricsCounter and
// PeerConnectionMetricsName is that the "Counter" is only counting the
// occurrences of events, while "Name" has a value associated with it which is
// used to form a histogram.

// This enum is backed by Chromium's histograms.xml,
// chromium/src/tools/metrics/histograms/histograms.xml
// Existing values cannot be re-ordered and new enums must be added
// before kBoundary.
enum PeerConnectionMetricsCounter {
  kPeerConnection_IPv4,
  kPeerConnection_IPv6,
  kBestConnections_IPv4,
  kBestConnections_IPv6,
  kPeerConnectionMetricsCounter_Max,
};

// TODO(guoweis): Keep previous name here until all references are renamed.
#define kBoundary kPeerConnectionMetricsCounter_Max

// TODO(guoweis): Keep previous name here until all references are renamed.
typedef PeerConnectionMetricsCounter PeerConnectionUMAMetricsCounter;

// This enum defines types for UMA samples, which will have a range.
enum PeerConnectionMetricsName {
  kNetworkInterfaces_IPv4,  // Number of IPv4 interfaces.
  kNetworkInterfaces_IPv6,  // Number of IPv6 interfaces.
  kTimeToConnect,           // In milliseconds.
  kLocalCandidates_IPv4,    // Number of IPv4 local candidates.
  kLocalCandidates_IPv6,    // Number of IPv6 local candidates.
  kPeerConnectionMetricsName_Max
};

// TODO(guoweis): Keep previous name here until all references are renamed.
typedef PeerConnectionMetricsName PeerConnectionUMAMetricsName;

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_UMA6METRICS_H_
