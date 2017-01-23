/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_ORTCFACTORY_H_
#define WEBRTC_PC_ORTCFACTORY_H_

#include <memory>

#include "webrtc/api/ortcfactoryinterface.h"
#include "webrtc/base/constructormagic.h"

namespace webrtc {

// Implementation of OrtcFactoryInterface.
//
// See ortcfactoryinterface.h for documentation.
class OrtcFactory : public OrtcFactoryInterface {
 public:
  OrtcFactory(rtc::Thread* network_thread,
              rtc::Thread* signaling_thread,
              rtc::NetworkManager* network_manager,
              rtc::PacketSocketFactory* socket_factory);
  ~OrtcFactory() override;
  std::unique_ptr<UdpTransportInterface>
  CreateUdpTransport(int family, uint16_t min_port, uint16_t max_port) override;

  rtc::Thread* network_thread() { return network_thread_; }
  rtc::Thread* worker_thread() { return owned_worker_thread_.get(); }
  rtc::Thread* signaling_thread() { return signaling_thread_; }

 private:
  rtc::Thread* network_thread_;
  rtc::Thread* signaling_thread_;
  rtc::NetworkManager* network_manager_;
  rtc::PacketSocketFactory* socket_factory_;
  // If we created/own the objects above, these will be non-null and thus will
  // be released automatically upon destruction.
  std::unique_ptr<rtc::Thread> owned_network_thread_;
  std::unique_ptr<rtc::Thread> owned_worker_thread_;
  bool wraps_signaling_thread_ = false;
  std::unique_ptr<rtc::NetworkManager> owned_network_manager_;
  std::unique_ptr<rtc::PacketSocketFactory> owned_socket_factory_;
  RTC_DISALLOW_COPY_AND_ASSIGN(OrtcFactory);
};

BEGIN_OWNED_PROXY_MAP(OrtcFactory)
  PROXY_SIGNALING_THREAD_DESTRUCTOR()
  PROXY_METHOD3(std::unique_ptr<UdpTransportInterface>,
                CreateUdpTransport,
                int,
                uint16_t,
                uint16_t)
END_PROXY_MAP()

}  // namespace webrtc

#endif  // WEBRTC_PC_ORTCFACTORY_H_
