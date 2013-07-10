/*
 * libjingle
 * Copyright 2012, Google Inc.
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

// This file contains a class used for gathering statistics from an ongoing
// libjingle PeerConnection.

#ifndef TALK_APP_WEBRTC_STATSCOLLECTOR_H_
#define TALK_APP_WEBRTC_STATSCOLLECTOR_H_

#include <string>
#include <map>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/statstypes.h"
#include "talk/app/webrtc/webrtcsession.h"

#include "talk/base/timing.h"

namespace webrtc {

class StatsCollector {
 public:
  StatsCollector();

  // Register the session Stats should operate on.
  // Set to NULL if the session has ended.
  void set_session(WebRtcSession* session) {
    session_ = session;
  }

  // Adds a MediaStream with tracks that can be used as a |selector| in a call
  // to GetStats.
  void AddStream(MediaStreamInterface* stream);

  // Gather statistics from the session and store them for future use.
  void UpdateStats();

  // Gets a StatsReports of the last collected stats. Note that UpdateStats must
  // be called before this function to get the most recent stats. |selector| is
  // a track label or empty string. The most recent reports are stored in
  // |reports|.
  bool GetStats(MediaStreamTrackInterface* track, StatsReports* reports);

  WebRtcSession* session() { return session_; }
  // Prepare an SSRC report for the given ssrc. Used internally.
  StatsReport* PrepareReport(uint32 ssrc, const std::string& transport);
  // Extracts the ID of a Transport belonging to an SSRC. Used internally.
  bool GetTransportIdFromProxy(const std::string& proxy,
                               std::string* transport_id);

 private:
  bool CopySelectedReports(const std::string& selector, StatsReports* reports);

  void ExtractSessionInfo();
  void ExtractVoiceInfo();
  void ExtractVideoInfo();
  double GetTimeNow();
  void BuildSsrcToTransportId();

  // A map from the report id to the report.
  std::map<std::string, webrtc::StatsReport> reports_;
  // Raw pointer to the session the statistics are gathered from.
  WebRtcSession* session_;
  double stats_gathering_started_;
  talk_base::Timing timing_;
  cricket::ProxyTransportMap proxy_to_transport_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_STATSCOLLECTOR_H_
