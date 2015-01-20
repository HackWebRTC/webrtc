/*
 * libjingle
 * Copyright 2009 Google Inc.
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

/*
 * Author: lexnikitin@google.com (Alexey Nikitin)
 *
 * V4LLookup provides basic functionality to work with V2L2 devices in Linux
 * The functionality is implemented as a class with virtual methods for
 * the purpose of unit testing.
 */
#ifndef TALK_MEDIA_DEVICES_V4LLOOKUP_H_
#define TALK_MEDIA_DEVICES_V4LLOOKUP_H_

#include <string>

#ifdef LINUX
namespace cricket {
class V4LLookup {
 public:
  virtual ~V4LLookup() {}

  static bool IsV4L2Device(const std::string& device_path) {
    return GetV4LLookup()->CheckIsV4L2Device(device_path);
  }

  static void SetV4LLookup(V4LLookup* v4l_lookup) {
    v4l_lookup_ = v4l_lookup;
  }

  static V4LLookup* GetV4LLookup() {
    if (!v4l_lookup_) {
      v4l_lookup_ = new V4LLookup();
    }
    return v4l_lookup_;
  }

 protected:
  static V4LLookup* v4l_lookup_;
  // Making virtual so it is easier to mock
  virtual bool CheckIsV4L2Device(const std::string& device_path);
};

}  // namespace cricket

#endif  // LINUX
#endif  // TALK_MEDIA_DEVICES_V4LLOOKUP_H_
