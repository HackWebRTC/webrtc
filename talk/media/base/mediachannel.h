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

#include "talk/base/basictypes.h"
#include "talk/base/buffer.h"
#include "talk/base/dscp.h"
#include "talk/base/logging.h"
#include "talk/base/sigslot.h"
#include "talk/base/socket.h"
#include "talk/base/window.h"
#include "talk/media/base/codec.h"
#include "talk/media/base/constants.h"
#include "talk/media/base/streamparams.h"
// TODO(juberti): re-evaluate this include
#include "talk/session/media/audiomonitor.h"

namespace talk_base {
class Buffer;
class RateLimiter;
class Timing;
}

namespace webrtc {
struct DataChannelInit;
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

  virtual void Set(T val) {
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
    return set_ ? talk_base::ToString(val_) : "";
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

class SettablePercent : public Settable<float> {
 public:
  virtual void Set(float val) {
    if (val < 0) {
      val = 0;
    }
    if (val >  1.0) {
      val = 1.0;
    }
    Settable<float>::Set(val);
  }
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
    typing_detection.SetFrom(change.typing_detection);
    aecm_generate_comfort_noise.SetFrom(change.aecm_generate_comfort_noise);
    conference_mode.SetFrom(change.conference_mode);
    adjust_agc_delta.SetFrom(change.adjust_agc_delta);
    experimental_agc.SetFrom(change.experimental_agc);
    experimental_aec.SetFrom(change.experimental_aec);
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
  }

  bool operator==(const AudioOptions& o) const {
    return echo_cancellation == o.echo_cancellation &&
        auto_gain_control == o.auto_gain_control &&
        rx_auto_gain_control == o.rx_auto_gain_control &&
        noise_suppression == o.noise_suppression &&
        highpass_filter == o.highpass_filter &&
        stereo_swapping == o.stereo_swapping &&
        typing_detection == o.typing_detection &&
        aecm_generate_comfort_noise == o.aecm_generate_comfort_noise &&
        conference_mode == o.conference_mode &&
        experimental_agc == o.experimental_agc &&
        experimental_aec == o.experimental_aec &&
        adjust_agc_delta == o.adjust_agc_delta &&
        aec_dump == o.aec_dump &&
        tx_agc_target_dbov == o.tx_agc_target_dbov &&
        tx_agc_digital_compression_gain == o.tx_agc_digital_compression_gain &&
        tx_agc_limiter == o.tx_agc_limiter &&
        rx_agc_target_dbov == o.rx_agc_target_dbov &&
        rx_agc_digital_compression_gain == o.rx_agc_digital_compression_gain &&
        rx_agc_limiter == o.rx_agc_limiter &&
        recording_sample_rate == o.recording_sample_rate &&
        playout_sample_rate == o.playout_sample_rate;
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
    ost << ToStringIfSet("typing", typing_detection);
    ost << ToStringIfSet("comfort_noise", aecm_generate_comfort_noise);
    ost << ToStringIfSet("conference", conference_mode);
    ost << ToStringIfSet("agc_delta", adjust_agc_delta);
    ost << ToStringIfSet("experimental_agc", experimental_agc);
    ost << ToStringIfSet("experimental_aec", experimental_aec);
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
  // Audio processing to detect typing.
  Settable<bool> typing_detection;
  Settable<bool> aecm_generate_comfort_noise;
  Settable<bool> conference_mode;
  Settable<int> adjust_agc_delta;
  Settable<bool> experimental_agc;
  Settable<bool> experimental_aec;
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
};

// Options that can be applied to a VideoMediaChannel or a VideoMediaEngine.
// Used to be flags, but that makes it hard to selectively apply options.
// We are moving all of the setting of options to structs like this,
// but some things currently still use flags.
struct VideoOptions {
  VideoOptions() {
    process_adaptation_threshhold.Set(kProcessCpuThreshold);
    system_low_adaptation_threshhold.Set(kLowSystemCpuThreshold);
    system_high_adaptation_threshhold.Set(kHighSystemCpuThreshold);
  }

  void SetAll(const VideoOptions& change) {
    adapt_input_to_encoder.SetFrom(change.adapt_input_to_encoder);
    adapt_input_to_cpu_usage.SetFrom(change.adapt_input_to_cpu_usage);
    adapt_cpu_with_smoothing.SetFrom(change.adapt_cpu_with_smoothing);
    adapt_view_switch.SetFrom(change.adapt_view_switch);
    video_adapt_third.SetFrom(change.video_adapt_third);
    video_noise_reduction.SetFrom(change.video_noise_reduction);
    video_three_layers.SetFrom(change.video_three_layers);
    video_one_layer_screencast.SetFrom(change.video_one_layer_screencast);
    video_high_bitrate.SetFrom(change.video_high_bitrate);
    video_watermark.SetFrom(change.video_watermark);
    video_temporal_layer_screencast.SetFrom(
        change.video_temporal_layer_screencast);
    video_temporal_layer_realtime.SetFrom(
        change.video_temporal_layer_realtime);
    video_leaky_bucket.SetFrom(change.video_leaky_bucket);
    cpu_overuse_detection.SetFrom(change.cpu_overuse_detection);
    conference_mode.SetFrom(change.conference_mode);
    process_adaptation_threshhold.SetFrom(change.process_adaptation_threshhold);
    system_low_adaptation_threshhold.SetFrom(
        change.system_low_adaptation_threshhold);
    system_high_adaptation_threshhold.SetFrom(
        change.system_high_adaptation_threshhold);
    buffered_mode_latency.SetFrom(change.buffered_mode_latency);
  }

  bool operator==(const VideoOptions& o) const {
    return adapt_input_to_encoder == o.adapt_input_to_encoder &&
        adapt_input_to_cpu_usage == o.adapt_input_to_cpu_usage &&
        adapt_cpu_with_smoothing == o.adapt_cpu_with_smoothing &&
        adapt_view_switch == o.adapt_view_switch &&
        video_adapt_third == o.video_adapt_third &&
        video_noise_reduction == o.video_noise_reduction &&
        video_three_layers == o.video_three_layers &&
        video_one_layer_screencast == o.video_one_layer_screencast &&
        video_high_bitrate == o.video_high_bitrate &&
        video_watermark == o.video_watermark &&
        video_temporal_layer_screencast == o.video_temporal_layer_screencast &&
        video_temporal_layer_realtime == o.video_temporal_layer_realtime &&
        video_leaky_bucket == o.video_leaky_bucket &&
        cpu_overuse_detection == o.cpu_overuse_detection &&
        conference_mode == o.conference_mode &&
        process_adaptation_threshhold == o.process_adaptation_threshhold &&
        system_low_adaptation_threshhold ==
            o.system_low_adaptation_threshhold &&
        system_high_adaptation_threshhold ==
            o.system_high_adaptation_threshhold &&
        buffered_mode_latency == o.buffered_mode_latency;
  }

  std::string ToString() const {
    std::ostringstream ost;
    ost << "VideoOptions {";
    ost << ToStringIfSet("encoder adaption", adapt_input_to_encoder);
    ost << ToStringIfSet("cpu adaption", adapt_input_to_cpu_usage);
    ost << ToStringIfSet("cpu adaptation smoothing", adapt_cpu_with_smoothing);
    ost << ToStringIfSet("adapt view switch", adapt_view_switch);
    ost << ToStringIfSet("video adapt third", video_adapt_third);
    ost << ToStringIfSet("noise reduction", video_noise_reduction);
    ost << ToStringIfSet("3 layers", video_three_layers);
    ost << ToStringIfSet("1 layer screencast", video_one_layer_screencast);
    ost << ToStringIfSet("high bitrate", video_high_bitrate);
    ost << ToStringIfSet("watermark", video_watermark);
    ost << ToStringIfSet("video temporal layer screencast",
                         video_temporal_layer_screencast);
    ost << ToStringIfSet("video temporal layer realtime",
                         video_temporal_layer_realtime);
    ost << ToStringIfSet("leaky bucket", video_leaky_bucket);
    ost << ToStringIfSet("cpu overuse detection", cpu_overuse_detection);
    ost << ToStringIfSet("conference mode", conference_mode);
    ost << ToStringIfSet("process", process_adaptation_threshhold);
    ost << ToStringIfSet("low", system_low_adaptation_threshhold);
    ost << ToStringIfSet("high", system_high_adaptation_threshhold);
    ost << ToStringIfSet("buffered mode latency", buffered_mode_latency);
    ost << "}";
    return ost.str();
  }

  // Encoder adaption, which is the gd callback in LMI, and TBA in WebRTC.
  Settable<bool> adapt_input_to_encoder;
  // Enable CPU adaptation?
  Settable<bool> adapt_input_to_cpu_usage;
  // Enable CPU adaptation smoothing?
  Settable<bool> adapt_cpu_with_smoothing;
  // Enable Adapt View Switch?
  Settable<bool> adapt_view_switch;
  // Enable video adapt third?
  Settable<bool> video_adapt_third;
  // Enable denoising?
  Settable<bool> video_noise_reduction;
  // Experimental: Enable multi layer?
  Settable<bool> video_three_layers;
  // Experimental: Enable one layer screencast?
  Settable<bool> video_one_layer_screencast;
  // Experimental: Enable WebRtc higher bitrate?
  Settable<bool> video_high_bitrate;
  // Experimental: Add watermark to the rendered video image.
  Settable<bool> video_watermark;
  // Experimental: Enable WebRTC layered screencast.
  Settable<bool> video_temporal_layer_screencast;
  // Experimental: Enable WebRTC temporal layer strategy for realtime video.
  Settable<bool> video_temporal_layer_realtime;
  // Enable WebRTC leaky bucket when sending media packets.
  Settable<bool> video_leaky_bucket;
  // Enable WebRTC Cpu Overuse Detection, which is a new version of the CPU
  // adaptation algorithm. So this option will override the
  // |adapt_input_to_cpu_usage|.
  Settable<bool> cpu_overuse_detection;
  // Use conference mode?
  Settable<bool> conference_mode;
  // Threshhold for process cpu adaptation.  (Process limit)
  SettablePercent process_adaptation_threshhold;
  // Low threshhold for cpu adaptation.  (Adapt up)
  SettablePercent system_low_adaptation_threshhold;
  // High threshhold for cpu adaptation.  (Adapt down)
  SettablePercent system_high_adaptation_threshhold;
  // Specify buffered mode latency in milliseconds.
  Settable<int> buffered_mode_latency;
};

// A class for playing out soundclips.
class SoundclipMedia {
 public:
  enum SoundclipFlags {
    SF_LOOP = 1,
  };

  virtual ~SoundclipMedia() {}

  // Plays a sound out to the speakers with the given audio stream. The stream
  // must be 16-bit little-endian 16 kHz PCM. If a stream is already playing
  // on this SoundclipMedia, it is stopped. If clip is NULL, nothing is played.
  // Returns whether it was successful.
  virtual bool PlaySound(const char *clip, int len, int flags) = 0;
};

struct RtpHeaderExtension {
  RtpHeaderExtension() : id(0) {}
  RtpHeaderExtension(const std::string& u, int i) : uri(u), id(i) {}
  std::string uri;
  int id;
  // TODO(juberti): SendRecv direction;

  bool operator==(const RtpHeaderExtension& ext) const {
    // id is a reserved word in objective-c. Therefore the id attribute has to
    // be a fully qualified name in order to compile on IOS.
    return this->id == ext.id &&
        uri == ext.uri;
  }
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
        talk_base::Buffer* packet,
        talk_base::DiffServCodePoint dscp = talk_base::DSCP_NO_CHANGE) = 0;
    virtual bool SendRtcp(
        talk_base::Buffer* packet,
        talk_base::DiffServCodePoint dscp = talk_base::DSCP_NO_CHANGE) = 0;
    virtual int SetOption(SocketType type, talk_base::Socket::Option opt,
                          int option) = 0;
    virtual ~NetworkInterface() {}
  };

