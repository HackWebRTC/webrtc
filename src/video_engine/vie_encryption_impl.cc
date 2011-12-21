/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_encryption_impl.h"

#include "system_wrappers/interface/trace.h"
#include "video_engine/main/interface/vie_errors.h"
#include "video_engine/vie_channel.h"
#include "video_engine/vie_channel_manager.h"
#include "video_engine/vie_defines.h"
#include "video_engine/vie_impl.h"

namespace webrtc {

ViEEncryption* ViEEncryption::GetInterface(VideoEngine* video_engine) {
#ifdef WEBRTC_VIDEO_ENGINE_ENCRYPTION_API
  if (video_engine == NULL) {
    return NULL;
  }
  VideoEngineImpl* vie_impl = reinterpret_cast<VideoEngineImpl*>(video_engine);
  ViEEncryptionImpl* vie_encryption_impl = vie_impl;
  // Increase ref count.
  (*vie_encryption_impl)++;
  return vie_encryption_impl;
#else
  return NULL;
#endif
}

int ViEEncryptionImpl::Release() {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, instance_id_,
               "ViEEncryptionImpl::Release()");
  // Decrease ref count.
  (*this)--;

  WebRtc_Word32 ref_count = GetCount();
  if (ref_count < 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, instance_id_,
                 "ViEEncryptionImpl release too many times");
    SetLastError(kViEAPIDoesNotExist);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, instance_id_,
               "ViEEncryptionImpl reference count: %d", ref_count);
  return ref_count;
}

ViEEncryptionImpl::ViEEncryptionImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, instance_id_,
               "ViEEncryptionImpl::ViEEncryptionImpl() Ctor");
}

ViEEncryptionImpl::~ViEEncryptionImpl() {
  WEBRTC_TRACE(kTraceMemory, kTraceVideo, instance_id_,
               "ViEEncryptionImpl::~ViEEncryptionImpl() Dtor");
}

int ViEEncryptionImpl::RegisterExternalEncryption(const int video_channel,
                                                  Encryption& encryption) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, video_channel),
               "RegisterExternalEncryption(video_channel=%d)", video_channel);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (vie_channel == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVideo,
                 ViEId(instance_id_, video_channel), "%s: No channel %d",
                 __FUNCTION__, video_channel);
    SetLastError(kViEEncryptionInvalidChannelId);
    return -1;
  }
  if (vie_channel->RegisterExternalEncryption(&encryption) != 0) {
    SetLastError(kViEEncryptionUnknownError);
    return -1;
  }
  return 0;
}

int ViEEncryptionImpl::DeregisterExternalEncryption(const int video_channel) {
  WEBRTC_TRACE(kTraceApiCall, kTraceVideo, ViEId(instance_id_, video_channel),
               "RegisterExternalEncryption(video_channel=%d)", video_channel);

  ViEChannelManagerScoped cs(channel_manager_);
  ViEChannel* vie_channel = cs.Channel(video_channel);
  if (vie_channel == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(instance_id_, video_channel),
                 "%s: No channel %d", __FUNCTION__, video_channel);
    SetLastError(kViEEncryptionInvalidChannelId);
    return -1;
  }

  if (vie_channel->DeRegisterExternalEncryption() != 0) {
    SetLastError(kViEEncryptionUnknownError);
    return -1;
  }
  return 0;
}

}  // namespace webrtc
