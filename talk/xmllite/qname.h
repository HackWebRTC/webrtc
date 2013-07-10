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

#ifndef TALK_XMLLITE_QNAME_H_
#define TALK_XMLLITE_QNAME_H_

#include <string>

namespace buzz {

class QName;

// StaticQName is used to represend constant quailified names. They
// can be initialized statically and don't need intializers code, e.g.
//   const StaticQName QN_FOO = { "foo_namespace", "foo" };
//
// Beside this use case, QName should be used everywhere
// else. StaticQName instances are implicitly converted to QName
// objects.
struct StaticQName {
  const char* const ns;
  const char* const local;

  bool operator==(const QName& other) const;
  bool operator!=(const QName& other) const;
};

class QName {
 public:
  QName();
  QName(const QName& qname);
  QName(const StaticQName& const_value);
  QName(const std::string& ns, const std::string& local);
  explicit QName(const std::string& merged_or_local);
  ~QName();

  const std::string& Namespace() const { return namespace_; }
  const std::string& LocalPart() const { return local_part_; }
  std::string Merged() const;
  bool IsEmpty() const;

  int Compare(const StaticQName& other) const;
  int Compare(const QName& other) const;

  bool operator==(const StaticQName& other) const {
    return Compare(other) == 0;
  }
  bool operator==(const QName& other) const {
    return Compare(other) == 0;
  }
  bool operator!=(const StaticQName& other) const {
    return Compare(other) != 0;
  }
  bool operator!=(const QName& other) const {
    return Compare(other) != 0;
  }
  bool operator<(const QName& other) const {
    return Compare(other) < 0;
  }

 private:
  std::string namespace_;
  std::string local_part_;
};

inline bool StaticQName::operator==(const QName& other) const {
  return other.Compare(*this) == 0;
}

inline bool StaticQName::operator!=(const QName& other) const {
  return other.Compare(*this) != 0;
}

}  // namespace buzz

#endif  // TALK_XMLLITE_QNAME_H_
