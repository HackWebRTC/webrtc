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

#include <string>
#include <vector>

#include "talk/app/webrtc/test/fakeconstraints.h"
#include "talk/base/gunit.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/base/fakevideorenderer.h"
#include "talk/media/devices/fakedevicemanager.h"

using webrtc::LocalAudioSource;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;

TEST(LocalAudioSourceTest, SetValidOptions) {
  webrtc::FakeConstraints constraints;
  constraints.AddMandatory(MediaConstraintsInterface::kEchoCancellation, false);
  constraints.AddOptional(
      MediaConstraintsInterface::kExperimentalEchoCancellation, true);
  constraints.AddOptional(MediaConstraintsInterface::kAutoGainControl, true);
  constraints.AddOptional(
      MediaConstraintsInterface::kExperimentalAutoGainControl, true);
  constraints.AddMandatory(MediaConstraintsInterface::kNoiseSuppression, false);
  constraints.AddOptional(MediaConstraintsInterface::kHighpassFilter, true);

  talk_base::scoped_refptr<LocalAudioSource> source =
      LocalAudioSource::Create(&constraints);

  bool value;
  EXPECT_TRUE(source->options().echo_cancellation.Get(&value));
  EXPECT_FALSE(value);
  EXPECT_TRUE(source->options().experimental_aec.Get(&value));
  EXPECT_TRUE(value);
  EXPECT_TRUE(source->options().auto_gain_control.Get(&value));
  EXPECT_TRUE(value);
  EXPECT_TRUE(source->options().experimental_agc.Get(&value));
  EXPECT_TRUE(value);
  EXPECT_TRUE(source->options().noise_suppression.Get(&value));
  EXPECT_FALSE(value);
  EXPECT_TRUE(source->options().highpass_filter.Get(&value));
  EXPECT_TRUE(value);
}

TEST(LocalAudioSourceTest, OptionNotSet) {
  webrtc::FakeConstraints constraints;
  talk_base::scoped_refptr<LocalAudioSource> source =
      LocalAudioSource::Create(&constraints);
  bool value;
  EXPECT_FALSE(source->options().highpass_filter.Get(&value));
}

TEST(LocalAudioSourceTest, MandatoryOverridesOptional) {
  webrtc::FakeConstraints constraints;
  constraints.AddMandatory(MediaConstraintsInterface::kEchoCancellation, false);
  constraints.AddOptional(MediaConstraintsInterface::kEchoCancellation, true);

  talk_base::scoped_refptr<LocalAudioSource> source =
      LocalAudioSource::Create(&constraints);

  bool value;
  EXPECT_TRUE(source->options().echo_cancellation.Get(&value));
  EXPECT_FALSE(value);
}

TEST(LocalAudioSourceTest, InvalidOptional) {
  webrtc::FakeConstraints constraints;
  constraints.AddOptional(MediaConstraintsInterface::kHighpassFilter, false);
  constraints.AddOptional("invalidKey", false);

  talk_base::scoped_refptr<LocalAudioSource> source =
      LocalAudioSource::Create(&constraints);

  EXPECT_EQ(MediaSourceInterface::kLive, source->state());
  bool value;
  EXPECT_TRUE(source->options().highpass_filter.Get(&value));
  EXPECT_FALSE(value);
}

TEST(LocalAudioSourceTest, InvalidMandatory) {
  webrtc::FakeConstraints constraints;
  constraints.AddMandatory(MediaConstraintsInterface::kHighpassFilter, false);
  constraints.AddMandatory("invalidKey", false);

  talk_base::scoped_refptr<LocalAudioSource> source =
      LocalAudioSource::Create(&constraints);

  EXPECT_EQ(MediaSourceInterface::kEnded, source->state());
  bool value;
  EXPECT_FALSE(source->options().highpass_filter.Get(&value));
}
