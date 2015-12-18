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

#ifndef TALK_MEDIA_BASE_MEDIACHANNEL_H_
#define TALK_MEDIA_BASE_MEDIACHANNEL_H_

#include <string>
#include <vector>

#include "talk/media/base/codec.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/streamparams.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/optional.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/socket.h"
#include "webrtc/base/window.h"
// TODO(juberti): re-evaluate this include
#include "talk/session/media/audiomonitor.h"

namespace rtc {
class Buffer;
class RateLimiter;
class Timing;
}

namespace webrtc {
class AudioSinkInterface;
}

namespace cricket {

class AudioRenderer;
class ScreencastId;
class VideoCapturer;
class VideoRenderer;
struct RtpHeader;
struct VideoFormat;

const int kMinRtpHeaderExtensionId = 1;
const int kMaxRtpHeaderExtensionId = 255;
const int kScreencastDefaultFps = 5;

template <class T>
static std::string ToStringIfSet(const char* key, const rtc::Optional<T>& val) {
  std::string str;
  if (val) {
    str = key;
    str += ": ";
    str += val ? rtc::ToString(*val) : "";
    str += ", ";
  }
  return str;
}

template <class T>
static std::string VectorToString(const std::vector<T>& vals) {
    std::ostringstream ost;
    ost << "[";
    for (size_t i = 0; i < vals.size(); ++i) {
      if (i > 0) {
        ost << ", ";
      }
      ost << vals[i].ToString();
    }
    ost << "]";
    return ost.str();
}

// Options that can be applied to a VoiceMediaChannel or a VoiceMediaEngine.
// Used to be flags, but that makes it hard to selectively apply options.
// We are moving all of the setting of options to structs like this,
// but some things currently still use flags.
struct AudioOptions {
  void SetAll(const AudioOptions& change) {
    SetFrom(&echo_cancellation, change.echo_cancellation);
    SetFrom(&auto_gain_control, change.auto_gain_control);
    SetFrom(&noise_suppression, change.noise_suppression);
    SetFrom(&highpass_filter, change.highpass_filter);
    SetFrom(&stereo_swapping, change.stereo_swapping);
    SetFrom(&audio_jitter_buffer_max_packets,
            change.audio_jitter_buffer_max_packets);
    SetFrom(&audio_jitter_buffer_fast_accelerate,
            change.audio_jitter_buffer_fast_accelerate);
    SetFrom(&typing_detection, change.typing_detection);
    SetFrom(&aecm_generate_comfort_noise, change.aecm_generate_comfort_noise);
    SetFrom(&conference_mode, change.conference_mode);
    SetFrom(&adjust_agc_delta, change.adjust_agc_delta);
    SetFrom(&experimental_agc, change.experimental_agc);
    SetFrom(&extended_filter_aec, change.extended_filter_aec);
    SetFrom(&delay_agnostic_aec, change.delay_agnostic_aec);
    SetFrom(&experimental_ns, change.experimental_ns);
    SetFrom(&aec_dump, change.aec_dump);
    SetFrom(&tx_agc_target_dbov, change.tx_agc_target_dbov);
    SetFrom(&tx_agc_digital_compression_gain,
            change.tx_agc_digital_compression_gain);
    SetFrom(&tx_agc_limiter, change.tx_agc_limiter);
    SetFrom(&recording_sample_rate, change.recording_sample_rate);
    SetFrom(&playout_sample_rate, change.playout_sample_rate);
    SetFrom(&dscp, change.dscp);
    SetFrom(&combined_audio_video_bwe, change.combined_audio_video_bwe);
  }

  bool operator==(const AudioOptions& o) const {
    return echo_cancellation == o.echo_cancellation &&
        auto_gain_control == o.auto_gain_control &&
        noise_suppression == o.noise_suppression &&
        highpass_filter == o.highpass_filter &&
        stereo_swapping == o.stereo_swapping &&
        audio_jitter_buffer_max_packets == o.audio_jitter_buffer_max_packets &&
        audio_jitter_buffer_fast_accelerate ==
            o.audio_jitter_buffer_fast_accelerate &&
        typing_detection == o.typing_detection &&
        aecm_generate_comfort_noise == o.aecm_generate_comfort_noise &&
        conference_mode == o.conference_mode &&
        experimental_agc == o.experimental_agc &&
        extended_filter_aec == o.extended_filter_aec &&
        delay_agnostic_aec == o.delay_agnostic_aec &&
        experimental_ns == o.experimental_ns &&
        adjust_agc_delta == o.adjust_agc_delta &&
        aec_dump == o.aec_dump &&
        tx_agc_target_dbov == o.tx_agc_target_dbov &&
        tx_agc_digital_compression_gain == o.tx_agc_digital_compression_gain &&
        tx_agc_limiter == o.tx_agc_limiter &&
        recording_sample_rate == o.recording_sample_rate &&
        playout_sample_rate == o.playout_sample_rate &&
        dscp == o.dscp &&
        combined_audio_video_bwe == o.combined_audio_video_bwe;
  }

