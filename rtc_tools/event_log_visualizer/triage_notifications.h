/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RTC_TOOLS_EVENT_LOG_VISUALIZER_TRIAGE_NOTIFICATIONS_H_
#define RTC_TOOLS_EVENT_LOG_VISUALIZER_TRIAGE_NOTIFICATIONS_H_

#include <string>

namespace webrtc {
namespace plotting {

class TriageNotification {
 public:
  TriageNotification() : time_seconds_() {}
  explicit TriageNotification(float time_seconds)
      : time_seconds_(time_seconds) {}
  virtual ~TriageNotification() = default;
  virtual std::string ToString() = 0;
  rtc::Optional<float> Time() { return time_seconds_; }

 private:
  rtc::Optional<float> time_seconds_;
};

class IncomingRtpReceiveTimeGap : public TriageNotification {
 public:
  IncomingRtpReceiveTimeGap(float time_seconds, int64_t duration)
      : TriageNotification(time_seconds), duration_(duration) {}
  std::string ToString() {
    return std::string("No RTP packets received for ") +
           std::to_string(duration_) + std::string(" ms");
  }

 private:
  int64_t duration_;
};

class IncomingRtcpReceiveTimeGap : public TriageNotification {
 public:
  IncomingRtcpReceiveTimeGap(float time_seconds, int64_t duration)
      : TriageNotification(time_seconds), duration_(duration) {}
  std::string ToString() {
    return std::string("No RTCP packets received for ") +
           std::to_string(duration_) + std::string(" ms");
  }

 private:
  int64_t duration_;
};

class OutgoingRtpSendTimeGap : public TriageNotification {
 public:
  OutgoingRtpSendTimeGap(float time_seconds, int64_t duration)
      : TriageNotification(time_seconds), duration_(duration) {}
  std::string ToString() {
    return std::string("No RTP packets sent for ") + std::to_string(duration_) +
           std::string(" ms");
  }

 private:
  int64_t duration_;
};

class OutgoingRtcpSendTimeGap : public TriageNotification {
 public:
  OutgoingRtcpSendTimeGap(float time_seconds, int64_t duration)
      : TriageNotification(time_seconds), duration_(duration) {}
  std::string ToString() {
    return std::string("No RTCP packets sent for ") +
           std::to_string(duration_) + std::string(" ms");
  }

 private:
  int64_t duration_;
};

class IncomingSeqNoJump : public TriageNotification {
 public:
  IncomingSeqNoJump(float time_seconds, uint32_t ssrc)
      : TriageNotification(time_seconds), ssrc_(ssrc) {}
  std::string ToString() {
    return std::string("Sequence number jumps on incoming SSRC ") +
           std::to_string(ssrc_);
  }

 private:
  uint32_t ssrc_;
};

class IncomingCaptureTimeJump : public TriageNotification {
 public:
  IncomingCaptureTimeJump(float time_seconds, uint32_t ssrc)
      : TriageNotification(time_seconds), ssrc_(ssrc) {}
  std::string ToString() {
    return std::string("Capture timestamp jumps on incoming SSRC ") +
           std::to_string(ssrc_);
  }

 private:
  uint32_t ssrc_;
};

class OutgoingSeqNoJump : public TriageNotification {
 public:
  OutgoingSeqNoJump(float time_seconds, uint32_t ssrc)
      : TriageNotification(time_seconds), ssrc_(ssrc) {}
  std::string ToString() {
    return std::string("Sequence number jumps on outgoing SSRC ") +
           std::to_string(ssrc_);
  }

 private:
  uint32_t ssrc_;
};

class OutgoingCaptureTimeJump : public TriageNotification {
 public:
  OutgoingCaptureTimeJump(float time_seconds, uint32_t ssrc)
      : TriageNotification(time_seconds), ssrc_(ssrc) {}
  std::string ToString() {
    return std::string("Capture timestamp jumps on outgoing SSRC ") +
           std::to_string(ssrc_);
  }

 private:
  uint32_t ssrc_;
};

class OutgoingHighLoss : public TriageNotification {
 public:
  explicit OutgoingHighLoss(double avg_loss_fraction)
      : avg_loss_fraction_(avg_loss_fraction) {}
  std::string ToString() {
    return std::string("High average loss (") +
           std::to_string(avg_loss_fraction_ * 100) +
           std::string("%) across the call.");
  }

 private:
  double avg_loss_fraction_;
};

}  // namespace plotting
}  // namespace webrtc

#endif  // RTC_TOOLS_EVENT_LOG_VISUALIZER_TRIAGE_NOTIFICATIONS_H_
