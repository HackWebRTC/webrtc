/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#ifndef TALK_SESSION_PHONE_MEDIASESSIONCLIENT_H_
#define TALK_SESSION_PHONE_MEDIASESSIONCLIENT_H_

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include "talk/session/phone/call.h"
#include "talk/session/phone/channelmanager.h"
#include "talk/session/phone/cryptoparams.h"
#include "talk/base/sigslot.h"
#include "talk/base/sigslotrepeater.h"
#include "talk/base/messagequeue.h"
#include "talk/base/thread.h"
#include "talk/p2p/base/sessionmanager.h"
#include "talk/p2p/base/session.h"
#include "talk/p2p/base/sessionclient.h"
#include "talk/p2p/base/sessiondescription.h"

namespace cricket {

class Call;
class SessionDescription;
typedef std::vector<AudioCodec> AudioCodecs;
typedef std::vector<VideoCodec> VideoCodecs;

// SEC_ENABLED and SEC_REQUIRED should only be used if the session
// was negotiated over TLS, to protect the inline crypto material
// exchange.
// SEC_DISABLED: No crypto in outgoing offer and answer. Fail any
//               offer with crypto required.
// SEC_ENABLED: Crypto in outgoing offer and answer. Fail any offer
//              with unsupported required crypto. Crypto set but not
//              required in outgoing offer.
// SEC_REQUIRED: Crypto in outgoing offer and answer with
//               required='true'. Fail any offer with no or
//               unsupported crypto (implicit crypto required='true'
//               in the offer.)
enum SecureMediaPolicy {SEC_DISABLED, SEC_ENABLED, SEC_REQUIRED};

const int kAutoBandwidth = -1;

struct CallOptions {
  CallOptions() :
      is_video(false),
      is_muc(false),
      video_bandwidth(kAutoBandwidth) {
  }

  bool is_video;
  bool is_muc;
  // bps. -1 == auto.
  int video_bandwidth;
};

class MediaSessionClient: public SessionClient, public sigslot::has_slots<> {
 public:

  MediaSessionClient(const buzz::Jid& jid, SessionManager *manager);
  // Alternative constructor, allowing injection of media_engine
  // and device_manager.
  MediaSessionClient(const buzz::Jid& jid, SessionManager *manager,
      MediaEngine* media_engine, DeviceManager* device_manager);
  ~MediaSessionClient();

  const buzz::Jid &jid() const { return jid_; }
  SessionManager* session_manager() const { return session_manager_; }
  ChannelManager* channel_manager() const { return channel_manager_; }

  int GetCapabilities() { return channel_manager_->GetCapabilities(); }

  Call *CreateCall();
  void DestroyCall(Call *call);

  Call *GetFocus();
  void SetFocus(Call *call);

  void JoinCalls(Call *call_to_join, Call *call);

  bool GetAudioInputDevices(std::vector<std::string>* names) {
    return channel_manager_->GetAudioInputDevices(names);
  }
  bool GetAudioOutputDevices(std::vector<std::string>* names) {
    return channel_manager_->GetAudioOutputDevices(names);
  }
  bool GetVideoCaptureDevices(std::vector<std::string>* names) {
    return channel_manager_->GetVideoCaptureDevices(names);
  }

  bool SetAudioOptions(const std::string& in_name, const std::string& out_name,
                       int opts) {
    return channel_manager_->SetAudioOptions(in_name, out_name, opts);
  }
  bool SetOutputVolume(int level) {
    return channel_manager_->SetOutputVolume(level);
  }
  bool SetVideoOptions(const std::string& cam_device) {
    return channel_manager_->SetVideoOptions(cam_device);
  }

  sigslot::signal2<Call *, Call *> SignalFocus;
  sigslot::signal1<Call *> SignalCallCreate;
  sigslot::signal1<Call *> SignalCallDestroy;
  sigslot::repeater0<> SignalDevicesChange;

  SessionDescription* CreateOffer(const CallOptions& options);
  SessionDescription* CreateAnswer(const SessionDescription* offer,
                                   const CallOptions& options);

  SecureMediaPolicy secure() const { return secure_; }
  void set_secure(SecureMediaPolicy s) { secure_ = s; }

 private:
  void Construct();
  void OnSessionCreate(Session *session, bool received_initiate);
  void OnSessionState(BaseSession *session, BaseSession::State state);
  void OnSessionDestroy(Session *session);
  virtual bool ParseContent(SignalingProtocol protocol,
                            const buzz::XmlElement* elem,
                            const ContentDescription** content,
                            ParseError* error);
  virtual bool WriteContent(SignalingProtocol protocol,
                            const ContentDescription* content,
                            buzz::XmlElement** elem,
                            WriteError* error);
  Session *CreateSession(Call *call);