  std::string ToString() const {
    std::ostringstream ost;
    ost << "AudioOptions {";
    ost << ToStringIfSet("aec", echo_cancellation);
    ost << ToStringIfSet("agc", auto_gain_control);
    ost << ToStringIfSet("ns", noise_suppression);
    ost << ToStringIfSet("hf", highpass_filter);
    ost << ToStringIfSet("swap", stereo_swapping);
    ost << ToStringIfSet("audio_jitter_buffer_max_packets",
                         audio_jitter_buffer_max_packets);
    ost << ToStringIfSet("audio_jitter_buffer_fast_accelerate",
                         audio_jitter_buffer_fast_accelerate);
    ost << ToStringIfSet("typing", typing_detection);
    ost << ToStringIfSet("comfort_noise", aecm_generate_comfort_noise);
    ost << ToStringIfSet("conference", conference_mode);
    ost << ToStringIfSet("agc_delta", adjust_agc_delta);
    ost << ToStringIfSet("experimental_agc", experimental_agc);
    ost << ToStringIfSet("extended_filter_aec", extended_filter_aec);
    ost << ToStringIfSet("delay_agnostic_aec", delay_agnostic_aec);
    ost << ToStringIfSet("experimental_ns", experimental_ns);
    ost << ToStringIfSet("aec_dump", aec_dump);
    ost << ToStringIfSet("tx_agc_target_dbov", tx_agc_target_dbov);
    ost << ToStringIfSet("tx_agc_digital_compression_gain",
        tx_agc_digital_compression_gain);
    ost << ToStringIfSet("tx_agc_limiter", tx_agc_limiter);
    ost << ToStringIfSet("recording_sample_rate", recording_sample_rate);
    ost << ToStringIfSet("playout_sample_rate", playout_sample_rate);
    ost << ToStringIfSet("dscp", dscp);
    ost << ToStringIfSet("combined_audio_video_bwe", combined_audio_video_bwe);
    ost << "}";
    return ost.str();
  }

  // Audio processing that attempts to filter away the output signal from
  // later inbound pickup.
  rtc::Optional<bool> echo_cancellation;
  // Audio processing to adjust the sensitivity of the local mic dynamically.
  rtc::Optional<bool> auto_gain_control;
  // Audio processing to filter out background noise.
  rtc::Optional<bool> noise_suppression;
  // Audio processing to remove background noise of lower frequencies.
  rtc::Optional<bool> highpass_filter;
  // Audio processing to swap the left and right channels.
  rtc::Optional<bool> stereo_swapping;
  // Audio receiver jitter buffer (NetEq) max capacity in number of packets.
  rtc::Optional<int> audio_jitter_buffer_max_packets;
  // Audio receiver jitter buffer (NetEq) fast accelerate mode.
  rtc::Optional<bool> audio_jitter_buffer_fast_accelerate;
  // Audio processing to detect typing.
  rtc::Optional<bool> typing_detection;
  rtc::Optional<bool> aecm_generate_comfort_noise;
  rtc::Optional<bool> conference_mode;
  rtc::Optional<int> adjust_agc_delta;
  rtc::Optional<bool> experimental_agc;
  rtc::Optional<bool> extended_filter_aec;
  rtc::Optional<bool> delay_agnostic_aec;
  rtc::Optional<bool> experimental_ns;
  rtc::Optional<bool> aec_dump;
  // Note that tx_agc_* only applies to non-experimental AGC.
  rtc::Optional<uint16_t> tx_agc_target_dbov;
  rtc::Optional<uint16_t> tx_agc_digital_compression_gain;
  rtc::Optional<bool> tx_agc_limiter;
  rtc::Optional<uint32_t> recording_sample_rate;
  rtc::Optional<uint32_t> playout_sample_rate;
  // Set DSCP value for packet sent from audio channel.
  rtc::Optional<bool> dscp;
  // Enable combined audio+bandwidth BWE.
  rtc::Optional<bool> combined_audio_video_bwe;

 private:
  template <typename T>
  static void SetFrom(rtc::Optional<T>* s, const rtc::Optional<T>& o) {
    if (o) {
      *s = o;
    }
  }
};

// Options that can be applied to a VideoMediaChannel or a VideoMediaEngine.
// Used to be flags, but that makes it hard to selectively apply options.
// We are moving all of the setting of options to structs like this,
// but some things currently still use flags.
struct VideoOptions {
  VideoOptions()
      : process_adaptation_threshhold(kProcessCpuThreshold),
        system_low_adaptation_threshhold(kLowSystemCpuThreshold),
        system_high_adaptation_threshhold(kHighSystemCpuThreshold),
        unsignalled_recv_stream_limit(kNumDefaultUnsignalledVideoRecvStreams) {}

  void SetAll(const VideoOptions& change) {
    SetFrom(&adapt_input_to_cpu_usage, change.adapt_input_to_cpu_usage);
    SetFrom(&adapt_cpu_with_smoothing, change.adapt_cpu_with_smoothing);
    SetFrom(&video_adapt_third, change.video_adapt_third);
    SetFrom(&video_noise_reduction, change.video_noise_reduction);
    SetFrom(&video_start_bitrate, change.video_start_bitrate);
    SetFrom(&cpu_overuse_detection, change.cpu_overuse_detection);
    SetFrom(&cpu_underuse_threshold, change.cpu_underuse_threshold);
    SetFrom(&cpu_overuse_threshold, change.cpu_overuse_threshold);
    SetFrom(&cpu_underuse_encode_rsd_threshold,
            change.cpu_underuse_encode_rsd_threshold);
    SetFrom(&cpu_overuse_encode_rsd_threshold,
            change.cpu_overuse_encode_rsd_threshold);
    SetFrom(&cpu_overuse_encode_usage, change.cpu_overuse_encode_usage);
    SetFrom(&conference_mode, change.conference_mode);
    SetFrom(&process_adaptation_threshhold,
            change.process_adaptation_threshhold);
    SetFrom(&system_low_adaptation_threshhold,
            change.system_low_adaptation_threshhold);
    SetFrom(&system_high_adaptation_threshhold,
            change.system_high_adaptation_threshhold);
    SetFrom(&dscp, change.dscp);
    SetFrom(&suspend_below_min_bitrate, change.suspend_below_min_bitrate);
    SetFrom(&unsignalled_recv_stream_limit,
            change.unsignalled_recv_stream_limit);
    SetFrom(&use_simulcast_adapter, change.use_simulcast_adapter);
    SetFrom(&screencast_min_bitrate, change.screencast_min_bitrate);
    SetFrom(&disable_prerenderer_smoothing,
            change.disable_prerenderer_smoothing);
  }

