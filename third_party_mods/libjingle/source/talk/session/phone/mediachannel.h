/*
 * libjingle
 * Copyright 2004--2010, Google Inc.
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

#ifndef TALK_SESSION_PHONE_MEDIACHANNEL_H_
#define TALK_SESSION_PHONE_MEDIACHANNEL_H_

#include <string>
#include <vector>

#include "talk/base/basictypes.h"
#include "talk/base/sigslot.h"
#include "talk/base/socket.h"
#include "talk/session/phone/codec.h"
// TODO: re-evaluate this include
#include "talk/session/phone/audiomonitor.h"

namespace talk_base {
class Buffer;
}

namespace flute {
class MagicCamVideoRenderer;
}

namespace cricket {

const int kMinRtpHeaderExtensionId = 1;
const int kMaxRtpHeaderExtensionId = 255;

struct RtpHeaderExtension {
  RtpHeaderExtension(const std::string& u, int i) : uri(u), id(i) {}
  std::string uri;
  int id;
  // TODO: SendRecv direction;
};

enum VoiceMediaChannelOptions {
  OPT_CONFERENCE = 0x10000,   // tune the audio stream for conference mode

};

enum VideoMediaChannelOptions {
  OPT_INTERPOLATE = 0x10000   // Increase the output framerate by 2x by
                              // interpolating frames
};

class MediaChannel : public sigslot::has_slots<> {
 public:
  class NetworkInterface {
   public:
    enum SocketType { ST_RTP, ST_RTCP };
    virtual bool SendPacket(talk_base::Buffer* packet) = 0;
    virtual bool SendRtcp(talk_base::Buffer* packet) = 0;
    virtual int SetOption(SocketType type, talk_base::Socket::Option opt,
                          int option) = 0;
    virtual ~NetworkInterface() {}
  };

  MediaChannel() : network_interface_(NULL) {}
  virtual ~MediaChannel() {}

  // Gets/sets the abstract inteface class for sending RTP/RTCP data.
  NetworkInterface *network_interface() { return network_interface_; }
  virtual void SetInterface(NetworkInterface *iface) {
    network_interface_ = iface;
  }

  // Called when a RTP packet is received.
  virtual void OnPacketReceived(talk_base::Buffer* packet) = 0;
  // Called when a RTCP packet is received.
  virtual void OnRtcpReceived(talk_base::Buffer* packet) = 0;
  // Sets the SSRC to be used for outgoing data.
  virtual void SetSendSsrc(uint32 id) = 0;
  // Set the CNAME of RTCP
  virtual bool SetRtcpCName(const std::string& cname) = 0;
  // Mutes the channel.
  virtual bool Mute(bool on) = 0;

  // Sets the RTP extension headers and IDs to use when sending RTP.
  virtual bool SetRecvRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) = 0;
  virtual bool SetSendRtpHeaderExtensions(
      const std::vector<RtpHeaderExtension>& extensions) = 0;
  // Sets the rate control to use when sending data.
  virtual bool SetSendBandwidth(bool autobw, int bps) = 0;
  // Sets the media options to use.
  virtual bool SetOptions(int options) = 0;
  // Gets the Rtc channel id
  virtual int GetMediaChannelId() = 0;

 protected:
  NetworkInterface *network_interface_;
};

enum SendFlags {
  SEND_NOTHING,
  SEND_RINGBACKTONE,
  SEND_MICROPHONE
};

struct VoiceSenderInfo {
  uint32 ssrc;
  int bytes_sent;
  int packets_sent;
  int packets_lost;
  float fraction_lost;
  int ext_seqnum;
  int rtt_ms;
  int jitter_ms;
  int audio_level;
};

struct VoiceReceiverInfo {
  uint32 ssrc;
  int bytes_rcvd;
  int packets_rcvd;
  int packets_lost;
  float fraction_lost;
  int ext_seqnum;
  int jitter_ms;
  int jitter_buffer_ms;
  int jitter_buffer_preferred_ms;
  int delay_estimate_ms;
  int audio_level;
};

struct VideoSenderInfo {
  uint32 ssrc;
  int bytes_sent;
  int packets_sent;
  int packets_cached;
  int packets_lost;
  float fraction_lost;
  int firs_rcvd;
  int nacks_rcvd;
  int rtt_ms;
  int frame_width;
  int frame_height;
  int framerate_input;
  int framerate_sent;
  int nominal_bitrate;
  int preferred_bitrate;
};

struct VideoReceiverInfo {
  uint32 ssrc;
  int bytes_rcvd;
  // vector<int> layer_bytes_rcvd;
  int packets_rcvd;
  int packets_lost;
  int packets_concealed;
  float fraction_lost;
  int firs_sent;
  int nacks_sent;
  int frame_width;
  int frame_height;
  int framerate_rcvd;
  int framerate_decoded;
  int framerate_output;
};

struct BandwidthEstimationInfo {
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
  // Adds a new receive-only stream with the specified SSRC.
  virtual bool AddStream(uint32 ssrc) = 0;
  // Removes a stream added with AddStream.
  virtual bool RemoveStream(uint32 ssrc) = 0;
  // Gets current energy levels for all incoming streams.
  virtual bool GetActiveStreams(AudioInfo::StreamList* actives) = 0;
  // Get the current energy level for the outgoing stream.
  virtual int GetOutputLevel() = 0;
  // Specifies a ringback tone to be played during call setup.
  virtual bool SetRingbackTone(const char *buf, int len) = 0;
  // Plays or stops the aforementioned ringback tone
  virtual bool PlayRingbackTone(uint32 ssrc, bool play, bool loop) = 0;
  // Sends a out-of-band DTMF signal using the specified event.
  virtual bool PressDTMF(int event, bool playout) = 0;
  // Gets quality stats for the channel.
  virtual bool GetStats(VoiceMediaInfo* info) = 0;
  // Gets last reported error for this media channel.
  virtual void GetLastMediaError(uint32* ssrc,
                                 VoiceMediaChannel::Error* error) {
    ASSERT(error != NULL);
    *error = ERROR_NONE;
  }
  // Signal errors from MediaChannel.  Arguments are:
  //     ssrc(uint32), and error(VoiceMediaChannel::Error).
  sigslot::signal2<uint32, VoiceMediaChannel::Error> SignalMediaError;
};

// Represents a YUV420 (a.k.a. I420) video frame.
class VideoFrame {
  friend class flute::MagicCamVideoRenderer;

 public:
  VideoFrame() : rendered_(false) {}

  virtual ~VideoFrame() {}

  virtual size_t GetWidth() const = 0;
  virtual size_t GetHeight() const = 0;
  virtual const uint8 *GetYPlane() const = 0;
  virtual const uint8 *GetUPlane() const = 0;
  virtual const uint8 *GetVPlane() const = 0;
  virtual uint8 *GetYPlane() = 0;
  virtual uint8 *GetUPlane() = 0;
  virtual uint8 *GetVPlane() = 0;
  virtual int32 GetYPitch() const = 0;
  virtual int32 GetUPitch() const = 0;
  virtual int32 GetVPitch() const = 0;

  // For retrieving the aspect ratio of each pixel. Usually this is 1x1, but
  // the aspect_ratio_idc parameter of H.264 can specify non-square pixels.
  virtual size_t GetPixelWidth() const = 0;
  virtual size_t GetPixelHeight() const = 0;

  // TODO: Add a fourcc format here and probably combine VideoFrame
  // with CapturedFrame.
  virtual int64 GetElapsedTime() const = 0;
  virtual int64 GetTimeStamp() const = 0;
  virtual void SetElapsedTime(int64 elapsed_time) = 0;
  virtual void SetTimeStamp(int64 time_stamp) = 0;

  // Make a copy of the frame. The frame buffer itself may not be copied,
  // in which case both the current and new VideoFrame will share a single
  // reference-counted frame buffer.
  virtual VideoFrame *Copy() const = 0;

  // Writes the frame into the given frame buffer, provided that it is of
  // sufficient size. Returns the frame's actual size, regardless of whether
  // it was written or not (like snprintf). If there is insufficient space,
  // nothing is written.
  virtual size_t CopyToBuffer(uint8 *buffer, size_t size) const = 0;

  // Converts the I420 data to RGB of a certain type such as ARGB and ABGR.
  // Returns the frame's actual size, regardless of whether it was written or
  // not (like snprintf). Parameters size and pitch_rgb are in units of bytes.
  // If there is insufficient space, nothing is written.
  virtual size_t ConvertToRgbBuffer(uint32 to_fourcc, uint8 *buffer,
                                    size_t size, size_t pitch_rgb) const = 0;

  // Writes the frame into the given planes, stretched to the given width and
  // height. The parameter "interpolate" controls whether to interpolate or just
  // take the nearest-point. The parameter "crop" controls whether to crop this
  // frame to the aspect ratio of the given dimensions before stretching.
  virtual void StretchToPlanes(uint8 *y, uint8 *u, uint8 *v,
                               int32 pitchY, int32 pitchU, int32 pitchV,
                               size_t width, size_t height,
                               bool interpolate, bool crop) const = 0;

  // Writes the frame into the given frame buffer, stretched to the given width
  // and height, provided that it is of sufficient size. Returns the frame's
  // actual size, regardless of whether it was written or not (like snprintf).
  // If there is insufficient space, nothing is written. The parameter
  // "interpolate" controls whether to interpolate or just take the
  // nearest-point. The parameter "crop" controls whether to crop this frame to
  // the aspect ratio of the given dimensions before stretching.
  virtual size_t StretchToBuffer(size_t w, size_t h, uint8 *buffer, size_t size,
                                 bool interpolate, bool crop) const = 0;

  // Writes the frame into the target VideoFrame, stretched to the size of that
  // frame. The parameter "interpolate" controls whether to interpolate or just
  // take the nearest-point. The parameter "crop" controls whether to crop this
  // frame to the aspect ratio of the target frame before stretching.
  virtual void StretchToFrame(VideoFrame *target, bool interpolate,
                              bool crop) const = 0;

  // Stretches the frame to the given size, creating a new VideoFrame object to
  // hold it. The parameter "interpolate" controls whether to interpolate or
  // just take the nearest-point. The parameter "crop" controls whether to crop
  // this frame to the aspect ratio of the given dimensions before stretching.
  virtual VideoFrame *Stretch(size_t w, size_t h, bool interpolate,
                              bool crop) const = 0;

  // Size of an I420 image of given dimensions when stored as a frame buffer.
  static size_t SizeOf(size_t w, size_t h) {
    return w * h + ((w + 1) / 2) * ((h + 1) / 2) * 2;
  }

 protected:
  // The frame needs to be rendered to magiccam only once.
  // TODO: Remove this flag once magiccam rendering is fully replaced
  // by client3d rendering.
  mutable bool rendered_;
};

// Simple subclass for use in mocks.
class NullVideoFrame : public VideoFrame {
 public:
  virtual size_t GetWidth() const { return 0; }
  virtual size_t GetHeight() const { return 0; }
  virtual const uint8 *GetYPlane() const { return NULL; }
  virtual const uint8 *GetUPlane() const { return NULL; }
  virtual const uint8 *GetVPlane() const { return NULL; }
  virtual uint8 *GetYPlane() { return NULL; }
  virtual uint8 *GetUPlane() { return NULL; }
  virtual uint8 *GetVPlane() { return NULL; }
  virtual int32 GetYPitch() const { return 0; }
  virtual int32 GetUPitch() const { return 0; }
  virtual int32 GetVPitch() const { return 0; }

  virtual size_t GetPixelWidth() const { return 1; }
  virtual size_t GetPixelHeight() const { return 1; }
  virtual int64 GetElapsedTime() const { return 0; }
  virtual int64 GetTimeStamp() const { return 0; }
  virtual void SetElapsedTime(int64 elapsed_time) {}
  virtual void SetTimeStamp(int64 time_stamp) {}

  virtual VideoFrame *Copy() const {
    return NULL;
  }

  virtual size_t CopyToBuffer(uint8 *buffer, size_t size) const {
    return 0;
  }

  virtual size_t ConvertToRgbBuffer(uint32 to_fourcc, uint8 *buffer,
                                    size_t size, size_t pitch_rgb) const {
    return 0;
  }

  virtual void StretchToPlanes(uint8 *y, uint8 *u, uint8 *v,
                               int32 pitchY, int32 pitchU, int32 pitchV,
                               size_t width, size_t height,
                               bool interpolate, bool crop) const {
  }

  virtual size_t StretchToBuffer(size_t w, size_t h, uint8 *buffer, size_t size,
                                 bool interpolate, bool crop) const {
    return 0;
  }

  virtual void StretchToFrame(VideoFrame *target, bool interpolate,
                              bool crop) const {
  }

  virtual VideoFrame *Stretch(size_t w, size_t h, bool interpolate,
                              bool crop) const {
    return NULL;
  }
};

// Abstract interface for rendering VideoFrames.
class VideoRenderer {
 public:
  virtual ~VideoRenderer() {}
  // Called when the video has changed size.
  virtual bool SetSize(int width, int height, int reserved) = 0;
  // Called when a new frame is available for display.
  virtual bool RenderFrame(const VideoFrame *frame) = 0;
};

// Simple implementation for use in tests.
class NullVideoRenderer : public VideoRenderer {
  virtual bool SetSize(int width, int height, int reserved) {
    return true;
  }
  // Called when a new frame is available for display.
  virtual bool RenderFrame(const VideoFrame *frame) {
    return true;
  }
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
    ERROR_PLAY_SRTP_ERROR = 200,          // Generic receiver SRTP failure.
    ERROR_PLAY_SRTP_AUTH_FAILED,          // Failed to authenticate packets.
    ERROR_PLAY_SRTP_REPLAY,               // Packet replay detected.
  };

  VideoMediaChannel() { renderer_ = NULL; }
  virtual ~VideoMediaChannel() {}
  // Sets the codecs/payload types to be used for incoming media.
  virtual bool SetRecvCodecs(const std::vector<VideoCodec> &codecs) = 0;
  // Sets the codecs/payload types to be used for outgoing media.
  virtual bool SetSendCodecs(const std::vector<VideoCodec> &codecs) = 0;
  // Starts or stops playout of received video.
  virtual bool SetRender(bool render) = 0;
  // Starts or stops transmission (and potentially capture) of local video.
  virtual bool SetSend(bool send) = 0;
  // Adds a new receive-only stream with the specified SSRC.
  virtual bool AddStream(uint32 ssrc, uint32 voice_ssrc) = 0;
  // Removes a stream added with AddStream.
  virtual bool RemoveStream(uint32 ssrc) = 0;
  // Sets the renderer object to be used for the specified stream.
  // If SSRC is 0, the renderer is used for the 'default' stream.
  virtual bool SetRenderer(uint32 ssrc, VideoRenderer* renderer) = 0;
  // Sets the renderer object to be used for the specified stream.
  // If SSRC is 0, the renderer is used for the 'default' stream.
  virtual bool SetExternalRenderer(uint32 ssrc, void* renderer) = 0;
  // Gets quality stats for the channel.
  virtual bool GetStats(VideoMediaInfo* info) = 0;

  // Send an intra frame to the receivers.
  virtual bool SendIntraFrame() = 0;
  // Reuqest each of the remote senders to send an intra frame.
  virtual bool RequestIntraFrame() = 0;

  sigslot::signal2<uint32, Error> SignalMediaError;

 protected:
  VideoRenderer *renderer_;
};

}  // namespace cricket

#endif  // TALK_SESSION_PHONE_MEDIACHANNEL_H_
