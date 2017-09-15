/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "pc/srtpsession.h"

#include "third_party/libsrtp/include/srtp.h"
#include "third_party/libsrtp/include/srtp_priv.h"
#include "media/base/rtputils.h"
#include "pc/externalhmac.h"
#include "rtc_base/logging.h"
#include "rtc_base/sslstreamadapter.h"

namespace cricket {

bool SrtpSession::inited_ = false;

// This lock protects SrtpSession::inited_.
rtc::GlobalLockPod SrtpSession::lock_;

SrtpSession::SrtpSession() {}

SrtpSession::~SrtpSession() {
  if (session_) {
    srtp_set_user_data(session_, nullptr);
    srtp_dealloc(session_);
  }
}

bool SrtpSession::SetSend(int cs, const uint8_t* key, size_t len) {
  return SetKey(ssrc_any_outbound, cs, key, len);
}

bool SrtpSession::UpdateSend(int cs, const uint8_t* key, size_t len) {
  return UpdateKey(ssrc_any_outbound, cs, key, len);
}

bool SrtpSession::SetRecv(int cs, const uint8_t* key, size_t len) {
  return SetKey(ssrc_any_inbound, cs, key, len);
}

bool SrtpSession::UpdateRecv(int cs, const uint8_t* key, size_t len) {
  return UpdateKey(ssrc_any_inbound, cs, key, len);
}

bool SrtpSession::ProtectRtp(void* p, int in_len, int max_len, int* out_len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
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
  int seq_num;
  GetRtpSeqNum(p, in_len, &seq_num);
  if (err != srtp_err_status_ok) {
    LOG(LS_WARNING) << "Failed to protect SRTP packet, seqnum=" << seq_num
                    << ", err=" << err
                    << ", last seqnum=" << last_send_seq_num_;
    return false;
  }
  last_send_seq_num_ = seq_num;
  return true;
}

bool SrtpSession::ProtectRtp(void* p,
                             int in_len,
                             int max_len,
                             int* out_len,
                             int64_t* index) {
  if (!ProtectRtp(p, in_len, max_len, out_len)) {
    return false;
  }
  return (index) ? GetSendStreamPacketIndex(p, in_len, index) : true;
}

bool SrtpSession::ProtectRtcp(void* p, int in_len, int max_len, int* out_len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!session_) {
    LOG(LS_WARNING) << "Failed to protect SRTCP packet: no SRTP Session";
    return false;
  }

  int need_len = in_len + sizeof(uint32_t) + rtcp_auth_tag_len_;  // NOLINT
  if (max_len < need_len) {
    LOG(LS_WARNING) << "Failed to protect SRTCP packet: The buffer length "
                    << max_len << " is less than the needed " << need_len;
    return false;
  }

  *out_len = in_len;
  int err = srtp_protect_rtcp(session_, p, out_len);
  if (err != srtp_err_status_ok) {
    LOG(LS_WARNING) << "Failed to protect SRTCP packet, err=" << err;
    return false;
  }
  return true;
}

bool SrtpSession::UnprotectRtp(void* p, int in_len, int* out_len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!session_) {
    LOG(LS_WARNING) << "Failed to unprotect SRTP packet: no SRTP Session";
    return false;
  }

  *out_len = in_len;
  int err = srtp_unprotect(session_, p, out_len);
  if (err != srtp_err_status_ok) {
    LOG(LS_WARNING) << "Failed to unprotect SRTP packet, err=" << err;
    return false;
  }
  return true;
}

bool SrtpSession::UnprotectRtcp(void* p, int in_len, int* out_len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!session_) {
    LOG(LS_WARNING) << "Failed to unprotect SRTCP packet: no SRTP Session";
    return false;
  }

  *out_len = in_len;
  int err = srtp_unprotect_rtcp(session_, p, out_len);
  if (err != srtp_err_status_ok) {
    LOG(LS_WARNING) << "Failed to unprotect SRTCP packet, err=" << err;
    return false;
  }
  return true;
}

