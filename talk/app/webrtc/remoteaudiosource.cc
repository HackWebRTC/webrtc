/*
 * libjingle
 * Copyright 2014 Google Inc.
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

#include "talk/app/webrtc/remoteaudiosource.h"

#include <algorithm>
#include <functional>
#include <utility>

#include "talk/app/webrtc/mediastreamprovider.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/thread.h"

namespace webrtc {

class RemoteAudioSource::MessageHandler : public rtc::MessageHandler {
 public:
  explicit MessageHandler(RemoteAudioSource* source) : source_(source) {}

 private:
  ~MessageHandler() override {}

  void OnMessage(rtc::Message* msg) override {
    source_->OnMessage(msg);
    delete this;
  }

  const rtc::scoped_refptr<RemoteAudioSource> source_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(MessageHandler);
};

class RemoteAudioSource::Sink : public AudioSinkInterface {
 public:
  explicit Sink(RemoteAudioSource* source) : source_(source) {}
  ~Sink() override { source_->OnAudioProviderGone(); }

 private:
  void OnData(const AudioSinkInterface::Data& audio) override {
    if (source_)
      source_->OnData(audio);
  }

  const rtc::scoped_refptr<RemoteAudioSource> source_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(Sink);
};

rtc::scoped_refptr<RemoteAudioSource> RemoteAudioSource::Create(
    uint32_t ssrc,
    AudioProviderInterface* provider) {
  rtc::scoped_refptr<RemoteAudioSource> ret(
      new rtc::RefCountedObject<RemoteAudioSource>());
  ret->Initialize(ssrc, provider);
  return ret;
}

RemoteAudioSource::RemoteAudioSource()
    : main_thread_(rtc::Thread::Current()),
      state_(MediaSourceInterface::kLive) {
  RTC_DCHECK(main_thread_);
}

RemoteAudioSource::~RemoteAudioSource() {
  RTC_DCHECK(main_thread_->IsCurrent());
  RTC_DCHECK(audio_observers_.empty());
  RTC_DCHECK(sinks_.empty());
}

void RemoteAudioSource::Initialize(uint32_t ssrc,
                                   AudioProviderInterface* provider) {
  RTC_DCHECK(main_thread_->IsCurrent());
  // To make sure we always get notified when the provider goes out of scope,
  // we register for callbacks here and not on demand in AddSink.
  if (provider) {  // May be null in tests.
    provider->SetRawAudioSink(
        ssrc, rtc::scoped_ptr<AudioSinkInterface>(new Sink(this)));
  }
}

MediaSourceInterface::SourceState RemoteAudioSource::state() const {
  RTC_DCHECK(main_thread_->IsCurrent());
  return state_;
}

bool RemoteAudioSource::remote() const {
  RTC_DCHECK(main_thread_->IsCurrent());
  return true;
}

void RemoteAudioSource::SetVolume(double volume) {
  RTC_DCHECK(volume >= 0 && volume <= 10);
  for (auto* observer : audio_observers_)
    observer->OnSetVolume(volume);
}

void RemoteAudioSource::RegisterAudioObserver(AudioObserver* observer) {
  RTC_DCHECK(observer != NULL);
  RTC_DCHECK(std::find(audio_observers_.begin(), audio_observers_.end(),
                       observer) == audio_observers_.end());
  audio_observers_.push_back(observer);
}

void RemoteAudioSource::UnregisterAudioObserver(AudioObserver* observer) {
  RTC_DCHECK(observer != NULL);
  audio_observers_.remove(observer);
}

void RemoteAudioSource::AddSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK(main_thread_->IsCurrent());
  RTC_DCHECK(sink);

  if (state_ != MediaSourceInterface::kLive) {
    LOG(LS_ERROR) << "Can't register sink as the source isn't live.";
    return;
  }

  rtc::CritScope lock(&sink_lock_);
  RTC_DCHECK(std::find(sinks_.begin(), sinks_.end(), sink) == sinks_.end());
  sinks_.push_back(sink);
}

void RemoteAudioSource::RemoveSink(AudioTrackSinkInterface* sink) {
  RTC_DCHECK(main_thread_->IsCurrent());
  RTC_DCHECK(sink);

  rtc::CritScope lock(&sink_lock_);
  sinks_.remove(sink);
}

void RemoteAudioSource::OnData(const AudioSinkInterface::Data& audio) {
  // Called on the externally-owned audio callback thread, via/from webrtc.
  rtc::CritScope lock(&sink_lock_);
  for (auto* sink : sinks_) {
    sink->OnData(audio.data, 16, audio.sample_rate, audio.channels,
                 audio.samples_per_channel);
  }
}

void RemoteAudioSource::OnAudioProviderGone() {
  // Called when the data provider is deleted.  It may be the worker thread
  // in libjingle or may be a different worker thread.
  main_thread_->Post(new MessageHandler(this));
}

void RemoteAudioSource::OnMessage(rtc::Message* msg) {
  RTC_DCHECK(main_thread_->IsCurrent());
  sinks_.clear();
  state_ = MediaSourceInterface::kEnded;
  FireOnChanged();
}

}  // namespace webrtc
