/*
 * libjingle
 * Copyright 2004--2011, Google Inc.
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

#ifndef TALK_APP_WEBRTC_WEBRTCSESSIONCHANNEL_H_
#define TALK_APP_WEBRTC_WEBRTCSESSIONCHANNEL_H_

#include <string>
#include <vector>

#include "talk/app/webrtc/mediastream.h"
#include "talk/base/messagehandler.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/p2p/base/candidate.h"
#include "talk/p2p/base/sessiondescription.h"

namespace talk_base {
class Thread;
}

namespace cricket {
class BaseChannel;
class ChannelManager;
class BaseSession;
class SessionDescription;
class MediaContentDescription;
}

namespace webrtc {
// NOTE: Channels are responsible for creating the JSON message for media
// stream. This was done to accommodate additional signaling attributes which
// are currenly not available in part of cricket::SessionDescription.
// One example is StreamDirection which will be added as "sendonly"
// "recvonly" "sendrecv" and "inactive".
// Another reason to create session channels is to support uni-directional
// stream management and these channels apply content to cricket::BaseChannel
// not through cricket::BaseSession::SetState.
//
//    State transition at local and remote peer
//     (Local)                         (Remote)
//      INIT                             INIT
//        |                                |
//   SENTINITIATE (AddStream)       RECEIVEDINITIATE (OnAddStream)
//        |                                |
//  RECEIVEDACCEPT (StartSend)        SENTACCEPT (StartReceive)
//        |                                |
//     SENDING                         RECEIVING
//        |                                |
//     SENDRECV (OnAddStream,StartRecv) SENDRECV (AddStream, StartSend)
//
//
class WebRtcSessionChannel : public talk_base::MessageHandler,
                             public Observer {
 public:
  enum State {
    STATE_INIT,               // Channel Initialization state
    STATE_SENTINITIATE,       // After local AddStream (sendrecv)
    STATE_SENTACCEPT,         // Accepted incoming stream (recvonly)
    STATE_RECEIVEDACCEPT,     // Receives acceptance from remote (sendonly)
    STATE_RECEIVEDINITIATE,   // Initial stream request (onAddStream)
    STATE_SENDING,            // Starts sending media to remote
    STATE_RECEIVING,          // starts receiving media
    STATE_SENDRECV,           // Send and Recv from/to remote
    STATE_INVALID,            // Invalid state
  };

  enum StreamDirection {
    SD_SENDONLY,    // media stream is sendonly
    SD_RECVONLY,    // media stream is recvonly
    SD_SENDRECV,    // media stream is both sendrecv
    SD_INACTIVE,    // media stream is inactive
  };

  // From cricket::BaseSession
  enum Error {
    ERROR_NONE = 0,     // no error
    ERROR_CONTENT = 1,  // channel errors in SetLocalContent/SetRemoteContent
  };

  WebRtcSessionChannel(MediaStreamTrack* track,
                       cricket::ChannelManager* channel_manager,
                       talk_base::Thread* signaling_thread);
  virtual ~WebRtcSessionChannel();

  bool Initialize(cricket::BaseSession* session);
  void DestroyMediaChannel();
  void OnChanged();
  void set_enabled(bool enabled) {
    enabled_ = enabled;
  }
  bool enabled() {
    return enabled_;
  }

  // This will be called from WebRtcSession not from MediaStreamTrack
  bool EnableMediaChannel(bool enable);
  std::string name() {
    return transport_channel_name_;
  }
  void set_transport_channel_name(const std::string& name) {
    transport_channel_name_ = name;
  }

  MediaStreamTrack* media_stream_track() {
    return media_stream_track_;
  }
  void SendSignalingMessage(
      const std::vector<cricket::Candidate>& candidates);

  sigslot::signal2<WebRtcSessionChannel*,
                   const std::string&> SignalJSONMessageReady;
  sigslot::signal2<WebRtcSessionChannel*, Error> SignalSessionChannelError;
  void SetState(State state);
  bool ProcessRemoteMessage(cricket::SessionDescription* sdp);

  void set_local_description(cricket::SessionDescription* sdesc) {
    if (sdesc != local_description_) {
      delete local_description_;
      local_description_ = sdesc;
    }
  }

  void set_remote_description(cricket::SessionDescription* sdesc) {
    if (sdesc != remote_description_) {
      delete remote_description_;
      remote_description_ = sdesc;
    }
  }

 private:
  void OnMessage(talk_base::Message* message);
  void OnStateChange();
  // These two methods are used to set directly the media content description
  // On BaseChannel, rather than going through BaseSession::SetState
  // This will give us the flexibility when to send and receive the data
  // based on AddStream
  bool SetLocalMediaContent(const cricket::SessionDescription* sdp,
                            cricket::ContentAction action);
  bool SetRemoteMediaContent(const cricket::SessionDescription* sdp,
                             cricket::ContentAction action);
  cricket::SessionDescription* GetChannelMediaDesc();
  void SendSignalingMessage_s(
      const std::vector<cricket::Candidate>& candidates);
  // methods from BaseChannel
  const cricket::MediaContentDescription* GetFirstContent(
      const cricket::SessionDescription* sdesc,
      bool video);

  bool video_;
  std::string transport_channel_name_;
  bool enabled_;
  talk_base::scoped_ptr<cricket::BaseChannel> media_channel_;
  MediaStreamTrack* media_stream_track_;
  cricket::ChannelManager* channel_manager_;
  StreamDirection direction_;
  talk_base::Thread* signaling_thread_;
  State state_;
  const cricket::SessionDescription* local_description_;
  cricket::SessionDescription* remote_description_;
  DISALLOW_COPY_AND_ASSIGN(WebRtcSessionChannel);
};
}  // namspace webrtc

#endif  // TALK_APP_WEBRTC_WEBRTCSESSIONCHANNEL_H_
