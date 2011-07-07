/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string.h>
#include <stdio.h>
#include <time.h>

#include "interpolator.h"
#include "vplib.h"
#include "test_util.h"

using namespace webrtc;

int interpolation_test(CmdArgs& args)
{
    // Read input file, interpolate first frame according to requested method
    // for now only YUV input and output

    FILE* sourceFile;
    FILE* outputFile;

    std::string outname = args.outputFile;
    if (outname == "")
    {
        outname = "InterTest_out.yuv";
    }
    if (args.width < 1 || args.height < 1 ||
        args.dstWidth < 1 || args.dstHeight < 1)
    {
        printf("Error in input dimensions\n" );
        return -1;
    }

    WebRtc_Word32 ret;

    // create interpolator
    webrtc::interpolator* inter = new webrtc::interpolator();
    ret = inter->Set(args.width, args.height,
                     args.dstWidth, args.dstHeight,
                     kI420, kI420,
                     (webrtc::interpolatorType) args.intMethod);
    if (ret != 0)
    {
        printf("Error in set interpolator = %d\n", ret);
        delete inter;
        return ret;
    }

    // read frame into buffer / create destination buffer
    if ((outputFile = fopen(outname.c_str(), "wb")) == NULL)
    {
        printf("Cannot write file %s.\n", outname.c_str());
        exit(1);
    }

    std::string inname = args.inputFile;
    if ((sourceFile = fopen(inname.c_str(), "rb")) == NULL)
    {
        printf("Cannot read file %s.\n", inname.c_str());
        exit(1);
    }

    WebRtc_UWord32 inRequiredSize = args.width * args.height * 3 >> 1;
    WebRtc_UWord32 outRequiredSize = args.dstWidth * args.dstHeight * 3 >> 1;
    WebRtc_UWord8* inputBuffer = new WebRtc_UWord8[inRequiredSize];
    WebRtc_UWord8* outputBuffer = new WebRtc_UWord8[outRequiredSize];
    WebRtc_UWord32 outputBufferSz = outRequiredSize;

    clock_t startClock, TotalClock;
    TotalClock = 0;
    int frameCnt = 0;

    // running through entire sequence
    while (feof(sourceFile) == 0)
    {
        if (inRequiredSize != fread(inputBuffer, 1, inRequiredSize, sourceFile))
        {
            break;
        }

        startClock = clock();
        ret = inter->Interpolate(inputBuffer, outputBuffer, outputBufferSz);
        TotalClock += clock() - startClock;

        if (ret == args.dstHeight)
        {
            fwrite(outputBuffer, 1, outRequiredSize, outputFile);
            ret = 0;  // signaling OK to main tester
        }
        else
        {
            printf("frame #%d: Interpolation Error, ret = %d\n", frameCnt, ret);
        }
        frameCnt++;
        printf(".");
    }

    printf("\nProcessed %d frames\n", frameCnt);
    if (frameCnt)
    {
        printf("\nAvg. Time per frame[mS]: %.2lf\n",
              (1000.0 * static_cast<double>(TotalClock + 0.0)
               /CLOCKS_PER_SEC)/frameCnt);
    }

    delete [] outputBuffer;
    outputBuffer = NULL;
    outputBufferSz = 0;

    // running some sanity checks
    ret = inter->Set(0, 10, 20, 30, kI420, kI420,
                    (webrtc::interpolatorType) args.intMethod);
    TEST(ret < 0);
    ret = inter->Set(1, 10, 0, 30, kI420, kI420,
                       (webrtc::interpolatorType) args.intMethod);
    TEST(ret < 0);

    rewind(sourceFile);
    if (inRequiredSize != fread(inputBuffer, 1, inRequiredSize, sourceFile))
    {
        printf("Error reading input file\n");
        return -1;
    }

    ret = inter->Set(20, 10, 20, 30, kI420, kI420,
                     (webrtc::interpolatorType) 2);
    TEST(ret < 0);
    ret = inter->Interpolate(inputBuffer, outputBuffer, outputBufferSz);
    TEST(ret < 0);

    // computing required size for user-defined settings
    WebRtc_UWord32 dstRequiredSize = args.dstWidth * args.dstHeight * 3 >> 1;
    // null output buffer - should allocate required size
    ret = inter->Set(args.width, args.height,
                     args.dstWidth, args.dstHeight,
                     kI420, kI420,
                     (webrtc::interpolatorType) args.intMethod);
    TEST(ret == 0);
    ret = inter->Interpolate(inputBuffer, outputBuffer, outputBufferSz);
    TEST(ret == args.dstHeight);
    TEST(outputBufferSz == dstRequiredSize);
    if (outputBuffer)
    {
        delete [] outputBuffer;
    }

    // output buffer too small (should reallocate)
    outputBufferSz = dstRequiredSize / 2;
    outputBuffer = new WebRtc_UWord8[outputBufferSz];
    ret = inter->Interpolate(inputBuffer, outputBuffer, outputBufferSz);
    TEST(ret == args.dstHeight);
    TEST(outputBufferSz == dstRequiredSize);
    if (outputBuffer)
    {
        delete [] outputBuffer;
    }

    // output buffer too large (should maintain existing buffer size)
    outputBufferSz = dstRequiredSize + 20;
    outputBuffer = new WebRtc_UWord8[outputBufferSz];
    ret = inter->Interpolate(inputBuffer, outputBuffer, outputBufferSz);
    TEST(ret == args.dstHeight);
    TEST(outputBufferSz == dstRequiredSize + 20);
    if (outputBuffer)
    {
        delete [] outputBuffer;
    }

    fclose(sourceFile);
    fclose(outputFile);

    delete inter;
    delete [] inputBuffer;

    return 0;
}
