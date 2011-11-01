/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#ifndef TALK_SESSION_PHONE_STREAMPARAMS_H_
#define TALK_SESSION_PHONE_STREAMPARAMS_H_

#include <string>
#include <vector>

namespace cricket {

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

  std::string semantics;  // e.g FIX, FEC, SIM.
  std::vector<uint32> ssrcs;  // SSRCs of this type.
};

struct StreamParams {
  StreamParams(const std::string& name,
               const std::vector<uint32>& ssrcs,
               const std::vector<SsrcGroup>& ssrc_groups,
               const std::string& cname,
               const std::string& sync_label)
      : name(name),
        ssrcs(ssrcs),
        ssrc_groups(ssrc_groups),
        cname(cname),
        sync_label(sync_label) {
  }
  StreamParams(const std::string& name,
               uint32 ssrc,
               const std::string& cname,
               const std::string& sync_label)
      : name(name),
        cname(cname),
        sync_label(sync_label) {
    ssrcs.push_back(ssrc);
  }
  bool operator==(const StreamParams& other) const {
    return (name == other.name && ssrcs == other.ssrcs &&
        ssrc_groups == other.ssrc_groups && cname == other.cname &&
        sync_label == sync_label);
  }
  bool operator!=(const StreamParams &other) const {
    return !(*this == other);
  }

  std::string name;  // Unique name of this source.
  std::vector<uint32> ssrcs;  // All SSRCs for this source
  std::vector<SsrcGroup> ssrc_groups;  // e.g. FID, FEC, SIM
  std::string cname;  // RTCP CNAME
  std::string sync_label;  // Friendly name of cname.
};

}  // namespace cricket

#endif  // TALK_SESSION_PHONE_STREAMPARAMS_H_
