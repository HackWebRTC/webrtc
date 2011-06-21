/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_LEVEL_ESTIMATOR_IMPL_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_LEVEL_ESTIMATOR_IMPL_H_

#include "audio_processing.h"
#include "processing_component.h"

namespace webrtc {
class AudioProcessingImpl;
class AudioBuffer;

class LevelEstimatorImpl : public LevelEstimator,
                           public ProcessingComponent {
 public:
  explicit LevelEstimatorImpl(const AudioProcessingImpl* apm);
  virtual ~LevelEstimatorImpl();

  int AnalyzeReverseStream(AudioBuffer* audio);
  int ProcessCaptureAudio(AudioBuffer* audio);

  // LevelEstimator implementation.
  virtual bool is_enabled() const;

  // ProcessingComponent implementation.
  virtual int get_version(char* version, int version_len_bytes) const;

 private:
  // LevelEstimator implementation.
  virtual int Enable(bool enable);
  virtual int GetMetrics(Metrics* metrics, Metrics* reverse_metrics);

  // ProcessingComponent implementation.
  virtual void* CreateHandle() const;
  virtual int InitializeHandle(void* handle) const;
  virtual int ConfigureHandle(void* handle) const;
  virtual int DestroyHandle(void* handle) const;
  virtual int num_handles_required() const;
  virtual int GetHandleError(void* handle) const;

  const AudioProcessingImpl* apm_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_LEVEL_ESTIMATOR_IMPL_H_
