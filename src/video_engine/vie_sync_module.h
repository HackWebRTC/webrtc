/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// ViESyncModule is responsible for synchronization audio and video for a given
// VoE and ViE channel couple.

#ifndef WEBRTC_VIDEO_ENGINE_VIE_SYNC_MODULE_H_
#define WEBRTC_VIDEO_ENGINE_VIE_SYNC_MODULE_H_

#include "module.h"
#include "system_wrappers/interface/scoped_ptr.h"
#include "tick_util.h"

namespace webrtc {

class CriticalSectionWrapper;
class RtpRtcp;
class VideoCodingModule;
class VoEVideoSync;

class ViESyncModule : public Module {
 public:
  ViESyncModule(const int32_t channel_id, VideoCodingModule* vcm);
  ~ViESyncModule();

  int ConfigureSync(int voe_channel_id,
                    VoEVideoSync* voe_sync_interface,
                    RtpRtcp* video_rtcp_module);

  int VoiceChannel();

  // Implements Module.
  virtual WebRtc_Word32 TimeUntilNextProcess();
  virtual WebRtc_Word32 Process();

 private:
  scoped_ptr<CriticalSectionWrapper> data_cs_;
  const int32_t channel_id_;
  VideoCodingModule* vcm_;
  RtpRtcp* video_rtcp_module_;
  int voe_channel_id_;
  VoEVideoSync* voe_sync_interface_;
  TickTime last_sync_time_;

  struct ViESyncDelay {
    ViESyncDelay() {
      extra_video_delay_ms = 0;
      last_video_delay_ms = 0;
      extra_audio_delay_ms = 0;
      last_sync_delay = 0;
      network_delay = 120;
    }
    int extra_video_delay_ms;
    int last_video_delay_ms;
    int extra_audio_delay_ms;
    int last_sync_delay;
    int network_delay;
  };
  ViESyncDelay channel_delay_;
};

}  // namespace webrtc

#endif  // WEBRTC_VIDEO_ENGINE_VIE_SYNC_MODULE_H_
