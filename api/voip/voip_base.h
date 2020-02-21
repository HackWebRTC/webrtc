//
//  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
//
//  Use of this source code is governed by a BSD-style license
//  that can be found in the LICENSE file in the root of the source
//  tree. An additional intellectual property rights grant can be found
//  in the file PATENTS.  All contributing project authors may
//  be found in the AUTHORS file in the root of the source tree.
//

#ifndef API_VOIP_VOIP_BASE_H_
#define API_VOIP_VOIP_BASE_H_

#include "api/call/transport.h"

namespace webrtc {

// VoipBase interface
//
// VoipBase provides a management interface on a media session using a
// concept called 'channel'.  A channel represents an interface handle
// for application to request various media session operations.  This
// notion of channel is used throughout other interfaces as well.
//
// Underneath the interface, a channel handle is mapped into an audio session
// object that is capable of sending and receiving a single RTP stream with
// another media endpoint.  It's possible to create and use multiple active
// channels simultaneously which would mean that particular application
// session has RTP streams with multiple remote endpoints.
//
// A typical example for the usage context is outlined in VoipEngine
// header file.
class VoipBase {
 public:
  // This config enables application to set webrtc::Transport callback pointer
  // to receive rtp/rtcp packets from corresponding media session in VoIP
  // engine. VoipEngine framework expects applications to handle network I/O
  // directly and injection for incoming RTP from remote endpoint is handled
  // via VoipNetwork interface.
  struct Config {
    Transport* transport = nullptr;
    uint32_t local_ssrc = 0;
  };

  // Create a channel handle.
  // Valid handle value is zero or greater integer whereas -1 represents error
  // during media session construction. Each channel handle maps into one
  // audio media session where each has its own separate module for
  // send/receive rtp packet with one peer.
  virtual int CreateChannel(const Config& config) = 0;

  // Following methods return boolean to indicate if the operation is succeeded.
  // API is subject to expand to reflect error condition to application later.

  // Release |channel| that has served the purpose.
  // Released channel handle will be re-allocated again. Invoking
  // an operation on released channel will lead to undefined behavior.
  virtual bool ReleaseChannel(int channel) = 0;

  // Start sending on |channel|. This will start microphone if first to start.
  virtual bool StartSend(int channel) = 0;

  // Stop sending on |channel|. If this is the last active channel, it will
  // stop microphone input from underlying audio platform layer.
  virtual bool StopSend(int channel) = 0;

  // Start playing on speaker device for |channel|.
  // This will start underlying platform speaker device if not started.
  virtual bool StartPlayout(int channel) = 0;

  // Stop playing on speaker device for |channel|. If this is the last
  // active channel playing, then it will stop speaker from the platform layer.
  virtual bool StopPlayout(int channel) = 0;

 protected:
  virtual ~VoipBase() = default;
};

}  // namespace webrtc

#endif  // API_VOIP_VOIP_BASE_H_
