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

#include <string>

#include "talk/app/webrtc/audiotrack.h"
#include "talk/app/webrtc/mediastream.h"
#include "talk/app/webrtc/mediastreamsignaling.h"
#include "talk/app/webrtc/streamcollection.h"
#include "talk/app/webrtc/test/fakeconstraints.h"
#include "talk/app/webrtc/test/fakedatachannelprovider.h"
#include "talk/app/webrtc/videotrack.h"
#include "talk/base/gunit.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/stringutils.h"
#include "talk/base/thread.h"
#include "talk/media/base/fakemediaengine.h"
#include "talk/media/devices/fakedevicemanager.h"
#include "talk/p2p/base/constants.h"
#include "talk/p2p/base/sessiondescription.h"
#include "talk/session/media/channelmanager.h"

static const char kStreams[][8] = {"stream1", "stream2"};
static const char kAudioTracks[][32] = {"audiotrack0", "audiotrack1"};
static const char kVideoTracks[][32] = {"videotrack0", "videotrack1"};

using webrtc::AudioTrack;
using webrtc::AudioTrackInterface;
using webrtc::AudioTrackVector;
using webrtc::VideoTrack;
using webrtc::VideoTrackInterface;
using webrtc::VideoTrackVector;
using webrtc::DataChannelInterface;
using webrtc::FakeConstraints;
using webrtc::IceCandidateInterface;
using webrtc::MediaConstraintsInterface;
using webrtc::MediaStreamInterface;
using webrtc::MediaStreamTrackInterface;
using webrtc::SdpParseError;
using webrtc::SessionDescriptionInterface;
using webrtc::StreamCollection;
using webrtc::StreamCollectionInterface;

