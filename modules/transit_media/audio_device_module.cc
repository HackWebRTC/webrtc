#include "modules/transit_media/audio_device_module.h"

#include "modules/transit_media/mute_audio_device_module.h"
#include "rtc_base/ref_counted_object.h"

namespace webrtc {

rtc::scoped_refptr<AudioDeviceModule> CreateMuteAudioDeviceModule() {
  return new rtc::RefCountedObject<MuteAudioDeviceModule>();
}

}  // namespace webrtc
