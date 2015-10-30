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

#include <string>
#include <vector>

#include "talk/app/webrtc/test/fakeconstraints.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/base/fakevideorenderer.h"
#include "webrtc/base/gunit.h"

using webrtc::LocalAudioSource;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaSourceInterface;
using webrtc::PeerConnectionFactoryInterface;

TEST(LocalAudioSourceTest, SetValidOptions) {
  webrtc::FakeConstraints constraints;
  constraints.AddMandatory(
      MediaConstraintsInterface::kGoogEchoCancellation, false);
  constraints.AddOptional(
      MediaConstraintsInterface::kExtendedFilterEchoCancellation, true);
  constraints.AddOptional(MediaConstraintsInterface::kDAEchoCancellation, true);
  constraints.AddOptional(MediaConstraintsInterface::kAutoGainControl, true);
  constraints.AddOptional(
      MediaConstraintsInterface::kExperimentalAutoGainControl, true);
  constraints.AddMandatory(MediaConstraintsInterface::kNoiseSuppression, false);
  constraints.AddOptional(MediaConstraintsInterface::kHighpassFilter, true);
  constraints.AddOptional(MediaConstraintsInterface::kAecDump, true);

  rtc::scoped_refptr<LocalAudioSource> source =
      LocalAudioSource::Create(PeerConnectionFactoryInterface::Options(),
                               &constraints);

  EXPECT_EQ(rtc::Maybe<bool>(false), source->options().echo_cancellation);
  EXPECT_EQ(rtc::Maybe<bool>(true), source->options().extended_filter_aec);
  EXPECT_EQ(rtc::Maybe<bool>(true), source->options().delay_agnostic_aec);
  EXPECT_EQ(rtc::Maybe<bool>(true), source->options().auto_gain_control);
  EXPECT_EQ(rtc::Maybe<bool>(true), source->options().experimental_agc);
  EXPECT_EQ(rtc::Maybe<bool>(false), source->options().noise_suppression);
  EXPECT_EQ(rtc::Maybe<bool>(true), source->options().highpass_filter);
  EXPECT_EQ(rtc::Maybe<bool>(true), source->options().aec_dump);
}

TEST(LocalAudioSourceTest, OptionNotSet) {
  webrtc::FakeConstraints constraints;
  rtc::scoped_refptr<LocalAudioSource> source =
      LocalAudioSource::Create(PeerConnectionFactoryInterface::Options(),
                               &constraints);
  EXPECT_EQ(rtc::Maybe<bool>(), source->options().highpass_filter);
}

TEST(LocalAudioSourceTest, MandatoryOverridesOptional) {
  webrtc::FakeConstraints constraints;
  constraints.AddMandatory(
      MediaConstraintsInterface::kGoogEchoCancellation, false);
  constraints.AddOptional(
      MediaConstraintsInterface::kGoogEchoCancellation, true);

  rtc::scoped_refptr<LocalAudioSource> source =
      LocalAudioSource::Create(PeerConnectionFactoryInterface::Options(),
                               &constraints);

  EXPECT_EQ(rtc::Maybe<bool>(false), source->options().echo_cancellation);
}

TEST(LocalAudioSourceTest, InvalidOptional) {
  webrtc::FakeConstraints constraints;
  constraints.AddOptional(MediaConstraintsInterface::kHighpassFilter, false);
  constraints.AddOptional("invalidKey", false);

  rtc::scoped_refptr<LocalAudioSource> source =
      LocalAudioSource::Create(PeerConnectionFactoryInterface::Options(),
                               &constraints);

  EXPECT_EQ(MediaSourceInterface::kLive, source->state());
  EXPECT_EQ(rtc::Maybe<bool>(false), source->options().highpass_filter);
}

TEST(LocalAudioSourceTest, InvalidMandatory) {
  webrtc::FakeConstraints constraints;
  constraints.AddMandatory(MediaConstraintsInterface::kHighpassFilter, false);
  constraints.AddMandatory("invalidKey", false);

  rtc::scoped_refptr<LocalAudioSource> source =
      LocalAudioSource::Create(PeerConnectionFactoryInterface::Options(),
                               &constraints);

  EXPECT_EQ(MediaSourceInterface::kLive, source->state());
  EXPECT_EQ(rtc::Maybe<bool>(false), source->options().highpass_filter);
}