  bool operator==(const VideoOptions& o) const {
    return adapt_input_to_cpu_usage == o.adapt_input_to_cpu_usage &&
           adapt_cpu_with_smoothing == o.adapt_cpu_with_smoothing &&
           video_adapt_third == o.video_adapt_third &&
           video_noise_reduction == o.video_noise_reduction &&
           video_start_bitrate == o.video_start_bitrate &&
           cpu_overuse_detection == o.cpu_overuse_detection &&
           cpu_underuse_threshold == o.cpu_underuse_threshold &&
           cpu_overuse_threshold == o.cpu_overuse_threshold &&
           cpu_underuse_encode_rsd_threshold ==
               o.cpu_underuse_encode_rsd_threshold &&
           cpu_overuse_encode_rsd_threshold ==
               o.cpu_overuse_encode_rsd_threshold &&
           cpu_overuse_encode_usage == o.cpu_overuse_encode_usage &&
           conference_mode == o.conference_mode &&
           process_adaptation_threshhold == o.process_adaptation_threshhold &&
           system_low_adaptation_threshhold ==
               o.system_low_adaptation_threshhold &&
           system_high_adaptation_threshhold ==
               o.system_high_adaptation_threshhold &&
           dscp == o.dscp &&
           suspend_below_min_bitrate == o.suspend_below_min_bitrate &&
           unsignalled_recv_stream_limit == o.unsignalled_recv_stream_limit &&
           use_simulcast_adapter == o.use_simulcast_adapter &&
           screencast_min_bitrate == o.screencast_min_bitrate &&
           disable_prerenderer_smoothing == o.disable_prerenderer_smoothing;
  }

  std::string ToString() const {
    std::ostringstream ost;
    ost << "VideoOptions {";
    ost << ToStringIfSet("cpu adaption", adapt_input_to_cpu_usage);
    ost << ToStringIfSet("cpu adaptation smoothing", adapt_cpu_with_smoothing);
    ost << ToStringIfSet("video adapt third", video_adapt_third);
    ost << ToStringIfSet("noise reduction", video_noise_reduction);
    ost << ToStringIfSet("start bitrate", video_start_bitrate);
    ost << ToStringIfSet("cpu overuse detection", cpu_overuse_detection);
    ost << ToStringIfSet("cpu underuse threshold", cpu_underuse_threshold);
    ost << ToStringIfSet("cpu overuse threshold", cpu_overuse_threshold);
    ost << ToStringIfSet("cpu underuse encode rsd threshold",
                         cpu_underuse_encode_rsd_threshold);
    ost << ToStringIfSet("cpu overuse encode rsd threshold",
                         cpu_overuse_encode_rsd_threshold);
    ost << ToStringIfSet("cpu overuse encode usage",
                         cpu_overuse_encode_usage);
    ost << ToStringIfSet("conference mode", conference_mode);
    ost << ToStringIfSet("process", process_adaptation_threshhold);
    ost << ToStringIfSet("low", system_low_adaptation_threshhold);
    ost << ToStringIfSet("high", system_high_adaptation_threshhold);
    ost << ToStringIfSet("dscp", dscp);
    ost << ToStringIfSet("suspend below min bitrate",
                         suspend_below_min_bitrate);
    ost << ToStringIfSet("num channels for early receive",
                         unsignalled_recv_stream_limit);
    ost << ToStringIfSet("use simulcast adapter", use_simulcast_adapter);
    ost << ToStringIfSet("screencast min bitrate", screencast_min_bitrate);
    ost << "}";
    return ost.str();
  }

  // Enable CPU adaptation?
  rtc::Optional<bool> adapt_input_to_cpu_usage;
  // Enable CPU adaptation smoothing?
  rtc::Optional<bool> adapt_cpu_with_smoothing;
  // Enable video adapt third?
  rtc::Optional<bool> video_adapt_third;
  // Enable denoising?
  rtc::Optional<bool> video_noise_reduction;
  // Experimental: Enable WebRtc higher start bitrate?
  rtc::Optional<int> video_start_bitrate;
  // Enable WebRTC Cpu Overuse Detection, which is a new version of the CPU
  // adaptation algorithm. So this option will override the
  // |adapt_input_to_cpu_usage|.
  rtc::Optional<bool> cpu_overuse_detection;
  // Low threshold (t1) for cpu overuse adaptation.  (Adapt up)
  // Metric: encode usage (m1). m1 < t1 => underuse.
  rtc::Optional<int> cpu_underuse_threshold;
  // High threshold (t1) for cpu overuse adaptation.  (Adapt down)
  // Metric: encode usage (m1). m1 > t1 => overuse.
  rtc::Optional<int> cpu_overuse_threshold;
  // Low threshold (t2) for cpu overuse adaptation. (Adapt up)
  // Metric: relative standard deviation of encode time (m2).
  // Optional threshold. If set, (m1 < t1 && m2 < t2) => underuse.
  // Note: t2 will have no effect if t1 is not set.
  rtc::Optional<int> cpu_underuse_encode_rsd_threshold;
  // High threshold (t2) for cpu overuse adaptation. (Adapt down)
  // Metric: relative standard deviation of encode time (m2).
  // Optional threshold. If set, (m1 > t1 || m2 > t2) => overuse.
  // Note: t2 will have no effect if t1 is not set.
  rtc::Optional<int> cpu_overuse_encode_rsd_threshold;
  // Use encode usage for cpu detection.
  rtc::Optional<bool> cpu_overuse_encode_usage;
  // Use conference mode?
  rtc::Optional<bool> conference_mode;
  // Threshhold for process cpu adaptation.  (Process limit)
  rtc::Optional<float> process_adaptation_threshhold;
  // Low threshhold for cpu adaptation.  (Adapt up)
  rtc::Optional<float> system_low_adaptation_threshhold;
  // High threshhold for cpu adaptation.  (Adapt down)
  rtc::Optional<float> system_high_adaptation_threshhold;
  // Set DSCP value for packet sent from video channel.
  rtc::Optional<bool> dscp;
  // Enable WebRTC suspension of video. No video frames will be sent when the
  // bitrate is below the configured minimum bitrate.
  rtc::Optional<bool> suspend_below_min_bitrate;
  // Limit on the number of early receive channels that can be created.
  rtc::Optional<int> unsignalled_recv_stream_limit;
  // Enable use of simulcast adapter.
  rtc::Optional<bool> use_simulcast_adapter;
  // Force screencast to use a minimum bitrate
  rtc::Optional<int> screencast_min_bitrate;
  // Set to true if the renderer has an algorithm of frame selection.
  // If the value is true, then WebRTC will hand over a frame as soon as
  // possible without delay, and rendering smoothness is completely the duty
  // of the renderer;
  // If the value is false, then WebRTC is responsible to delay frame release
  // in order to increase rendering smoothness.
  rtc::Optional<bool> disable_prerenderer_smoothing;