// Reference SDP with a MediaStream with label "stream1" and audio track with
// id "audio_1" and a video track with id "video_1;
static const char kSdpStringWithStream1[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "a=ssrc:1 cname:stream1\r\n"
    "a=ssrc:1 mslabel:stream1\r\n"
    "a=ssrc:1 label:audiotrack0\r\n"
    "m=video 1 RTP/AVPF 120\r\n"
    "a=mid:video\r\n"
    "a=rtpmap:120 VP8/90000\r\n"
    "a=ssrc:2 cname:stream1\r\n"
    "a=ssrc:2 mslabel:stream1\r\n"
    "a=ssrc:2 label:videotrack0\r\n";

// Reference SDP with two MediaStreams with label "stream1" and "stream2. Each
// MediaStreams have one audio track and one video track.
// This uses MSID.
static const char kSdpStringWith2Stream[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=msid-semantic: WMS stream1 stream2\r\n"
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "a=ssrc:1 cname:stream1\r\n"
    "a=ssrc:1 msid:stream1 audiotrack0\r\n"
    "a=ssrc:3 cname:stream2\r\n"
    "a=ssrc:3 msid:stream2 audiotrack1\r\n"
    "m=video 1 RTP/AVPF 120\r\n"
    "a=mid:video\r\n"
    "a=rtpmap:120 VP8/0\r\n"
    "a=ssrc:2 cname:stream1\r\n"
    "a=ssrc:2 msid:stream1 videotrack0\r\n"
    "a=ssrc:4 cname:stream2\r\n"
    "a=ssrc:4 msid:stream2 videotrack1\r\n";

// Reference SDP without MediaStreams. Msid is not supported.
static const char kSdpStringWithoutStreams[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "m=video 1 RTP/AVPF 120\r\n"
    "a=mid:video\r\n"
    "a=rtpmap:120 VP8/90000\r\n";

// Reference SDP without MediaStreams. Msid is supported.
static const char kSdpStringWithMsidWithoutStreams[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=msid-semantic: WMS\r\n"
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=rtpmap:103 ISAC/16000\r\n"
    "m=video 1 RTP/AVPF 120\r\n"
    "a=mid:video\r\n"
    "a=rtpmap:120 VP8/90000\r\n";

// Reference SDP without MediaStreams and audio only.
static const char kSdpStringWithoutStreamsAudioOnly[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=rtpmap:103 ISAC/16000\r\n";

static const char kSdpStringInit[] =
    "v=0\r\n"
    "o=- 0 0 IN IP4 127.0.0.1\r\n"
    "s=-\r\n"
    "t=0 0\r\n"
    "a=msid-semantic: WMS\r\n";

static const char kSdpStringAudio[] =
    "m=audio 1 RTP/AVPF 103\r\n"
    "a=mid:audio\r\n"
    "a=rtpmap:103 ISAC/16000\r\n";

static const char kSdpStringVideo[] =
    "m=video 1 RTP/AVPF 120\r\n"
    "a=mid:video\r\n"
    "a=rtpmap:120 VP8/90000\r\n";

static const char kSdpStringMs1Audio0[] =
    "a=ssrc:1 cname:stream1\r\n"
    "a=ssrc:1 msid:stream1 audiotrack0\r\n";

static const char kSdpStringMs1Video0[] =
    "a=ssrc:2 cname:stream1\r\n"
    "a=ssrc:2 msid:stream1 videotrack0\r\n";

static const char kSdpStringMs1Audio1[] =
    "a=ssrc:3 cname:stream1\r\n"
    "a=ssrc:3 msid:stream1 audiotrack1\r\n";

static const char kSdpStringMs1Video1[] =
    "a=ssrc:4 cname:stream1\r\n"
    "a=ssrc:4 msid:stream1 videotrack1\r\n";

// Verifies that |options| contain all tracks in |collection| and that
// the |options| has set the the has_audio and has_video flags correct.
static void VerifyMediaOptions(StreamCollectionInterface* collection,
                               const cricket::MediaSessionOptions& options) {
  if (!collection) {
    return;
  }

  size_t stream_index = 0;
  for (size_t i = 0; i < collection->count(); ++i) {
    MediaStreamInterface* stream = collection->at(i);
    AudioTrackVector audio_tracks = stream->GetAudioTracks();
    ASSERT_GE(options.streams.size(), stream_index + audio_tracks.size());
    for (size_t j = 0; j < audio_tracks.size(); ++j) {
      webrtc::AudioTrackInterface* audio = audio_tracks[j];
      EXPECT_EQ(options.streams[stream_index].sync_label, stream->label());
      EXPECT_EQ(options.streams[stream_index++].id, audio->id());
      EXPECT_TRUE(options.has_audio);
    }
    VideoTrackVector video_tracks = stream->GetVideoTracks();
    ASSERT_GE(options.streams.size(), stream_index + video_tracks.size());
    for (size_t j = 0; j < video_tracks.size(); ++j) {
      webrtc::VideoTrackInterface* video = video_tracks[j];
      EXPECT_EQ(options.streams[stream_index].sync_label, stream->label());
      EXPECT_EQ(options.streams[stream_index++].id, video->id());
      EXPECT_TRUE(options.has_video);
    }
  }
}

static bool CompareStreamCollections(StreamCollectionInterface* s1,
                                     StreamCollectionInterface* s2) {
  if (s1 == NULL || s2 == NULL || s1->count() != s2->count())
    return false;

  for (size_t i = 0; i != s1->count(); ++i) {
    if (s1->at(i)->label() != s2->at(i)->label())
      return false;
    webrtc::AudioTrackVector audio_tracks1 = s1->at(i)->GetAudioTracks();
    webrtc::AudioTrackVector audio_tracks2 = s2->at(i)->GetAudioTracks();
    webrtc::VideoTrackVector video_tracks1 = s1->at(i)->GetVideoTracks();
    webrtc::VideoTrackVector video_tracks2 = s2->at(i)->GetVideoTracks();

    if (audio_tracks1.size() != audio_tracks2.size())
      return false;
    for (size_t j = 0; j != audio_tracks1.size(); ++j) {
       if (audio_tracks1[j]->id() != audio_tracks2[j]->id())
         return false;
    }
    if (video_tracks1.size() != video_tracks2.size())
      return false;
    for (size_t j = 0; j != video_tracks1.size(); ++j) {
      if (video_tracks1[j]->id() != video_tracks2[j]->id())
        return false;
    }
  }
  return true;
}

class MockSignalingObserver : public webrtc::MediaStreamSignalingObserver {
 public:
  MockSignalingObserver()
      : remote_media_streams_(StreamCollection::Create()) {
  }

  virtual ~MockSignalingObserver() {
  }

  // New remote stream have been discovered.
  virtual void OnAddRemoteStream(MediaStreamInterface* remote_stream) {
    remote_media_streams_->AddStream(remote_stream);
  }

  // Remote stream is no longer available.
  virtual void OnRemoveRemoteStream(MediaStreamInterface* remote_stream) {
    remote_media_streams_->RemoveStream(remote_stream);
  }

  virtual void OnAddDataChannel(DataChannelInterface* data_channel) {
  }

  virtual void OnAddLocalAudioTrack(MediaStreamInterface* stream,
                                    AudioTrackInterface* audio_track,
                                    uint32 ssrc) {
    AddTrack(&local_audio_tracks_, stream, audio_track, ssrc);
  }

  virtual void OnAddLocalVideoTrack(MediaStreamInterface* stream,
                                    VideoTrackInterface* video_track,
                                    uint32 ssrc) {
    AddTrack(&local_video_tracks_, stream, video_track, ssrc);
  }

  virtual void OnRemoveLocalAudioTrack(MediaStreamInterface* stream,
                                       AudioTrackInterface* audio_track) {
    RemoveTrack(&local_audio_tracks_, stream, audio_track);
  }

  virtual void OnRemoveLocalVideoTrack(MediaStreamInterface* stream,
                                       VideoTrackInterface* video_track) {
    RemoveTrack(&local_video_tracks_, stream, video_track);
  }

  virtual void OnAddRemoteAudioTrack(MediaStreamInterface* stream,
                                     AudioTrackInterface* audio_track,
                                     uint32 ssrc) {
    AddTrack(&remote_audio_tracks_, stream, audio_track, ssrc);
  }

  virtual void OnAddRemoteVideoTrack(MediaStreamInterface* stream,
                                     VideoTrackInterface* video_track,
                                     uint32 ssrc) {
    AddTrack(&remote_video_tracks_, stream, video_track, ssrc);
  }

  virtual void OnRemoveRemoteAudioTrack(MediaStreamInterface* stream,
                                        AudioTrackInterface* audio_track) {
    RemoveTrack(&remote_audio_tracks_, stream, audio_track);
  }

  virtual void OnRemoveRemoteVideoTrack(MediaStreamInterface* stream,
                                        VideoTrackInterface* video_track) {
    RemoveTrack(&remote_video_tracks_, stream, video_track);
  }

  virtual void OnRemoveLocalStream(MediaStreamInterface* stream) {
  }

  MediaStreamInterface* RemoteStream(const std::string& label) {
    return remote_media_streams_->find(label);
  }

  StreamCollectionInterface* remote_streams() const {
    return remote_media_streams_;
  }

  size_t NumberOfRemoteAudioTracks() { return remote_audio_tracks_.size(); }

  void  VerifyRemoteAudioTrack(const std::string& stream_label,
                               const std::string& track_id,
                               uint32 ssrc) {
    VerifyTrack(remote_audio_tracks_, stream_label, track_id, ssrc);
  }

  size_t NumberOfRemoteVideoTracks() { return remote_video_tracks_.size(); }

  void  VerifyRemoteVideoTrack(const std::string& stream_label,
                               const std::string& track_id,
                               uint32 ssrc) {
    VerifyTrack(remote_video_tracks_, stream_label, track_id, ssrc);
  }

  size_t NumberOfLocalAudioTracks() { return local_audio_tracks_.size(); }
  void  VerifyLocalAudioTrack(const std::string& stream_label,
                              const std::string& track_id,
                              uint32 ssrc) {
    VerifyTrack(local_audio_tracks_, stream_label, track_id, ssrc);
  }

  size_t NumberOfLocalVideoTracks() { return local_video_tracks_.size(); }

  void  VerifyLocalVideoTrack(const std::string& stream_label,
                              const std::string& track_id,
                              uint32 ssrc) {
    VerifyTrack(local_video_tracks_, stream_label, track_id, ssrc);
  }

 private:
  struct TrackInfo {
    TrackInfo() {}
    TrackInfo(const std::string& stream_label, const std::string track_id,
              uint32 ssrc)
        : stream_label(stream_label),
          track_id(track_id),
          ssrc(ssrc) {
    }
    std::string stream_label;
    std::string track_id;
    uint32 ssrc;
  };
  typedef std::map<std::string, TrackInfo> TrackInfos;

  void AddTrack(TrackInfos* track_infos, MediaStreamInterface* stream,
                MediaStreamTrackInterface* track,
                uint32 ssrc) {
    (*track_infos)[track->id()] = TrackInfo(stream->label(), track->id(),
                                            ssrc);
  }

  void RemoveTrack(TrackInfos* track_infos, MediaStreamInterface* stream,
                   MediaStreamTrackInterface* track) {
    TrackInfos::iterator it = track_infos->find(track->id());
    ASSERT_TRUE(it != track_infos->end());
    ASSERT_EQ(it->second.stream_label, stream->label());
    track_infos->erase(it);
  }

  void VerifyTrack(const TrackInfos& track_infos,
                   const std::string& stream_label,
                   const std::string& track_id,
                   uint32 ssrc) {
    TrackInfos::const_iterator it = track_infos.find(track_id);
    ASSERT_TRUE(it != track_infos.end());
    EXPECT_EQ(stream_label, it->second.stream_label);
    EXPECT_EQ(ssrc, it->second.ssrc);
  }

  TrackInfos remote_audio_tracks_;
  TrackInfos remote_video_tracks_;
  TrackInfos local_audio_tracks_;
  TrackInfos local_video_tracks_;

  talk_base::scoped_refptr<StreamCollection> remote_media_streams_;
};

class MediaStreamSignalingForTest : public webrtc::MediaStreamSignaling {
 public:
  MediaStreamSignalingForTest(MockSignalingObserver* observer,
                              cricket::ChannelManager* channel_manager)
      : webrtc::MediaStreamSignaling(talk_base::Thread::Current(), observer,
                                     channel_manager) {
  };

  using webrtc::MediaStreamSignaling::GetOptionsForOffer;
  using webrtc::MediaStreamSignaling::GetOptionsForAnswer;
  using webrtc::MediaStreamSignaling::OnRemoteDescriptionChanged;
  using webrtc::MediaStreamSignaling::remote_streams;
};

class MediaStreamSignalingTest: public testing::Test {
 protected:
  virtual void SetUp() {
    observer_.reset(new MockSignalingObserver());
    channel_manager_.reset(
        new cricket::ChannelManager(new cricket::FakeMediaEngine(),
                                    new cricket::FakeDeviceManager(),
                                    talk_base::Thread::Current()));
    signaling_.reset(new MediaStreamSignalingForTest(observer_.get(),
                                                     channel_manager_.get()));
  }

  // Create a collection of streams.
  // CreateStreamCollection(1) creates a collection that
  // correspond to kSdpString1.
  // CreateStreamCollection(2) correspond to kSdpString2.
  talk_base::scoped_refptr<StreamCollection>
  CreateStreamCollection(int number_of_streams) {
    talk_base::scoped_refptr<StreamCollection> local_collection(
        StreamCollection::Create());

    for (int i = 0; i < number_of_streams; ++i) {
      talk_base::scoped_refptr<webrtc::MediaStreamInterface> stream(
          webrtc::MediaStream::Create(kStreams[i]));

      // Add a local audio track.
      talk_base::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
          webrtc::AudioTrack::Create(kAudioTracks[i], NULL));
      stream->AddTrack(audio_track);

      // Add a local video track.
      talk_base::scoped_refptr<webrtc::VideoTrackInterface> video_track(
          webrtc::VideoTrack::Create(kVideoTracks[i], NULL));
      stream->AddTrack(video_track);

      local_collection->AddStream(stream);
    }
    return local_collection;
  }

  // This functions Creates a MediaStream with label kStreams[0] and
  // |number_of_audio_tracks| and |number_of_video_tracks| tracks and the
  // corresponding SessionDescriptionInterface. The SessionDescriptionInterface
  // is returned in |desc| and the MediaStream is stored in
  // |reference_collection_|
  void CreateSessionDescriptionAndReference(
      size_t number_of_audio_tracks,
      size_t number_of_video_tracks,
      SessionDescriptionInterface** desc) {
    ASSERT_TRUE(desc != NULL);
    ASSERT_LE(number_of_audio_tracks, 2u);
    ASSERT_LE(number_of_video_tracks, 2u);

    reference_collection_ = StreamCollection::Create();
    std::string sdp_ms1 = std::string(kSdpStringInit);

    std::string mediastream_label = kStreams[0];

    talk_base::scoped_refptr<webrtc::MediaStreamInterface> stream(
            webrtc::MediaStream::Create(mediastream_label));
    reference_collection_->AddStream(stream);

    if (number_of_audio_tracks > 0) {
      sdp_ms1 += std::string(kSdpStringAudio);
      sdp_ms1 += std::string(kSdpStringMs1Audio0);
      AddAudioTrack(kAudioTracks[0], stream);
    }
    if (number_of_audio_tracks > 1) {
      sdp_ms1 += kSdpStringMs1Audio1;
      AddAudioTrack(kAudioTracks[1], stream);
    }

    if (number_of_video_tracks > 0) {
      sdp_ms1 += std::string(kSdpStringVideo);
      sdp_ms1 += std::string(kSdpStringMs1Video0);
      AddVideoTrack(kVideoTracks[0], stream);
    }
    if (number_of_video_tracks > 1) {
      sdp_ms1 += kSdpStringMs1Video1;
      AddVideoTrack(kVideoTracks[1], stream);
    }

    *desc = webrtc::CreateSessionDescription(
        SessionDescriptionInterface::kOffer, sdp_ms1, NULL);
  }

  void AddAudioTrack(const std::string& track_id,
                     MediaStreamInterface* stream) {
    talk_base::scoped_refptr<webrtc::AudioTrackInterface> audio_track(
        webrtc::AudioTrack::Create(track_id, NULL));
    ASSERT_TRUE(stream->AddTrack(audio_track));
  }

  void AddVideoTrack(const std::string& track_id,
                     MediaStreamInterface* stream) {
    talk_base::scoped_refptr<webrtc::VideoTrackInterface> video_track(
        webrtc::VideoTrack::Create(track_id, NULL));
    ASSERT_TRUE(stream->AddTrack(video_track));
  }

  // ChannelManager is used by VideoSource, so it should be released after all
  // the video tracks. Put it as the first private variable should ensure that.
  talk_base::scoped_ptr<cricket::ChannelManager> channel_manager_;
  talk_base::scoped_refptr<StreamCollection> reference_collection_;
  talk_base::scoped_ptr<MockSignalingObserver> observer_;
  talk_base::scoped_ptr<MediaStreamSignalingForTest> signaling_;
};

// Test that a MediaSessionOptions is created for an offer if
// kOfferToReceiveAudio and kOfferToReceiveVideo constraints are set but no
// MediaStreams are sent.
TEST_F(MediaStreamSignalingTest, GetMediaSessionOptionsForOfferWithAudioVideo) {
  FakeConstraints constraints;
  constraints.SetMandatoryReceiveAudio(true);
  constraints.SetMandatoryReceiveVideo(true);
  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(&constraints, &options));
  EXPECT_TRUE(options.has_audio);
  EXPECT_TRUE(options.has_video);
  EXPECT_TRUE(options.bundle_enabled);
}

// Test that a correct MediaSessionOptions is created for an offer if
// kOfferToReceiveAudio constraints is set but no MediaStreams are sent.
TEST_F(MediaStreamSignalingTest, GetMediaSessionOptionsForOfferWithAudio) {
  FakeConstraints constraints;
  constraints.SetMandatoryReceiveAudio(true);
  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(&constraints, &options));
  EXPECT_TRUE(options.has_audio);
  EXPECT_FALSE(options.has_video);
  EXPECT_TRUE(options.bundle_enabled);
}

// Test that a correct MediaSessionOptions is created for an offer if
// no constraints or MediaStreams are sent.
TEST_F(MediaStreamSignalingTest, GetDefaultMediaSessionOptionsForOffer) {
  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(NULL, &options));
  EXPECT_TRUE(options.has_audio);
  EXPECT_FALSE(options.has_video);
  EXPECT_TRUE(options.bundle_enabled);
}

// Test that a correct MediaSessionOptions is created for an offer if
// kOfferToReceiveVideo constraints is set but no MediaStreams are sent.
TEST_F(MediaStreamSignalingTest, GetMediaSessionOptionsForOfferWithVideo) {
  FakeConstraints constraints;
  constraints.SetMandatoryReceiveAudio(false);
  constraints.SetMandatoryReceiveVideo(true);
  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(&constraints, &options));
  EXPECT_FALSE(options.has_audio);
  EXPECT_TRUE(options.has_video);
  EXPECT_TRUE(options.bundle_enabled);
}

