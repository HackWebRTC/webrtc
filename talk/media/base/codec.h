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

#ifndef TALK_MEDIA_BASE_CODEC_H_
#define TALK_MEDIA_BASE_CODEC_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "talk/media/base/constants.h"

namespace cricket {

typedef std::map<std::string, std::string> CodecParameterMap;

class FeedbackParam {
 public:
  FeedbackParam(const std::string& id, const std::string& param)
      : id_(id),
        param_(param) {
  }
  explicit FeedbackParam(const std::string& id)
      : id_(id),
        param_(kParamValueEmpty) {
  }
  bool operator==(const FeedbackParam& other) const;

  const std::string& id() const { return id_; }
  const std::string& param() const { return param_; }

 private:
  std::string id_;  // e.g. "nack", "ccm"
  std::string param_;  // e.g. "", "rpsi", "fir"
};

class FeedbackParams {
 public:
  bool operator==(const FeedbackParams& other) const;

  bool Has(const FeedbackParam& param) const;
  void Add(const FeedbackParam& param);

  void Intersect(const FeedbackParams& from);

  const std::vector<FeedbackParam>& params() const { return params_; }
 private:
  bool HasDuplicateEntries() const;

  std::vector<FeedbackParam> params_;
};

struct Codec {
  int id;
  std::string name;
  int clockrate;
  int preference;
  CodecParameterMap params;
  FeedbackParams feedback_params;

  // Creates a codec with the given parameters.
  Codec(int id, const std::string& name, int clockrate, int preference)
      : id(id),
        name(name),
        clockrate(clockrate),
        preference(preference) {
  }

  // Creates an empty codec.
  Codec() : id(0), clockrate(0), preference(0) {}

  // Indicates if this codec is compatible with the specified codec.
  bool Matches(const Codec& codec) const;

  // Find the parameter for |name| and write the value to |out|.
  bool GetParam(const std::string& name, std::string* out) const;
  bool GetParam(const std::string& name, int* out) const;

  void SetParam(const std::string& name, const std::string& value);
  void SetParam(const std::string& name, int value);

  bool HasFeedbackParam(const FeedbackParam& param) const;
  void AddFeedbackParam(const FeedbackParam& param);

  static bool Preferable(const Codec& first, const Codec& other) {
    return first.preference > other.preference;
  }

  // Filter |this| feedbacks params such that only those shared by both |this|
  // and |other| are kept.
  void IntersectFeedbackParams(const Codec& other);

  Codec& operator=(const Codec& c) {
    this->id = c.id;  // id is reserved in objective-c
    name = c.name;
    clockrate = c.clockrate;
    preference = c.preference;
    return *this;
  }

  bool operator==(const Codec& c) const {
    return this->id == c.id &&  // id is reserved in objective-c
        name == c.name &&
        clockrate == c.clockrate &&
        preference == c.preference;
  }

  bool operator!=(const Codec& c) const {
    return !(*this == c);
  }
};

struct AudioCodec : public Codec {
  int bitrate;
  int channels;

  // Creates a codec with the given parameters.
  AudioCodec(int pt, const std::string& nm, int cr, int br, int cs, int pr)
      : Codec(pt, nm, cr, pr),
        bitrate(br),
        channels(cs) {
  }

  // Creates an empty codec.
  AudioCodec() : Codec(), bitrate(0), channels(0) {}

  // Indicates if this codec is compatible with the specified codec.
  bool Matches(const AudioCodec& codec) const;

  static bool Preferable(const AudioCodec& first, const AudioCodec& other) {
    return first.preference > other.preference;
  }

  std::string ToString() const;

  AudioCodec& operator=(const AudioCodec& c) {
    this->id = c.id;  // id is reserved in objective-c
    name = c.name;
    clockrate = c.clockrate;
    bitrate = c.bitrate;
    channels = c.channels;
    preference =  c.preference;
    params = c.params;
    feedback_params = c.feedback_params;
    return *this;
  }

  bool operator==(const AudioCodec& c) const {
    return this->id == c.id &&  // id is reserved in objective-c
           name == c.name &&
           clockrate == c.clockrate &&
           bitrate == c.bitrate &&
           channels == c.channels &&
           preference == c.preference &&
           params == c.params &&
           feedback_params == c.feedback_params;
  }

  bool operator!=(const AudioCodec& c) const {
    return !(*this == c);
  }
};

struct VideoCodec : public Codec {
  int width;
  int height;
  int framerate;

  // Creates a codec with the given parameters.
  VideoCodec(int pt, const std::string& nm, int w, int h, int fr, int pr)
      : Codec(pt, nm, kVideoCodecClockrate, pr),
        width(w),
        height(h),
        framerate(fr) {
  }

  // Creates an empty codec.
  VideoCodec()
      : Codec(),
        width(0),
        height(0),
        framerate(0) {
    clockrate = kVideoCodecClockrate;
  }

  static bool Preferable(const VideoCodec& first, const VideoCodec& other) {
    return first.preference > other.preference;
  }

  std::string ToString() const;

  VideoCodec& operator=(const VideoCodec& c) {
    this->id = c.id;  // id is reserved in objective-c
    name = c.name;
    clockrate = c.clockrate;
    width = c.width;
    height = c.height;
    framerate = c.framerate;
    preference =  c.preference;
    params = c.params;
    feedback_params = c.feedback_params;
    return *this;
  }

  bool operator==(const VideoCodec& c) const {
    return this->id == c.id &&  // id is reserved in objective-c
           name == c.name &&
           clockrate == c.clockrate &&
           width == c.width &&
           height == c.height &&
           framerate == c.framerate &&
           preference == c.preference &&
           params == c.params &&
           feedback_params == c.feedback_params;
  }

  bool operator!=(const VideoCodec& c) const {
    return !(*this == c);
  }
};

struct DataCodec : public Codec {
  DataCodec(int id, const std::string& name, int preference)
      : Codec(id, name, kDataCodecClockrate, preference) {
  }

  DataCodec() : Codec() {
    clockrate = kDataCodecClockrate;
  }

  std::string ToString() const;
};

struct VideoEncoderConfig {
  static const int kDefaultMaxThreads = -1;
  static const int kDefaultCpuProfile = -1;

  VideoEncoderConfig()
      : max_codec(),
        num_threads(kDefaultMaxThreads),
        cpu_profile(kDefaultCpuProfile) {
  }

  VideoEncoderConfig(const VideoCodec& c)
      : max_codec(c),
        num_threads(kDefaultMaxThreads),
        cpu_profile(kDefaultCpuProfile) {
  }

  VideoEncoderConfig(const VideoCodec& c, int t, int p)
      : max_codec(c),
        num_threads(t),
        cpu_profile(p) {
  }

  VideoEncoderConfig& operator=(const VideoEncoderConfig& config) {
    max_codec = config.max_codec;
    num_threads = config.num_threads;
    cpu_profile = config.cpu_profile;
    return *this;
  }

  bool operator==(const VideoEncoderConfig& config) const {
    return max_codec == config.max_codec &&
           num_threads == config.num_threads &&
           cpu_profile == config.cpu_profile;
  }

  bool operator!=(const VideoEncoderConfig& config) const {
    return !(*this == config);
  }

  VideoCodec max_codec;
  int num_threads;
  int cpu_profile;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_CODEC_H_
