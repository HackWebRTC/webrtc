/*
 * libjingle
 * Copyright 2011 Google Inc.
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

#ifndef TALK_APP_WEBRTC_STREAMCOLLECTION_H_
#define TALK_APP_WEBRTC_STREAMCOLLECTION_H_

#include <string>
#include <vector>

#include "talk/app/webrtc/peerconnectioninterface.h"

namespace webrtc {

// Implementation of StreamCollection.
class StreamCollection : public StreamCollectionInterface {
 public:
  static rtc::scoped_refptr<StreamCollection> Create() {
    rtc::RefCountedObject<StreamCollection>* implementation =
         new rtc::RefCountedObject<StreamCollection>();
    return implementation;
  }

  static rtc::scoped_refptr<StreamCollection> Create(
      StreamCollection* streams) {
    rtc::RefCountedObject<StreamCollection>* implementation =
         new rtc::RefCountedObject<StreamCollection>(streams);
    return implementation;
  }

  virtual size_t count() {
    return media_streams_.size();
  }

  virtual MediaStreamInterface* at(size_t index) {
    return media_streams_.at(index);
  }

  virtual MediaStreamInterface* find(const std::string& label) {
    for (StreamVector::iterator it = media_streams_.begin();
         it != media_streams_.end(); ++it) {
      if ((*it)->label().compare(label) == 0) {
        return (*it);
      }
    }
    return NULL;
  }

  virtual MediaStreamTrackInterface* FindAudioTrack(
      const std::string& id) {
    for (size_t i = 0; i < media_streams_.size(); ++i) {
      MediaStreamTrackInterface* track = media_streams_[i]->FindAudioTrack(id);
      if (track) {
        return track;
      }
    }
    return NULL;
  }

  virtual MediaStreamTrackInterface* FindVideoTrack(
      const std::string& id) {
    for (size_t i = 0; i < media_streams_.size(); ++i) {
      MediaStreamTrackInterface* track = media_streams_[i]->FindVideoTrack(id);
      if (track) {
        return track;
      }
    }
    return NULL;
  }

  void AddStream(MediaStreamInterface* stream) {
    for (StreamVector::iterator it = media_streams_.begin();
         it != media_streams_.end(); ++it) {
      if ((*it)->label().compare(stream->label()) == 0)
        return;
    }
    media_streams_.push_back(stream);
  }

  void RemoveStream(MediaStreamInterface* remove_stream) {
    for (StreamVector::iterator it = media_streams_.begin();
         it != media_streams_.end(); ++it) {
      if ((*it)->label().compare(remove_stream->label()) == 0) {
        media_streams_.erase(it);
        break;
      }
    }
  }

 protected:
  StreamCollection() {}
  explicit StreamCollection(StreamCollection* original)
      : media_streams_(original->media_streams_) {
  }
  typedef std::vector<rtc::scoped_refptr<MediaStreamInterface> >
      StreamVector;
  StreamVector media_streams_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_STREAMCOLLECTION_H_
