/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include <errno.h>
#include <string.h>
#include <algorithm>
#include <string>

#include "rtc_base/checks.h"
#include "rtc_base/location.h"
#include "rtc_base/message_queue.h"
#include "rtc_base/stream.h"
#include "rtc_base/thread.h"

#if defined(WEBRTC_WIN)
#include <windows.h>

#define fileno _fileno
#include "rtc_base/string_utils.h"
#endif

namespace rtc {

///////////////////////////////////////////////////////////////////////////////
// StreamInterface
///////////////////////////////////////////////////////////////////////////////
StreamInterface::~StreamInterface() {}

StreamResult StreamInterface::WriteAll(const void* data,
                                       size_t data_len,
                                       size_t* written,
                                       int* error) {
  StreamResult result = SR_SUCCESS;
  size_t total_written = 0, current_written;
  while (total_written < data_len) {
    result = Write(static_cast<const char*>(data) + total_written,
                   data_len - total_written, &current_written, error);
    if (result != SR_SUCCESS)
      break;
    total_written += current_written;
  }
  if (written)
    *written = total_written;
  return result;
}

void StreamInterface::PostEvent(Thread* t, int events, int err) {
  t->Post(RTC_FROM_HERE, this, MSG_POST_EVENT,
          new StreamEventData(events, err));
}

void StreamInterface::PostEvent(int events, int err) {
  PostEvent(Thread::Current(), events, err);
}

bool StreamInterface::Flush() {
  return false;
}

StreamInterface::StreamInterface() {}

void StreamInterface::OnMessage(Message* msg) {
  if (MSG_POST_EVENT == msg->message_id) {
    StreamEventData* pe = static_cast<StreamEventData*>(msg->pdata);
    SignalEvent(this, pe->events, pe->error);
    delete msg->pdata;
  }
}

///////////////////////////////////////////////////////////////////////////////
// StreamAdapterInterface
///////////////////////////////////////////////////////////////////////////////

StreamAdapterInterface::StreamAdapterInterface(StreamInterface* stream,
                                               bool owned)
    : stream_(stream), owned_(owned) {
  if (nullptr != stream_)
    stream_->SignalEvent.connect(this, &StreamAdapterInterface::OnEvent);
}

StreamState StreamAdapterInterface::GetState() const {
  return stream_->GetState();
}
StreamResult StreamAdapterInterface::Read(void* buffer,
                                          size_t buffer_len,
                                          size_t* read,
                                          int* error) {
  return stream_->Read(buffer, buffer_len, read, error);
}
StreamResult StreamAdapterInterface::Write(const void* data,
                                           size_t data_len,
                                           size_t* written,
                                           int* error) {
  return stream_->Write(data, data_len, written, error);
}
void StreamAdapterInterface::Close() {
  stream_->Close();
}

bool StreamAdapterInterface::Flush() {
  return stream_->Flush();
}

void StreamAdapterInterface::Attach(StreamInterface* stream, bool owned) {
  if (nullptr != stream_)
    stream_->SignalEvent.disconnect(this);
  if (owned_)
    delete stream_;
  stream_ = stream;
  owned_ = owned;
  if (nullptr != stream_)
    stream_->SignalEvent.connect(this, &StreamAdapterInterface::OnEvent);
}

StreamInterface* StreamAdapterInterface::Detach() {
  if (nullptr != stream_)
    stream_->SignalEvent.disconnect(this);
  StreamInterface* stream = stream_;
  stream_ = nullptr;
  return stream;
}

StreamAdapterInterface::~StreamAdapterInterface() {
  if (owned_)
    delete stream_;
}

void StreamAdapterInterface::OnEvent(StreamInterface* stream,
                                     int events,
                                     int err) {
  SignalEvent(this, events, err);
}

///////////////////////////////////////////////////////////////////////////////
// FileStream
///////////////////////////////////////////////////////////////////////////////

FileStream::FileStream() : file_(nullptr) {}

FileStream::~FileStream() {
  FileStream::Close();
}

bool FileStream::Open(const std::string& filename,
                      const char* mode,
                      int* error) {
  Close();
#if defined(WEBRTC_WIN)
  std::wstring wfilename;
  if (Utf8ToWindowsFilename(filename, &wfilename)) {
    file_ = _wfopen(wfilename.c_str(), ToUtf16(mode).c_str());
  } else {
    if (error) {
      *error = -1;
      return false;
    }
  }
#else
  file_ = fopen(filename.c_str(), mode);
#endif
  if (!file_ && error) {
    *error = errno;
  }
  return (file_ != nullptr);
}

bool FileStream::OpenShare(const std::string& filename,
                           const char* mode,
                           int shflag,
                           int* error) {
  Close();
#if defined(WEBRTC_WIN)
  std::wstring wfilename;
  if (Utf8ToWindowsFilename(filename, &wfilename)) {
    file_ = _wfsopen(wfilename.c_str(), ToUtf16(mode).c_str(), shflag);
    if (!file_ && error) {
      *error = errno;
      return false;
    }
    return file_ != nullptr;
  } else {
    if (error) {
      *error = -1;
    }
    return false;
  }
#else
  return Open(filename, mode, error);
#endif
}

bool FileStream::DisableBuffering() {
  if (!file_)
    return false;
  return (setvbuf(file_, nullptr, _IONBF, 0) == 0);
}

StreamState FileStream::GetState() const {
  return (file_ == nullptr) ? SS_CLOSED : SS_OPEN;
}

StreamResult FileStream::Read(void* buffer,
                              size_t buffer_len,
                              size_t* read,
                              int* error) {
  if (!file_)
    return SR_EOS;
  size_t result = fread(buffer, 1, buffer_len, file_);
  if ((result == 0) && (buffer_len > 0)) {
    if (feof(file_))
      return SR_EOS;
    if (error)
      *error = errno;
    return SR_ERROR;
  }
  if (read)
    *read = result;
  return SR_SUCCESS;
}

StreamResult FileStream::Write(const void* data,
                               size_t data_len,
                               size_t* written,
                               int* error) {
  if (!file_)
    return SR_EOS;
  size_t result = fwrite(data, 1, data_len, file_);
  if ((result == 0) && (data_len > 0)) {
    if (error)
      *error = errno;
    return SR_ERROR;
  }
  if (written)
    *written = result;
  return SR_SUCCESS;
}

void FileStream::Close() {
  if (file_) {
    DoClose();
    file_ = nullptr;
  }
}

bool FileStream::SetPosition(size_t position) {
  if (!file_)
    return false;
  return (fseek(file_, static_cast<int>(position), SEEK_SET) == 0);
}

bool FileStream::Flush() {
  if (file_) {
    return (0 == fflush(file_));
  }
  // try to flush empty file?
  RTC_NOTREACHED();
  return false;
}

void FileStream::DoClose() {
  fclose(file_);
}

}  // namespace rtc
