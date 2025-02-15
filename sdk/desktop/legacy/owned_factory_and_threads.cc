/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "sdk/desktop/legacy/owned_factory_and_threads.h"

namespace webrtc {
namespace legacy {

OwnedFactoryAndThreads::OwnedFactoryAndThreads(
    std::unique_ptr<Thread> network_thread,
    std::unique_ptr<Thread> worker_thread,
    std::unique_ptr<Thread> signaling_thread,
    rtc::NetworkMonitorFactory* network_monitor_factory,
    rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> factory)
    : network_thread_(std::move(network_thread)),
      worker_thread_(std::move(worker_thread)),
      signaling_thread_(std::move(signaling_thread)),
      factory_(factory) {}

OwnedFactoryAndThreads::~OwnedFactoryAndThreads() {
  factory_->Release();
}

}  // namespace legacy
}  // namespace webrtc
