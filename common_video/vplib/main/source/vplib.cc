/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vplib.h"

#include <string.h>     // memcpy(), memset()
#include <assert.h>
#include <stdlib.h>     // abs

//#define SCALEOPT //Currently for windows only. June 2010

#ifdef SCALEOPT
#include <emmintrin.h>
#endif

// webrtc includes
#include "conversion_tables.h"

namespace webrtc
{

//Verify and allocate buffer
static WebRtc_Word32 VerifyAndAllocate(WebRtc_UWord8*& buffer,
                                       WebRtc_UWord32 currentSize,
                                       WebRtc_UWord32 newSize);
// clip value to [0,255]
inline WebRtc_UWord8 Clip(WebRtc_Word32 val);

#ifdef SCALEOPT
void *memcpy_16(void * dest, const void * src, size_t n);
void *memcpy_8(void * dest, const void * src, size_t n);
#endif


WebRtc_UWord32
CalcBufferSize(VideoType type, WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    WebRtc_UWord32 bitsPerPixel = 32;
    switch(type)
    {
        case kI420:
            bitsPerPixel = 12;
            break;
        case kNV12:
            bitsPerPixel = 12;
            break;
        case kNV21:
            bitsPerPixel = 12;
            break;
        case kIYUV:
            bitsPerPixel = 12;
            break;
        case kYV12:
            bitsPerPixel = 12;
            break;
        case kRGB24:
            bitsPerPixel = 24;
            break;
        case kARGB:
            bitsPerPixel = 32;
            break;
        case kARGB4444:
            bitsPerPixel = 16;
            break;
        case kRGB565:
            bitsPerPixel = 16;
            break;
        case kARGB1555:
            bitsPerPixel = 16;
            break;
        case kYUY2:
            bitsPerPixel = 16;
            break;
        case kUYVY:
            bitsPerPixel = 16;
            break;
        default:
            assert(false);
            break;
    }
    return (width * height * bitsPerPixel) >> 3; // bytes
}

WebRtc_UWord32
CalcBufferSize(VideoType incomingVideoType, VideoType convertedVideoType,
               WebRtc_UWord32 length)
{
    WebRtc_UWord32 incomingBitsPerPixel = 32;
    switch(incomingVideoType)
    {
        case kI420:
            incomingBitsPerPixel = 12;
            break;
        case kNV12:
            incomingBitsPerPixel = 12;
            break;
        case kNV21:
            incomingBitsPerPixel = 12;
            break;
        case kIYUV:
            incomingBitsPerPixel = 12;
            break;
        case kYV12:
            incomingBitsPerPixel = 12;
            break;
        case kRGB24:
            incomingBitsPerPixel = 24;
            break;
        case kARGB:
            incomingBitsPerPixel = 32;
            break;
        case kARGB4444:
            incomingBitsPerPixel = 16;
            break;
        case kRGB565:
            incomingBitsPerPixel = 16;
            break;
        case kARGB1555:
            incomingBitsPerPixel = 16;
            break;
        case kYUY2:
            incomingBitsPerPixel = 16;
            break;
        case kUYVY:
            incomingBitsPerPixel = 16;
            break;
        default:
            assert(false);
            break;
    }

    WebRtc_Word32 convertedBitsPerPixel = 32;
    switch(convertedVideoType)
    {
        case kI420:
            convertedBitsPerPixel = 12;
            break;
        case kIYUV:
            convertedBitsPerPixel = 12;
            break;
        case kYV12:
            convertedBitsPerPixel = 12;
            break;
        case kRGB24:
            convertedBitsPerPixel = 24;
            break;
        case kARGB:
            convertedBitsPerPixel = 32;
            break;
        case kARGB4444:
            convertedBitsPerPixel = 16;
            break;
        case kRGB565:
            convertedBitsPerPixel = 16;
            break;
        case kARGB1555:
            convertedBitsPerPixel = 16;
            break;
        case kYUY2:
            convertedBitsPerPixel = 16;
            break;
        case kUYVY:
            convertedBitsPerPixel = 16;
            break;
        default:
            assert(false);
            break;
    }
    return (length * convertedBitsPerPixel) / incomingBitsPerPixel;
}

WebRtc_Word32
ConvertI420ToRGB24(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                   WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }

