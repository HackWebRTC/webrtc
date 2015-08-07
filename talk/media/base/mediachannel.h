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

namespace cricket {

class AudioRenderer;
struct RtpHeader;
class ScreencastId;
struct VideoFormat;
class VideoCapturer;
class VideoRenderer;

const int kMinRtpHeaderExtensionId = 1;
const int kMaxRtpHeaderExtensionId = 255;
const int kScreencastDefaultFps = 5;
const int kHighStartBitrate = 1500;

// Used in AudioOptions and VideoOptions to signify "unset" values.
template <class T>
class Settable {
 public:
  Settable() : set_(false), val_() {}
  explicit Settable(T val) : set_(true), val_(val) {}

  bool IsSet() const {
    return set_;
  }

  bool Get(T* out) const {
    *out = val_;
    return set_;
  }

  T GetWithDefaultIfUnset(const T& default_value) const {
    return set_ ? val_ : default_value;
  }

  void Set(T val) {
    set_ = true;
    val_ = val;
  }

  void Clear() {
    Set(T());
    set_ = false;
  }

  void SetFrom(const Settable<T>& o) {
    // Set this value based on the value of o, iff o is set.  If this value is
    // set and o is unset, the current value will be unchanged.
    T val;
    if (o.Get(&val)) {
      Set(val);
    }
  }

  std::string ToString() const {
    return set_ ? rtc::ToString(val_) : "";
  }

  bool operator==(const Settable<T>& o) const {
    // Equal if both are unset with any value or both set with the same value.
    return (set_ == o.set_) && (!set_ || (val_ == o.val_));
  }

  bool operator!=(const Settable<T>& o) const {
    return !operator==(o);
  }

 protected:
  void InitializeValue(const T &val) {
    val_ = val;
  }

