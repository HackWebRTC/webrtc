/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#include "talk/app/webrtc/dtmfsender.h"

#include <ctype.h>

#include <string>

#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"

namespace webrtc {

enum {
  MSG_DO_INSERT_DTMF = 0,
};

// RFC4733
//  +-------+--------+------+---------+
//  | Event | Code   | Type | Volume? |
//  +-------+--------+------+---------+
//  | 0--9  | 0--9   | tone | yes     |
//  | *     | 10     | tone | yes     |
//  | #     | 11     | tone | yes     |
//  | A--D  | 12--15 | tone | yes     |
//  +-------+--------+------+---------+
// The "," is a special event defined by the WebRTC spec. It means to delay for
// 2 seconds before processing the next tone. We use -1 as its code.
static const int kDtmfCodeTwoSecondDelay = -1;
static const int kDtmfTwoSecondInMs = 2000;
static const char kDtmfValidTones[] = ",0123456789*#ABCDabcd";
static const char kDtmfTonesTable[] = ",0123456789*#ABCD";
// The duration cannot be more than 6000ms or less than 70ms. The gap between
// tones must be at least 50 ms.
static const int kDtmfDefaultDurationMs = 100;
static const int kDtmfMinDurationMs = 70;
static const int kDtmfMaxDurationMs = 6000;
static const int kDtmfDefaultGapMs = 50;
static const int kDtmfMinGapMs = 50;

// Get DTMF code from the DTMF event character.
bool GetDtmfCode(char tone, int* code) {
  // Convert a-d to A-D.
  char event = toupper(tone);
  const char* p = strchr(kDtmfTonesTable, event);
  if (!p) {
    return false;
  }
  *code = p - kDtmfTonesTable - 1;
  return true;
}

rtc::scoped_refptr<DtmfSender> DtmfSender::Create(
    AudioTrackInterface* track,
    rtc::Thread* signaling_thread,
    DtmfProviderInterface* provider) {
  if (!track || !signaling_thread) {
    return NULL;
  }
  rtc::scoped_refptr<DtmfSender> dtmf_sender(
      new rtc::RefCountedObject<DtmfSender>(track, signaling_thread,
                                                  provider));
  return dtmf_sender;
}

DtmfSender::DtmfSender(AudioTrackInterface* track,
                       rtc::Thread* signaling_thread,
                       DtmfProviderInterface* provider)
    : track_(track),
      observer_(NULL),
      signaling_thread_(signaling_thread),
      provider_(provider),
      duration_(kDtmfDefaultDurationMs),
      inter_tone_gap_(kDtmfDefaultGapMs) {
  ASSERT(track_ != NULL);
  ASSERT(signaling_thread_ != NULL);
  if (provider_) {
    ASSERT(provider_->GetOnDestroyedSignal() != NULL);
    provider_->GetOnDestroyedSignal()->connect(
        this, &DtmfSender::OnProviderDestroyed);
  }
}

DtmfSender::~DtmfSender() {
  if (provider_) {
    ASSERT(provider_->GetOnDestroyedSignal() != NULL);
    provider_->GetOnDestroyedSignal()->disconnect(this);
  }
  StopSending();
}

void DtmfSender::RegisterObserver(DtmfSenderObserverInterface* observer) {
  observer_ = observer;
}

void DtmfSender::UnregisterObserver() {
  observer_ = NULL;
}

bool DtmfSender::CanInsertDtmf() {
  ASSERT(signaling_thread_->IsCurrent());
  if (!provider_) {
    return false;
  }
  return provider_->CanInsertDtmf(track_->id());
}

bool DtmfSender::InsertDtmf(const std::string& tones, int duration,
                            int inter_tone_gap) {
  ASSERT(signaling_thread_->IsCurrent());

  if (duration > kDtmfMaxDurationMs ||
      duration < kDtmfMinDurationMs ||
      inter_tone_gap < kDtmfMinGapMs) {
    LOG(LS_ERROR) << "InsertDtmf is called with invalid duration or tones gap. "
        << "The duration cannot be more than " << kDtmfMaxDurationMs
        << "ms or less than " << kDtmfMinDurationMs << "ms. "
        << "The gap between tones must be at least " << kDtmfMinGapMs << "ms.";
    return false;
  }

  if (!CanInsertDtmf()) {
    LOG(LS_ERROR)
        << "InsertDtmf is called on DtmfSender that can't send DTMF.";
    return false;
  }

  tones_ = tones;
  duration_ = duration;
  inter_tone_gap_ = inter_tone_gap;
  // Clear the previous queue.
  signaling_thread_->Clear(this, MSG_DO_INSERT_DTMF);
  // Kick off a new DTMF task queue.
  signaling_thread_->Post(this, MSG_DO_INSERT_DTMF);
  return true;
}

const AudioTrackInterface* DtmfSender::track() const {
  return track_;
}

std::string DtmfSender::tones() const {
  return tones_;
}

int DtmfSender::duration() const {
  return duration_;
}

int DtmfSender::inter_tone_gap() const {
  return inter_tone_gap_;
}

void DtmfSender::OnMessage(rtc::Message* msg) {
  switch (msg->message_id) {
    case MSG_DO_INSERT_DTMF: {
      DoInsertDtmf();
      break;
    }
    default: {
      ASSERT(false);
      break;
    }
  }
}

void DtmfSender::DoInsertDtmf() {
  ASSERT(signaling_thread_->IsCurrent());

  // Get the first DTMF tone from the tone buffer. Unrecognized characters will
  // be ignored and skipped.
  size_t first_tone_pos = tones_.find_first_of(kDtmfValidTones);
  int code = 0;
  if (first_tone_pos == std::string::npos) {
    tones_.clear();
    // Fire a “OnToneChange” event with an empty string and stop.
    if (observer_) {
      observer_->OnToneChange(std::string());
    }
    return;
  } else {
    char tone = tones_[first_tone_pos];
    if (!GetDtmfCode(tone, &code)) {
      // The find_first_of(kDtmfValidTones) should have guarantee |tone| is
      // a valid DTMF tone.
      ASSERT(false);
    }
  }

  int tone_gap = inter_tone_gap_;
  if (code == kDtmfCodeTwoSecondDelay) {
    // Special case defined by WebRTC - The character',' indicates a delay of 2
    // seconds before processing the next character in the tones parameter.
    tone_gap = kDtmfTwoSecondInMs;
  } else {
    if (!provider_) {
      LOG(LS_ERROR) << "The DtmfProvider has been destroyed.";
      return;
    }
    // The provider starts playout of the given tone on the
    // associated RTP media stream, using the appropriate codec.
    if (!provider_->InsertDtmf(track_->id(), code, duration_)) {
      LOG(LS_ERROR) << "The DtmfProvider can no longer send DTMF.";
      return;
    }
    // Wait for the number of milliseconds specified by |duration_|.
    tone_gap += duration_;
  }

  // Fire a “OnToneChange” event with the tone that's just processed.
  if (observer_) {
    observer_->OnToneChange(tones_.substr(first_tone_pos, 1));
  }

  // Erase the unrecognized characters plus the tone that's just processed.
  tones_.erase(0, first_tone_pos + 1);

  // Continue with the next tone.
  signaling_thread_->PostDelayed(tone_gap, this, MSG_DO_INSERT_DTMF);
}

void DtmfSender::OnProviderDestroyed() {
  LOG(LS_INFO) << "The Dtmf provider is deleted. Clear the sending queue.";
  StopSending();
  provider_ = NULL;
}

void DtmfSender::StopSending() {
  signaling_thread_->Clear(this);
}

}  // namespace webrtc