  MediaChannel() : network_interface_(NULL) {}
  virtual ~MediaChannel() {}

  // Sets the abstract interface class for sending RTP/RTCP data.
  virtual void SetInterface(NetworkInterface *iface) {
    talk_base::CritScope cs(&network_interface_crit_);
    network_interface_ = iface;
  }

  // Called when a RTP packet is received.
  virtual void OnPacketReceived(talk_base::Buffer* packet) = 0;
  // Called when a RTCP packet is received.
  virtual void OnRtcpReceived(talk_base::Buffer* packet) = 0;
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
  // Sets the rate control to use when sending data.
  virtual bool SetSendBandwidth(bool autobw, int bps) = 0;

  // Base method to send packet using NetworkInterface.
  bool SendPacket(talk_base::Buffer* packet) {
    return DoSendPacket(packet, false);
  }

  bool SendRtcp(talk_base::Buffer* packet) {
    return DoSendPacket(packet, true);
  }

  int SetOption(NetworkInterface::SocketType type,
                talk_base::Socket::Option opt,
                int option) {
    talk_base::CritScope cs(&network_interface_crit_);
    if (!network_interface_)
      return -1;

    return network_interface_->SetOption(type, opt, option);
  }

 private:
  bool DoSendPacket(talk_base::Buffer* packet, bool rtcp) {
    talk_base::CritScope cs(&network_interface_crit_);
    if (!network_interface_)
      return false;

    return (!rtcp) ? network_interface_->SendPacket(packet) :
                     network_interface_->SendRtcp(packet);
  }

