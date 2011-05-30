/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * JPEG wrapper
 */

#ifndef WEBRTC_COMMON_VIDEO_JPEG
#define WEBRTC_COMMON_VIDEO_JPEG

#include "typedefs.h"

// jpeg forward declaration
struct jpeg_compress_struct;
struct jpeg_decompress_struct;

namespace webrtc
{

class JpegEncoder
{
public:
    JpegEncoder();
    ~JpegEncoder();

// SetFileName
// Input:
//  - fileName - Pointer to input vector (should be less than 256) to which the
//               compressed  file will be written to
//    Output:
//    - 0             : OK
//    - (-1)          : Error
    WebRtc_Word32 SetFileName(const WebRtc_Word8* fileName);

// Encode an I420 image. The encoded image is saved to a file
//
// Input:
//          - inputImage        : Image to be encoded
//
//    Output:
//    - 0             : OK
//    - (-1)          : Error
    WebRtc_Word32 Encode(const WebRtc_UWord8* imageBuffer,
                         const WebRtc_UWord32 imageBufferSize,
                         const WebRtc_UWord32 width,
                         const WebRtc_UWord32 height);

private:
    WebRtc_Word32 Encode(const WebRtc_UWord8* imageBuffer,
                         const WebRtc_UWord32 imageBufferSize);

    jpeg_compress_struct*   _cinfo;
    WebRtc_Word8            _fileName[256];
    WebRtc_UWord32          _width;
    WebRtc_UWord32          _height;
};

class JpegDecoder
{
 public:
    JpegDecoder();
    ~JpegDecoder();

//Decodes a JPEG-stream
//Supports 1 image component. 3 interleaved image components, YCbCr sub-sampling 4:4:4, 4:2:2, 4:2:0.
//
//Input:
//    - encodedBuffer     : Pointer to the encoded stream to be decoded.
//    - encodedBufferSize : Size of the data to be decoded
//    - decodedBuffer     : Reference to the destination of the decoded I420-image.
//    - width             : Reference returning width of decoded image.
//    - height            : Reference returning height of decoded image.
//
//    Output:
//    - 0             : OK
//    - (-1)          : Error
//Note: decodedBuffer should be freed by user
    WebRtc_Word32 Decode(const WebRtc_UWord8* encodedBuffer,
                         const WebRtc_UWord32 encodedBufferSize,
                         WebRtc_UWord8*& decodedBuffer,
                         WebRtc_UWord32& width,
                         WebRtc_UWord32& height);
 private:
    jpeg_decompress_struct*    _cinfo;
};


}
#endif /* WEBRTC_COMMON_VIDEO_JPEG  */