bool SrtpSession::GetRtpAuthParams(uint8_t** key, int* key_len, int* tag_len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(IsExternalAuthActive());
  if (!IsExternalAuthActive()) {
    return false;
  }

  ExternalHmacContext* external_hmac = nullptr;
  // stream_template will be the reference context for other streams.
  // Let's use it for getting the keys.
  srtp_stream_ctx_t* srtp_context = session_->stream_template;
  if (srtp_context && srtp_context->session_keys &&
      srtp_context->session_keys->rtp_auth) {
    external_hmac = reinterpret_cast<ExternalHmacContext*>(
        srtp_context->session_keys->rtp_auth->state);
  }

  if (!external_hmac) {
    LOG(LS_ERROR) << "Failed to get auth keys from libsrtp!.";
    return false;
  }

  *key = external_hmac->key;
  *key_len = external_hmac->key_length;
  *tag_len = rtp_auth_tag_len_;
  return true;
}

int SrtpSession::GetSrtpOverhead() const {
  return rtp_auth_tag_len_;
}

void SrtpSession::EnableExternalAuth() {
  RTC_DCHECK(!session_);
  external_auth_enabled_ = true;
}

bool SrtpSession::IsExternalAuthEnabled() const {
  return external_auth_enabled_;
}

bool SrtpSession::IsExternalAuthActive() const {
  return external_auth_active_;
}

bool SrtpSession::GetSendStreamPacketIndex(void* p,
                                           int in_len,
                                           int64_t* index) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  srtp_hdr_t* hdr = reinterpret_cast<srtp_hdr_t*>(p);
  srtp_stream_ctx_t* stream = srtp_get_stream(session_, hdr->ssrc);
  if (!stream) {
    return false;
  }

  // Shift packet index, put into network byte order
  *index = static_cast<int64_t>(rtc::NetworkToHost64(
      srtp_rdbx_get_packet_index(&stream->rtp_rdbx) << 16));
  return true;
}

bool SrtpSession::DoSetKey(int type, int cs, const uint8_t* key, size_t len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());

  srtp_policy_t policy;
  memset(&policy, 0, sizeof(policy));
  if (cs == rtc::SRTP_AES128_CM_SHA1_80) {
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
  } else if (cs == rtc::SRTP_AES128_CM_SHA1_32) {
    // RTP HMAC is shortened to 32 bits, but RTCP remains 80 bits.
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
    srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);
  } else if (cs == rtc::SRTP_AEAD_AES_128_GCM) {
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);
  } else if (cs == rtc::SRTP_AEAD_AES_256_GCM) {
    srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
    srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);
  } else {
    LOG(LS_WARNING) << "Failed to " << (session_ ? "update" : "create")
                    << " SRTP session: unsupported cipher_suite " << cs;
    return false;
  }

  int expected_key_len;
  int expected_salt_len;
  if (!rtc::GetSrtpKeyAndSaltLengths(cs, &expected_key_len,
                                     &expected_salt_len)) {
    // This should never happen.
    LOG(LS_WARNING)
        << "Failed to " << (session_ ? "update" : "create")
        << " SRTP session: unsupported cipher_suite without length information"
        << cs;
    return false;
  }

  if (!key ||
      len != static_cast<size_t>(expected_key_len + expected_salt_len)) {
    LOG(LS_WARNING) << "Failed to " << (session_ ? "update" : "create")
                    << " SRTP session: invalid key";
    return false;
  }

  policy.ssrc.type = static_cast<srtp_ssrc_type_t>(type);
  policy.ssrc.value = 0;
  policy.key = const_cast<uint8_t*>(key);
  // TODO(astor) parse window size from WSH session-param
  policy.window_size = 1024;
  policy.allow_repeat_tx = 1;
  // If external authentication option is enabled, supply custom auth module
  // id EXTERNAL_HMAC_SHA1 in the policy structure.
  // We want to set this option only for rtp packets.
  // By default policy structure is initialized to HMAC_SHA1.
  // Enable external HMAC authentication only for outgoing streams and only
  // for cipher suites that support it (i.e. only non-GCM cipher suites).
  if (type == ssrc_any_outbound && IsExternalAuthEnabled() &&
      !rtc::IsGcmCryptoSuite(cs)) {
    policy.rtp.auth_type = EXTERNAL_HMAC_SHA1;
  }
  if (!encrypted_header_extension_ids_.empty()) {
    policy.enc_xtn_hdr = const_cast<int*>(&encrypted_header_extension_ids_[0]);
    policy.enc_xtn_hdr_count =
        static_cast<int>(encrypted_header_extension_ids_.size());
  }
  policy.next = nullptr;

  if (!session_) {
    int err = srtp_create(&session_, &policy);
    if (err != srtp_err_status_ok) {
      session_ = nullptr;
      LOG(LS_ERROR) << "Failed to create SRTP session, err=" << err;
      return false;
    }
    srtp_set_user_data(session_, this);
  } else {
    int err = srtp_update(session_, &policy);
    if (err != srtp_err_status_ok) {
      LOG(LS_ERROR) << "Failed to update SRTP session, err=" << err;
      return false;
    }
  }

  rtp_auth_tag_len_ = policy.rtp.auth_tag_len;
  rtcp_auth_tag_len_ = policy.rtcp.auth_tag_len;
  external_auth_active_ = (policy.rtp.auth_type == EXTERNAL_HMAC_SHA1);
  return true;
}

