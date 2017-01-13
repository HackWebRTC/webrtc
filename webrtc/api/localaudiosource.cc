/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/api/localaudiosource.h"

#include <vector>

#include "webrtc/api/mediaconstraintsinterface.h"
#include "webrtc/media/base/mediaengine.h"

using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;

namespace webrtc {

rtc::scoped_refptr<LocalAudioSource> LocalAudioSource::Create(
    const PeerConnectionFactoryInterface::Options& options,
    const MediaConstraintsInterface* constraints) {
  rtc::scoped_refptr<LocalAudioSource> source(
      new rtc::RefCountedObject<LocalAudioSource>());
  source->Initialize(options, constraints);
  return source;
}

rtc::scoped_refptr<LocalAudioSource> LocalAudioSource::Create(
    const PeerConnectionFactoryInterface::Options& options,
    const cricket::AudioOptions* audio_options) {
  rtc::scoped_refptr<LocalAudioSource> source(
      new rtc::RefCountedObject<LocalAudioSource>());
  source->Initialize(options, audio_options);
  return source;
}

void LocalAudioSource::Initialize(
    const PeerConnectionFactoryInterface::Options& options,
    const MediaConstraintsInterface* constraints) {
  CopyConstraintsIntoAudioOptions(constraints, &options_);
}

void LocalAudioSource::Initialize(
    const PeerConnectionFactoryInterface::Options& options,
    const cricket::AudioOptions* audio_options) {
  if (!audio_options)
    return;

  options_ = *audio_options;
}

}  // namespace webrtc
