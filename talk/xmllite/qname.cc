/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
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

#include "talk/xmllite/qname.h"

namespace buzz {

QName::QName() {
}

QName::QName(const QName& qname)
    : namespace_(qname.namespace_),
      local_part_(qname.local_part_) {
}

QName::QName(const StaticQName& const_value)
    : namespace_(const_value.ns),
      local_part_(const_value.local) {
}

QName::QName(const std::string& ns, const std::string& local)
    : namespace_(ns),
      local_part_(local) {
}

QName::QName(const std::string& merged_or_local) {
  size_t i = merged_or_local.rfind(':');
  if (i == std::string::npos) {
    local_part_ = merged_or_local;
  } else {
    namespace_ = merged_or_local.substr(0, i);
    local_part_ = merged_or_local.substr(i + 1);
  }
}

QName::~QName() {
}

std::string QName::Merged() const {
  if (namespace_[0] == '\0')
    return local_part_;

  std::string result;
  result.reserve(namespace_.length() + 1 + local_part_.length());
  result += namespace_;
  result += ':';
  result += local_part_;
  return result;
}

bool QName::IsEmpty() const {
  return namespace_.empty() && local_part_.empty();
}

int QName::Compare(const StaticQName& other) const {
  int result = local_part_.compare(other.local);
  if (result != 0)
    return result;

  return namespace_.compare(other.ns);
}

int QName::Compare(const QName& other) const {
  int result = local_part_.compare(other.local_part_);
  if (result != 0)
    return result;

  return namespace_.compare(other.namespace_);
}

}  // namespace buzz
