/* Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef API_MEDIA_TRANSPORT_CONFIG_H_
#define API_MEDIA_TRANSPORT_CONFIG_H_

#include <memory>
#include <string>
#include <utility>

namespace webrtc {

class MediaTransportInterface;

// MediaTransportConfig contains meida transport (if provided) and passed from
// PeerConnection to call obeject and media layers that require access to media
// transport. In the future we can add other transport (for example, datagram
// transport) and related configuration.
struct MediaTransportConfig {
  // Default constructor for no-media transport scenarios.
  MediaTransportConfig() = default;

  // TODO(sukhanov): Consider adding RtpTransport* to MediaTransportConfig,
  // because it's almost always passes along with media_transport.
  // Does not own media_transport.
  explicit MediaTransportConfig(MediaTransportInterface* media_transport)
      : media_transport(media_transport) {}

  std::string DebugString() const;

  // If provided, all media is sent through media_transport.
  MediaTransportInterface* media_transport = nullptr;
};

}  // namespace webrtc

#endif  // API_MEDIA_TRANSPORT_CONFIG_H_