 private:
  template <typename T>
  static void SetFrom(rtc::Optional<T>* s, const rtc::Optional<T>& o) {
    if (o) {
      *s = o;
    }
  }
};

struct RtpHeaderExtension {
  RtpHeaderExtension() : id(0) {}
  RtpHeaderExtension(const std::string& u, int i) : uri(u), id(i) {}

  bool operator==(const RtpHeaderExtension& ext) const {
    // id is a reserved word in objective-c. Therefore the id attribute has to
    // be a fully qualified name in order to compile on IOS.
    return this->id == ext.id &&
        uri == ext.uri;
  }

  std::string ToString() const {
    std::ostringstream ost;
    ost << "{";
    ost << "uri: " << uri;
    ost << ", id: " << id;
    ost << "}";
    return ost.str();
  }

  std::string uri;
  int id;
  // TODO(juberti): SendRecv direction;
};

// Returns the named header extension if found among all extensions, NULL
// otherwise.
inline const RtpHeaderExtension* FindHeaderExtension(
    const std::vector<RtpHeaderExtension>& extensions,
    const std::string& name) {
  for (std::vector<RtpHeaderExtension>::const_iterator it = extensions.begin();
       it != extensions.end(); ++it) {
    if (it->uri == name)
      return &(*it);
  }
  return NULL;
}

enum MediaChannelOptions {
  // Tune the stream for conference mode.
  OPT_CONFERENCE = 0x0001
};

enum VoiceMediaChannelOptions {
  // Tune the audio stream for vcs with different target levels.
  OPT_AGC_MINUS_10DB = 0x80000000
};

class MediaChannel : public sigslot::has_slots<> {
 public:
  class NetworkInterface {
   public:
    enum SocketType { ST_RTP, ST_RTCP };
    virtual bool SendPacket(rtc::Buffer* packet,
                            const rtc::PacketOptions& options) = 0;
    virtual bool SendRtcp(rtc::Buffer* packet,
                          const rtc::PacketOptions& options) = 0;
    virtual int SetOption(SocketType type, rtc::Socket::Option opt,
                          int option) = 0;
    virtual ~NetworkInterface() {}
  };

  MediaChannel() : network_interface_(NULL) {}
  virtual ~MediaChannel() {}

  // Sets the abstract interface class for sending RTP/RTCP data.
  virtual void SetInterface(NetworkInterface *iface) {
    rtc::CritScope cs(&network_interface_crit_);
    network_interface_ = iface;
  }

  // Called when a RTP packet is received.
  virtual void OnPacketReceived(rtc::Buffer* packet,
                                const rtc::PacketTime& packet_time) = 0;
  // Called when a RTCP packet is received.
  virtual void OnRtcpReceived(rtc::Buffer* packet,
                              const rtc::PacketTime& packet_time) = 0;
  // Called when the socket's ability to send has changed.
  virtual void OnReadyToSend(bool ready) = 0;
  // Creates a new outgoing media stream with SSRCs and CNAME as described
  // by sp.
  virtual bool AddSendStream(const StreamParams& sp) = 0;
  // Removes an outgoing media stream.
  // ssrc must be the first SSRC of the media stream if the stream uses
  // multiple SSRCs.
  virtual bool RemoveSendStream(uint32_t ssrc) = 0;
  // Creates a new incoming media stream with SSRCs and CNAME as described
  // by sp.
  virtual bool AddRecvStream(const StreamParams& sp) = 0;
  // Removes an incoming media stream.
  // ssrc must be the first SSRC of the media stream if the stream uses
  // multiple SSRCs.
  virtual bool RemoveRecvStream(uint32_t ssrc) = 0;

  // Returns the absoulte sendtime extension id value from media channel.
  virtual int GetRtpSendTimeExtnId() const {
    return -1;
  }

  // Base method to send packet using NetworkInterface.
  bool SendPacket(rtc::Buffer* packet, const rtc::PacketOptions& options) {
    return DoSendPacket(packet, false, options);
  }

  bool SendRtcp(rtc::Buffer* packet, const rtc::PacketOptions& options) {
    return DoSendPacket(packet, true, options);
  }

  int SetOption(NetworkInterface::SocketType type,
                rtc::Socket::Option opt,
                int option) {
    rtc::CritScope cs(&network_interface_crit_);
    if (!network_interface_)
      return -1;

    return network_interface_->SetOption(type, opt, option);
  }

 protected:
  // This method sets DSCP |value| on both RTP and RTCP channels.
  int SetDscp(rtc::DiffServCodePoint value) {
    int ret;
    ret = SetOption(NetworkInterface::ST_RTP,
                    rtc::Socket::OPT_DSCP,
                    value);
    if (ret == 0) {
      ret = SetOption(NetworkInterface::ST_RTCP,
                      rtc::Socket::OPT_DSCP,
                      value);
    }
    return ret;
  }

 private:
  bool DoSendPacket(rtc::Buffer* packet,
                    bool rtcp,
                    const rtc::PacketOptions& options) {
    rtc::CritScope cs(&network_interface_crit_);
    if (!network_interface_)
      return false;

    return (!rtcp) ? network_interface_->SendPacket(packet, options)
                   : network_interface_->SendRtcp(packet, options);
  }

