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

#ifndef TALK_MEDIA_WEBRTC_SIMULCAST_H_
#define TALK_MEDIA_WEBRTC_SIMULCAST_H_

#include <vector>

#include "webrtc/base/basictypes.h"
#include "webrtc/config.h"

namespace cricket {
struct StreamParams;

// Config for use with screen cast when temporal layers are enabled.
struct ScreenshareLayerConfig {
 public:
  ScreenshareLayerConfig(int tl0_bitrate, int tl1_bitrate);

  // Bitrates, for temporal layers 0 and 1.
  int tl0_bitrate_kbps;
  int tl1_bitrate_kbps;

  static ScreenshareLayerConfig GetDefault();

  // Parse bitrate from group name on format "(tl0_bitrate)-(tl1_bitrate)",
  // eg. "100-1000" for the default rates.
  static bool FromFieldTrialGroup(const std::string& group,
                                  ScreenshareLayerConfig* config);
};

// TODO(pthatcher): Write unit tests just for these functions,
// independent of WebrtcVideoEngine.

int GetTotalMaxBitrateBps(const std::vector<webrtc::VideoStream>& streams);

// Get the ssrcs of the SIM group from the stream params.
void GetSimulcastSsrcs(const StreamParams& sp, std::vector<uint32_t>* ssrcs);

// Get simulcast settings.
std::vector<webrtc::VideoStream> GetSimulcastConfig(size_t max_streams,
                                                    int width,
                                                    int height,
                                                    int max_bitrate_bps,
                                                    int max_qp,
                                                    int max_framerate);

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTC_SIMULCAST_H_
