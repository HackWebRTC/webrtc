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

namespace webrtc {
struct VideoCodec;
}

namespace cricket {
struct VideoOptions;
struct StreamParams;

enum SimulcastBitrateMode {
  SBM_NORMAL = 0,
  SBM_HIGH,
  SBM_VERY_HIGH,
  SBM_COUNT
};

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

// Get the simulcast bitrate mode to use based on
// options.video_highest_bitrate.
SimulcastBitrateMode GetSimulcastBitrateMode(
    const VideoOptions& options);

// Get the ssrcs of the SIM group from the stream params.
void GetSimulcastSsrcs(const StreamParams& sp, std::vector<uint32>* ssrcs);

// Get simulcast settings.
std::vector<webrtc::VideoStream> GetSimulcastConfig(
    size_t max_streams,
    SimulcastBitrateMode bitrate_mode,
    int width,
    int height,
    int max_bitrate_bps,
    int max_qp,
    int max_framerate);

// Set the codec->simulcastStreams, codec->width, and codec->height
// based on the number of ssrcs to use and the bitrate mode to use.
bool ConfigureSimulcastCodec(int number_ssrcs,
                             SimulcastBitrateMode bitrate_mode,
                             webrtc::VideoCodec* codec);

// Set the codec->simulcastStreams, codec->width, and codec->height
// based on the video options (to get the simulcast bitrate mode) and
// the stream params (to get the number of ssrcs).  This is really a
// convenience function.
bool ConfigureSimulcastCodec(const StreamParams& sp,
                             const VideoOptions& options,
                             webrtc::VideoCodec* codec);

// Set the numberOfTemporalLayers in each codec->simulcastStreams[i].
// Apparently it is useful to do this at a different time than
// ConfigureSimulcastCodec.
// TODO(pthatcher): Figure out why and put this code into
// ConfigureSimulcastCodec.
void ConfigureSimulcastTemporalLayers(
    int num_temporal_layers, webrtc::VideoCodec* codec);

// Turn off all simulcasting for the given codec.
void DisableSimulcastCodec(webrtc::VideoCodec* codec);

// Log useful info about each of the simulcast substreams of the
// codec.
void LogSimulcastSubstreams(const webrtc::VideoCodec& codec);

// Configure the codec's bitrate and temporal layers so that it's good
// for a screencast in conference mode. Technically, this shouldn't
// go in simulcast.cc. But it's closely related.
void ConfigureConferenceModeScreencastCodec(webrtc::VideoCodec* codec);

}  // namespace cricket

#endif  // TALK_MEDIA_WEBRTC_SIMULCAST_H_
