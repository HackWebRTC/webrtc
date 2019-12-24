//
// Created by Piasy on 08/11/2017.
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <libavutil/samplefmt.h>

#ifdef __cplusplus
}
#endif

namespace webrtc {

typedef void (*SourceFinishCallback)(void* opaque, int32_t ssrc);

typedef void (*SourceErrorCallback)(void* opaque, int32_t ssrc, int32_t code);

static constexpr AVSampleFormat kOutputSampleFormat = AV_SAMPLE_FMT_S16;

static constexpr int32_t kMixerErrEof = -99;
static constexpr int32_t kMixerErrInit = -100;
static constexpr int32_t kMixerErrDecode = -101;
static constexpr int32_t kMixerErrResample = -102;
}
