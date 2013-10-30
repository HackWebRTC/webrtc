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

#include "talk/media/base/streamparams.h"

#include <sstream>

namespace cricket {

const char kFecSsrcGroupSemantics[] = "FEC";
const char kFidSsrcGroupSemantics[] = "FID";
const char kSimSsrcGroupSemantics[] = "SIM";

static std::string SsrcsToString(const std::vector<uint32>& ssrcs) {
  std::ostringstream ost;
  ost << "ssrcs:[";
  for (std::vector<uint32>::const_iterator it = ssrcs.begin();
       it != ssrcs.end(); ++it) {
    if (it != ssrcs.begin()) {
      ost << ",";
    }
    ost << *it;
  }
  ost << "]";
  return ost.str();
}

bool SsrcGroup::has_semantics(const std::string& semantics_in) const {
  return (semantics == semantics_in && ssrcs.size() > 0);
}

std::string SsrcGroup::ToString() const {
  std::ostringstream ost;
  ost << "{";
  ost << "semantics:" << semantics << ";";
  ost << SsrcsToString(ssrcs);
  ost << "}";
  return ost.str();
}

std::string StreamParams::ToString() const {
  std::ostringstream ost;
  ost << "{";
  if (!groupid.empty()) {
    ost << "groupid:" << groupid << ";";
  }
  if (!id.empty()) {
    ost << "id:" << id << ";";
  }
  ost << SsrcsToString(ssrcs) << ";";
  ost << "ssrc_groups:";
  for (std::vector<SsrcGroup>::const_iterator it = ssrc_groups.begin();
       it != ssrc_groups.end(); ++it) {
    if (it != ssrc_groups.begin()) {
      ost << ",";
    }
    ost << it->ToString();
  }
  ost << ";";
  if (!type.empty()) {
    ost << "type:" << type << ";";
  }
  if (!display.empty()) {
    ost << "display:" << display << ";";
  }
  if (!cname.empty()) {
    ost << "cname:" << cname << ";";
  }
  if (!sync_label.empty()) {
    ost << "sync_label:" << sync_label;
  }
  ost << "}";
  return ost.str();
}

bool StreamParams::AddSecondarySsrc(const std::string& semantics,
                                    uint32 primary_ssrc,
                                    uint32 secondary_ssrc) {
  if (!has_ssrc(primary_ssrc)) {
    return false;
  }

  ssrcs.push_back(secondary_ssrc);
  std::vector<uint32> ssrc_vector;
  ssrc_vector.push_back(primary_ssrc);
  ssrc_vector.push_back(secondary_ssrc);
  SsrcGroup ssrc_group = SsrcGroup(semantics, ssrc_vector);
  ssrc_groups.push_back(ssrc_group);
  return true;
}

bool StreamParams::GetSecondarySsrc(const std::string& semantics,
                                    uint32 primary_ssrc,
                                    uint32* secondary_ssrc) const {
  for (std::vector<SsrcGroup>::const_iterator it = ssrc_groups.begin();
       it != ssrc_groups.end(); ++it) {
    if (it->has_semantics(semantics) &&
          it->ssrcs.size() >= 2 &&
          it->ssrcs[0] == primary_ssrc) {
      *secondary_ssrc = it->ssrcs[1];
      return true;
    }
  }
  return false;
}

bool GetStream(const StreamParamsVec& streams,
               const StreamSelector& selector,
               StreamParams* stream_out) {
  for (StreamParamsVec::const_iterator stream = streams.begin();
       stream != streams.end(); ++stream) {
    if (selector.Matches(*stream)) {
      if (stream_out != NULL) {
        *stream_out = *stream;
      }
      return true;
    }
  }
  return false;
}

bool GetStreamBySsrc(const StreamParamsVec& streams, uint32 ssrc,
                     StreamParams* stream_out) {
  return GetStream(streams, StreamSelector(ssrc), stream_out);
}

bool GetStreamByIds(const StreamParamsVec& streams,
                    const std::string& groupid,
                    const std::string& id,
                    StreamParams* stream_out) {
  return GetStream(streams, StreamSelector(groupid, id), stream_out);
}

bool RemoveStream(StreamParamsVec* streams,
                  const StreamSelector& selector) {
  bool ret = false;
  for (StreamParamsVec::iterator stream = streams->begin();
       stream != streams->end(); ) {
    if (selector.Matches(*stream)) {
      stream = streams->erase(stream);
      ret = true;
    } else {
      ++stream;
    }
  }
  return ret;
}

bool RemoveStreamBySsrc(StreamParamsVec* streams, uint32 ssrc) {
  return RemoveStream(streams, StreamSelector(ssrc));
}

bool RemoveStreamByIds(StreamParamsVec* streams,
                       const std::string& groupid,
                       const std::string& id) {
  return RemoveStream(streams, StreamSelector(groupid, id));
}

}  // namespace cricket
