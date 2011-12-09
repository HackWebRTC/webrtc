/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// system includes
#include <assert.h>
#include <string.h>     // memcpy

#include "test_buffer.h"
#include "common_video/libyuv/include/libyuv.h"

TestBuffer::TestBuffer():
_buffer(0),
_bufferSize(0),
_bufferLength(0),
_width(0),
_height(0)
{
    //
}

TestBuffer::~TestBuffer()
{
    _bufferLength = 0;
    _bufferSize = 0;
    if(_buffer)
    {
        delete [] _buffer;
        _buffer = 0;
    }
}

TestBuffer::TestBuffer(const TestBuffer& rhs)
:
_bufferLength(rhs._bufferLength),
_bufferSize(rhs._bufferSize),
_height(rhs._height),
_width(rhs._width),
_buffer(0)
{
    // make sure that our buffer is big enough
    _buffer = new WebRtc_UWord8[_bufferSize];
    // only copy required length 
    memcpy(_buffer, rhs._buffer, _bufferLength);
}

WebRtc_UWord32
TestBuffer::GetWidth() const
{
    return _width;
}

WebRtc_UWord32
TestBuffer::GetHeight() const
{
    return _height;
}

void            
TestBuffer::SetWidth(WebRtc_UWord32 width)
{
    _width = width;
}

void            
TestBuffer::SetHeight(WebRtc_UWord32 height)
{
    _height = height;
}

void
TestBuffer::Free()
{
    _bufferLength = 0;
    _bufferSize = 0;
    _height = 0;
    _width = 0;
    if(_buffer)
    {
        delete [] _buffer;
        _buffer = 0;
    }
}

void
TestBuffer::VerifyAndAllocate(WebRtc_UWord32 minimumSize)
{
    if(minimumSize > _bufferSize)
    {
        // make sure that our buffer is big enough
        WebRtc_UWord8 * newBufferBuffer = new WebRtc_UWord8[minimumSize];
        if(_buffer)
        {
            // copy the old data
            memcpy(newBufferBuffer, _buffer, _bufferSize);
            delete [] _buffer;
        }
        _buffer = newBufferBuffer;
        _bufferSize = minimumSize;
    }
}

void
TestBuffer::UpdateLength(WebRtc_UWord32 newLength)
{
    assert(newLength <= _bufferSize);
    _bufferLength = newLength;
}

void
TestBuffer::CopyBuffer(WebRtc_UWord32 length, const WebRtc_UWord8* buffer)
{
    assert(length <= _bufferSize);
     memcpy(_buffer, buffer, length);
    _bufferLength = length;
}

void
TestBuffer::CopyBuffer(TestBuffer& fromVideoBuffer)
{
    assert(fromVideoBuffer.GetLength() <= _bufferSize);
    assert(fromVideoBuffer.GetSize() <= _bufferSize);
    _bufferLength = fromVideoBuffer.GetLength();
    _height = fromVideoBuffer.GetHeight();
    _width = fromVideoBuffer.GetWidth();
    memcpy(_buffer, fromVideoBuffer.GetBuffer(), fromVideoBuffer.GetLength());
}

void
TestBuffer::Set(WebRtc_UWord8* tempBuffer,WebRtc_UWord32 tempSize, WebRtc_UWord32 tempLength)
{
    _buffer = tempBuffer;
    _bufferSize = tempSize;
    _bufferLength = tempLength;

}

WebRtc_UWord8*
TestBuffer::GetBuffer() const
{
  return _buffer;
}

WebRtc_UWord32
TestBuffer::GetSize() const
{
    return _bufferSize;
}

WebRtc_UWord32
TestBuffer::GetLength() const
{
    return _bufferLength;
}

