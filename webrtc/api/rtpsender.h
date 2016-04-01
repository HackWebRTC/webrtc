/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This file contains classes that implement RtpSenderInterface.
// An RtpSender associates a MediaStreamTrackInterface with an underlying
// transport (provided by AudioProviderInterface/VideoProviderInterface)

#ifndef WEBRTC_API_RTPSENDER_H_
#define WEBRTC_API_RTPSENDER_H_

#include <string>

#include "webrtc/api/mediastreamprovider.h"
#include "webrtc/api/rtpsenderinterface.h"
#include "webrtc/api/statscollector.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/media/base/audiosource.h"

namespace webrtc {

// LocalAudioSinkAdapter receives data callback as a sink to the local
// AudioTrack, and passes the data to the sink of AudioSource.
class LocalAudioSinkAdapter : public AudioTrackSinkInterface,
                              public cricket::AudioSource {
 public:
  LocalAudioSinkAdapter();
  virtual ~LocalAudioSinkAdapter();

 private:
  // AudioSinkInterface implementation.
  void OnData(const void* audio_data,
              int bits_per_sample,
              int sample_rate,
              size_t number_of_channels,
              size_t number_of_frames) override;

  // cricket::AudioSource implementation.
  void SetSink(cricket::AudioSource::Sink* sink) override;

  cricket::AudioSource::Sink* sink_;
  // Critical section protecting |sink_|.
  rtc::CriticalSection lock_;
};

class AudioRtpSender : public ObserverInterface,
                       public rtc::RefCountedObject<RtpSenderInterface> {
 public:
  // StatsCollector provided so that Add/RemoveLocalAudioTrack can be called
  // at the appropriate times.
  AudioRtpSender(AudioTrackInterface* track,
                 const std::string& stream_id,
                 AudioProviderInterface* provider,
                 StatsCollector* stats);

  // Randomly generates stream_id.
  AudioRtpSender(AudioTrackInterface* track,
                 AudioProviderInterface* provider,
                 StatsCollector* stats);

  // Randomly generates id and stream_id.
  AudioRtpSender(AudioProviderInterface* provider, StatsCollector* stats);

  virtual ~AudioRtpSender();

  // ObserverInterface implementation
  void OnChanged() override;

  // RtpSenderInterface implementation
  bool SetTrack(MediaStreamTrackInterface* track) override;
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_.get();
  }

  void SetSsrc(uint32_t ssrc) override;

  uint32_t ssrc() const override { return ssrc_; }

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_AUDIO;
  }

  std::string id() const override { return id_; }

  void set_stream_id(const std::string& stream_id) override {
    stream_id_ = stream_id;
  }
  std::string stream_id() const override { return stream_id_; }

  void Stop() override;

  RtpParameters GetParameters() const;
  bool SetParameters(const RtpParameters& parameters);

 private:
  // TODO(nisse): Since SSRC == 0 is technically valid, figure out
  // some other way to test if we have a valid SSRC.
  bool can_send_track() const { return track_ && ssrc_; }
  // Helper function to construct options for
  // AudioProviderInterface::SetAudioSend.
  void SetAudioSend();

  std::string id_;
  std::string stream_id_;
  AudioProviderInterface* provider_;
  StatsCollector* stats_;
  rtc::scoped_refptr<AudioTrackInterface> track_;
  uint32_t ssrc_ = 0;
  bool cached_track_enabled_ = false;
  bool stopped_ = false;

  // Used to pass the data callback from the |track_| to the other end of
  // cricket::AudioSource.
  rtc::scoped_ptr<LocalAudioSinkAdapter> sink_adapter_;
};

class VideoRtpSender : public ObserverInterface,
                       public rtc::RefCountedObject<RtpSenderInterface> {
 public:
  VideoRtpSender(VideoTrackInterface* track,
                 const std::string& stream_id,
                 VideoProviderInterface* provider);

  // Randomly generates stream_id.
  VideoRtpSender(VideoTrackInterface* track, VideoProviderInterface* provider);

  // Randomly generates id and stream_id.
  explicit VideoRtpSender(VideoProviderInterface* provider);

  virtual ~VideoRtpSender();

  // ObserverInterface implementation
  void OnChanged() override;

  // RtpSenderInterface implementation
  bool SetTrack(MediaStreamTrackInterface* track) override;
  rtc::scoped_refptr<MediaStreamTrackInterface> track() const override {
    return track_.get();
  }

  void SetSsrc(uint32_t ssrc) override;

  uint32_t ssrc() const override { return ssrc_; }

  cricket::MediaType media_type() const override {
    return cricket::MEDIA_TYPE_VIDEO;
  }

  std::string id() const override { return id_; }

  void set_stream_id(const std::string& stream_id) override {
    stream_id_ = stream_id;
  }
  std::string stream_id() const override { return stream_id_; }

  void Stop() override;

  RtpParameters GetParameters() const;
  bool SetParameters(const RtpParameters& parameters);

 private:
  bool can_send_track() const { return track_ && ssrc_; }
  // Helper function to construct options for
  // VideoProviderInterface::SetVideoSend.
  void SetVideoSend();

  std::string id_;
  std::string stream_id_;
  VideoProviderInterface* provider_;
  rtc::scoped_refptr<VideoTrackInterface> track_;
  uint32_t ssrc_ = 0;
  bool cached_track_enabled_ = false;
  bool stopped_ = false;
};

}  // namespace webrtc

#endif  // WEBRTC_API_RTPSENDER_H_
