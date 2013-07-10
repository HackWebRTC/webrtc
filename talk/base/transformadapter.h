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

#ifndef TALK_BASE_TRANSFORMADAPTER_H__
#define TALK_BASE_TRANSFORMADAPTER_H__

#include "talk/base/stream.h"

namespace talk_base {
///////////////////////////////////////////////////////////////////////////////

class TransformInterface {
public:
  virtual ~TransformInterface() { }

  // Transform should convert the in_len bytes of input into the out_len-sized
  // output buffer.  If flush is true, there will be no more data following
  // input.
  // After the transformation, in_len contains the number of bytes consumed, and
  // out_len contains the number of bytes ready in output.
  // Note: Transform should not return SR_BLOCK, as there is no asynchronous
  // notification available.
  virtual StreamResult Transform(const void * input, size_t * in_len,
                                 void * output, size_t * out_len,
                                 bool flush) = 0;
};

///////////////////////////////////////////////////////////////////////////////

// TransformAdapter causes all data passed through to be transformed by the
// supplied TransformInterface object, which may apply compression, encryption,
// etc.

class TransformAdapter : public StreamAdapterInterface {
public:
  // Note that the transformation is unidirectional, in the direction specified
  // by the constructor.  Operations in the opposite direction result in SR_EOS.
  TransformAdapter(StreamInterface * stream,
                   TransformInterface * transform,
                   bool direction_read);
  virtual ~TransformAdapter();
  
  virtual StreamResult Read(void * buffer, size_t buffer_len,
                            size_t * read, int * error);
  virtual StreamResult Write(const void * data, size_t data_len,
                             size_t * written, int * error);
  virtual void Close();

  // Apriori, we can't tell what the transformation does to the stream length.
  virtual bool GetAvailable(size_t* size) const { return false; }
  virtual bool ReserveSize(size_t size) { return true; }

  // Transformations might not be restartable
  virtual bool Rewind() { return false; }

private:
  enum State { ST_PROCESSING, ST_FLUSHING, ST_COMPLETE, ST_ERROR };
  enum { BUFFER_SIZE = 1024 };

  TransformInterface * transform_;
  bool direction_read_;
  State state_;
  int error_;

  char buffer_[BUFFER_SIZE];
  size_t len_;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace talk_base

#endif // TALK_BASE_TRANSFORMADAPTER_H__
