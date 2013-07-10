// libjingle
// Copyright 2004 Google Inc.
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
// Implementation of VideoRecorder and FileVideoCapturer.

#include "talk/media/devices/filevideocapturer.h"

#include "talk/base/bytebuffer.h"
#include "talk/base/logging.h"
#include "talk/base/thread.h"

namespace cricket {

/////////////////////////////////////////////////////////////////////
// Implementation of class VideoRecorder
/////////////////////////////////////////////////////////////////////
bool VideoRecorder::Start(const std::string& filename, bool write_header) {
  Stop();
  write_header_ = write_header;
  int err;
  if (!video_file_.Open(filename, "wb", &err)) {
    LOG(LS_ERROR) << "Unable to open file " << filename << " err=" << err;
    return false;
  }
  return true;
}

void VideoRecorder::Stop() {
  video_file_.Close();
}

bool VideoRecorder::RecordFrame(const CapturedFrame& frame) {
  if (talk_base::SS_CLOSED == video_file_.GetState()) {
    LOG(LS_ERROR) << "File not opened yet";
    return false;
  }

  uint32 size = 0;
  if (!frame.GetDataSize(&size)) {
    LOG(LS_ERROR) << "Unable to calculate the data size of the frame";
    return false;
  }

  if (write_header_) {
    // Convert the frame header to bytebuffer.
    talk_base::ByteBuffer buffer;
    buffer.WriteUInt32(frame.width);
    buffer.WriteUInt32(frame.height);
    buffer.WriteUInt32(frame.fourcc);
    buffer.WriteUInt32(frame.pixel_width);
    buffer.WriteUInt32(frame.pixel_height);
    buffer.WriteUInt64(frame.elapsed_time);
    buffer.WriteUInt64(frame.time_stamp);
    buffer.WriteUInt32(size);

    // Write the bytebuffer to file.
    if (talk_base::SR_SUCCESS != video_file_.Write(buffer.Data(),
                                                   buffer.Length(),
                                                   NULL,
                                                   NULL)) {
      LOG(LS_ERROR) << "Failed to write frame header";
      return false;
    }
  }
  // Write the frame data to file.
  if (talk_base::SR_SUCCESS != video_file_.Write(frame.data,
                                                 size,
                                                 NULL,
                                                 NULL)) {
    LOG(LS_ERROR) << "Failed to write frame data";
    return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////
// Definition of private class FileReadThread that periodically reads
// frames from a file.
///////////////////////////////////////////////////////////////////////
class FileVideoCapturer::FileReadThread
    : public talk_base::Thread, public talk_base::MessageHandler {
 public:
  explicit FileReadThread(FileVideoCapturer* capturer)
      : capturer_(capturer),
        finished_(false) {
  }

  // Override virtual method of parent Thread. Context: Worker Thread.
  virtual void Run() {
    // Read the first frame and start the message pump. The pump runs until
    // Stop() is called externally or Quit() is called by OnMessage().
    int waiting_time_ms = 0;
    if (capturer_ && capturer_->ReadFrame(true, &waiting_time_ms)) {
      PostDelayed(waiting_time_ms, this);
      Thread::Run();
    }
    finished_ = true;
  }

  // Override virtual method of parent MessageHandler. Context: Worker Thread.
  virtual void OnMessage(talk_base::Message* /*pmsg*/) {
    int waiting_time_ms = 0;
    if (capturer_ && capturer_->ReadFrame(false, &waiting_time_ms)) {
      PostDelayed(waiting_time_ms, this);
    } else {
      Quit();
    }
  }

  // Check if Run() is finished.
  bool Finished() const { return finished_; }

 private:
  FileVideoCapturer* capturer_;
  bool finished_;

  DISALLOW_COPY_AND_ASSIGN(FileReadThread);
};

/////////////////////////////////////////////////////////////////////
// Implementation of class FileVideoCapturer
/////////////////////////////////////////////////////////////////////
static const int64 kNumNanoSecsPerMilliSec = 1000000;
const char* FileVideoCapturer::kVideoFileDevicePrefix = "video-file:";

FileVideoCapturer::FileVideoCapturer()
    : frame_buffer_size_(0),
      file_read_thread_(NULL),
      repeat_(0),
      start_time_ns_(0),
      last_frame_timestamp_ns_(0),
      ignore_framerate_(false) {
}

FileVideoCapturer::~FileVideoCapturer() {
  Stop();
  delete[] static_cast<char*> (captured_frame_.data);
}

bool FileVideoCapturer::Init(const Device& device) {
  if (!FileVideoCapturer::IsFileVideoCapturerDevice(device)) {
    return false;
  }
  std::string filename(device.name);
  if (IsRunning()) {
    LOG(LS_ERROR) << "The file video capturer is already running";
    return false;
  }
  // Open the file.
  int err;
  if (!video_file_.Open(filename, "rb", &err)) {
    LOG(LS_ERROR) << "Unable to open the file " << filename << " err=" << err;
    return false;
  }
  // Read the first frame's header to determine the supported format.
  CapturedFrame frame;
  if (talk_base::SR_SUCCESS != ReadFrameHeader(&frame)) {
    LOG(LS_ERROR) << "Failed to read the first frame header";
    video_file_.Close();
    return false;
  }
  // Seek back to the start of the file.
  if (!video_file_.SetPosition(0)) {
    LOG(LS_ERROR) << "Failed to seek back to beginning of the file";
    video_file_.Close();
    return false;
  }

  // Enumerate the supported formats. We have only one supported format. We set
  // the frame interval to kMinimumInterval here. In Start(), if the capture
  // format's interval is greater than kMinimumInterval, we use the interval;
  // otherwise, we use the timestamp in the file to control the interval.
  VideoFormat format(frame.width, frame.height, VideoFormat::kMinimumInterval,
                     frame.fourcc);
  std::vector<VideoFormat> supported;
  supported.push_back(format);

  SetId(device.id);
  SetSupportedFormats(supported);
  return true;
}

bool FileVideoCapturer::Init(const std::string& filename) {
  return Init(FileVideoCapturer::CreateFileVideoCapturerDevice(filename));
}

CaptureState FileVideoCapturer::Start(const VideoFormat& capture_format) {
  if (IsRunning()) {
    LOG(LS_ERROR) << "The file video capturer is already running";
    return CS_FAILED;
  }

  if (talk_base::SS_CLOSED == video_file_.GetState()) {
    LOG(LS_ERROR) << "File not opened yet";
    return CS_NO_DEVICE;
  } else if (!video_file_.SetPosition(0)) {
    LOG(LS_ERROR) << "Failed to seek back to beginning of the file";
    return CS_FAILED;
  }

  SetCaptureFormat(&capture_format);
  // Create a thread to read the file.
  file_read_thread_ = new FileReadThread(this);
  bool ret = file_read_thread_->Start();
  start_time_ns_ = kNumNanoSecsPerMilliSec *
      static_cast<int64>(talk_base::Time());
  if (ret) {
    LOG(LS_INFO) << "File video capturer '" << GetId() << "' started";
    return CS_RUNNING;
  } else {
    LOG(LS_ERROR) << "File video capturer '" << GetId() << "' failed to start";
    return CS_FAILED;
  }
}

bool FileVideoCapturer::IsRunning() {
  return file_read_thread_ && !file_read_thread_->Finished();
}

void FileVideoCapturer::Stop() {
  if (file_read_thread_) {
    file_read_thread_->Stop();
    file_read_thread_ = NULL;
    LOG(LS_INFO) << "File video capturer '" << GetId() << "' stopped";
  }
  SetCaptureFormat(NULL);
}

bool FileVideoCapturer::GetPreferredFourccs(std::vector<uint32>* fourccs) {
  if (!fourccs) {
    return false;
  }

  fourccs->push_back(GetSupportedFormats()->at(0).fourcc);
  return true;
}

talk_base::StreamResult FileVideoCapturer::ReadFrameHeader(
    CapturedFrame* frame) {
  // We first read kFrameHeaderSize bytes from the file stream to a memory
  // buffer, then construct a bytebuffer from the memory buffer, and finally
  // read the frame header from the bytebuffer.
  char header[CapturedFrame::kFrameHeaderSize];
  talk_base::StreamResult sr;
  size_t bytes_read;
  int error;
  sr = video_file_.Read(header,
                        CapturedFrame::kFrameHeaderSize,
                        &bytes_read,
                        &error);
  LOG(LS_VERBOSE) << "Read frame header: stream_result = " << sr
                  << ", bytes read = " << bytes_read << ", error = " << error;
  if (talk_base::SR_SUCCESS == sr) {
    if (CapturedFrame::kFrameHeaderSize != bytes_read) {
      return talk_base::SR_EOS;
    }
    talk_base::ByteBuffer buffer(header, CapturedFrame::kFrameHeaderSize);
    buffer.ReadUInt32(reinterpret_cast<uint32*>(&frame->width));
    buffer.ReadUInt32(reinterpret_cast<uint32*>(&frame->height));
    buffer.ReadUInt32(&frame->fourcc);
    buffer.ReadUInt32(&frame->pixel_width);
    buffer.ReadUInt32(&frame->pixel_height);
    buffer.ReadUInt64(reinterpret_cast<uint64*>(&frame->elapsed_time));
    buffer.ReadUInt64(reinterpret_cast<uint64*>(&frame->time_stamp));
    buffer.ReadUInt32(&frame->data_size);
  }

  return sr;
}

// Executed in the context of FileReadThread.
bool FileVideoCapturer::ReadFrame(bool first_frame, int* wait_time_ms) {
  uint32 start_read_time_ms = talk_base::Time();

  // 1. Signal the previously read frame to downstream.
  if (!first_frame) {
    captured_frame_.time_stamp = kNumNanoSecsPerMilliSec *
        static_cast<int64>(start_read_time_ms);
    captured_frame_.elapsed_time = captured_frame_.time_stamp - start_time_ns_;
    SignalFrameCaptured(this, &captured_frame_);
  }

  // 2. Read the next frame.
  if (talk_base::SS_CLOSED == video_file_.GetState()) {
    LOG(LS_ERROR) << "File not opened yet";
    return false;
  }
  // 2.1 Read the frame header.
  talk_base::StreamResult result = ReadFrameHeader(&captured_frame_);
  if (talk_base::SR_EOS == result) {  // Loop back if repeat.
    if (repeat_ != talk_base::kForever) {
      if (repeat_ > 0) {
        --repeat_;
      } else {
        return false;
      }
    }

    if (video_file_.SetPosition(0)) {
      result = ReadFrameHeader(&captured_frame_);
    }
  }
  if (talk_base::SR_SUCCESS != result) {
    LOG(LS_ERROR) << "Failed to read the frame header";
    return false;
  }
  // 2.2 Reallocate memory for the frame data if necessary.
  if (frame_buffer_size_ < captured_frame_.data_size) {
    frame_buffer_size_ = captured_frame_.data_size;
    delete[] static_cast<char*> (captured_frame_.data);
    captured_frame_.data = new char[frame_buffer_size_];
  }
  // 2.3 Read the frame adata.
  if (talk_base::SR_SUCCESS != video_file_.Read(captured_frame_.data,
                                                captured_frame_.data_size,
                                                NULL, NULL)) {
    LOG(LS_ERROR) << "Failed to read frame data";
    return false;
  }

  // 3. Decide how long to wait for the next frame.
  *wait_time_ms = 0;

  // If the capture format's interval is not kMinimumInterval, we use it to
  // control the rate; otherwise, we use the timestamp in the file to control
  // the rate.
  if (!first_frame && !ignore_framerate_) {
    int64 interval_ns =
        GetCaptureFormat()->interval > VideoFormat::kMinimumInterval ?
        GetCaptureFormat()->interval :
        captured_frame_.time_stamp - last_frame_timestamp_ns_;
    int interval_ms = static_cast<int>(interval_ns / kNumNanoSecsPerMilliSec);
    interval_ms -= talk_base::Time() - start_read_time_ms;
    if (interval_ms > 0) {
      *wait_time_ms = interval_ms;
    }
  }
  // Keep the original timestamp read from the file.
  last_frame_timestamp_ns_ = captured_frame_.time_stamp;
  return true;
}

}  // namespace cricket