  // |network_interface_| can be accessed from the worker_thread and
  // from any MediaEngine threads. This critical section is to protect accessing
  // of network_interface_ object.
  rtc::CriticalSection network_interface_crit_;
  NetworkInterface* network_interface_;
};

enum SendFlags {
  SEND_NOTHING,
  SEND_MICROPHONE
};

// The stats information is structured as follows:
// Media are represented by either MediaSenderInfo or MediaReceiverInfo.
// Media contains a vector of SSRC infos that are exclusively used by this
// media. (SSRCs shared between media streams can't be represented.)

// Information about an SSRC.
// This data may be locally recorded, or received in an RTCP SR or RR.
struct SsrcSenderInfo {
  SsrcSenderInfo()
      : ssrc(0),
    timestamp(0) {
  }
  uint32_t ssrc;
  double timestamp;  // NTP timestamp, represented as seconds since epoch.
};

struct SsrcReceiverInfo {
  SsrcReceiverInfo()
      : ssrc(0),
        timestamp(0) {
  }
  uint32_t ssrc;
  double timestamp;
};

struct MediaSenderInfo {
  MediaSenderInfo()
      : bytes_sent(0),
        packets_sent(0),
        packets_lost(0),
        fraction_lost(0.0),
        rtt_ms(0) {
  }
  void add_ssrc(const SsrcSenderInfo& stat) {
    local_stats.push_back(stat);
  }
  // Temporary utility function for call sites that only provide SSRC.
  // As more info is added into SsrcSenderInfo, this function should go away.
  void add_ssrc(uint32_t ssrc) {
    SsrcSenderInfo stat;
    stat.ssrc = ssrc;
    add_ssrc(stat);
  }
  // Utility accessor for clients that are only interested in ssrc numbers.
  std::vector<uint32_t> ssrcs() const {
    std::vector<uint32_t> retval;
    for (std::vector<SsrcSenderInfo>::const_iterator it = local_stats.begin();
         it != local_stats.end(); ++it) {
      retval.push_back(it->ssrc);
    }
    return retval;
  }
  // Utility accessor for clients that make the assumption only one ssrc
  // exists per media.
  // This will eventually go away.
  uint32_t ssrc() const {
    if (local_stats.size() > 0) {
      return local_stats[0].ssrc;
    } else {
      return 0;
    }
  }
  int64_t bytes_sent;
  int packets_sent;
  int packets_lost;
  float fraction_lost;
  int64_t rtt_ms;
  std::string codec_name;
  std::vector<SsrcSenderInfo> local_stats;
  std::vector<SsrcReceiverInfo> remote_stats;
};

template<class T>
struct VariableInfo {
  VariableInfo()
      : min_val(),
        mean(0.0),
        max_val(),
        variance(0.0) {
  }
  T min_val;
  double mean;
  T max_val;
  double variance;
};

struct MediaReceiverInfo {
  MediaReceiverInfo()
      : bytes_rcvd(0),
        packets_rcvd(0),
        packets_lost(0),
        fraction_lost(0.0) {
  }
  void add_ssrc(const SsrcReceiverInfo& stat) {
    local_stats.push_back(stat);
  }
  // Temporary utility function for call sites that only provide SSRC.
  // As more info is added into SsrcSenderInfo, this function should go away.
  void add_ssrc(uint32_t ssrc) {
    SsrcReceiverInfo stat;
    stat.ssrc = ssrc;
    add_ssrc(stat);
  }
  std::vector<uint32_t> ssrcs() const {
    std::vector<uint32_t> retval;
    for (std::vector<SsrcReceiverInfo>::const_iterator it = local_stats.begin();
         it != local_stats.end(); ++it) {
      retval.push_back(it->ssrc);
    }
    return retval;
  }
  // Utility accessor for clients that make the assumption only one ssrc
  // exists per media.
  // This will eventually go away.
  uint32_t ssrc() const {
    if (local_stats.size() > 0) {
      return local_stats[0].ssrc;
    } else {
      return 0;
    }
  }

  int64_t bytes_rcvd;
  int packets_rcvd;
  int packets_lost;
  float fraction_lost;
  std::string codec_name;
  std::vector<SsrcReceiverInfo> local_stats;
  std::vector<SsrcSenderInfo> remote_stats;
};

struct VoiceSenderInfo : public MediaSenderInfo {
  VoiceSenderInfo()
      : ext_seqnum(0),
        jitter_ms(0),
        audio_level(0),
        aec_quality_min(0.0),
        echo_delay_median_ms(0),
        echo_delay_std_ms(0),
        echo_return_loss(0),
        echo_return_loss_enhancement(0),
        typing_noise_detected(false) {
  }

  int ext_seqnum;
  int jitter_ms;
  int audio_level;
  float aec_quality_min;
  int echo_delay_median_ms;
  int echo_delay_std_ms;
  int echo_return_loss;
  int echo_return_loss_enhancement;
  bool typing_noise_detected;
};

struct VoiceReceiverInfo : public MediaReceiverInfo {
  VoiceReceiverInfo()
      : ext_seqnum(0),
        jitter_ms(0),
        jitter_buffer_ms(0),
        jitter_buffer_preferred_ms(0),
        delay_estimate_ms(0),
        audio_level(0),
        expand_rate(0),
        speech_expand_rate(0),
        secondary_decoded_rate(0),
        accelerate_rate(0),
        preemptive_expand_rate(0),
        decoding_calls_to_silence_generator(0),
        decoding_calls_to_neteq(0),
        decoding_normal(0),
        decoding_plc(0),
        decoding_cng(0),
        decoding_plc_cng(0),
        capture_start_ntp_time_ms(-1) {}

