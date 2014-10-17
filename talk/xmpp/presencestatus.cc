/*
 * libjingle
 * Copyright 2004--2012, Google Inc.
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

#include "talk/xmpp/presencestatus.h"

namespace buzz {
PresenceStatus::PresenceStatus()
  : pri_(0),
    show_(SHOW_NONE),
    available_(false),
    e_code_(0),
    feedback_probation_(false),
    know_capabilities_(false),
    voice_capability_(false),
    pmuc_capability_(false),
    video_capability_(false),
    camera_capability_(false) {
}

void PresenceStatus::UpdateWith(const PresenceStatus& new_value) {
  if (!new_value.know_capabilities()) {
    bool k = know_capabilities();
    bool p = voice_capability();
    std::string node = caps_node();
    std::string v = version();

    *this = new_value;

    set_know_capabilities(k);
    set_caps_node(node);
    set_voice_capability(p);
     set_version(v);
  } else {
    *this = new_value;
  }
}

} // namespace buzz
