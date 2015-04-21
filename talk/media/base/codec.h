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

extern const int kMaxPayloadId;

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
  Codec(int id, const std::string& name, int clockrate, int preference);
  // Creates an empty codec.
  Codec();
  Codec(const Codec& c);
  ~Codec();

  // Indicates if this codec is compatible with the specified codec.
  bool Matches(const Codec& codec) const;

  // Find the parameter for |name| and write the value to |out|.
  bool GetParam(const std::string& name, std::string* out) const;
  bool GetParam(const std::string& name, int* out) const;

  void SetParam(const std::string& name, const std::string& value);
  void SetParam(const std::string& name, int value);

  // It is safe to input a non-existent parameter.
  // Returns true if the parameter existed, false if it did not exist.
  bool RemoveParam(const std::string& name);

  bool HasFeedbackParam(const FeedbackParam& param) const;
  void AddFeedbackParam(const FeedbackParam& param);

  static bool Preferable(const Codec& first, const Codec& other) {
    return first.preference > other.preference;
  }

  // Filter |this| feedbacks params such that only those shared by both |this|
  // and |other| are kept.
  void IntersectFeedbackParams(const Codec& other);

  Codec& operator=(const Codec& c);

  bool operator==(const Codec& c) const;

  bool operator!=(const Codec& c) const {
    return !(*this == c);
  }
};

struct AudioCodec : public Codec {
  int bitrate;
  int channels;

  // Creates a codec with the given parameters.
  AudioCodec(int pt, const std::string& nm, int cr, int br, int cs, int pr);
  // Creates an empty codec.
  AudioCodec();
  AudioCodec(const AudioCodec& c);
  ~AudioCodec() = default;

  // Indicates if this codec is compatible with the specified codec.
  bool Matches(const AudioCodec& codec) const;

  static bool Preferable(const AudioCodec& first, const AudioCodec& other) {
    return first.preference > other.preference;
  }

  std::string ToString() const;

  AudioCodec& operator=(const AudioCodec& c);

  bool operator==(const AudioCodec& c) const;

  bool operator!=(const AudioCodec& c) const {
    return !(*this == c);
  }
};

struct VideoCodec : public Codec {
  int width;
  int height;
  int framerate;

  // Creates a codec with the given parameters.
  VideoCodec(int pt, const std::string& nm, int w, int h, int fr, int pr);
  VideoCodec(int pt, const std::string& nm);
  // Creates an empty codec.
  VideoCodec();
  VideoCodec(const VideoCodec& c);
  ~VideoCodec() = default;

  static bool Preferable(const VideoCodec& first, const VideoCodec& other) {
    return first.preference > other.preference;
  }

  std::string ToString() const;

  VideoCodec& operator=(const VideoCodec& c);

  bool operator==(const VideoCodec& c) const;

  bool operator!=(const VideoCodec& c) const {
    return !(*this == c);
  }

  static VideoCodec CreateRtxCodec(int rtx_payload_type,
                                   int associated_payload_type);

  enum CodecType {
    CODEC_VIDEO,
    CODEC_RED,
    CODEC_ULPFEC,
    CODEC_RTX,
  };

  CodecType GetCodecType() const;
  // Validates a VideoCodec's payload type, dimensions and bitrates etc. If they
  // don't make sense (such as max < min bitrate), and error is logged and
  // ValidateCodecFormat returns false.
  bool ValidateCodecFormat() const;
};

struct DataCodec : public Codec {
  DataCodec(int id, const std::string& name, int preference);
  DataCodec();
  DataCodec(const DataCodec& c);

  DataCodec& operator=(const DataCodec& c);

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

// Get the codec setting associated with |payload_type|. If there
// is no codec associated with that payload type it returns false.
template <class Codec>
bool FindCodecById(const std::vector<Codec>& codecs,
                   int payload_type,
                   Codec* codec_out) {
  for (const auto& codec : codecs) {
    if (codec.id == payload_type) {
      *codec_out = codec;
      return true;
    }
  }
  return false;
}

bool CodecNamesEq(const std::string& name1, const std::string& name2);
bool HasNack(const VideoCodec& codec);
bool HasRemb(const VideoCodec& codec);

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_CODEC_H_