 private:
  bool set_;
  T val_;
};

template <class T>
static std::string ToStringIfSet(const char* key, const Settable<T>& val) {
  std::string str;
  if (val.IsSet()) {
    str = key;
    str += ": ";
    str += val.ToString();
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
    echo_cancellation.SetFrom(change.echo_cancellation);
    auto_gain_control.SetFrom(change.auto_gain_control);
    rx_auto_gain_control.SetFrom(change.rx_auto_gain_control);
    noise_suppression.SetFrom(change.noise_suppression);
    highpass_filter.SetFrom(change.highpass_filter);
    stereo_swapping.SetFrom(change.stereo_swapping);
    audio_jitter_buffer_max_packets.SetFrom(
        change.audio_jitter_buffer_max_packets);
    audio_jitter_buffer_fast_accelerate.SetFrom(
        change.audio_jitter_buffer_fast_accelerate);
    typing_detection.SetFrom(change.typing_detection);
    aecm_generate_comfort_noise.SetFrom(change.aecm_generate_comfort_noise);
    conference_mode.SetFrom(change.conference_mode);
    adjust_agc_delta.SetFrom(change.adjust_agc_delta);
    experimental_agc.SetFrom(change.experimental_agc);
    extended_filter_aec.SetFrom(change.extended_filter_aec);
    delay_agnostic_aec.SetFrom(change.delay_agnostic_aec);
    experimental_ns.SetFrom(change.experimental_ns);
    aec_dump.SetFrom(change.aec_dump);
    tx_agc_target_dbov.SetFrom(change.tx_agc_target_dbov);
    tx_agc_digital_compression_gain.SetFrom(
        change.tx_agc_digital_compression_gain);
    tx_agc_limiter.SetFrom(change.tx_agc_limiter);
    rx_agc_target_dbov.SetFrom(change.rx_agc_target_dbov);
    rx_agc_digital_compression_gain.SetFrom(
        change.rx_agc_digital_compression_gain);
    rx_agc_limiter.SetFrom(change.rx_agc_limiter);
    recording_sample_rate.SetFrom(change.recording_sample_rate);
    playout_sample_rate.SetFrom(change.playout_sample_rate);
    dscp.SetFrom(change.dscp);
    combined_audio_video_bwe.SetFrom(change.combined_audio_video_bwe);
  }

  bool operator==(const AudioOptions& o) const {
    return echo_cancellation == o.echo_cancellation &&
        auto_gain_control == o.auto_gain_control &&
        rx_auto_gain_control == o.rx_auto_gain_control &&
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
        rx_agc_target_dbov == o.rx_agc_target_dbov &&
        rx_agc_digital_compression_gain == o.rx_agc_digital_compression_gain &&
        rx_agc_limiter == o.rx_agc_limiter &&
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
    ost << ToStringIfSet("rx_agc", rx_auto_gain_control);
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
    ost << ToStringIfSet("rx_agc_target_dbov", rx_agc_target_dbov);
    ost << ToStringIfSet("rx_agc_digital_compression_gain",
        rx_agc_digital_compression_gain);
    ost << ToStringIfSet("rx_agc_limiter", rx_agc_limiter);
    ost << ToStringIfSet("recording_sample_rate", recording_sample_rate);
    ost << ToStringIfSet("playout_sample_rate", playout_sample_rate);
    ost << ToStringIfSet("dscp", dscp);
    ost << ToStringIfSet("combined_audio_video_bwe", combined_audio_video_bwe);
    ost << "}";
    return ost.str();
  }

  // Audio processing that attempts to filter away the output signal from
  // later inbound pickup.
  Settable<bool> echo_cancellation;
  // Audio processing to adjust the sensitivity of the local mic dynamically.
  Settable<bool> auto_gain_control;
  // Audio processing to apply gain to the remote audio.
  Settable<bool> rx_auto_gain_control;
  // Audio processing to filter out background noise.
  Settable<bool> noise_suppression;
  // Audio processing to remove background noise of lower frequencies.
  Settable<bool> highpass_filter;
  // Audio processing to swap the left and right channels.
  Settable<bool> stereo_swapping;
  // Audio receiver jitter buffer (NetEq) max capacity in number of packets.
  Settable<int> audio_jitter_buffer_max_packets;
  // Audio receiver jitter buffer (NetEq) fast accelerate mode.
  Settable<bool> audio_jitter_buffer_fast_accelerate;
  // Audio processing to detect typing.
  Settable<bool> typing_detection;
  Settable<bool> aecm_generate_comfort_noise;
  Settable<bool> conference_mode;
  Settable<int> adjust_agc_delta;
  Settable<bool> experimental_agc;
  Settable<bool> extended_filter_aec;
  Settable<bool> delay_agnostic_aec;
  Settable<bool> experimental_ns;
  Settable<bool> aec_dump;
  // Note that tx_agc_* only applies to non-experimental AGC.
  Settable<uint16> tx_agc_target_dbov;
  Settable<uint16> tx_agc_digital_compression_gain;
  Settable<bool> tx_agc_limiter;
  Settable<uint16> rx_agc_target_dbov;
  Settable<uint16> rx_agc_digital_compression_gain;
  Settable<bool> rx_agc_limiter;
  Settable<uint32> recording_sample_rate;
  Settable<uint32> playout_sample_rate;
  // Set DSCP value for packet sent from audio channel.
  Settable<bool> dscp;
  // Enable combined audio+bandwidth BWE.
  Settable<bool> combined_audio_video_bwe;
};

// Options that can be applied to a VideoMediaChannel or a VideoMediaEngine.
// Used to be flags, but that makes it hard to selectively apply options.
// We are moving all of the setting of options to structs like this,
// but some things currently still use flags.
struct VideoOptions {
  enum HighestBitrate {
    NORMAL,
    HIGH,
    VERY_HIGH
  };

