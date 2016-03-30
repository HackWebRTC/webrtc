/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_NETWORKROUTE_H_
#define WEBRTC_BASE_NETWORKROUTE_H_

// TODO(honghaiz): Make a directory that describes the interfaces and structs
// the media code can rely on and the network code can implement, and both can
// depend on that, but not depend on each other. Then, move this file to that
// directory.
namespace cricket {

struct NetworkRoute {
  bool connected;
  uint16_t local_network_id;
  uint16_t remote_network_id;

  NetworkRoute()
      : connected(false), local_network_id(0), remote_network_id(0) {}

  // The route is connected if the local and remote network ids are provided.
  NetworkRoute(uint16_t local_net_id, uint16_t remote_net_id)
      : connected(true),
        local_network_id(local_net_id),
        remote_network_id(remote_net_id) {}

  bool operator==(const NetworkRoute& nr) const {
    return connected == nr.connected &&
           local_network_id == nr.local_network_id &&
           remote_network_id == nr.remote_network_id;
  }

  bool operator!=(const NetworkRoute& nr) const { return !(*this == nr); }
};
}  // namespace cricket

#endif  // WEBRTC_BASE_NETWORKROUTE_H_