  int ext_seqnum;
  int jitter_ms;
  int jitter_buffer_ms;
  int jitter_buffer_preferred_ms;
  int delay_estimate_ms;
  int audio_level;
  // fraction of synthesized audio inserted through expansion.
  float expand_rate;
  // fraction of synthesized speech inserted through expansion.
  float speech_expand_rate;
  // fraction of data out of secondary decoding, including FEC and RED.
  float secondary_decoded_rate;
  // Fraction of data removed through time compression.
  float accelerate_rate;
  // Fraction of data inserted through time stretching.
  float preemptive_expand_rate;
  int decoding_calls_to_silence_generator;
  int decoding_calls_to_neteq;
  int decoding_normal;
  int decoding_plc;
  int decoding_cng;
  int decoding_plc_cng;
  // Estimated capture start time in NTP time in ms.
  int64_t capture_start_ntp_time_ms;
};

struct VideoSenderInfo : public MediaSenderInfo {
  VideoSenderInfo()
      : packets_cached(0),
        firs_rcvd(0),
        plis_rcvd(0),
        nacks_rcvd(0),
        input_frame_width(0),
        input_frame_height(0),
        send_frame_width(0),
        send_frame_height(0),
        framerate_input(0),
        framerate_sent(0),
        nominal_bitrate(0),
        preferred_bitrate(0),
        adapt_reason(0),
        adapt_changes(0),
        avg_encode_ms(0),
        encode_usage_percent(0) {
  }

  std::vector<SsrcGroup> ssrc_groups;
  std::string encoder_implementation_name;
  int packets_cached;
  int firs_rcvd;
  int plis_rcvd;
  int nacks_rcvd;
  int input_frame_width;
  int input_frame_height;
  int send_frame_width;
  int send_frame_height;
  int framerate_input;
  int framerate_sent;
  int nominal_bitrate;
  int preferred_bitrate;
  int adapt_reason;
  int adapt_changes;
  int avg_encode_ms;
  int encode_usage_percent;
  VariableInfo<int> adapt_frame_drops;
  VariableInfo<int> effects_frame_drops;
  VariableInfo<double> capturer_frame_time;
};

struct VideoReceiverInfo : public MediaReceiverInfo {
  VideoReceiverInfo()
      : packets_concealed(0),
        firs_sent(0),
        plis_sent(0),
        nacks_sent(0),
        frame_width(0),
        frame_height(0),
        framerate_rcvd(0),
        framerate_decoded(0),
        framerate_output(0),
        framerate_render_input(0),
        framerate_render_output(0),
        decode_ms(0),
        max_decode_ms(0),
        jitter_buffer_ms(0),
        min_playout_delay_ms(0),
        render_delay_ms(0),
        target_delay_ms(0),
        current_delay_ms(0),
        capture_start_ntp_time_ms(-1) {
  }

  std::vector<SsrcGroup> ssrc_groups;
  std::string decoder_implementation_name;
  int packets_concealed;
  int firs_sent;
  int plis_sent;
  int nacks_sent;
  int frame_width;
  int frame_height;
  int framerate_rcvd;
  int framerate_decoded;
  int framerate_output;
  // Framerate as sent to the renderer.
  int framerate_render_input;
  // Framerate that the renderer reports.
  int framerate_render_output;

  // All stats below are gathered per-VideoReceiver, but some will be correlated
  // across MediaStreamTracks.  NOTE(hta): when sinking stats into per-SSRC
  // structures, reflect this in the new layout.

  // Current frame decode latency.
  int decode_ms;
  // Maximum observed frame decode latency.
  int max_decode_ms;
  // Jitter (network-related) latency.
  int jitter_buffer_ms;
  // Requested minimum playout latency.
  int min_playout_delay_ms;
  // Requested latency to account for rendering delay.
  int render_delay_ms;
  // Target overall delay: network+decode+render, accounting for
  // min_playout_delay_ms.
  int target_delay_ms;
  // Current overall delay, possibly ramping towards target_delay_ms.
  int current_delay_ms;

  // Estimated capture start time in NTP time in ms.
  int64_t capture_start_ntp_time_ms;
};

struct DataSenderInfo : public MediaSenderInfo {
  DataSenderInfo()
      : ssrc(0) {
  }

  uint32_t ssrc;
};

struct DataReceiverInfo : public MediaReceiverInfo {
  DataReceiverInfo()
      : ssrc(0) {
  }

  uint32_t ssrc;
};

struct BandwidthEstimationInfo {
  BandwidthEstimationInfo()
      : available_send_bandwidth(0),
        available_recv_bandwidth(0),
        target_enc_bitrate(0),
        actual_enc_bitrate(0),
        retransmit_bitrate(0),
        transmit_bitrate(0),
        bucket_delay(0) {
  }

  int available_send_bandwidth;
  int available_recv_bandwidth;
  int target_enc_bitrate;
  int actual_enc_bitrate;
  int retransmit_bitrate;
  int transmit_bitrate;
  int64_t bucket_delay;
};

struct VoiceMediaInfo {
  void Clear() {
    senders.clear();
    receivers.clear();
  }
  std::vector<VoiceSenderInfo> senders;
  std::vector<VoiceReceiverInfo> receivers;
};

struct VideoMediaInfo {
  void Clear() {
    senders.clear();
    receivers.clear();
    bw_estimations.clear();
  }
  std::vector<VideoSenderInfo> senders;
  std::vector<VideoReceiverInfo> receivers;
  std::vector<BandwidthEstimationInfo> bw_estimations;
};

struct DataMediaInfo {
  void Clear() {
    senders.clear();
    receivers.clear();
  }
  std::vector<DataSenderInfo> senders;
  std::vector<DataReceiverInfo> receivers;
};

struct RtcpParameters {
  bool reduced_size = false;
};

template <class Codec>
struct RtpParameters {
  virtual std::string ToString() const {
    std::ostringstream ost;
    ost << "{";
    ost << "codecs: " << VectorToString(codecs) << ", ";
    ost << "extensions: " << VectorToString(extensions);
    ost << "}";
    return ost.str();
  }