  // |network_interface_| can be accessed from the worker_thread and
  // from any MediaEngine threads. This critical section is to protect accessing
  // of network_interface_ object.
  talk_base::CriticalSection network_interface_crit_;
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
  int64 bytes_sent;
  int packets_sent;
  int packets_lost;
  float fraction_lost;
  int rtt_ms;
  std::string codec_name;
  std::vector<SsrcSenderInfo> local_stats;
  std::vector<SsrcReceiverInfo> remote_stats;
};

struct MediaReceiverInfo {
  MediaReceiverInfo()
      : bytes_rcvd(0),
        packets_rcvd(0),
        packets_lost(0),
        fraction_lost(0.0) {
  }
  int64 bytes_rcvd;
  int packets_rcvd;
  int packets_lost;
  float fraction_lost;
  std::vector<SsrcReceiverInfo> local_stats;
  std::vector<SsrcSenderInfo> remote_stats;
};

struct VoiceSenderInfo : public MediaSenderInfo {
  VoiceSenderInfo()
      : ssrc(0),
        ext_seqnum(0),
        jitter_ms(0),
        audio_level(0),
        aec_quality_min(0.0),
        echo_delay_median_ms(0),
        echo_delay_std_ms(0),
        echo_return_loss(0),
        echo_return_loss_enhancement(0),
        typing_noise_detected(false) {
  }