// Test that a correct MediaSessionOptions is created for an offer if
// kUseRtpMux constraints is set to false.
TEST_F(MediaStreamSignalingTest,
       GetMediaSessionOptionsForOfferWithBundleDisabled) {
  FakeConstraints constraints;
  constraints.SetMandatoryReceiveAudio(true);
  constraints.SetMandatoryReceiveVideo(true);
  constraints.SetMandatoryUseRtpMux(false);
  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(&constraints, &options));
  EXPECT_TRUE(options.has_audio);
  EXPECT_TRUE(options.has_video);
  EXPECT_FALSE(options.bundle_enabled);
}

// Test that a correct MediaSessionOptions is created to restart ice if
// kIceRestart constraints is set. It also tests that subsequent
// MediaSessionOptions don't have |transport_options.ice_restart| set.
TEST_F(MediaStreamSignalingTest,
       GetMediaSessionOptionsForOfferWithIceRestart) {
  FakeConstraints constraints;
  constraints.SetMandatoryIceRestart(true);
  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(&constraints, &options));
  EXPECT_TRUE(options.transport_options.ice_restart);

  EXPECT_TRUE(signaling_->GetOptionsForOffer(NULL, &options));
  EXPECT_FALSE(options.transport_options.ice_restart);
}

// Test that GetMediaSessionOptionsForOffer and GetOptionsForAnswer work as
// expected if unknown constraints are used.
TEST_F(MediaStreamSignalingTest, GetMediaSessionOptionsWithBadConstraints) {
  FakeConstraints mandatory;
  mandatory.AddMandatory("bad_key", "bad_value");
  cricket::MediaSessionOptions options;
  EXPECT_FALSE(signaling_->GetOptionsForOffer(&mandatory, &options));
  EXPECT_FALSE(signaling_->GetOptionsForAnswer(&mandatory, &options));

  FakeConstraints optional;
  optional.AddOptional("bad_key", "bad_value");
  EXPECT_TRUE(signaling_->GetOptionsForOffer(&optional, &options));
  EXPECT_TRUE(signaling_->GetOptionsForAnswer(&optional, &options));
}

