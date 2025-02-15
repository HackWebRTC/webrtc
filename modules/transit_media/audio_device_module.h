#pragma once

#include "api/scoped_refptr.h"
#include "modules/audio_device/include/audio_device.h"

namespace webrtc {

rtc::scoped_refptr<AudioDeviceModule> CreateMuteAudioDeviceModule();

}  // namespace webrtc
