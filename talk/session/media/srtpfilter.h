/*
 * libjingle
 * Copyright 2009 Google Inc.
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

#ifndef TALK_SESSION_MEDIA_SRTPFILTER_H_
#define TALK_SESSION_MEDIA_SRTPFILTER_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "talk/base/basictypes.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslotrepeater.h"
#include "talk/media/base/cryptoparams.h"
#include "talk/p2p/base/sessiondescription.h"

// Forward declaration to avoid pulling in libsrtp headers here
struct srtp_event_data_t;
struct srtp_ctx_t;
typedef srtp_ctx_t* srtp_t;
struct srtp_policy_t;

namespace cricket {

// Cipher suite to use for SRTP. Typically a 80-bit HMAC will be used, except
// in applications (voice) where the additional bandwidth may be significant.
// A 80-bit HMAC is always used for SRTCP.
// 128-bit AES with 80-bit SHA-1 HMAC.
extern const char CS_AES_CM_128_HMAC_SHA1_80[];
// 128-bit AES with 32-bit SHA-1 HMAC.
extern const char CS_AES_CM_128_HMAC_SHA1_32[];
// Key is 128 bits and salt is 112 bits == 30 bytes. B64 bloat => 40 bytes.
extern const int SRTP_MASTER_KEY_BASE64_LEN;

// Needed for DTLS-SRTP
extern const int SRTP_MASTER_KEY_KEY_LEN;
extern const int SRTP_MASTER_KEY_SALT_LEN;

class SrtpSession;
class SrtpStat;

void EnableSrtpDebugging();

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

  // Just set up both sets of keys directly.
  // Used with DTLS-SRTP.
  bool SetRtpParams(const std::string& send_cs,
                    const uint8* send_key, int send_key_len,
                    const std::string& recv_cs,
                    const uint8* recv_key, int recv_key_len);
  bool SetRtcpParams(const std::string& send_cs,
                     const uint8* send_key, int send_key_len,
                     const std::string& recv_cs,
                     const uint8* recv_key, int recv_key_len);

  // Encrypts/signs an individual RTP/RTCP packet, in-place.
  // If an HMAC is used, this will increase the packet size.
  bool ProtectRtp(void* data, int in_len, int max_len, int* out_len);
  bool ProtectRtcp(void* data, int in_len, int max_len, int* out_len);
  // Decrypts/verifies an invidiual RTP/RTCP packet.
  // If an HMAC is used, this will decrease the packet size.
  bool UnprotectRtp(void* data, int in_len, int* out_len);
  bool UnprotectRtcp(void* data, int in_len, int* out_len);

  // Update the silent threshold (in ms) for signaling errors.
  void set_signal_silent_time(uint32 signal_silent_time_in_ms);

  sigslot::repeater3<uint32, Mode, Error> SignalSrtpError;

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
  bool ResetParams();
  static bool ParseKeyParams(const std::string& params, uint8* key, int len);

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
  State state_;
  uint32 signal_silent_time_in_ms_;
  std::vector<CryptoParams> offer_params_;
  talk_base::scoped_ptr<SrtpSession> send_session_;
  talk_base::scoped_ptr<SrtpSession> recv_session_;
  talk_base::scoped_ptr<SrtpSession> send_rtcp_session_;
  talk_base::scoped_ptr<SrtpSession> recv_rtcp_session_;
  CryptoParams applied_send_params_;
  CryptoParams applied_recv_params_;
};

// Class that wraps a libSRTP session.
class SrtpSession {
 public:
  SrtpSession();
  ~SrtpSession();

  // Configures the session for sending data using the specified
  // cipher-suite and key. Receiving must be done by a separate session.
  bool SetSend(const std::string& cs, const uint8* key, int len);
  // Configures the session for receiving data using the specified
  // cipher-suite and key. Sending must be done by a separate session.
  bool SetRecv(const std::string& cs, const uint8* key, int len);

  // Encrypts/signs an individual RTP/RTCP packet, in-place.
  // If an HMAC is used, this will increase the packet size.
  bool ProtectRtp(void* data, int in_len, int max_len, int* out_len);
  bool ProtectRtcp(void* data, int in_len, int max_len, int* out_len);
  // Decrypts/verifies an invidiual RTP/RTCP packet.
  // If an HMAC is used, this will decrease the packet size.
  bool UnprotectRtp(void* data, int in_len, int* out_len);
  bool UnprotectRtcp(void* data, int in_len, int* out_len);

  // Update the silent threshold (in ms) for signaling errors.
  void set_signal_silent_time(uint32 signal_silent_time_in_ms);

  sigslot::repeater3<uint32, SrtpFilter::Mode, SrtpFilter::Error>
      SignalSrtpError;

 private:
  bool SetKey(int type, const std::string& cs, const uint8* key, int len);
  static bool Init();
  void HandleEvent(const srtp_event_data_t* ev);
  static void HandleEventThunk(srtp_event_data_t* ev);
  static std::list<SrtpSession*>* sessions();

  srtp_t session_;
  int rtp_auth_tag_len_;
  int rtcp_auth_tag_len_;
  talk_base::scoped_ptr<SrtpStat> srtp_stat_;
  static bool inited_;
  int last_send_seq_num_;
  DISALLOW_COPY_AND_ASSIGN(SrtpSession);
};

// Class that collects failures of SRTP.
class SrtpStat {
 public:
  SrtpStat();

  // Report RTP protection results to the handler.
  void AddProtectRtpResult(uint32 ssrc, int result);
  // Report RTP unprotection results to the handler.
  void AddUnprotectRtpResult(uint32 ssrc, int result);
  // Report RTCP protection results to the handler.
  void AddProtectRtcpResult(int result);
  // Report RTCP unprotection results to the handler.
  void AddUnprotectRtcpResult(int result);

  // Get silent time (in ms) for SRTP statistics handler.
  uint32 signal_silent_time() const { return signal_silent_time_; }
  // Set silent time (in ms) for SRTP statistics handler.
  void set_signal_silent_time(uint32 signal_silent_time) {
    signal_silent_time_ = signal_silent_time;
  }

  // Sigslot for reporting errors.
  sigslot::signal3<uint32, SrtpFilter::Mode, SrtpFilter::Error>
      SignalSrtpError;

 private:
  // For each different ssrc and error, we collect statistics separately.
  struct FailureKey {
    FailureKey()
        : ssrc(0),
          mode(SrtpFilter::PROTECT),
          error(SrtpFilter::ERROR_NONE) {
    }
    FailureKey(uint32 in_ssrc, SrtpFilter::Mode in_mode,
               SrtpFilter::Error in_error)
        : ssrc(in_ssrc),
          mode(in_mode),
          error(in_error) {
    }
    bool operator <(const FailureKey& key) const {
      return ssrc < key.ssrc || mode < key.mode || error < key.error;
    }
    uint32 ssrc;
    SrtpFilter::Mode mode;
    SrtpFilter::Error error;
  };
  // For tracing conditions for signaling, currently we only use
  // last_signal_time.  Wrap this as a struct so that later on, if we need any
  // other improvements, it will be easier.
  struct FailureStat {
    FailureStat()
        : last_signal_time(0) {
    }
    explicit FailureStat(uint32 in_last_signal_time)
        : last_signal_time(in_last_signal_time) {
    }
    void Reset() {
      last_signal_time = 0;
    }
    uint32 last_signal_time;
  };

  // Inspect SRTP result and signal error if needed.
  void HandleSrtpResult(const FailureKey& key);

  std::map<FailureKey, FailureStat> failures_;
  // Threshold in ms to silent the signaling errors.
  uint32 signal_silent_time_;

  DISALLOW_COPY_AND_ASSIGN(SrtpStat);
};

}  // namespace cricket

#endif  // TALK_SESSION_MEDIA_SRTPFILTER_H_