bool SrtpSession::SetKey(int type, int cs, const uint8_t* key, size_t len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (session_) {
    LOG(LS_ERROR) << "Failed to create SRTP session: "
                  << "SRTP session already created";
    return false;
  }

  if (!Init()) {
    return false;
  }

  return DoSetKey(type, cs, key, len);
}

bool SrtpSession::UpdateKey(int type, int cs, const uint8_t* key, size_t len) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!session_) {
    LOG(LS_ERROR) << "Failed to update non-existing SRTP session";
    return false;
  }

  return DoSetKey(type, cs, key, len);
}

void SrtpSession::SetEncryptedHeaderExtensionIds(
    const std::vector<int>& encrypted_header_extension_ids) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  encrypted_header_extension_ids_ = encrypted_header_extension_ids;
}

bool SrtpSession::Init() {
  rtc::GlobalLockScope ls(&lock_);

  if (!inited_) {
    int err;
    err = srtp_init();
    if (err != srtp_err_status_ok) {
      LOG(LS_ERROR) << "Failed to init SRTP, err=" << err;
      return false;
    }

    err = srtp_install_event_handler(&SrtpSession::HandleEventThunk);
    if (err != srtp_err_status_ok) {
      LOG(LS_ERROR) << "Failed to install SRTP event handler, err=" << err;
      return false;
    }

    err = external_crypto_init();
    if (err != srtp_err_status_ok) {
      LOG(LS_ERROR) << "Failed to initialize fake auth, err=" << err;
      return false;
    }
    inited_ = true;
  }

  return true;
}

void SrtpSession::Terminate() {
  rtc::GlobalLockScope ls(&lock_);

  if (inited_) {
    int err = srtp_shutdown();
    if (err) {
      LOG(LS_ERROR) << "srtp_shutdown failed. err=" << err;
      return;
    }
    inited_ = false;
  }
}

void SrtpSession::HandleEvent(const srtp_event_data_t* ev) {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
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
  // Callback will be executed from same thread that calls the "srtp_protect"
  // and "srtp_unprotect" functions.
  SrtpSession* session =
      static_cast<SrtpSession*>(srtp_get_user_data(ev->session));
  if (session) {
    session->HandleEvent(ev);
  }
}

}  // namespace cricket
