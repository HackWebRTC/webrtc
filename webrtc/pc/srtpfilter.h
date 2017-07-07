/*
 *  Copyright 2009 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_PC_SRTPFILTER_H_
#define WEBRTC_PC_SRTPFILTER_H_

#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/media/base/cryptoparams.h"
#include "webrtc/p2p/base/sessiondescription.h"
#include "webrtc/rtc_base/basictypes.h"
#include "webrtc/rtc_base/constructormagic.h"
#include "webrtc/rtc_base/criticalsection.h"
#include "webrtc/rtc_base/sslstreamadapter.h"
#include "webrtc/rtc_base/thread_checker.h"

// Forward declaration to avoid pulling in libsrtp headers here
struct srtp_event_data_t;
struct srtp_ctx_t_;

namespace cricket {

class SrtpSession;

void ShutdownSrtp();

// Class to transform SRTP to/from RTP.
// Initialize by calling SetSend with the local security params, then call
// SetRecv once the remote security params are received. At that point
// Protect/UnprotectRt(c)p can be called to encrypt/decrypt data.
// TODO: Figure out concurrency policy for SrtpFilter.
class SrtpFilter {
 public:
  enum Mode {
    PROTECT,
    UNPROTECT
  };
  enum Error {
    ERROR_NONE,
    ERROR_FAIL,
    ERROR_AUTH,
    ERROR_REPLAY,
  };

  SrtpFilter();
  ~SrtpFilter();

  // Whether the filter is active (i.e. crypto has been properly negotiated).
  bool IsActive() const;

  // Indicates which crypto algorithms and keys were contained in the offer.
  // offer_params should contain a list of available parameters to use, or none,
  // if crypto is not desired. This must be called before SetAnswer.
  bool SetOffer(const std::vector<CryptoParams>& offer_params,
                ContentSource source);
  // Same as SetAnwer. But multiple calls are allowed to SetProvisionalAnswer
  // after a call to SetOffer.
  bool SetProvisionalAnswer(const std::vector<CryptoParams>& answer_params,
                            ContentSource source);
  // Indicates which crypto algorithms and keys were contained in the answer.
  // answer_params should contain the negotiated parameters, which may be none,
  // if crypto was not desired or could not be negotiated (and not required).
  // This must be called after SetOffer. If crypto negotiation completes
  // successfully, this will advance the filter to the active state.
  bool SetAnswer(const std::vector<CryptoParams>& answer_params,
                 ContentSource source);

  // Set the header extension ids that should be encrypted for the given source.
  void SetEncryptedHeaderExtensionIds(ContentSource source,
      const std::vector<int>& extension_ids);

  // Just set up both sets of keys directly.
  // Used with DTLS-SRTP.
  bool SetRtpParams(int send_cs,
                    const uint8_t* send_key,
                    int send_key_len,
                    int recv_cs,
                    const uint8_t* recv_key,
                    int recv_key_len);
  bool UpdateRtpParams(int send_cs,
                       const uint8_t* send_key,
                       int send_key_len,
                       int recv_cs,
                       const uint8_t* recv_key,
                       int recv_key_len);
  bool SetRtcpParams(int send_cs,
                     const uint8_t* send_key,
                     int send_key_len,
                     int recv_cs,
                     const uint8_t* recv_key,
                     int recv_key_len);

  // Encrypts/signs an individual RTP/RTCP packet, in-place.
  // If an HMAC is used, this will increase the packet size.
  bool ProtectRtp(void* data, int in_len, int max_len, int* out_len);
  // Overloaded version, outputs packet index.
  bool ProtectRtp(void* data,
                  int in_len,
                  int max_len,
                  int* out_len,
                  int64_t* index);
  bool ProtectRtcp(void* data, int in_len, int max_len, int* out_len);
  // Decrypts/verifies an invidiual RTP/RTCP packet.
  // If an HMAC is used, this will decrease the packet size.
  bool UnprotectRtp(void* data, int in_len, int* out_len);
  bool UnprotectRtcp(void* data, int in_len, int* out_len);

  // Returns rtp auth params from srtp context.
  bool GetRtpAuthParams(uint8_t** key, int* key_len, int* tag_len);

  // Returns srtp overhead for rtp packets.
  bool GetSrtpOverhead(int* srtp_overhead) const;

  // If external auth is enabled, SRTP will write a dummy auth tag that then
  // later must get replaced before the packet is sent out. Only supported for
  // non-GCM cipher suites and can be checked through "IsExternalAuthActive"
  // if it is actually used. This method is only valid before the RTP params
  // have been set.
  void EnableExternalAuth();
  bool IsExternalAuthEnabled() const;

  // A SRTP filter supports external creation of the auth tag if a non-GCM
  // cipher is used. This method is only valid after the RTP params have
  // been set.
  bool IsExternalAuthActive() const;

  bool ResetParams();

 protected:
  bool ExpectOffer(ContentSource source);
  bool StoreParams(const std::vector<CryptoParams>& params,
                   ContentSource source);
  bool ExpectAnswer(ContentSource source);
  bool DoSetAnswer(const std::vector<CryptoParams>& answer_params,
                     ContentSource source,
                     bool final);
  void CreateSrtpSessions();
  bool NegotiateParams(const std::vector<CryptoParams>& answer_params,
                       CryptoParams* selected_params);
  bool ApplyParams(const CryptoParams& send_params,
                   const CryptoParams& recv_params);
  static bool ParseKeyParams(const std::string& params,
                             uint8_t* key,
                             size_t len);

 private:
  enum State {
    ST_INIT,           // SRTP filter unused.
    ST_SENTOFFER,      // Offer with SRTP parameters sent.
    ST_RECEIVEDOFFER,  // Offer with SRTP parameters received.
    ST_SENTPRANSWER_NO_CRYPTO,  // Sent provisional answer without crypto.
    // Received provisional answer without crypto.
    ST_RECEIVEDPRANSWER_NO_CRYPTO,
    ST_ACTIVE,         // Offer and answer set.
    // SRTP filter is active but new parameters are offered.
    // When the answer is set, the state transitions to ST_ACTIVE or ST_INIT.
    ST_SENTUPDATEDOFFER,
    // SRTP filter is active but new parameters are received.
    // When the answer is set, the state transitions back to ST_ACTIVE.
    ST_RECEIVEDUPDATEDOFFER,
    // SRTP filter is active but the sent answer is only provisional.
    // When the final answer is set, the state transitions to ST_ACTIVE or
    // ST_INIT.
    ST_SENTPRANSWER,
    // SRTP filter is active but the received answer is only provisional.
    // When the final answer is set, the state transitions to ST_ACTIVE or
    // ST_INIT.
    ST_RECEIVEDPRANSWER
  };
  State state_ = ST_INIT;
  bool external_auth_enabled_ = false;
  std::vector<CryptoParams> offer_params_;
  std::unique_ptr<SrtpSession> send_session_;
  std::unique_ptr<SrtpSession> recv_session_;
  std::unique_ptr<SrtpSession> send_rtcp_session_;
  std::unique_ptr<SrtpSession> recv_rtcp_session_;
  CryptoParams applied_send_params_;
  CryptoParams applied_recv_params_;
  std::vector<int> send_encrypted_header_extension_ids_;
  std::vector<int> recv_encrypted_header_extension_ids_;
};

}  // namespace cricket

#endif  // WEBRTC_PC_SRTPFILTER_H_