// Test that a correct MediaSessionOptions are created for an offer if
// a MediaStream is sent and later updated with a new track.
// MediaConstraints are not used.
TEST_F(MediaStreamSignalingTest, AddTrackToLocalMediaStream) {
  talk_base::scoped_refptr<StreamCollection> local_streams(
      CreateStreamCollection(1));
  MediaStreamInterface* local_stream = local_streams->at(0);
  EXPECT_TRUE(signaling_->AddLocalStream(local_stream));
  cricket::MediaSessionOptions options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(NULL, &options));
  VerifyMediaOptions(local_streams, options);

  cricket::MediaSessionOptions updated_options;
  local_stream->AddTrack(AudioTrack::Create(kAudioTracks[1], NULL));
  EXPECT_TRUE(signaling_->GetOptionsForOffer(NULL, &options));
  VerifyMediaOptions(local_streams, options);
}

// Test that the MediaConstraints in an answer don't affect if audio and video
// is offered in an offer but that if kOfferToReceiveAudio or
// kOfferToReceiveVideo constraints are true in an offer, the media type will be
// included in subsequent answers.
TEST_F(MediaStreamSignalingTest, MediaConstraintsInAnswer) {
  FakeConstraints answer_c;
  answer_c.SetMandatoryReceiveAudio(true);
  answer_c.SetMandatoryReceiveVideo(true);

  cricket::MediaSessionOptions answer_options;
  EXPECT_TRUE(signaling_->GetOptionsForAnswer(&answer_c, &answer_options));
  EXPECT_TRUE(answer_options.has_audio);
  EXPECT_TRUE(answer_options.has_video);

  FakeConstraints offer_c;
  offer_c.SetMandatoryReceiveAudio(false);
  offer_c.SetMandatoryReceiveVideo(false);

  cricket::MediaSessionOptions offer_options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(&offer_c, &offer_options));
  EXPECT_FALSE(offer_options.has_audio);
  EXPECT_FALSE(offer_options.has_video);

  FakeConstraints updated_offer_c;
  updated_offer_c.SetMandatoryReceiveAudio(true);
  updated_offer_c.SetMandatoryReceiveVideo(true);

  cricket::MediaSessionOptions updated_offer_options;
  EXPECT_TRUE(signaling_->GetOptionsForOffer(&updated_offer_c,
                                             &updated_offer_options));
  EXPECT_TRUE(updated_offer_options.has_audio);
  EXPECT_TRUE(updated_offer_options.has_video);

  // Since an offer has been created with both audio and video, subsequent
  // offers and answers should contain both audio and video.
  // Answers will only contain the media types that exist in the offer
  // regardless of the value of |updated_answer_options.has_audio| and
  // |updated_answer_options.has_video|.
  FakeConstraints updated_answer_c;
  answer_c.SetMandatoryReceiveAudio(false);
  answer_c.SetMandatoryReceiveVideo(false);

  cricket::MediaSessionOptions updated_answer_options;
  EXPECT_TRUE(signaling_->GetOptionsForAnswer(&updated_answer_c,
                                              &updated_answer_options));
  EXPECT_TRUE(updated_answer_options.has_audio);
  EXPECT_TRUE(updated_answer_options.has_video);

  EXPECT_TRUE(signaling_->GetOptionsForOffer(NULL,
                                             &updated_offer_options));
  EXPECT_TRUE(updated_offer_options.has_audio);
  EXPECT_TRUE(updated_offer_options.has_video);
}

