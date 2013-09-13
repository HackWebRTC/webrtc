/*
 * libjingle
 * Copyright 2013, Google Inc.
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

#include "talk/media/base/mediaengine.h"
#include "talk/app/webrtc/mediaconstraintsinterface.h"

using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;

namespace webrtc {

// Constraint keys.
// They are declared as static members in mediaconstraintsinterface.h
const char MediaConstraintsInterface::kEchoCancellation[] =
    "googEchoCancellation";
const char MediaConstraintsInterface::kExperimentalEchoCancellation[] =
    "googEchoCancellation2";
const char MediaConstraintsInterface::kAutoGainControl[] =
    "googAutoGainControl";
const char MediaConstraintsInterface::kExperimentalAutoGainControl[] =
    "googAutoGainControl2";
const char MediaConstraintsInterface::kNoiseSuppression[] =
    "googNoiseSuppression";
const char MediaConstraintsInterface::kHighpassFilter[] =
    "googHighpassFilter";
const char MediaConstraintsInterface::kTypingNoiseDetection[] =
    "googTypingNoiseDetection";
const char MediaConstraintsInterface::kInternalAecDump[] = "internalAecDump";

namespace {

// Convert constraints to audio options. Return false if constraints are
// invalid.
bool FromConstraints(const MediaConstraintsInterface::Constraints& constraints,
                     cricket::AudioOptions* options) {
  bool success = true;
  MediaConstraintsInterface::Constraints::const_iterator iter;

  // This design relies on the fact that all the audio constraints are actually
  // "options", i.e. boolean-valued and always satisfiable.  If the constraints
  // are extended to include non-boolean values or actual format constraints,
  // a different algorithm will be required.
  for (iter = constraints.begin(); iter != constraints.end(); ++iter) {
    bool value = false;

    if (!talk_base::FromString(iter->value, &value)) {
      success = false;
      continue;
    }

    if (iter->key == MediaConstraintsInterface::kEchoCancellation)
      options->echo_cancellation.Set(value);
    else if (iter->key ==
        MediaConstraintsInterface::kExperimentalEchoCancellation)
      options->experimental_aec.Set(value);
    else if (iter->key == MediaConstraintsInterface::kAutoGainControl)
      options->auto_gain_control.Set(value);
    else if (iter->key ==
        MediaConstraintsInterface::kExperimentalAutoGainControl)
      options->experimental_agc.Set(value);
    else if (iter->key == MediaConstraintsInterface::kNoiseSuppression)
      options->noise_suppression.Set(value);
    else if (iter->key == MediaConstraintsInterface::kHighpassFilter)
      options->highpass_filter.Set(value);
    else if (iter->key == MediaConstraintsInterface::kInternalAecDump)
      options->aec_dump.Set(value);
    else if (iter->key == MediaConstraintsInterface::kTypingNoiseDetection)
      options->typing_detection.Set(value);
    else
      success = false;
  }
  return success;
}

}  // namespace

talk_base::scoped_refptr<LocalAudioSource> LocalAudioSource::Create(
    const MediaConstraintsInterface* constraints) {
  talk_base::scoped_refptr<LocalAudioSource> source(
      new talk_base::RefCountedObject<LocalAudioSource>());
  source->Initialize(constraints);
  return source;
}

void LocalAudioSource::Initialize(
    const MediaConstraintsInterface* constraints) {
  if (!constraints)
    return;

  // Apply optional constraints first, they will be overwritten by mandatory
  // constraints.
  FromConstraints(constraints->GetOptional(), &options_);

  cricket::AudioOptions options;
  if (!FromConstraints(constraints->GetMandatory(), &options)) {
    source_state_ = kEnded;
    return;
  }
  options_.SetAll(options);
  source_state_ = kLive;
}

}  // namespace webrtc
