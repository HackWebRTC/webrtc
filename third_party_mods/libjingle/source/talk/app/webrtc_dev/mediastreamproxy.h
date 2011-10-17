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

#ifndef TALK_APP_WEBRTC_MEDIASTREAMPROXY_H_
#define TALK_APP_WEBRTC_MEDIASTREAMPROXY_H_

#include <string>
#include <vector>

#include "talk/app/webrtc_dev/mediastreamimpl.h"
#include "talk/base/thread.h"

namespace webrtc {

// MediaStreamProxy is a proxy for the MediaStream interface. The purpose is
// to make sure MediaStreamImpl is only accessed from the signaling thread.
// It can be used as a proxy for both local and remote MediaStreams.
class MediaStreamProxy : public LocalMediaStreamInterface,
                         public talk_base::MessageHandler {
 public:
  class MediaStreamTrackListProxy : public MediaStreamTrackListInterface,
                                    public talk_base::MessageHandler {
   public:
    MediaStreamTrackListProxy(MediaStreamTrackListInterface* track_list,
                              talk_base::Thread* signaling_thread);
    virtual size_t count();
    virtual MediaStreamTrackInterface* at(size_t index);

    // Implement Notifier
    virtual void RegisterObserver(Observer* observer);
    virtual void UnregisterObserver(Observer* observer);
   private:
    void Send(uint32 id, talk_base::MessageData* data) const;
    void OnMessage(talk_base::Message* msg);

    scoped_refptr<MediaStreamTrackListInterface> track_list_;
    mutable talk_base::Thread* signaling_thread_;
  };

  static scoped_refptr<MediaStreamProxy> Create(
      const std::string& label,
      talk_base::Thread* signaling_thread);

  // Implement LocalStream.
  virtual bool AddTrack(MediaStreamTrackInterface* track);

  // Implement MediaStream.
  virtual std::string label() const;
  virtual MediaStreamTrackListInterface* tracks();
  virtual ReadyState ready_state();
  virtual void set_ready_state(ReadyState new_state);

  // Implement Notifier
  virtual void RegisterObserver(Observer* observer);
  virtual void UnregisterObserver(Observer* observer);

 protected:
  explicit MediaStreamProxy(const std::string& label,
                            talk_base::Thread* signaling_thread);

  void Send(uint32 id, talk_base::MessageData* data) const;
  // Implement MessageHandler
  virtual void OnMessage(talk_base::Message* msg);

  mutable talk_base::Thread* signaling_thread_;
  scoped_refptr<MediaStreamImpl> media_stream_impl_;
  scoped_refptr<MediaStreamTrackListProxy> track_list_;
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_MEDIASTREAMPROXY_H_