// This test verifies that the remote MediaStreams corresponding to a received
// SDP string is created. In this test the two separate MediaStreams are
// signaled.
TEST_F(MediaStreamSignalingTest, UpdateRemoteStreams) {
  talk_base::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithStream1, NULL));
  EXPECT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());

  talk_base::scoped_refptr<StreamCollection> reference(
      CreateStreamCollection(1));
  EXPECT_TRUE(CompareStreamCollections(signaling_->remote_streams(),
                                       reference.get()));
  EXPECT_TRUE(CompareStreamCollections(observer_->remote_streams(),
                                       reference.get()));
  EXPECT_EQ(1u, observer_->NumberOfRemoteAudioTracks());
  observer_->VerifyRemoteAudioTrack(kStreams[0], kAudioTracks[0], 1);
  EXPECT_EQ(1u, observer_->NumberOfRemoteVideoTracks());
  observer_->VerifyRemoteVideoTrack(kStreams[0], kVideoTracks[0], 2);
  ASSERT_EQ(1u, observer_->remote_streams()->count());
  MediaStreamInterface* remote_stream =  observer_->remote_streams()->at(0);
  EXPECT_TRUE(remote_stream->GetVideoTracks()[0]->GetSource() != NULL);

  // Create a session description based on another SDP with another
  // MediaStream.
  talk_base::scoped_ptr<SessionDescriptionInterface> update_desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWith2Stream, NULL));
  EXPECT_TRUE(update_desc != NULL);
  signaling_->OnRemoteDescriptionChanged(update_desc.get());

  talk_base::scoped_refptr<StreamCollection> reference2(
      CreateStreamCollection(2));
  EXPECT_TRUE(CompareStreamCollections(signaling_->remote_streams(),
                                       reference2.get()));
  EXPECT_TRUE(CompareStreamCollections(observer_->remote_streams(),
                                       reference2.get()));

  EXPECT_EQ(2u, observer_->NumberOfRemoteAudioTracks());
  observer_->VerifyRemoteAudioTrack(kStreams[0], kAudioTracks[0], 1);
  observer_->VerifyRemoteAudioTrack(kStreams[1], kAudioTracks[1], 3);
  EXPECT_EQ(2u, observer_->NumberOfRemoteVideoTracks());
  observer_->VerifyRemoteVideoTrack(kStreams[0], kVideoTracks[0], 2);
  observer_->VerifyRemoteVideoTrack(kStreams[1], kVideoTracks[1], 4);
}

