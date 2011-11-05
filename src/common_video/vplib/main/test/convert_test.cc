/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Test application for color space conversion functions

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include <cmath>
#include <time.h>

#include "test_util.h"
#include "vplib.h"

using namespace webrtc;


// Optimization testing
//#define SCALEOPT //For Windows currently, June 2010

WebRtc_Word32
ImagePSNRfromBuffer(WebRtc_UWord8 *refBufName, WebRtc_UWord8 *testBufName,
                    WebRtc_Word32 width, WebRtc_Word32 height,
                    VideoType vType, double *YPSNRptr);
void TestRetVal(int testVal, int refVal );

int convert_test(CmdArgs& args)
{
    // reading YUV frame - testing on the first frame of the foreman sequence
    
    //SET UP
    int j = 0;
    int retVal;
    std::string outname = args.outputFile;
    if (outname == "")
    {
        outname = "conversionTest_out.yuv";
    }
    std::string inname;
    inname = args.inputFile;
    FILE* sourceFile;
    FILE* outputFile;
    FILE* logFile;
    WebRtc_UWord32 width = args.width;
    WebRtc_UWord32 height = args.height;
    WebRtc_UWord32 lengthSourceFrame = width*height*3/2;
    double psnr = 0;
    if ((sourceFile = fopen(inname.c_str(), "rb")) == NULL)
    {
        printf("Cannot read file %s.\n", inname.c_str());
        return 1;
    }
    if ((outputFile = fopen(outname.c_str(), "wb")) == NULL)
    {
        printf("Cannot write file %s.\n", outname.c_str());
        return 1;
    }
    if ((logFile = fopen("../log.txt", "a")) == NULL)
    {
        printf("Cannot write file ../log.txt.\n");
        return 1;
    }

    // reading first frame of Foreman sequence
    WebRtc_UWord8* origBuffer = new WebRtc_UWord8[width * height*3/2];
    if (fread(origBuffer, 1, lengthSourceFrame, sourceFile) !=
        lengthSourceFrame)
    {
        printf("Error reading file %s\n", inname.c_str());
        return 1;
    }

    // START TEST
    printf("\nTEST #%d I420 <-> RGB24\n", j);

    WebRtc_UWord8* resRGBBuffer2  = new WebRtc_UWord8[width*height*3];
    WebRtc_UWord8* resI420Buffer = new WebRtc_UWord8[width*height*3/2];
    retVal = ConvertFromI420(kRGB24, origBuffer, width, height,resRGBBuffer2);
    TestRetVal(retVal, width*height*3);
    clock_t tticks = clock();
    for (int tt = 0; tt < 1000; tt++)
    {
        retVal = ConvertToI420(kRGB24, resRGBBuffer2, width, height,
                               resI420Buffer);
    }
    tticks = clock() - tticks;
    printf("RGB24->I420 Time(1000): %d\n", (int)tticks);

    TestRetVal(retVal, width*height*3/2);
    fwrite(resI420Buffer, lengthSourceFrame, 1, outputFile);
    ImagePSNRfromBuffer(origBuffer, resI420Buffer, width, height, kI420, &psnr);
    printf("Conversion between type #%d and type #%d, PSNR = %f\n", kI420,
           kRGB24, psnr);
    j++;
    delete [] resRGBBuffer2;


    printf("\nTEST #%d I420 <-> UYVY\n", j);
    WebRtc_UWord8* outUYVYBuffer = new WebRtc_UWord8[width*height*2];

    clock_t ticks = clock();
    for (int t = 0; t < 100; t++)
    {
        retVal = ConvertFromI420(kUYVY, origBuffer, width,
                                 height, outUYVYBuffer);
    }
    ticks = clock() - ticks;
#ifndef SCALEOPT
    fprintf(logFile, "\nConvertI420ToUYVY, before opt: %d\n", (int)ticks);
#else
    fprintf(logFile, "\nConvertI420ToUYVY, after opt: %d\n", (int)ticks);
#endif

    TestRetVal(retVal, width*height*2);
    retVal = ConvertToI420(kUYVY, outUYVYBuffer, width, height, resI420Buffer);
    TestRetVal(retVal, width*height*3/2);

    ImagePSNRfromBuffer(origBuffer, resI420Buffer, width, height, kI420, &psnr);
    printf("Conversion between type #%d and type #%d, PSNR = %f\n",
           kI420, kUYVY, psnr);
    j++;
    delete [] outUYVYBuffer;


    printf("\nTEST #%d I420 <-> I420 \n", j);

    WebRtc_UWord8* outI420Buffer = new WebRtc_UWord8[width*height*2];
    retVal = ConvertToI420(kI420, origBuffer, width, height, outI420Buffer);
    TestRetVal(retVal, width*height*3/2);
    retVal = ConvertToI420(kI420 ,outI420Buffer, width, height, resI420Buffer);
    TestRetVal(retVal, width*height*3/2);
    fwrite(resI420Buffer, lengthSourceFrame, 1, outputFile);
    ImagePSNRfromBuffer(origBuffer, resI420Buffer, width, height,
                        kI420, &psnr);
    printf("Conversion between type #%d and type #%d, PSNR = %f\n",
           kI420, kUYVY, psnr);
    j++;
    delete [] outI420Buffer;

    printf("\nTEST #%d I420 <-> YV12\n", j);
    outI420Buffer = new WebRtc_UWord8[width*height*3/2]; // assuming DIFF = 0

    ticks = clock();
    for (int t = 0; t < 1000; t++)
    {
        retVal = ConvertFromI420(kYV12, origBuffer, width, height, outI420Buffer);
    }
    ticks = clock() - ticks;
#ifndef SCALEOPT
    fprintf(logFile, "\nConvertI420ToYV12, before opt: %d\n", (int)ticks);
#else
    fprintf(logFile, "\nConvertI420ToYV12, after opt: %d\n", (int)ticks);
#endif
    TestRetVal(retVal, width*height*3/2);
    retVal = webrtc::ConvertYV12ToI420(outI420Buffer, width, height,
                                       resI420Buffer);
    TestRetVal(retVal, width*height*3/2);

    fwrite(resI420Buffer, lengthSourceFrame, 1, outputFile);

    ImagePSNRfromBuffer(origBuffer, resI420Buffer, width, height, kI420, &psnr);
    printf("Conversion between type #%d and type #%d, PSNR = %f\n", kI420,
           kYV12, psnr);
    j++;
    delete [] outI420Buffer;
    delete [] resI420Buffer;


    printf("\nTEST #%d I420<-> RGB565\n", j);
    WebRtc_UWord8* res2ByteBuffer = new WebRtc_UWord8[width*height*2];
    resI420Buffer = new WebRtc_UWord8[width * height * 3 / 2];
    retVal = ConvertFromI420(kRGB565, origBuffer, width, height, res2ByteBuffer);
    TestRetVal(retVal, width*height*2);
    retVal = ConvertRGB565ToI420(res2ByteBuffer, width, height, resI420Buffer);
    TestRetVal(retVal, width*height*3/2);
    fwrite(resI420Buffer, lengthSourceFrame, 1, outputFile);
    ImagePSNRfromBuffer(origBuffer, resI420Buffer, width, height, kI420, &psnr);
    printf("Note: Frame was compressed!\n");
    printf("Conversion between type #%d and type #%d, PSNR = %f\n", kI420,
           kRGB565, psnr);
 
    delete [] res2ByteBuffer;
    j++;

    printf("\nTEST #%d I420 <-> YUY2\n", j);
    WebRtc_UWord8* outYUY2Buffer = new WebRtc_UWord8[width*height*2];

    ticks = clock();
    for (int t = 0; t < 1000; t++)
    {
        retVal = ConvertI420ToYUY2(origBuffer, outYUY2Buffer, width, height,0);
    }
    ticks = clock() - ticks;
#ifndef SCALEOPT
    fprintf(logFile, "\nConvertI420ToYUY2, before opt: %d\n", (int)ticks);
#else
    fprintf(logFile, "\nConvertI420ToYUY2, after opt: %d\n", (int)ticks);
#endif
    TestRetVal(retVal, width*height*2);
    ticks = clock();
    for (int t = 0; t < 1000; t++)
    {
        retVal = ConvertToI420(kYUY2, outYUY2Buffer, width, height,
                               resI420Buffer);
    }
    ticks = clock() - ticks;
#ifndef SCALEOPT
    fprintf(logFile, "\nConvertYUY2ToI420, before opt: %d\n", (int)ticks);
#else
    fprintf(logFile, "\nConvertYUY2ToI420, after opt: %d\n", (int)ticks);
#endif
    TestRetVal(retVal, width*height*3/2);
    fwrite(resI420Buffer, lengthSourceFrame, 1, outputFile);
    ImagePSNRfromBuffer(origBuffer, resI420Buffer, width, height, kI420, &psnr);
    printf("Conversion between type #%d and type #%d,PSNR = %f\n", kI420,
           kYUY2, psnr);
  
    delete [] outYUY2Buffer;
    j++;
 
    printf("\nTEST #%d I420 <-> UYVY\n", j);

    outUYVYBuffer = new WebRtc_UWord8[width*height*2]; // assuming DIFF = 0
    WebRtc_UWord8* resYUVBuffer = new WebRtc_UWord8[width*height*2];
    retVal = ConvertFromI420(kUYVY, origBuffer, width, height, outUYVYBuffer);
    TestRetVal(retVal, width*height*2);
    retVal = ConvertToI420(kUYVY, outUYVYBuffer, width, height, resYUVBuffer);
    TestRetVal(retVal, width*height*3/2);
    fwrite(resYUVBuffer, lengthSourceFrame, 1, outputFile);
    ImagePSNRfromBuffer(origBuffer, resYUVBuffer, width, height, kI420, &psnr);
    printf("Conversion between type #%d and type #%d,PSNR = %f\n", kI420,
           kUYVY, psnr);

    delete [] outUYVYBuffer;
    delete [] resYUVBuffer;

    j++;

     /*******************************************************************
     * THE FOLLOWING FUNCTIONS HAVE NO INVERSE, BUT ARE PART OF THE TEST
     * IN ORDER TO VERIFY THAT THEY DO NOT CRASH
     *******************************************************************/
     printf("\n\n Running functions with no inverse...\n");

     //printf("TEST #%d I420 -> ARGB4444 \n", j);
     res2ByteBuffer  = new WebRtc_UWord8[width*height*2];
     ConvertI420ToARGB4444(origBuffer, res2ByteBuffer, width, height, 0);
     delete [] res2ByteBuffer;

     // YUY2 conversions
     //printf("TEST #%d I420 -> YUY2 \n", j);
     WebRtc_UWord8* sourceYUY2 = new WebRtc_UWord8[width*height*2];
     ConvertI420ToYUY2(origBuffer, sourceYUY2, width, height, 0);

     delete [] sourceYUY2;

     //UYVY conversions
     WebRtc_UWord8* sourceUYVY = new WebRtc_UWord8[width*height*2];
     ConvertI420ToUYVY(origBuffer, sourceUYVY, width, height, 0);

     //printf("TEST I420-> ARGB444\n");
     res2ByteBuffer  = new WebRtc_UWord8[(width+10)*height*2];
     retVal = webrtc::ConvertI420ToARGB4444(origBuffer, res2ByteBuffer,
                                            width, height, width + 10);
     TestRetVal(retVal, (width+10)*height*2);
     delete [] res2ByteBuffer;

      //printf("TEST I420-> ARGB1555\n");
     res2ByteBuffer  = new WebRtc_UWord8[(width+10)*height*2];
     retVal = ConvertI420ToARGB1555(origBuffer, res2ByteBuffer, width,
                                    height, width + 10);
     TestRetVal(retVal, (width+10)*height*2);
     delete [] res2ByteBuffer;

     //printf("TEST NV12 - > I420\n");
     // using original I420 sequence - > just to verify it doesn't crash
     ConvertNV12ToI420(origBuffer,resI420Buffer, width, height);
     //printf("TEST NV12 - > I420 and Rotate 180\n");
     ConvertNV12ToI420AndRotate180(origBuffer, resI420Buffer, width, height);
     //printf("TEST NV12 - > I420 and Rotate anti Clockwise\n");
     ConvertNV12ToI420AndRotateAntiClockwise(origBuffer, resI420Buffer,
                                             width, height);
     //printf("TEST NV12 - > I420 and Rotate Clockwise\n");
     ConvertNV12ToI420AndRotateClockwise(origBuffer, resI420Buffer,
                                         width, height);
     //printf("TEST NV12 -> RGB565 \n");
     res2ByteBuffer  = new WebRtc_UWord8[(width+10)*height*2];
     ConvertNV12ToRGB565(origBuffer, res2ByteBuffer, width, height);
     delete [] res2ByteBuffer;

     //printf("TEST I420 - > RGBAIPhone");
     WebRtc_UWord8* resBuffer = new WebRtc_UWord8[(width + 10) * height * 4];
     ConvertI420ToRGBAIPhone(origBuffer, resBuffer, width, height, width + 10);
     delete [] resBuffer;

     //printf("TEST #%d I420 <-> ARGB_Mac", j);
     WebRtc_UWord8* outARGBBuffer = new WebRtc_UWord8[width * height * 4];
     retVal = ConvertI420ToARGBMac(origBuffer,outARGBBuffer, width, height, 0);
     TestRetVal(retVal, width * height * 4);
     delete [] outARGBBuffer;




    //closing
    fclose(sourceFile);
    fclose(outputFile);
    fclose(logFile);
    delete [] origBuffer;
    delete [] resI420Buffer;
    std::cout << "\n**  View output file **\n";
    std::cout << "Press enter to  quit test...";
    std::string str;
    std::getline(std::cin, str);
    
    return 0;
}