  VideoOptions() {
    process_adaptation_threshhold.Set(kProcessCpuThreshold);
    system_low_adaptation_threshhold.Set(kLowSystemCpuThreshold);
    system_high_adaptation_threshhold.Set(kHighSystemCpuThreshold);
    unsignalled_recv_stream_limit.Set(kNumDefaultUnsignalledVideoRecvStreams);
  }

  void SetAll(const VideoOptions& change) {
    adapt_input_to_cpu_usage.SetFrom(change.adapt_input_to_cpu_usage);
    adapt_cpu_with_smoothing.SetFrom(change.adapt_cpu_with_smoothing);
    video_adapt_third.SetFrom(change.video_adapt_third);
    video_noise_reduction.SetFrom(change.video_noise_reduction);
    video_start_bitrate.SetFrom(change.video_start_bitrate);
    video_highest_bitrate.SetFrom(change.video_highest_bitrate);
    cpu_overuse_detection.SetFrom(change.cpu_overuse_detection);
    cpu_underuse_threshold.SetFrom(change.cpu_underuse_threshold);
    cpu_overuse_threshold.SetFrom(change.cpu_overuse_threshold);
    cpu_underuse_encode_rsd_threshold.SetFrom(
        change.cpu_underuse_encode_rsd_threshold);
    cpu_overuse_encode_rsd_threshold.SetFrom(
        change.cpu_overuse_encode_rsd_threshold);
    cpu_overuse_encode_usage.SetFrom(change.cpu_overuse_encode_usage);
    conference_mode.SetFrom(change.conference_mode);
    process_adaptation_threshhold.SetFrom(change.process_adaptation_threshhold);
    system_low_adaptation_threshhold.SetFrom(
        change.system_low_adaptation_threshhold);
    system_high_adaptation_threshhold.SetFrom(
        change.system_high_adaptation_threshhold);
    dscp.SetFrom(change.dscp);
    suspend_below_min_bitrate.SetFrom(change.suspend_below_min_bitrate);
    unsignalled_recv_stream_limit.SetFrom(change.unsignalled_recv_stream_limit);
    use_simulcast_adapter.SetFrom(change.use_simulcast_adapter);
    screencast_min_bitrate.SetFrom(change.screencast_min_bitrate);
  }

  bool operator==(const VideoOptions& o) const {
    return adapt_input_to_cpu_usage == o.adapt_input_to_cpu_usage &&
           adapt_cpu_with_smoothing == o.adapt_cpu_with_smoothing &&
           video_adapt_third == o.video_adapt_third &&
           video_noise_reduction == o.video_noise_reduction &&
           video_start_bitrate == o.video_start_bitrate &&
           video_highest_bitrate == o.video_highest_bitrate &&
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
           screencast_min_bitrate == o.screencast_min_bitrate;
  }