  buzz::Jid jid_;
  SessionManager* session_manager_;
  Call *focus_call_;
  ChannelManager *channel_manager_;
  std::map<uint32, Call *> calls_;
  std::map<std::string, Call *> session_map_;
  SecureMediaPolicy secure_;
  friend class Call;
};

enum MediaType {
  MEDIA_TYPE_AUDIO,
  MEDIA_TYPE_VIDEO
};

class MediaContentDescription : public ContentDescription {
 public:
  MediaContentDescription()
      : ssrc_(0),
        ssrc_set_(false),
        rtcp_mux_(false),
        bandwidth_(kAutoBandwidth),
        crypto_required_(false),
        rtp_header_extensions_set_(false) {
  }

  virtual MediaType type() const = 0;

  uint32 ssrc() const { return ssrc_; }
  bool ssrc_set() const { return ssrc_set_; }
  void set_ssrc(uint32 ssrc) {
    ssrc_ = ssrc;
    ssrc_set_ = true;
  }

  bool rtcp_mux() const { return rtcp_mux_; }
  void set_rtcp_mux(bool mux) { rtcp_mux_ = mux; }

  int bandwidth() const { return bandwidth_; }
  void set_bandwidth(int bandwidth) { bandwidth_ = bandwidth; }

  const std::vector<CryptoParams>& cryptos() const { return cryptos_; }
  void AddCrypto(const CryptoParams& params) {
    cryptos_.push_back(params);
  }
  bool crypto_required() const { return crypto_required_; }
  void set_crypto_required(bool crypto) {
    crypto_required_ = crypto;
  }

  const std::vector<RtpHeaderExtension>& rtp_header_extensions() const {
    return rtp_header_extensions_;
  }
  void AddRtpHeaderExtension(const RtpHeaderExtension& ext) {
    rtp_header_extensions_.push_back(ext);
    rtp_header_extensions_set_ = true;
  }
  void ClearRtpHeaderExtensions() {
    rtp_header_extensions_.clear();
    rtp_header_extensions_set_ = true;
  }
  // We can't always tell if an empty list of header extensions is
  // because the other side doesn't support them, or just isn't hooked up to
  // signal them. For now we assume an empty list means no signaling, but
  // provide the ClearRtpHeaderExtensions method to allow "no support" to be
  // clearly indicated (i.e. when derived from other information).
  bool rtp_header_extensions_set() const {
    return rtp_header_extensions_set_;
  }

 protected:
  uint32 ssrc_;
  bool ssrc_set_;
  bool rtcp_mux_;
  int bandwidth_;
  std::vector<CryptoParams> cryptos_;
  bool crypto_required_;
  std::vector<RtpHeaderExtension> rtp_header_extensions_;
  bool rtp_header_extensions_set_;
};

template <class C>
class MediaContentDescriptionImpl : public MediaContentDescription {
 public:
  struct PreferenceSort {
    bool operator()(C a, C b) { return a.preference > b.preference; }
  };

  const std::vector<C>& codecs() const { return codecs_; }
  void AddCodec(const C& codec) {
    codecs_.push_back(codec);
  }
  void SortCodecs() {
    std::sort(codecs_.begin(), codecs_.end(), PreferenceSort());
  }

 private:
  std::vector<C> codecs_;
};

class AudioContentDescription : public MediaContentDescriptionImpl<AudioCodec> {
 public:
  AudioContentDescription() :
      conference_mode_(false) {}

  virtual MediaType type() const { return MEDIA_TYPE_AUDIO; }

  bool conference_mode() const { return conference_mode_; }
  void set_conference_mode(bool enable) {
    conference_mode_ = enable;
  }

  const std::string &lang() const { return lang_; }
  void set_lang(const std::string &lang) { lang_ = lang; }


 private:
  bool conference_mode_;
  std::string lang_;
};

class VideoContentDescription : public MediaContentDescriptionImpl<VideoCodec> {
 public:
  virtual MediaType type() const { return MEDIA_TYPE_VIDEO; }
};

// Convenience functions.
bool IsAudioContent(const ContentInfo* content);
bool IsVideoContent(const ContentInfo* content);
const ContentInfo* GetFirstAudioContent(const SessionDescription* sdesc);
const ContentInfo* GetFirstVideoContent(const SessionDescription* sdesc);

}  // namespace cricket

#endif  // TALK_SESSION_PHONE_MEDIASESSIONCLIENT_H_
