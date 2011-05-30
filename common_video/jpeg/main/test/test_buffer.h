/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef WEBRTC_COMMON_VIDEO_JPEG_TEST_BUFFER_H
#define WEBRTC_COMMON_VIDEO_JPEG_TEST_BUFFER_H

#include "typedefs.h"

class TestBuffer
{
public:
    TestBuffer();

    virtual ~TestBuffer();

    TestBuffer(const TestBuffer& rhs);

    /**
     * Verifies that current allocated buffer size is larger than or equal to the input size.
    * If the current buffer size is smaller, a new allocation is made and the old buffer data is copied to the new buffer.
    */
    void VerifyAndAllocate(WebRtc_UWord32 minimumSize);

    void UpdateLength(WebRtc_UWord32 newLength);



    void CopyBuffer(WebRtc_UWord32 length, const WebRtc_UWord8* fromBuffer);

    void CopyBuffer(TestBuffer& fromBuffer);

    void Free();    // Deletes frame buffer and resets members to zero

    /**
     *   Gets pointer to frame buffer
     */
    WebRtc_UWord8*    GetBuffer() const;

    /**
     *   Gets allocated buffer size
     */
    WebRtc_UWord32    GetSize() const;

    /**
     *   Gets length of frame
     */
    WebRtc_UWord32    GetLength() const;


    WebRtc_UWord32    GetWidth() const;
    WebRtc_UWord32    GetHeight() const;

    void            SetWidth(WebRtc_UWord32 width);
    void            SetHeight(WebRtc_UWord32 height);

private:
  //  TestBuffer& operator=(const TestBuffer& inBuffer);

private:
    void Set(WebRtc_UWord8* buffer,WebRtc_UWord32 size,WebRtc_UWord32 length);

    WebRtc_UWord8*      _buffer;          // Pointer to frame buffer
    WebRtc_UWord32      _bufferSize;      // Allocated buffer size
    WebRtc_UWord32      _bufferLength;    // Length (in bytes) of frame
    WebRtc_UWord32      _width;
    WebRtc_UWord32      _height;
};

#endif
