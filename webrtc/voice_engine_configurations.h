/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_CONFIGURATIONS_H_
#define WEBRTC_VOICE_ENGINE_CONFIGURATIONS_H_

#include "webrtc/typedefs.h"

// ============================================================================
//                                 VoiceEngine
// ============================================================================

// ----------------------------------------------------------------------------
//  Settings for VoiceEngine
// ----------------------------------------------------------------------------

#if !defined(WEBRTC_ANDROID) && !defined(WEBRTC_IOS)
#define WEBRTC_VOICE_ENGINE_TYPING_DETECTION    // Typing detection
#endif

// ----------------------------------------------------------------------------
//  VoiceEngine sub-APIs
// ----------------------------------------------------------------------------

#define WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API
#define WEBRTC_VOICE_ENGINE_CODEC_API
#define WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
#define WEBRTC_VOICE_ENGINE_FILE_API
#define WEBRTC_VOICE_ENGINE_HARDWARE_API
#define WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
#define WEBRTC_VOICE_ENGINE_RTP_RTCP_API
#define WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
#define WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API

#endif  // WEBRTC_VOICE_ENGINE_CONFIGURATIONS_H_