  std::vector<Codec> codecs;
  std::vector<RtpHeaderExtension> extensions;
  // TODO(pthatcher): Add streams.
  RtcpParameters rtcp;
};

template <class Codec, class Options>
struct RtpSendParameters : RtpParameters<Codec> {
  std::string ToString() const override {
    std::ostringstream ost;
    ost << "{";
    ost << "codecs: " << VectorToString(this->codecs) << ", ";
    ost << "extensions: " << VectorToString(this->extensions) << ", ";
    ost << "max_bandiwidth_bps: " << max_bandwidth_bps << ", ";
    ost << "options: " << options.ToString();
    ost << "}";
    return ost.str();
  }

  int max_bandwidth_bps = -1;
  Options options;
};

struct AudioSendParameters : RtpSendParameters<AudioCodec, AudioOptions> {
};

struct AudioRecvParameters : RtpParameters<AudioCodec> {
};

class VoiceMediaChannel : public MediaChannel {
 public:
  enum Error {
    ERROR_NONE = 0,                       // No error.
    ERROR_OTHER,                          // Other errors.
    ERROR_REC_DEVICE_OPEN_FAILED = 100,   // Could not open mic.
    ERROR_REC_DEVICE_MUTED,               // Mic was muted by OS.
    ERROR_REC_DEVICE_SILENT,              // No background noise picked up.
    ERROR_REC_DEVICE_SATURATION,          // Mic input is clipping.
    ERROR_REC_DEVICE_REMOVED,             // Mic was removed while active.
    ERROR_REC_RUNTIME_ERROR,              // Processing is encountering errors.
    ERROR_REC_SRTP_ERROR,                 // Generic SRTP failure.
    ERROR_REC_SRTP_AUTH_FAILED,           // Failed to authenticate packets.
    ERROR_REC_TYPING_NOISE_DETECTED,      // Typing noise is detected.
    ERROR_PLAY_DEVICE_OPEN_FAILED = 200,  // Could not open playout.
    ERROR_PLAY_DEVICE_MUTED,              // Playout muted by OS.
    ERROR_PLAY_DEVICE_REMOVED,            // Playout removed while active.
    ERROR_PLAY_RUNTIME_ERROR,             // Errors in voice processing.
    ERROR_PLAY_SRTP_ERROR,                // Generic SRTP failure.
    ERROR_PLAY_SRTP_AUTH_FAILED,          // Failed to authenticate packets.
    ERROR_PLAY_SRTP_REPLAY,               // Packet replay detected.
  };

  VoiceMediaChannel() {}
  virtual ~VoiceMediaChannel() {}
  virtual bool SetSendParameters(const AudioSendParameters& params) = 0;
  virtual bool SetRecvParameters(const AudioRecvParameters& params) = 0;
  // Starts or stops playout of received audio.
  virtual bool SetPlayout(bool playout) = 0;
  // Starts or stops sending (and potentially capture) of local audio.
  virtual bool SetSend(SendFlags flag) = 0;
  // Configure stream for sending.
  virtual bool SetAudioSend(uint32_t ssrc,
                            bool enable,
                            const AudioOptions* options,
                            AudioRenderer* renderer) = 0;
  // Gets current energy levels for all incoming streams.
  virtual bool GetActiveStreams(AudioInfo::StreamList* actives) = 0;
  // Get the current energy level of the stream sent to the speaker.
  virtual int GetOutputLevel() = 0;
  // Get the time in milliseconds since last recorded keystroke, or negative.
  virtual int GetTimeSinceLastTyping() = 0;
  // Temporarily exposed field for tuning typing detect options.
  virtual void SetTypingDetectionParameters(int time_window,
    int cost_per_typing, int reporting_threshold, int penalty_decay,
    int type_event_delay) = 0;
  // Set speaker output volume of the specified ssrc.
  virtual bool SetOutputVolume(uint32_t ssrc, double volume) = 0;
  // Returns if the telephone-event has been negotiated.
  virtual bool CanInsertDtmf() = 0;
  // Send a DTMF |event|. The DTMF out-of-band signal will be used.
  // The |ssrc| should be either 0 or a valid send stream ssrc.
  // The valid value for the |event| are 0 to 15 which corresponding to
  // DTMF event 0-9, *, #, A-D.
  virtual bool InsertDtmf(uint32_t ssrc, int event, int duration) = 0;
  // Gets quality stats for the channel.
  virtual bool GetStats(VoiceMediaInfo* info) = 0;

  virtual void SetRawAudioSink(
      uint32_t ssrc,
      rtc::scoped_ptr<webrtc::AudioSinkInterface> sink) = 0;
};

struct VideoSendParameters : RtpSendParameters<VideoCodec, VideoOptions> {
};

struct VideoRecvParameters : RtpParameters<VideoCodec> {
};

class VideoMediaChannel : public MediaChannel {
 public:
  enum Error {
    ERROR_NONE = 0,                       // No error.
    ERROR_OTHER,                          // Other errors.
    ERROR_REC_DEVICE_OPEN_FAILED = 100,   // Could not open camera.
    ERROR_REC_DEVICE_NO_DEVICE,           // No camera.
    ERROR_REC_DEVICE_IN_USE,              // Device is in already use.
    ERROR_REC_DEVICE_REMOVED,             // Device is removed.
    ERROR_REC_SRTP_ERROR,                 // Generic sender SRTP failure.
    ERROR_REC_SRTP_AUTH_FAILED,           // Failed to authenticate packets.
    ERROR_REC_CPU_MAX_CANT_DOWNGRADE,     // Can't downgrade capture anymore.
    ERROR_PLAY_SRTP_ERROR = 200,          // Generic receiver SRTP failure.
    ERROR_PLAY_SRTP_AUTH_FAILED,          // Failed to authenticate packets.
    ERROR_PLAY_SRTP_REPLAY,               // Packet replay detected.
  };

  VideoMediaChannel() : renderer_(NULL) {}
  virtual ~VideoMediaChannel() {}