  std::string ToString() const {
    std::ostringstream ost;
    ost << "VideoOptions {";
    ost << ToStringIfSet("cpu adaption", adapt_input_to_cpu_usage);
    ost << ToStringIfSet("cpu adaptation smoothing", adapt_cpu_with_smoothing);
    ost << ToStringIfSet("video adapt third", video_adapt_third);
    ost << ToStringIfSet("noise reduction", video_noise_reduction);
    ost << ToStringIfSet("start bitrate", video_start_bitrate);
    ost << ToStringIfSet("highest video bitrate", video_highest_bitrate);
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
  Settable<bool> adapt_input_to_cpu_usage;
  // Enable CPU adaptation smoothing?
  Settable<bool> adapt_cpu_with_smoothing;
  // Enable video adapt third?
  Settable<bool> video_adapt_third;
  // Enable denoising?
  Settable<bool> video_noise_reduction;
  // Experimental: Enable WebRtc higher start bitrate?
  Settable<int> video_start_bitrate;
  // Set highest bitrate mode for video.
  Settable<HighestBitrate> video_highest_bitrate;
  // Enable WebRTC Cpu Overuse Detection, which is a new version of the CPU
  // adaptation algorithm. So this option will override the
  // |adapt_input_to_cpu_usage|.
  Settable<bool> cpu_overuse_detection;
  // Low threshold (t1) for cpu overuse adaptation.  (Adapt up)
  // Metric: encode usage (m1). m1 < t1 => underuse.
  Settable<int> cpu_underuse_threshold;
  // High threshold (t1) for cpu overuse adaptation.  (Adapt down)
  // Metric: encode usage (m1). m1 > t1 => overuse.
  Settable<int> cpu_overuse_threshold;
  // Low threshold (t2) for cpu overuse adaptation. (Adapt up)
  // Metric: relative standard deviation of encode time (m2).
  // Optional threshold. If set, (m1 < t1 && m2 < t2) => underuse.
  // Note: t2 will have no effect if t1 is not set.
  Settable<int> cpu_underuse_encode_rsd_threshold;
  // High threshold (t2) for cpu overuse adaptation. (Adapt down)
  // Metric: relative standard deviation of encode time (m2).
  // Optional threshold. If set, (m1 > t1 || m2 > t2) => overuse.
  // Note: t2 will have no effect if t1 is not set.
  Settable<int> cpu_overuse_encode_rsd_threshold;
  // Use encode usage for cpu detection.
  Settable<bool> cpu_overuse_encode_usage;
  // Use conference mode?
  Settable<bool> conference_mode;
  // Threshhold for process cpu adaptation.  (Process limit)
  Settable<float> process_adaptation_threshhold;
  // Low threshhold for cpu adaptation.  (Adapt up)
  Settable<float> system_low_adaptation_threshhold;
  // High threshhold for cpu adaptation.  (Adapt down)
  Settable<float> system_high_adaptation_threshhold;
  // Set DSCP value for packet sent from video channel.
  Settable<bool> dscp;
  // Enable WebRTC suspension of video. No video frames will be sent when the
  // bitrate is below the configured minimum bitrate.
  Settable<bool> suspend_below_min_bitrate;
  // Limit on the number of early receive channels that can be created.
  Settable<int> unsignalled_recv_stream_limit;
  // Enable use of simulcast adapter.
  Settable<bool> use_simulcast_adapter;
  // Force screencast to use a minimum bitrate
  Settable<int> screencast_min_bitrate;
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
    ost << "id: , " << id;
    ost << "uri: " << uri;
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

// DTMF flags to control if a DTMF tone should be played and/or sent.
enum DtmfFlags {
  DF_PLAY = 0x01,
  DF_SEND = 0x02,
};

class MediaChannel : public sigslot::has_slots<> {
 public:
  class NetworkInterface {
   public:
    enum SocketType { ST_RTP, ST_RTCP };
    virtual bool SendPacket(
        rtc::Buffer* packet,
        rtc::DiffServCodePoint dscp = rtc::DSCP_NO_CHANGE) = 0;
    virtual bool SendRtcp(
        rtc::Buffer* packet,
        rtc::DiffServCodePoint dscp = rtc::DSCP_NO_CHANGE) = 0;
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
  virtual bool RemoveSendStream(uint32 ssrc) = 0;
  // Creates a new incoming media stream with SSRCs and CNAME as described
  // by sp.
  virtual bool AddRecvStream(const StreamParams& sp) = 0;
  // Removes an incoming media stream.
  // ssrc must be the first SSRC of the media stream if the stream uses
  // multiple SSRCs.
  virtual bool RemoveRecvStream(uint32 ssrc) = 0;

  // Mutes the channel.
  virtual bool MuteStream(uint32 ssrc, bool on) = 0;

  // Sets the RTP extension headers and IDs to use when sending RTP.
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) = 0;
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) = 0;
  // Returns the absoulte sendtime extension id value from media channel.
  virtual int GetRtpSendTimeExtnId() const {
    return -1;
  }
  // Sets the maximum allowed bandwidth to use when sending data.
  virtual bool SetMaxSendBandwidth(int bps) = 0;

  // Base method to send packet using NetworkInterface.
  bool SendPacket(rtc::Buffer* packet) {
    return DoSendPacket(packet, false);
  }

  bool SendRtcp(rtc::Buffer* packet) {
    return DoSendPacket(packet, true);
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
  bool DoSendPacket(rtc::Buffer* packet, bool rtcp) {
    rtc::CritScope cs(&network_interface_crit_);
    if (!network_interface_)
      return false;

    return (!rtcp) ? network_interface_->SendPacket(packet) :
                     network_interface_->SendRtcp(packet);
  }

  // |network_interface_| can be accessed from the worker_thread and
  // from any MediaEngine threads. This critical section is to protect accessing
  // of network_interface_ object.
  rtc::CriticalSection network_interface_crit_;
  NetworkInterface* network_interface_;
};

enum SendFlags {
  SEND_NOTHING,
  SEND_RINGBACKTONE,
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
  uint32 ssrc;
  double timestamp;  // NTP timestamp, represented as seconds since epoch.
};

struct SsrcReceiverInfo {
  SsrcReceiverInfo()
      : ssrc(0),
        timestamp(0) {
  }
  uint32 ssrc;
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
  void add_ssrc(uint32 ssrc) {
    SsrcSenderInfo stat;
    stat.ssrc = ssrc;
    add_ssrc(stat);
  }
  // Utility accessor for clients that are only interested in ssrc numbers.
  std::vector<uint32> ssrcs() const {
    std::vector<uint32> retval;
    for (std::vector<SsrcSenderInfo>::const_iterator it = local_stats.begin();
         it != local_stats.end(); ++it) {
      retval.push_back(it->ssrc);
    }
    return retval;
  }
  // Utility accessor for clients that make the assumption only one ssrc
  // exists per media.
  // This will eventually go away.
  uint32 ssrc() const {
    if (local_stats.size() > 0) {
      return local_stats[0].ssrc;
    } else {
      return 0;
    }
  }
  int64 bytes_sent;
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
  void add_ssrc(uint32 ssrc) {
    SsrcReceiverInfo stat;
    stat.ssrc = ssrc;
    add_ssrc(stat);
  }
  std::vector<uint32> ssrcs() const {
    std::vector<uint32> retval;
    for (std::vector<SsrcReceiverInfo>::const_iterator it = local_stats.begin();
         it != local_stats.end(); ++it) {
      retval.push_back(it->ssrc);
    }
    return retval;
  }
  // Utility accessor for clients that make the assumption only one ssrc
  // exists per media.
  // This will eventually go away.
  uint32 ssrc() const {
    if (local_stats.size() > 0) {
      return local_stats[0].ssrc;
    } else {
      return 0;
    }
  }

  int64 bytes_rcvd;
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
  int64 capture_start_ntp_time_ms;
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
  int64 capture_start_ntp_time_ms;
};

struct DataSenderInfo : public MediaSenderInfo {
  DataSenderInfo()
      : ssrc(0) {
  }

  uint32 ssrc;
};

struct DataReceiverInfo : public MediaReceiverInfo {
  DataReceiverInfo()
      : ssrc(0) {
  }

  uint32 ssrc;
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

template <class Codec>
struct RtpParameters {
  virtual std::string ToString() {
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
};

template <class Codec, class Options>
struct RtpSendParameters : RtpParameters<Codec> {
  std::string ToString() override {
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
  // TODO(pthatcher): Remove SetSendCodecs,
  // SetSendRtpHeaderExtensions, SetMaxSendBandwidth, and SetOptions
  // once all implementations implement SetSendParameters.
  virtual bool SetSendParameters(const AudioSendParameters& params) {
    return (SetSendCodecs(params.codecs) &&
            SetSendRtpHeaderExtensions(params.extensions) &&
            SetMaxSendBandwidth(params.max_bandwidth_bps) &&
            SetOptions(params.options));
  }
  // TODO(pthatcher): Remove SetRecvCodecs and
  // SetRecvRtpHeaderExtensions once all implementations implement
  // SetRecvParameters.
  virtual bool SetRecvParameters(const AudioRecvParameters& params) {
    return (SetRecvCodecs(params.codecs) &&
            SetRecvRtpHeaderExtensions(params.extensions));
  }
  // Sets the codecs/payload types to be used for incoming media.
  virtual bool SetRecvCodecs(const std::vector<AudioCodec>& codecs) = 0;
  // Sets the codecs/payload types to be used for outgoing media.
  virtual bool SetSendCodecs(const std::vector<AudioCodec>& codecs) = 0;
  // Starts or stops playout of received audio.
  virtual bool SetPlayout(bool playout) = 0;
  // Starts or stops sending (and potentially capture) of local audio.
  virtual bool SetSend(SendFlags flag) = 0;
  // Sets the renderer object to be used for the specified remote audio stream.
  virtual bool SetRemoteRenderer(uint32 ssrc, AudioRenderer* renderer) = 0;
  // Sets the renderer object to be used for the specified local audio stream.
  virtual bool SetLocalRenderer(uint32 ssrc, AudioRenderer* renderer) = 0;
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
  // Set left and right scale for speaker output volume of the specified ssrc.
  virtual bool SetOutputScaling(uint32 ssrc, double left, double right) = 0;
  // Get left and right scale for speaker output volume of the specified ssrc.
  virtual bool GetOutputScaling(uint32 ssrc, double* left, double* right) = 0;
  // Specifies a ringback tone to be played during call setup.
  virtual bool SetRingbackTone(const char *buf, int len) = 0;
  // Plays or stops the aforementioned ringback tone
  virtual bool PlayRingbackTone(uint32 ssrc, bool play, bool loop) = 0;
  // Returns if the telephone-event has been negotiated.
  virtual bool CanInsertDtmf() { return false; }
  // Send and/or play a DTMF |event| according to the |flags|.
  // The DTMF out-of-band signal will be used on sending.
  // The |ssrc| should be either 0 or a valid send stream ssrc.
  // The valid value for the |event| are 0 to 15 which corresponding to
  // DTMF event 0-9, *, #, A-D.
  virtual bool InsertDtmf(uint32 ssrc, int event, int duration, int flags) = 0;
  // Gets quality stats for the channel.
  virtual bool GetStats(VoiceMediaInfo* info) = 0;
  // Gets last reported error for this media channel.
  virtual void GetLastMediaError(uint32* ssrc,
                                 VoiceMediaChannel::Error* error) {
    ASSERT(error != NULL);
    *error = ERROR_NONE;
  }
  // Sets the media options to use.
  virtual bool SetOptions(const AudioOptions& options) = 0;
  virtual bool GetOptions(AudioOptions* options) const = 0;

  // Signal errors from MediaChannel.  Arguments are:
  //     ssrc(uint32), and error(VoiceMediaChannel::Error).
  sigslot::signal2<uint32, VoiceMediaChannel::Error> SignalMediaError;
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
  // Allow video channel to unhook itself from an associated voice channel.
  virtual void DetachVoiceChannel() = 0;
  // TODO(pthatcher): Remove SetSendCodecs,
  // SetSendRtpHeaderExtensions, SetMaxSendBandwidth, and SetOptions
  // once all implementations implement SetSendParameters.
  virtual bool SetSendParameters(const VideoSendParameters& params) {
    return (SetSendCodecs(params.codecs) &&
            SetSendRtpHeaderExtensions(params.extensions) &&
            SetMaxSendBandwidth(params.max_bandwidth_bps) &&
            SetOptions(params.options));
  }
  // TODO(pthatcher): Remove SetRecvCodecs and
  // SetRecvRtpHeaderExtensions once all implementations implement
  // SetRecvParameters.
  virtual bool SetRecvParameters(const VideoRecvParameters& params) {
    return (SetRecvCodecs(params.codecs) &&
            SetRecvRtpHeaderExtensions(params.extensions));
  }
  // Sets the codecs/payload types to be used for incoming media.
  virtual bool SetRecvCodecs(const std::vector<VideoCodec>& codecs) = 0;
  // Sets the codecs/payload types to be used for outgoing media.
  virtual bool SetSendCodecs(const std::vector<VideoCodec>& codecs) = 0;
  // Gets the currently set codecs/payload types to be used for outgoing media.
  virtual bool GetSendCodec(VideoCodec* send_codec) = 0;
  // Sets the format of a specified outgoing stream.
  virtual bool SetSendStreamFormat(uint32 ssrc, const VideoFormat& format) = 0;
  // Starts or stops playout of received video.
  virtual bool SetRender(bool render) = 0;
  // Starts or stops transmission (and potentially capture) of local video.
  virtual bool SetSend(bool send) = 0;
  // Sets the renderer object to be used for the specified stream.
  // If SSRC is 0, the renderer is used for the 'default' stream.
  virtual bool SetRenderer(uint32 ssrc, VideoRenderer* renderer) = 0;
  // If |ssrc| is 0, replace the default capturer (engine capturer) with
  // |capturer|. If |ssrc| is non zero create a new stream with |ssrc| as SSRC.
  virtual bool SetCapturer(uint32 ssrc, VideoCapturer* capturer) = 0;
  // Gets quality stats for the channel.
  virtual bool GetStats(VideoMediaInfo* info) = 0;
  // Send an intra frame to the receivers.
  virtual bool SendIntraFrame() = 0;
  // Reuqest each of the remote senders to send an intra frame.
  virtual bool RequestIntraFrame() = 0;
  // Sets the media options to use.
  virtual bool SetOptions(const VideoOptions& options) = 0;
  virtual bool GetOptions(VideoOptions* options) const = 0;
  virtual void UpdateAspectRatio(int ratio_w, int ratio_h) = 0;

  // Signal errors from MediaChannel.  Arguments are:
  //     ssrc(uint32), and error(VideoMediaChannel::Error).
  sigslot::signal2<uint32, Error> SignalMediaError;

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
  uint32 ssrc;
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
  uint32 ssrc;
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
  std::string ToString() {
    return "{}";
  }
};

struct DataSendParameters : RtpSendParameters<DataCodec, DataOptions> {
  std::string ToString() {
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

  // TODO(pthatcher): Remove SetSendCodecs,
  // SetSendRtpHeaderExtensions, SetMaxSendBandwidth, and SetOptions
  // once all implementations implement SetSendParameters.
  virtual bool SetSendParameters(const DataSendParameters& params) {
    return (SetSendCodecs(params.codecs) &&
            SetMaxSendBandwidth(params.max_bandwidth_bps));
  }
  // TODO(pthatcher): Remove SetRecvCodecs and
  // SetRecvRtpHeaderExtensions once all implementations implement
  // SetRecvParameters.
  virtual bool SetRecvParameters(const DataRecvParameters& params) {
    return SetRecvCodecs(params.codecs);
  }
  virtual bool SetSendCodecs(const std::vector<DataCodec>& codecs) = 0;
  virtual bool SetRecvCodecs(const std::vector<DataCodec>& codecs) = 0;

  virtual bool MuteStream(uint32 ssrc, bool on) { return false; }
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
  // Signal errors from MediaChannel.  Arguments are:
  //     ssrc(uint32), and error(DataMediaChannel::Error).
  sigslot::signal2<uint32, DataMediaChannel::Error> SignalMediaError;
  // Signal when the media channel is ready to send the stream. Arguments are:
  //     writable(bool)
  sigslot::signal1<bool> SignalReadyToSend;
  // Signal for notifying that the remote side has closed the DataChannel.
  sigslot::signal1<uint32> SignalStreamClosedRemotely;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_MEDIACHANNEL_H_
