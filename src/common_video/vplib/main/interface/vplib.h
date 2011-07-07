/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef WEBRTC_COMMON_VIDEO_INTERFACE_VPLIB_H
#define WEBRTC_COMMON_VIDEO_INTERFACE_VPLIB_H


#include "typedefs.h"

namespace webrtc
{

// Supported video types
enum VideoType
{
    kUnknown,
    kI420,
    kIYUV,
    kRGB24,
    kARGB,
    kARGB4444,
    kRGB565,
    kARGB1555,
    kYUY2,
    kYV12,
    kUYVY,
    kMJPG,
    kNV21,
    kNV12,
    kARGBMac,
    kRGBAMac,

    kNumberOfVideoTypes
};


// Supported rotation
enum VideoRotationMode
{
    kRotateNone = 0,
    kRotateClockwise = 90,
    kRotateAntiClockwise = -90,
    kRotate180 = 180,
};


    // Calculate the required buffer size
    // Input
    //    - type - The type of the designated video frame
    //    - width - frame width in pixels
    //    - height - frame height in pixels
    // Output
    //    - The required size in bytes to accommodate the specified video frame
    //
    WebRtc_UWord32 CalcBufferSize(VideoType type, WebRtc_UWord32 width, WebRtc_UWord32 height);


    // Calculate the required buffer size when converting from one type to another
    // Input
    //   - incomingVideoType - The type of the existing video frame
    //   - convertedVideoType - width of the designated video frame
    //   - length - length in bytes of the data
    // Output
    //   - The required size in bytes to accommodate the specified converted video frame
    //
    WebRtc_UWord32 CalcBufferSize(VideoType incomingVideoType, VideoType convertedVideoType,
                                  WebRtc_UWord32 length);

    //
    // Convert To/From I420
    //
    // The following 2 functions convert an image to/from a I420 type to/from a specified
    // format.
    //
    // Input:
    //    - incomingVideoType  : Type of input video
    //    - incomingBuffer     : Pointer to an input image.
    //    - width              : Image width in pixels.
    //    - height             : Image height in pixels.
    //    - outgoingBuffer     : Pointer to converted image.
    //    - interlaced         : Flag indicating if interlaced I420 output
    //    - rotate             : Rotation mode of output image
    // Return value            : Size of converted image if OK, otherwise, the following error
    //                           codes:
    //                           -1 : Parameter error
    //                           -2 : Unsupported command (parameter request)
    //
    // Note: the following functions includes the most common usage cases; for a more general
    // usage, refer to explicit function
    WebRtc_Word32 ConvertToI420(VideoType incomingVideoType,
                                const WebRtc_UWord8* incomingBuffer,
                                WebRtc_UWord32 width,
                                WebRtc_UWord32 height,
                                WebRtc_UWord8* outgoingBuffer,
                                bool interlaced = false ,
                                VideoRotationMode rotate  = kRotateNone
                                );

    WebRtc_Word32 ConvertFromI420(VideoType outgoingVideoType,
                                  const WebRtc_UWord8* incomingBuffer,
                                  WebRtc_UWord32 width,
                                  WebRtc_UWord32 height,
                                  WebRtc_UWord8* outgoingBuffer,
                                  bool interlaced = false ,
                                  VideoRotationMode rotate = kRotateNone
                                  );