// This test verifies that the remote MediaStreams corresponding to a received
// SDP string is created. In this test the same remote MediaStream is signaled
// but MediaStream tracks are added and removed.
TEST_F(MediaStreamSignalingTest, AddRemoveTrackFromExistingRemoteMediaStream) {
  talk_base::scoped_ptr<SessionDescriptionInterface> desc_ms1;
  CreateSessionDescriptionAndReference(1, 1, desc_ms1.use());
  signaling_->OnRemoteDescriptionChanged(desc_ms1.get());
  EXPECT_TRUE(CompareStreamCollections(signaling_->remote_streams(),
                                       reference_collection_));

  // Add extra audio and video tracks to the same MediaStream.
  talk_base::scoped_ptr<SessionDescriptionInterface> desc_ms1_two_tracks;
  CreateSessionDescriptionAndReference(2, 2, desc_ms1_two_tracks.use());
  signaling_->OnRemoteDescriptionChanged(desc_ms1_two_tracks.get());
  EXPECT_TRUE(CompareStreamCollections(signaling_->remote_streams(),
                                       reference_collection_));
  EXPECT_TRUE(CompareStreamCollections(observer_->remote_streams(),
                                       reference_collection_));

  // Remove the extra audio and video tracks again.
  talk_base::scoped_ptr<SessionDescriptionInterface> desc_ms2;
  CreateSessionDescriptionAndReference(1, 1, desc_ms2.use());
  signaling_->OnRemoteDescriptionChanged(desc_ms2.get());
  EXPECT_TRUE(CompareStreamCollections(signaling_->remote_streams(),
                                       reference_collection_));
  EXPECT_TRUE(CompareStreamCollections(observer_->remote_streams(),
                                       reference_collection_));
}

// This test that remote tracks are ended if a
// local session description is set that rejects the media content type.
TEST_F(MediaStreamSignalingTest, RejectMediaContent) {
  talk_base::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithStream1, NULL));
  EXPECT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());

  ASSERT_EQ(1u, observer_->remote_streams()->count());
  MediaStreamInterface* remote_stream =  observer_->remote_streams()->at(0);
  ASSERT_EQ(1u, remote_stream->GetVideoTracks().size());
  ASSERT_EQ(1u, remote_stream->GetAudioTracks().size());

  talk_base::scoped_refptr<webrtc::VideoTrackInterface> remote_video =
      remote_stream->GetVideoTracks()[0];
  EXPECT_EQ(webrtc::MediaStreamTrackInterface::kLive, remote_video->state());
  talk_base::scoped_refptr<webrtc::AudioTrackInterface> remote_audio =
      remote_stream->GetAudioTracks()[0];
  EXPECT_EQ(webrtc::MediaStreamTrackInterface::kLive, remote_audio->state());

  cricket::ContentInfo* video_info =
      desc->description()->GetContentByName("video");
  ASSERT_TRUE(video_info != NULL);
  video_info->rejected = true;
  signaling_->OnLocalDescriptionChanged(desc.get());
  EXPECT_EQ(webrtc::MediaStreamTrackInterface::kEnded, remote_video->state());
  EXPECT_EQ(webrtc::MediaStreamTrackInterface::kLive, remote_audio->state());

  cricket::ContentInfo* audio_info =
      desc->description()->GetContentByName("audio");
  ASSERT_TRUE(audio_info != NULL);
  audio_info->rejected = true;
  signaling_->OnLocalDescriptionChanged(desc.get());
  EXPECT_EQ(webrtc::MediaStreamTrackInterface::kEnded, remote_audio->state());
}

// This test that it won't crash if the remote track as been removed outside
// of MediaStreamSignaling and then MediaStreamSignaling tries to reject
// this track.
TEST_F(MediaStreamSignalingTest, RemoveTrackThenRejectMediaContent) {
  talk_base::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithStream1, NULL));
  EXPECT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());

  MediaStreamInterface* remote_stream =  observer_->remote_streams()->at(0);
  remote_stream->RemoveTrack(remote_stream->GetVideoTracks()[0]);
  remote_stream->RemoveTrack(remote_stream->GetAudioTracks()[0]);

  cricket::ContentInfo* video_info =
      desc->description()->GetContentByName("video");
  video_info->rejected = true;
  signaling_->OnLocalDescriptionChanged(desc.get());

  cricket::ContentInfo* audio_info =
      desc->description()->GetContentByName("audio");
  audio_info->rejected = true;
  signaling_->OnLocalDescriptionChanged(desc.get());

  // No crash is a pass.
}

