/*
 * libjingle
 * Copyright 2012, Google Inc.
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

#include "talk/app/webrtc/mediastreamhandler.h"

#include "talk/app/webrtc/localaudiosource.h"
#include "talk/app/webrtc/videosource.h"
#include "talk/app/webrtc/videosourceinterface.h"

namespace webrtc {

TrackHandler::TrackHandler(MediaStreamTrackInterface* track, uint32 ssrc)
    : track_(track),
      ssrc_(ssrc),
      state_(track->state()),
      enabled_(track->enabled()) {
  track_->RegisterObserver(this);
}

TrackHandler::~TrackHandler() {
  track_->UnregisterObserver(this);
}

void TrackHandler::OnChanged() {
  if (state_ != track_->state()) {
    state_ = track_->state();
    OnStateChanged();
  }
  if (enabled_ != track_->enabled()) {
    enabled_ = track_->enabled();
    OnEnabledChanged();
  }
}

LocalAudioSinkAdapter::LocalAudioSinkAdapter() : sink_(NULL) {}

LocalAudioSinkAdapter::~LocalAudioSinkAdapter() {}

void LocalAudioSinkAdapter::OnData(const void* audio_data,
                                   int bits_per_sample,
                                   int sample_rate,
                                   int number_of_channels,
                                   int number_of_frames) {
  talk_base::CritScope lock(&lock_);
  if (sink_) {
    sink_->OnData(audio_data, bits_per_sample, sample_rate,
                  number_of_channels, number_of_frames);
  }
}

void LocalAudioSinkAdapter::SetSink(cricket::AudioRenderer::Sink* sink) {
  talk_base::CritScope lock(&lock_);
  ASSERT(!sink || !sink_);
  sink_ = sink;
}

LocalAudioTrackHandler::LocalAudioTrackHandler(
    AudioTrackInterface* track,
    uint32 ssrc,
    AudioProviderInterface* provider)
    : TrackHandler(track, ssrc),
      audio_track_(track),
      provider_(provider),
      sink_adapter_(new LocalAudioSinkAdapter()) {
  OnEnabledChanged();
  track->AddSink(sink_adapter_.get());
}

LocalAudioTrackHandler::~LocalAudioTrackHandler() {
}

void LocalAudioTrackHandler::OnStateChanged() {
  // TODO(perkj): What should happen when the state change?
}

void LocalAudioTrackHandler::Stop() {
  audio_track_->RemoveSink(sink_adapter_.get());
  cricket::AudioOptions options;
  provider_->SetAudioSend(ssrc(), false, options, NULL);
}

void LocalAudioTrackHandler::OnEnabledChanged() {
  cricket::AudioOptions options;
  if (audio_track_->enabled() && audio_track_->GetSource()) {
    options = static_cast<LocalAudioSource*>(
        audio_track_->GetSource())->options();
  }

  // Use the renderer if the audio track has one, otherwise use the sink
  // adapter owned by this class.
  cricket::AudioRenderer* renderer = audio_track_->GetRenderer() ?
      audio_track_->GetRenderer() : sink_adapter_.get();
  ASSERT(renderer);
  provider_->SetAudioSend(ssrc(), audio_track_->enabled(), options, renderer);
}

RemoteAudioTrackHandler::RemoteAudioTrackHandler(
    AudioTrackInterface* track,
    uint32 ssrc,
    AudioProviderInterface* provider)
    : TrackHandler(track, ssrc),
      audio_track_(track),
      provider_(provider) {
  OnEnabledChanged();
}

RemoteAudioTrackHandler::~RemoteAudioTrackHandler() {
}

void RemoteAudioTrackHandler::Stop() {
  provider_->SetAudioPlayout(ssrc(), false, NULL);
}

void RemoteAudioTrackHandler::OnStateChanged() {
}

void RemoteAudioTrackHandler::OnEnabledChanged() {
  provider_->SetAudioPlayout(ssrc(), audio_track_->enabled(),
                             audio_track_->GetRenderer());
}

LocalVideoTrackHandler::LocalVideoTrackHandler(
    VideoTrackInterface* track,
    uint32 ssrc,
    VideoProviderInterface* provider)
    : TrackHandler(track, ssrc),
      local_video_track_(track),
      provider_(provider) {
  VideoSourceInterface* source = local_video_track_->GetSource();
  if (source)
    provider_->SetCaptureDevice(ssrc, source->GetVideoCapturer());
  OnEnabledChanged();
}

LocalVideoTrackHandler::~LocalVideoTrackHandler() {
}

void LocalVideoTrackHandler::OnStateChanged() {
}

void LocalVideoTrackHandler::Stop() {
  provider_->SetCaptureDevice(ssrc(), NULL);
  provider_->SetVideoSend(ssrc(), false, NULL);
}

void LocalVideoTrackHandler::OnEnabledChanged() {
  const cricket::VideoOptions* options = NULL;
  VideoSourceInterface* source = local_video_track_->GetSource();
  if (local_video_track_->enabled() && source) {
    options = source->options();
  }
  provider_->SetVideoSend(ssrc(), local_video_track_->enabled(), options);
}

RemoteVideoTrackHandler::RemoteVideoTrackHandler(
    VideoTrackInterface* track,
    uint32 ssrc,
    VideoProviderInterface* provider)
    : TrackHandler(track, ssrc),
      remote_video_track_(track),
      provider_(provider) {
  OnEnabledChanged();
  provider_->SetVideoPlayout(ssrc, true,
                             remote_video_track_->GetSource()->FrameInput());
}

RemoteVideoTrackHandler::~RemoteVideoTrackHandler() {
}

void RemoteVideoTrackHandler::Stop() {
  // Since cricket::VideoRenderer is not reference counted
  // we need to remove the renderer before we are deleted.
  provider_->SetVideoPlayout(ssrc(), false, NULL);
}

void RemoteVideoTrackHandler::OnStateChanged() {
}

void RemoteVideoTrackHandler::OnEnabledChanged() {
}

MediaStreamHandler::MediaStreamHandler(MediaStreamInterface* stream,
                                       AudioProviderInterface* audio_provider,
                                       VideoProviderInterface* video_provider)
    : stream_(stream),
      audio_provider_(audio_provider),
      video_provider_(video_provider) {
}

MediaStreamHandler::~MediaStreamHandler() {
  for (TrackHandlers::iterator it = track_handlers_.begin();
       it != track_handlers_.end(); ++it) {
    delete *it;
  }
}

void MediaStreamHandler::RemoveTrack(MediaStreamTrackInterface* track) {
  for (TrackHandlers::iterator it = track_handlers_.begin();
       it != track_handlers_.end(); ++it) {
    if ((*it)->track() == track) {
      TrackHandler* track = *it;
      track->Stop();
      delete track;
      track_handlers_.erase(it);
      break;
    }
  }
}

TrackHandler* MediaStreamHandler::FindTrackHandler(
    MediaStreamTrackInterface* track) {
  TrackHandlers::iterator it = track_handlers_.begin();
  for (; it != track_handlers_.end(); ++it) {
    if ((*it)->track() == track) {
      return *it;
      break;
    }
  }
  return NULL;
}

MediaStreamInterface* MediaStreamHandler::stream() {
  return stream_.get();
}

void MediaStreamHandler::OnChanged() {
}

void MediaStreamHandler::Stop() {
  for (TrackHandlers::const_iterator it = track_handlers_.begin();
      it != track_handlers_.end(); ++it) {
    (*it)->Stop();
  }
}

LocalMediaStreamHandler::LocalMediaStreamHandler(
    MediaStreamInterface* stream,
    AudioProviderInterface* audio_provider,
    VideoProviderInterface* video_provider)
    : MediaStreamHandler(stream, audio_provider, video_provider) {
}

LocalMediaStreamHandler::~LocalMediaStreamHandler() {
}

void LocalMediaStreamHandler::AddAudioTrack(AudioTrackInterface* audio_track,
                                            uint32 ssrc) {
  ASSERT(!FindTrackHandler(audio_track));

  TrackHandler* handler(new LocalAudioTrackHandler(audio_track, ssrc,
                                                   audio_provider_));
  track_handlers_.push_back(handler);
}

void LocalMediaStreamHandler::AddVideoTrack(VideoTrackInterface* video_track,
                                            uint32 ssrc) {
  ASSERT(!FindTrackHandler(video_track));

  TrackHandler* handler(new LocalVideoTrackHandler(video_track, ssrc,
                                                   video_provider_));
  track_handlers_.push_back(handler);
}

RemoteMediaStreamHandler::RemoteMediaStreamHandler(
    MediaStreamInterface* stream,
    AudioProviderInterface* audio_provider,
    VideoProviderInterface* video_provider)
    : MediaStreamHandler(stream, audio_provider, video_provider) {
}

RemoteMediaStreamHandler::~RemoteMediaStreamHandler() {
}

void RemoteMediaStreamHandler::AddAudioTrack(AudioTrackInterface* audio_track,
                                             uint32 ssrc) {
  ASSERT(!FindTrackHandler(audio_track));
  TrackHandler* handler(
      new RemoteAudioTrackHandler(audio_track, ssrc, audio_provider_));
  track_handlers_.push_back(handler);
}

void RemoteMediaStreamHandler::AddVideoTrack(VideoTrackInterface* video_track,
                                             uint32 ssrc) {
  ASSERT(!FindTrackHandler(video_track));
  TrackHandler* handler(
      new RemoteVideoTrackHandler(video_track, ssrc, video_provider_));
  track_handlers_.push_back(handler);
}

MediaStreamHandlerContainer::MediaStreamHandlerContainer(
    AudioProviderInterface* audio_provider,
    VideoProviderInterface* video_provider)
    : audio_provider_(audio_provider),
      video_provider_(video_provider) {
}

MediaStreamHandlerContainer::~MediaStreamHandlerContainer() {
  ASSERT(remote_streams_handlers_.empty());
  ASSERT(local_streams_handlers_.empty());
}

void MediaStreamHandlerContainer::TearDown() {
  for (StreamHandlerList::iterator it = remote_streams_handlers_.begin();
       it != remote_streams_handlers_.end(); ++it) {
    (*it)->Stop();
    delete *it;
  }
  remote_streams_handlers_.clear();
  for (StreamHandlerList::iterator it = local_streams_handlers_.begin();
       it != local_streams_handlers_.end(); ++it) {
    (*it)->Stop();
    delete *it;
  }
  local_streams_handlers_.clear();
}

void MediaStreamHandlerContainer::RemoveRemoteStream(
    MediaStreamInterface* stream) {
  DeleteStreamHandler(&remote_streams_handlers_, stream);
}

void MediaStreamHandlerContainer::AddRemoteAudioTrack(
    MediaStreamInterface* stream,
    AudioTrackInterface* audio_track,
    uint32 ssrc) {
  MediaStreamHandler* handler = FindStreamHandler(remote_streams_handlers_,
                                                  stream);
  if (handler == NULL) {
    handler = CreateRemoteStreamHandler(stream);
  }
  handler->AddAudioTrack(audio_track, ssrc);
}

void MediaStreamHandlerContainer::AddRemoteVideoTrack(
    MediaStreamInterface* stream,
    VideoTrackInterface* video_track,
    uint32 ssrc) {
  MediaStreamHandler* handler = FindStreamHandler(remote_streams_handlers_,
                                                  stream);
  if (handler == NULL) {
    handler = CreateRemoteStreamHandler(stream);
  }
  handler->AddVideoTrack(video_track, ssrc);
}

void MediaStreamHandlerContainer::RemoveRemoteTrack(
    MediaStreamInterface* stream,
    MediaStreamTrackInterface* track) {
  MediaStreamHandler* handler = FindStreamHandler(remote_streams_handlers_,
                                                  stream);
  if (!VERIFY(handler != NULL)) {
    LOG(LS_WARNING) << "Local MediaStreamHandler for stream  with id "
                    << stream->label() << "doesnt't exist.";
    return;
  }
  handler->RemoveTrack(track);
}

void MediaStreamHandlerContainer::RemoveLocalStream(
    MediaStreamInterface* stream) {
  DeleteStreamHandler(&local_streams_handlers_, stream);
}

void MediaStreamHandlerContainer::AddLocalAudioTrack(
    MediaStreamInterface* stream,
    AudioTrackInterface* audio_track,
    uint32 ssrc) {
  MediaStreamHandler* handler = FindStreamHandler(local_streams_handlers_,
                                                  stream);
  if (handler == NULL) {
    handler = CreateLocalStreamHandler(stream);
  }
  handler->AddAudioTrack(audio_track, ssrc);
}

void MediaStreamHandlerContainer::AddLocalVideoTrack(
    MediaStreamInterface* stream,
    VideoTrackInterface* video_track,
    uint32 ssrc) {
  MediaStreamHandler* handler = FindStreamHandler(local_streams_handlers_,
                                                  stream);
  if (handler == NULL) {
    handler = CreateLocalStreamHandler(stream);
  }
  handler->AddVideoTrack(video_track, ssrc);
}

void MediaStreamHandlerContainer::RemoveLocalTrack(
    MediaStreamInterface* stream,
    MediaStreamTrackInterface* track) {
  MediaStreamHandler* handler = FindStreamHandler(local_streams_handlers_,
                                                  stream);
  if (!VERIFY(handler != NULL)) {
    LOG(LS_WARNING) << "Remote MediaStreamHandler for stream with id "
                    << stream->label() << "doesnt't exist.";
    return;
  }
  handler->RemoveTrack(track);
}

MediaStreamHandler* MediaStreamHandlerContainer::CreateRemoteStreamHandler(
    MediaStreamInterface* stream) {
  ASSERT(!FindStreamHandler(remote_streams_handlers_, stream));

  RemoteMediaStreamHandler* handler =
      new RemoteMediaStreamHandler(stream, audio_provider_, video_provider_);
  remote_streams_handlers_.push_back(handler);
  return handler;
}

MediaStreamHandler* MediaStreamHandlerContainer::CreateLocalStreamHandler(
    MediaStreamInterface* stream) {
  ASSERT(!FindStreamHandler(local_streams_handlers_, stream));

  LocalMediaStreamHandler* handler =
      new LocalMediaStreamHandler(stream, audio_provider_, video_provider_);
  local_streams_handlers_.push_back(handler);
  return handler;
}

MediaStreamHandler* MediaStreamHandlerContainer::FindStreamHandler(
    const StreamHandlerList& handlers,
    MediaStreamInterface* stream) {
  StreamHandlerList::const_iterator it = handlers.begin();
  for (; it != handlers.end(); ++it) {
    if ((*it)->stream() == stream) {
      return *it;
    }
  }
  return NULL;
}

void MediaStreamHandlerContainer::DeleteStreamHandler(
    StreamHandlerList* streamhandlers, MediaStreamInterface* stream) {
  StreamHandlerList::iterator it = streamhandlers->begin();
  for (; it != streamhandlers->end(); ++it) {
    if ((*it)->stream() == stream) {
      (*it)->Stop();
      delete *it;
      streamhandlers->erase(it);
      break;
    }
  }
}

}  // namespace webrtc
