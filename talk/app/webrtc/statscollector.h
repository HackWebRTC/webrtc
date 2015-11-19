/*
 * libjingle
 * Copyright 2012 Google Inc.
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

class PeerConnection;

// Conversion function to convert candidate type string to the corresponding one
// from  enum RTCStatsIceCandidateType.
const char* IceCandidateTypeToStatsType(const std::string& candidate_type);

// Conversion function to convert adapter type to report string which are more
// fitting to the general style of http://w3c.github.io/webrtc-stats. This is
// only used by stats collector.
const char* AdapterTypeToStatsType(rtc::AdapterType type);

// A mapping between track ids and their StatsReport.
typedef std::map<std::string, StatsReport*> TrackIdMap;

class StatsCollector {
 public:
  // The caller is responsible for ensuring that the pc outlives the
  // StatsCollector instance.
  explicit StatsCollector(PeerConnection* pc);
  virtual ~StatsCollector();

  // Adds a MediaStream with tracks that can be used as a |selector| in a call
  // to GetStats.
  void AddStream(MediaStreamInterface* stream);

  // Adds a local audio track that is used for getting some voice statistics.
  void AddLocalAudioTrack(AudioTrackInterface* audio_track, uint32_t ssrc);

  // Removes a local audio tracks that is used for getting some voice
  // statistics.
  void RemoveLocalAudioTrack(AudioTrackInterface* audio_track, uint32_t ssrc);

  // Gather statistics from the session and store them for future use.
  void UpdateStats(PeerConnectionInterface::StatsOutputLevel level);

  // Gets a StatsReports of the last collected stats. Note that UpdateStats must
  // be called before this function to get the most recent stats. |selector| is
  // a track label or empty string. The most recent reports are stored in
  // |reports|.
  // TODO(tommi): Change this contract to accept a callback object instead
  // of filling in |reports|.  As is, there's a requirement that the caller
  // uses |reports| immediately without allowing any async activity on
  // the thread (message handling etc) and then discard the results.
  void GetStats(MediaStreamTrackInterface* track,
                StatsReports* reports);

  // Prepare a local or remote SSRC report for the given ssrc. Used internally
  // in the ExtractStatsFromList template.
  StatsReport* PrepareReport(bool local,
                             uint32_t ssrc,
                             const StatsReport::Id& transport_id,
                             StatsReport::Direction direction);

  // Method used by the unittest to force a update of stats since UpdateStats()
  // that occur less than kMinGatherStatsPeriod number of ms apart will be
  // ignored.
  void ClearUpdateStatsCacheForTest();

 private:
  friend class StatsCollectorTest;

  // Overridden in unit tests to fake timing.
  virtual double GetTimeNow();

  bool CopySelectedReports(const std::string& selector, StatsReports* reports);

  // Helper method for AddCertificateReports.
  StatsReport* AddOneCertificateReport(
      const rtc::SSLCertificate* cert, const StatsReport* issuer);

  // Helper method for creating IceCandidate report. |is_local| indicates
  // whether this candidate is local or remote.
  StatsReport* AddCandidateReport(const cricket::Candidate& candidate,
                                  bool local);

  // Adds a report for this certificate and every certificate in its chain, and
  // returns the leaf certificate's report.
  StatsReport* AddCertificateReports(const rtc::SSLCertificate* cert);

  StatsReport* AddConnectionInfoReport(const std::string& content_name,
      int component, int connection_id,
      const StatsReport::Id& channel_report_id,
      const cricket::ConnectionInfo& info);

  void ExtractDataInfo();
  void ExtractSessionInfo();
  void ExtractVoiceInfo();
  void ExtractVideoInfo(PeerConnectionInterface::StatsOutputLevel level);
  void BuildSsrcToTransportId();
  webrtc::StatsReport* GetReport(const StatsReport::StatsType& type,
                                 const std::string& id,
                                 StatsReport::Direction direction);

  // Helper method to get stats from the local audio tracks.
  void UpdateStatsFromExistingLocalAudioTracks();
  void UpdateReportFromAudioTrack(AudioTrackInterface* track,
                                  StatsReport* report);

  // Helper method to get the id for the track identified by ssrc.
  // |direction| tells if the track is for sending or receiving.
  bool GetTrackIdBySsrc(uint32_t ssrc,
                        std::string* track_id,
                        StatsReport::Direction direction);

  // Helper method to update the timestamp of track records.
  void UpdateTrackReports();

  // A collection for all of our stats reports.
  StatsCollection reports_;
  TrackIdMap track_ids_;
  // Raw pointer to the peer connection the statistics are gathered from.
  PeerConnection* const pc_;
  double stats_gathering_started_;
  ProxyTransportMap proxy_to_transport_;

  // TODO(tommi): We appear to be holding on to raw pointers to reference
  // counted objects?  We should be using scoped_refptr here.
  typedef std::vector<std::pair<AudioTrackInterface*, uint32_t> >
      LocalAudioTrackVector;
  LocalAudioTrackVector local_audio_tracks_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_STATSCOLLECTOR_H_
