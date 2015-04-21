/*
 * libjingle
 * Copyright 2004 Google Inc.
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

#include "talk/media/base/codec.h"

#include <algorithm>
#include <sstream>

#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/stringencode.h"
#include "webrtc/base/stringutils.h"

namespace cricket {

const int kMaxPayloadId = 127;

bool FeedbackParam::operator==(const FeedbackParam& other) const {
  return _stricmp(other.id().c_str(), id().c_str()) == 0 &&
      _stricmp(other.param().c_str(), param().c_str()) == 0;
}

bool FeedbackParams::operator==(const FeedbackParams& other) const {
  return params_ == other.params_;
}

bool FeedbackParams::Has(const FeedbackParam& param) const {
  return std::find(params_.begin(), params_.end(), param) != params_.end();
}

void FeedbackParams::Add(const FeedbackParam& param) {
  if (param.id().empty()) {
    return;
  }
  if (Has(param)) {
    // Param already in |this|.
    return;
  }
  params_.push_back(param);
  ASSERT(!HasDuplicateEntries());
}

void FeedbackParams::Intersect(const FeedbackParams& from) {
  std::vector<FeedbackParam>::iterator iter_to = params_.begin();
  while (iter_to != params_.end()) {
    if (!from.Has(*iter_to)) {
      iter_to = params_.erase(iter_to);
    } else {
      ++iter_to;
    }
  }
}

bool FeedbackParams::HasDuplicateEntries() const {
  for (std::vector<FeedbackParam>::const_iterator iter = params_.begin();
       iter != params_.end(); ++iter) {
    for (std::vector<FeedbackParam>::const_iterator found = iter + 1;
         found != params_.end(); ++found) {
      if (*found == *iter) {
        return true;
      }
    }
  }
  return false;
}

Codec::Codec(int id, const std::string& name, int clockrate, int preference)
    : id(id), name(name), clockrate(clockrate), preference(preference) {
}

Codec::Codec() : id(0), clockrate(0), preference(0) {
}

Codec::Codec(const Codec& c) = default;

Codec::~Codec() = default;

Codec& Codec::operator=(const Codec& c) {
  this->id = c.id;  // id is reserved in objective-c
  name = c.name;
  clockrate = c.clockrate;
  preference = c.preference;
  params = c.params;
  feedback_params = c.feedback_params;
  return *this;
}

bool Codec::operator==(const Codec& c) const {
  return this->id == c.id &&  // id is reserved in objective-c
         name == c.name && clockrate == c.clockrate &&
         preference == c.preference && params == c.params &&
         feedback_params == c.feedback_params;
}

bool Codec::Matches(const Codec& codec) const {
  // Match the codec id/name based on the typical static/dynamic name rules.
  // Matching is case-insensitive.
  const int kMaxStaticPayloadId = 95;
  return (codec.id <= kMaxStaticPayloadId) ?
      (id == codec.id) : (_stricmp(name.c_str(), codec.name.c_str()) == 0);
}

bool Codec::GetParam(const std::string& name, std::string* out) const {
  CodecParameterMap::const_iterator iter = params.find(name);
  if (iter == params.end())
    return false;
  *out = iter->second;
  return true;
}

bool Codec::GetParam(const std::string& name, int* out) const {
  CodecParameterMap::const_iterator iter = params.find(name);
  if (iter == params.end())
    return false;
  return rtc::FromString(iter->second, out);
}

void Codec::SetParam(const std::string& name, const std::string& value) {
  params[name] = value;
}

void Codec::SetParam(const std::string& name, int value)  {
  params[name] = rtc::ToString(value);
}

bool Codec::RemoveParam(const std::string& name) {
  return params.erase(name) == 1;
}

void Codec::AddFeedbackParam(const FeedbackParam& param) {
  feedback_params.Add(param);
}

bool Codec::HasFeedbackParam(const FeedbackParam& param) const {
  return feedback_params.Has(param);
}

void Codec::IntersectFeedbackParams(const Codec& other) {
  feedback_params.Intersect(other.feedback_params);
}

AudioCodec::AudioCodec(int pt,
                       const std::string& nm,
                       int cr,
                       int br,
                       int cs,
                       int pr)
    : Codec(pt, nm, cr, pr), bitrate(br), channels(cs) {
}

AudioCodec::AudioCodec() : Codec(), bitrate(0), channels(0) {
}

AudioCodec::AudioCodec(const AudioCodec& c) = default;

AudioCodec& AudioCodec::operator=(const AudioCodec& c) {
  Codec::operator=(c);
  bitrate = c.bitrate;
  channels = c.channels;
  return *this;
}

bool AudioCodec::operator==(const AudioCodec& c) const {
  return bitrate == c.bitrate && channels == c.channels && Codec::operator==(c);
}

bool AudioCodec::Matches(const AudioCodec& codec) const {
  // If a nonzero clockrate is specified, it must match the actual clockrate.
  // If a nonzero bitrate is specified, it must match the actual bitrate,
  // unless the codec is VBR (0), where we just force the supplied value.
  // The number of channels must match exactly, with the exception
  // that channels=0 is treated synonymously as channels=1, per RFC
  // 4566 section 6: " [The channels] parameter is OPTIONAL and may be
  // omitted if the number of channels is one."
  // Preference is ignored.
  // TODO(juberti): Treat a zero clockrate as 8000Hz, the RTP default clockrate.
  return Codec::Matches(codec) &&
      ((codec.clockrate == 0 /*&& clockrate == 8000*/) ||
          clockrate == codec.clockrate) &&
      (codec.bitrate == 0 || bitrate <= 0 || bitrate == codec.bitrate) &&
      ((codec.channels < 2 && channels < 2) || channels == codec.channels);
}

