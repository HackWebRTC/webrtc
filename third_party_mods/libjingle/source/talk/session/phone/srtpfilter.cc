/*
 * libjingle
 * Copyright 2009, Google Inc.
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

// talk's config.h, generated from mac_config_dot_h for OSX, conflicts with the
// one included by the libsrtp headers. Don't use it. Instead, we keep HAVE_SRTP
// and LOGGING defined in config.h.
#undef HAVE_CONFIG_H

#ifdef OSX
// TODO: For the XCode build, we force SRTP (b/2500074)
#ifndef HAVE_SRTP
#define HAVE_SRTP 1
#endif  // HAVE_SRTP
// If LOGGING is not defined, define it to 1 (b/3245816)
#ifndef LOGGING
#define LOGGING 1
#endif  // HAVE_SRTP
#endif

#include "talk/session/phone/srtpfilter.h"

#include <algorithm>
#include <cstring>

#include "talk/base/base64.h"
#include "talk/base/logging.h"
#include "talk/base/time.h"
#include "talk/session/phone/rtputils.h"

// Enable this line to turn on SRTP debugging
// #define SRTP_DEBUG

#ifdef HAVE_SRTP
#ifdef SRTP_RELATIVE_PATH
#include "srtp.h"  // NOLINT
#else
#include "third_party/libsrtp/include/srtp.h"
#endif  // SRTP_RELATIVE_PATH
#ifdef _DEBUG
extern "C" debug_module_t mod_srtp;
extern "C" debug_module_t mod_auth;
extern "C" debug_module_t mod_cipher;
extern "C" debug_module_t mod_stat;
extern "C" debug_module_t mod_alloc;
extern "C" debug_module_t mod_aes_icm;
extern "C" debug_module_t mod_aes_hmac;
#endif
#else
// SrtpFilter needs that constant.
#define SRTP_MASTER_KEY_LEN 30
#endif  // HAVE_SRTP

namespace cricket {

const std::string& CS_DEFAULT = CS_AES_CM_128_HMAC_SHA1_80;
const std::string CS_AES_CM_128_HMAC_SHA1_80 = "AES_CM_128_HMAC_SHA1_80";
const std::string CS_AES_CM_128_HMAC_SHA1_32 = "AES_CM_128_HMAC_SHA1_32";
const int SRTP_MASTER_KEY_BASE64_LEN = SRTP_MASTER_KEY_LEN * 4 / 3;

#ifndef HAVE_SRTP

// This helper function is used on systems that don't (yet) have SRTP,
// to log that the functions that require it won't do anything.
namespace {
bool SrtpNotAvailable(const char *func) {
  LOG(LS_ERROR) << func << ": SRTP is not available on your system.";
  return false;
}
}  // anonymous namespace

#endif  // !HAVE_SRTP

#ifdef HAVE_SRTP //due to cricket namespace it can't be clubbed with above cond
void EnableSrtpDebugging() {
#ifdef _DEBUG
  debug_on(mod_srtp);
  debug_on(mod_auth);
  debug_on(mod_cipher);
  debug_on(mod_stat);
  debug_on(mod_alloc);
  debug_on(mod_aes_icm);
  // debug_on(mod_aes_cbc);
  // debug_on(mod_hmac);
#endif
}
#endif

SrtpFilter::SrtpFilter()
    : state_(ST_INIT),
      send_session_(new SrtpSession()),
      recv_session_(new SrtpSession()) {
  SignalSrtpError.repeat(send_session_->SignalSrtpError);
  SignalSrtpError.repeat(recv_session_->SignalSrtpError);
}

SrtpFilter::~SrtpFilter() {
}

bool SrtpFilter::IsActive() const {
  return (state_ == ST_ACTIVE);
}

bool SrtpFilter::SetOffer(const std::vector<CryptoParams>& offer_params,
                          ContentSource source) {
  bool ret = false;
  if (state_ == ST_INIT) {
    ret = StoreParams(offer_params, source);
  } else {
    LOG(LS_ERROR) << "Invalid state for SRTP offer";
  }
  return ret;
}

bool SrtpFilter::SetAnswer(const std::vector<CryptoParams>& answer_params,
                           ContentSource source) {
  bool ret = false;
  if ((state_ == ST_SENTOFFER && source == CS_REMOTE) ||
      (state_ == ST_RECEIVEDOFFER && source == CS_LOCAL)) {
    // If the answer requests crypto, finalize the parameters and apply them.
    // Otherwise, complete the negotiation of a unencrypted session.
    if (!answer_params.empty()) {
      CryptoParams selected_params;
      ret = NegotiateParams(answer_params, &selected_params);
      if (ret) {
        if (state_ == ST_SENTOFFER) {
          ret = ApplyParams(selected_params, answer_params[0]);
        } else {  // ST_RECEIVEDOFFER
          ret = ApplyParams(answer_params[0], selected_params);
        }
      }
    } else {
      ret = ResetParams();
    }
  } else {
    LOG(LS_ERROR) << "Invalid state for SRTP answer";
  }
  return ret;
}

bool SrtpFilter::ProtectRtp(void* p, int in_len, int max_len, int* out_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to ProtectRtp: SRTP not active";
    return false;
  }
  return send_session_->ProtectRtp(p, in_len, max_len, out_len);
}

bool SrtpFilter::ProtectRtcp(void* p, int in_len, int max_len, int* out_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to ProtectRtcp: SRTP not active";
    return false;
  }
  return send_session_->ProtectRtcp(p, in_len, max_len, out_len);
}

bool SrtpFilter::UnprotectRtp(void* p, int in_len, int* out_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to UnprotectRtp: SRTP not active";
    return false;
  }
  return recv_session_->UnprotectRtp(p, in_len, out_len);
}

bool SrtpFilter::UnprotectRtcp(void* p, int in_len, int* out_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to UnprotectRtcp: SRTP not active";
    return false;
  }
  return recv_session_->UnprotectRtcp(p, in_len, out_len);
}

void SrtpFilter::set_signal_silent_time(uint32 signal_silent_time_in_ms) {
  send_session_->set_signal_silent_time(signal_silent_time_in_ms);
  recv_session_->set_signal_silent_time(signal_silent_time_in_ms);
}

bool SrtpFilter::StoreParams(const std::vector<CryptoParams>& params,
                             ContentSource source) {
  offer_params_ = params;
  state_ = (source == CS_LOCAL) ? ST_SENTOFFER : ST_RECEIVEDOFFER;
  return true;
}

bool SrtpFilter::NegotiateParams(const std::vector<CryptoParams>& answer_params,
                                 CryptoParams* selected_params) {
  // We're processing an accept. We should have exactly one set of params,
  // unless the offer didn't mention crypto, in which case we shouldn't be here.
  bool ret = (answer_params.size() == 1U && !offer_params_.empty());
  if (ret) {
    // We should find a match between the answer params and the offered params.
    std::vector<CryptoParams>::const_iterator it;
    for (it = offer_params_.begin(); it != offer_params_.end(); ++it) {
      if (answer_params[0].Matches(*it)) {
        break;
      }
    }

    if (it != offer_params_.end()) {
      *selected_params = *it;
    } else {
      ret = false;
    }
  }

  if (!ret) {
    LOG(LS_WARNING) << "Invalid parameters in SRTP answer";
  }
  return ret;
}

bool SrtpFilter::ApplyParams(const CryptoParams& send_params,
                             const CryptoParams& recv_params) {
  // TODO: Zero these buffers after use.
  bool ret;
  uint8 send_key[SRTP_MASTER_KEY_LEN], recv_key[SRTP_MASTER_KEY_LEN];
  ret = (ParseKeyParams(send_params.key_params, send_key, sizeof(send_key)) &&
         ParseKeyParams(recv_params.key_params, recv_key, sizeof(recv_key)));
  if (ret) {
    ret = (send_session_->SetSend(send_params.cipher_suite,
                                  send_key, sizeof(send_key)) &&
           recv_session_->SetRecv(recv_params.cipher_suite,
                                  recv_key, sizeof(recv_key)));
  }
  if (ret) {
    offer_params_.clear();
    state_ = ST_ACTIVE;
    LOG(LS_INFO) << "SRTP activated with negotiated parameters:"
                 << " send cipher_suite " << send_params.cipher_suite
                 << " recv cipher_suite " << recv_params.cipher_suite;
  } else {
    LOG(LS_WARNING) << "Failed to apply negotiated SRTP parameters";
  }
  return ret;
}

bool SrtpFilter::ResetParams() {
  offer_params_.clear();
  state_ = ST_INIT;
  LOG(LS_INFO) << "SRTP reset to init state";
  return true;
}

bool SrtpFilter::ParseKeyParams(const std::string& key_params,
                                uint8* key, int len) {
  // example key_params: "inline:YUJDZGVmZ2hpSktMbW9QUXJzVHVWd3l6MTIzNDU2"

  // Fail if key-method is wrong.
  if (key_params.find("inline:") != 0) {
    return false;
  }

  // Fail if base64 decode fails, or the key is the wrong size.
  std::string key_b64(key_params.substr(7)), key_str;
  if (!talk_base::Base64::Decode(key_b64, talk_base::Base64::DO_STRICT,
                                 &key_str, NULL) ||
      static_cast<int>(key_str.size()) != len) {
    return false;
  }

  memcpy(key, key_str.c_str(), len);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// SrtpSession

#ifdef HAVE_SRTP

bool SrtpSession::inited_ = false;
std::list<SrtpSession*> SrtpSession::sessions_;

SrtpSession::SrtpSession()
    : session_(NULL),
      rtp_auth_tag_len_(0),
      rtcp_auth_tag_len_(0),
      srtp_stat_(new SrtpStat()),
      last_send_seq_num_(-1) {
  sessions_.push_back(this);
  SignalSrtpError.repeat(srtp_stat_->SignalSrtpError);
}

SrtpSession::~SrtpSession() {
  sessions_.erase(std::find(sessions_.begin(), sessions_.end(), this));
  if (session_) {
    srtp_dealloc(session_);
  }
}

bool SrtpSession::SetSend(const std::string& cs, const uint8* key, int len) {
  return SetKey(ssrc_any_outbound, cs, key, len);
}

bool SrtpSession::SetRecv(const std::string& cs, const uint8* key, int len) {
  return SetKey(ssrc_any_inbound, cs, key, len);
}

bool SrtpSession::ProtectRtp(void* p, int in_len, int max_len, int* out_len) {
  if (!session_) {
    LOG(LS_WARNING) << "Failed to protect SRTP packet: no SRTP Session";
    return false;
  }

  int need_len = in_len + rtp_auth_tag_len_;  // NOLINT
  if (max_len < need_len) {
    LOG(LS_WARNING) << "Failed to protect SRTP packet: The buffer length "
                    << max_len << " is less than the needed " << need_len;
    return false;
  }

  *out_len = in_len;
  int err = srtp_protect(session_, p, out_len);
  uint32 ssrc;
  if (GetRtpSsrc(p, in_len, &ssrc)) {
    srtp_stat_->AddProtectRtpResult(ssrc, err);
  }
  int seq_num;
  GetRtpSeqNum(p, in_len, &seq_num);
  if (err != err_status_ok) {
    LOG(LS_WARNING) << "Failed to protect SRTP packet, seqnum="
                    << seq_num << ", err=" << err << ", last seqnum="
                    << last_send_seq_num_;
    return false;
  }
  last_send_seq_num_ = seq_num;
  return true;
}

bool SrtpSession::ProtectRtcp(void* p, int in_len, int max_len, int* out_len) {
  if (!session_) {
    LOG(LS_WARNING) << "Failed to protect SRTCP packet: no SRTP Session";
    return false;
  }

  int need_len = in_len + sizeof(uint32) + rtcp_auth_tag_len_;  // NOLINT
  if (max_len < need_len) {
    LOG(LS_WARNING) << "Failed to protect SRTCP packet: The buffer length "
                    << max_len << " is less than the needed " << need_len;
    return false;
  }

  *out_len = in_len;
  int err = srtp_protect_rtcp(session_, p, out_len);
  srtp_stat_->AddProtectRtcpResult(err);
  if (err != err_status_ok) {
    LOG(LS_WARNING) << "Failed to protect SRTCP packet, err=" << err;
    return false;
  }
  return true;
}

bool SrtpSession::UnprotectRtp(void* p, int in_len, int* out_len) {
  if (!session_) {
    LOG(LS_WARNING) << "Failed to unprotect SRTP packet: no SRTP Session";
    return false;
  }

  *out_len = in_len;
  int err = srtp_unprotect(session_, p, out_len);
  uint32 ssrc;
  if (GetRtpSsrc(p, in_len, &ssrc)) {
    srtp_stat_->AddUnprotectRtpResult(ssrc, err);
  }
  if (err != err_status_ok) {
    LOG(LS_WARNING) << "Failed to unprotect SRTP packet, err=" << err;
    return false;
  }
  return true;
}

bool SrtpSession::UnprotectRtcp(void* p, int in_len, int* out_len) {
  if (!session_) {
    LOG(LS_WARNING) << "Failed to unprotect SRTCP packet: no SRTP Session";
    return false;
  }

  *out_len = in_len;
  int err = srtp_unprotect_rtcp(session_, p, out_len);
  srtp_stat_->AddUnprotectRtcpResult(err);
  if (err != err_status_ok) {
    LOG(LS_WARNING) << "Failed to unprotect SRTCP packet, err=" << err;
    return false;
  }
  return true;
}

void SrtpSession::set_signal_silent_time(uint32 signal_silent_time_in_ms) {
  srtp_stat_->set_signal_silent_time(signal_silent_time_in_ms);
}

bool SrtpSession::SetKey(int type, const std::string& cs,
                         const uint8* key, int len) {
  if (session_) {
    LOG(LS_ERROR) << "Failed to create SRTP session: "
                  << "SRTP session already created";
    return false;
  }

  if (!Init()) {
    return false;
  }

  srtp_policy_t policy;
  memset(&policy, 0, sizeof(policy));

  if (cs == CS_AES_CM_128_HMAC_SHA1_80) {
    crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
    crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
  } else if (cs == CS_AES_CM_128_HMAC_SHA1_32) {
    crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);   // rtp is 32,
    crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);  // rtcp still 80
  } else {
    LOG(LS_WARNING) << "Failed to create SRTP session: unsupported"
                    << " cipher_suite " << cs.c_str();
    return false;
  }

  if (!key || len != SRTP_MASTER_KEY_LEN) {
    LOG(LS_WARNING) << "Failed to create SRTP session: invalid key";
    return false;
  }

  policy.ssrc.type = static_cast<ssrc_type_t>(type);
  policy.ssrc.value = 0;
  policy.key = const_cast<uint8*>(key);
  // TODO parse window size from WSH session-param
  policy.window_size = 1024;
  policy.allow_repeat_tx = 1;
  policy.next = NULL;

  int err = srtp_create(&session_, &policy);
  if (err != err_status_ok) {
    LOG(LS_ERROR) << "Failed to create SRTP session, err=" << err;
    return false;
  }

  rtp_auth_tag_len_ = policy.rtp.auth_tag_len;
  rtcp_auth_tag_len_ = policy.rtcp.auth_tag_len;
  return true;
}

bool SrtpSession::Init() {
  if (!inited_) {
    int err;
    err = srtp_init();
    if (err != err_status_ok) {
      LOG(LS_ERROR) << "Failed to init SRTP, err=" << err;
      return false;
    }

    err = srtp_install_event_handler(&SrtpSession::HandleEventThunk);
    if (err != err_status_ok) {
      LOG(LS_ERROR) << "Failed to install SRTP event handler, err=" << err;
      return false;
    }

    inited_ = true;
  }

  return true;
}

void SrtpSession::HandleEvent(const srtp_event_data_t* ev) {
  switch (ev->event) {
    case event_ssrc_collision:
      LOG(LS_INFO) << "SRTP event: SSRC collision";
      break;
    case event_key_soft_limit:
      LOG(LS_INFO) << "SRTP event: reached soft key usage limit";
      break;
    case event_key_hard_limit:
      LOG(LS_INFO) << "SRTP event: reached hard key usage limit";
      break;
    case event_packet_index_limit:
      LOG(LS_INFO) << "SRTP event: reached hard packet limit (2^48 packets)";
      break;
    default:
      LOG(LS_INFO) << "SRTP event: unknown " << ev->event;
      break;
  }
}

void SrtpSession::HandleEventThunk(srtp_event_data_t* ev) {
  for (std::list<SrtpSession*>::iterator it = sessions_.begin();
       it != sessions_.end(); ++it) {
    if ((*it)->session_ == ev->session) {
      (*it)->HandleEvent(ev);
      break;
    }
  }
}

#else   // !HAVE_SRTP


SrtpSession::SrtpSession() {
  LOG(WARNING) << "SRTP implementation is missing.";
}

SrtpSession::~SrtpSession() {
}

bool SrtpSession::SetSend(const std::string& cs, const uint8* key, int len) {
  return SrtpNotAvailable(__FUNCTION__);
}

bool SrtpSession::SetRecv(const std::string& cs, const uint8* key, int len) {
  return SrtpNotAvailable(__FUNCTION__);
}

bool SrtpSession::ProtectRtp(void* data, int in_len, int max_len,
                             int* out_len) {
  return SrtpNotAvailable(__FUNCTION__);
}

bool SrtpSession::ProtectRtcp(void* data, int in_len, int max_len,
                              int* out_len) {
  return SrtpNotAvailable(__FUNCTION__);
}

bool SrtpSession::UnprotectRtp(void* data, int in_len, int* out_len) {
  return SrtpNotAvailable(__FUNCTION__);
}

bool SrtpSession::UnprotectRtcp(void* data, int in_len, int* out_len) {
  return SrtpNotAvailable(__FUNCTION__);
}

void SrtpSession::set_signal_silent_time(uint32 signal_silent_time) {
  // Do nothing.
}

#endif  // HAVE_SRTP

///////////////////////////////////////////////////////////////////////////////
// SrtpStat

#ifdef HAVE_SRTP

SrtpStat::SrtpStat()
    : signal_silent_time_(1000) {
}

void SrtpStat::AddProtectRtpResult(uint32 ssrc, int result) {
  FailureKey key;
  key.ssrc = ssrc;
  key.mode = SrtpFilter::PROTECT;
  switch (result) {
    case err_status_ok:
      key.error = SrtpFilter::ERROR_NONE;
      break;
    case err_status_auth_fail:
      key.error = SrtpFilter::ERROR_AUTH;
      break;
    default:
      key.error = SrtpFilter::ERROR_FAIL;
  }
  HandleSrtpResult(key);
}

void SrtpStat::AddUnprotectRtpResult(uint32 ssrc, int result) {
  FailureKey key;
  key.ssrc = ssrc;
  key.mode = SrtpFilter::UNPROTECT;
  switch (result) {
    case err_status_ok:
      key.error = SrtpFilter::ERROR_NONE;
      break;
    case err_status_auth_fail:
      key.error = SrtpFilter::ERROR_AUTH;
      break;
    case err_status_replay_fail:
    case err_status_replay_old:
      key.error = SrtpFilter::ERROR_REPLAY;
      break;
    default:
      key.error = SrtpFilter::ERROR_FAIL;
  }
  HandleSrtpResult(key);
}

void SrtpStat::AddProtectRtcpResult(int result) {
  AddProtectRtpResult(0U, result);
}

void SrtpStat::AddUnprotectRtcpResult(int result) {
  AddUnprotectRtpResult(0U, result);
}

void SrtpStat::HandleSrtpResult(const SrtpStat::FailureKey& key) {
  // Handle some cases where error should be signalled right away. For other
  // errors, trigger error for the first time seeing it.  After that, silent
  // the same error for a certain amount of time (default 1 sec).
  if (key.error != SrtpFilter::ERROR_NONE) {
    // For errors, signal first time and wait for 1 sec.
    FailureStat* stat = &(failures_[key]);
    uint32 current_time = talk_base::Time();
    if (stat->last_signal_time == 0 ||
        talk_base::TimeDiff(current_time, stat->last_signal_time) >
        static_cast<int>(signal_silent_time_)) {
      SignalSrtpError(key.ssrc, key.mode, key.error);
      stat->last_signal_time = current_time;
    }
  }
}

#else   // !HAVE_SRTP


SrtpStat::SrtpStat()
    : signal_silent_time_(1000) {
  LOG(WARNING) << "SRTP implementation is missing.";
}

void SrtpStat::AddProtectRtpResult(uint32 ssrc, int result) {
  SrtpNotAvailable(__FUNCTION__);
}

void SrtpStat::AddUnprotectRtpResult(uint32 ssrc, int result) {
  SrtpNotAvailable(__FUNCTION__);
}

void SrtpStat::AddProtectRtcpResult(int result) {
  SrtpNotAvailable(__FUNCTION__);
}

void SrtpStat::AddUnprotectRtcpResult(int result) {
  SrtpNotAvailable(__FUNCTION__);
}

void SrtpStat::HandleSrtpResult(const SrtpStat::FailureKey& key) {
  SrtpNotAvailable(__FUNCTION__);
}

#endif  // HAVE_SRTP

}  // namespace cricket
