/*
 *  Copyright 2009 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/srtpfilter.h"

#include <string.h>

#include <algorithm>

#include "media/base/rtputils.h"
#include "pc/srtpsession.h"
#include "rtc_base/base64.h"
#include "rtc_base/buffer.h"
#include "rtc_base/byteorder.h"
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/stringencode.h"
#include "rtc_base/timeutils.h"

namespace cricket {

// NOTE: This is called from ChannelManager D'tor.
void ShutdownSrtp() {
  // If srtp_dealloc is not executed then this will clear all existing sessions.
  // This should be called when application is shutting down.
  SrtpSession::Terminate();
}

SrtpFilter::SrtpFilter() {
}

SrtpFilter::~SrtpFilter() {
}

bool SrtpFilter::IsActive() const {
  return state_ >= ST_ACTIVE;
}

bool SrtpFilter::SetOffer(const std::vector<CryptoParams>& offer_params,
                          ContentSource source) {
  if (!ExpectOffer(source)) {
     LOG(LS_ERROR) << "Wrong state to update SRTP offer";
     return false;
  }
  return StoreParams(offer_params, source);
}

bool SrtpFilter::SetAnswer(const std::vector<CryptoParams>& answer_params,
                           ContentSource source) {
  return DoSetAnswer(answer_params, source, true);
}

bool SrtpFilter::SetProvisionalAnswer(
    const std::vector<CryptoParams>& answer_params,
    ContentSource source) {
  return DoSetAnswer(answer_params, source, false);
}

bool SrtpFilter::SetRtpParams(int send_cs,
                              const uint8_t* send_key,
                              int send_key_len,
                              int recv_cs,
                              const uint8_t* recv_key,
                              int recv_key_len) {
  if (IsActive()) {
    LOG(LS_ERROR) << "Tried to set SRTP Params when filter already active";
    return false;
  }
  CreateSrtpSessions();
  send_session_->SetEncryptedHeaderExtensionIds(
      send_encrypted_header_extension_ids_);
  if (!send_session_->SetSend(send_cs, send_key, send_key_len)) {
    return false;
  }

  recv_session_->SetEncryptedHeaderExtensionIds(
      recv_encrypted_header_extension_ids_);
  if (!recv_session_->SetRecv(recv_cs, recv_key, recv_key_len)) {
    return false;
  }

  state_ = ST_ACTIVE;

  LOG(LS_INFO) << "SRTP activated with negotiated parameters:"
               << " send cipher_suite " << send_cs << " recv cipher_suite "
               << recv_cs;
  return true;
}

bool SrtpFilter::UpdateRtpParams(int send_cs,
                                 const uint8_t* send_key,
                                 int send_key_len,
                                 int recv_cs,
                                 const uint8_t* recv_key,
                                 int recv_key_len) {
  if (!IsActive()) {
    LOG(LS_ERROR) << "Tried to update SRTP Params when filter is not active";
    return false;
  }
  send_session_->SetEncryptedHeaderExtensionIds(
      send_encrypted_header_extension_ids_);
  if (!send_session_->UpdateSend(send_cs, send_key, send_key_len)) {
    return false;
  }

  recv_session_->SetEncryptedHeaderExtensionIds(
      recv_encrypted_header_extension_ids_);
  if (!recv_session_->UpdateRecv(recv_cs, recv_key, recv_key_len)) {
    return false;
  }

  LOG(LS_INFO) << "SRTP updated with negotiated parameters:"
               << " send cipher_suite " << send_cs << " recv cipher_suite "
               << recv_cs;
  return true;
}

// This function is provided separately because DTLS-SRTP behaves
// differently in RTP/RTCP mux and non-mux modes.
//
// - In the non-muxed case, RTP and RTCP are keyed with different
//   keys (from different DTLS handshakes), and so we need a new
//   SrtpSession.
// - In the muxed case, they are keyed with the same keys, so
//   this function is not needed
bool SrtpFilter::SetRtcpParams(int send_cs,
                               const uint8_t* send_key,
                               int send_key_len,
                               int recv_cs,
                               const uint8_t* recv_key,
                               int recv_key_len) {
  // This can only be called once, but can be safely called after
  // SetRtpParams
  if (send_rtcp_session_ || recv_rtcp_session_) {
    LOG(LS_ERROR) << "Tried to set SRTCP Params when filter already active";
    return false;
  }

  send_rtcp_session_.reset(new SrtpSession());
  if (!send_rtcp_session_->SetRecv(send_cs, send_key, send_key_len)) {
    return false;
  }

  recv_rtcp_session_.reset(new SrtpSession());
  if (!recv_rtcp_session_->SetRecv(recv_cs, recv_key, recv_key_len)) {
    return false;
  }

  LOG(LS_INFO) << "SRTCP activated with negotiated parameters:"
               << " send cipher_suite " << send_cs << " recv cipher_suite "
               << recv_cs;

  return true;
}

bool SrtpFilter::ProtectRtp(void* p, int in_len, int max_len, int* out_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to ProtectRtp: SRTP not active";
    return false;
  }
  RTC_CHECK(send_session_);
  return send_session_->ProtectRtp(p, in_len, max_len, out_len);
}

bool SrtpFilter::ProtectRtp(void* p,
                            int in_len,
                            int max_len,
                            int* out_len,
                            int64_t* index) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to ProtectRtp: SRTP not active";
    return false;
  }
  RTC_CHECK(send_session_);
  return send_session_->ProtectRtp(p, in_len, max_len, out_len, index);
}

bool SrtpFilter::ProtectRtcp(void* p, int in_len, int max_len, int* out_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to ProtectRtcp: SRTP not active";
    return false;
  }
  if (send_rtcp_session_) {
    return send_rtcp_session_->ProtectRtcp(p, in_len, max_len, out_len);
  } else {
    RTC_CHECK(send_session_);
    return send_session_->ProtectRtcp(p, in_len, max_len, out_len);
  }
}

bool SrtpFilter::UnprotectRtp(void* p, int in_len, int* out_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to UnprotectRtp: SRTP not active";
    return false;
  }
  RTC_CHECK(recv_session_);
  return recv_session_->UnprotectRtp(p, in_len, out_len);
}

bool SrtpFilter::UnprotectRtcp(void* p, int in_len, int* out_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to UnprotectRtcp: SRTP not active";
    return false;
  }
  if (recv_rtcp_session_) {
    return recv_rtcp_session_->UnprotectRtcp(p, in_len, out_len);
  } else {
    RTC_CHECK(recv_session_);
    return recv_session_->UnprotectRtcp(p, in_len, out_len);
  }
}

bool SrtpFilter::GetRtpAuthParams(uint8_t** key, int* key_len, int* tag_len) {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to GetRtpAuthParams: SRTP not active";
    return false;
  }

  RTC_CHECK(send_session_);
  return send_session_->GetRtpAuthParams(key, key_len, tag_len);
}

bool SrtpFilter::GetSrtpOverhead(int* srtp_overhead) const {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to GetSrtpOverhead: SRTP not active";
    return false;
  }

  RTC_CHECK(send_session_);
  *srtp_overhead = send_session_->GetSrtpOverhead();
  return true;
}

void SrtpFilter::EnableExternalAuth() {
  RTC_DCHECK(!IsActive());
  external_auth_enabled_ = true;
}

bool SrtpFilter::IsExternalAuthEnabled() const {
  return external_auth_enabled_;
}

bool SrtpFilter::IsExternalAuthActive() const {
  if (!IsActive()) {
    LOG(LS_WARNING) << "Failed to check IsExternalAuthActive: SRTP not active";
    return false;
  }

  RTC_CHECK(send_session_);
  return send_session_->IsExternalAuthActive();
}

void SrtpFilter::SetEncryptedHeaderExtensionIds(
    ContentSource source,
    const std::vector<int>& extension_ids) {
  if (source == CS_LOCAL) {
    recv_encrypted_header_extension_ids_ = extension_ids;
  } else {
    send_encrypted_header_extension_ids_ = extension_ids;
  }
}

bool SrtpFilter::ExpectOffer(ContentSource source) {
  return ((state_ == ST_INIT) ||
          (state_ == ST_ACTIVE) ||
          (state_  == ST_SENTOFFER && source == CS_LOCAL) ||
          (state_  == ST_SENTUPDATEDOFFER && source == CS_LOCAL) ||
          (state_ == ST_RECEIVEDOFFER && source == CS_REMOTE) ||
          (state_ == ST_RECEIVEDUPDATEDOFFER && source == CS_REMOTE));
}

bool SrtpFilter::StoreParams(const std::vector<CryptoParams>& params,
                             ContentSource source) {
  offer_params_ = params;
  if (state_ == ST_INIT) {
    state_ = (source == CS_LOCAL) ? ST_SENTOFFER : ST_RECEIVEDOFFER;
  } else if (state_ == ST_ACTIVE) {
    state_ =
        (source == CS_LOCAL) ? ST_SENTUPDATEDOFFER : ST_RECEIVEDUPDATEDOFFER;
  }
  return true;
}

bool SrtpFilter::ExpectAnswer(ContentSource source) {
  return ((state_ == ST_SENTOFFER && source == CS_REMOTE) ||
          (state_ == ST_RECEIVEDOFFER && source == CS_LOCAL) ||
          (state_ == ST_SENTUPDATEDOFFER && source == CS_REMOTE) ||
          (state_ == ST_RECEIVEDUPDATEDOFFER && source == CS_LOCAL) ||
          (state_ == ST_SENTPRANSWER_NO_CRYPTO && source == CS_LOCAL) ||
          (state_ == ST_SENTPRANSWER && source == CS_LOCAL) ||
          (state_ == ST_RECEIVEDPRANSWER_NO_CRYPTO && source == CS_REMOTE) ||
          (state_ == ST_RECEIVEDPRANSWER && source == CS_REMOTE));
}

bool SrtpFilter::DoSetAnswer(const std::vector<CryptoParams>& answer_params,
                             ContentSource source,
                             bool final) {
  if (!ExpectAnswer(source)) {
    LOG(LS_ERROR) << "Invalid state for SRTP answer";
    return false;
  }

  // If the answer doesn't requests crypto complete the negotiation of an
  // unencrypted session.
  // Otherwise, finalize the parameters and apply them.
  if (answer_params.empty()) {
    if (final) {
      return ResetParams();
    } else {
      // Need to wait for the final answer to decide if
      // we should go to Active state.
      state_ = (source == CS_LOCAL) ? ST_SENTPRANSWER_NO_CRYPTO :
                                      ST_RECEIVEDPRANSWER_NO_CRYPTO;
      return true;
    }
  }
  CryptoParams selected_params;
  if (!NegotiateParams(answer_params, &selected_params))
    return false;
  const CryptoParams& send_params =
      (source == CS_REMOTE) ? selected_params : answer_params[0];
  const CryptoParams& recv_params =
      (source == CS_REMOTE) ? answer_params[0] : selected_params;
  if (!ApplyParams(send_params, recv_params)) {
    return false;
  }

  if (final) {
    offer_params_.clear();
    state_ = ST_ACTIVE;
  } else {
    state_ =
        (source == CS_LOCAL) ? ST_SENTPRANSWER : ST_RECEIVEDPRANSWER;
  }
  return true;
}

void SrtpFilter::CreateSrtpSessions() {
  send_session_.reset(new SrtpSession());
  applied_send_params_ = CryptoParams();
  recv_session_.reset(new SrtpSession());
  applied_recv_params_ = CryptoParams();

  if (external_auth_enabled_) {
    send_session_->EnableExternalAuth();
  }
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
  // TODO(jiayl): Split this method to apply send and receive CryptoParams
  // independently, so that we can skip one method when either send or receive
  // CryptoParams is unchanged.
  if (applied_send_params_.cipher_suite == send_params.cipher_suite &&
      applied_send_params_.key_params == send_params.key_params &&
      applied_recv_params_.cipher_suite == recv_params.cipher_suite &&
      applied_recv_params_.key_params == recv_params.key_params) {
    LOG(LS_INFO) << "Applying the same SRTP parameters again. No-op.";

    // We do not want to reset the ROC if the keys are the same. So just return.
    return true;
  }

  int send_suite = rtc::SrtpCryptoSuiteFromName(send_params.cipher_suite);
  int recv_suite = rtc::SrtpCryptoSuiteFromName(recv_params.cipher_suite);
  if (send_suite == rtc::SRTP_INVALID_CRYPTO_SUITE ||
      recv_suite == rtc::SRTP_INVALID_CRYPTO_SUITE) {
    LOG(LS_WARNING) << "Unknown crypto suite(s) received:"
                    << " send cipher_suite " << send_params.cipher_suite
                    << " recv cipher_suite " << recv_params.cipher_suite;
    return false;
  }

  int send_key_len, send_salt_len;
  int recv_key_len, recv_salt_len;
  if (!rtc::GetSrtpKeyAndSaltLengths(send_suite, &send_key_len,
                                     &send_salt_len) ||
      !rtc::GetSrtpKeyAndSaltLengths(recv_suite, &recv_key_len,
                                     &recv_salt_len)) {
    LOG(LS_WARNING) << "Could not get lengths for crypto suite(s):"
                    << " send cipher_suite " << send_params.cipher_suite
                    << " recv cipher_suite " << recv_params.cipher_suite;
    return false;
  }

  // TODO(juberti): Zero these buffers after use.
  bool ret;
  rtc::Buffer send_key(send_key_len + send_salt_len);
  rtc::Buffer recv_key(recv_key_len + recv_salt_len);
  ret = (ParseKeyParams(send_params.key_params, send_key.data(),
                        send_key.size()) &&
         ParseKeyParams(recv_params.key_params, recv_key.data(),
                        recv_key.size()));
  if (ret) {
    CreateSrtpSessions();
    send_session_->SetEncryptedHeaderExtensionIds(
        send_encrypted_header_extension_ids_);
    recv_session_->SetEncryptedHeaderExtensionIds(
        recv_encrypted_header_extension_ids_);
    ret = (send_session_->SetSend(
               rtc::SrtpCryptoSuiteFromName(send_params.cipher_suite),
               send_key.data(), send_key.size()) &&
           recv_session_->SetRecv(
               rtc::SrtpCryptoSuiteFromName(recv_params.cipher_suite),
               recv_key.data(), recv_key.size()));
  }
  if (ret) {
    LOG(LS_INFO) << "SRTP activated with negotiated parameters:"
                 << " send cipher_suite " << send_params.cipher_suite
                 << " recv cipher_suite " << recv_params.cipher_suite;
    applied_send_params_ = send_params;
    applied_recv_params_ = recv_params;
  } else {
    LOG(LS_WARNING) << "Failed to apply negotiated SRTP parameters";
  }
  return ret;
}

bool SrtpFilter::ResetParams() {
  offer_params_.clear();
  state_ = ST_INIT;
  send_session_ = nullptr;
  recv_session_ = nullptr;
  send_rtcp_session_ = nullptr;
  recv_rtcp_session_ = nullptr;
  LOG(LS_INFO) << "SRTP reset to init state";
  return true;
}

bool SrtpFilter::ParseKeyParams(const std::string& key_params,
                                uint8_t* key,
                                size_t len) {
  // example key_params: "inline:YUJDZGVmZ2hpSktMbW9QUXJzVHVWd3l6MTIzNDU2"

  // Fail if key-method is wrong.
  if (key_params.find("inline:") != 0) {
    return false;
  }

  // Fail if base64 decode fails, or the key is the wrong size.
  std::string key_b64(key_params.substr(7)), key_str;
  if (!rtc::Base64::Decode(key_b64, rtc::Base64::DO_STRICT, &key_str,
                           nullptr) ||
      key_str.size() != len) {
    return false;
  }

  memcpy(key, key_str.c_str(), len);
  return true;
}

}  // namespace cricket