    // Designated Convert Functions
    // The following list describes the designated conversion function which are called by the
    // 2 prior general conversion function.
    // Input and output descriptions match the above descriptions, and are therefore omitted.
    WebRtc_Word32 ConvertI420ToRGB24(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                     WebRtc_UWord32 width, WebRtc_UWord32 height);
    WebRtc_Word32 ConvertI420ToARGB(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                    WebRtc_UWord32 width, WebRtc_UWord32 height,
                                    WebRtc_UWord32 strideOut);
    WebRtc_Word32 ConvertI420ToARGB4444(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                        WebRtc_UWord32 width, WebRtc_UWord32 height,
                                        WebRtc_UWord32 strideOut);
    WebRtc_Word32 ConvertI420ToRGB565(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                      WebRtc_UWord32 width, WebRtc_UWord32 height);
    WebRtc_Word32 ConvertI420ToRGB565Android(const WebRtc_UWord8* inFrame,
                                             WebRtc_UWord8* outFrame, WebRtc_UWord32 width,
                                             WebRtc_UWord32 height);
    WebRtc_Word32 ConvertI420ToARGB1555(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                        WebRtc_UWord32 width, WebRtc_UWord32 height,
                                        WebRtc_UWord32 strideOut);
    WebRtc_Word32 ConvertI420ToARGBMac(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                       WebRtc_UWord32 width, WebRtc_UWord32 height,
                                       WebRtc_UWord32 strideOut);
    WebRtc_Word32 ConvertI420ToRGBAMac(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                       WebRtc_UWord32 width, WebRtc_UWord32 height,
                                       WebRtc_UWord32 strideOut);
    WebRtc_Word32 ConvertI420ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                    WebRtc_UWord32 width, WebRtc_UWord32 height,
                                    WebRtc_UWord32 strideOut = 0);
    WebRtc_Word32 ConvertI420ToUYVY(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                    WebRtc_UWord32 width, WebRtc_UWord32 height,
                                    WebRtc_UWord32 strideOut = 0);
    WebRtc_Word32 ConvertI420ToYUY2(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                    WebRtc_UWord32 width, WebRtc_UWord32 height,
                                    WebRtc_UWord32 strideOut = 0);
    WebRtc_Word32 ConvertI420ToYV12(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                    WebRtc_UWord32 width, WebRtc_UWord32 height,
                                    WebRtc_UWord32 strideOut);
    WebRtc_Word32 ConvertYUY2ToI420interlaced(const WebRtc_UWord8* inFrame, WebRtc_UWord32 inWidth,
                                              WebRtc_UWord32 inHeight, WebRtc_UWord8* outFrame,
                                              WebRtc_UWord32 outWidth, WebRtc_UWord32 outHeight);
    WebRtc_Word32 ConvertYV12ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord32 width,
                                    WebRtc_UWord32 height, WebRtc_UWord8* outFrame);
    WebRtc_Word32 ConvertRGB24ToARGB(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                     WebRtc_UWord32 width, WebRtc_UWord32 height,
                                     WebRtc_UWord32 strideOut);
    WebRtc_Word32 ConvertRGB24ToI420(WebRtc_Word32 width, WebRtc_Word32 height,
                                     const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame);
    WebRtc_Word32 ConvertRGB565ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord32 width,
                                      WebRtc_UWord32 height, WebRtc_UWord8* outFrame);
    WebRtc_Word32 ConvertARGBMacToI420(WebRtc_UWord32 width, WebRtc_UWord32 height,
                                    const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame);
    WebRtc_Word32 ConvertUYVYToI420(WebRtc_UWord32 width, WebRtc_UWord32 height,
                                    const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame);

    // pad cut and convert
    WebRtc_Word32 ConvertYUY2ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord32 inWidth,
                                    WebRtc_UWord32 inHeight, WebRtc_UWord8* outFrame,
                                    WebRtc_UWord32 outWidth, WebRtc_UWord32 outHeight);
    WebRtc_Word32 ConvertUYVYToI420interlaced(const WebRtc_UWord8* inFrame,
                                              WebRtc_UWord32 inWidth, WebRtc_UWord32 inHeight,
                                              WebRtc_UWord8* outFrame, WebRtc_UWord32 outWidth,
                                              WebRtc_UWord32 outHeight);
    WebRtc_Word32 ConvertRGB24ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord32 inWidth,
                                     WebRtc_UWord32 inHeight, WebRtc_UWord8* outFrame,
                                     WebRtc_UWord32 outWidth, WebRtc_UWord32 outHeight);
    WebRtc_Word32 ConvertI420ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord32 inWidth,
                                    WebRtc_UWord32 inHeight, WebRtc_UWord8* outFrame,
                                    WebRtc_UWord32 outWidth, WebRtc_UWord32 outHeight);

    //NV12 Conversion/Rotation
    WebRtc_Word32 ConvertNV12ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                    WebRtc_UWord32 width, WebRtc_UWord32 height);
    WebRtc_Word32 ConvertNV12ToI420AndRotate180(const WebRtc_UWord8* inFrame,
                                                WebRtc_UWord8* outFrame, WebRtc_UWord32 width,
                                                WebRtc_UWord32 height);
    WebRtc_Word32 ConvertNV12ToI420AndRotateAntiClockwise(const WebRtc_UWord8* inFrame,
                                                          WebRtc_UWord8* outFrame,
                                                          WebRtc_UWord32 width,
                                                          WebRtc_UWord32 height);
    WebRtc_Word32 ConvertNV12ToI420AndRotateClockwise(const WebRtc_UWord8* inFrame,
                                                      WebRtc_UWord8* outFrame,
                                                      WebRtc_UWord32 width,
                                                      WebRtc_UWord32 height);
    WebRtc_Word32 ConvertNV12ToRGB565(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                      WebRtc_UWord32 width, WebRtc_UWord32 height);

    //NV21 Conversion/Rotation
    WebRtc_Word32 ConvertNV21ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                    WebRtc_UWord32 width, WebRtc_UWord32 height);
    WebRtc_Word32 ConvertNV21ToI420AndRotate180(const WebRtc_UWord8* inFrame,
                                                WebRtc_UWord8* outFrame,
                                                WebRtc_UWord32 width, WebRtc_UWord32 height);
    WebRtc_Word32 ConvertNV21ToI420AndRotateAntiClockwise(const WebRtc_UWord8* inFrame,
                                                          WebRtc_UWord8* outFrame,
                                                          WebRtc_UWord32 width,
                                                          WebRtc_UWord32 height);
    WebRtc_Word32 ConvertNV21ToI420AndRotateClockwise(const WebRtc_UWord8* inFrame,
                                                      WebRtc_UWord8* outFrame,
                                                      WebRtc_UWord32 width,
                                                      WebRtc_UWord32 height);

    //IPhone
    WebRtc_Word32 ConvertI420ToRGBAIPhone(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                          WebRtc_UWord32 width, WebRtc_UWord32 height,
                                          WebRtc_UWord32 strideOut);

    // I420 Cut and Pad - make a center cut
    WebRtc_Word32 CutI420Frame(WebRtc_UWord8* frame, WebRtc_UWord32 fromWidth,
                               WebRtc_UWord32 fromHeight, WebRtc_UWord32 toWidth,
                               WebRtc_UWord32 toHeight);

    // Pad an I420 frame
    // Input:
    //    - inBuffer       : Pointer to the input image component to be padded.
    //    - outBuffer      : Pointer to the output padded image component.
    //    - fromWidth      : Width in pixels of the inBuffer component.
    //    - fromHeight     : Height in pixels of the inBuffer component.
    //    - toWidth        : Width in pixels of the outBuffer component.
    //    - toHeight       : Height in pixels of the outBuffer component.
    // Return Value:
    //      - Length of the output component.
    WebRtc_Word32 PadI420Frame(const WebRtc_UWord8* inBuffer, WebRtc_UWord8* outBuffer,
                               WebRtc_UWord32 fromWidth, WebRtc_UWord32 fromHeight,
                               WebRtc_UWord32 toWidth, WebRtc_UWord32 toHeight);
    WebRtc_Word32 PadI420BottomRows(WebRtc_UWord8* buffer, WebRtc_UWord32 size,
                                    WebRtc_UWord32 width, WebRtc_UWord32 height,
                                    WebRtc_Word32 nrRows, WebRtc_UWord32& newLength);

    // I420 Scale
    // Scale an I420 frame:Half frame, quarter frame
    // Input:
    //    - inFrame     : Pointer to the image component to be scaled
    //    - width          : Width in pixels of the output frame.
    //    - height         : Height in pixels of the out frame.
    // Return Value:
    //      - Length of the output component.
    WebRtc_Word32 ScaleI420FrameQuarter(WebRtc_UWord32 width, WebRtc_UWord32 height,
                                        WebRtc_UWord8* inFrame);
    WebRtc_Word32 ScaleI420DownHalfFrame(WebRtc_UWord32 width, WebRtc_UWord32 height,
                                         WebRtc_UWord8* inFrame);
    WebRtc_Word32 ScaleI420UpHalfFrame(WebRtc_UWord32 width, WebRtc_UWord32 height,
                                       WebRtc_UWord8* inFrame);

    // Scales up an I420-frame to twice its width and height. Interpolates by using mean value
    // of neighboring pixels.
    // The following two function allow up-scaling by either twice or 3/2 of the original
    // the width and height
    // Input:
    //    - width          : Width of input frame in pixels.
    //    - height         : Height of input frame in pixels.
    //    - buffer         : Reference to a buffer containing the frame.
    //    - size           :Size of allocated buffer
    //    - scaledWidth    : Reference to the width of scaled frame in pixels.
    //    - scaledHeight   : Reference to the height of scaled frame in pixels.
    // Return value:
    //    - (length)       : Length of scaled frame.
    //    - (-1)           : Error.
    WebRtc_Word32 ScaleI420Up2(WebRtc_UWord32 width, WebRtc_UWord32 height,
                               WebRtc_UWord8*& buffer, WebRtc_UWord32 size,
                               WebRtc_UWord32 &scaledWidth, WebRtc_UWord32 &scaledHeight);
    WebRtc_Word32 ScaleI420Up3_2(WebRtc_UWord32 width, WebRtc_UWord32 height,
                                 WebRtc_UWord8*& buffer, WebRtc_UWord32 size,
                                 WebRtc_UWord32 &scaledWidth, WebRtc_UWord32 &scaledHeight);

    // Scales down an I420-frame to one third its width and height.
    // Input:
    //    - width          : Width of frame in pixels.
    //    - height         : Height of frame in pixels.
    //    - videoBuffer    : Reference to a buffer containing the frame.
    //    - scaledWidth    : Width of scaled frame in pixels.
    //    - scaledHeight   : Height of scaled frame in pixels.
    // Return value:
    //    - (length)       : Length of scaled frame.
    //    - (-1)           : Error.
    WebRtc_Word32 ScaleI420Down1_3(WebRtc_UWord32 width, WebRtc_UWord32 height,
                                   WebRtc_UWord8*& buffer, WebRtc_UWord32 size,
                                   WebRtc_UWord32 &scaledWidth, WebRtc_UWord32 &scaledHeight);

    // Convert From I420/YV12 to I420 and Rotate clockwise
    // Input:
    //    - srcBuffer    : Reference to a buffer containing the source frame.
    //    - srcWidth     : Width of source frame in pixels.
    //    - srcHeight    : Height of source frame in pixels.
    //    - dstBuffer    : Reference to a buffer containing the destination frame.
    //    - dstWidth     : Width of destination frame in pixels.
    //    - dstHeight    : Height of destination frame in pixels.
    //    - colorSpaceIn : Input color space
    // Return value:
    //    - (length)       : Length of scaled frame.
    //    - (-1)           : Error.
    WebRtc_Word32 ConvertToI420AndRotateClockwise(const WebRtc_UWord8* srcBuffer,
                                                  WebRtc_UWord32 srcWidth,
                                                  WebRtc_UWord32 srcHeight,
                                                  WebRtc_UWord8* dstBuffer,
                                                  WebRtc_UWord32 dstWidth,
                                                  WebRtc_UWord32 dstHeight,
                                                  VideoType colorSpaceIn);

    // Convert From I420/YV12 to I420 and Rotate anti clockwise
    // Inputs/outputs as the above function
    WebRtc_Word32 ConvertToI420AndRotateAntiClockwise(const WebRtc_UWord8* srcBuffer,
                                                      WebRtc_UWord32 srcWidth,
                                                      WebRtc_UWord32 srcHeight,
                                                      WebRtc_UWord8* dstBuffer,
                                                      WebRtc_UWord32 dstWidth,
                                                      WebRtc_UWord32 dstHeight,
                                                      VideoType colorSpaceIn);

    // Mirror functions
    // The following 2 functions perform mirroring on a given image (LeftRight/UpDown)
    // Input:
    //    - width       : Image width in pixels.
    //    - height      : Image height in pixels.
    //    - inFrame     : Reference to input image.
    //    - outFrame    : Reference to converted image.
    // Return value: 0 if OK, < 0 otherwise.
    WebRtc_Word32 MirrorI420LeftRight(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                      WebRtc_UWord32 width, WebRtc_UWord32 height);
    WebRtc_Word32 MirrorI420UpDown(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                                   WebRtc_UWord32 width, WebRtc_UWord32 height);

    // Mirror functions - Don't work in place (srcBuffer == dstBuffer),
    // and are therefore faster. Also combine mirroring with conversion to speed things up.
    // Input:
    //    - srcBuffer       : Pointer to source image.
    //    - dstBuffer       : Pointer to destination image.
    //    - srcWidth        : Width of input buffer.
    //    - srcHeight       : Height of input buffer.
    //    - colorSpaceIn    : Color space to convert from, I420 if no conversion should be done
    //    - dstBuffer       : Pointer to converted/rotated image.
    // Return value:      0 if OK, < 0 otherwise.
    WebRtc_Word32 ConvertToI420AndMirrorUpDown(const WebRtc_UWord8* srcBuffer,
                                               WebRtc_UWord8* dstBuffer,
                                               WebRtc_UWord32 srcWidth,
                                               WebRtc_UWord32 srcHeight,
                                               VideoType colorSpaceIn = kI420);

}

#endif
