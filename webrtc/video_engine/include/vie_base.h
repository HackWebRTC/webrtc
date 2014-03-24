/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//
//  - Creating and deleting VideoEngine instances.
//  - Creating and deleting channels.
//  - Connect a video channel with a corresponding voice channel for audio/video
//    synchronization.
//  - Start and stop sending and receiving.

#ifndef WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_BASE_H_
#define WEBRTC_VIDEO_ENGINE_INCLUDE_VIE_BASE_H_

#include "webrtc/common_types.h"

#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
#include <jni.h>
#endif

namespace webrtc {

class Config;
class VoiceEngine;

// CpuOveruseObserver is called when a system overuse is detected and
// VideoEngine cannot keep up the encoding frequency.
class CpuOveruseObserver {
 public:
  // Called as soon as an overuse is detected.
  virtual void OveruseDetected() = 0;
  // Called periodically when the system is not overused any longer.
  virtual void NormalUsage() = 0;

 protected:
  virtual ~CpuOveruseObserver() {}
};

// Limits on standard deviation for under/overuse.
#ifdef WEBRTC_ANDROID
const float kOveruseStdDevMs = 32.0f;
const float kNormalUseStdDevMs = 27.0f;
#elif WEBRTC_LINUX
const float kOveruseStdDevMs = 20.0f;
const float kNormalUseStdDevMs = 14.0f;
#elif WEBRTC_MAC
const float kOveruseStdDevMs = 27.0f;
const float kNormalUseStdDevMs = 21.0f;
#elif WEBRTC_WIN
const float kOveruseStdDevMs = 20.0f;
const float kNormalUseStdDevMs = 14.0f;
#else
const float kOveruseStdDevMs = 30.0f;
const float kNormalUseStdDevMs = 20.0f;
#endif

struct CpuOveruseOptions {
  CpuOveruseOptions()
    : enable_capture_jitter_method(true),
      low_capture_jitter_threshold_ms(kNormalUseStdDevMs),
      high_capture_jitter_threshold_ms(kOveruseStdDevMs),
      enable_encode_usage_method(false),
      low_encode_usage_threshold_percent(60),
      high_encode_usage_threshold_percent(90),
      frame_timeout_interval_ms(1500),
      min_frame_samples(120),
      min_process_count(3),
      high_threshold_consecutive_count(2) {}

  // Method based on inter-arrival jitter of captured frames.
  bool enable_capture_jitter_method;
  float low_capture_jitter_threshold_ms;  // Threshold for triggering underuse.
  float high_capture_jitter_threshold_ms; // Threshold for triggering overuse.
  // Method based on encode time of frames.
  bool enable_encode_usage_method;
  int low_encode_usage_threshold_percent;  // Threshold for triggering underuse.
  int high_encode_usage_threshold_percent; // Threshold for triggering overuse.
  // General settings.
  int frame_timeout_interval_ms;  // The maximum allowed interval between two
                                  // frames before resetting estimations.
  int min_frame_samples;  // The minimum number of frames required.
  int min_process_count;  // The number of initial process times required before
                          // triggering an overuse/underuse.
  int high_threshold_consecutive_count; // The number of consecutive checks
                                        // above the high threshold before
                                        // triggering an overuse.

  bool Equals(const CpuOveruseOptions& o) const {
    return enable_capture_jitter_method == o.enable_capture_jitter_method &&
        low_capture_jitter_threshold_ms == o.low_capture_jitter_threshold_ms &&
        high_capture_jitter_threshold_ms ==
        o.high_capture_jitter_threshold_ms &&
        enable_encode_usage_method == o.enable_encode_usage_method &&
        low_encode_usage_threshold_percent ==
        o.low_encode_usage_threshold_percent &&
        high_encode_usage_threshold_percent ==
        o.high_encode_usage_threshold_percent &&
        frame_timeout_interval_ms == o.frame_timeout_interval_ms &&
        min_frame_samples == o.min_frame_samples &&
        min_process_count == o.min_process_count &&
        high_threshold_consecutive_count == o.high_threshold_consecutive_count;
  }
};

class WEBRTC_DLLEXPORT VideoEngine {
 public:
  // Creates a VideoEngine object, which can then be used to acquire sub‐APIs.
  static VideoEngine* Create();
  static VideoEngine* Create(const Config& config);

  // Deletes a VideoEngine instance.
  static bool Delete(VideoEngine*& video_engine);

  // Specifies the amount and type of trace information, which will be created
  // by the VideoEngine.
  static int SetTraceFilter(const unsigned int filter);

