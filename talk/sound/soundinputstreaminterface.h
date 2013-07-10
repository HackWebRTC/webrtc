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

#ifndef TALK_SOUND_SOUNDINPUTSTREAMINTERFACE_H_
#define TALK_SOUND_SOUNDINPUTSTREAMINTERFACE_H_

#include "talk/base/constructormagic.h"
#include "talk/base/sigslot.h"

namespace cricket {

// Interface for consuming an input stream from a recording device.
// Semantics and thread-safety of StartReading()/StopReading() are the same as
// for talk_base::Worker.
class SoundInputStreamInterface {
 public:
  virtual ~SoundInputStreamInterface() {}

  // Starts the reading of samples on the current thread.
  virtual bool StartReading() = 0;
  // Stops the reading of samples.
  virtual bool StopReading() = 0;

  // Retrieves the current input volume for this stream. Nominal range is
  // defined by SoundSystemInterface::k(Max|Min)Volume, but values exceeding the
  // max may be possible in some implementations. This call retrieves the actual
  // volume currently in use by the OS, not a cached value from a previous
  // (Get|Set)Volume() call.
  virtual bool GetVolume(int *volume) = 0;

  // Changes the input volume for this stream. Nominal range is defined by
  // SoundSystemInterface::k(Max|Min)Volume. The effect of exceeding kMaxVolume
  // is implementation-defined.
  virtual bool SetVolume(int volume) = 0;

  // Closes this stream object. If currently reading then this may only be
  // called from the reading thread.
  virtual bool Close() = 0;

  // Get the latency of the stream.
  virtual int LatencyUsecs() = 0;

  // Notifies the consumer of new data read from the device.
  // The first parameter is a pointer to the data read, and is only valid for
  // the duration of the call.
  // The second parameter is the amount of data read in bytes (i.e., the valid
  // length of the memory pointed to).
  // The 3rd parameter is the stream that is issuing the callback.
  sigslot::signal3<const void *, size_t,
      SoundInputStreamInterface *> SignalSamplesRead;

 protected:
  SoundInputStreamInterface() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SoundInputStreamInterface);
};

}  // namespace cricket

#endif  // TALK_SOUND_SOUNDOUTPUTSTREAMINTERFACE_H_
