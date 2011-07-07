/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_PROCESSING_COMPONENT_H_
#define WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_PROCESSING_COMPONENT_H_

#include <vector>

#include "audio_processing.h"

namespace webrtc {
class AudioProcessingImpl;

/*template <class T>
class ComponentHandle {
  public:
    ComponentHandle();
    virtual ~ComponentHandle();

    virtual int Create() = 0;
    virtual T* ptr() const = 0;
};*/

class ProcessingComponent {
 public:
  explicit ProcessingComponent(const AudioProcessingImpl* apm);
  virtual ~ProcessingComponent();

  virtual int Initialize();
  virtual int Destroy();
  virtual int get_version(char* version, int version_len_bytes) const = 0;

 protected:
  virtual int Configure();
  int EnableComponent(bool enable);
  bool is_component_enabled() const;
  void* handle(int index) const;
  int num_handles() const;

 private:
  virtual void* CreateHandle() const = 0;
  virtual int InitializeHandle(void* handle) const = 0;
  virtual int ConfigureHandle(void* handle) const = 0;
  virtual int DestroyHandle(void* handle) const = 0;
  virtual int num_handles_required() const = 0;
  virtual int GetHandleError(void* handle) const = 0;

  const AudioProcessingImpl* apm_;
  std::vector<void*> handles_;
  bool initialized_;
  bool enabled_;
  int num_handles_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_PROCESSING_MAIN_SOURCE_PROCESSING_COMPONENT_H__
