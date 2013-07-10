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

#include "talk/base/versionparsing.h"

#include <cstdlib>

namespace talk_base {

bool ParseVersionString(const std::string& version_str,
                        int num_expected_segments,
                        int version[]) {
  size_t pos = 0;
  for (int i = 0;;) {
    size_t dot_pos = version_str.find('.', pos);
    size_t n;
    if (dot_pos == std::string::npos) {
      // npos here is a special value meaning "to the end of the string"
      n = std::string::npos;
    } else {
      n = dot_pos - pos;
    }

    version[i] = atoi(version_str.substr(pos, n).c_str());

    if (++i >= num_expected_segments) break;

    if (dot_pos == std::string::npos) {
      // Previous segment was not terminated by a dot, but there's supposed to
      // be more segments, so that's an error.
      return false;
    }
    pos = dot_pos + 1;
  }
  return true;
}

int CompareVersions(const int version1[],
                    const int version2[],
                    int num_segments) {
  for (int i = 0; i < num_segments; ++i) {
    int diff = version1[i] - version2[i];
    if (diff != 0) {
      return diff;
    }
  }
  return 0;
}

}  // namespace talk_base
