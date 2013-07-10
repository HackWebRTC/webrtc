// libjingle
// Copyright 2010 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Declaration of abstract class VideoCapturer

#ifndef TALK_MEDIA_BASE_VIDEOCAPTURER_H_
#define TALK_MEDIA_BASE_VIDEOCAPTURER_H_

#include <string>
#include <vector>

#include "talk/base/basictypes.h"
#include "talk/base/criticalsection.h"
#include "talk/base/messagehandler.h"
#include "talk/base/scoped_ptr.h"
#include "talk/base/sigslot.h"
#include "talk/base/thread.h"
#include "talk/media/base/videocommon.h"
#include "talk/media/devices/devicemanager.h"


namespace cricket {

class VideoProcessor;

// Current state of the capturer.
// TODO(hellner): CS_NO_DEVICE is an error code not a capture state. Separate
//                error codes and states.
enum CaptureState {
  CS_STOPPED,    // The capturer has been stopped or hasn't started yet.
  CS_STARTING,   // The capturer is in the process of starting. Note, it may
                 // still fail to start.
  CS_RUNNING,    // The capturer has been started successfully and is now
                 // capturing.
  CS_PAUSED,     // The capturer has been paused.
  CS_FAILED,     // The capturer failed to start.
  CS_NO_DEVICE,  // The capturer has no device and consequently failed to start.
};

class VideoFrame;

struct CapturedFrame {
  static const uint32 kFrameHeaderSize = 40;  // Size from width to data_size.
  static const uint32 kUnknownDataSize = 0xFFFFFFFF;

  CapturedFrame();

  // Get the number of bytes of the frame data. If data_size is known, return
  // it directly. Otherwise, calculate the size based on width, height, and
  // fourcc. Return true if succeeded.
  bool GetDataSize(uint32* size) const;

  // The width and height of the captured frame could be different from those
  // of VideoFormat. Once the first frame is captured, the width, height,
  // fourcc, pixel_width, and pixel_height should keep the same over frames.
  int    width;         // in number of pixels
  int    height;        // in number of pixels
  uint32 fourcc;        // compression
  uint32 pixel_width;   // width of a pixel, default is 1
  uint32 pixel_height;  // height of a pixel, default is 1
  int64  elapsed_time;  // elapsed time since the creation of the frame
                        // source (that is, the camera), in nanoseconds.
  int64  time_stamp;    // timestamp of when the frame was captured, in unix
                        // time with nanosecond units.
  uint32 data_size;     // number of bytes of the frame data
  int    rotation;      // rotation in degrees of the frame (0, 90, 180, 270)
  void*  data;          // pointer to the frame data. This object allocates the
                        // memory or points to an existing memory.

