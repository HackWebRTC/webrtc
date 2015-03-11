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

#include <stdio.h>

#include "talk/media/base/mediachannel.h"  // For VideoOptions
#include "talk/media/base/streamparams.h"
#include "talk/media/webrtc/simulcast.h"
#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/common_types.h"  // For webrtc::VideoCodec
#include "webrtc/system_wrappers/interface/field_trial.h"
namespace cricket {

struct SimulcastFormat {
  int width;
  int height;
  // The maximum number of simulcast layers can be used for
  // resolutions at |widthxheigh|.
  size_t max_layers;
  // The maximum bitrate for encoding stream at |widthxheight|, when we are
  // not sending the next higher spatial stream.
  int max_bitrate_kbps[SBM_COUNT];
  // The target bitrate for encoding stream at |widthxheight|, when this layer
  // is not the highest layer (i.e., when we are sending another higher spatial
  // stream).
  int target_bitrate_kbps[SBM_COUNT];
  // The minimum bitrate needed for encoding stream at |widthxheight|.
  int min_bitrate_kbps[SBM_COUNT];
};

// These tables describe from which resolution we can use how many
// simulcast layers at what bitrates (maximum, target, and minimum).
// Important!! Keep this table from high resolution to low resolution.
const SimulcastFormat kSimulcastFormats[] = {
  {1280, 720, 3, {1200, 1200, 2500}, {1200, 1200, 2500}, {500, 600, 600}},
  {960, 540, 3, {900, 900, 900}, {900, 900, 900}, {350, 450, 450}},
  {640, 360, 2, {500, 700, 700}, {500, 500, 500}, {100, 150, 150}},
  {480, 270, 2, {350, 450, 450}, {350, 350, 350}, {100, 150, 150}},
  {320, 180, 1, {100, 200, 200}, {100, 150, 150}, {30, 30, 30}},
  {0, 0, 1, {100, 200, 200}, {100, 150, 150}, {30, 30, 30}}
};

// Multiway: Number of temporal layers for each simulcast stream, for maximum
// possible number of simulcast streams |kMaxSimulcastStreams|. The array
// goes from lowest resolution at position 0 to highest resolution.
// For example, first three elements correspond to say: QVGA, VGA, WHD.
static const int
    kDefaultConferenceNumberOfTemporalLayers[webrtc::kMaxSimulcastStreams] =
    {3, 3, 3, 3};

void GetSimulcastSsrcs(const StreamParams& sp, std::vector<uint32>* ssrcs) {
  const SsrcGroup* sim_group = sp.get_ssrc_group(kSimSsrcGroupSemantics);
  if (sim_group) {
    ssrcs->insert(
        ssrcs->end(), sim_group->ssrcs.begin(), sim_group->ssrcs.end());
  }
}

SimulcastBitrateMode GetSimulcastBitrateMode(
    const VideoOptions& options) {
  VideoOptions::HighestBitrate bitrate_mode;
  if (options.video_highest_bitrate.Get(&bitrate_mode)) {
    switch (bitrate_mode) {
      case VideoOptions::HIGH:
        return SBM_HIGH;
      case VideoOptions::VERY_HIGH:
        return SBM_VERY_HIGH;
      default:
        break;
    }
  }
  return SBM_NORMAL;
}

void MaybeExchangeWidthHeight(int* width, int* height) {
  // |kSimulcastFormats| assumes |width| >= |height|. If not, exchange them
  // before comparing.
  if (*width < *height) {
    int temp = *width;
    *width = *height;
    *height = temp;
  }
}

int FindSimulcastFormatIndex(int width, int height) {
  MaybeExchangeWidthHeight(&width, &height);

  for (int i = 0; i < ARRAY_SIZE(kSimulcastFormats); ++i) {
    if (width >= kSimulcastFormats[i].width &&
        height >= kSimulcastFormats[i].height) {
      return i;
    }
  }
  return -1;
}

int FindSimulcastFormatIndex(int width, int height, size_t max_layers) {
  MaybeExchangeWidthHeight(&width, &height);

  for (int i = 0; i < ARRAY_SIZE(kSimulcastFormats); ++i) {
    if (width >= kSimulcastFormats[i].width &&
        height >= kSimulcastFormats[i].height &&
        max_layers == kSimulcastFormats[i].max_layers) {
      return i;
    }
  }
  return -1;
}

SimulcastBitrateMode FindSimulcastBitrateMode(
    size_t max_layers,
    int stream_idx,
    SimulcastBitrateMode highest_enabled) {

  if (highest_enabled > SBM_NORMAL) {
    // We want high or very high for all layers if enabled.
    return highest_enabled;
  }
  if (kSimulcastFormats[stream_idx].max_layers == max_layers) {
    // We want high for the top layer.
    return SBM_HIGH;
  }
  // And normal for everything else.
  return SBM_NORMAL;
}

// Simulcast stream width and height must both be dividable by
// |2 ^ simulcast_layers - 1|.
int NormalizeSimulcastSize(int size, size_t simulcast_layers) {
  const int base2_exponent = static_cast<int>(simulcast_layers) - 1;
  return ((size >> base2_exponent) << base2_exponent);
}

size_t FindSimulcastMaxLayers(int width, int height) {
  int index = FindSimulcastFormatIndex(width, height);
  if (index == -1) {
    return -1;
  }
  return kSimulcastFormats[index].max_layers;
}

// TODO(marpan): Investigate if we should return 0 instead of -1 in
// FindSimulcast[Max/Target/Min]Bitrate functions below, since the
// codec struct max/min/targeBitrates are unsigned.
int FindSimulcastMaxBitrateBps(int width,
                               int height,
                               size_t max_layers,
                               SimulcastBitrateMode highest_enabled) {
  const int format_index = FindSimulcastFormatIndex(width, height);
  if (format_index == -1) {
    return -1;
  }
  const SimulcastBitrateMode bitrate_mode = FindSimulcastBitrateMode(
      max_layers, format_index, highest_enabled);
  return kSimulcastFormats[format_index].max_bitrate_kbps[bitrate_mode] * 1000;
}

int FindSimulcastTargetBitrateBps(int width,
                                  int height,
                                  size_t max_layers,
                                  SimulcastBitrateMode highest_enabled) {
  const int format_index = FindSimulcastFormatIndex(width, height);
  if (format_index == -1) {
    return -1;
  }
  const SimulcastBitrateMode bitrate_mode = FindSimulcastBitrateMode(
      max_layers, format_index, highest_enabled);
  return kSimulcastFormats[format_index].target_bitrate_kbps[bitrate_mode] *
         1000;
}

int FindSimulcastMinBitrateBps(int width,
                               int height,
                               size_t max_layers,
                               SimulcastBitrateMode highest_enabled) {
  const int format_index = FindSimulcastFormatIndex(width, height);
  if (format_index == -1) {
    return -1;
  }
  const SimulcastBitrateMode bitrate_mode = FindSimulcastBitrateMode(
      max_layers, format_index, highest_enabled);
  return kSimulcastFormats[format_index].min_bitrate_kbps[bitrate_mode] * 1000;
}

bool SlotSimulcastMaxResolution(size_t max_layers, int* width, int* height) {
  int index = FindSimulcastFormatIndex(*width, *height, max_layers);
  if (index == -1) {
    LOG(LS_ERROR) << "SlotSimulcastMaxResolution";
    return false;
  }

  *width = kSimulcastFormats[index].width;
  *height = kSimulcastFormats[index].height;
  LOG(LS_INFO) << "SlotSimulcastMaxResolution to width:" << *width
               << " height:" << *height;
  return true;
}

int GetTotalMaxBitrateBps(const std::vector<webrtc::VideoStream>& streams) {
  int total_max_bitrate_bps = 0;
  for (size_t s = 0; s < streams.size() - 1; ++s) {
    total_max_bitrate_bps += streams[s].target_bitrate_bps;
  }
  total_max_bitrate_bps += streams.back().max_bitrate_bps;
  return total_max_bitrate_bps;
}

std::vector<webrtc::VideoStream> GetSimulcastConfig(
    size_t max_streams,
    SimulcastBitrateMode bitrate_mode,
    int width,
    int height,
    int max_bitrate_bps,
    int max_qp,
    int max_framerate) {
  size_t simulcast_layers = FindSimulcastMaxLayers(width, height);
  if (simulcast_layers > max_streams) {
    // If the number of SSRCs in the group differs from our target
    // number of simulcast streams for current resolution, switch down
    // to a resolution that matches our number of SSRCs.
    if (!SlotSimulcastMaxResolution(max_streams, &width, &height)) {
      return std::vector<webrtc::VideoStream>();
    }
    simulcast_layers = max_streams;
  }
  std::vector<webrtc::VideoStream> streams;
  streams.resize(simulcast_layers);

  // Format width and height has to be divisible by |2 ^ number_streams - 1|.
  width = NormalizeSimulcastSize(width, simulcast_layers);
  height = NormalizeSimulcastSize(height, simulcast_layers);

  // Add simulcast sub-streams from lower resolution to higher resolutions.
  // Add simulcast streams, from highest resolution (|s| = number_streams -1)
  // to lowest resolution at |s| = 0.
  for (size_t s = simulcast_layers - 1;; --s) {
    streams[s].width = width;
    streams[s].height = height;
    // TODO(pbos): Fill actual temporal-layer bitrate thresholds.
    streams[s].temporal_layer_thresholds_bps.resize(
        kDefaultConferenceNumberOfTemporalLayers[s] - 1);
    streams[s].max_bitrate_bps = FindSimulcastMaxBitrateBps(
        width, height, simulcast_layers, bitrate_mode);
    streams[s].target_bitrate_bps = FindSimulcastTargetBitrateBps(
        width, height, simulcast_layers, bitrate_mode);
    streams[s].min_bitrate_bps = FindSimulcastMinBitrateBps(
        width, height, simulcast_layers, bitrate_mode);
    streams[s].max_qp = max_qp;
    streams[s].max_framerate = max_framerate;
    width /= 2;
    height /= 2;
    if (s == 0) {
      break;
    }
  }

  // Spend additional bits to boost the max stream.
  int bitrate_left_bps = max_bitrate_bps - GetTotalMaxBitrateBps(streams);
  if (bitrate_left_bps > 0) {
    streams.back().max_bitrate_bps += bitrate_left_bps;
  }

  return streams;
}

bool ConfigureSimulcastCodec(
    int number_ssrcs,
    SimulcastBitrateMode bitrate_mode,
    webrtc::VideoCodec* codec) {
  std::vector<webrtc::VideoStream> streams =
      GetSimulcastConfig(static_cast<size_t>(number_ssrcs),
                         bitrate_mode,
                         static_cast<int>(codec->width),
                         static_cast<int>(codec->height),
                         codec->maxBitrate * 1000,
                         codec->qpMax,
                         codec->maxFramerate);
  // Add simulcast sub-streams from lower resolution to higher resolutions.
  codec->numberOfSimulcastStreams = static_cast<unsigned int>(streams.size());
  codec->width = static_cast<unsigned short>(streams.back().width);
  codec->height = static_cast<unsigned short>(streams.back().height);
  // When using simulcast, |codec->maxBitrate| is set to the sum of the max
  // bitrates over all streams. For a given stream |s|, the max bitrate for that
  // stream is set by |simulcastStream[s].targetBitrate|, if it is not the
  // highest resolution stream, otherwise it is set by
  // |simulcastStream[s].maxBitrate|.

  for (size_t s = 0; s < streams.size(); ++s) {
    codec->simulcastStream[s].width =
        static_cast<unsigned short>(streams[s].width);
    codec->simulcastStream[s].height =
        static_cast<unsigned short>(streams[s].height);
    codec->simulcastStream[s].numberOfTemporalLayers =
        static_cast<unsigned int>(
            streams[s].temporal_layer_thresholds_bps.size() + 1);
    codec->simulcastStream[s].minBitrate = streams[s].min_bitrate_bps / 1000;
    codec->simulcastStream[s].targetBitrate =
        streams[s].target_bitrate_bps / 1000;
    codec->simulcastStream[s].maxBitrate = streams[s].max_bitrate_bps / 1000;
    codec->simulcastStream[s].qpMax = streams[s].max_qp;
  }

  codec->maxBitrate =
      static_cast<unsigned int>(GetTotalMaxBitrateBps(streams) / 1000);

  codec->codecSpecific.VP8.numberOfTemporalLayers =
      kDefaultConferenceNumberOfTemporalLayers[0];

  return true;
}

bool ConfigureSimulcastCodec(
    const StreamParams& sp,
    const VideoOptions& options,
    webrtc::VideoCodec* codec) {
  std::vector<uint32> ssrcs;
  GetSimulcastSsrcs(sp, &ssrcs);
  SimulcastBitrateMode bitrate_mode = GetSimulcastBitrateMode(options);
  return ConfigureSimulcastCodec(static_cast<int>(ssrcs.size()), bitrate_mode,
                                 codec);
}

void ConfigureSimulcastTemporalLayers(
    int num_temporal_layers, webrtc::VideoCodec* codec) {
  for (size_t i = 0; i < codec->numberOfSimulcastStreams; ++i) {
    codec->simulcastStream[i].numberOfTemporalLayers = num_temporal_layers;
  }
}

void DisableSimulcastCodec(webrtc::VideoCodec* codec) {
  // TODO(hellner): the proper solution is to uncomment the next code line
  // and remove the lines following it in this condition. This is pending
  // b/7012070 being fixed.
  // codec->numberOfSimulcastStreams = 0;
  // It is possible to set non simulcast without the above line. However,
  // the max bitrate for every simulcast layer must be set to 0. Further,
  // there is a sanity check making sure that the aspect ratio is the same
  // for all simulcast layers. The for-loop makes sure that the sanity check
  // does not fail.
  if (codec->numberOfSimulcastStreams > 0) {
    const int ratio = codec->width / codec->height;
    for (int i = 0; i < codec->numberOfSimulcastStreams - 1; ++i) {
      // Min/target bitrate has to be zero not to influence padding
      // calculations in VideoEngine.
      codec->simulcastStream[i].minBitrate = 0;
      codec->simulcastStream[i].targetBitrate = 0;
      codec->simulcastStream[i].maxBitrate = 0;
      codec->simulcastStream[i].width =
          codec->simulcastStream[i].height * ratio;
      codec->simulcastStream[i].numberOfTemporalLayers = 1;
    }
    // The for loop above did not set the bitrate of the highest layer.
    codec->simulcastStream[codec->numberOfSimulcastStreams - 1]
        .minBitrate = 0;
    codec->simulcastStream[codec->numberOfSimulcastStreams - 1]
        .targetBitrate = 0;
    codec->simulcastStream[codec->numberOfSimulcastStreams - 1].
        maxBitrate = 0;
    // The highest layer has to correspond to the non-simulcast resolution.
    codec->simulcastStream[codec->numberOfSimulcastStreams - 1].
        width = codec->width;
    codec->simulcastStream[codec->numberOfSimulcastStreams - 1].
        height = codec->height;
    codec->simulcastStream[codec->numberOfSimulcastStreams - 1].
        numberOfTemporalLayers = 1;
    // TODO(hellner): the maxFramerate should also be set here according to
    //                the screencasts framerate. Doing so will break some
    //                unittests.
  }
}

void LogSimulcastSubstreams(const webrtc::VideoCodec& codec) {
  for (size_t i = 0; i < codec.numberOfSimulcastStreams; ++i) {
    LOG(LS_INFO) << "Simulcast substream " << i << ": "
                 << codec.simulcastStream[i].width << "x"
                 << codec.simulcastStream[i].height << "@"
                 << codec.simulcastStream[i].minBitrate << "-"
                 << codec.simulcastStream[i].maxBitrate << "kbps"
                 << " with "
                 << static_cast<int>(
                     codec.simulcastStream[i].numberOfTemporalLayers)
                 << " temporal layers";
  }
}

static const int kScreenshareMinBitrateKbps = 50;
static const int kScreenshareMaxBitrateKbps = 6000;
static const int kScreenshareDefaultTl0BitrateKbps = 100;
static const int kScreenshareDefaultTl1BitrateKbps = 1000;

static const char* kScreencastLayerFieldTrialName =
    "WebRTC-ScreenshareLayerRates";

ScreenshareLayerConfig::ScreenshareLayerConfig(int tl0_bitrate, int tl1_bitrate)
    : tl0_bitrate_kbps(tl0_bitrate), tl1_bitrate_kbps(tl1_bitrate) {
}

ScreenshareLayerConfig ScreenshareLayerConfig::GetDefault() {
  std::string group =
      webrtc::field_trial::FindFullName(kScreencastLayerFieldTrialName);

  ScreenshareLayerConfig config(kScreenshareDefaultTl0BitrateKbps,
                                kScreenshareDefaultTl1BitrateKbps);
  if (!group.empty() && !FromFieldTrialGroup(group, &config)) {
    LOG(LS_WARNING) << "Unable to parse WebRTC-ScreenshareLayerRates"
                       " field trial group: '" << group << "'.";
  }
  return config;
}

bool ScreenshareLayerConfig::FromFieldTrialGroup(
    const std::string& group,
    ScreenshareLayerConfig* config) {
  // Parse field trial group name, containing bitrates for tl0 and tl1.
  int tl0_bitrate;
  int tl1_bitrate;
  if (sscanf(group.c_str(), "%d-%d", &tl0_bitrate, &tl1_bitrate) != 2) {
    return false;
  }

  // Sanity check.
  if (tl0_bitrate < kScreenshareMinBitrateKbps ||
      tl0_bitrate > kScreenshareMaxBitrateKbps ||
      tl1_bitrate < kScreenshareMinBitrateKbps ||
      tl1_bitrate > kScreenshareMaxBitrateKbps || tl0_bitrate > tl1_bitrate) {
    return false;
  }

  config->tl0_bitrate_kbps = tl0_bitrate;
  config->tl1_bitrate_kbps = tl1_bitrate;

  return true;
}

void ConfigureConferenceModeScreencastCodec(webrtc::VideoCodec* codec) {
  codec->codecSpecific.VP8.numberOfTemporalLayers = 2;
  ScreenshareLayerConfig config = ScreenshareLayerConfig::GetDefault();

  // For screenshare in conference mode, tl0 and tl1 bitrates are piggybacked
  // on the VideoCodec struct as target and max bitrates, respectively.
  // See eg. webrtc::VP8EncoderImpl::SetRates().
  codec->targetBitrate = config.tl0_bitrate_kbps;
  codec->maxBitrate = config.tl1_bitrate_kbps;
}

}  // namespace cricket
