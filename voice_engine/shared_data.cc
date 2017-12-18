/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine/shared_data.h"

#include "voice_engine/channel.h"

namespace webrtc {

namespace voe {

static int32_t _gInstanceCounter = 0;

SharedData::SharedData()
    : _instanceId(++_gInstanceCounter),
      _channelManager(_gInstanceCounter),
      _audioDevicePtr(NULL),
      _moduleProcessThreadPtr(ProcessThread::Create("VoiceProcessThread")),
      encoder_queue_("AudioEncoderQueue") {
}

SharedData::~SharedData()
{
    if (_audioDevicePtr) {
        _audioDevicePtr->Release();
    }
    _moduleProcessThreadPtr->Stop();
}

rtc::TaskQueue* SharedData::encoder_queue() {
  RTC_DCHECK_RUN_ON(&construction_thread_);
  return &encoder_queue_;
}

void SharedData::set_audio_device(
    const rtc::scoped_refptr<AudioDeviceModule>& audio_device) {
  _audioDevicePtr = audio_device;
}
}  // namespace voe

}  // namespace webrtc