 private:
  DISALLOW_COPY_AND_ASSIGN(CapturedFrame);
};

// VideoCapturer is an abstract class that defines the interfaces for video
// capturing. The subclasses implement the video capturer for various types of
// capturers and various platforms.
//
// The captured frames may need to be adapted (for example, cropping). Adaptors
// can be registered to the capturer or applied externally to the capturer.
// If the adaptor is needed, it acts as the downstream of VideoCapturer, adapts
// the captured frames, and delivers the adapted frames to other components
// such as the encoder. Effects can also be registered to the capturer or
// applied externally.
//
// Programming model:
//   Create an object of a subclass of VideoCapturer
//   Initialize
//   SignalStateChange.connect()
//   SignalFrameCaptured.connect()
//   Find the capture format for Start() by either calling GetSupportedFormats()
//   and selecting one of the supported or calling GetBestCaptureFormat().
//   Start()
//   GetCaptureFormat() optionally
//   Stop()
//
// Assumption:
//   The Start() and Stop() methods are called by a single thread (E.g., the
//   media engine thread). Hence, the VideoCapture subclasses dont need to be
//   thread safe.
//
class VideoCapturer
    : public sigslot::has_slots<>,
      public talk_base::MessageHandler {
 public:
  typedef std::vector<VideoProcessor*> VideoProcessors;

  // All signals are marshalled to |thread| or the creating thread if
  // none is provided.
  VideoCapturer();
  explicit VideoCapturer(talk_base::Thread* thread);
  virtual ~VideoCapturer() {}

  // Gets the id of the underlying device, which is available after the capturer
  // is initialized. Can be used to determine if two capturers reference the
  // same device.
  const std::string& GetId() const { return id_; }

  // Get the capture formats supported by the video capturer. The supported
  // formats are non empty after the device has been opened successfully.
  const std::vector<VideoFormat>* GetSupportedFormats() const;

  // Get the best capture format for the desired format. The best format is the
  // same as one of the supported formats except that the frame interval may be
  // different. If the application asks for 16x9 and the camera does not support
  // 16x9 HD or the application asks for 16x10, we find the closest 4x3 and then
  // crop; Otherwise, we find what the application asks for. Note that we assume
  // that for HD, the desired format is always 16x9. The subclasses can override
  // the default implementation.
  // Parameters
  //   desired: the input desired format. If desired.fourcc is not kAnyFourcc,
  //            the best capture format has the exactly same fourcc. Otherwise,
  //            the best capture format uses a fourcc in GetPreferredFourccs().
  //   best_format: the output of the best capture format.
  // Return false if there is no such a best format, that is, the desired format
  // is not supported.
  virtual bool GetBestCaptureFormat(const VideoFormat& desired,
                                    VideoFormat* best_format);

  // TODO(hellner): deprecate (make private) the Start API in favor of this one.
  //                Also remove CS_STARTING as it is implied by the return
  //                value of StartCapturing().
  bool StartCapturing(const VideoFormat& capture_format);
  // Start the video capturer with the specified capture format.
  // Parameter
  //   capture_format: The caller got this parameter by either calling
  //                   GetSupportedFormats() and selecting one of the supported
  //                   or calling GetBestCaptureFormat().
  // Return
  //   CS_STARTING:  The capturer is trying to start. Success or failure will
  //                 be notified via the |SignalStateChange| callback.
  //   CS_RUNNING:   if the capturer is started and capturing.
  //   CS_PAUSED:    Will never be returned.
  //   CS_FAILED:    if the capturer failes to start..
  //   CS_NO_DEVICE: if the capturer has no device and fails to start.
  virtual CaptureState Start(const VideoFormat& capture_format) = 0;
  // Sets the desired aspect ratio. If the capturer is capturing at another
  // aspect ratio it will crop the width or the height so that asked for
  // aspect ratio is acheived. Note that ratio_w and ratio_h do not need to be
  // relatively prime.
  void UpdateAspectRatio(int ratio_w, int ratio_h);
  void ClearAspectRatio();

  // Get the current capture format, which is set by the Start() call.
  // Note that the width and height of the captured frames may differ from the
  // capture format. For example, the capture format is HD but the captured
  // frames may be smaller than HD.
  const VideoFormat* GetCaptureFormat() const {
    return capture_format_.get();
  }

  // Pause the video capturer.
  virtual bool Pause(bool paused);
  // Stop the video capturer.
  virtual void Stop() = 0;
  // Check if the video capturer is running.
  virtual bool IsRunning() = 0;
  // Restart the video capturer with the new |capture_format|.
  // Default implementation stops and starts the capturer.
  virtual bool Restart(const VideoFormat& capture_format);
  // TODO(thorcarpenter): This behavior of keeping the camera open just to emit
  // black frames is a total hack and should be fixed.
  // When muting, produce black frames then pause the camera.
  // When unmuting, start the camera. Camera starts unmuted.
  virtual bool MuteToBlackThenPause(bool muted);
  virtual bool IsMuted() const {
    return muted_;
  }
  CaptureState capture_state() const {
    return capture_state_;
  }

  // Adds a video processor that will be applied on VideoFrames returned by
  // |SignalVideoFrame|. Multiple video processors can be added. The video
  // processors will be applied in the order they were added.
  void AddVideoProcessor(VideoProcessor* video_processor);
  // Removes the |video_processor| from the list of video processors or
  // returns false.
  bool RemoveVideoProcessor(VideoProcessor* video_processor);

  // Returns true if the capturer is screencasting. This can be used to
  // implement screencast specific behavior.
  virtual bool IsScreencast() const = 0;

  // Caps the VideoCapturer's format according to max_format. It can e.g. be
  // used to prevent cameras from capturing at a resolution or framerate that
  // the capturer is capable of but not performing satisfactorily at.
  // The capping is an upper bound for each component of the capturing format.
  // The fourcc component is ignored.
  void ConstrainSupportedFormats(const VideoFormat& max_format);

  void set_enable_camera_list(bool enable_camera_list) {
    enable_camera_list_ = enable_camera_list;
  }
  bool enable_camera_list() {
    return enable_camera_list_;
  }
  // Signal all capture state changes that are not a direct result of calling
  // Start().
  sigslot::signal2<VideoCapturer*, CaptureState> SignalStateChange;
  // TODO(hellner): rename |SignalFrameCaptured| to something like
  //                |SignalRawFrame| or |SignalNativeFrame|.
  // Frame callbacks are multithreaded to allow disconnect and connect to be
  // called concurrently. It also ensures that it is safe to call disconnect
  // at any time which is needed since the signal may be called from an
  // unmarshalled thread owned by the VideoCapturer.
  // Signal the captured frame to downstream.
  sigslot::signal2<VideoCapturer*, const CapturedFrame*,
                   sigslot::multi_threaded_local> SignalFrameCaptured;
  // Signal the captured frame converted to I420 to downstream.
  sigslot::signal2<VideoCapturer*, const VideoFrame*,
                   sigslot::multi_threaded_local> SignalVideoFrame;

  const VideoProcessors& video_processors() const { return video_processors_; }

 protected:
  // Callback attached to SignalFrameCaptured where SignalVideoFrames is called.
  void OnFrameCaptured(VideoCapturer* video_capturer,
                       const CapturedFrame* captured_frame);
  void SetCaptureState(CaptureState state);

  // Marshals SignalStateChange onto thread_.
  void OnMessage(talk_base::Message* message);

  // subclasses override this virtual method to provide a vector of fourccs, in
  // order of preference, that are expected by the media engine.
  virtual bool GetPreferredFourccs(std::vector<uint32>* fourccs) = 0;

  // mutators to set private attributes
  void SetId(const std::string& id) {
    id_ = id;
  }

  void SetCaptureFormat(const VideoFormat* format) {
    capture_format_.reset(format ? new VideoFormat(*format) : NULL);
  }

  void SetSupportedFormats(const std::vector<VideoFormat>& formats);

 private:
  void Construct();
  // Get the distance between the desired format and the supported format.
  // Return the max distance if they mismatch. See the implementation for
  // details.
  int64 GetFormatDistance(const VideoFormat& desired,
                          const VideoFormat& supported);

  // Convert captured frame to readable string for LOG messages.
  std::string ToString(const CapturedFrame* frame) const;

  // Applies all registered processors. If any of the processors signal that
  // the frame should be dropped the return value will be false. Note that
  // this frame should be dropped as it has not applied all processors.
  bool ApplyProcessors(VideoFrame* video_frame);

  // Updates filtered_supported_formats_ so that it contains the formats in
  // supported_formats_ that fulfill all applied restrictions.
  void UpdateFilteredSupportedFormats();
  // Returns true if format doesn't fulfill all applied restrictions.
  bool ShouldFilterFormat(const VideoFormat& format) const;

  talk_base::Thread* thread_;
  std::string id_;
  CaptureState capture_state_;
  talk_base::scoped_ptr<VideoFormat> capture_format_;
  std::vector<VideoFormat> supported_formats_;
  talk_base::scoped_ptr<VideoFormat> max_format_;
  std::vector<VideoFormat> filtered_supported_formats_;

  int ratio_w_;  // View resolution. e.g. 1280 x 720.
  int ratio_h_;
  bool enable_camera_list_;
  int scaled_width_;  // Current output size from ComputeScale.
  int scaled_height_;
  bool muted_;
  int black_frame_count_down_;

  talk_base::CriticalSection crit_;
  VideoProcessors video_processors_;

  DISALLOW_COPY_AND_ASSIGN(VideoCapturer);
};

}  // namespace cricket

#endif  // TALK_MEDIA_BASE_VIDEOCAPTURER_H_
