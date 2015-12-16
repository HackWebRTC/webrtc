/*
 * libjingle
 * Copyright 2015 Google Inc.
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

#ifndef TALK_APP_WEBRTC_MEDIASTREAMOBSERVER_H_
#define TALK_APP_WEBRTC_MEDIASTREAMOBSERVER_H_

#include "talk/app/webrtc/mediastreaminterface.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/sigslot.h"

namespace webrtc {

// Helper class which will listen for changes to a stream and emit the
// corresponding signals.
class MediaStreamObserver : public ObserverInterface {
 public:
  explicit MediaStreamObserver(MediaStreamInterface* stream);
  ~MediaStreamObserver();

  const MediaStreamInterface* stream() const { return stream_; }

  void OnChanged() override;

  sigslot::signal2<AudioTrackInterface*, MediaStreamInterface*>
      SignalAudioTrackAdded;
  sigslot::signal2<AudioTrackInterface*, MediaStreamInterface*>
      SignalAudioTrackRemoved;
  sigslot::signal2<VideoTrackInterface*, MediaStreamInterface*>
      SignalVideoTrackAdded;
  sigslot::signal2<VideoTrackInterface*, MediaStreamInterface*>
      SignalVideoTrackRemoved;

 private:
  rtc::scoped_refptr<MediaStreamInterface> stream_;
  AudioTrackVector cached_audio_tracks_;
  VideoTrackVector cached_video_tracks_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAMOBSERVER_H_