// This tests that a default MediaStream is created if a remote session
// description doesn't contain any streams and no MSID support.
// It also tests that the default stream is updated if a video m-line is added
// in a subsequent session description.
TEST_F(MediaStreamSignalingTest, SdpWithoutMsidCreatesDefaultStream) {
  talk_base::scoped_ptr<SessionDescriptionInterface> desc_audio_only(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithoutStreamsAudioOnly,
                                       NULL));
  ASSERT_TRUE(desc_audio_only != NULL);
  signaling_->OnRemoteDescriptionChanged(desc_audio_only.get());

  EXPECT_EQ(1u, signaling_->remote_streams()->count());
  ASSERT_EQ(1u, observer_->remote_streams()->count());
  MediaStreamInterface* remote_stream = observer_->remote_streams()->at(0);

  EXPECT_EQ(1u, remote_stream->GetAudioTracks().size());
  EXPECT_EQ(0u, remote_stream->GetVideoTracks().size());
  EXPECT_EQ("default", remote_stream->label());

  talk_base::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithoutStreams, NULL));
  ASSERT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());
  EXPECT_EQ(1u, signaling_->remote_streams()->count());
  ASSERT_EQ(1u, remote_stream->GetAudioTracks().size());
  EXPECT_EQ("defaulta0", remote_stream->GetAudioTracks()[0]->id());
  ASSERT_EQ(1u, remote_stream->GetVideoTracks().size());
  EXPECT_EQ("defaultv0", remote_stream->GetVideoTracks()[0]->id());
  observer_->VerifyRemoteAudioTrack("default", "defaulta0", 0);
  observer_->VerifyRemoteVideoTrack("default", "defaultv0", 0);
}

// This tests that it won't crash when MediaStreamSignaling tries to remove
//  a remote track that as already been removed from the mediastream.
TEST_F(MediaStreamSignalingTest, RemoveAlreadyGoneRemoteStream) {
  talk_base::scoped_ptr<SessionDescriptionInterface> desc_audio_only(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithoutStreams,
                                       NULL));
  ASSERT_TRUE(desc_audio_only != NULL);
  signaling_->OnRemoteDescriptionChanged(desc_audio_only.get());
  MediaStreamInterface* remote_stream = observer_->remote_streams()->at(0);
  remote_stream->RemoveTrack(remote_stream->GetAudioTracks()[0]);
  remote_stream->RemoveTrack(remote_stream->GetVideoTracks()[0]);

  talk_base::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithoutStreams, NULL));
  ASSERT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());

  // No crash is a pass.
}

// This tests that a default MediaStream is created if the remote session
// description doesn't contain any streams and don't contain an indication if
// MSID is supported.
TEST_F(MediaStreamSignalingTest,
       SdpWithoutMsidAndStreamsCreatesDefaultStream) {
  talk_base::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithoutStreams,
                                       NULL));
  ASSERT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());

  ASSERT_EQ(1u, observer_->remote_streams()->count());
  MediaStreamInterface* remote_stream = observer_->remote_streams()->at(0);
  EXPECT_EQ(1u, remote_stream->GetAudioTracks().size());
  EXPECT_EQ(1u, remote_stream->GetVideoTracks().size());
}

// This tests that a default MediaStream is not created if the remote session
// description doesn't contain any streams but does support MSID.
TEST_F(MediaStreamSignalingTest, SdpWitMsidDontCreatesDefaultStream) {
  talk_base::scoped_ptr<SessionDescriptionInterface> desc_msid_without_streams(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithMsidWithoutStreams,
                                       NULL));
  signaling_->OnRemoteDescriptionChanged(desc_msid_without_streams.get());
  EXPECT_EQ(0u, observer_->remote_streams()->count());
}

// This test that a default MediaStream is not created if a remote session
// description is updated to not have any MediaStreams.
TEST_F(MediaStreamSignalingTest, VerifyDefaultStreamIsNotCreated) {
  talk_base::scoped_ptr<SessionDescriptionInterface> desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithStream1,
                                       NULL));
  ASSERT_TRUE(desc != NULL);
  signaling_->OnRemoteDescriptionChanged(desc.get());
  talk_base::scoped_refptr<StreamCollection> reference(
      CreateStreamCollection(1));
  EXPECT_TRUE(CompareStreamCollections(observer_->remote_streams(),
                                       reference.get()));

  talk_base::scoped_ptr<SessionDescriptionInterface> desc_without_streams(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       kSdpStringWithoutStreams,
                                       NULL));
  signaling_->OnRemoteDescriptionChanged(desc_without_streams.get());
  EXPECT_EQ(0u, observer_->remote_streams()->count());
}

// This test that the correct MediaStreamSignalingObserver methods are called
// when MediaStreamSignaling::OnLocalDescriptionChanged is called with an
// updated local session description.
TEST_F(MediaStreamSignalingTest, LocalDescriptionChanged) {
  talk_base::scoped_ptr<SessionDescriptionInterface> desc_1;
  CreateSessionDescriptionAndReference(2, 2, desc_1.use());

  signaling_->AddLocalStream(reference_collection_->at(0));
  signaling_->OnLocalDescriptionChanged(desc_1.get());
  EXPECT_EQ(2u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(2u, observer_->NumberOfLocalVideoTracks());
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[0], 1);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[0], 2);
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[1], 3);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[1], 4);

  // Remove an audio and video track.
  talk_base::scoped_ptr<SessionDescriptionInterface> desc_2;
  CreateSessionDescriptionAndReference(1, 1, desc_2.use());
  signaling_->OnLocalDescriptionChanged(desc_2.get());
  EXPECT_EQ(1u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(1u, observer_->NumberOfLocalVideoTracks());
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[0], 1);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[0], 2);
}