  virtual bool SetSendParameters(const VideoSendParameters& params) = 0;
  virtual bool SetRecvParameters(const VideoRecvParameters& params) = 0;
  // Gets the currently set codecs/payload types to be used for outgoing media.
  virtual bool GetSendCodec(VideoCodec* send_codec) = 0;
  // Sets the format of a specified outgoing stream.
  virtual bool SetSendStreamFormat(uint32_t ssrc,
                                   const VideoFormat& format) = 0;
  // Starts or stops transmission (and potentially capture) of local video.
  virtual bool SetSend(bool send) = 0;
  // Configure stream for sending.
  virtual bool SetVideoSend(uint32_t ssrc,
                            bool enable,
                            const VideoOptions* options) = 0;
  // Sets the renderer object to be used for the specified stream.
  // If SSRC is 0, the renderer is used for the 'default' stream.
  virtual bool SetRenderer(uint32_t ssrc, VideoRenderer* renderer) = 0;
  // If |ssrc| is 0, replace the default capturer (engine capturer) with
  // |capturer|. If |ssrc| is non zero create a new stream with |ssrc| as SSRC.
  virtual bool SetCapturer(uint32_t ssrc, VideoCapturer* capturer) = 0;
  // Gets quality stats for the channel.
  virtual bool GetStats(VideoMediaInfo* info) = 0;
  // Send an intra frame to the receivers.
  virtual bool SendIntraFrame() = 0;
  // Reuqest each of the remote senders to send an intra frame.
  virtual bool RequestIntraFrame() = 0;
  virtual void UpdateAspectRatio(int ratio_w, int ratio_h) = 0;

 protected:
  VideoRenderer *renderer_;
};

enum DataMessageType {
  // Chrome-Internal use only.  See SctpDataMediaChannel for the actual PPID
  // values.
  DMT_NONE = 0,
  DMT_CONTROL = 1,
  DMT_BINARY = 2,
  DMT_TEXT = 3,
};

// Info about data received in DataMediaChannel.  For use in
// DataMediaChannel::SignalDataReceived and in all of the signals that
// signal fires, on up the chain.
struct ReceiveDataParams {
  // The in-packet stream indentifier.
  // For SCTP, this is really SID, not SSRC.
  uint32_t ssrc;
  // The type of message (binary, text, or control).
  DataMessageType type;
  // A per-stream value incremented per packet in the stream.
  int seq_num;
  // A per-stream value monotonically increasing with time.
  int timestamp;

  ReceiveDataParams() :
      ssrc(0),
      type(DMT_TEXT),
      seq_num(0),
      timestamp(0) {
  }
};

struct SendDataParams {
  // The in-packet stream indentifier.
  // For SCTP, this is really SID, not SSRC.
  uint32_t ssrc;
  // The type of message (binary, text, or control).
  DataMessageType type;

  // For SCTP, whether to send messages flagged as ordered or not.
  // If false, messages can be received out of order.
  bool ordered;
  // For SCTP, whether the messages are sent reliably or not.
  // If false, messages may be lost.
  bool reliable;
  // For SCTP, if reliable == false, provide partial reliability by
  // resending up to this many times.  Either count or millis
  // is supported, not both at the same time.
  int max_rtx_count;
  // For SCTP, if reliable == false, provide partial reliability by
  // resending for up to this many milliseconds.  Either count or millis
  // is supported, not both at the same time.
  int max_rtx_ms;

  SendDataParams() :
      ssrc(0),
      type(DMT_TEXT),
      // TODO(pthatcher): Make these true by default?
      ordered(false),
      reliable(false),
      max_rtx_count(0),
      max_rtx_ms(0) {
  }
};

enum SendDataResult { SDR_SUCCESS, SDR_ERROR, SDR_BLOCK };

struct DataOptions {
  std::string ToString() const {
    return "{}";
  }
};

struct DataSendParameters : RtpSendParameters<DataCodec, DataOptions> {
  std::string ToString() const {
    std::ostringstream ost;
    // Options and extensions aren't used.
    ost << "{";
    ost << "codecs: " << VectorToString(codecs) << ", ";
    ost << "max_bandiwidth_bps: " << max_bandwidth_bps;
    ost << "}";
    return ost.str();
  }
};

struct DataRecvParameters : RtpParameters<DataCodec> {
};

class DataMediaChannel : public MediaChannel {
 public:
  enum Error {
    ERROR_NONE = 0,                       // No error.
    ERROR_OTHER,                          // Other errors.
    ERROR_SEND_SRTP_ERROR = 200,          // Generic SRTP failure.
    ERROR_SEND_SRTP_AUTH_FAILED,          // Failed to authenticate packets.
    ERROR_RECV_SRTP_ERROR,                // Generic SRTP failure.
    ERROR_RECV_SRTP_AUTH_FAILED,          // Failed to authenticate packets.
    ERROR_RECV_SRTP_REPLAY,               // Packet replay detected.
  };

  virtual ~DataMediaChannel() {}

  virtual bool SetSendParameters(const DataSendParameters& params) = 0;
  virtual bool SetRecvParameters(const DataRecvParameters& params) = 0;

  // TODO(pthatcher): Implement this.
  virtual bool GetStats(DataMediaInfo* info) { return true; }

  virtual bool SetSend(bool send) = 0;
  virtual bool SetReceive(bool receive) = 0;

  virtual bool SendData(
      const SendDataParams& params,
      const rtc::Buffer& payload,
      SendDataResult* result = NULL) = 0;
  // Signals when data is received (params, data, len)
  sigslot::signal3<const ReceiveDataParams&,
                   const char*,
                   size_t> SignalDataReceived;
  // Signal when the media channel is ready to send the stream. Arguments are:
  //     writable(bool)
  sigslot::signal1<bool> SignalReadyToSend;
  // Signal for notifying that the remote side has closed the DataChannel.
  sigslot::signal1<uint32_t> SignalStreamClosedRemotely;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_MEDIACHANNEL_H_
