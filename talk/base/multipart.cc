// libjingle
// Copyright 2004--2010, Google Inc.
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


#include "talk/base/common.h"
#include "talk/base/httpcommon.h"
#include "talk/base/multipart.h"

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////
// MultipartStream
///////////////////////////////////////////////////////////////////////////////

MultipartStream::MultipartStream(const std::string& type,
                                 const std::string& boundary)
    : type_(type),
      boundary_(boundary),
      adding_(true),
      current_(0),
      position_(0) {
  // The content type should be multipart/*.
  ASSERT(0 == strncmp(type_.c_str(), "multipart/", 10));
}

MultipartStream::~MultipartStream() {
  Close();
}

void MultipartStream::GetContentType(std::string* content_type) {
  ASSERT(NULL != content_type);
  content_type->assign(type_);
  content_type->append("; boundary=");
  content_type->append(boundary_);
}

bool MultipartStream::AddPart(StreamInterface* data_stream,
                              const std::string& content_disposition,
                              const std::string& content_type) {
  if (!AddPart("", content_disposition, content_type))
    return false;
  parts_.push_back(data_stream);
  data_stream->SignalEvent.connect(this, &MultipartStream::OnEvent);
  return true;
}

bool MultipartStream::AddPart(const std::string& data,
                              const std::string& content_disposition,
                              const std::string& content_type) {
  ASSERT(adding_);
  if (!adding_)
    return false;
  std::stringstream ss;
  if (!parts_.empty()) {
    ss << "\r\n";
  }
  ss << "--" << boundary_ << "\r\n";
  if (!content_disposition.empty()) {
    ss << ToString(HH_CONTENT_DISPOSITION) << ": "
       << content_disposition << "\r\n";
  }
  if (!content_type.empty()) {
    ss << ToString(HH_CONTENT_TYPE) << ": "
       << content_type << "\r\n";
  }
  ss << "\r\n" << data;
  parts_.push_back(new MemoryStream(ss.str().data(), ss.str().size()));
  return true;
}

void MultipartStream::EndParts() {
  ASSERT(adding_);
  if (!adding_)
    return;

  std::stringstream ss;
  if (!parts_.empty()) {
    ss << "\r\n";
  }
  ss << "--" << boundary_ << "--" << "\r\n";
  parts_.push_back(new MemoryStream(ss.str().data(), ss.str().size()));

  ASSERT(0 == current_);
  ASSERT(0 == position_);
  adding_ = false;
  SignalEvent(this, SE_OPEN | SE_READ, 0);
}

size_t MultipartStream::GetPartSize(const std::string& data,
                                    const std::string& content_disposition,
                                    const std::string& content_type) const {
  size_t size = 0;
  if (!parts_.empty()) {
    size += 2;  // for "\r\n";
  }
  size += boundary_.size() + 4;  // for "--boundary_\r\n";
  if (!content_disposition.empty()) {
    // for ToString(HH_CONTENT_DISPOSITION): content_disposition\r\n
    size += std::string(ToString(HH_CONTENT_DISPOSITION)).size() + 2 +
        content_disposition.size() + 2;
  }
  if (!content_type.empty()) {
    // for ToString(HH_CONTENT_TYPE): content_type\r\n
    size += std::string(ToString(HH_CONTENT_TYPE)).size() + 2 +
        content_type.size() + 2;
  }
  size += 2 + data.size();  // for \r\ndata
  return size;
}

size_t MultipartStream::GetEndPartSize() const {
  size_t size = 0;
  if (!parts_.empty()) {
    size += 2;  // for "\r\n";
  }
  size += boundary_.size() + 6;  // for "--boundary_--\r\n";
  return size;
}

//
// StreamInterface
//

StreamState MultipartStream::GetState() const {
  if (adding_) {
    return SS_OPENING;
  }
  return (current_ < parts_.size()) ? SS_OPEN : SS_CLOSED;
}

StreamResult MultipartStream::Read(void* buffer, size_t buffer_len,
                                   size_t* read, int* error) {
  if (adding_) {
    return SR_BLOCK;
  }
  size_t local_read;
  if (!read) read = &local_read;
  while (current_ < parts_.size()) {
    StreamResult result = parts_[current_]->Read(buffer, buffer_len, read,
                                                 error);
    if (SR_EOS != result) {
      if (SR_SUCCESS == result) {
        position_ += *read;
      }
      return result;
    }
    ++current_;
  }
  return SR_EOS;
}

StreamResult MultipartStream::Write(const void* data, size_t data_len,
                                    size_t* written, int* error) {
  if (error) {
    *error = -1;
  }
  return SR_ERROR;
}

void MultipartStream::Close() {
  for (size_t i = 0; i < parts_.size(); ++i) {
    delete parts_[i];
  }
  parts_.clear();
  adding_ = false;
  current_ = 0;
  position_ = 0;
}

bool MultipartStream::SetPosition(size_t position) {
  if (adding_) {
    return false;
  }
  size_t part_size, part_offset = 0;
  for (size_t i = 0; i < parts_.size(); ++i) {
    if (!parts_[i]->GetSize(&part_size)) {
      return false;
    }
    if (part_offset + part_size > position) {
      for (size_t j = i+1; j < _min(parts_.size(), current_+1); ++j) {
        if (!parts_[j]->Rewind()) {
          return false;
        }
      }
      if (!parts_[i]->SetPosition(position - part_offset)) {
        return false;
      }
      current_ = i;
      position_ = position;
      return true;
    }
    part_offset += part_size;
  }
  return false;
}

bool MultipartStream::GetPosition(size_t* position) const {
  if (position) {
    *position = position_;
  }
  return true;
}

bool MultipartStream::GetSize(size_t* size) const {
  size_t part_size, total_size = 0;
  for (size_t i = 0; i < parts_.size(); ++i) {
    if (!parts_[i]->GetSize(&part_size)) {
      return false;
    }
    total_size += part_size;
  }
  if (size) {
    *size = total_size;
  }
  return true;
}

bool MultipartStream::GetAvailable(size_t* size) const {
  if (adding_) {
    return false;
  }
  size_t part_size, total_size = 0;
  for (size_t i = current_; i < parts_.size(); ++i) {
    if (!parts_[i]->GetAvailable(&part_size)) {
      return false;
    }
    total_size += part_size;
  }
  if (size) {
    *size = total_size;
  }
  return true;
}

//
// StreamInterface Slots
//

void MultipartStream::OnEvent(StreamInterface* stream, int events, int error) {
  if (adding_ || (current_ >= parts_.size()) || (parts_[current_] != stream)) {
    return;
  }
  SignalEvent(this, events, error);
}

}  // namespace talk_base