    // RGB orientation - bottom up
    WebRtc_UWord8* out = outFrame + width * height * 3 - width * 3;
    WebRtc_UWord8* out2 = out - width * 3;
    WebRtc_UWord32 h, w;
    WebRtc_Word32 tmpR, tmpG, tmpB;
    const WebRtc_UWord8 *y1, *y2 ,*u, *v;
    y1 = inFrame;
    y2 = y1 + width;
    u =  y1 + width * height;
    v =  u + ((width * height) >> 2);
    for (h = (height >> 1); h > 0; h--)
    {  // 2 rows at a time, 2 y's at a time
        for (w = 0; w < (width >> 1); w++)
        {// vertical and horizontal sub-sampling
            tmpR = (WebRtc_Word32)((mapYc[y1[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[0]] + mapUcb[u[0]] + 128) >> 8);
            out[2] = Clip(tmpR);
            out[1] = Clip(tmpG);
            out[0] = Clip(tmpB);

            tmpR = (WebRtc_Word32)((mapYc[y2[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[0]] + mapUcb[u[0]] + 128) >> 8);
            out2[2] = Clip(tmpR);
            out2[1] = Clip(tmpG);
            out2[0] = Clip(tmpB);

            tmpR = (WebRtc_Word32)((mapYc[y1[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[1]] + mapUcb[u[0]] + 128) >> 8);
            out[5] = Clip(tmpR);
            out[4] = Clip(tmpG);
            out[3] = Clip(tmpB);

            tmpR = (WebRtc_Word32)((mapYc[y2[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[1]] + mapUcb[u[0]] + 128) >> 8);
            out2[5] = Clip(tmpR);
            out2[4] = Clip(tmpG);
            out2[3] = Clip(tmpB);

            out  += 6;
            out2 += 6;
            y1 += 2;
            y2 += 2;
            u++;
            v++;
        }
        y1 += width;
        y2 += width;
        out -= width * 9;
        out2 -= width * 9;
    } // end height for

    return width * height * 3;
}

WebRtc_Word32
ConvertI420ToARGB(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                  WebRtc_UWord32 width, WebRtc_UWord32 height,
                  WebRtc_UWord32 strideOut)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    if (strideOut == 0)
    {
        strideOut = width;
    }
    else if (strideOut < width)
    {
        return -1;
    }
    WebRtc_Word32 diff = strideOut - width;
    WebRtc_UWord8* out1 = outFrame;
    WebRtc_UWord8* out2 = out1 + strideOut * 4;
    const WebRtc_UWord8 *y1,*y2, *u, *v;
    y1 = inFrame;
    y2 = y1 + width;
    u = y1 + width * height;
    v = u + (( width * height ) >> 2 );
    WebRtc_UWord32 h, w;
    WebRtc_Word32 tmpR, tmpG, tmpB;

    for (h = (height >> 1); h > 0; h--)
    {
        //do 2 rows at the time
        for (w = 0; w < (width >> 1); w++)
        {   // vertical and horizontal sub-sampling

            tmpR = (WebRtc_UWord32)((mapYc[y1[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_UWord32)((mapYc[y1[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                     +128) >> 8);
            tmpB = (WebRtc_UWord32)((mapYc[y1[0]] + mapUcb[u[0]] + 128) >> 8);
            out1[3] = 0xff;
            out1[2] = Clip(tmpR);
            out1[1] = Clip(tmpG);
            out1[0] = Clip(tmpB);

            tmpR = (WebRtc_UWord32)((mapYc[y2[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_UWord32)((mapYc[y2[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                     + 128) >> 8);
            tmpB = (WebRtc_UWord32)((mapYc[y2[0]] + mapUcb[u[0]] + 128) >> 8);
            out2[3] = 0xff;
            out2[2] = Clip(tmpR);
            out2[1] = Clip(tmpG);
            out2[0] = Clip(tmpB);

            tmpR = (WebRtc_UWord32)((mapYc[y1[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_UWord32)((mapYc[y1[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                     + 128) >> 8);
            tmpB = (WebRtc_UWord32)((mapYc[y1[1]] + mapUcb[u[0]] + 128) >> 8);
            out1[7] = 0xff;
            out1[6] = Clip(tmpR);
            out1[5] = Clip(tmpG);
            out1[4] = Clip(tmpB);

            tmpR = (WebRtc_UWord32)((mapYc[y2[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_UWord32)((mapYc[y2[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                     + 128) >> 8);
            tmpB = (WebRtc_UWord32)((mapYc[y2[1]] + mapUcb[u[0]] + 128) >> 8);
            out2[7] = 0xff;
            out2[6] = Clip(tmpR);
            out2[5] = Clip(tmpG);
            out2[4] = Clip(tmpB);

            out1 += 8;
            out2 += 8;
            y1 += 2;
            y2 += 2;
            u++;
            v++;
        }
        y1 += width;
        y2 += width;
        out1 += (strideOut + diff) * 4;
        out2 += (strideOut + diff) * 4;

    } // end height for
    return strideOut * height * 4;
}

WebRtc_Word32
ConvertI420ToRGBAMac(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                     WebRtc_UWord32 width, WebRtc_UWord32 height,
                     WebRtc_UWord32 strideOut)
{
    if (height < 1 || width < 1)
    {
        return -1;
    }

    if (strideOut == 0)
    {
        strideOut = width;
    } else if (strideOut  < width)
    {
        return -1;
    }
    WebRtc_Word32 diff = strideOut - width;

    WebRtc_UWord8 * out = outFrame;
    WebRtc_UWord8 * out2 = out + strideOut * 4;
    const WebRtc_UWord8 *y1,*y2, *u, *v;
    WebRtc_Word32 tmpG, tmpB, tmpR;
    WebRtc_UWord32 h, w;
    y1 = inFrame;
    y2 = y1 + width;
    v = y1 + width * height;
    u = v + ((width * height) >> 2);

    for (h = (height >> 1); h > 0; h--)
    {
        //do 2 rows at the time
        for (w = 0; w < (width >> 1); w++)
        {
            tmpR = (WebRtc_Word32)((mapYc[y1[0]] + mapVcr[v[0]] + 128  ) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[0]] + mapUcb[u[0]] + 128 ) >> 8);
            out[1] = Clip(tmpR);
            out[2] = Clip(tmpG);
            out[3] = Clip(tmpB);

            tmpR = (WebRtc_Word32)((mapYc[y2[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[0]] + mapUcb[u[0]] + 128) >> 8);
            out2[1] = Clip(tmpR);
            out2[2] = Clip(tmpG);
            out2[3] = Clip(tmpB);

            tmpR = (WebRtc_Word32)((mapYc[y1[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[1]] + mapUcb[u[0]] + 128) >> 8);
            out[5] = Clip(tmpR);
            out[6] = Clip(tmpG);
            out[7] = Clip(tmpB);

            tmpR = (WebRtc_Word32)((mapYc[y2[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[1]] + mapUcb[u[0]] + 128) >> 8);
            out2[5] = Clip(tmpR);
            out2[6] = Clip(tmpG);
            out2[7] = Clip(tmpB);

            out[0] = 0xff;
            out[4] = 0xff;
            out += 8;
            out2[0] = 0xff;
            out2[4] = 0xff;
            out2 += 8;
            y1 += 2;
            y2 += 2;
            u++;
            v++;
        }

        y1 += width;
        y2 += width;
        out += (width + diff * 2) * 4;
        out2 += (width + diff * 2) * 4;
    }
    return strideOut * height * 4;
}

// Little Endian...
WebRtc_Word32
ConvertI420ToARGB4444(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                      WebRtc_UWord32 width, WebRtc_UWord32 height,
                      WebRtc_UWord32 strideOut)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    if (strideOut == 0)
    {
        strideOut = width;
    }
    else if (strideOut < width)
    {
        return -1;
    }
    // RGB orientation - bottom up
    WebRtc_UWord8* out = outFrame + strideOut * (height - 1) * 2;
    WebRtc_UWord8* out2 = out - 2 * strideOut;
    WebRtc_Word32 tmpR, tmpG, tmpB;
    const WebRtc_UWord8 *y1,*y2, *u, *v;
    y1 = inFrame;
    y2 = y1 + width;
    u = y1 + width * height;
    v = u + ((width * height) >> 2);
    WebRtc_UWord32 h, w;

    for (h = (height >> 1); h > 0; h--)
    {  // 2 rows at a time, 2 y's at a time
        for (w = 0; w < (width >> 1); w++)
        {   // vertical and horizontal sub-sampling
            // Convert to RGB888 and re-scale to 4 bits
            tmpR = (WebRtc_Word32)((mapYc[y1[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[0]] + mapUcb[u[0]] + 128) >> 8);
            out[0] =(WebRtc_UWord8)((Clip(tmpG) & 0xf0) + (Clip(tmpB) >> 4));
            out[1] = (WebRtc_UWord8)(0xf0 + (Clip(tmpR) >> 4));

            tmpR = (WebRtc_Word32)((mapYc[y2[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[0]] + mapUcb[u[0]] + 128) >> 8);
            out2[0] = (WebRtc_UWord8)((Clip(tmpG) & 0xf0 ) + (Clip(tmpB) >> 4));
            out2[1] = (WebRtc_UWord8) (0xf0 + (Clip(tmpR) >> 4));

            tmpR = (WebRtc_Word32)((mapYc[y1[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[1]] + mapUcb[u[0]] + 128) >> 8);
            out[2] = (WebRtc_UWord8)((Clip(tmpG) & 0xf0 ) + (Clip(tmpB) >> 4));
            out[3] = (WebRtc_UWord8)(0xf0 + (Clip(tmpR) >> 4));

            tmpR = (WebRtc_Word32)((mapYc[y2[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[1]] + mapUcb[u[0]] + 128) >> 8);
            out2[2] = (WebRtc_UWord8)((Clip(tmpG) & 0xf0 ) + (Clip(tmpB) >> 4));
            out2[3] = (WebRtc_UWord8)(0xf0 + (Clip(tmpR) >> 4));

            out  += 4;
            out2 += 4;
            y1 += 2;
            y2 += 2;
            u++;
            v++;
        }
        y1 += width;
        y2 += width;
        out -= (2 * strideOut + width) * 2;
        out2 -= (2 * strideOut + width) * 2;
     } // end height for

    return strideOut * height * 2;
}

WebRtc_Word32
ConvertI420ToRGB565(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                    WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }

    WebRtc_UWord16* out = (WebRtc_UWord16*)(outFrame) + width * (height - 1);
    WebRtc_UWord16* out2 = out - width ;
    WebRtc_Word32 tmpR, tmpG, tmpB;
    const WebRtc_UWord8 *y1,*y2, *u, *v;
    y1 = inFrame;
    y2 = y1 + width;
    u = y1 + width * height;
    v = u + (width * height >> 2);
    WebRtc_UWord32 h, w;

    for (h = (height >> 1); h > 0; h--)
    {  // 2 rows at a time, 2 y's at a time
        for (w = 0; w < (width >> 1); w++)
        {   // vertical and horizontal sub-sampling
            // 1. Convert to RGB888
            // 2. Shift to adequate location (in the 16 bit word) - RGB 565

            tmpR = (WebRtc_Word32)((mapYc[y1[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[0]] + mapUcb[u[0]] + 128) >> 8);
            out[0]  = (WebRtc_UWord16)((Clip(tmpR) & 0xf8) << 8) + ((Clip(tmpG)
                                        & 0xfc) << 3) + (Clip(tmpB) >> 3);

            tmpR = (WebRtc_Word32)((mapYc[y2[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[0]] + mapUcb[u[0]] + 128) >> 8);
            out2[0] = (WebRtc_UWord16)((Clip(tmpR) & 0xf8) << 8) + ((Clip(tmpG)
                                        & 0xfc) << 3) + (Clip(tmpB) >> 3);

            tmpR = (WebRtc_Word32)((mapYc[y1[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[1]] + mapUcb[u[0]] + 128) >> 8);
            out[1] = (WebRtc_UWord16)((Clip(tmpR) & 0xf8) << 8) + ((Clip(tmpG)
                                       & 0xfc) << 3) + (Clip(tmpB ) >> 3);

            tmpR = (WebRtc_Word32)((mapYc[y2[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[1]] + mapUcb[u[0]] + 128) >> 8);
            out2[1] = (WebRtc_UWord16)((Clip(tmpR) & 0xf8) << 8) + ((Clip(tmpG)
                                        & 0xfc) << 3) + (Clip(tmpB) >> 3);

            y1 += 2;
            y2 += 2;
            out += 2;
            out2 += 2;
            u++;
            v++;
        }
        y1 += width;
        y2 += width;
        out -= 3 * width;
        out2 -=  3 * width;
    } // end height for

    return width * height * 2;
}


//Same as ConvertI420ToRGB565 but doesn't flip vertically.
WebRtc_Word32
ConvertI420ToRGB565Android(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                           WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }

    WebRtc_UWord16* out = (WebRtc_UWord16*)(outFrame);
    WebRtc_UWord16* out2 = out +  (width) ;
    WebRtc_Word32 tmpR, tmpG, tmpB;
    const WebRtc_UWord8 *y1,*y2, *u, *v;
    WebRtc_UWord32 h, w;
    y1 = inFrame;
    y2 = y1 + width;
    u = y1 + width * height;
    v = u + (width * height >> 2);

    for (h = (height >>1); h > 0; h--)
    {
      // 2 rows at a time, 2 y's at a time
        for (w = 0; w < (width >> 1); w++)
        {
            // vertical and horizontal sub-sampling
            // 1. Convert to RGB888
            // 2. Shift to adequate location (in the 16 bit word) - RGB 565

            tmpR = (WebRtc_Word32)((mapYc[y1[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[0]] + mapUcb[u[0]] + 128) >> 8);
            out[0]  = (WebRtc_UWord16)((Clip(tmpR) & 0xf8) << 8) + ((Clip(tmpG)
                                        & 0xfc) << 3) + (Clip(tmpB) >> 3);

            tmpR = (WebRtc_Word32)((mapYc[y2[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[0]] + mapUcb[u[0]] + 128) >> 8);
            out2[0] = (WebRtc_UWord16)((Clip(tmpR) & 0xf8) << 8) + ((Clip(tmpG)
                                        & 0xfc) << 3) + (Clip(tmpB) >> 3);

            tmpR = (WebRtc_Word32)((mapYc[y1[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[1]] + mapUcb[u[0]] + 128) >> 8);
            out[1] = (WebRtc_UWord16)((Clip(tmpR) & 0xf8) << 8) + ((Clip(tmpG)
                                        & 0xfc) << 3) + (Clip(tmpB ) >> 3);

            tmpR = (WebRtc_Word32)((mapYc[y2[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[1]] + mapUcb[u[0]] + 128) >> 8);
            out2[1] = (WebRtc_UWord16)((Clip(tmpR) & 0xf8) << 8) + ((Clip(tmpG)
                                        & 0xfc) << 3) + (Clip(tmpB) >> 3);

            y1 += 2;
            y2 += 2;
            out += 2;
            out2 += 2;
            u++;
            v++;
        }
        y1 += width;
        y2 += width;
        out += width;
        out2 +=  width;
    } // end height for

    return width * height * 2;
}

WebRtc_Word32
ConvertI420ToARGB1555(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                      WebRtc_UWord32 width, WebRtc_UWord32 height,
                      WebRtc_UWord32 strideOut)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    if (strideOut == 0)
    {
        strideOut = width;
    }
    else if (strideOut < width)
    {
        return -1;
    }

    WebRtc_UWord16* out = (WebRtc_UWord16*)(outFrame) + width * (height - 1);
    WebRtc_UWord16* out2 = out - width ;
    WebRtc_Word32 tmpR, tmpG, tmpB;
    const WebRtc_UWord8 *y1,*y2, *u, *v;
    WebRtc_UWord32 h, w;

    y1 = inFrame;
    y2 = y1 + width;
    u = y1 + width * height;
    v = u + (width * height >> 2);

    for (h = (height >> 1); h > 0; h--)
    {  // 2 rows at a time, 2 y's at a time
        for (w = 0; w < (width >> 1); w++)
        {
            // vertical and horizontal sub-sampling
            // 1. Convert to RGB888
            // 2. shift to adequate location (in the 16 bit word) - RGB 555
            // 3. Add 1 for alpha value
            tmpR = (WebRtc_Word32)((mapYc[y1[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[0]] + mapUcb[u[0]] + 128) >> 8);
            out[0]  = (WebRtc_UWord16)(0x8000 + ((Clip(tmpR) & 0xf8) << 10) +
                      ((Clip(tmpG) & 0xf8) << 3) + (Clip(tmpB) >> 3));

            tmpR = (WebRtc_Word32)((mapYc[y2[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[0]] + mapUcb[u[0]] + 128) >> 8);
            out2[0]  = (WebRtc_UWord16)(0x8000 + ((Clip(tmpR) & 0xf8) << 10) +
                       ((Clip(tmpG) & 0xf8) << 3) + (Clip(tmpB) >> 3));

            tmpR = (WebRtc_Word32)((mapYc[y1[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[1]] + mapUcb[u[0]] + 128) >> 8);
            out[1]  = (WebRtc_UWord16)(0x8000 + ((Clip(tmpR) & 0xf8) << 10) +
                      ((Clip(tmpG) & 0xf8) << 3)  + (Clip(tmpB) >> 3));

            tmpR = (WebRtc_Word32)((mapYc[y2[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[1]] + mapUcb[u[0]] + 128) >> 8);
            out2[1]  = (WebRtc_UWord16)(0x8000 + ((Clip(tmpR) & 0xf8) << 10) +
                       ((Clip(tmpG) & 0xf8) << 3)  + (Clip(tmpB) >> 3));

            y1 += 2;
            y2 += 2;
            out += 2;
            out2 += 2;
            u++;
            v++;
        }
        y1 += width;
        y2 += width;
        out -= 3 * width;
        out2 -=  3 * width;
    } // end height for
    return strideOut * height * 2;
}

WebRtc_Word32
ConvertI420ToYUY2(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                  WebRtc_UWord32 width, WebRtc_UWord32 height,
                  WebRtc_UWord32 strideOut)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    if(strideOut == 0)
    {
      strideOut = width;
    }
    else if (strideOut < width)
    {
        return -1;
     }

    const WebRtc_UWord8* in1 = inFrame;
    const WebRtc_UWord8* in2 = inFrame + width ;
    const WebRtc_UWord8* inU = inFrame + width * height;
    const WebRtc_UWord8* inV = inU + width * (height >> 2);

    WebRtc_UWord8* out1 = outFrame;
    WebRtc_UWord8* out2 = outFrame + 2 * strideOut;

    //YUY2 - Macro-pixel = 2 image pixels
    //Y0U0Y1V0....Y2U2Y3V2...Y4U4Y5V4....

#ifndef SCALEOPT
    for (WebRtc_UWord32 i = 0; i < (height >> 1);i++)
    {
        for (WebRtc_UWord32 j = 0; j < (width >> 1);j++)
        {
            out1[0] = in1[0];
            out1[1] = *inU;
            out1[2] = in1[1];
            out1[3] = *inV;

            out2[0] = in2[0];
            out2[1] = *inU;
            out2[2] = in2[1];
            out2[3] = *inV;
            out1 += 4;
            out2 += 4;
            inU++;
            inV++;
            in1 += 2;
            in2 += 2;
        }
        in1 += width;
        in2 += width;
        out1 += 2 * strideOut + 2 * (strideOut - width);
        out2 += 2 * strideOut + 2 * (strideOut - width);
    }
#else
    for (WebRtc_UWord32 i = 0; i < (height >> 1);i++)
    {
        WebRtc_Word32 width__ = (width >> 4);
        _asm
        {
            ;pusha
            mov       eax, DWORD PTR [in1]                       ;1939.33
            mov       ecx, DWORD PTR [in2]                       ;1939.33
            mov       ebx, DWORD PTR [inU]                       ;1939.33
            mov       edx, DWORD PTR [inV]                       ;1939.33
            loop0:
            movq      xmm6, QWORD PTR [ebx]          ;inU
            movq      xmm0, QWORD PTR [edx]          ;inV
            punpcklbw xmm6, xmm0                     ;inU, inV mix
            ;movdqa    xmm1, xmm6
            ;movdqa    xmm2, xmm6
            ;movdqa    xmm4, xmm6

            movdqu    xmm3, XMMWORD PTR [eax]        ;in1
            movdqa    xmm1, xmm3
            punpcklbw xmm1, xmm6                     ;in1, inU, in1, inV
            mov       esi, DWORD PTR [out1]
            movdqu    XMMWORD PTR [esi], xmm1        ;write to out1

            movdqu    xmm5, XMMWORD PTR [ecx]        ;in2
            movdqa    xmm2, xmm5
            punpcklbw xmm2, xmm6                     ;in2, inU, in2, inV
            mov       edi, DWORD PTR [out2]
            movdqu    XMMWORD PTR [edi], xmm2        ;write to out2

            punpckhbw xmm3, xmm6                     ;in1, inU, in1, inV again
            movdqu    XMMWORD PTR [esi+16], xmm3     ;write to out1 again
            add       esi, 32
            mov       DWORD PTR [out1], esi

            punpckhbw xmm5, xmm6                     ;inU, in2, inV again
            movdqu    XMMWORD PTR [edi+16], xmm5     ;write to out2 again
            add       edi, 32
            mov       DWORD PTR [out2], edi

            add       ebx, 8
            add       edx, 8
            add       eax, 16
            add       ecx, 16

            mov       esi, DWORD PTR [width__]
            sub       esi, 1
            mov       DWORD PTR [width__], esi
            jg        loop0

            mov       DWORD PTR [in1], eax                       ;1939.33
            mov       DWORD PTR [in2], ecx                       ;1939.33
            mov       DWORD PTR [inU], ebx                       ;1939.33
            mov       DWORD PTR [inV], edx                       ;1939.33

            ;popa
            emms
        }
        in1 += width;
        in2 += width;
        out1 += 2 * strideOut + 2 * (strideOut - width);
        out2 += 2 * strideOut + 2 * (strideOut - width);
    }
#endif
    return strideOut * height * 2;
}

WebRtc_Word32
ConvertI420ToUYVY(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                  WebRtc_UWord32 width,WebRtc_UWord32 height,
                  WebRtc_UWord32 strideOut)
{
    if (width < 1 || height < 1)
    {
         return -1;
    }
    if(strideOut == 0)
    {
        strideOut = width;
    }
    else if (strideOut < width)
    {
        return -1;
    }
    WebRtc_UWord32 i = 0;
    const WebRtc_UWord8* in1 = inFrame;
    const WebRtc_UWord8* in2 = inFrame + width ;
    const WebRtc_UWord8* inU = inFrame + width * height;
    const WebRtc_UWord8* inV = inFrame + width * height + width * (height >> 2);

    WebRtc_UWord8* out1 = outFrame;
    WebRtc_UWord8* out2 = outFrame + 2 * strideOut;

    //Macro-pixel = 2 image pixels
    //U0Y0V0Y1....U2Y2V2Y3...U4Y4V4Y5.....

#ifndef SCALEOPT
    for (; i < (height >> 1);i++)
    {
        for (WebRtc_UWord32 j = 0; j < (width >> 1) ;j++)
        {
            out1[0] = *inU;
            out1[1] = in1[0];
            out1[2] = *inV;
            out1[3] = in1[1];

            out2[0] = *inU;
            out2[1] = in2[0];
            out2[2] = *inV;
            out2[3] = in2[1];
            out1 += 4;
            out2 += 4;
            inU++;
            inV++;
            in1 += 2;
            in2 += 2;
        }
        in1 += width;
        in2 += width;
        out1 += 2 * (strideOut + (strideOut - width));
        out2 += 2 * (strideOut + (strideOut - width));
    }
#else
    for (; i< (height >> 1);i++)
    {
        WebRtc_Word32 width__ = (width >> 4);
        _asm
        {
            ;pusha
            mov       eax, DWORD PTR [in1]                       ;1939.33
            mov       ecx, DWORD PTR [in2]                       ;1939.33
            mov       ebx, DWORD PTR [inU]                       ;1939.33
            mov       edx, DWORD PTR [inV]                       ;1939.33
loop0:
            movq      xmm6, QWORD PTR [ebx]          ;inU
            movq      xmm0, QWORD PTR [edx]          ;inV
            punpcklbw xmm6, xmm0                     ;inU, inV mix
            movdqa    xmm1, xmm6
            movdqa    xmm2, xmm6
            movdqa    xmm4, xmm6

            movdqu    xmm3, XMMWORD PTR [eax]        ;in1
            punpcklbw xmm1, xmm3                     ;inU, in1, inV
            mov       esi, DWORD PTR [out1]
            movdqu    XMMWORD PTR [esi], xmm1        ;write to out1

            movdqu    xmm5, XMMWORD PTR [ecx]        ;in2
            punpcklbw xmm2, xmm5                     ;inU, in2, inV
            mov       edi, DWORD PTR [out2]
            movdqu    XMMWORD PTR [edi], xmm2        ;write to out2

            punpckhbw xmm4, xmm3                     ;inU, in1, inV again
            movdqu    XMMWORD PTR [esi+16], xmm4     ;write to out1 again
            add       esi, 32
            mov       DWORD PTR [out1], esi

            punpckhbw xmm6, xmm5                     ;inU, in2, inV again
            movdqu    XMMWORD PTR [edi+16], xmm6     ;write to out2 again
            add       edi, 32
            mov       DWORD PTR [out2], edi

            add       ebx, 8
            add       edx, 8
            add       eax, 16
            add       ecx, 16

            mov       esi, DWORD PTR [width__]
            sub       esi, 1
            mov       DWORD PTR [width__], esi
            jg        loop0

            mov       DWORD PTR [in1], eax                       ;1939.33
            mov       DWORD PTR [in2], ecx                       ;1939.33
            mov       DWORD PTR [inU], ebx                       ;1939.33
            mov       DWORD PTR [inV], edx                       ;1939.33

            ;popa
            emms
        }
        in1 += width;
        in2 += width;
        out1 += 2 * (strideOut + (strideOut - width));
        out2 += 2 * (strideOut + (strideOut - width));
    }
#endif
    return strideOut * height * 2;
}

WebRtc_Word32
ConvertI420ToYV12(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                  WebRtc_UWord32 width, WebRtc_UWord32 height,
                  WebRtc_UWord32 strideOut)
{
    if (height < 1 || width < 1 )
    {
        return -1;
    }
    if (strideOut == 0)
    {
        strideOut = width;
    }
    else if (strideOut < width)
    {
        return -1;
    }

    // copy Y
    for (WebRtc_UWord32 i = 0; i < height; i++)
    {
#ifndef SCALEOPT
        memcpy(outFrame, inFrame, width);
#else
        memcpy_16(outFrame, inFrame, width);
#endif
        inFrame += width;
        outFrame += strideOut;
    }
    // copy U
    outFrame += (strideOut >> 1) * height >> 1;
    for (WebRtc_UWord32 i = 0; i < height >>1; i++)
    {
#ifndef SCALEOPT
        memcpy(outFrame, inFrame, width >> 1);
#else
        memcpy_8(outFrame, inFrame, width >> 1);
#endif
        inFrame += width >> 1;
        outFrame += strideOut >> 1;
    }
    outFrame -= strideOut * height >> 1;
    // copy V
    for (WebRtc_UWord32 i = 0; i < height >> 1; i++)
    {
#ifndef SCALEOPT
        memcpy(outFrame, inFrame, width >> 1);
#else
        memcpy_8(outFrame, inFrame, width >> 1);
#endif
        inFrame += width >> 1;
        outFrame += strideOut >> 1;
    }
    return ((3 * strideOut * height) >> 1);
}

WebRtc_Word32
ConvertYV12ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord32 width,
                  WebRtc_UWord32 height, WebRtc_UWord8* outFrame)
{
    if (height < 1 || width <1)
    {
        return -1;
    }
    WebRtc_UWord8 *u, *v, *uo, *vo;
    WebRtc_Word32 lumlen = 0;
    WebRtc_Word32 crlen = 0;

    lumlen = height * width;
    crlen = (lumlen >> 2);
    v = (WebRtc_UWord8 *)inFrame + lumlen;
    uo = outFrame + lumlen;
    u = v + crlen;
    vo = uo + crlen;

    memcpy(outFrame, inFrame, lumlen); // copy luminance
    memcpy(vo, v, crlen);   // copy V to V out
    memcpy(uo, u, crlen);   // copy U to U out

    return (width * height * 3) >> 1;
}

WebRtc_Word32
ConvertNV12ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                  WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }

    // Bi-Planar: Y plane followed by an interlaced U and V plane
    WebRtc_UWord8* out = outFrame;
    // copying Y plane as is
    memcpy(out, inFrame, width * height);
    // de-interlacing U and V
    const WebRtc_UWord8 *interlacedSrc;
    WebRtc_UWord8 *u, *v;
    u = outFrame + width * height;
    v = u + (width * height >> 2);
    interlacedSrc = inFrame + width * height;
    for (WebRtc_UWord32 ind = 0; ind < (width * height >> 2); ind ++)
    {
        u[ind] = interlacedSrc[2 * ind];
        v[ind] = interlacedSrc[2 * ind + 1];
    }
    return (width * height * 3 >> 1);
}
WebRtc_Word32
ConvertNV12ToI420AndRotate180(const WebRtc_UWord8* inFrame,
                              WebRtc_UWord8* outFrame,
                              WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }

    // Bi-Planar: Y plane followed by an interlaced U and V plane
    WebRtc_UWord8* out = outFrame;

    for(WebRtc_UWord32 index = 0; index < width * height; index++)
    {
        out[index] = inFrame[width * height - index - 1];
    }
    // de-interlacing U and V
    const WebRtc_UWord8 *interlacedSrc;
    WebRtc_UWord8 *u, *v;
    u = outFrame + width * height;
    v = u + (width * height >> 2);
    interlacedSrc = inFrame + width * height;
    // extracting and rotating 180
    for (WebRtc_UWord32 index = 0; index < (width * height >> 2); index++)
    {
        u[(width * height >> 2) - index - 1] = interlacedSrc[2 * index];
        v[(width * height >> 2) - index - 1] = interlacedSrc[2 * index + 1];
    }
    return (width * height * 3 >> 1);
}

WebRtc_Word32
ConvertNV12ToI420AndRotateClockwise(const WebRtc_UWord8* inFrame,
                                    WebRtc_UWord8* outFrame,
                                    WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }

    WebRtc_UWord8* targetBuffer = outFrame;
    const WebRtc_UWord8* sourcePtr = inFrame;
    const WebRtc_UWord8* interlacedSrc = inFrame + width * height;

    WebRtc_UWord32 index = 0;

    // Rotate Y
    for(WebRtc_UWord32 newRow = 0; newRow < width; ++newRow)
    {
        for(WebRtc_Word32 newColumn = height-1; newColumn >= 0; --newColumn)
        {
            targetBuffer[index++] = sourcePtr[newColumn * width + newRow];
        }
    }

    // extracting and rotating U and V
    WebRtc_UWord8* u  = targetBuffer + width * height;
    WebRtc_UWord8* v  = u + (width * height >> 2);
    for (WebRtc_UWord32 colInd = 0; colInd < height >> 1; colInd ++)
    {
        for (WebRtc_UWord32 rowInd = 0; rowInd < width >> 1; rowInd ++)
        {
            u[rowInd * height / 2 + colInd] =
            interlacedSrc[(height / 2 - colInd - 1) * width  + 2 * rowInd];
            v[rowInd * height / 2 + colInd] =
            interlacedSrc[(height / 2 - colInd - 1) * width + 2 * rowInd + 1];
        }
    }

    return (width * height * 3 >> 1);
}

WebRtc_Word32
ConvertNV12ToI420AndRotateAntiClockwise(const WebRtc_UWord8* inFrame,
                                        WebRtc_UWord8* outFrame,
                                        WebRtc_UWord32 width,
                                        WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    WebRtc_UWord8* targetBuffer = outFrame;
    const WebRtc_UWord8* sourcePtr = inFrame;
    const WebRtc_UWord8* interlacedSrc = inFrame + width * height;

    WebRtc_UWord32 index = 0;
    // Rotate Y
    for(WebRtc_Word32 newRow = width - 1; newRow >= 0; --newRow)
    {
        for(WebRtc_UWord32 newColumn = 0; newColumn < height; ++newColumn)
        {
            targetBuffer[index++] = sourcePtr[newColumn * width + newRow];
        }
    }

    // extracting and rotating U and V
    WebRtc_UWord8* u  = targetBuffer + width * height;
    WebRtc_UWord8* v  = u + (width * height >> 2);
    index = 0;
    for(WebRtc_Word32 newRow = (width >> 1) - 1; newRow >= 0; --newRow)
    {
        for(WebRtc_UWord32 newColumn = 0; newColumn < (height >> 1); ++newColumn)
        {
            u[index] = interlacedSrc[2 * (newColumn * (width >> 1) + newRow)];
            v[index] = interlacedSrc[2 * (newColumn * (width >> 1) + newRow) + 1];
            index++;
        }
    }

    return (width * height * 3 >> 1);
}

WebRtc_Word32
ConvertNV12ToRGB565(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                    WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }

    // Bi-Planar: Y plane followed by an interlaced U and V plane
    const WebRtc_UWord8* interlacedSrc = inFrame + width * height;
    WebRtc_UWord16* out = (WebRtc_UWord16*)(outFrame) + width * (height - 1);
    WebRtc_UWord16* out2 = out - width;
    WebRtc_Word32 tmpR, tmpG, tmpB;
    const WebRtc_UWord8 *y1,*y2;
    y1 = inFrame;
    y2 = y1 + width;
    WebRtc_UWord32 h, w;

    for (h = (height >> 1); h > 0; h--)
    {  // 2 rows at a time, 2 y's at a time
        for (w = 0; w < (width >> 1); w++)
        {   // vertical and horizontal sub-sampling
            // 1. Convert to RGB888
            // 2. Shift to adequate location (in the 16 bit word) - RGB 565

            tmpR = (WebRtc_Word32)((mapYc[y1[0]] + mapVcr[interlacedSrc[1]]
                                    + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[0]] + mapUcg[interlacedSrc[0]]
                                    + mapVcg[interlacedSrc[1]] + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[0]] + mapUcb[interlacedSrc[0]]
                                    + 128) >> 8);
            out[0]  = (WebRtc_UWord16)((Clip(tmpR) & 0xf8) << 8) + ((Clip(tmpG)
                                        & 0xfc) << 3) + (Clip(tmpB) >> 3);

            tmpR = (WebRtc_Word32)((mapYc[y2[0]] + mapVcr[interlacedSrc[1]]
                                    + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[0]] + mapUcg[interlacedSrc[0]]
                                    + mapVcg[interlacedSrc[1]] + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[0]] + mapUcb[interlacedSrc[0]]
                                    + 128) >> 8);
            out2[0] = (WebRtc_UWord16)((Clip(tmpR) & 0xf8) << 8) + ((Clip(tmpG)
                                        & 0xfc) << 3) + (Clip(tmpB) >> 3);

            tmpR = (WebRtc_Word32)((mapYc[y1[1]] + mapVcr[interlacedSrc[1]]
                                    + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[1]] + mapUcg[interlacedSrc[0]]
                                    + mapVcg[interlacedSrc[1]] + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[1]] + mapUcb[interlacedSrc[0]]
                                    + 128) >> 8);
            out[1] = (WebRtc_UWord16)((Clip(tmpR) & 0xf8) << 8) + ((Clip(tmpG)
                                       & 0xfc) << 3) + (Clip(tmpB ) >> 3);

            tmpR = (WebRtc_Word32)((mapYc[y2[1]] + mapVcr[interlacedSrc[1]]
                                    + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[1]] + mapUcg[interlacedSrc[0]]
                                    + mapVcg[interlacedSrc[1]] + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[1]] + mapUcb[interlacedSrc[0]]
                                    + 128) >> 8);
            out2[1] = (WebRtc_UWord16)((Clip(tmpR) & 0xf8) << 8) + ((Clip(tmpG)
                                        & 0xfc) << 3) + (Clip(tmpB) >> 3);

            y1 += 2;
            y2 += 2;
            out += 2;
            out2 += 2;
            interlacedSrc += 2;
            }
        y1 += width;
        y2 += width;
        out -= 3 * width;
        out2 -= 3 * width;
    } // end height for

    return (width * height * 2);
}

//NV21 Android Functions
WebRtc_Word32
ConvertNV21ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                  WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }

    // Bi-Planar: Y plane followed by an interlaced U and V plane
    WebRtc_UWord8* out = outFrame;
    // copying Y plane as is
    memcpy(out, inFrame, width * height);
    // de-interlacing U and V
    const WebRtc_UWord8 *interlacedSrc;
    WebRtc_UWord8 *u, *v;
    u = outFrame + width * height;
    v = u + (width * height >> 2);
    interlacedSrc = inFrame + width * height;
    for (WebRtc_UWord32 ind = 0; ind < (width * height >> 2); ind ++)
    {
        v[ind] = interlacedSrc[2 * ind];
        u[ind] = interlacedSrc[2 * ind + 1];
    }
    return (width * height * 3 >> 1);
}
WebRtc_Word32
ConvertNV21ToI420AndRotate180(const WebRtc_UWord8* inFrame,
                              WebRtc_UWord8* outFrame,
                              WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }

    // Bi-Planar: Y plane followed by an interlaced U and V plane
    WebRtc_UWord8* out = outFrame;
    for(WebRtc_UWord32 index = 0; index < width * height; index++)
    {
        out[index] = inFrame[width * height - index - 1];
    }
    // de-interlacing U and V
    const WebRtc_UWord8 *interlacedSrc;
    WebRtc_UWord8 *u, *v;
    u = outFrame + width * height;
    v = u + (width * height >> 2);
    interlacedSrc = inFrame + width * height;
    // extracting and rotating 180
    for (WebRtc_UWord32 index = 0; index < (width * height >> 2); index++)
    {
        v[(width * height >> 2) - index - 1] = interlacedSrc[2 * index];
        u[(width * height >> 2) - index - 1] = interlacedSrc[2 * index + 1];
    }
    return (width * height * 3 >> 1);
}

WebRtc_Word32
ConvertNV21ToI420AndRotateClockwise(const WebRtc_UWord8* inFrame,
                                    WebRtc_UWord8* outFrame,
                                    WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    // Paint the destination buffer black
    memset(outFrame,0,width * height);
    memset(outFrame + width * height,127,(width * height) / 2);
    const WebRtc_Word32 offset = (width - height) / 2;

    //Y
    WebRtc_UWord8* yn= outFrame;
    const WebRtc_UWord8* ys= inFrame;
    for (WebRtc_UWord32 m = 0; m < height; ++m)// New row
    {
        yn += offset;
        for (WebRtc_UWord32 n = 0; n < height; ++n) // new column
        {
            (*yn++) = ys[(height - 1 - n) * width + offset + m];
        }
        yn += offset;
    }

    //U & V
    WebRtc_UWord8* un= outFrame + height * width;
    WebRtc_UWord8* vn= outFrame+height * width + height * width / 4;
    const WebRtc_UWord8* uvs= inFrame + height * width;

    for (WebRtc_UWord32 m = 0;m < height / 2; ++m)// New row
    {
        un += offset / 2;
        vn += offset / 2;
        for (WebRtc_UWord32 n = 0;n < height / 2; ++n) // new column
        {
            (*un++) = uvs[(height / 2 - 1 - n) * width + offset + 2 * m + 1];
            (*vn++) = uvs[(height / 2 - 1 - n) * width + offset + 2 * m];
        }
        un += offset / 2;
        vn += offset / 2;
    }

    return (width * height * 3 >> 1);
}

WebRtc_Word32
ConvertNV21ToI420AndRotateAntiClockwise(const WebRtc_UWord8* inFrame,
                                        WebRtc_UWord8* outFrame,
                                        WebRtc_UWord32 width,
                                        WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    // Paint the destination buffer black
    memset(outFrame,0,width * height);
    memset(outFrame + width * height, 127, (width * height) / 2);

    const WebRtc_Word32 offset = (width - height) / 2;

    //Y
    WebRtc_UWord8* yn = outFrame;
    const WebRtc_UWord8* ys = inFrame;
    for (WebRtc_UWord32 m = 0;m < height; ++m)// New row
    {
        yn += offset;
        for (WebRtc_UWord32 n = 0;n < height; ++n) // new column
        {
            (*yn++) = ys[width * (n + 1) - 1 - offset - m];
        }
        yn += offset;
    }

    //U & V
    WebRtc_UWord8* un= outFrame + height * width;
    WebRtc_UWord8* vn= outFrame + height * width + height * width / 4;
    const WebRtc_UWord8* uvs= inFrame + height * width;

    for (WebRtc_UWord32 m = 0;m < height / 2; ++m)// New row
    {
        un += offset / 2;
        vn += offset / 2;
        for (WebRtc_UWord32 n = 0;n < height / 2; ++n) // new column
        {
            (*un++) = uvs[width * (n + 1) - 1 - offset - 2 * m];;
            (*vn++) = uvs[width * (n + 1) - 1 - offset - 2 * m - 1];;
        }
        un += offset / 2;
        vn += offset / 2;
    }
    return (width * height * 3 >> 1);
}

WebRtc_Word32
ConvertI420ToRGBAIPhone(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                        WebRtc_UWord32 width, WebRtc_UWord32 height,
                        WebRtc_UWord32 strideOut)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    if (strideOut == 0)
    {
        strideOut = width;
    } else if (strideOut < width)
    {
        return -1;
    }

    // RGB orientation - bottom up
    // same as ARGB but reverting RGB <-> BGR (same as previous version)
    WebRtc_UWord8* out = outFrame + strideOut * height * 4 - strideOut * 4;
    WebRtc_UWord8* out2 = out - strideOut * 4;
    WebRtc_Word32 tmpR, tmpG, tmpB;
    const WebRtc_UWord8 *y1,*y2, *u, *v;
    WebRtc_UWord32 h, w;

    y1 = inFrame;
    y2 = y1 + width;
    u = y1 + width * height;
    v = u + ((width * height) >> 2);

    for (h = (height >> 1); h > 0; h--)
    {  // 2 rows at a time, 2 y's at a time
        for (w = 0; w < (width >> 1); w++)
        {   // vertical and horizontal sub-sampling
            tmpR = (WebRtc_Word32)((298 * (y1[0] - 16) + 409 * (v[0] - 128)
                                    + 128) >> 8);
            tmpG = (WebRtc_Word32)((298 * (y1[0] - 16) - 100 * (u[0] - 128)
                                    - 208 * (v[0] - 128) + 128 ) >> 8);
            tmpB = (WebRtc_Word32)((298 * (y1[0] - 16) + 516 * (u[0] - 128)
                                    + 128 ) >> 8);

            out[3] = 0xff;
            out[0] = Clip(tmpR);
            out[1] = Clip(tmpG);
            out[2] = Clip(tmpB);

            tmpR = (WebRtc_Word32)((298 * (y2[0] - 16) + 409 * (v[0] - 128)
                                    + 128) >> 8);
            tmpG = (WebRtc_Word32)((298 * (y2[0] - 16) - 100 * (u[0] - 128)
                                    - 208 * (v[0] - 128) + 128) >> 8);
            tmpB = (WebRtc_Word32)((298 * (y2[0] - 16) + 516 * (u[0] - 128)
                                    + 128) >> 8);

            out2[3] = 0xff;
            out2[0] = Clip(tmpR);
            out2[1] = Clip(tmpG);
            out2[2] = Clip(tmpB);

            tmpR = (WebRtc_Word32)((298 * (y1[1] - 16) + 409 * (v[0] - 128)
                                    + 128 ) >> 8);
            tmpG = (WebRtc_Word32)((298 * (y1[1] - 16) - 100 * (u[0] - 128)
                                    - 208 * (v[0] - 128) + 128 ) >> 8);
            tmpB = (WebRtc_Word32)((298 * (y1[1] - 16) + 516 * (u[0] - 128)
                                    + 128) >> 8);

            out[7] = 0xff;
            out[4] = Clip(tmpR);
            out[5] = Clip(tmpG);
            out[6] = Clip(tmpB);

            tmpR = (WebRtc_Word32)((298 * (y2[1] - 16) + 409 * (v[0] - 128)
                                    + 128) >> 8);
            tmpG = (WebRtc_Word32)((298 * (y2[1] - 16) - 100 * (u[0] - 128)
                                    - 208 * (v[0] - 128) + 128) >> 8);
            tmpB = (WebRtc_Word32)((298 * (y2[1] - 16) + 516 * (u[0] - 128)
                                    + 128 ) >> 8);

            out2[7] = 0xff;
            out2[4] = Clip(tmpR);
            out2[5] = Clip(tmpG);
            out2[6] = Clip(tmpB);

            out  += 8;
            out2 += 8;
            y1 += 2;
            y2 += 2;
            u++;
            v++;
        }

        y1 += width;
        y2 += width;
        out -= (2 * strideOut + width) * 4;
        out2 -= (2 * strideOut + width) * 4;
    } // end height for

    return strideOut * height * 4;
}

WebRtc_Word32
ConvertI420ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                  WebRtc_UWord32 width,
                  WebRtc_UWord32 height, WebRtc_UWord32 strideOut)
{
    if (strideOut == 0 || strideOut == width)
    {
        memcpy(outFrame, inFrame, 3 * width * (height >> 1));
        strideOut = width;
    } else if (strideOut < width)
    {
        return -1;
    } else
    {
        WebRtc_UWord32 i = 0;
        for (; i < height; i++)
        {
            memcpy(outFrame,inFrame ,width);
            outFrame += strideOut;
            inFrame += width;
        }
        for (i = 0; i < (height >> 1);i++)
        {
            memcpy(outFrame, inFrame,width >> 1);
            outFrame += strideOut >> 1;
            inFrame += width >> 1;
        }
        for (i = 0; i< (height >> 1); i++)
        {
            memcpy(outFrame, inFrame,width >> 1);
            outFrame += strideOut >> 1;
            inFrame += width >> 1;
        }
    }
    return 3 * strideOut * (height >> 1);
}

WebRtc_Word32
ConvertUYVYToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord32 inWidth,
                  WebRtc_UWord32 inHeight, WebRtc_UWord8* outFrame,
                  WebRtc_UWord32 outWidth, WebRtc_UWord32 outHeight)
{
    if (inWidth < 1 || inHeight < 1 || outHeight < 1 || outWidth < 1)
    {
        return -1;
    }
    WebRtc_UWord32 i = 0;
    WebRtc_UWord32 j = 0;
    WebRtc_Word32 cutDiff = 0; // in pixels
    WebRtc_Word32 padDiffLow = 0; // in pixels
    WebRtc_Word32 padDiffHigh = 0; // in pixels
    WebRtc_UWord8* outI = outFrame;
    WebRtc_UWord8* outCr = outFrame + outWidth * outHeight;
    WebRtc_UWord8* outCb = outFrame + outWidth * outHeight +
                           outWidth * (outHeight >> 2);

    // cut height?
    if (inHeight > outHeight)
    {
        // parse away half of the lines
      inFrame += ((inHeight - outHeight) / 2) * inWidth * 2;
    }
    // cut width?
    if (inWidth > outWidth)
    {
        cutDiff = (inWidth - outWidth); // in pixels
        // start half of the width diff into the line
        // each pixel is 2 bytes hence diff is the correct value in bytes
        inFrame += cutDiff;
    }
    // pad height?
    if (inHeight < outHeight)
    {
        // pad top
        WebRtc_Word32 diff = (outHeight - inHeight) >> 1;
        memset(outI, 0, diff * outWidth);
        outI += diff * outWidth;
        WebRtc_Word32 colorLength = (diff >> 1) * (outWidth >> 1);
        memset(outCr, 127, colorLength);
        memset(outCb, 127, colorLength);
        outCr += colorLength;
        outCb += colorLength;

        // pad bottom
        memset(outI + outWidth * inHeight, 0, diff * outWidth);
        memset(outCr + (outWidth * inHeight >> 2), 127, colorLength);
        memset(outCb + (outWidth * inHeight >> 2), 127, colorLength);
    }
    // pad width?
    if (inWidth < outWidth)
    {
        padDiffLow = (outWidth - inWidth) >> 1; // in pixels
        padDiffHigh = (outWidth - inWidth) - padDiffLow; // in pixels
    }
    WebRtc_UWord32 height = 0;
    if (inHeight > outHeight)
        height = outHeight;
    else
        height = inHeight;

    for (; i< (height >> 1); i++) // 2 rows per loop
    {
        // pad beginning of row?
        if (padDiffLow)
        {
            // pad row
            memset(outI,0,padDiffLow);
            memset(outCr,127,padDiffLow >> 1);
            memset(outCb,127,padDiffLow >> 1);
            outI += padDiffLow;
            outCr += padDiffLow >> 1;
            outCb += padDiffLow >> 1;
        }

        for (j = 0; j < (inWidth >> 1); j++) // 2 pixels per loop
        {
            outI[0] = inFrame[1];
            *outCr = inFrame[0];
            outI[1] = inFrame[3];
            *outCb = inFrame[2];
            inFrame += 4;
            outI += 2;
            outCr++;
            outCb++;
        }
        // pad end of row?
        if (padDiffHigh)
        {
            memset(outI,0,padDiffHigh);
            memset(outCr,127,padDiffHigh >> 1);
            memset(outCb,127,padDiffHigh >> 1);
            outI += padDiffHigh;
            outCr += padDiffHigh >> 1;
            outCb += padDiffHigh >> 1;
        }
        // next row
        // pad beginning of row?
        memset(outI,0,padDiffLow);
        outI += padDiffLow;

        for (j = 0; j < (inWidth >> 1);j++)
        {
            outI[0] = inFrame[1];
            outI[1] = inFrame[3];
            inFrame += 4;
            outI += 2;
        }
        // pad end of row?
        if (padDiffHigh)
        {
            memset(outI,0,padDiffHigh);
            outI += padDiffHigh;
        } else
        {
            // cut row
            for (j = 0; j < (outWidth >> 1); j++) // 2 pixels per loop
            {
                outI[0] = inFrame[1];
                *outCr = inFrame[0];
                outI[1] = inFrame[3];
                *outCb = inFrame[2];
                inFrame += 4;
                outI += 2;
                outCr++;
                outCb++;
            }
            inFrame += cutDiff * 2;
            // next row
            for (j = 0; j < (outWidth >> 1);j++)
            {
                outI[0] = inFrame[1];
                outI[1] = inFrame[3];
                inFrame += 4;
                outI += 2;
            }
            inFrame += cutDiff * 2;
        }
    }
    return outWidth * (outHeight >> 1) * 3;
}

WebRtc_Word32
ConvertUYVYToI420interlaced(const WebRtc_UWord8* inFrame, WebRtc_UWord32 inWidth,
                            WebRtc_UWord32 inHeight, WebRtc_UWord8* outFrame,
                            WebRtc_UWord32 outWidth, WebRtc_UWord32 outHeight)
{
    if (inWidth < 1 || inHeight < 1 || outHeight < 1 || outWidth < 1)
    {
        return -1;
    }
    WebRtc_Word32 i = 0;
    WebRtc_UWord32 j = 0;
    WebRtc_Word32 cutDiff = 0; // in pixels
    WebRtc_Word32 padDiffLow = 0; // in pixels
    WebRtc_Word32 padDiffHigh = 0; // in pixels
    WebRtc_UWord8* outI = outFrame;
    WebRtc_UWord8* outCr = outFrame + outWidth * outHeight;
    WebRtc_UWord8* outCb = outFrame + outWidth * outHeight +
                           outWidth * ( outHeight >> 2 );

    // cut height?
    if (inHeight > outHeight)
    {
        // parse away half of the lines
        inFrame += (( inHeight - outHeight ) / 2) * inWidth * 2;
    }
    // cut width?
    if (inWidth > outWidth)
    {
        cutDiff = (inWidth - outWidth); // in pixels
        // start half of the width diff into the line
        // each pixel is 2 bytes hence diff is the correct value in bytes
        inFrame += cutDiff;
    }
    // pad height?
    if (inHeight < outHeight)
    {
        // pad top
        WebRtc_Word32 diff = (outHeight - inHeight) >> 1;
        memset(outI, 0, diff * outWidth);
        outI += diff * outWidth;
        WebRtc_Word32 colorLength =(diff >> 1) * (outWidth >> 1);
        memset(outCr, 127, colorLength);
        memset(outCb, 127, colorLength);
        outCr += colorLength;
        outCb += colorLength;

        // pad bottom
        memset(outI+outWidth * inHeight, 0, diff * outWidth);
        memset(outCr+(outWidth * inHeight >> 2), 127, colorLength);
        memset(outCb+(outWidth * inHeight >> 2), 127, colorLength);
    }
    // pad width?
    if (inWidth < outWidth)
    {
        padDiffLow = (outWidth - inWidth) >> 1; // in pixels
        padDiffHigh = (outWidth - inWidth) - padDiffLow; // in pixels
    }
    WebRtc_Word32 height = 0;
    if (inHeight > outHeight)
        height = outHeight;
    else
        height = inHeight;

    for (; i < (height >> 1); i++) // 2 rows per loop
    {
        // pad beginning of row?
        if (padDiffLow)
        {
            // pad row
            memset(outI,0,padDiffLow);
            memset(outCr, 127, padDiffLow >> 1);
            memset(outCb, 127, padDiffLow >> 1);
            outI += padDiffLow;
            outCr += padDiffLow / 2;
            outCb += padDiffLow / 2;

            for (j = 0; j < (inWidth >> 1); j++) // 2 pixels per loop
            {
                outI[0] = inFrame[1];
                *outCr = inFrame[0];
                outI[1] = inFrame[3];
                *outCb = inFrame[2];
                inFrame += 4;
                outI += 2;
                outCr++;
                outCb++;
            }
            // pad end of row?
            if (padDiffHigh)
            {
                memset(outI, 0, padDiffHigh);
                memset(outCr, 127, padDiffHigh >> 1);
                memset(outCb, 127, padDiffHigh >> 1);
                outI += padDiffHigh;
                outCr += padDiffHigh >> 1;
                outCb += padDiffHigh >> 1;
            }
            // next row
            // pad beginning of row?
            memset(outI,0,padDiffLow);
            outI += padDiffLow;

            for (j = 0; j < (inWidth >> 1); j++)
            {
                outI[0] = inFrame[1];
                outI[1] = inFrame[3];
                inFrame += 4;
                outI += 2;
            }
            // pad end of row?
            if (padDiffHigh)
            {
                memset(outI,0,padDiffHigh);
                outI += padDiffHigh;
            }
        } else
        {
            // cut row
            for (j = 0; j < (outWidth >> 1); j++) // 2 pixels per loop
            {
                outI[0] = inFrame[1];
                *outCr = inFrame[0];
                outI[1] = inFrame[3];
                *outCb = inFrame[2];
                inFrame += 4;
                outI += 2;
                outCr++;
                outCb++;
            }
            inFrame -= (outWidth * 2);
            const WebRtc_UWord8 *inFrame2 = inFrame + (inWidth * 2) * 2;

            if(i + 1 == (height >> 1))
            {
                // last row
                for (j = 0; j < (outWidth >> 1); j++)
                {
                    // copy last row
                    outI[0] = inFrame[1];
                    outI[1] = inFrame[3];
                    inFrame += 4;
                    inFrame2 += 4;
                    outI += 2;
                }
            } else
            {
                // next row
                for (j = 0; j < (outWidth >> 1); j++)
                {
                    outI[0] = (inFrame[1] + inFrame2[1]) >> 1;
                    outI[1] = (inFrame[3] + inFrame2[1]) >> 1;
                    inFrame += 4;
                    inFrame2 += 4;
                    outI += 2;
                }
            }
            inFrame += cutDiff * 2;
            inFrame += inWidth * 2; // skip next row
        }
    }
    return outWidth * (outHeight >> 1) * 3;
}

WebRtc_Word32
ConvertUYVYToI420(WebRtc_UWord32 width,WebRtc_UWord32 height,
                  const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    WebRtc_UWord32 i = 0;
    WebRtc_UWord32 j = 0;
    WebRtc_UWord8* outI = outFrame;
    WebRtc_UWord8* outCr = outFrame + width * height;
    WebRtc_UWord8* outCb = outFrame + width * height + width * (height >> 2);
    for (; i< (height >> 1);i++)
    {
        for (j = 0; j < (width >> 1); j++)
        {
            outI[0] = inFrame[1];
            *outCr = inFrame[0];
            outI[1] = inFrame[3];
            *outCb = inFrame[2];
            inFrame += 4;
            outI += 2;
            outCr++;
            outCb++;
        }
        for (j = 0; j < (width >> 1); j++)
        {
            outI[0] = inFrame[1];
            outI[1] = inFrame[3];
            inFrame += 4;
            outI += 2;
        }
    }
    return width * (height >> 1) * 3;
}

WebRtc_Word32
ConvertYUY2ToI420interlaced(const WebRtc_UWord8* inFrame, WebRtc_UWord32 inWidth,
                            WebRtc_UWord32 inHeight, WebRtc_UWord8* outFrame,
                            WebRtc_UWord32 outWidth, WebRtc_UWord32 outHeight)
{
    if (inWidth < 1 || inHeight < 1 || outHeight < 1 || outWidth < 1)
    {
        return -1;
    }
    // use every other row and interpolate the removed row
    WebRtc_UWord32 i = 0;
    WebRtc_UWord32 j = 0;
    WebRtc_Word32 cutDiff = 0; // in pixels
    WebRtc_Word32 padDiffLow = 0; // in pixels
    WebRtc_Word32 padDiffHigh = 0; // in pixels
    WebRtc_UWord8* outI = outFrame;
    // ptr to third row
    WebRtc_UWord8* inPtr3 = (WebRtc_UWord8*)inFrame + inWidth * 2 * 2;
    WebRtc_UWord8* outCr = outFrame + outWidth * outHeight;
    WebRtc_UWord8* outCb = outFrame +outWidth * outHeight +
                           outWidth * (outHeight >> 2);

    // cut height?
    if(inHeight > outHeight)
    {
        // parse away half of the lines
        inFrame += ((inHeight - outHeight) / 2) * inWidth * 2;
        inPtr3 += ((inHeight - outHeight) / 2) * inWidth * 2;
    }
    // cut width?
    if(inWidth > outWidth)
    {
        cutDiff = (inWidth - outWidth); // in pixels
        // start half of the width diff into the line
        inPtr3 += cutDiff;
        // each pixel is 2 bytes hence diff is the correct value in bytes
        inFrame += cutDiff;
    }
    // pad height?
    if(inHeight < outHeight)
    {
        // pad top
        WebRtc_Word32 diff = (outHeight - inHeight) / 2;
        memset(outI, 0, diff * outWidth);
        outI += diff * outWidth;
        WebRtc_Word32 colorLength =(diff / 2) * (outWidth / 2);
        memset(outCr, 127, colorLength);
        memset(outCb, 127, colorLength);
        outCr+= colorLength;
        outCb+= colorLength;

        // pad bottom
        memset(outI + outWidth * inHeight, 0, diff * outWidth);
        memset(outCr + (outWidth * inHeight / 4), 127, colorLength);
        memset(outCb + (outWidth * inHeight / 4), 127, colorLength);
    }
    // pad width?
    if(inWidth < outWidth)
    {
        padDiffLow = (outWidth - inWidth) / 2; // in pixels
        padDiffHigh = (outWidth - inWidth) - padDiffLow; // in pixels
    }
    WebRtc_UWord32 height = 0;
    if(inHeight > outHeight)
        height = outHeight;
    else
        height = inHeight;

    for (; i< (height >> 1);i++) // 2 rows per loop
    {
        // pad beginning of row?
        if(padDiffLow)
        {
            // pad row
            memset(outI, 0, padDiffLow);
            memset(outCr, 127, padDiffLow / 2);
            memset(outCb, 127, padDiffLow / 2);
            outI += padDiffLow;
            outCr += padDiffLow / 2;
            outCb += padDiffLow / 2;

            for (j = 0; j< (inWidth >> 1);j++) // 2 pixels per loop
            {
                outI[0] = inFrame[0];
                *outCr = inFrame[1];
                outI[1] = inFrame[2];
                *outCb = inFrame[3];
                inFrame +=4;
                outI += 2;
                outCr++;
                outCb++;
            }
            // pad end of row?
            if (padDiffHigh)
            {
                memset(outI, 0, padDiffHigh);
                memset(outCr, 127, padDiffHigh / 2);
                memset(outCb, 127, padDiffHigh / 2);
                outI += padDiffHigh;
                outCr += padDiffHigh / 2;
                outCb += padDiffHigh / 2;
            }
            // next row
            // pad beginning of row?
            memset(outI,0,padDiffLow);
            outI += padDiffLow;
            inFrame -= inWidth * 2;
            if (i == (height >> 1) - 1)
            {
                // last loop
                // copy the last row
                for (j = 0; j< (inWidth >> 1); j++)
                {
                    outI[0] = inFrame[0];
                    outI[1] = inFrame[2];
                    inFrame += 4;
                    outI += 2;
                }
            } else
            {
                // turn back inFrame
                for (j = 0; j < (inWidth >> 1); j++)
                {
                    outI[0] = (inFrame[0] + inPtr3[0]) >> 1;
                    outI[1] = (inFrame[2] + inPtr3[2]) >> 1;
                    inFrame += 4;
                    inPtr3 += 4;
                    outI += 2;
                }
                inFrame += inWidth * 2;
                inPtr3 += inWidth * 2;
            }

            // pad end of row?
            if (padDiffHigh)
            {
                memset(outI,0,padDiffHigh);
                outI += padDiffHigh;
            }
        } else
        {
            // cut row
            for (j = 0; j < (outWidth >> 1); j++) // 2 pixels per loop
            {
                outI[0] = inFrame[0];
                *outCr = inFrame[1];
                outI[1] = inFrame[2];
                *outCb = inFrame[3];
                inFrame += 4;
                outI += 2;
                outCr++;
                outCb++;
            }
            inFrame += cutDiff * 2;
            inFrame -= inWidth * 2;

            if (i == (height >> 1) -1)
            {
                // last loop
                // copy the last row
                for (j = 0; j < (outWidth >> 1);j++)
                {
                    outI[0] = inFrame[0];
                    outI[1] = inFrame[2];
                    inFrame +=4;
                    outI += 2;
                }
            } else
            {
                // next row
                for (j = 0; j< (outWidth >> 1);j++)
                {
                    outI[0] = (inFrame[0] + inPtr3[0]) >> 1;
                    outI[1] = (inFrame[2] + inPtr3[2]) >> 1;
                    inPtr3 += 4;
                    inFrame += 4;
                    outI += 2;
                }
                inFrame += cutDiff * 2;
                inPtr3 += cutDiff * 2;
            }
            inFrame += inWidth * 2;
            inPtr3 += inWidth * 2;
        }
    }
    return outWidth * (outHeight >> 1) * 3;
}

WebRtc_Word32
ConvertYUY2ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord32 inWidth,
                  WebRtc_UWord32 inHeight, WebRtc_UWord8* outFrame,
                  WebRtc_UWord32 outWidth, WebRtc_UWord32 outHeight)
{
    if (inWidth < 1 || inHeight < 1 || outHeight < 1 || outWidth < 1)
    {
        return -1;
    }
    WebRtc_UWord32 i = 0;
    WebRtc_UWord32 j = 0;
    WebRtc_Word32 cutDiff = 0; // in pixels
    WebRtc_Word32 padDiffLow = 0; // in pixels
    WebRtc_Word32 padDiffHigh = 0; // in pixels
    WebRtc_UWord8* outI = outFrame;
    WebRtc_UWord8* outCr = outFrame + outWidth * outHeight;
    WebRtc_UWord8* outCb = outFrame + outWidth * outHeight +
                           outWidth * (outHeight >> 2);

    // cut height?
    if (inHeight > outHeight)
    {
        // parse away half of the lines
        inFrame += ((inHeight - outHeight) >> 1) * inWidth * 2;
    }
    // cut width?
    if (inWidth > outWidth)
    {
        cutDiff = (inWidth - outWidth); // in pixels
        // start half of the width diff into the line
        // each pixel is 2 bytes hence diff is the correct value in bytes
        inFrame += cutDiff;
    }
    // pad height?
    if (inHeight < outHeight)
    {
        // pad top
        WebRtc_Word32 diff = (outHeight - inHeight) >> 1;
        memset(outI, 0, diff * outWidth);
        outI += diff * outWidth;
        WebRtc_Word32 colorLength =(diff >> 1) * (outWidth >> 1);
        memset(outCr, 127, colorLength);
        memset(outCb, 127, colorLength);
        outCr += colorLength;
        outCb += colorLength;

        // pad bottom
        memset(outI + outWidth * inHeight, 0, diff * outWidth);
        memset(outCr + (outWidth * inHeight >> 2), 127, colorLength);
        memset(outCb + (outWidth * inHeight >> 2), 127, colorLength);
    }
    // pad width?
    if (inWidth < outWidth)
    {
        padDiffLow = (outWidth - inWidth) >> 1; // in pixels
        padDiffHigh = (outWidth - inWidth) - padDiffLow; // in pixels
    }
    WebRtc_UWord32 height = 0;
    if (inHeight > outHeight)
        height = outHeight;
    else
        height = inHeight;

    for (; i< (height >> 1); i++) // 2 rows per loop
    {
        // pad beginning of row?
        if (padDiffLow)
        {
            // pad row
            memset(outI,0,padDiffLow);
            memset(outCr,127,padDiffLow >> 1);
            memset(outCb,127,padDiffLow >> 1);
            outI += padDiffLow;
            outCr += padDiffLow >> 1;
            outCb += padDiffLow >> 1;

            for (j = 0; j < (inWidth >> 1);j++) // 2 pixels per loop
            {
                outI[0] = inFrame[0];
                *outCr = (inFrame[1] + inFrame[1 + inWidth] + 1) >> 1;
                outI[1] = inFrame[2];
                *outCb = (inFrame[3] + inFrame[3 + inWidth] + 1) >> 1;;
                inFrame += 4;
                outI += 2;
                outCr++;
                outCb++;
            }
            // pad end of row?
            if (padDiffHigh)
            {
                memset(outI,0,padDiffHigh);
                memset(outCr,127,padDiffHigh >> 1);
                memset(outCb,127,padDiffHigh >> 1);
                outI += padDiffHigh;
                outCr += padDiffHigh >> 1;
                outCb += padDiffHigh >> 1;
            }
            // next row
            // pad beginning of row?
            memset(outI,0,padDiffLow);
            outI += padDiffLow;

            for (j = 0; j < (inWidth >> 1); j++)
            {
                outI[0] = inFrame[0];
                outI[1] = inFrame[2];
                inFrame += 4;
                outI += 2;
            }
            // pad end of row?
            if (padDiffHigh)
            {
                memset(outI,0,padDiffHigh);
                outI += padDiffHigh;
            }
        } else
        {
            // cut row
            for (j = 0; j < (outWidth >> 1); j++) // 2 pixels per loop
            {
                outI[0] = inFrame[0];
                *outCr = inFrame[1];
                outI[1] = inFrame[2];
                *outCb = inFrame[3];
                inFrame += 4;
                outI += 2;
                outCr++;
                outCb++;
            }
            inFrame += cutDiff * 2;
            // next row
            for (j = 0; j < (outWidth >> 1); j++)
            {
                outI[0] = inFrame[0];
                outI[1] = inFrame[2];
                inFrame += 4;
                outI += 2;
            }
            inFrame += cutDiff * 2;
        }
    }
    return outWidth * (outHeight >> 1) * 3;
}

WebRtc_Word32
ConvertYUY2ToI420(WebRtc_UWord32 width, WebRtc_UWord32 height,
                  const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    WebRtc_UWord32 i = 0;
    WebRtc_UWord32 j = 0;
    WebRtc_UWord8* outI = outFrame;
    WebRtc_UWord8* outCr = outFrame + width * height;
    WebRtc_UWord8* outCb = outFrame + width * height + width * (height >> 2);
#ifndef SCALEOPT


    for (; i < (height >> 1); i++)
    {
        for (j = 0; j < (width >> 1); j++)
        {
            outI[0] = inFrame[0];
            *outCr = (inFrame[1] + inFrame[1 + width] + 1) >> 1;
            outI[1] = inFrame[2];
            *outCb = (inFrame[3] + inFrame[3 + width] + 1) >> 1;
            inFrame += 4;
            outI += 2;
            outCr++;
            outCb++;
        }
        for (j = 0; j < (width >> 1); j++)
        {
            outI[0] = inFrame[0];
            outI[1] = inFrame[2];
            inFrame += 4;
            outI += 2;
        }
    }
#else

    WebRtc_Word32 height_half = height / 2;

    _asm{
     mov       esi, DWORD PTR [width]
     mov       edx, DWORD PTR [height_half]

     ; prepare masks:
     pxor      xmm0, xmm0
     pcmpeqd   xmm1, xmm1
     punpcklbw xmm1, xmm0
     pcmpeqd   xmm2, xmm2
     punpcklbw xmm0, xmm2
     test      edx, edx
     jle       exit_

     xor       ebx, ebx
     mov       edi, DWORD PTR [outFrame]
     test      esi, esi
     jle       exit_

     loop0:
     add       ebx, 1

     mov       DWORD PTR [i], ebx
     mov       ebx, DWORD PTR [inFrame]
     mov       edx, DWORD PTR [outCr]
     mov       eax, DWORD PTR [outCb]
     xor       ecx, ecx

     loop1:

     movdqa    xmm5, xmm1

     movdqa    xmm2, xmm1
     movdqu    xmm4, XMMWORD PTR [ebx]
     movdqu    xmm3, XMMWORD PTR [ebx+16]
     pand      xmm5, xmm4
     pand      xmm2, xmm3

     pavgb     xmm4, XMMWORD PTR [ebx + esi * 2]
     pavgb     xmm3, XMMWORD PTR [ebx + 16 + esi * 2]

     add       ebx, 32
     pand      xmm4, xmm0
     psrldq    xmm4, 1
     pand      xmm3, xmm0
     psrldq    xmm3, 1

     packuswb  xmm5, xmm2

     movdqu    XMMWORD PTR [edi], xmm5
     movdqa    xmm2, xmm1
     packuswb  xmm4, xmm3
     pand      xmm2, xmm4
     pand      xmm4, xmm0
     psrldq    xmm4, 1
     packuswb  xmm2, xmm4

     movq      QWORD PTR [edx], xmm2
     psrldq    xmm2, 8
     movq      QWORD PTR [eax], xmm2

     add       edi, 16
     add       edx, 8
     add       eax, 8
     add       ecx, 16
     cmp       ecx, esi
     jl        loop1

     mov       DWORD PTR [outCb], eax
     mov       DWORD PTR [outCr], edx
     mov       edx, DWORD PTR [height_half]
     mov       DWORD PTR [inFrame], ebx
     mov       ebx, DWORD PTR [i]

     test      esi, esi
     jle       exit_

     mov       eax, DWORD PTR [inFrame] //now becomes 00568FE8
     xor       ecx, ecx

     loop2:

     movdqu    xmm3, XMMWORD PTR [eax]
     movdqu    xmm2, XMMWORD PTR [eax+16]
     add       eax, 32
     pand      xmm3, xmm1
     pand      xmm2, xmm1
     packuswb  xmm3, xmm2
     movdqu    XMMWORD PTR [edi], xmm3

     add       edi, 16
     add       ecx, 16
     cmp       ecx, esi
     jl        loop2

     mov       DWORD PTR [inFrame], eax //now 005692A8
     mov       eax, DWORD PTR [width]
     cmp       ebx, edx
     jl        loop0
     exit_:
     }
#endif
    return width * (height >> 1) * 3;
}

// make a center cut
WebRtc_Word32
CutI420Frame(WebRtc_UWord8* frame,
             WebRtc_UWord32 fromWidth, WebRtc_UWord32 fromHeight,
             WebRtc_UWord32 toWidth, WebRtc_UWord32 toHeight)
{
    if (toWidth < 1 || fromWidth < 1 || toHeight < 1 || fromHeight < 1 )
    {
        return -1;
    }
    if (toWidth == fromWidth && toHeight == fromHeight)
    {
        // nothing to do
      return 3 * toHeight * toWidth / 2;
    }
    if (toWidth > fromWidth || toHeight > fromHeight)
    {
        // error
        return -1;
    }
    WebRtc_UWord32 i = 0;
    WebRtc_Word32 m = 0;
    WebRtc_UWord32 loop = 0;
    WebRtc_UWord32 halfToWidth = toWidth / 2;
    WebRtc_UWord32 halfToHeight = toHeight / 2;
    WebRtc_UWord32 halfFromWidth = fromWidth / 2;
    WebRtc_UWord32 halfFromHeight= fromHeight / 2;
    WebRtc_UWord32 cutHeight = ( fromHeight - toHeight ) / 2; //12
    WebRtc_UWord32 cutWidth = ( fromWidth - toWidth ) / 2; // 16

    for (i = fromWidth * cutHeight + cutWidth; loop < toHeight ;
        loop++, i += fromWidth)
    {
        memcpy(&frame[m],&frame[i],toWidth);
        m += toWidth;
    }
    i = fromWidth * fromHeight; // ilum
    loop = 0;
    for ( i += (halfFromWidth * cutHeight / 2 + cutWidth / 2);
          loop < halfToHeight; loop++,i += halfFromWidth)
    {
        memcpy(&frame[m],&frame[i],halfToWidth);
        m += halfToWidth;
    }
    loop = 0;
    i = fromWidth * fromHeight + halfFromHeight * halfFromWidth; // ilum +Cr
    for ( i += (halfFromWidth * cutHeight / 2 + cutWidth / 2);
          loop < halfToHeight; loop++, i += halfFromWidth)
    {
        memcpy(&frame[m],&frame[i],halfToWidth);
        m += halfToWidth;
    }
    return halfToWidth * toHeight * 3;// new size  64*96*3; // 128/2 == 64
}

WebRtc_Word32
ConvertI420ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord32 inWidth,
                  WebRtc_UWord32 inHeight, WebRtc_UWord8* outFrame,
                  WebRtc_UWord32 outWidth, WebRtc_UWord32 outHeight)
{
    if (inWidth < 1 || outWidth < 1 || inHeight < 1 || outHeight < 1 )
    {
        return -1;
    }
    if (inWidth == outWidth && inHeight == outHeight)
    {
        memcpy(outFrame, inFrame, 3*outWidth*(outHeight>>1));
    }
    else
    {
        if ( inHeight < outHeight)
        {
            // pad height
            WebRtc_Word32 padH = outHeight - inHeight;
            WebRtc_UWord32 i =0;
            WebRtc_Word32 padW = 0;
            WebRtc_Word32 cutW = 0;
            WebRtc_Word32 width = inWidth;
            if (inWidth < outWidth)
            {
                // pad width
                padW = outWidth - inWidth;
            }
            else
            {
              // cut width
              cutW = inWidth - outWidth;
              width = outWidth;
            }
            if (padH)
            {
                memset(outFrame, 0, outWidth * (padH >> 1));
                outFrame +=  outWidth * (padH >> 1);
            }
            for (i = 0; i < inHeight;i++)
            {
                if (padW)
                {
                    memset(outFrame, 0, padW / 2);
                    outFrame +=  padW / 2;
                }
                inFrame += cutW >> 1; // in case we have a cut
                memcpy(outFrame,inFrame ,width);
                inFrame += cutW >> 1;
                outFrame += width;
                inFrame += width;
                if (padW)
                {
                    memset(outFrame, 0, padW / 2);
                    outFrame +=  padW / 2;
                }
            }
            if (padH)
            {
                memset(outFrame, 0, outWidth * (padH >> 1));
                outFrame +=  outWidth * (padH >> 1);
            }
            if (padH)
            {
                memset(outFrame, 127, (outWidth >> 2) * (padH >> 1));
                outFrame +=  (outWidth >> 2) * (padH >> 1);
            }
            for (i = 0; i < (inHeight >> 1); i++)
            {
                if (padW)
                {
                    memset(outFrame, 127, padW >> 2);
                    outFrame +=  padW >> 2;
                }
                inFrame += cutW >> 2; // in case we have a cut
                memcpy(outFrame, inFrame,width >> 1);
                inFrame += cutW >> 2;
                outFrame += width >> 1;
                inFrame += width >> 1;
                if (padW)
                {
                    memset(outFrame, 127, padW >> 2);
                    outFrame +=  padW >> 2;
                }
            }
            if (padH)
            {
                memset(outFrame, 127, (outWidth >> 1) * (padH >> 1));
                outFrame +=  (outWidth >> 1) * (padH >> 1);
            }
            for (i = 0; i < (inHeight >> 1); i++)
            {
                if (padW)
                {
                    memset(outFrame, 127, padW >> 2);
                    outFrame +=  padW >> 2;
                }
                inFrame += cutW >> 2; // in case we have a cut
                memcpy(outFrame, inFrame,width >> 1);
                inFrame += cutW >> 2;
                outFrame += width >> 1;
                inFrame += width >> 1;
                if (padW)
                {
                    memset(outFrame, 127, padW >> 2);
                    outFrame += padW >> 2;
                }
            }
            if (padH)
            {
                memset(outFrame, 127, (outWidth >> 2) * (padH >> 1));
                outFrame +=  (outWidth >> 2) * (padH >> 1);
            }
        }
        else
        {
            // cut height
            WebRtc_UWord32 i =0;
            WebRtc_Word32 padW = 0;
            WebRtc_Word32 cutW = 0;
            WebRtc_Word32 width = inWidth;

            if (inWidth < outWidth)
            {
                // pad width
                padW = outWidth - inWidth;
            } else
            {
                // cut width
                cutW = inWidth - outWidth;
                width = outWidth;
            }
            WebRtc_Word32 diffH = inHeight - outHeight;
            inFrame += inWidth * (diffH >> 1);  // skip top I

            for (i = 0; i < outHeight; i++)
            {
                if (padW)
                {
                    memset(outFrame, 0, padW / 2);
                    outFrame +=  padW / 2;
                }
                inFrame += cutW >> 1; // in case we have a cut
                memcpy(outFrame,inFrame ,width);
                inFrame += cutW >> 1;
                outFrame += width;
                inFrame += width;
                if (padW)
                {
                    memset(outFrame, 0, padW / 2);
                    outFrame +=  padW / 2;
                }
            }
            inFrame += inWidth * (diffH >> 1);  // skip end I
            inFrame += (inWidth >> 2) * (diffH >> 1); // skip top of Cr
            for (i = 0; i < (outHeight >> 1); i++)
            {
                if (padW)
                {
                    memset(outFrame, 127, padW >> 2);
                    outFrame +=  padW >> 2;
                }
                inFrame += cutW >> 2; // in case we have a cut
                memcpy(outFrame, inFrame,width >> 1);
                inFrame += cutW >> 2;
                outFrame += width >> 1;
                inFrame += width >> 1;
                if (padW)
                {
                    memset(outFrame, 127, padW >> 2);
                    outFrame +=  padW >> 2;
                }
            }
            inFrame += (inWidth >> 2) * (diffH >> 1); // skip end of Cr
            inFrame += (inWidth >> 2) * (diffH >> 1); // skip top of Cb
            for (i = 0; i < (outHeight >> 1); i++)
            {
                if (padW)
                {
                    memset(outFrame, 127, padW >> 2);
                    outFrame +=  padW >> 2;
                }
                inFrame += cutW >> 2; // in case we have a cut
                memcpy(outFrame, inFrame, width >> 1);
                inFrame += cutW >> 2;
                outFrame += width >> 1;
                inFrame += width >> 1;
                if (padW)
                {
                    memset(outFrame, 127, padW >> 2);
                    outFrame +=  padW >> 2;
                }
            }
        }
    }
    return 3 * outWidth * (outHeight >> 1);
}

WebRtc_Word32
ConvertRGB24ToARGB(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                   WebRtc_UWord32 width, WebRtc_UWord32 height,
                   WebRtc_UWord32 strideOut)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    if (strideOut == 0)
    {
        strideOut = width;
    }
    else if (strideOut < width)
    {
        return -1;
    }

    WebRtc_UWord32 i, j, offset;

    outFrame += strideOut * (height - 1) * 4;
    for(i = 0; i < height; i++)
    {
        for(j = 0; j < width; j++)
        {
            offset = j*4;
            outFrame[0 + offset] = inFrame[0];
            outFrame[1 + offset] = inFrame[1];
            outFrame[2 + offset] = inFrame[2];
            outFrame[3 + offset] = 0xff;
            inFrame += 3;
        }
        outFrame -= 4 * (strideOut - width);
    }
    return strideOut * height * 4;
}

WebRtc_Word32
ConvertRGB24ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord32 inWidth,
                   WebRtc_UWord32 inHeight, WebRtc_UWord8* outFrame,
                   WebRtc_UWord32 outWidth, WebRtc_UWord32 outHeight)
{
    if (inWidth < 1 || outWidth < 1 || inHeight < 1 || outHeight < 1 )
    {
        return -1;
    }
    WebRtc_UWord32* yStartPtr = (WebRtc_UWord32*)(outFrame +
                                                 (outWidth * outHeight));
    WebRtc_UWord8* uStartPtr = outFrame + (outWidth * outHeight) +
                               ((outWidth * outHeight) >> 2);
    WebRtc_UWord8* vStartPtr = outFrame + (outWidth * outHeight) +
                               ((outWidth * outHeight) >> 1);

    yStartPtr--;
    uStartPtr--;
    vStartPtr--;
    const WebRtc_UWord8* inpPtr;
    const WebRtc_UWord8* inFramePtr = inFrame;
    WebRtc_Word32 offset = 0;
    WebRtc_Word32 height = inHeight;
    WebRtc_Word32 cutDiff = 0;
    WebRtc_Word32 padDiffLow= 0;
    WebRtc_Word32 padDiffHigh = 0;

    if (inHeight > outHeight)
    {
        // cut height
        // skip the first diff/2 rows
        inFramePtr += inWidth * 3 * ((inHeight - outHeight) >> 1);
        height = outHeight;
    }
    if (outHeight > inHeight)
    {
        // Pad height.
        WebRtc_UWord8* outI = outFrame;
        WebRtc_UWord8* outCr = outFrame + outWidth * outHeight;
        WebRtc_UWord8* outCb = outCr + ((outWidth * outHeight) >> 2);

        // -- I --
        WebRtc_UWord32 padHeight = outHeight - inHeight;
        WebRtc_UWord32 padHeightT = padHeight >> 1;
        WebRtc_UWord32 padHeightB = padHeight - padHeightT;
        WebRtc_UWord32 padLength = padHeightT * outWidth;
        memset(outI, 0, padLength); // Pad the top.
        outI += padLength;

        outI += outWidth * inHeight; // Skip the image.
        padLength = padHeightB * outWidth;
        memset(outI, 0, padLength); // Pad the bottom.

        // Shift the out poWebRtc_Word32er.
        yStartPtr -= (padLength >> 2); // (>> 2) due to WebRtc_Word32 pointer.

        // -- Cr and Cb --
        padHeight >>= 1;
        padHeightT >>= 1;
        padHeightB = padHeight - padHeightT;

        padLength = padHeightT * (outWidth >> 1);
        memset(outCr, 127, padLength); // Pad the top.
        memset(outCb, 127, padLength);
        outCr += padLength;
        outCb += padLength;

        padLength = (outWidth * inHeight) >> 2;
        outCr += padLength; // Skip the image.
        outCb += padLength;
        padLength = padHeightB * (outWidth >> 1);
        memset(outCr, 127, padLength); // Pad the bottom.
        memset(outCb, 127, padLength);

        // Shift the out pointers.
        uStartPtr -= padLength;
        vStartPtr -= padLength;
    }
    // cut width?
    if (inWidth > outWidth)
    {
        cutDiff = (inWidth - outWidth) >> 1; // in pixels
    }
    // pad width?
    if (inWidth < outWidth)
    {
        padDiffLow = (outWidth - inWidth) >> 1; // in pixels
        padDiffHigh = (outWidth - inWidth) - padDiffLow; // in pixels
    }

    for (WebRtc_Word32 y = 0; y < height; y++)
    {
        offset = y * inWidth * 3;
        inpPtr = &inFramePtr[offset + (inWidth - 4) * 3]; // right to left
        inpPtr -= 3*cutDiff;
        WebRtc_Word32 i = (inWidth - (cutDiff * 2)) >> 2;
        WebRtc_UWord32 tmp;
        if (padDiffLow)
        {
            yStartPtr -= padDiffLow >> 2; //div by 4 since its a WebRtc_Word32 ptr
            memset(yStartPtr + 1, 0, padDiffLow);
        }
        for (; i > 0; i--) // do 4 pixels wide in one loop
        {
#ifdef  WEBRTC_BIG_ENDIAN
            tmp = (WebRtc_UWord8)((66 * inpPtr[2] + 129 * inpPtr[1] +
                                   25 * inpPtr[0] + 128)
                   >> 8) + 16;
            tmp = tmp << 8;
            tmp += (WebRtc_UWord8)((66 * inpPtr[5] + 129 * inpPtr[4] +
                                    25 * inpPtr[3] + 128)
                    >> 8) + 16;
            tmp = tmp << 8;
            tmp += (WebRtc_UWord8)((66 *  inpPtr[8] + 129 * inpPtr[7] +
                                    25 * inpPtr[6] + 128) >> 8) + 16;
            tmp = tmp << 8;
            tmp += (WebRtc_UWord8)((66 * inpPtr[11] + 129 * inpPtr[10] +
                                    25 * inpPtr[9] + 128) >> 8) + 16;

#else
            tmp = (WebRtc_UWord8)((66 * inpPtr[11] + 129 * inpPtr[10] +
                                   25 * inpPtr[9] + 128) >> 8) + 16;
            tmp = tmp << 8;
            tmp += (WebRtc_UWord8)((66 * inpPtr[8] + 129 * inpPtr[7] +
                                    25 * inpPtr[6] + 128) >> 8) + 16;
            tmp = tmp << 8;
            tmp += (WebRtc_UWord8)((66 * inpPtr[5] + 129 * inpPtr[4] +
                                    25 * inpPtr[3] + 128) >> 8) + 16;
            tmp = tmp << 8;
            tmp += (WebRtc_UWord8)((66 * inpPtr[2] + 129 * inpPtr[1] +
                                    25 * inpPtr[0] + 128) >> 8) + 16;
#endif
            *yStartPtr = tmp;
            yStartPtr--;
            inpPtr -= 12;
        }
        if (padDiffHigh)
        {
            yStartPtr -= padDiffHigh >> 2; // WebRtc_Word32 => div by 4
            memset(yStartPtr + 1, 0, padDiffHigh);
        }
        y++; // doing an ugly add to my loop variable
        offset = y * inWidth * 3;
        inpPtr = &inFramePtr[offset + (inWidth - 4) * 3];
        inpPtr -= 3 * cutDiff;
        i = (inWidth - (cutDiff * 2)) >> 2;

        if (padDiffLow)
        {
            yStartPtr -= padDiffLow >> 2; // WebRtc_Word32 => div by 4
            uStartPtr -= padDiffLow >> 1;
            vStartPtr -= padDiffLow >> 1;
            memset(yStartPtr + 1, 0, padDiffLow);
            memset(uStartPtr + 1, 127, padDiffLow >> 1);
            memset(vStartPtr + 1, 127, padDiffLow >> 1);
        }
        for (; i > 0; i--)
        {
            *uStartPtr = (WebRtc_UWord8)((-38 * inpPtr[8] - 74 * inpPtr[7] +
                                          112 * inpPtr[6] + 128) >> 8) + 128;
            uStartPtr--;
            *vStartPtr = (WebRtc_UWord8)((112 * inpPtr[8] - 94 * inpPtr[7] -
                                          18 * inpPtr[6] + 128) >> 8) + 128;
            vStartPtr--;
            *uStartPtr = (WebRtc_UWord8)((-38 * inpPtr[2] - 74 * inpPtr[1] +
                                          112 * inpPtr[0] + 128) >> 8) + 128;
            uStartPtr--;
            *vStartPtr = (WebRtc_UWord8)((112 * inpPtr[2] - 94 * inpPtr[1] -
                                          18 * inpPtr[0] + 128) >> 8) + 128;
            vStartPtr--;
#ifdef WEBRTC_BIG_ENDIAN
            tmp = (WebRtc_UWord8)((66 * inpPtr[2] + 129 * inpPtr[1] +
                                   25 * inpPtr[0] + 128 ) >> 8) + 16;
            tmp = tmp << 8;
            tmp += (WebRtc_UWord8)((66 * inpPtr[5] + 129 * inpPtr[4] +
                                    25 * inpPtr[3] + 128) >> 8) + 16;
            tmp = tmp << 8;
            tmp += (WebRtc_UWord8)((66 * inpPtr[8] + 129 * inpPtr[7] +
                                    25 * inpPtr[6] + 128) >> 8) + 16;
            tmp = tmp << 8;
            tmp += (WebRtc_UWord8)((66 * inpPtr[11] + 129 * inpPtr[10] +
                                    25 * inpPtr[9] + 128) >> 8) + 16;
#else
            tmp = (WebRtc_UWord8)((66 * inpPtr[11] + 129 * inpPtr[10] +
                                   25 * inpPtr[9]+ 128) >> 8) + 16;
            tmp = tmp << 8;
            tmp += (WebRtc_UWord8)((66 * inpPtr[8] + 129 * inpPtr[7] +
                                    25 * inpPtr[6] + 128) >> 8) + 16;
            tmp = tmp << 8;
            tmp += (WebRtc_UWord8)((66 * inpPtr[5] + 129 * inpPtr[4] +
                                    25 * inpPtr[3] + 128 ) >> 8) + 16;
            tmp = tmp << 8;
            tmp += (WebRtc_UWord8)((66 * inpPtr[2] + 129 * inpPtr[1] +
                                    25 * inpPtr[0] + 128) >> 8) + 16;
#endif
            *yStartPtr = tmp;
            yStartPtr--;
            inpPtr -= 12;
        }
        if (padDiffHigh)
        {
            yStartPtr -= padDiffHigh >> 2; // WebRtc_Word32 => div by 4
            uStartPtr -= padDiffHigh >> 1;
            vStartPtr -= padDiffHigh >> 1;
            memset(yStartPtr + 1, 0, padDiffHigh);
            memset(uStartPtr + 1, 127, padDiffHigh >> 1);
            memset(vStartPtr + 1, 127, padDiffHigh >> 1);
        }
    }
    return (outWidth >> 1) * outHeight * 3;
}


WebRtc_Word32
ConvertRGB24ToI420(WebRtc_UWord32 width, WebRtc_UWord32 height,
                   const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame)
{
    if (height < 1 || width < 1)
    {
        return -1;
    }

    WebRtc_UWord8* yStartPtr;
    WebRtc_UWord8* yStartPtr2;
    WebRtc_UWord8* uStartPtr;
    WebRtc_UWord8* vStartPtr;
    const WebRtc_UWord8* inpPtr;
    const WebRtc_UWord8* inpPtr2;

    // assuming RGB in a bottom up orientation.
    yStartPtr = outFrame;
    yStartPtr2 = yStartPtr + width;
    uStartPtr = outFrame + (width * height);
    vStartPtr = uStartPtr + (width * height >> 2);
    inpPtr = inFrame + width * height * 3 - 3 * width;
    inpPtr2 = inpPtr - 3 * width;

    for (WebRtc_UWord32 h = 0; h < (height >> 1); h++ )
    {
        for (WebRtc_UWord32 w = 0; w < (width >> 1); w++)
        {
            //Y
            yStartPtr[0] =  (WebRtc_UWord8)((66 * inpPtr[2] + 129 * inpPtr[1]
                                            + 25 * inpPtr[0] + 128) >> 8) + 16;
            yStartPtr2[0] = (WebRtc_UWord8)((66 * inpPtr2[2] + 129 * inpPtr2[1]
                                            + 25 * inpPtr2[0] + 128) >> 8) + 16;
            // moving to next column
            yStartPtr[1] = (WebRtc_UWord8)((66 * inpPtr[5] + 129 * inpPtr[4]
                                           + 25 * inpPtr[3]  + 128) >> 8) + 16;
            yStartPtr2[1] = (WebRtc_UWord8)((66 * inpPtr2[5] + 129 * inpPtr2[4]
                                            + 25 * inpPtr2[3] + 128) >> 8 ) + 16;
            //U
            uStartPtr[0] = (WebRtc_UWord8)((-38 * inpPtr[2] - 74 * inpPtr[1] +
                                            112 * inpPtr[0] + 128) >> 8) + 128;
            //V
            vStartPtr[0] = (WebRtc_UWord8)((112 * inpPtr[2] -94 * inpPtr[1] -
                                           18 * inpPtr[0] + 128) >> 8) + 128;

            yStartPtr += 2;
            yStartPtr2 += 2;
            uStartPtr++;
            vStartPtr++;
            inpPtr += 6;
            inpPtr2 += 6;
        } // end for w
        yStartPtr += width;
        yStartPtr2 += width;
        inpPtr -= 9 * width;
        inpPtr2 -= 9 * width;
    } // end for h
    return (width >> 1) * height * 3;
}

WebRtc_Word32
ConvertI420ToARGBMac(const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                     WebRtc_UWord32 width, WebRtc_UWord32 height,
                     WebRtc_UWord32 strideOut)
{
    if (height < 1 || width < 1)
    {
        return -1;
    }
    if (strideOut == 0)
    {
        strideOut = width;
    } else if (strideOut  < width)
    {
        return -1;
    }
    WebRtc_Word32 diff = strideOut - width;
    WebRtc_UWord8* out = outFrame;
    WebRtc_UWord8* out2 = out + strideOut * 4;
    const WebRtc_UWord8 *y1,*y2, *u, *v;
    WebRtc_UWord32 h, w;
    y1 = inFrame;
    y2 = y1 + width;
    v = y1 + width * height;
    u = v + ((width * height) >> 2);

    for (h = (height >> 1); h > 0; h--)
    {
      WebRtc_Word32 tmpG, tmpB, tmpR;
        //do 2 rows at the time
        for (w = 0; w < (width >> 1); w++)
        {
            tmpR = (WebRtc_Word32)((mapYc[y1[0]] + mapVcr[v[0]] + 128  )>> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[0]] + mapUcb[u[0]] + 128 )>> 8);
            out[2] = Clip(tmpR);
            out[1] = Clip(tmpG);
            out[0] = Clip(tmpB);

            tmpR = (WebRtc_Word32)((mapYc[y2[0]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[0]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[0]] + mapUcb[u[0]] + 128) >> 8);
            out2[2] = Clip(tmpR);
            out2[1] = Clip(tmpG);
            out2[0] = Clip(tmpB);

            tmpR = (WebRtc_Word32)((mapYc[y1[1]] + mapVcr[v[0]] + 128)>> 8);
            tmpG = (WebRtc_Word32)((mapYc[y1[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y1[1]] + mapUcb[u[0]] + 128) >> 8);
            out[6] = Clip(tmpR);
            out[5] = Clip(tmpG);
            out[4] = Clip(tmpB);

            tmpR = (WebRtc_Word32)((mapYc[y2[1]] + mapVcr[v[0]] + 128) >> 8);
            tmpG = (WebRtc_Word32)((mapYc[y2[1]] + mapUcg[u[0]] + mapVcg[v[0]]
                                    + 128) >> 8);
            tmpB = (WebRtc_Word32)((mapYc[y2[1]] + mapUcb[u[0]] + 128) >> 8);
            out2[6] = Clip(tmpR);
            out2[5] = Clip(tmpG);
            out2[4] = Clip(tmpB);


            out[3] = 0xff;
            out[7] = 0xff;
            out += 8;
            out2[3] = 0xff;
            out2[7] = 0xff;
            out2 += 8;
            y1 += 2;
            y2 += 2;
            u++;
            v++;
        }

        y1 += width;
        y2 += width;
        out += (width + diff * 2) * 4;
        out2 += (width + diff * 2) * 4;
    }
    return strideOut * height * 4;
}

WebRtc_Word32
ConvertRGB565ToI420(const WebRtc_UWord8* inFrame, WebRtc_UWord32 width,
                    WebRtc_UWord32 height, WebRtc_UWord8* outFrame)
{
    if (width < 1 || height < 1 )
    {
        return -1;
    }
    WebRtc_UWord8 tmpR, tmpG, tmpB;
    WebRtc_UWord8 tmpR2, tmpG2, tmpB2;

    WebRtc_UWord8* yStartPtr = outFrame;
    WebRtc_UWord8* yStartPtr2 = yStartPtr + width;
    WebRtc_UWord8* uStartPtr = outFrame + (width * height);
    WebRtc_UWord8* vStartPtr = uStartPtr + (width * height >> 2);
    const WebRtc_UWord16* inpPtr = (const WebRtc_UWord16*)inFrame;
    inpPtr += width * (height - 1);
    const WebRtc_UWord16* inpPtr2 = inpPtr - width;

    for (WebRtc_UWord32 h = 0; h < (height >> 1); h++ )
    {
        for (WebRtc_UWord32 w = 0; w < (width >> 1); w++)
        {
          // calculating 8 bit values
            tmpB = (WebRtc_UWord8)((inpPtr[0] & 0x001F) << 3);
            tmpG = (WebRtc_UWord8)((inpPtr[0] & 0x07E0) >> 3);
            tmpR = (WebRtc_UWord8)((inpPtr[0] & 0xF800) >> 8);
            tmpB2 = (WebRtc_UWord8)((inpPtr2[0] & 0x001F) << 3);
            tmpG2 = (WebRtc_UWord8)((inpPtr2[0] & 0x07E0) >> 3);
            tmpR2 = (WebRtc_UWord8)((inpPtr2[0] & 0xF800) >> 8);

            //Y
            yStartPtr[0] = (WebRtc_UWord8)((66 * tmpR + 129 * tmpG + 25 * tmpB
                                            + 128) >> 8) + 16;
            //U
            uStartPtr[0] = (WebRtc_UWord8)((-38 * tmpR - 74 * tmpG + 112 * tmpB
                                            + 128) >> 8) + 128;
            //V
            vStartPtr[0] = (WebRtc_UWord8)((112 * tmpR - 94 * tmpG - 18 * tmpB
                                            + 128) >> 8) + 128;

            yStartPtr2[0] = (WebRtc_UWord8)((66 * tmpR2 + 129 * tmpG2 +
                                             25 * tmpB2 + 128) >> 8) + 16;

            // moving to next column
            tmpB = (WebRtc_UWord8)((inpPtr[1] & 0x001F) << 3);
            tmpG = (WebRtc_UWord8)((inpPtr[1] & 0x07E0) >> 3);
            tmpR = (WebRtc_UWord8)((inpPtr[1] & 0xF800) >> 8);

            tmpB2 = (WebRtc_UWord8)((inpPtr2[1] & 0x001F) << 3);
            tmpG2 = (WebRtc_UWord8)((inpPtr2[1] & 0x07E0) >> 3);
            tmpR2 = (WebRtc_UWord8)((inpPtr2[1] & 0xF800) >> 8);

            yStartPtr[1] =  (WebRtc_UWord8)((66 * tmpR + 129 * tmpG +
                                             25 * tmpB + 128) >> 8) + 16;
            yStartPtr2[1] =  (WebRtc_UWord8)((66 * tmpR2 +129 * tmpG2 +
                                              25 * tmpB2 + 128) >> 8) + 16;

            yStartPtr += 2;
            yStartPtr2 += 2;
            uStartPtr++;
            vStartPtr++;
            inpPtr += 2;
            inpPtr2 += 2;
        }
        yStartPtr += width;
        yStartPtr2 += width;
        inpPtr -= 3 * width;
        inpPtr2 -= 3 * width;
    }
    return (width >> 1) * height * 3;
}

WebRtc_Word32
ConvertARGBMacToI420(WebRtc_UWord32 width, WebRtc_UWord32 height,
                     const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame)
{
    if (height < 1 || width < 1)
    {
        return -1;
    }

    WebRtc_UWord8* yStartPtr;
    WebRtc_UWord8* yStartPtr2;
    WebRtc_UWord8* uStartPtr;
    WebRtc_UWord8* vStartPtr;
    const WebRtc_UWord8* inpPtr;
    const WebRtc_UWord8* inpPtr2;

    yStartPtr = outFrame;
    yStartPtr2 = yStartPtr + width;
    uStartPtr = outFrame + (width * height);
    vStartPtr = uStartPtr + (width * height >> 2);
    inpPtr = inFrame;
    inpPtr2 = inpPtr + 4 * width;
    WebRtc_UWord32 h, w;
    for (h = 0; h < (height >> 1); h++)
    {
        for (w = 0; w < (width >> 1); w++)
        {   //Y
            yStartPtr[0]  = (WebRtc_UWord8)((66 * inpPtr[1] + 129 * inpPtr[2]
                                             + 25 * inpPtr[3] + 128) >> 8) + 16;
            yStartPtr2[0] = (WebRtc_UWord8)((66 * inpPtr2[1] + 129 * inpPtr2[2]
                                             + 25 * inpPtr2[3] + 128) >> 8) + 16;
            // moving to next column
            yStartPtr[1] =  (WebRtc_UWord8)((66 * inpPtr[5] + 129 * inpPtr[6]
                                             + 25 * inpPtr[7] + 128) >> 8) + 16;
            yStartPtr2[1] = (WebRtc_UWord8)((66 * inpPtr2[5] + 129 * inpPtr2[6]
                                             + 25 * inpPtr2[7] + 128) >> 8) + 16;
            //U
            uStartPtr[0] = (WebRtc_UWord8)((-38 * inpPtr[1] - 74 * inpPtr[2]
                                            + 112 * inpPtr[3] + 128) >> 8) + 128;
            //V
            vStartPtr[0] = (WebRtc_UWord8)((112 * inpPtr[1] - 94 * inpPtr[2]
                                            - 18 * inpPtr[3] + 128) >> 8) + 128;

            yStartPtr += 2;
            yStartPtr2 += 2;
            uStartPtr++;
            vStartPtr++;
            inpPtr += 8;
            inpPtr2 += 8;
        }
        yStartPtr += width;
        yStartPtr2 += width;
        inpPtr += 4 * width;
        inpPtr2 += 4 * width;
    }
    return (width * height * 3 >> 1);
}


WebRtc_Word32
PadI420BottomRows(WebRtc_UWord8* inputVideoBuffer, WebRtc_UWord32 size,
                  WebRtc_UWord32 width, WebRtc_UWord32 height,
                  WebRtc_Word32 nrRows, WebRtc_UWord32& newLength)
{
    // sanity
    WebRtc_UWord32 length = 3 * (width >> 1) * (height + nrRows);
    if (size < length)
        return -1;

    if (nrRows < 0)
        return -1;

    WebRtc_Word32 colorSize = (width * height) >> 2;
    WebRtc_Word32 padSize = width * nrRows;
    WebRtc_Word32 padSizeColor = (width * nrRows) >> 2;
    WebRtc_Word32 outColorSize = (width *(height + nrRows)) >> 2;
    WebRtc_Word32 j = width * (height + nrRows) + outColorSize;

    WebRtc_Word32 i = width*height + colorSize; // start of Cr
    memmove(&inputVideoBuffer[j], &inputVideoBuffer[i], colorSize);
    memset((&inputVideoBuffer[j])+colorSize,127,padSizeColor);

    i = width*height; // start of Cb
    j = width*(height+nrRows);
    memmove(&inputVideoBuffer[j], &inputVideoBuffer[i], colorSize);
    memset((&inputVideoBuffer[j])+colorSize,127,padSizeColor);

    memset(&inputVideoBuffer[i],0,padSize);

    newLength = length;
    return 0;
}


static WebRtc_UWord32
PadI420Component(const WebRtc_UWord8* inBuf, WebRtc_UWord8* outBuf,
                 const WebRtc_UWord32 fromWidth, const WebRtc_UWord32 fromHeight,
                 const WebRtc_UWord32 padWidth,  const WebRtc_UWord32 padWidthL,
                 const WebRtc_UWord32 padHeight, const WebRtc_UWord32 padHeightT,
                 const WebRtc_UWord8 padValue)
{
    const WebRtc_Word32 toWidth = fromWidth + padWidth;
    const WebRtc_Word32 padWidthR = padWidth - padWidthL;
    const WebRtc_Word32 padHeightB = padHeight - padHeightT;

    // Top border
    memset(outBuf, padValue, toWidth * padHeightT);
    WebRtc_UWord32 outIdx = toWidth * padHeightT;
    WebRtc_UWord32 inIdx = 0;
    for (WebRtc_UWord32 i = 0; i < fromHeight; i++)
    {
        // Left border
        memset(&outBuf[outIdx], padValue, padWidthL);
        outIdx += padWidthL;

        // Copy image
        memcpy(&outBuf[outIdx], &inBuf[inIdx], fromWidth);
        outIdx += fromWidth;
        inIdx += fromWidth;

        // Right border
        memset(&outBuf[outIdx], padValue, padWidthR);
        outIdx += padWidthR;
    }
    // Bottom border
    memset(&outBuf[outIdx], padValue, toWidth * padHeightB);
    outIdx += toWidth * padHeightB;

    return outIdx;
}

WebRtc_Word32
PadI420Frame(const WebRtc_UWord8* inBuffer, WebRtc_UWord8* outBuffer,
             WebRtc_UWord32 fromWidth, WebRtc_UWord32 fromHeight,
             WebRtc_UWord32 toWidth, WebRtc_UWord32 toHeight)
{
    if (toWidth < 1 || fromWidth < 1 || toHeight < 1 || fromHeight < 1)
    {
        return -1;
    }
    if (toWidth == fromWidth && toHeight == fromHeight)
    {
        // nothing to do
        return (3 * toHeight * toWidth) >> 1;
    }

    if (inBuffer == NULL)
    {
        return -1;
    }

    if (outBuffer == NULL)
    {
        return -1;
    }

    if (fromWidth < 0 || fromHeight < 0)
    {
        return -1;
    }

    if (toWidth < 0 || toHeight < 0)
    {
        return -1;
    }

    if (toWidth < fromWidth || toHeight < fromHeight)
    {
        return -1;
    }

    WebRtc_UWord32 padWidth = toWidth - fromWidth;
    WebRtc_UWord32 padHeight = toHeight - fromHeight;
    WebRtc_UWord32 padWidthL = 0;
    WebRtc_UWord32 padHeightT = 0;

    // If one of the padded dimensions is a multiple of 16, we apply the padding
    // in blocks of 16.
    if (padHeight % 16 == 0)
    {
        WebRtc_UWord32 num16blocks = padHeight >> 4;
        padHeightT = ((num16blocks >> 1) << 4); // NOTE: not the same as
                                                //       num16blocks << 3
    }
    else
    {
        padHeightT = padHeight >> 1;
    }

    if (padWidth % 16 == 0)
    {
        WebRtc_UWord32 num16blocks = padWidth >> 4;
        padWidthL = ((num16blocks >> 1) << 4);
    }
    else
    {
        padWidthL = padWidth >> 1;
    }

    // -- I --
    WebRtc_UWord32 inIdx = 0;
    WebRtc_UWord32 outIdx = 0;
    outIdx = PadI420Component(&inBuffer[inIdx], &outBuffer[outIdx], fromWidth,
                              fromHeight, padWidth, padWidthL, padHeight,
                              padHeightT, 0);
    // -- Cr --
    inIdx = fromWidth * fromHeight;
    fromWidth >>= 1;
    fromHeight >>= 1;
    padWidth >>= 1;
    padWidthL >>= 1;
    padHeight >>= 1;
    padHeightT >>= 1;
    outIdx += PadI420Component(&inBuffer[inIdx], &outBuffer[outIdx], fromWidth,
                               fromHeight, padWidth, padWidthL, padHeight,
                               padHeightT, 127);
    // -- Cb --
    inIdx += fromWidth * fromHeight;
    outIdx += PadI420Component(&inBuffer[inIdx], &outBuffer[outIdx], fromWidth,
                               fromHeight, padWidth, padWidthL, padHeight,
                               padHeightT, 127);

    return outIdx;
}

WebRtc_Word32
PadI420Frame(WebRtc_UWord32 size, const WebRtc_UWord8* inBuffer,
             WebRtc_UWord8* outBuffer, bool block16Bit)
{
    if (size < 1)
    {
        return -1;
    }
    WebRtc_Word32 i = 0;
    WebRtc_Word32 m = 0;
    WebRtc_Word32 loop = 0;
    WebRtc_Word32 dropHeightBits = 0; // must be a factor of 4
    WebRtc_Word32 halfToWidth;
    WebRtc_Word32 halfToHeight;
    WebRtc_Word32 halfFromWidth;
    WebRtc_Word32 halfFromHeight;
    WebRtc_Word32 padHeightT;
    WebRtc_Word32 padHeightB;
    WebRtc_Word32 padWidthL;
    WebRtc_Word32 padWidthR;
    WebRtc_Word32 toWidth;
    WebRtc_Word32 toHeight;
    WebRtc_Word32 fromWidth;
    WebRtc_Word32 fromHeight;
    if (block16Bit)
    {
        if (size == 115200) // to 152064
        {
            toWidth = 352;
            toHeight = 288;
            fromWidth = 320;
            fromHeight =240;
            padHeightT = 16;
            padHeightB = 32;
            padWidthL = 16;
            padWidthR = 16;
        } else if (size == 28800)
        {
            fromWidth = 160;
            fromHeight =120;
            dropHeightBits = 8; // drop 8 bits
            toWidth = 176;
            toHeight = 144;
            padHeightT = 16;
            padHeightB = 16;
            padWidthL = 0;
            padWidthR = 16;
        } else
        {
            return -1;
        }
    } else
    {
        return -1;
    }
    halfFromWidth = fromWidth >> 1;
    halfFromHeight = fromHeight >> 1;
    halfToWidth = toWidth >> 1;
    halfToHeight = toHeight >> 1;

    //Ilum
    memset(outBuffer,0,toWidth * padHeightT + padWidthL); // black
    i =  toWidth * padHeightT + padWidthL;
    m = (dropHeightBits >> 1) * fromWidth;
    for (loop = 0; loop < (fromHeight - dropHeightBits); loop++)
    {
        memcpy(&outBuffer[i], &inBuffer[m],fromWidth);
        i += fromWidth;
        m += fromWidth;
        memset(&outBuffer[i],0,padWidthL + padWidthR); // black
        i += padWidthL + padWidthR;
    }
    memset(&outBuffer[i],0,toWidth * padHeightB - padWidthL); // black
    m += (dropHeightBits >> 1) * fromWidth;
    i = toWidth * toHeight; // ilum end

    // Cr
    // black
    memset(&outBuffer[i],127,halfToWidth * (padHeightT >> 1) + (padWidthL >> 1));
    i += halfToWidth * (padHeightT >> 1) + (padWidthL >> 1);

    m += (dropHeightBits >> 2) * halfFromWidth;
    for (loop =0 ; loop < (halfFromHeight - (dropHeightBits >> 1)); loop++)
    {
        memcpy(&outBuffer[i],&inBuffer[m],halfFromWidth);
        m += halfFromWidth;
        i += halfFromWidth;
        memset(&outBuffer[i],127,(padWidthL + padWidthR) >> 1); // black
        i += (padWidthL + padWidthR) >> 1;
    }
    // black
    memset(&outBuffer[i],127,halfToWidth * (padHeightB >> 1) - (padWidthL >> 1) );
    m += (dropHeightBits>>2) * halfFromWidth;
    i = toWidth * toHeight + halfToHeight * halfToWidth; // ilum +Cr

    // Cb
    // black
    memset(&outBuffer[i],127,halfToWidth * (padHeightT >> 1) + (padWidthL >> 2));
    i += halfToWidth * (padHeightT >> 1) + (padWidthL >> 1);

    m += (dropHeightBits >> 2) * halfFromWidth;
    for (loop = 0; loop < (halfFromHeight - (dropHeightBits >> 1)); loop++)
    {
        memcpy(&outBuffer[i],&inBuffer[m],halfFromWidth);
        m += halfFromWidth;
        i += halfFromWidth;
        memset(&outBuffer[i],127,((padWidthL + padWidthR) >> 1)); // black
        i+=((padWidthL + padWidthR) >> 1);
    }
    // black
    memset(&outBuffer[i],127,halfToWidth * (padHeightB >> 1) - (padWidthL >> 1));
    return halfToWidth * toHeight * 3;
}

WebRtc_Word32
ScaleI420UpHalfFrame(WebRtc_UWord32 width, WebRtc_UWord32 height,
                     WebRtc_UWord8* inFrame)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    WebRtc_UWord8* inPtr = inFrame + (width * height / 4 * 3) -1;
    WebRtc_UWord8* outPtr = inFrame + (width * height / 2 * 3) -1;

    for(WebRtc_Word32 i = (width * height / 4 * 3)-1; i > 0; i--)
    {
        *outPtr = *inPtr;
        outPtr--;
        inPtr--;
        *outPtr = ((inPtr[0] + inPtr[1]) / 2);
        outPtr--;
    }
    *outPtr = *inPtr;
    outPtr--;
    *outPtr = *inPtr;

    return 3 * width * height / 2;
}

WebRtc_Word32
ScaleI420DownHalfFrame(WebRtc_UWord32 width, WebRtc_UWord32 height,
                       WebRtc_UWord8* inFrame)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    WebRtc_UWord8* inPtr1 = inFrame;
    WebRtc_UWord8* outPtr = inFrame;
    WebRtc_UWord32 y = 0;
    WebRtc_UWord32 x = 0;
    // ilum
    for (; y < (height); y++)
    {
        for (x = 0; x < (width >> 1); x++)
        {
            WebRtc_Word32 avg = inPtr1[0] + inPtr1[1];
            avg = avg >>1;
            *outPtr= (WebRtc_UWord8)(avg);
            inPtr1 += 2;
            outPtr++;
        }
    }
    inPtr1 = inFrame + (width * height);

    // color
    for (y = 0; y < height; y++)
    {
        // 2 rows
        for (x = 0; x < (width >> 2); x++)
        {
            WebRtc_Word32 avg = inPtr1[0] + inPtr1[1] ;
            *outPtr = (WebRtc_UWord8)(avg >> 1);
            inPtr1 += 2;
            outPtr++;
        }
    }
    return height * (width >> 1) * 3;
}

WebRtc_Word32
ScaleI420FrameQuarter(WebRtc_UWord32 width, WebRtc_UWord32 height,
                      WebRtc_UWord8* inFrame)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    WebRtc_UWord8* inPtr1 = inFrame;
    WebRtc_UWord8* inPtr2 = inFrame + width;
    WebRtc_UWord8* outPtr = inFrame;

    WebRtc_UWord32 y = 0;
    WebRtc_UWord32 x = 0;
    // ilum
    for(; y < (height >> 1); y++)
    {
        // 2 rows
        for(x = 0; x < (width >> 1); x++)
        {
            WebRtc_Word32 avg = inPtr1[0] + inPtr2[0] + inPtr1[1] + inPtr2[1];
            *outPtr= (WebRtc_UWord8)(avg >> 2);
            inPtr1 += 2;
            inPtr2 += 2;
            outPtr++;
        }
        inPtr1 +=width;
        inPtr2 +=width;
    }

    inPtr1 = inFrame + (width * height);
    inPtr2 = inPtr1 + (width>>1);

    // color
    for(y = 0; y < (height>>1); y++)
    {
        // 2 rows
        for(x = 0; x < (width>>2); x++)
        {
            WebRtc_Word32 avg = inPtr1[0] + inPtr2[0] + inPtr1[1] + inPtr2[1];
            *outPtr= (WebRtc_UWord8)(avg >> 2);

            inPtr1 += 2;
            inPtr2 += 2;
            outPtr++;
        }
        inPtr1 += (width >> 1);
        inPtr2 += (width >> 1);
    }
    return height * (width >> 1) * 3;
}

WebRtc_Word32
ScaleI420Up2(WebRtc_UWord32 width, WebRtc_UWord32 height,
             WebRtc_UWord8*& buffer, WebRtc_UWord32 size,
             WebRtc_UWord32 &scaledWidth, WebRtc_UWord32 &scaledHeight)
{
    if (width <= 1 || height <= 1 || (width % 2) != 0 || (height % 2) != 0)
    {
        return -1;
    }

    if (size < (WebRtc_UWord32)(width * height * 3 / 2))
    {
        return -1;
    }

    scaledWidth  = (width << 1);
    scaledHeight = (height << 1);

    // Verify allocated size
    WebRtc_UWord32 scaledBufferSize = CalcBufferSize(kI420, scaledWidth, scaledHeight);
    VerifyAndAllocate(buffer, size, scaledBufferSize);
    WebRtc_UWord8* inPtr1 = buffer + (3 * width * (height >> 1)) - 1;
    WebRtc_UWord8* inPtr2 = buffer + (3 * width * (height >> 1)) - (width >> 1) - 1;
    WebRtc_UWord8* outPtr1 = buffer + (3 * scaledWidth * (scaledHeight >> 1)) - 1;
    WebRtc_UWord8* outPtr2 = buffer + (3 * scaledWidth * (scaledHeight >> 1)) -
                             (scaledWidth >> 1) - 1;

    // Color
    for (WebRtc_Word32 i = 1; i <= 2; i++)
    {
        for (WebRtc_UWord32 y = 0; y < (height >> 1) - 1; y++)
        {
            for (WebRtc_UWord32 x = 0; x < (width >> 1) - 1; x++)
            {
                  *outPtr1 = *inPtr1;
                  *outPtr2 = ((inPtr1[0] + inPtr2[0]) >> 1);
                  inPtr1--;
                  inPtr2--;
                  outPtr1--;
                  outPtr2--;
                  *outPtr1 = ((inPtr1[0] + inPtr1[1]) >> 1);
                  *outPtr2 = ((inPtr1[0] + inPtr1[1] + inPtr2[0] + inPtr2[1]) >> 2);
                  outPtr1--;
                  outPtr2--;
            }
            *outPtr1 = *inPtr1;
            *outPtr2 = ((inPtr1[0] + inPtr2[0]) >> 1);
            outPtr1--;
            outPtr2--;
            *outPtr1 = *inPtr1;
            *outPtr2 = ((inPtr1[0] + inPtr2[0]) >> 1);
            outPtr1--;
            outPtr2--;
            inPtr1--;
            inPtr2--;
            outPtr1 -= width;
            outPtr2 -= width;
        }
        // First row
        for (WebRtc_UWord32 x = 0; x < (width >> 1) - 1; x++)
        {
            *outPtr1 = *inPtr1;
            *outPtr2 = *outPtr1;
            inPtr1--;
            inPtr2--;
            outPtr1--;
            outPtr2--;
            *outPtr1 = ((inPtr1[0] + inPtr1[1]) >> 1);
            *outPtr2 = *outPtr1;
            outPtr1--;
            outPtr2--;
        }
        *outPtr1 = *inPtr1;
        *outPtr2 = *inPtr1;
        outPtr1--;
        outPtr2--;
        *outPtr1 = *inPtr1;
        *outPtr2 = *inPtr1;
        outPtr1--;
        outPtr2--;
        inPtr1--;
        inPtr2--;
        outPtr1 -= width;
        outPtr2 -= width;
    }

    inPtr2 -= (width >> 1);
    outPtr2 -= width;

    // illum
    for (WebRtc_UWord32 y = 0; y < height - 1; y++)
    {
        for (WebRtc_UWord32 x = 0; x < width - 1; x++)
        {
            *outPtr1 = *inPtr1;
            *outPtr2 = ((inPtr1[0] + inPtr2[0]) >> 1);
            inPtr1--;
            inPtr2--;
            outPtr1--;
            outPtr2--;
            *outPtr1 = ((inPtr1[0] + inPtr1[1]) >> 1);
            *outPtr2 = ((inPtr1[0] + inPtr1[1] + inPtr2[0] + inPtr2[1]) >> 2);
            outPtr1--;
            outPtr2--;
        }
        *outPtr1 = *inPtr1;
        *outPtr2 = ((inPtr1[0] + inPtr2[0]) >> 1);
        outPtr1--;
        outPtr2--;
        *outPtr1 = *inPtr1;
        *outPtr2 = ((inPtr1[0] + inPtr2[0]) >> 1);
        outPtr1--;
        outPtr2--;
        inPtr1--;
        inPtr2--;

        outPtr1 -= scaledWidth;
        outPtr2 -= scaledWidth;
    }
    // First row
    for (WebRtc_UWord32 x = 0; x < width - 1; x++)
    {
        *outPtr1 = *inPtr1;
        *outPtr2 = *outPtr1;
        inPtr1--;
        outPtr1--;
        outPtr2--;
        *outPtr1 = ((inPtr1[0] + inPtr1[1]) >> 1);
        *outPtr2 = *outPtr1;
        outPtr1--;
        outPtr2--;
    }
    *outPtr1 = *inPtr1;
    *outPtr2 = *inPtr1;
    outPtr1--;
    outPtr2--;
    *outPtr1 = *inPtr1;
    *outPtr2 = *inPtr1;

    return scaledHeight * (scaledWidth >> 1) * 3;
}

WebRtc_Word32
ScaleI420Up3_2(WebRtc_UWord32 width, WebRtc_UWord32 height,
               WebRtc_UWord8*& buffer, WebRtc_UWord32 size,
               WebRtc_UWord32 &scaledWidth, WebRtc_UWord32 &scaledHeight)
{
    if (width <= 1 || height <= 1)
    {
        return -1;
    }

    if ((width % 2) != 0 || (height % 2) != 0 || ((width >> 1) % 2) != 0 ||
        ((height >> 1) % 2) != 0)
    {
        return -1;
    }

    if (size < (WebRtc_UWord32)(width * height * 3 / 2))
    {
        return -1;
    }

    scaledWidth = 3 * (width >> 1);
    scaledHeight = 3 * (height >> 1);

    // Verify new buffer size
    WebRtc_UWord32 scaledBufferSize = webrtc::CalcBufferSize(kI420, scaledWidth,
                                                             scaledHeight);
    VerifyAndAllocate(buffer, size, scaledBufferSize);

    WebRtc_UWord8* inPtr1 = buffer + (3 * width * (height >> 1)) - 1;
    WebRtc_UWord8* inPtr2 = buffer + (3 * width*(height >> 1)) - (width >> 1) - 1;

    WebRtc_UWord8* outPtr1 = buffer + (3 * scaledWidth * (scaledHeight >> 1)) - 1;
    WebRtc_UWord8* outPtr2 = buffer + (3 * scaledWidth * (scaledHeight >> 1)) -
                             (scaledWidth >> 1) - 1;

    WebRtc_Word32 cy = 0;
    WebRtc_Word32 cx = 0;
    // Color
    for (WebRtc_UWord32 y = 0; y < (height); y++)
    {
        for (WebRtc_UWord32 x = 0; x < (width >> 1); x++)
        {
            *outPtr1 = *inPtr1;
            outPtr1--;
            cy = y % 2;
            cx = x % 2;
            if (cy == 0)
            {
                *outPtr2 = ((inPtr1[0] + inPtr2[0]) >> 1);
            }
            outPtr2--;
            inPtr1--;
            inPtr2--;

            if (cx == 0 && cy == 0)
            {
                *outPtr2 = ((inPtr1[0] + inPtr1[1] + inPtr2[0] + inPtr2[1]) >> 2);
            }
            if (cx == 0)
            {
                *outPtr1 = ((inPtr1[0] + inPtr1[1]) >> 1);
                outPtr1--;
                outPtr2--;
            }
        }
        if (cy == 0)
        {
            outPtr1 -= (scaledWidth >> 1);
            outPtr2 -= (scaledWidth >> 1);
        }
    }
    inPtr2 -= (width >> 1);
    outPtr2 -= (scaledWidth >> 1);

    // illum
    for (WebRtc_UWord32 y = 0; y < height; y++)
    {
        for (WebRtc_UWord32 x = 0; x < width; x++)
        {
            *outPtr1 = *inPtr1;
            outPtr1--;
            cy = y % 2;
            cx = x % 2;
            if (cy == 0)
            {
                *outPtr2 = ((inPtr1[0] + inPtr2[0]) >> 1);
            }
            outPtr2--;
            inPtr1--;
            inPtr2--;
            if (cx == 0 && cy == 0)
            {
                *outPtr2 = ((inPtr1[0] + inPtr1[1] + inPtr2[0] + inPtr2[1]) >> 2);
            }
            if (cx == 0)
            {
                *outPtr1 = ((inPtr1[0] + inPtr1[1]) >> 1);
                outPtr1--;
                outPtr2--;
            }
        }
        if (cy == 0)
        {
            outPtr1 -= scaledWidth;
            outPtr2 -= scaledWidth;
        }
    }

    return scaledHeight * (scaledWidth >> 1) * 3;
}

WebRtc_Word32
ScaleI420Down1_3(WebRtc_UWord32 width, WebRtc_UWord32 height,
                 WebRtc_UWord8*& buffer, WebRtc_UWord32 size,
                 WebRtc_UWord32 &scaledWidth, WebRtc_UWord32 &scaledHeight)
{
    if (width <= 5 || height <= 5)
    {
        return -1;
    }

    if ((width % 2) != 0 || (height % 2) != 0 || (((height / 3) % 2) != 0))
    {
        return -1;
    }

    if (size < (WebRtc_UWord32)(width * height * 3 / 2))
    {
        return -1;
    }

    scaledWidth = width / 3;
    scaledHeight = height / 3;
    WebRtc_Word32 scaledBufferSize = CalcBufferSize(kI420, scaledWidth,
                                                    scaledHeight);
    VerifyAndAllocate(buffer, size, scaledBufferSize);

    WebRtc_UWord8* inPtr1 = buffer;
    WebRtc_UWord8* inPtr2 = buffer + width;
    WebRtc_UWord8* outPtr = buffer;

    WebRtc_Word32 remWidth = width - scaledWidth * 3;

    bool addWidth = false;
    if (scaledWidth % 2)
    {
        scaledWidth++;
        addWidth = true;
    }
    WebRtc_Word32 remWidthCol = (width >> 1) -
                                 WebRtc_Word32((scaledWidth >> 1) * 3.0);

    // illum
    for (WebRtc_UWord32 y = 0; y < height / 3; y++)
    {
        for (WebRtc_UWord32 x = 0; x < width / 3; x++)
        {
            *outPtr = ((inPtr1[0] + inPtr2[0] + inPtr1[1] + inPtr2[1]) >> 2);
            inPtr1 += 3;
            inPtr2 += 3;
            outPtr++;
        }
        if (addWidth)
        {
            *outPtr = ((inPtr1[0] + inPtr2[0]) >> 1);
            outPtr++;
        }
            inPtr1 += (width << 1) + remWidth;
            inPtr2 += (width << 1) + remWidth;
    }
    inPtr1 = buffer + (width * height);
    inPtr2 = inPtr1 + (width >> 1);

    // Color
    for (WebRtc_UWord32 y = 0; y < (scaledHeight >> 1); y++)
    {
        for (WebRtc_UWord32 x = 0; x < (scaledWidth >> 1); x++)
        {
            *outPtr = ((inPtr1[0] + inPtr2[0] + inPtr1[1] + inPtr2[1]) >> 2);
            inPtr1 += 3;
            inPtr2 += 3;
            outPtr++;
        }
        inPtr1 += width + (remWidthCol);
        inPtr2 += width + (remWidthCol);
    }
    inPtr1 = buffer + (width * height) + (width * height >> 2);
    inPtr2 = inPtr1 + (width >> 1);

    for (WebRtc_UWord32 y = 0; y < (scaledHeight >> 1); y++)
    {
        for (WebRtc_UWord32 x = 0; x < (scaledWidth >> 1); x++)
        {
            *outPtr = ((inPtr1[0] + inPtr2[0] + inPtr1[1] + inPtr2[1]) >> 2);
            inPtr1 += 3;
            inPtr2 += 3;
            outPtr++;
        }
        inPtr1 += width + (remWidthCol);
        inPtr2 += width + (remWidthCol);
    }

    return scaledHeight * (scaledWidth >> 1) * 3;
}


WebRtc_Word32
ConvertToI420(VideoType incomingVideoType,
              const WebRtc_UWord8* incomingBuffer,
              WebRtc_UWord32 width,
              WebRtc_UWord32 height,
              WebRtc_UWord8* outgoingBuffer,
              bool interlaced /* =false */,
              VideoRotationMode rotate /* =  kRotateNone  */)

{
    if (width < 1  || height < 1 )
    {
        return -1;
    }
    WebRtc_Word32 outgoingLength = 0;
    WebRtc_Word32 length = 0;
    switch(incomingVideoType)
    {
        case kRGB24:
            outgoingLength = ConvertRGB24ToI420(width, height, incomingBuffer,
                                                outgoingBuffer);
            break;
        case kRGB565:
            outgoingLength = ConvertRGB565ToI420(incomingBuffer, width, height,
                                                 outgoingBuffer);
            break;
#ifdef WEBRTC_MAC
        case kARGB:
            outgoingLength = ConvertARGBMacToI420(width, height, incomingBuffer,
                                                  outgoingBuffer);
            break;
#endif
        case kI420:
            switch(rotate)
            {
              case kRotateNone:
                  length = CalcBufferSize(kI420, width, height);
                  outgoingLength = length;
                  memcpy(outgoingBuffer, incomingBuffer, length);
                  break;
              case kRotateClockwise:
                  outgoingLength = ConvertToI420AndRotateClockwise(
                                    incomingBuffer, width,
                                    height, outgoingBuffer,
                                    height, width, kI420);
                  break;
              case kRotateAntiClockwise:
                  outgoingLength = ConvertToI420AndRotateAntiClockwise(
                                    incomingBuffer, width,
                                    height, outgoingBuffer,
                                    height, width, kI420);
                  break;
              case kRotate180:
                  outgoingLength = ConvertToI420AndMirrorUpDown(incomingBuffer,
                                                                outgoingBuffer,
                                                                width, height,
                                                                kI420);
                  break;
              default:
                  assert(false);
                  break;
            }
            break;
        case kYUY2:
            if (interlaced) {
                outgoingLength = ConvertYUY2ToI420interlaced(incomingBuffer,
                                                             width, height,
                                                             outgoingBuffer,
                                                             width, height);
            } else {
                outgoingLength = ConvertYUY2ToI420(incomingBuffer, width, height,
                                                   outgoingBuffer, width, height);
            }
            break;
        case kUYVY:
            if (interlaced) {
                outgoingLength = ConvertUYVYToI420interlaced(incomingBuffer,
                                                             width, height,
                                                             outgoingBuffer,
                                                             width, height);
            } else {
                outgoingLength = ConvertUYVYToI420(width, height, incomingBuffer,
                                                   outgoingBuffer);
            }
            break;
        case kYV12:
            switch(rotate)
                {
                    case kRotateNone:
                        outgoingLength = ConvertYV12ToI420(incomingBuffer,
                                                           width, height,
                                                           outgoingBuffer);
                        break;
                    case kRotateClockwise:
                        outgoingLength = ConvertToI420AndRotateClockwise(
                                          incomingBuffer, width,
                                          height, outgoingBuffer,
                                          height,width, kYV12);
                        break;
                    case kRotateAntiClockwise:
                        outgoingLength = ConvertToI420AndRotateAntiClockwise(
                                          incomingBuffer,
                                          width, height,
                                          outgoingBuffer,
                                          height, width,
                                          kYV12);
                        break;
                    case kRotate180:
                        outgoingLength = ConvertToI420AndMirrorUpDown(
                                          incomingBuffer,
                                          outgoingBuffer,
                                          width, height, kYV12);
                        break;
                    default:
                        assert(false);
                        break;
                }
            break;
        case kNV12:
            switch(rotate)
            {
                case kRotateNone:
                    outgoingLength = ConvertNV12ToI420(incomingBuffer,
                                                       outgoingBuffer,
                                                       width, height);
                    break;
                case kRotateClockwise:
                    outgoingLength = ConvertNV12ToI420AndRotateClockwise(
                                      incomingBuffer,
                                      outgoingBuffer,
                                      width, height);
                    break;
                case kRotateAntiClockwise:
                    outgoingLength = ConvertNV12ToI420AndRotateAntiClockwise(
                                      incomingBuffer,
                                      outgoingBuffer,
                                      width, height);
                    break;
                case kRotate180:
                    outgoingLength = ConvertNV12ToI420AndRotate180(
                                      incomingBuffer,
                                      outgoingBuffer,
                                      width, height);
                    break;
                default:
                    assert(false);
                    break;
            }
            break;
        case kNV21:
            switch(rotate)
            {
                case kRotateNone:
                    outgoingLength = ConvertNV21ToI420(incomingBuffer,
                                                       outgoingBuffer,
                                                       width, height);
                    break;
                case kRotateClockwise:
                    outgoingLength = ConvertNV21ToI420AndRotateClockwise(
                                      incomingBuffer,
                                      outgoingBuffer,
                                      width, height);
                    break;
                case kRotateAntiClockwise:
                    outgoingLength = ConvertNV21ToI420AndRotateAntiClockwise(
                                        incomingBuffer,
                                        outgoingBuffer,
                                        width, height);
                    break;
                case kRotate180:
                    outgoingLength = ConvertNV21ToI420AndRotate180(
                                      incomingBuffer,
                                      outgoingBuffer,
                                      width, height);
                    break;
                default:
                    assert(false);
                    break;
            }
            break;
        default:
            assert(false);
            break;
    }
    return outgoingLength;
}

WebRtc_Word32 ConvertFromI420(VideoType outgoingVideoType,
                               const WebRtc_UWord8* incomingBuffer,
                               WebRtc_UWord32 width,
                               WebRtc_UWord32 height,
                               WebRtc_UWord8* outgoingBuffer,
                               bool interlaced /* = false */,
                               VideoRotationMode rotate /* = kRotateNone */)

{
    if (width < 1  || height < 1)
    {
        return -1;
    }
    WebRtc_Word32 outgoingLength = 0;
    WebRtc_Word32 length = 0;
    switch(outgoingVideoType)
    {
      case kRGB24:
          outgoingLength = ConvertI420ToRGB24(incomingBuffer, outgoingBuffer,
                                              width, height);
          break;
      case kARGB:
         outgoingLength = ConvertI420ToARGB(incomingBuffer, outgoingBuffer,
                                            width, height, 0);
         break;
      case kARGB4444:
          outgoingLength = ConvertI420ToARGB4444(incomingBuffer, outgoingBuffer,
                                                 width, height, 0);
          break;
      case kARGB1555:
          outgoingLength = ConvertI420ToARGB1555(incomingBuffer, outgoingBuffer,
                                                 width, height,0);
          break;
      case kRGB565:
          outgoingLength = ConvertI420ToRGB565(incomingBuffer, outgoingBuffer,
                                               width, height);
          break;
      case kI420:
          length = CalcBufferSize(kI420, width, height);
          outgoingLength = length;
          memcpy(outgoingBuffer, incomingBuffer, length);
          break;
      case kUYVY:
          outgoingLength = ConvertI420ToUYVY(incomingBuffer, outgoingBuffer,
                                             width, height);
          break;
      case kYUY2:
          outgoingLength = ConvertI420ToYUY2(incomingBuffer, outgoingBuffer,
                                             width, height,0);
          break;
      case kYV12:
        outgoingLength = ConvertI420ToYV12(incomingBuffer, outgoingBuffer,
                                           width, height,0);
        break;
#ifdef WEBRTC_MAC
      case kRGBAMac:
        outgoingLength = ConvertI420ToRGBAMac(incomingBuffer, outgoingBuffer,
                                              width, height,0);
        break;
      case kARGBMac:
        outgoingLength = ConvertI420ToARGBMac(incomingBuffer, outgoingBuffer,
                                              width, height,0);
        break;
#endif
      default:
          assert(false);
          break;
    }
    return outgoingLength;
}

WebRtc_Word32
MirrorI420LeftRight( const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                     WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }

    WebRtc_Word32 indO = 0;
    WebRtc_Word32 indS  = 0;
    WebRtc_UWord32 wind, hind;
    WebRtc_UWord8 tmpVal;
    // will swap two values per iteration
    const WebRtc_UWord32 halfW = width >> 1;
    // Y
    for (wind = 0; wind < halfW; wind++ )
    {
        for (hind = 0; hind < height; hind++ )
        {
            indO = hind * width + wind;
            indS = hind * width + (width - wind - 1); // swapping index
            tmpVal = inFrame[indO];
            outFrame[indO] = inFrame[indS];
            outFrame[indS] = tmpVal;
        } // end for (height)
    } // end for(width)
    const WebRtc_UWord32 lengthW = width >> 2;
    const WebRtc_UWord32 lengthH = height >> 1;
    // V
    WebRtc_Word32 zeroInd = width * height;
    for (wind = 0; wind < lengthW; wind++ )
    {
        for (hind = 0; hind < lengthH; hind++ )
        {
            indO = zeroInd + hind * halfW + wind;
            indS = zeroInd + hind * halfW + (halfW - wind - 1); // swapping index
            tmpVal = inFrame[indO];
            outFrame[indO] = inFrame[indS];
            outFrame[indS] = tmpVal;
        } // end for (height)
    } // end for(width)

    //U
    zeroInd += width * height >> 2;
    for (wind = 0; wind < lengthW; wind++ )
    {
        for (hind = 0; hind < lengthH; hind++ )
        {
            indO = zeroInd + hind * halfW + wind;
            indS = zeroInd + hind * halfW + (halfW - wind - 1); // swapping index
            tmpVal = inFrame[indO];
            outFrame[indO] = inFrame[indS];
            outFrame[indS] = tmpVal;
        } // end for (height)
    } // end for(width)

    return 0;
}

WebRtc_Word32
MirrorI420UpDown( const WebRtc_UWord8* inFrame, WebRtc_UWord8* outFrame,
                  WebRtc_UWord32 width, WebRtc_UWord32 height)
{
    if (width < 1 || height < 1)
    {
        return -1;
    }
    WebRtc_UWord32 indO = 0;
    WebRtc_UWord32 indS  = 0;
    WebRtc_UWord32 wind, hind;
    WebRtc_UWord8 tmpVal;
    WebRtc_UWord32 halfH = height >> 1;
    WebRtc_UWord32 halfW = width >> 1;
    // Y
    for (hind = 0; hind < halfH; hind++ )
    {
        for (wind = 0; wind < width; wind++ )
        {
            indO = hind * width + wind;
            indS = (height - hind - 1) * width + wind;
            tmpVal = inFrame[indO];
            outFrame[indO] = inFrame[indS];
            outFrame[indS] = tmpVal;
        }
    }
    // V
    WebRtc_UWord32 lengthW = width >> 1;
    WebRtc_UWord32 lengthH = height >> 2;
    WebRtc_UWord32 zeroInd = width * height;
    for (hind = 0; hind < lengthH; hind++ )
    {
        for (wind = 0; wind < lengthW; wind++ )
        {
            indO = zeroInd + hind * halfW + wind;
            indS = zeroInd + (halfH - hind - 1) * halfW + wind;
            tmpVal = inFrame[indO];
            outFrame[indO] = inFrame[indS];
            outFrame[indS] = tmpVal;
        }
    }
    // U
    zeroInd += width * height >> 2;
    for (hind = 0; hind < lengthH; hind++ )
    {
        for (wind = 0; wind < lengthW; wind++ )
        {
            indO = zeroInd + hind * halfW + wind;
            indS = zeroInd + (halfH - hind - 1) * halfW + wind;
            tmpVal = inFrame[indO];
            outFrame[indO] = inFrame[indS];
            outFrame[indS] = tmpVal;
        }
    }
    return 0;
}

WebRtc_Word32
ConvertToI420AndMirrorUpDown(const WebRtc_UWord8* srcBuffer, WebRtc_UWord8* dstBuffer,
                             WebRtc_UWord32 srcWidth, WebRtc_UWord32 srcHeight,
                             VideoType colorSpaceIn)
{
    if (colorSpaceIn != kI420 && colorSpaceIn != kYV12)
    {
        return -1;
    }

    const WebRtc_Word32 sourceHeight = srcHeight;
    const WebRtc_Word32 halfHeight = srcHeight >> 1;
    const WebRtc_Word32 sourceWidth = srcWidth;
    const WebRtc_Word32 halfWidth = sourceWidth >> 1;
    WebRtc_UWord8* targetBuffer = dstBuffer;
    const WebRtc_UWord8* sourcePtr = srcBuffer;

    //Mirror Y component
    for (WebRtc_UWord32 newRow = 0; newRow < srcHeight; ++newRow)
    {
        memcpy(targetBuffer,
               &sourcePtr[((srcHeight - newRow) - 1) * sourceWidth], sourceWidth);
        targetBuffer += sourceWidth;
    }

    //Mirror U component
    sourcePtr += sourceHeight * sourceWidth;
    if (colorSpaceIn == kYV12)
    {
        sourcePtr += (sourceHeight * sourceWidth) >> 2;
    }
    for (WebRtc_Word32 newRow = 0; newRow < halfHeight; ++newRow)
    {
        memcpy(targetBuffer, &sourcePtr[(
               (halfHeight - newRow) - 1) * halfWidth], halfWidth);
        targetBuffer += halfWidth;
    }

    //Mirror V component
    if (colorSpaceIn != kYV12)
    {
        sourcePtr += (sourceHeight * sourceWidth) >> 2;
    }
    else
    {
        sourcePtr -= (sourceHeight * sourceWidth) >> 2;
    }
    for(WebRtc_Word32 newRow = 0; newRow < halfHeight; ++newRow)
    {
        memcpy(targetBuffer, &sourcePtr[(
              (halfHeight - newRow) - 1) * halfWidth], halfWidth);
        targetBuffer += halfWidth;
    }
    return 0;
}


WebRtc_Word32
ConvertToI420AndRotateClockwise(const WebRtc_UWord8* srcBuffer,
                                WebRtc_UWord32 srcWidth,
                                WebRtc_UWord32 srcHeight,
                                WebRtc_UWord8* dstBuffer,
                                WebRtc_UWord32 dstWidth,
                                WebRtc_UWord32 dstHeight,
                                VideoType colorSpaceIn)
{
    if (colorSpaceIn != kI420 && colorSpaceIn != kYV12)
    {
         return -1;
     }

    const WebRtc_Word32 targetHeight = dstHeight;
    const WebRtc_Word32 targetWidth = dstWidth;
    const WebRtc_Word32 sourceHeight = srcHeight;
    const WebRtc_Word32 sourceWidth = srcWidth;

    WebRtc_UWord8* targetBuffer = dstBuffer;
    const WebRtc_UWord8* sourcePtr = srcBuffer;

    // Paint the destination buffer black
    memset(dstBuffer,0,dstWidth * dstHeight);
    memset(dstBuffer + dstWidth * dstHeight,127,(dstWidth * dstHeight) / 2);

    const WebRtc_Word32 paddingWidth = (targetWidth - sourceHeight) / 2;
    const WebRtc_Word32 halfPaddingWidth = paddingWidth / 2;
    const WebRtc_Word32 paddingHeight = (targetHeight - sourceWidth) / 2;
    const WebRtc_Word32 halfPaddingHeight = paddingHeight / 2;

    //Rotate Y component
    targetBuffer += paddingHeight * targetWidth;
    for (WebRtc_Word32 newRow = 0; newRow < sourceWidth; ++newRow)
    {
        targetBuffer+=paddingWidth;
        for (WebRtc_Word32 newColumn = sourceHeight - 1;
             newColumn >= 0;--newColumn)
        {
            (*targetBuffer++) = sourcePtr[newColumn * sourceWidth + newRow];
        }
        targetBuffer += paddingWidth;
    }
    targetBuffer += paddingHeight * targetWidth;

    //Rotate U component and store as kI420
    sourcePtr += sourceHeight * sourceWidth;
    if (colorSpaceIn == kYV12)
    {
        sourcePtr += (sourceHeight * sourceWidth) >> 2;
    }
    targetBuffer += halfPaddingHeight * targetWidth / 2;
    for(WebRtc_Word32 newRow = 0;newRow < sourceWidth / 2; ++newRow)
    {
        targetBuffer += halfPaddingWidth;
        for(WebRtc_Word32 newColumn=sourceHeight / 2 - 1;
            newColumn >= 0; --newColumn)
        {
            (*targetBuffer++) = sourcePtr[(newColumn * sourceWidth >> 1) + newRow];
        }
        targetBuffer += halfPaddingWidth;
    }
    targetBuffer += halfPaddingHeight * targetWidth / 2;

    //Rotate V component
    if (colorSpaceIn != kYV12)
    {
        sourcePtr += (sourceHeight * sourceWidth) >> 2;
    } else
    {
        sourcePtr -= (sourceHeight * sourceWidth) >> 2;
    }
    targetBuffer += halfPaddingHeight * targetWidth / 2;
    for (WebRtc_Word32 newRow = 0; newRow < sourceWidth / 2; ++newRow)
    {
        targetBuffer+=halfPaddingWidth;
        for (WebRtc_Word32 newColumn = sourceHeight / 2 - 1;
            newColumn >= 0; --newColumn)
        {
            (*targetBuffer++) = sourcePtr[(newColumn * sourceWidth >> 1)
                                          + newRow];
        }
        targetBuffer += halfPaddingWidth;
    }
    targetBuffer += halfPaddingHeight * targetWidth / 2;
    return 0;
}


WebRtc_Word32
ConvertToI420AndRotateAntiClockwise(const WebRtc_UWord8* srcBuffer,
                                    WebRtc_UWord32 srcWidth,
                                    WebRtc_UWord32 srcHeight,
                                    WebRtc_UWord8* dstBuffer,
                                    WebRtc_UWord32 dstWidth,
                                    WebRtc_UWord32 dstHeight,
                                    VideoType colorSpaceIn)
{
    if (colorSpaceIn != kI420 && colorSpaceIn != kYV12)
    {
        return -1;
    }
    if (dstWidth < srcHeight || dstHeight < srcWidth)
    {
        return -1;
    }
    const WebRtc_Word32 targetHeight = dstHeight;
    const WebRtc_Word32 targetWidth = dstWidth;
    const WebRtc_Word32 sourceHeight = srcHeight;
    const WebRtc_Word32 sourceWidth = srcWidth;

    WebRtc_UWord8* targetBuffer = dstBuffer;

    const WebRtc_UWord8* sourcePtr = srcBuffer;

    // Paint the destination buffer black
    memset(dstBuffer,0,dstWidth * dstHeight);
    memset(dstBuffer + dstWidth * dstHeight,127,(dstWidth * dstHeight) / 2);

    const WebRtc_Word32 paddingWidth = (targetWidth - sourceHeight) / 2;
    const WebRtc_Word32 halfPaddingWidth = paddingWidth / 2;

    const WebRtc_Word32 paddingHeight = (targetHeight - sourceWidth) / 2;
    const WebRtc_Word32 halfPaddingHeight = paddingHeight / 2;

    //Rotate Y component
    targetBuffer += paddingHeight*targetWidth;
    for (WebRtc_Word32 newRow = sourceWidth - 1; newRow >= 0; --newRow)
    {
        targetBuffer+=paddingWidth;
         for (WebRtc_Word32 newColumn = 0; newColumn < sourceHeight; ++newColumn)
         {
             (*targetBuffer++) = sourcePtr[newColumn * sourceWidth + newRow];
         }
         targetBuffer += paddingWidth;
    }
    targetBuffer += paddingHeight * targetWidth;

    //Rotate U component and store as kI420
    sourcePtr += sourceHeight * sourceWidth;
    if (colorSpaceIn == kYV12)
    {
        sourcePtr += (sourceHeight * sourceWidth) >> 2;
    }
    targetBuffer += halfPaddingHeight * targetWidth / 2;
    for (WebRtc_Word32 newRow = sourceWidth / 2 - 1; newRow >= 0;--newRow)
    {
        targetBuffer += halfPaddingWidth;
        for (WebRtc_Word32 newColumn = 0; newColumn < sourceHeight / 2; ++newColumn)
        {
            (*targetBuffer++) = sourcePtr[(newColumn * sourceWidth >> 1)
                                          + newRow];
        }
        targetBuffer += halfPaddingWidth;
    }
    targetBuffer += halfPaddingHeight * targetWidth / 2;

    //Rotate V component
    if (colorSpaceIn != kYV12)
    {
        sourcePtr += (sourceHeight * sourceWidth) >> 2;
    } else
    {
        sourcePtr -= (sourceHeight * sourceWidth) >> 2;
    }
    targetBuffer += halfPaddingHeight * targetWidth / 2;
    for (WebRtc_Word32 newRow = sourceWidth / 2 - 1; newRow >= 0; --newRow)
    {
        targetBuffer += halfPaddingWidth;
        for (WebRtc_Word32 newColumn = 0;
            newColumn < sourceHeight / 2; ++newColumn)
        {
            (*targetBuffer++) = sourcePtr[(newColumn * sourceWidth >> 1)
                                          + newRow];
        }
        targetBuffer += halfPaddingWidth;
    }
    targetBuffer += halfPaddingHeight * targetWidth / 2;
    return 0;
}


inline
WebRtc_UWord8 Clip(WebRtc_Word32 val)
{
    if (val < 0)
    {
        return (WebRtc_UWord8)0;
    } else if (val > 255)
    {
        return (WebRtc_UWord8)255;
    }
    return (WebRtc_UWord8)val;
}

WebRtc_Word32
VerifyAndAllocate(WebRtc_UWord8*& buffer, WebRtc_UWord32 currentSize,
                  WebRtc_UWord32 newSize)
{
    if (newSize > currentSize)
    {
        // make sure that our buffer is big enough
        WebRtc_UWord8* newBuffer = new WebRtc_UWord8[newSize];
        if (buffer)
        {
            // copy old data
            memcpy(newBuffer, buffer, currentSize);
            delete [] buffer;
        }
        buffer = newBuffer;
        return newSize;
    }
    return currentSize;
}

#ifdef SCALEOPT
//memcpy_16 assumes that width is an integer multiple of 16!
void *memcpy_16(void * dest, const void * src, size_t n)
{
    _asm
    {
        mov eax, dword ptr [src]
        mov ebx, dword ptr [dest]
        mov ecx, dword ptr [n]

    loop0:

        movdqu    xmm0, XMMWORD PTR [eax]
        movdqu    XMMWORD PTR [ebx], xmm0
        add       eax, 16
        add       ebx, 16
        sub       ecx, 16
        jg        loop0
    }
}

//memcpy_8 assumes that width is an integer multiple of 8!
void *memcpy_8(void * dest, const void * src, size_t n)
{
    _asm
    {
        mov eax, dword ptr [src]
        mov ebx, dword ptr [dest]
        mov ecx, dword ptr [n]

    loop0:

        movq    mm0, QWORD PTR [eax]
        movq    QWORD PTR [ebx], mm0
        add       eax, 8
        add       ebx, 8
        sub       ecx, 8
        jg        loop0

    emms
    }

}

#endif

}