  uint32 ssrc;
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
      : ssrc(0),
        ext_seqnum(0),
        jitter_ms(0),
        jitter_buffer_ms(0),
        jitter_buffer_preferred_ms(0),
        delay_estimate_ms(0),
        audio_level(0),
        expand_rate(0) {
  }

  uint32 ssrc;
  int ext_seqnum;
  int jitter_ms;
  int jitter_buffer_ms;
  int jitter_buffer_preferred_ms;
  int delay_estimate_ms;
  int audio_level;
  // fraction of synthesized speech inserted through pre-emptive expansion
  float expand_rate;
};

struct VideoSenderInfo : public MediaSenderInfo {
  VideoSenderInfo()
      : packets_cached(0),
        firs_rcvd(0),
        nacks_rcvd(0),
        frame_width(0),
        frame_height(0),
        framerate_input(0),
        framerate_sent(0),
        nominal_bitrate(0),
        preferred_bitrate(0),
        adapt_reason(0) {
  }

  std::vector<uint32> ssrcs;
  std::vector<SsrcGroup> ssrc_groups;
  int packets_cached;
  int firs_rcvd;
  int nacks_rcvd;
  int frame_width;
  int frame_height;
  int framerate_input;
  int framerate_sent;
  int nominal_bitrate;
  int preferred_bitrate;
  int adapt_reason;
};

struct VideoReceiverInfo : public MediaReceiverInfo {
  VideoReceiverInfo()
      : packets_concealed(0),
        firs_sent(0),
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
        current_delay_ms(0) {
  }

  std::vector<uint32> ssrcs;
  std::vector<SsrcGroup> ssrc_groups;
  int packets_concealed;
  int firs_sent;
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
  int bucket_delay;
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

  virtual bool SetSendBandwidth(bool autobw, int bps) = 0;
  virtual bool SetSendCodecs(const std::vector<DataCodec>& codecs) = 0;
  virtual bool SetRecvCodecs(const std::vector<DataCodec>& codecs) = 0;
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) = 0;
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) = 0;
  virtual bool AddSendStream(const StreamParams& sp) = 0;
  virtual bool RemoveSendStream(uint32 ssrc) = 0;
  virtual bool AddRecvStream(const StreamParams& sp) = 0;
  virtual bool RemoveRecvStream(uint32 ssrc) = 0;
  virtual bool MuteStream(uint32 ssrc, bool on) { return false; }
  // TODO(pthatcher): Implement this.
  virtual bool GetStats(DataMediaInfo* info) { return true; }

  virtual bool SetSend(bool send) = 0;
  virtual bool SetReceive(bool receive) = 0;
  virtual void OnPacketReceived(talk_base::Buffer* packet) = 0;
  virtual void OnRtcpReceived(talk_base::Buffer* packet) = 0;

  virtual bool SendData(
      const SendDataParams& params,
      const talk_base::Buffer& payload,
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
  // Signal for notifying when a new stream is added from the remote side. Used
  // for the in-band negotioation through the OPEN message for SCTP data
  // channel.
  sigslot::signal2<const std::string&, const webrtc::DataChannelInit&>
      SignalNewStreamReceived;
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_MEDIACHANNEL_H_
