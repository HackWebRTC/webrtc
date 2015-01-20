/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#ifndef TALK_APP_WEBRTC_DTMFSENDERINTERFACE_H_
#define TALK_APP_WEBRTC_DTMFSENDERINTERFACE_H_

#include <string>

#include "talk/app/webrtc/mediastreaminterface.h"
#include "webrtc/base/common.h"
#include "webrtc/base/refcount.h"

// This file contains interfaces for DtmfSender.

namespace webrtc {

// DtmfSender callback interface. Application should implement this interface
// to get notifications from the DtmfSender.
class DtmfSenderObserverInterface {
 public:
  // Triggered when DTMF |tone| is sent.
  // If |tone| is empty that means the DtmfSender has sent out all the given
  // tones.
  virtual void OnToneChange(const std::string& tone) = 0;

 protected:
  virtual ~DtmfSenderObserverInterface() {}
};

// The interface of native implementation of the RTCDTMFSender defined by the
// WebRTC W3C Editor's Draft.
class DtmfSenderInterface : public rtc::RefCountInterface {
 public:
  virtual void RegisterObserver(DtmfSenderObserverInterface* observer) = 0;
  virtual void UnregisterObserver() = 0;

  // Returns true if this DtmfSender is capable of sending DTMF.
  // Otherwise returns false.
  virtual bool CanInsertDtmf() = 0;

  // Queues a task that sends the DTMF |tones|. The |tones| parameter is treated
  // as a series of characters. The characters 0 through 9, A through D, #, and
  // * generate the associated DTMF tones. The characters a to d are equivalent
  // to A to D. The character ',' indicates a delay of 2 seconds before
  // processing the next character in the tones parameter.
  // Unrecognized characters are ignored.
  // The |duration| parameter indicates the duration in ms to use for each
  // character passed in the |tones| parameter.
  // The duration cannot be more than 6000 or less than 70.
  // The |inter_tone_gap| parameter indicates the gap between tones in ms.
  // The |inter_tone_gap| must be at least 50 ms but should be as short as
  // possible.
  // If InsertDtmf is called on the same object while an existing task for this
  // object to generate DTMF is still running, the previous task is canceled.
  // Returns true on success and false on failure.
  virtual bool InsertDtmf(const std::string& tones, int duration,
                          int inter_tone_gap) = 0;

  // Returns the track given as argument to the constructor.
  virtual const AudioTrackInterface* track() const = 0;

  // Returns the tones remaining to be played out.
  virtual std::string tones() const = 0;

  // Returns the current tone duration value in ms.
  // This value will be the value last set via the InsertDtmf() method, or the
  // default value of 100 ms if InsertDtmf() was never called.
  virtual int duration() const = 0;

  // Returns the current value of the between-tone gap in ms.
  // This value will be the value last set via the InsertDtmf() method, or the
  // default value of 50 ms if InsertDtmf() was never called.
  virtual int inter_tone_gap() const = 0;

 protected:
  virtual ~DtmfSenderInterface() {}
};

}  // namespace webrtc

#endif  // TALK_APP_WEBRTC_DTMFSENDERINTERFACE_H_
