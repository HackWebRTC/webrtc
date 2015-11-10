/*
 * libjingle
 * Copyright 2013 Google Inc.
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

#include "talk/app/webrtc/localaudiosource.h"

#include <vector>

#include "talk/app/webrtc/mediaconstraintsinterface.h"
#include "talk/media/base/mediaengine.h"

using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;

namespace webrtc {

namespace {

// Convert constraints to audio options. Return false if constraints are
// invalid.
void FromConstraints(const MediaConstraintsInterface::Constraints& constraints,
                     cricket::AudioOptions* options) {
  // This design relies on the fact that all the audio constraints are actually
  // "options", i.e. boolean-valued and always satisfiable.  If the constraints
  // are extended to include non-boolean values or actual format constraints,
  // a different algorithm will be required.
  struct {
    const char* name;
    rtc::Optional<bool>& value;
  } key_to_value[] = {
      {MediaConstraintsInterface::kGoogEchoCancellation,
       options->echo_cancellation},
      {MediaConstraintsInterface::kExtendedFilterEchoCancellation,
       options->extended_filter_aec},
      {MediaConstraintsInterface::kDAEchoCancellation,
       options->delay_agnostic_aec},
      {MediaConstraintsInterface::kAutoGainControl, options->auto_gain_control},
      {MediaConstraintsInterface::kExperimentalAutoGainControl,
       options->experimental_agc},
      {MediaConstraintsInterface::kNoiseSuppression,
       options->noise_suppression},
      {MediaConstraintsInterface::kExperimentalNoiseSuppression,
       options->experimental_ns},
      {MediaConstraintsInterface::kHighpassFilter, options->highpass_filter},
      {MediaConstraintsInterface::kTypingNoiseDetection,
       options->typing_detection},
      {MediaConstraintsInterface::kAudioMirroring, options->stereo_swapping},
      {MediaConstraintsInterface::kAecDump, options->aec_dump}
  };

  for (const auto& constraint : constraints) {
    bool value = false;
    if (!rtc::FromString(constraint.value, &value))
      continue;

    for (auto& entry : key_to_value) {
      if (constraint.key.compare(entry.name) == 0)
        entry.value = rtc::Optional<bool>(value);
    }
  }
}

}  // namespace

rtc::scoped_refptr<LocalAudioSource> LocalAudioSource::Create(
    const PeerConnectionFactoryInterface::Options& options,
    const MediaConstraintsInterface* constraints) {
  rtc::scoped_refptr<LocalAudioSource> source(
      new rtc::RefCountedObject<LocalAudioSource>());
  source->Initialize(options, constraints);
  return source;
}

void LocalAudioSource::Initialize(
    const PeerConnectionFactoryInterface::Options& options,
    const MediaConstraintsInterface* constraints) {
  if (!constraints)
    return;

  // Apply optional constraints first, they will be overwritten by mandatory
  // constraints.
  FromConstraints(constraints->GetOptional(), &options_);

  cricket::AudioOptions mandatory_options;
  FromConstraints(constraints->GetMandatory(), &mandatory_options);
  options_.SetAll(mandatory_options);
  source_state_ = kLive;
}

}  // namespace webrtc
