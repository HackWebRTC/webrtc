/*
 * libjingle
 * Copyright 2004--2010, Google Inc.
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

#ifndef TALK_SOUND_LINUXSOUNDSYSTEM_H_
#define TALK_SOUND_LINUXSOUNDSYSTEM_H_

#include "talk/sound/automaticallychosensoundsystem.h"

namespace cricket {

extern const SoundSystemCreator kLinuxSoundSystemCreators[
#ifdef HAVE_LIBPULSE
    2
#else
    1
#endif
    ];

// The vast majority of Linux systems use ALSA for the device-level sound API,
// but an increasing number are using PulseAudio for the application API and
// only using ALSA internally in PulseAudio itself. But like everything on
// Linux this is user-configurable, so we need to support both and choose the
// right one at run-time.
// PulseAudioSoundSystem is designed to only successfully initialize if
// PulseAudio is installed and running, and if it is running then direct device
// access using ALSA typically won't work, so if PulseAudioSoundSystem
// initializes then we choose that. Otherwise we choose ALSA.
typedef AutomaticallyChosenSoundSystem<
    kLinuxSoundSystemCreators,
    ARRAY_SIZE(kLinuxSoundSystemCreators)> LinuxSoundSystem;

}  // namespace cricket

#endif  // TALK_SOUND_LINUXSOUNDSYSTEM_H_