// This test that the correct MediaStreamSignalingObserver methods are called
// when MediaStreamSignaling::AddLocalStream is called after
// MediaStreamSignaling::OnLocalDescriptionChanged is called.
TEST_F(MediaStreamSignalingTest, AddLocalStreamAfterLocalDescriptionChanged) {
  talk_base::scoped_ptr<SessionDescriptionInterface> desc_1;
  CreateSessionDescriptionAndReference(2, 2, desc_1.use());

  signaling_->OnLocalDescriptionChanged(desc_1.get());
  EXPECT_EQ(0u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(0u, observer_->NumberOfLocalVideoTracks());

  signaling_->AddLocalStream(reference_collection_->at(0));
  EXPECT_EQ(2u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(2u, observer_->NumberOfLocalVideoTracks());
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[0], 1);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[0], 2);
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[1], 3);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[1], 4);
}

// This test that the correct MediaStreamSignalingObserver methods are called
// if the ssrc on a local track is changed when
// MediaStreamSignaling::OnLocalDescriptionChanged is called.
TEST_F(MediaStreamSignalingTest, ChangeSsrcOnTrackInLocalSessionDescription) {
  talk_base::scoped_ptr<SessionDescriptionInterface> desc;
  CreateSessionDescriptionAndReference(1, 1, desc.use());

  signaling_->AddLocalStream(reference_collection_->at(0));
  signaling_->OnLocalDescriptionChanged(desc.get());
  EXPECT_EQ(1u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(1u, observer_->NumberOfLocalVideoTracks());
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[0], 1);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[0], 2);

  // Change the ssrc of the audio and video track.
  std::string sdp;
  desc->ToString(&sdp);
  std::string ssrc_org = "a=ssrc:1";
  std::string ssrc_to = "a=ssrc:97";
  talk_base::replace_substrs(ssrc_org.c_str(), ssrc_org.length(),
                             ssrc_to.c_str(), ssrc_to.length(),
                             &sdp);
  ssrc_org = "a=ssrc:2";
  ssrc_to = "a=ssrc:98";
  talk_base::replace_substrs(ssrc_org.c_str(), ssrc_org.length(),
                             ssrc_to.c_str(), ssrc_to.length(),
                             &sdp);
  talk_base::scoped_ptr<SessionDescriptionInterface> updated_desc(
      webrtc::CreateSessionDescription(SessionDescriptionInterface::kOffer,
                                       sdp, NULL));

  signaling_->OnLocalDescriptionChanged(updated_desc.get());
  EXPECT_EQ(1u, observer_->NumberOfLocalAudioTracks());
  EXPECT_EQ(1u, observer_->NumberOfLocalVideoTracks());
  observer_->VerifyLocalAudioTrack(kStreams[0], kAudioTracks[0], 97);
  observer_->VerifyLocalVideoTrack(kStreams[0], kVideoTracks[0], 98);
}

// Verifies that an even SCTP id is allocated for SSL_CLIENT and an odd id for
// SSL_SERVER.
TEST_F(MediaStreamSignalingTest, SctpIdAllocationBasedOnRole) {
  int id;
  ASSERT_TRUE(signaling_->AllocateSctpSid(talk_base::SSL_SERVER, &id));
  EXPECT_EQ(1, id);
  ASSERT_TRUE(signaling_->AllocateSctpSid(talk_base::SSL_CLIENT, &id));
  EXPECT_EQ(0, id);
  ASSERT_TRUE(signaling_->AllocateSctpSid(talk_base::SSL_SERVER, &id));
  EXPECT_EQ(3, id);
  ASSERT_TRUE(signaling_->AllocateSctpSid(talk_base::SSL_CLIENT, &id));
  EXPECT_EQ(2, id);
}

// Verifies that SCTP ids of existing DataChannels are not reused.
TEST_F(MediaStreamSignalingTest, SctpIdAllocationNoReuse) {
  talk_base::scoped_ptr<FakeDataChannelProvider> provider(
      new FakeDataChannelProvider());
  // Creates a DataChannel with id 1.
  webrtc::DataChannelInit config;
  config.id = 1;
  talk_base::scoped_refptr<webrtc::DataChannel> data_channel(
      webrtc::DataChannel::Create(
          provider.get(), cricket::DCT_SCTP, "a", &config));
  ASSERT_TRUE(data_channel.get() != NULL);
  ASSERT_TRUE(signaling_->AddDataChannel(data_channel.get()));

  int new_id;
  ASSERT_TRUE(signaling_->AllocateSctpSid(talk_base::SSL_SERVER, &new_id));
  EXPECT_NE(config.id, new_id);

  // Creates a DataChannel with id 0.
  config.id = 0;
  data_channel = webrtc::DataChannel::Create(
      provider.get(), cricket::DCT_SCTP, "b", &config);
  ASSERT_TRUE(data_channel.get() != NULL);
  ASSERT_TRUE(signaling_->AddDataChannel(data_channel.get()));
  ASSERT_TRUE(signaling_->AllocateSctpSid(talk_base::SSL_CLIENT, &new_id));
  EXPECT_NE(config.id, new_id);
}