WebRtc_Word32
ImagePSNRfromBuffer(WebRtc_UWord8 *refBufName, WebRtc_UWord8 *testBufName,
                    WebRtc_Word32 width, WebRtc_Word32 height,
                    VideoType vType, double *YPSNRptr)
{
    // currently assumes I420
    if (vType != kI420)
    {
        return -1;
    }
    double mse = 0.0;
    double mseLogSum = 0.0;
   
    WebRtc_UWord8 *ref = refBufName;
    WebRtc_UWord8 * test = testBufName;
    // comparing only 1 frame
    mse = 0.0;

    // calculate Y sum-square-difference
    for( int k = 0; k < width * height; k++ )
    {
          mse += (test[k] - ref[k]) * (test[k] - ref[k]);
    }

    // divide by number of pixels
    mse /= (double) (width * height);

    if (mse == 0)
    {
        *YPSNRptr = 48;
        return 0;
    }
    // accumulate for total average
    mseLogSum += std::log10( mse );
   
    *YPSNRptr = 20.0 * std::log10(255.0) - 10.0 * mseLogSum;

     return 0;
}

void TestRetVal(int testVal, int refVal )
{
    if (testVal != refVal)
    {
        printf("return value = %d, desired value = %d\n", testVal, refVal);
    }
}
