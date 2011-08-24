/*
 * libjingle
 * Copyright 2011, Google Inc.
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

#ifndef TALK_APP_WEBRTC_MEDIA_STREAM_IMPL_H_
#define TALK_APP_WEBRTC_MEDIA_STREAM_IMPL_H_

#include <string>
#include <vector>

#include "talk/app/webrtc/notifier_impl.h"
#include "talk/app/webrtc/ref_count.h"
#include "talk/app/webrtc/scoped_refptr.h"
#include "talk/app/webrtc/stream_dev.h"

namespace webrtc {

// MediaStreamImpl- help class for implementing the MediaStream interface.
class MediaStreamImpl {
 public:
  explicit MediaStreamImpl(const std::string& label);

  // Implement MediaStream
  const std::string& label() const;
  MediaStream::ReadyState ready_state() const;

 protected:
  std::string label_;
  MediaStream::ReadyState ready_state_;
};

class MediaStreamTrackListImpl {
 public:
  MediaStreamTrackListImpl();
  // Implement MediaStreamTrackList.
  bool AddTrack(MediaStreamTrack* track);
  size_t count() const;
  scoped_refptr<MediaStreamTrack> at(size_t index) const;
 protected:
  std::vector<scoped_refptr<MediaStreamTrack> > tracks_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIA_STREAM_IMPL_H_
