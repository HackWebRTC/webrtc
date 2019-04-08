/*
 *  Copyright 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/playout_latency.h"

#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/numerics/safe_minmax.h"
#include "rtc_base/thread.h"
#include "rtc_base/thread_checker.h"

namespace {
constexpr int kDefaultLatency = 0;
constexpr int kMaximumDelayMs = 10000;
}  // namespace

namespace webrtc {

PlayoutLatency::PlayoutLatency(rtc::Thread* worker_thread)
    : signaling_thread_(rtc::Thread::Current()), worker_thread_(worker_thread) {
  RTC_DCHECK(worker_thread_);
}

void PlayoutLatency::OnStart(cricket::Delayable* media_channel, uint32_t ssrc) {
  RTC_DCHECK_RUN_ON(signaling_thread_);

  media_channel_ = media_channel;
  ssrc_ = ssrc;

  // Trying to apply cached latency for the audio stream.
  if (cached_latency_) {
    SetLatency(cached_latency_.value());
  }
}

void PlayoutLatency::OnStop() {
  RTC_DCHECK_RUN_ON(signaling_thread_);
  // Assume that audio stream is no longer present for latency calls.
  media_channel_ = nullptr;
  ssrc_ = absl::nullopt;
}

void PlayoutLatency::SetLatency(double latency) {
  RTC_DCHECK_RUN_ON(worker_thread_);

  int delay_ms = rtc::dchecked_cast<int>(latency * 1000);
  delay_ms = rtc::SafeClamp(delay_ms, 0, kMaximumDelayMs);

  cached_latency_ = latency;
  if (media_channel_ && ssrc_) {
    media_channel_->SetBaseMinimumPlayoutDelayMs(ssrc_.value(), delay_ms);
  }
}

double PlayoutLatency::GetLatency() const {
  RTC_DCHECK_RUN_ON(worker_thread_);

  absl::optional<int> delay_ms;
  if (media_channel_ && ssrc_) {
    delay_ms = media_channel_->GetBaseMinimumPlayoutDelayMs(ssrc_.value());
  }

  if (delay_ms) {
    return delay_ms.value() / 1000.0;
  } else {
    return cached_latency_.value_or(kDefaultLatency);
  }
}

}  // namespace webrtc