  // Sets the name of the trace file and enables non‐encrypted trace messages.
  static int SetTraceFile(const char* file_nameUTF8,
                          const bool add_file_counter = false);

  // Installs the TraceCallback implementation to ensure that the VideoEngine
  // user receives callbacks for generated trace messages.
  static int SetTraceCallback(TraceCallback* callback);

#if defined(ANDROID) && !defined(WEBRTC_CHROMIUM_BUILD)
  // Android specific.
  static int SetAndroidObjects(JavaVM* java_vm);
#endif

 protected:
  VideoEngine() {}
  virtual ~VideoEngine() {}
};

class WEBRTC_DLLEXPORT ViEBase {
 public:
  // Factory for the ViEBase sub‐API and increases an internal reference
  // counter if successful. Returns NULL if the API is not supported or if
  // construction fails.
  static ViEBase* GetInterface(VideoEngine* video_engine);

  // Releases the ViEBase sub-API and decreases an internal reference counter.
  // Returns the new reference count. This value should be zero
  // for all sub-API:s before the VideoEngine object can be safely deleted.
  virtual int Release() = 0;

  // Initiates all common parts of the VideoEngine.
  virtual int Init() = 0;

  // Connects a VideoEngine instance to a VoiceEngine instance for audio video
  // synchronization.
  virtual int SetVoiceEngine(VoiceEngine* voice_engine) = 0;

  // Creates a new channel.
  virtual int CreateChannel(int& video_channel) = 0;

  // Creates a new channel grouped together with |original_channel|. The channel
  // can both send and receive video. It is assumed the channel is sending
  // and/or receiving video to the same end-point.
  // Note: |CreateReceiveChannel| will give better performance and network
  // properties for receive only channels.
  virtual int CreateChannel(int& video_channel,
                            int original_channel) = 0;

  // Creates a new channel grouped together with |original_channel|. The channel
  // can only receive video and it is assumed the remote end-point is the same
  // as for |original_channel|.
  virtual int CreateReceiveChannel(int& video_channel,
                                   int original_channel) = 0;

  // Deletes an existing channel and releases the utilized resources.
  virtual int DeleteChannel(const int video_channel) = 0;

  // Registers an observer to be called when an overuse is detected, see
  // 'CpuOveruseObserver' for details.
  // NOTE: This is still very experimental functionality.
  virtual int RegisterCpuOveruseObserver(int channel,
                                         CpuOveruseObserver* observer) = 0;

  // Sets options for cpu overuse detector.
  // TODO(asapersson): Remove default implementation.
  virtual int SetCpuOveruseOptions(int channel,
                                   const CpuOveruseOptions& options) {
    return -1;
  }

  // Gets cpu overuse measures.
  // capture_jitter_ms: The current estimated jitter in ms based on incoming
  //                    captured frames.
  // avg_encode_time_ms: The average encode time in ms.
  // encode_usage_percent: The average encode time divided by the average time
  //                       difference between incoming captured frames.
  // capture_queue_delay_ms_per_s: The current time delay between an incoming
  //                               captured frame until the frame is being
  //                               processed. The delay is expressed in ms
  //                               delay per second.
  // TODO(asapersson): Remove default implementation.
  virtual int CpuOveruseMeasures(int channel,
                                 int* capture_jitter_ms,
                                 int* avg_encode_time_ms,
                                 int* encode_usage_percent,
                                 int* capture_queue_delay_ms_per_s) {
    return -1;
  }

  // Specifies the VoiceEngine and VideoEngine channel pair to use for
  // audio/video synchronization.
  virtual int ConnectAudioChannel(const int video_channel,
                                  const int audio_channel) = 0;

  // Disconnects a previously paired VideoEngine and VoiceEngine channel pair.
  virtual int DisconnectAudioChannel(const int video_channel) = 0;

  // Starts sending packets to an already specified IP address and port number
  // for a specified channel.
  virtual int StartSend(const int video_channel) = 0;

  // Stops packets from being sent for a specified channel.
  virtual int StopSend(const int video_channel) = 0;

  // Prepares VideoEngine for receiving packets on the specified channel.
  virtual int StartReceive(const int video_channel) = 0;

  // Stops receiving incoming RTP and RTCP packets on the specified channel.
  virtual int StopReceive(const int video_channel) = 0;

  // Retrieves the version information for VideoEngine and its components.
  virtual int GetVersion(char version[1024]) = 0;

  // Returns the last VideoEngine error code.
  virtual int LastError() = 0;

 protected:
  ViEBase() {}
  virtual ~ViEBase() {}
};

}  // namespace webrtc

#endif  // #define WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_BASE_H_
