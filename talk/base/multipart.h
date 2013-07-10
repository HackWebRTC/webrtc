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

#ifndef TALK_BASE_MULTIPART_H__
#define TALK_BASE_MULTIPART_H__

#include <string>
#include <vector>

#include "talk/base/sigslot.h"
#include "talk/base/stream.h"

namespace talk_base {

///////////////////////////////////////////////////////////////////////////////
// MultipartStream - Implements an RFC2046 multipart stream by concatenating
// the supplied parts together, and adding the correct boundaries.
///////////////////////////////////////////////////////////////////////////////

class MultipartStream : public StreamInterface, public sigslot::has_slots<> {
 public:
  MultipartStream(const std::string& type, const std::string& boundary);
  virtual ~MultipartStream();

  void GetContentType(std::string* content_type);

  // Note: If content_disposition and/or content_type are the empty string,
  // they will be omitted.
  bool AddPart(StreamInterface* data_stream,
               const std::string& content_disposition,
               const std::string& content_type);
  bool AddPart(const std::string& data,
               const std::string& content_disposition,
               const std::string& content_type);
  void EndParts();

  // Calculates the size of a part before actually adding the part.
  size_t GetPartSize(const std::string& data,
                     const std::string& content_disposition,
                     const std::string& content_type) const;
  size_t GetEndPartSize() const;

  // StreamInterface
  virtual StreamState GetState() const;
  virtual StreamResult Read(void* buffer, size_t buffer_len,
                            size_t* read, int* error);
  virtual StreamResult Write(const void* data, size_t data_len,
                             size_t* written, int* error);
  virtual void Close();
  virtual bool SetPosition(size_t position);
  virtual bool GetPosition(size_t* position) const;
  virtual bool GetSize(size_t* size) const;
  virtual bool GetAvailable(size_t* size) const;

 private:
  typedef std::vector<StreamInterface*> PartList;

  // StreamInterface Slots
  void OnEvent(StreamInterface* stream, int events, int error);

  std::string type_, boundary_;
  PartList parts_;
  bool adding_;
  size_t current_;  // The index into parts_ of the current read position.
  size_t position_;  // The current read position in bytes.

  DISALLOW_COPY_AND_ASSIGN(MultipartStream);
};

}  // namespace talk_base

#endif  // TALK_BASE_MULTIPART_H__
