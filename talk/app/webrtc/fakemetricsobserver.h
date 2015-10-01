/*
 * libjingle
 * Copyright 2015 Google Inc.
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

#ifndef TALK_APP_WEBRTC_FAKEMETRICSOBSERVER_H_
#define TALK_APP_WEBRTC_FAKEMETRICSOBSERVER_H_

#include <map>
#include <string>

#include "talk/app/webrtc/peerconnectioninterface.h"
#include "webrtc/base/thread_checker.h"

namespace webrtc {

class FakeMetricsObserver : public MetricsObserverInterface {
 public:
  FakeMetricsObserver();
  void Reset();

  void IncrementEnumCounter(PeerConnectionEnumCounterType,
                            int counter,
                            int counter_max) override;
  void AddHistogramSample(PeerConnectionMetricsName type,
                          int value) override;
  void AddHistogramSample(PeerConnectionMetricsName type,
                          const std::string& value) override;

  // Accessors to be used by the tests.
  int GetEnumCounter(PeerConnectionEnumCounterType type, int counter) const;
  int GetIntHistogramSample(PeerConnectionMetricsName type) const;
  const std::string& GetStringHistogramSample(
      PeerConnectionMetricsName type) const;

 protected:
  ~FakeMetricsObserver() {}

 private:
  rtc::ThreadChecker thread_checker_;
  // This is a 2 dimension array. The first index is the enum counter type. The
  // 2nd index is the counter of that particular enum counter type.
  std::vector<std::vector<int>> counters_;
  int int_histogram_samples_[kPeerConnectionMetricsName_Max];
  std::string string_histogram_samples_[kPeerConnectionMetricsName_Max];
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_FAKEMETRICSOBSERVER_H_
