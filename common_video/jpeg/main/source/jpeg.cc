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
 * jpeg.cc
 */


#include <stdio.h>
#include <string.h>

#include "vplib.h"
#include "jpeg.h"
#include "data_manager.h"
#if defined(WIN32)
 #include <basetsd.h>
#endif
#ifdef WEBRTC_ANDROID
extern "C" {
#endif
#include "jpeglib.h"
#ifdef WEBRTC_ANDROID
}
#endif
#include <setjmp.h>


namespace webrtc
{

// Error handler
struct myErrorMgr {

    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};
typedef struct myErrorMgr * myErrorPtr;

METHODDEF(void)
MyErrorExit (j_common_ptr cinfo)
{
    myErrorPtr myerr = (myErrorPtr) cinfo->err;

    // Return control to the setjmp point
    longjmp(myerr->setjmp_buffer, 1);
}

JpegEncoder::JpegEncoder():
_width(0),
_height(0)
{
    _cinfo = new jpeg_compress_struct;
    strcpy(_fileName, "Snapshot.jpg");
}

JpegEncoder::~JpegEncoder()
{
    delete _cinfo;
    _cinfo = NULL;
}


WebRtc_Word32
JpegEncoder::SetFileName(const WebRtc_Word8* fileName)
{
    if (!fileName)
    {
        return -1;
    }

    if (fileName)
    {
        strncpy(_fileName, fileName, 256);
    }
    return 0;
}


WebRtc_Word32
JpegEncoder::Encode(const WebRtc_UWord8* imageBuffer,
                    const WebRtc_UWord32 imageBufferSize,
                    const WebRtc_UWord32 width,
                    const WebRtc_UWord32 height)
{
    if ((imageBuffer == NULL) || (imageBufferSize == 0))
    {
        return -1;
    }
    if (width < 1 || height < 1)
    {
        return -1;
    }

    FILE* outFile = NULL;

    _width = width;
    _height = height;

    // Set error handler
    myErrorMgr      jerr;
    _cinfo->err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = MyErrorExit;
    // Establish the setjmp return context
    if (setjmp(jerr.setjmp_buffer))
    {
        // If we get here, the JPEG code has signaled an error.
        jpeg_destroy_compress(_cinfo);
        if (outFile != NULL)
        {
            fclose(outFile);
        }
        return -1;
    }

    if ((outFile = fopen(_fileName, "wb")) == NULL)
    {
        fprintf(stderr, "can't open %s\n", _fileName);
        return -2;
    }
    // Create a compression object
    jpeg_create_compress(_cinfo);

    // Setting destination file
    jpeg_stdio_dest(_cinfo, outFile);

    WebRtc_Word32 ret = 0;

    // Height of image buffer should be  a multiple of 16
    if (_height % 16 == 0)
    {
        ret = Encode(imageBuffer, imageBufferSize);
    }
    else
    {
         WebRtc_UWord32 height16 = ((_height + 15) - 16) & - 16;
         height16 = (height16 < _height) ? height16 + 16 : height16;

         // Copy image to an adequate size buffer
         WebRtc_UWord32 requiredSize = height16 * _width * 3 >> 1;
         WebRtc_UWord8* origImagePtr = new WebRtc_UWord8[requiredSize];
         if (origImagePtr == NULL)
         {
             return -1;
         }
         memset(origImagePtr, 0, requiredSize);
         memcpy(origImagePtr, imageBuffer, imageBufferSize);

         ret = Encode(origImagePtr, requiredSize);

         // delete allocated buffer
         delete [] origImagePtr;
         origImagePtr = NULL;
    }


    fclose(outFile);

    return ret;
}

WebRtc_Word32
JpegEncoder::Encode(const WebRtc_UWord8* imageBuffer,
                    const WebRtc_UWord32 imageBufferSize)
{
    // Set parameters for compression
    _cinfo->in_color_space = JCS_YCbCr;
    jpeg_set_defaults(_cinfo);

    _cinfo->image_width = _width;
    _cinfo->image_height = _height;
    _cinfo->input_components = 3;

    _cinfo->comp_info[0].h_samp_factor = 2;   // Y
    _cinfo->comp_info[0].v_samp_factor = 2;
    _cinfo->comp_info[1].h_samp_factor = 1;   // U
    _cinfo->comp_info[1].v_samp_factor = 1;
    _cinfo->comp_info[2].h_samp_factor = 1;   // V
    _cinfo->comp_info[2].v_samp_factor = 1;
    _cinfo->raw_data_in = TRUE;

    jpeg_start_compress(_cinfo, TRUE);

    JSAMPROW y[16],u[8],v[8];
    JSAMPARRAY data[3];

    data[0] = y;
    data[1] = u;
    data[2] = v;

    WebRtc_UWord32 i, j;

    for (j = 0; j < _height; j += 16)
    {
        for (i = 0; i < 16; i++)
        {
            y[i] = (JSAMPLE*) imageBuffer + _width * (i + j);

            if (i % 2 == 0)
            {
                u[i / 2] = (JSAMPLE*) imageBuffer + _width * _height +
                            _width / 2 * ((i + j) / 2);
                v[i / 2] = (JSAMPLE*) imageBuffer + _width * _height +
                            _width * _height / 4 + _width / 2 * ((i + j) / 2);
            }
        }
            jpeg_write_raw_data(_cinfo, data, 16);
    }

    jpeg_finish_compress(_cinfo);
    jpeg_destroy_compress(_cinfo);

    return 0;
}


JpegDecoder::JpegDecoder()
{
    _cinfo = new jpeg_decompress_struct;
}

JpegDecoder::~JpegDecoder()
{
    delete _cinfo;
    _cinfo = NULL;
}

WebRtc_Word32
JpegDecoder::Decode(const WebRtc_UWord8* encodedBuffer,
                    const WebRtc_UWord32 encodedBufferSize,
                    WebRtc_UWord8*& decodedBuffer,
                    WebRtc_UWord32& width,
                    WebRtc_UWord32& height)
{
    // Set  error handler
    myErrorMgr    jerr;
    _cinfo->err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = MyErrorExit;

    // Establish the setjmp return context
    if (setjmp(jerr.setjmp_buffer))
    {
        if (_cinfo->is_decompressor)
        {
            jpeg_destroy_decompress(_cinfo);
        }
        return -1;
    }

    _cinfo->out_color_space = JCS_YCbCr;

    // Create decompression object
    jpeg_create_decompress(_cinfo);

    // Specify data source
    jpegSetSrcBuffer(_cinfo, (JOCTET*) encodedBuffer, encodedBufferSize);

    // Read header data
    jpeg_read_header(_cinfo, TRUE);

    _cinfo->raw_data_out = TRUE;
    jpeg_start_decompress(_cinfo);

    // Check header
    if (_cinfo->num_components == 4)
    {
        return -2; // not supported
    }
    if (_cinfo->progressive_mode == 1)
    {
        return -2; // not supported
    }

    height = _cinfo->image_height;
    width = _cinfo->image_width;

    // Making sure width and height are even
    if (height % 2)
        height++;
    if (width % 2)
         width++;

    WebRtc_UWord32 height16 = ((height + 15) - 16) & - 16;
    height16 = (height16 < height) ? height16 + 16 : height16;

    // allocate buffer to output
    if (decodedBuffer != NULL)
    {
        delete [] decodedBuffer;
        decodedBuffer = NULL;
    }
    decodedBuffer = new WebRtc_UWord8[width * height16 * 3 >> 1];
    if (decodedBuffer == NULL)
    {
        return -1;
    }

    JSAMPROW y[16],u[8],v[8];
    JSAMPARRAY data[3];
    data[0] = y;
    data[1] = u;
    data[2] = v;

    WebRtc_UWord32 hInd, i;
    WebRtc_UWord32 numScanLines = 16;
    WebRtc_UWord32 numLinesProcessed = 0;
    while(_cinfo->output_scanline < _cinfo->output_height)
    {
        hInd = _cinfo->output_scanline;
        for (i = 0; i < numScanLines; i++)
        {
            y[i] = decodedBuffer + width * (i + hInd);

            if (i % 2 == 0)
            {
                 u[i / 2] = decodedBuffer + width * height +
                            width / 2 * ((i + hInd) / 2);
                 v[i / 2] = decodedBuffer + width * height +
                            width * height / 4 + width / 2 * ((i + hInd) / 2);
            }
        }
        // Processes exactly one iMCU row per call
        numLinesProcessed = jpeg_read_raw_data(_cinfo, data, numScanLines);
        // Error in read
        if (numLinesProcessed == 0)
        {
            delete [] decodedBuffer;
            jpeg_abort((j_common_ptr)_cinfo);
            return -1;
        }
    }

    jpeg_finish_decompress(_cinfo);
    jpeg_destroy_decompress(_cinfo);
    return 0;
}


}
