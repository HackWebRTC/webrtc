/*
 * libjingle
 * Copyright 2011 Google Inc.
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

// This file contains structures for describing SSRCs from a media source such
// as a MediaStreamTrack when it is sent across an RTP session. Multiple media
// sources may be sent across the same RTP session, each of them will be
// described by one StreamParams object
// SsrcGroup is used to describe the relationship between the SSRCs that
// are used for this media source.
// E.x: Consider a source that is sent as 3 simulcast streams
// Let the simulcast elements have SSRC 10, 20, 30.
// Let each simulcast element use FEC and let the protection packets have
// SSRC 11,21,31.
// To describe this 4 SsrcGroups are needed,
// StreamParams would then contain ssrc = {10,11,20,21,30,31} and
// ssrc_groups = {{SIM,{10,20,30}, {FEC,{10,11}, {FEC, {20,21}, {FEC {30,31}}}
// Please see RFC 5576.

#ifndef TALK_MEDIA_BASE_STREAMPARAMS_H_
#define TALK_MEDIA_BASE_STREAMPARAMS_H_

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "webrtc/base/basictypes.h"

namespace cricket {

extern const char kFecSsrcGroupSemantics[];
extern const char kFidSsrcGroupSemantics[];
extern const char kSimSsrcGroupSemantics[];

struct SsrcGroup {
  SsrcGroup(const std::string& usage, const std::vector<uint32>& ssrcs)
      : semantics(usage), ssrcs(ssrcs) {
  }

  bool operator==(const SsrcGroup& other) const {
    return (semantics == other.semantics && ssrcs == other.ssrcs);
  }
  bool operator!=(const SsrcGroup &other) const {
    return !(*this == other);
  }

  bool has_semantics(const std::string& semantics) const;

  std::string ToString() const;

  std::string semantics;  // e.g FIX, FEC, SIM.
  std::vector<uint32> ssrcs;  // SSRCs of this type.
};

struct StreamParams {
  static StreamParams CreateLegacy(uint32 ssrc) {
    StreamParams stream;
    stream.ssrcs.push_back(ssrc);
    return stream;
  }

  bool operator==(const StreamParams& other) const {
    return (groupid == other.groupid &&
            id == other.id &&
            ssrcs == other.ssrcs &&
            ssrc_groups == other.ssrc_groups &&
            type == other.type &&
            display == other.display &&
            cname == other.cname &&
            sync_label == other.sync_label);
  }
  bool operator!=(const StreamParams &other) const {
    return !(*this == other);
  }

  uint32 first_ssrc() const {
    if (ssrcs.empty()) {
      return 0;
    }

    return ssrcs[0];
  }
  bool has_ssrcs() const {
    return !ssrcs.empty();
  }
  bool has_ssrc(uint32 ssrc) const {
    return std::find(ssrcs.begin(), ssrcs.end(), ssrc) != ssrcs.end();
  }
  void add_ssrc(uint32 ssrc) {
    ssrcs.push_back(ssrc);
  }
  bool has_ssrc_groups() const {
    return !ssrc_groups.empty();
  }
  bool has_ssrc_group(const std::string& semantics) const {
    return (get_ssrc_group(semantics) != NULL);
  }
  const SsrcGroup* get_ssrc_group(const std::string& semantics) const {
    for (std::vector<SsrcGroup>::const_iterator it = ssrc_groups.begin();
         it != ssrc_groups.end(); ++it) {
      if (it->has_semantics(semantics)) {
        return &(*it);
      }
    }
    return NULL;
  }

  // Convenience function to add an FID ssrc for a primary_ssrc
  // that's already been added.
  inline bool AddFidSsrc(uint32 primary_ssrc, uint32 fid_ssrc) {
    return AddSecondarySsrc(kFidSsrcGroupSemantics, primary_ssrc, fid_ssrc);
  }

  // Convenience function to lookup the FID ssrc for a primary_ssrc.
  // Returns false if primary_ssrc not found or FID not defined for it.
  inline bool GetFidSsrc(uint32 primary_ssrc, uint32* fid_ssrc) const {
    return GetSecondarySsrc(kFidSsrcGroupSemantics, primary_ssrc, fid_ssrc);
  }

  // Convenience to get all the SIM SSRCs if there are SIM ssrcs, or
  // the first SSRC otherwise.
  void GetPrimarySsrcs(std::vector<uint32>* ssrcs) const;

  // Convenience to get all the FID SSRCs for the given primary ssrcs.
  // If a given primary SSRC does not have a FID SSRC, the list of FID
  // SSRCS will be smaller than the list of primary SSRCs.
  void GetFidSsrcs(const std::vector<uint32>& primary_ssrcs,
                   std::vector<uint32>* fid_ssrcs) const;

  std::string ToString() const;

  // Resource of the MUC jid of the participant of with this stream.
  // For 1:1 calls, should be left empty (which means remote streams
  // and local streams should not be mixed together).
  std::string groupid;
  // Unique per-groupid, not across all groupids
  std::string id;
  std::vector<uint32> ssrcs;  // All SSRCs for this source
  std::vector<SsrcGroup> ssrc_groups;  // e.g. FID, FEC, SIM
  // Examples: "camera", "screencast"
  std::string type;
  // Friendly name describing stream
  std::string display;
  std::string cname;  // RTCP CNAME
  std::string sync_label;  // Friendly name of cname.

 private:
  bool AddSecondarySsrc(const std::string& semantics, uint32 primary_ssrc,
                        uint32 secondary_ssrc);
  bool GetSecondarySsrc(const std::string& semantics, uint32 primary_ssrc,
                        uint32* secondary_ssrc) const;
};

// A Stream can be selected by either groupid+id or ssrc.
struct StreamSelector {
  explicit StreamSelector(uint32 ssrc) :
      ssrc(ssrc) {
  }

  StreamSelector(const std::string& groupid,
                 const std::string& streamid) :
      ssrc(0),
      groupid(groupid),
      streamid(streamid) {
  }

  bool Matches(const StreamParams& stream) const {
    if (ssrc == 0) {
      return stream.groupid == groupid && stream.id == streamid;
    } else {
      return stream.has_ssrc(ssrc);
    }
  }

  uint32 ssrc;
  std::string groupid;
  std::string streamid;
};

typedef std::vector<StreamParams> StreamParamsVec;

// A collection of audio and video and data streams. Most of the
// methods are merely for convenience. Many of these methods are keyed
// by ssrc, which is the source identifier in the RTP spec
// (http://tools.ietf.org/html/rfc3550).
// TODO(pthatcher):  Add basic unit test for these.
// See https://code.google.com/p/webrtc/issues/detail?id=4107
struct MediaStreams {
 public:
  MediaStreams() {}
  void CopyFrom(const MediaStreams& sources);

  bool empty() const {
    return audio_.empty() && video_.empty() && data_.empty();
  }

  std::vector<StreamParams>* mutable_audio() { return &audio_; }
  std::vector<StreamParams>* mutable_video() { return &video_; }
  std::vector<StreamParams>* mutable_data() { return &data_; }
  const std::vector<StreamParams>& audio() const { return audio_; }
  const std::vector<StreamParams>& video() const { return video_; }
  const std::vector<StreamParams>& data() const { return data_; }

  // Gets a stream, returning true if found.
  bool GetAudioStream(
      const StreamSelector& selector, StreamParams* stream);
  bool GetVideoStream(
      const StreamSelector& selector, StreamParams* stream);
  bool GetDataStream(
      const StreamSelector& selector, StreamParams* stream);
  // Adds a stream.
  void AddAudioStream(const StreamParams& stream);
  void AddVideoStream(const StreamParams& stream);
  void AddDataStream(const StreamParams& stream);
  // Removes a stream, returning true if found and removed.
  bool RemoveAudioStream(const StreamSelector& selector);
  bool RemoveVideoStream(const StreamSelector& selector);
  bool RemoveDataStream(const StreamSelector& selector);

 private:
  std::vector<StreamParams> audio_;
  std::vector<StreamParams> video_;
  std::vector<StreamParams> data_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreams);
};

// A request for a specific format of a specific stream.
struct StaticVideoView {
  StaticVideoView(const StreamSelector& selector,
                  int width, int height, int framerate)
      : selector(selector),
        width(width),
        height(height),
        framerate(framerate),
        preference(0) {
  }

  StreamSelector selector;
  int width;
  int height;
  int framerate;
  int preference;
};

typedef std::vector<StaticVideoView> StaticVideoViews;

// A request for several streams in various formats.
struct ViewRequest {
  StaticVideoViews static_video_views;
};

template <class Condition>
const StreamParams* GetStream(const StreamParamsVec& streams,
                              Condition condition) {
  StreamParamsVec::const_iterator found =
      std::find_if(streams.begin(), streams.end(), condition);
  return found == streams.end() ? nullptr : &(*found);
}

inline const StreamParams* GetStreamBySsrc(const StreamParamsVec& streams,
                                           uint32 ssrc) {
  return GetStream(streams,
      [&ssrc](const StreamParams& sp) { return sp.has_ssrc(ssrc); });
}

inline const StreamParams* GetStreamByIds(const StreamParamsVec& streams,
                                          const std::string& groupid,
                                          const std::string& id) {
  return GetStream(streams,
      [&groupid, &id](const StreamParams& sp) {
        return sp.groupid == groupid && sp.id == id;
      });
}

inline const StreamParams* GetStream(const StreamParamsVec& streams,
                                     const StreamSelector& selector) {
  return GetStream(streams,
      [&selector](const StreamParams& sp) { return selector.Matches(sp); });
}

template <class Condition>
bool RemoveStream(StreamParamsVec* streams, Condition condition) {
  auto iter(std::remove_if(streams->begin(), streams->end(), condition));
  if (iter == streams->end())
    return false;
  streams->erase(iter, streams->end());
  return true;
}

// Removes the stream from streams. Returns true if a stream is
// found and removed.
inline bool RemoveStream(StreamParamsVec* streams,
                  const StreamSelector& selector) {
  return RemoveStream(streams,
      [&selector](const StreamParams& sp) { return selector.Matches(sp); });
}
inline bool RemoveStreamBySsrc(StreamParamsVec* streams, uint32 ssrc) {
  return RemoveStream(streams,
      [&ssrc](const StreamParams& sp) { return sp.has_ssrc(ssrc); });
}
inline bool RemoveStreamByIds(StreamParamsVec* streams,
                              const std::string& groupid,
                              const std::string& id) {
  return RemoveStream(streams,
      [&groupid, &id](const StreamParams& sp) {
        return sp.groupid == groupid && sp.id == id;
      });
}

// Checks if |sp| defines parameters for a single primary stream. There may
// be an RTX stream associated with the primary stream. Leaving as non-static so
// we can test this function.
bool IsOneSsrcStream(const StreamParams& sp);

// Checks if |sp| defines parameters for one Simulcast stream. There may be RTX
// streams associated with the simulcast streams. Leaving as non-static so we
// can test this function.
bool IsSimulcastStream(const StreamParams& sp);

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_STREAMPARAMS_H_
