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

#ifndef TALK_APP_WEBRTC_TEST_FAKEMEDIASTREAMSIGNALING_H_
#define TALK_APP_WEBRTC_TEST_FAKEMEDIASTREAMSIGNALING_H_

#include "talk/app/webrtc/audiotrack.h"
#include "talk/app/webrtc/mediastreamsignaling.h"
#include "talk/app/webrtc/videotrack.h"

static const char kStream1[] = "stream1";
static const char kVideoTrack1[] = "video1";
static const char kAudioTrack1[] = "audio1";

static const char kStream2[] = "stream2";
static const char kVideoTrack2[] = "video2";
static const char kAudioTrack2[] = "audio2";

class FakeMediaStreamSignaling : public webrtc::MediaStreamSignaling,
                                 public webrtc::MediaStreamSignalingObserver {
 public:
  explicit FakeMediaStreamSignaling(cricket::ChannelManager* channel_manager) :
    webrtc::MediaStreamSignaling(rtc::Thread::Current(), this,
                                 channel_manager) {
  }

  void SendAudioVideoStream1() {
    ClearLocalStreams();
    AddLocalStream(CreateStream(kStream1, kAudioTrack1, kVideoTrack1));
  }

  void SendAudioVideoStream2() {
    ClearLocalStreams();
    AddLocalStream(CreateStream(kStream2, kAudioTrack2, kVideoTrack2));
  }

  void SendAudioVideoStream1And2() {
    ClearLocalStreams();
    AddLocalStream(CreateStream(kStream1, kAudioTrack1, kVideoTrack1));
    AddLocalStream(CreateStream(kStream2, kAudioTrack2, kVideoTrack2));
  }

  void SendNothing() {
    ClearLocalStreams();
  }

  void UseOptionsAudioOnly() {
    ClearLocalStreams();
    AddLocalStream(CreateStream(kStream2, kAudioTrack2, ""));
  }

  void UseOptionsVideoOnly() {
    ClearLocalStreams();
    AddLocalStream(CreateStream(kStream2, "", kVideoTrack2));
  }

  void ClearLocalStreams() {
    while (local_streams()->count() != 0) {
      RemoveLocalStream(local_streams()->at(0));
    }
  }

  // Implements MediaStreamSignalingObserver.
  virtual void OnAddRemoteStream(webrtc::MediaStreamInterface* stream) {}
  virtual void OnRemoveRemoteStream(webrtc::MediaStreamInterface* stream) {}
  virtual void OnAddDataChannel(webrtc::DataChannelInterface* data_channel) {}
  virtual void OnAddLocalAudioTrack(webrtc::MediaStreamInterface* stream,
                                    webrtc::AudioTrackInterface* audio_track,
                                    uint32_t ssrc) {}
  virtual void OnAddLocalVideoTrack(webrtc::MediaStreamInterface* stream,
                                    webrtc::VideoTrackInterface* video_track,
                                    uint32_t ssrc) {}
  virtual void OnAddRemoteAudioTrack(webrtc::MediaStreamInterface* stream,
                                     webrtc::AudioTrackInterface* audio_track,
                                     uint32_t ssrc) {}
  virtual void OnAddRemoteVideoTrack(webrtc::MediaStreamInterface* stream,
                                     webrtc::VideoTrackInterface* video_track,
                                     uint32_t ssrc) {}
  virtual void OnRemoveRemoteAudioTrack(
      webrtc::MediaStreamInterface* stream,
      webrtc::AudioTrackInterface* audio_track) {}
  virtual void OnRemoveRemoteVideoTrack(
      webrtc::MediaStreamInterface* stream,
      webrtc::VideoTrackInterface* video_track) {}
  virtual void OnRemoveLocalAudioTrack(webrtc::MediaStreamInterface* stream,
                                       webrtc::AudioTrackInterface* audio_track,
                                       uint32_t ssrc) {}
  virtual void OnRemoveLocalVideoTrack(
      webrtc::MediaStreamInterface* stream,
      webrtc::VideoTrackInterface* video_track) {}
  virtual void OnRemoveLocalStream(webrtc::MediaStreamInterface* stream) {}

 private:
  rtc::scoped_refptr<webrtc::MediaStreamInterface> CreateStream(
      const std::string& stream_label,
      const std::string& audio_track_id,
      const std::string& video_track_id) {
    rtc::scoped_refptr<webrtc::MediaStreamInterface> stream(
        webrtc::MediaStream::Create(stream_label));

    if (!audio_track_id.empty()) {
      rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
          webrtc::AudioTrack::Create(audio_track_id, NULL));
      stream->AddTrack(audio_track);
    }

    if (!video_track_id.empty()) {
      rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
          webrtc::VideoTrack::Create(video_track_id, NULL));
      stream->AddTrack(video_track);
    }
    return stream;
  }
};

#endif  // TALK_APP_WEBRTC_TEST_FAKEMEDIASTREAMSIGNALING_H_
