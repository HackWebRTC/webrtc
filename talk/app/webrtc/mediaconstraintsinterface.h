/*
 * libjingle
 * Copyright 2013, Google Inc.
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

// This file contains the interface for MediaConstraints, corresponding to
// the definition at
// http://www.w3.org/TR/mediacapture-streams/#mediastreamconstraints and also
// used in WebRTC: http://dev.w3.org/2011/webrtc/editor/webrtc.html#constraints.

#ifndef TALK_APP_WEBRTC_MEDIACONSTRAINTSINTERFACE_H_
#define TALK_APP_WEBRTC_MEDIACONSTRAINTSINTERFACE_H_

#include <string>
#include <vector>

namespace webrtc {

// MediaConstraintsInterface
// Interface used for passing arguments about media constraints
// to the MediaStream and PeerConnection implementation.
class MediaConstraintsInterface {
 public:
  struct Constraint {
    Constraint() {}
    Constraint(const std::string& key, const std::string value)
        : key(key), value(value) {
    }
    std::string key;
    std::string value;
  };

  class Constraints : public std::vector<Constraint> {
   public:
    bool FindFirst(const std::string& key, std::string* value) const;
  };

  virtual const Constraints& GetMandatory() const = 0;
  virtual const Constraints& GetOptional() const = 0;


  // Constraint keys used by a local video source.
  // Specified by draft-alvestrand-constraints-resolution-00b
  static const char kMinAspectRatio[];  // minAspectRatio
  static const char kMaxAspectRatio[];  // maxAspectRatio
  static const char kMaxWidth[];  // maxWidth
  static const char kMinWidth[];  // minWidth
  static const char kMaxHeight[];  // maxHeight
  static const char kMinHeight[];  // minHeight
  static const char kMaxFrameRate[];  // maxFrameRate
  static const char kMinFrameRate[];  // minFrameRate

  // Constraint keys used by a local audio source.
  // These keys are google specific.
  static const char kEchoCancellation[];  // googEchoCancellation
  static const char kExperimentalEchoCancellation[];  // googEchoCancellation2
  static const char kAutoGainControl[];  // googAutoGainControl
  static const char kExperimentalAutoGainControl[];  // googAutoGainControl2
  static const char kNoiseSuppression[];  // googNoiseSuppression
  static const char kHighpassFilter[];  // googHighpassFilter
  static const char kTypingNoiseDetection[];  // googTypingNoiseDetection
  static const char kAudioMirroring[];  // googAudioMirroring

  // Google-specific constraint keys for a local video source
  static const char kNoiseReduction[];  // googNoiseReduction
  static const char kLeakyBucket[];  // googLeakyBucket
  // googTemporalLayeredScreencast
  static const char kTemporalLayeredScreencast[];
  static const char kCpuOveruseDetection[];

  // Constraint keys for CreateOffer / CreateAnswer
  // Specified by the W3C PeerConnection spec
  static const char kOfferToReceiveVideo[];  // OfferToReceiveVideo
  static const char kOfferToReceiveAudio[];  // OfferToReceiveAudio
  static const char kVoiceActivityDetection[];  // VoiceActivityDetection
  static const char kIceRestart[];  // IceRestart
  // These keys are google specific.
  static const char kUseRtpMux[];  // googUseRtpMUX

  // Constraints values.
  static const char kValueTrue[];  // true
  static const char kValueFalse[];  // false

  // Temporary pseudo-constraints used to enable DTLS-SRTP
  static const char kEnableDtlsSrtp[];  // Enable DTLS-SRTP
  // Temporary pseudo-constraints used to enable DataChannels
  static const char kEnableRtpDataChannels[];  // Enable RTP DataChannels
  // TODO(perkj): Remove kEnableSctpDataChannels once Chrome use
  // PeerConnectionFactory::SetOptions.
  static const char kEnableSctpDataChannels[];  // Enable SCTP DataChannels
  // Temporary pseudo-constraint for enabling DSCP through JS.
  static const char kEnableDscp[];

  // The prefix of internal-only constraints whose JS set values should be
  // stripped by Chrome before passed down to Libjingle.
  static const char kInternalConstraintPrefix[];

  // These constraints are for internal use only, representing Chrome command
  // line flags. So they are prefixed with "internal" so JS values will be
  // removed.
  // Used by a local audio source.
  // TODO(perkj): Remove once Chrome use PeerConnectionFactory::SetOptions.
  static const char kInternalAecDump[];  // internalAecDump

 protected:
  // Dtor protected as objects shouldn't be deleted via this interface
  virtual ~MediaConstraintsInterface() {}
};

bool FindConstraint(const MediaConstraintsInterface* constraints,
                    const std::string& key, bool* value,
                    size_t* mandatory_constraints);

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIACONSTRAINTSINTERFACE_H_
