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

#include <map>
#include <string>
#include <vector>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "talk/app/webrtc/peerconnectioninterface.h"
#include "talk/app/webrtc/statstypes.h"
#include "talk/app/webrtc/webrtcsession.h"

namespace webrtc {

class StatsCollector {
 public:
  enum TrackDirection {
    kSending = 0,
    kReceiving,
  };

  // The caller is responsible for ensuring that the session outlives the
  // StatsCollector instance.
  explicit StatsCollector(WebRtcSession* session);
  virtual ~StatsCollector();

  // Adds a MediaStream with tracks that can be used as a |selector| in a call
  // to GetStats.
  void AddStream(MediaStreamInterface* stream);

  // Adds a local audio track that is used for getting some voice statistics.
  void AddLocalAudioTrack(AudioTrackInterface* audio_track, uint32 ssrc);

  // Removes a local audio tracks that is used for getting some voice
  // statistics.
  void RemoveLocalAudioTrack(AudioTrackInterface* audio_track, uint32 ssrc);

  // Gather statistics from the session and store them for future use.
  void UpdateStats(PeerConnectionInterface::StatsOutputLevel level);

  // Gets a StatsReports of the last collected stats. Note that UpdateStats must
  // be called before this function to get the most recent stats. |selector| is
  // a track label or empty string. The most recent reports are stored in
  // |reports|.
  bool GetStats(MediaStreamTrackInterface* track,
                StatsReports* reports);

  // Prepare an SSRC report for the given ssrc. Used internally
  // in the ExtractStatsFromList template.
  StatsReport* PrepareLocalReport(uint32 ssrc, const std::string& transport,
                                  TrackDirection direction);
  // Prepare an SSRC report for the given remote ssrc. Used internally.
  StatsReport* PrepareRemoteReport(uint32 ssrc, const std::string& transport,
                                   TrackDirection direction);

  // Method used by the unittest to force a update of stats since UpdateStats()
  // that occur less than kMinGatherStatsPeriod number of ms apart will be
  // ignored.
  void ClearUpdateStatsCache();

 private:
  bool CopySelectedReports(const std::string& selector, StatsReports* reports);

  // Helper method for AddCertificateReports.
  std::string AddOneCertificateReport(
      const talk_base::SSLCertificate* cert, const std::string& issuer_id);

  // Adds a report for this certificate and every certificate in its chain, and
  // returns the leaf certificate's report's ID.
  std::string AddCertificateReports(const talk_base::SSLCertificate* cert);

  void ExtractSessionInfo();
  void ExtractVoiceInfo();
  void ExtractVideoInfo(PeerConnectionInterface::StatsOutputLevel level);
  void BuildSsrcToTransportId();
  webrtc::StatsReport* GetOrCreateReport(const std::string& type,
                                         const std::string& id,
                                         TrackDirection direction);
  webrtc::StatsReport* GetReport(const std::string& type,
                                 const std::string& id,
                                 TrackDirection direction);

  // Helper method to get stats from the local audio tracks.
  void UpdateStatsFromExistingLocalAudioTracks();
  void UpdateReportFromAudioTrack(AudioTrackInterface* track,
                                  StatsReport* report);

  // Helper method to get the id for the track identified by ssrc.
  // |direction| tells if the track is for sending or receiving.
  bool GetTrackIdBySsrc(uint32 ssrc, std::string* track_id,
                        TrackDirection direction);

  // A map from the report id to the report.
  std::map<std::string, StatsReport> reports_;
  // Raw pointer to the session the statistics are gathered from.
  WebRtcSession* const session_;
  double stats_gathering_started_;
  cricket::ProxyTransportMap proxy_to_transport_;

  typedef std::vector<std::pair<AudioTrackInterface*, uint32> >
      LocalAudioTrackVector;
  LocalAudioTrackVector local_audio_tracks_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_STATSCOLLECTOR_H_
