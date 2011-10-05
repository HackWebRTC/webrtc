/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#ifndef TALK_APP_WEBRTC_WEBRTCSESSION_H_
#define TALK_APP_WEBRTC_WEBRTCSESSION_H_

#include <string>
#include <vector>

#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/session.h"
#include "talk/session/phone/mediasession.h"
#include "talk/app/webrtc_dev/mediastreamprovider.h"
#include "talk/app/webrtc_dev/sessiondescriptionprovider.h"
#include "talk/app/webrtc_dev/webrtcsessionobserver.h"

namespace cricket {
class ChannelManager;
class Transport;
class VideoChannel;
class VoiceChannel;
}

namespace webrtc {
class MediaStream;
class PeerConnectionMessage;
class PeerConnectionSignaling;
class StreamCollection;

class WebRtcSession : public cricket::BaseSession,
                      public MediaProviderInterface,
                      public SessionDescriptionProvider {
 public:
  WebRtcSession(cricket::ChannelManager* channel_manager,
                talk_base::Thread* signaling_thread,
                talk_base::Thread* worker_thread,
                cricket::PortAllocator* port_allocator);
  ~WebRtcSession();

  bool Initialize();

  const cricket::VoiceChannel* voice_channel() const {
    return voice_channel_.get();
  }
  const cricket::VideoChannel* video_channel() const {
    return video_channel_.get();
  }

  // Generic error message callback from WebRtcSession.
  // TODO(mallinath) - It may be necessary to supply error code as well.
  sigslot::signal0<> SignalError;
  // This signal added for testing. Shouldn't be registered by other
  // objects.
  sigslot::signal2<WebRtcSession*,
                   cricket::Candidates&> SignalCandidatesReady;

  void ProcessSessionUpdate(const cricket::SessionDescription* local_desc,
                            const cricket::SessionDescription* remote_desc);

 private:
  // Implements MediaProviderInterface.
  virtual void SetCaptureDevice(uint32 ssrc, VideoCaptureModule* camera);
  virtual void SetLocalRenderer(uint32 ssrc,
                                cricket::VideoRenderer* renderer);
  virtual void SetRemoteRenderer(uint32 ssrc,
                                 cricket::VideoRenderer* renderer);

  //TODO mallinath: remove.
  void OnSignalUpdateSessionDescription(
      const cricket::SessionDescription* local_desc,
      const cricket::SessionDescription* remote_desc,
      const cricket::Candidates& remote_candidates);

  // Implements SessionDescriptionProvider
  virtual const cricket::SessionDescription* ProvideOffer(
      const cricket::MediaSessionOptions& options) {}
  virtual const cricket::SessionDescription* SetRemoteSessionDescription(
      const cricket::SessionDescription* remote_offer,
      const cricket::Candidates& remote_candidates) {}
  virtual const cricket::SessionDescription* ProvideAnswer(
      const cricket::MediaSessionOptions& options) {}
  virtual void NegotiationDone() {}


  // Transport related callbacks, override from cricket::BaseSession.
  virtual void OnTransportRequestSignaling(cricket::Transport* transport);
  virtual void OnTransportConnecting(cricket::Transport* transport);
  virtual void OnTransportWritable(cricket::Transport* transport);
  virtual void OnTransportCandidatesReady(
      cricket::Transport* transport,
      const cricket::Candidates& candidates);
  virtual void OnTransportChannelGone(cricket::Transport* transport);

  // Creates channels for voice and video.
  bool CreateChannels();
  virtual void OnMessage(talk_base::Message* msg);
  void InsertTransportCandidates(const cricket::Candidates& candidates);
  void Terminate();
  // Get candidate from the local candidates list by the name.
  bool CheckCandidate(const std::string& name);
  void SetRemoteCandidates(const cricket::Candidates& candidates);

  // Helper methods to get handle to the MediaContentDescription sources param.
  bool GetAudioSourceParamInfo(const cricket::SessionDescription* sdesc,
                               cricket::Sources* sources);
  bool GetVideoSourceParamInfo(const cricket::SessionDescription* sdesc,
                               cricket::Sources* sources);

  void ProcessLocalMediaChanges(const cricket::SessionDescription* sdesc);
  void ProcessRemoteMediaChanges(const cricket::SessionDescription* sdesc);

 private:
  WebRtcSessionObserver* observer_;
  talk_base::scoped_ptr<cricket::VoiceChannel> voice_channel_;
  talk_base::scoped_ptr<cricket::VideoChannel> video_channel_;
  cricket::ChannelManager* channel_manager_;
  cricket::Candidates local_candidates_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_WEBRTCSESSION_H_
