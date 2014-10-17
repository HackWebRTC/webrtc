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

#include <sstream>
#include "talk/p2p/base/transportchannel.h"

namespace cricket {

std::string TransportChannel::ToString() const {
  const char READABLE_ABBREV[2] = { '_', 'R' };
  const char WRITABLE_ABBREV[2] = { '_', 'W' };
  std::stringstream ss;
  ss << "Channel[" << content_name_
     << "|" << component_
     << "|" << READABLE_ABBREV[readable_] << WRITABLE_ABBREV[writable_] << "]";
  return ss.str();
}

void TransportChannel::set_readable(bool readable) {
  if (readable_ != readable) {
    readable_ = readable;
    SignalReadableState(this);
  }
}

void TransportChannel::set_writable(bool writable) {
  if (writable_ != writable) {
    writable_ = writable;
    if (writable_) {
      SignalReadyToSend(this);
    }
    SignalWritableState(this);
  }
}

}  // namespace cricket