std::string AudioCodec::ToString() const {
  std::ostringstream os;
  os << "AudioCodec[" << id << ":" << name << ":" << clockrate << ":" << bitrate
     << ":" << channels << ":" << preference << "]";
  return os.str();
}

std::string VideoCodec::ToString() const {
  std::ostringstream os;
  os << "VideoCodec[" << id << ":" << name << ":" << width << ":" << height
     << ":" << framerate << ":" << preference << "]";
  return os.str();
}

VideoCodec::VideoCodec(int pt,
                       const std::string& nm,
                       int w,
                       int h,
                       int fr,
                       int pr)
    : Codec(pt, nm, kVideoCodecClockrate, pr),
      width(w),
      height(h),
      framerate(fr) {
}

VideoCodec::VideoCodec(int pt, const std::string& nm)
    : Codec(pt, nm, kVideoCodecClockrate, 0),
      width(0),
      height(0),
      framerate(0) {
}

VideoCodec::VideoCodec() : Codec(), width(0), height(0), framerate(0) {
  clockrate = kVideoCodecClockrate;
}

VideoCodec::VideoCodec(const VideoCodec& c) = default;

VideoCodec& VideoCodec::operator=(const VideoCodec& c) {
  Codec::operator=(c);
  width = c.width;
  height = c.height;
  framerate = c.framerate;
  return *this;
}

bool VideoCodec::operator==(const VideoCodec& c) const {
  return width == c.width && height == c.height && framerate == c.framerate &&
         Codec::operator==(c);
}

VideoCodec VideoCodec::CreateRtxCodec(int rtx_payload_type,
                                      int associated_payload_type) {
  VideoCodec rtx_codec(rtx_payload_type, kRtxCodecName, 0, 0, 0, 0);
  rtx_codec.SetParam(kCodecParamAssociatedPayloadType, associated_payload_type);
  return rtx_codec;
}

VideoCodec::CodecType VideoCodec::GetCodecType() const {
  const char* payload_name = name.c_str();
  if (_stricmp(payload_name, kRedCodecName) == 0) {
    return CODEC_RED;
  }
  if (_stricmp(payload_name, kUlpfecCodecName) == 0) {
    return CODEC_ULPFEC;
  }
  if (_stricmp(payload_name, kRtxCodecName) == 0) {
    return CODEC_RTX;
  }

  return CODEC_VIDEO;
}

bool VideoCodec::ValidateCodecFormat() const {
  if (id < 0 || id > 127) {
    LOG(LS_ERROR) << "Codec with invalid payload type: " << ToString();
    return false;
  }
  if (GetCodecType() != CODEC_VIDEO) {
    return true;
  }

  // Video validation from here on.

  if (width <= 0 || height <= 0) {
    LOG(LS_ERROR) << "Codec with invalid dimensions: " << ToString();
    return false;
  }
  int min_bitrate = -1;
  int max_bitrate = -1;
  if (GetParam(kCodecParamMinBitrate, &min_bitrate) &&
      GetParam(kCodecParamMaxBitrate, &max_bitrate)) {
    if (max_bitrate < min_bitrate) {
      LOG(LS_ERROR) << "Codec with max < min bitrate: " << ToString();
      return false;
    }
  }
  return true;
}

DataCodec::DataCodec(int id, const std::string& name, int preference)
    : Codec(id, name, kDataCodecClockrate, preference) {
}

DataCodec::DataCodec() : Codec() {
  clockrate = kDataCodecClockrate;
}

DataCodec::DataCodec(const DataCodec& c) = default;

DataCodec& DataCodec::operator=(const DataCodec& c) = default;

std::string DataCodec::ToString() const {
  std::ostringstream os;
  os << "DataCodec[" << id << ":" << name << "]";
  return os.str();
}

bool HasNack(const VideoCodec& codec) {
  return codec.HasFeedbackParam(
      FeedbackParam(kRtcpFbParamNack, kParamValueEmpty));
}

bool HasRemb(const VideoCodec& codec) {
  return codec.HasFeedbackParam(
      FeedbackParam(kRtcpFbParamRemb, kParamValueEmpty));
}

bool CodecNamesEq(const std::string& name1, const std::string& name2) {
  return _stricmp(name1.c_str(), name2.c_str()) == 0;
}

}  // namespace cricket
