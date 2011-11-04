/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <cassert>
#include <iostream>
#include <string>

#include "vplib.h"

#include <cstring>

using namespace webrtc;

#define TEST_STR "Test Scale."
#define TEST_PASSED() std::cerr << TEST_STR << " : [OK]" << std::endl
#define PRINT_LINE std::cout << "------------------------------------------" << std::endl;

void PrintFrame(WebRtc_UWord8* ptrFrame, WebRtc_Word32 width, WebRtc_Word32 height)
{
    WebRtc_Word32 k = 0;
    for (WebRtc_Word32 i = 0; i < height; i++)
    {
        for (WebRtc_Word32 j = 0; j < width; j++)
        {
            std::cout << (WebRtc_Word32)ptrFrame[k++] << " ";
        }
        std::cout << " " << std::endl;
    }
    std::cout << " " << std::endl;
}


void PrintFrame(WebRtc_UWord8* ptrInFrame, WebRtc_Word32 width, WebRtc_Word32 height, const WebRtc_Word8* str)
{
    std::cout << str << " (" << width << "x" << height << ") = " << std::endl;

    WebRtc_UWord8* ptrFrameY  = ptrInFrame;
    WebRtc_UWord8* ptrFrameCb = ptrFrameY  + width*height;
    WebRtc_UWord8* ptrFrameCr = ptrFrameCb + width*height/4;
   
    PrintFrame(ptrFrameY,  width,   height);
    PrintFrame(ptrFrameCb, width/2, height/2);
    PrintFrame(ptrFrameCr, width/2, height/2);  
}

void CreateImage(WebRtc_Word32 width, WebRtc_Word32 height, WebRtc_UWord8* ptrFrame, WebRtc_Word32 offset, WebRtc_Word32 heightFactor, WebRtc_Word32 widthFactor = 0)
{
    for (WebRtc_Word32 i = 0; i < height; i++)
    {
        for (WebRtc_Word32 j = 0; j < width; j++)
        {
            *ptrFrame = (WebRtc_UWord8)((i + offset)*heightFactor + j*widthFactor);
            ptrFrame++;
        }
    }
}

void ValidateImage2(WebRtc_Word32 width, WebRtc_Word32 height, WebRtc_UWord8* ptrFrame, WebRtc_Word32 offset, WebRtc_Word32 factor)
{
    WebRtc_Word32 res = offset*factor;
    for (WebRtc_Word32 i = 0; i < height; i++)
    {
        for (WebRtc_Word32 j = 0; j < width; j++)
        {
            assert(ptrFrame[k++] == res);
        }
        if (i > 0)
        {
            res += factor/2;
        }
    }
}

void ValidateImage3_2(WebRtc_Word32 width, WebRtc_Word32 height, WebRtc_UWord8* ptrFrame, WebRtc_Word32 offset, WebRtc_Word32 factor)
{
    WebRtc_Word32 res = offset*factor;
    for (WebRtc_Word32 i = 1; i <= height; i++)
    {
        for (WebRtc_Word32 j = 0; j < width; j++)
        {
            assert(ptrFrame[k++] == res);
        }
        res += factor/2;
        if ((i % 3) == 0)
        {
            res += factor/2;  
        }
    }
}

void ValidateImage1_3(WebRtc_Word32 width, WebRtc_Word32 height, WebRtc_UWord8* ptrFrame, WebRtc_Word32 offset, WebRtc_Word32 factor)
{
    WebRtc_Word32 res = offset*factor;
    res += factor/2;
    for (WebRtc_Word32 i = 0; i < height; i++)
    {
        for (WebRtc_Word32 j = 0; j < width; j++)
        {
            assert(ptrFrame[k++] == res);
        }
        res += factor*3;  
    }
}

WebRtc_Word32 
VerifyAndAllocateTest(WebRtc_UWord8*& buffer, WebRtc_Word32 currentSize, WebRtc_Word32 newSize)
{
    if(newSize > currentSize)
    {
        // make sure that our buffer is big enough
        WebRtc_UWord8* newBuffer = new WebRtc_UWord8[newSize];
        if(buffer)
        {
            // copy the old data
            memcpy(newBuffer, buffer, currentSize);
            delete [] buffer;
        }
        buffer = newBuffer;
        return newSize;
    }

    return currentSize;
}

//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------

int
scale_test()
{

  std::string str;
    std::cout << "--------------------------------" << std::endl;
    std::cout << "-------- Test Scaling ----------" << std::endl;
    std::cout << "--------------------------------" << std::endl;
    std::cout << "  "  << std::endl;
  
    // -------------------------------
    // Test ScaleI420Up2() -----------
    // -------------------------------
    PRINT_LINE;
    std::cout << "Test ScaleI420Up2()" << std::endl;
    PRINT_LINE;

    WebRtc_UWord32 width = 12;
    WebRtc_UWord32 height = 10;
    WebRtc_Word32 factorY = 2;
    WebRtc_Word32 factorCb = 10;
    WebRtc_Word32 factorCr = 20;
    WebRtc_Word32 offset = 5;
    WebRtc_Word32 startBufferOffset = 10;
    WebRtc_UWord32 length = CalcBufferSize(kI420, width, height);

    // Test bad inputs
    WebRtc_UWord32 scW = 0;
    WebRtc_UWord32 scH = 0;
   
    WebRtc_UWord8* testFrame = new WebRtc_UWord8[length + offset];
    WebRtc_Word32 retVal = ScaleI420Up2(0, height,testFrame,length, scW, scH);
    assert(retVal == -1);
    retVal = ScaleI420Up2(width, 0, testFrame,length,  scW, scH);
    assert(retVal == -1);
    retVal = ScaleI420Up2(49, height, testFrame, length, scW, scH);
    assert(retVal == -1);
    retVal = ScaleI420Up2(width, 3, testFrame,length, scW, scH);   // odd height
    assert(retVal == -1);
    retVal = ScaleI420Up2(width + 2, height, testFrame,length, scW, scH); // width, height > allocated buffer size
    assert(retVal == -1);
    retVal = ScaleI420Up2(width, height + 2, testFrame,length, scW, scH); // width, height > allocated buffer size
    assert(retVal == -1);
    retVal = ScaleI420Up2(width, height, testFrame,length, scW, scH);     // width, height == allocated buffer size, OK
    assert((WebRtc_UWord32) retVal == scW * scH * 3 / 2);
    delete [] testFrame;

    testFrame = new WebRtc_UWord8[  length * 4 + startBufferOffset * 2];
    memset(testFrame, 255, length * 4 + startBufferOffset * 2);

    // Create input frame
    WebRtc_UWord8* ptrFrameY = testFrame;
    WebRtc_UWord8* ptrFrameCb = ptrFrameY + width*height;
    WebRtc_UWord8* ptrFrameCr = ptrFrameCb + width*height/4;
    CreateImage(width,   height,   ptrFrameY,  offset, factorY);  // Y
    CreateImage(width/2, height/2, ptrFrameCb, offset, factorCb); // Cb
    CreateImage(width/2, height/2, ptrFrameCr, offset, factorCr); // Cr
    PrintFrame(testFrame, width, height, "InputFrame");

    // Scale frame to twice its size
    WebRtc_UWord32 scaledWidth = 0;
    WebRtc_UWord32 scaledHeight = 0;
    retVal = ScaleI420Up2(width, height, testFrame, length * 4 + startBufferOffset * 2, scaledWidth, scaledHeight);
   
    PrintFrame(testFrame, scaledWidth, scaledHeight, "Output Frame");

    // Validate results
    assert((WebRtc_UWord32)retVal == scaledWidth * scaledHeight * 3 / 2);
    ptrFrameY = testFrame;
    ptrFrameCb  = ptrFrameY  + scaledWidth*scaledHeight;
    ptrFrameCr  = ptrFrameCb + scaledWidth*scaledHeight/4;

    ValidateImage2(scaledWidth,   scaledHeight,   ptrFrameY,  offset, factorY);
    ValidateImage2(scaledWidth/2, scaledHeight/2, ptrFrameCb, offset, factorCb);
    ValidateImage2(scaledWidth/2, scaledHeight/2, ptrFrameCr, offset, factorCr);

    delete [] testFrame;

    // --------------------------------
    // Test ScaleI420Up3_2() ----------
    // --------------------------------
    PRINT_LINE;
    std::cout << "Test ScaleI420Up3_2()" << std::endl;
    PRINT_LINE;

    width = 12;
    height = 8;
    factorY = 2;
    factorCb = 10;
    factorCr = 20;
    offset = 5;
    startBufferOffset = 10;
    length = CalcBufferSize(kI420, width, height);

    // Test bad inputs
    testFrame = new WebRtc_UWord8[length];

    retVal = ScaleI420Up3_2(0, height, testFrame,length, scW, scH);
    assert(retVal == -1);
    retVal = ScaleI420Up3_2(width, 0, testFrame,length, scW, scH);
    assert(retVal == -1);
    retVal = ScaleI420Up3_2(49, height, testFrame,length, scW, scH); // odd width
    assert(retVal == -1);
    retVal = ScaleI420Up3_2(width, 3, testFrame,length, scW, scH);   // odd height
    assert(retVal == -1);
    retVal = ScaleI420Up3_2(width, 10, testFrame,length, scW, scH);   // odd height (color)
    assert(retVal == -1);
    retVal = ScaleI420Up3_2(14, height, testFrame,length, scW, scH);   // odd width (color)
    assert(retVal == -1);
    retVal = ScaleI420Up3_2(width + 2, height, testFrame,length, scW, scH); // width, height > allocated buffer size
    assert(retVal == -1);
    retVal = ScaleI420Up3_2(width, height + 2, testFrame,length, scW, scH); // width, height > allocated buffer size
    assert(retVal == -1);
    retVal = ScaleI420Up3_2(width, height, testFrame,length, scW, scH);     // width, height == allocated buffer size, OK
    assert((WebRtc_UWord32)retVal == scW * scH * 3 / 2);

    delete [] testFrame;
       
    testFrame = new WebRtc_UWord8[length + startBufferOffset];
    memset(testFrame, 255, length + startBufferOffset);
    
    // Create input frame
    ptrFrameY = testFrame;
    ptrFrameCb = ptrFrameY + width*height;
    ptrFrameCr = ptrFrameCb + width*height/4;
    CreateImage(width,   height,   ptrFrameY,  offset, factorY);  // Y
    CreateImage(width/2, height/2, ptrFrameCb, offset, factorCb); // Cb
    CreateImage(width/2, height/2, ptrFrameCr, offset, factorCr); // Cr
    PrintFrame(testFrame, width, height, "Input Frame");


    // Scale frame to 1.5 times its size
    scaledWidth = 0;
    scaledHeight = 0;
    retVal = ScaleI420Up3_2(width, height, testFrame, length + startBufferOffset, scaledWidth, scaledHeight);
    
    PrintFrame(testFrame, scaledWidth, scaledHeight, "Output Frame");

    // Validate results
    assert((WebRtc_UWord32)retVal == scaledWidth * scaledHeight * 3 / 2);

    // Verify that function does not write outside buffer
    ptrFrameY  = testFrame;//imageBuffer.GetBuffer();
    ptrFrameCb = ptrFrameY  + scaledWidth*scaledHeight;
    ptrFrameCr = ptrFrameCb + scaledWidth*scaledHeight/4;

    ValidateImage3_2(scaledWidth,   scaledHeight,   ptrFrameY,  offset, factorY);
    ValidateImage3_2(scaledWidth/2, scaledHeight/2, ptrFrameCb, offset, factorCb);
    ValidateImage3_2(scaledWidth/2, scaledHeight/2, ptrFrameCr, offset, factorCr);

    delete [] testFrame;

    // --------------------------------
    // Test ScaleI420Down1_3() ----------
    // --------------------------------
    PRINT_LINE;
    std::cout << "Test ScaleI420Up1_3()" << std::endl;
    PRINT_LINE;

    width = 10;
    height = 8;
    factorY = 2;
    factorCb = 10;
    factorCr = 20;
    offset = 5;
    startBufferOffset = 10;
    length = webrtc::CalcBufferSize(kI420, width, height);

    // Test bad inputs
    testFrame = new WebRtc_UWord8[length];
    retVal = ScaleI420Down1_3(0, height, testFrame, length, scW, scH);
    assert(retVal == -1);
    retVal = ScaleI420Down1_3(width, 0, testFrame, length, scW, scH);
    assert(retVal == -1);
    retVal = ScaleI420Down1_3(49, height, testFrame, length, scW, scH); // odd width
    assert(retVal == -1);
    retVal = ScaleI420Down1_3(width, 3, testFrame, length, scW, scH);   // odd height
    assert(retVal == -1);
    retVal = ScaleI420Down1_3(width + 2, height, testFrame, length, scW, scH); // width, height > allocated buffer size
    assert(retVal == -1);
    retVal = ScaleI420Down1_3(width, height + 2, testFrame, length, scW, scH); // width, height > allocated buffer size
    assert(retVal == -1);
    retVal = ScaleI420Down1_3(width, height, testFrame, length, scW, scH);     // width, height == allocated buffer size, ok
    assert((WebRtc_UWord32)retVal == scW * scH * 3 / 2);
    
    delete [] testFrame;
        
    testFrame = new WebRtc_UWord8[length + startBufferOffset * 2];
    memset(testFrame, 255, length + startBufferOffset * 2);
    // Create input frame
    ptrFrameY = testFrame;
    ptrFrameCb = ptrFrameY + width*height;
    ptrFrameCr = ptrFrameCb + width*height/4;
    CreateImage(width,   height,   ptrFrameY,  offset, factorY);  // Y
    CreateImage(width/2, height/2, ptrFrameCb, offset, factorCb); // Cb
    CreateImage(width/2, height/2, ptrFrameCr, offset, factorCr); // Cr
    PrintFrame(testFrame, width, height, "Input Frame");

    // Scale frame to one third its size
    scaledWidth = 0;
    scaledHeight = 0;
    retVal = ScaleI420Down1_3(width, height, testFrame, length + startBufferOffset * 2 , scaledWidth, scaledHeight);
    
    PrintFrame(testFrame, scaledWidth, scaledHeight, "Output Frame");

    // Validate results
    assert((WebRtc_UWord32)retVal == scaledWidth * scaledHeight * 3 / 2);

    // Verify that function does not write outside buffer
    ptrFrameY  = testFrame;//imageBuffer.GetBuffer();
    ptrFrameCb = ptrFrameY  + scaledWidth*scaledHeight;
    ptrFrameCr = ptrFrameCb + scaledWidth*scaledHeight/4;

    ValidateImage1_3(scaledWidth,   scaledHeight,   ptrFrameY,  offset, factorY);
    ValidateImage1_3(scaledWidth/2, scaledHeight/2, ptrFrameCb, offset, factorCb);
    ValidateImage1_3(scaledWidth/2, scaledHeight/2, ptrFrameCr, offset, factorCr);

    delete [] testFrame;

    // -------------------
    // Test PadI420Frame()
    // -------------------
    PRINT_LINE;
    std::cout << "Test PadI420Frame()" << std::endl;
    PRINT_LINE;

    width = 16;
    height = 8;
    factorY = 1;
    factorCb = 1;
    factorCr = 1;
    offset = 5;
    startBufferOffset = 10;
    length = CalcBufferSize(kI420, width, height);

    testFrame = new WebRtc_UWord8[length];
    memset(testFrame, 255, length);

    // Create input frame
    ptrFrameY = testFrame;//imageBuffer.GetBuffer();
    ptrFrameCb = ptrFrameY + width*height;
    ptrFrameCr = ptrFrameCb + width*height/4;
    CreateImage(width,   height,   ptrFrameY,  1, factorY);  // Y
    CreateImage(width/2, height/2, ptrFrameCb, 100, factorCb); // Cb
    CreateImage(width/2, height/2, ptrFrameCr, 200, factorCr); // Cr
    PrintFrame(testFrame, width, height, "Input Frame");

    WebRtc_UWord8* testFrame2 = new WebRtc_UWord8[352*288];

    // Test bad input
    assert(PadI420Frame(NULL, testFrame2, 16, 16, 32, 32) == -1);
    assert(PadI420Frame(testFrame, NULL, 16, 16, 32, 32) == -1);
    assert(PadI420Frame(testFrame, testFrame2, 0, 16, 32, 32) == -1);
    assert(PadI420Frame(testFrame, testFrame2, 16, 0, 32, 32) == -1);
    assert(PadI420Frame(testFrame, testFrame2, 16, 16, 0, 32) == -1);
    assert(PadI420Frame(testFrame, testFrame2, 16, 16, 32, 0) == -1);
    assert(PadI420Frame(testFrame, testFrame2, 16, 16, 8, 32) == -1);
    assert(PadI420Frame(testFrame, testFrame2, 16, 16, 32, 8) == -1);
    assert(PadI420Frame(testFrame, testFrame2, 16, 16, 16, 16) == 3 * 16 * 16 / 2);

    enum { NumOfPaddedSizes = 4 };
    WebRtc_Word32 paddedWidth[NumOfPaddedSizes] = { 32, 22, 16, 20 };
    WebRtc_Word32 paddedHeight[NumOfPaddedSizes] = { 16, 14, 12, 8 };
    
    for (WebRtc_Word32 i = 0; i < NumOfPaddedSizes; i++)
    {
        scaledWidth = paddedWidth[i];
        scaledHeight = paddedHeight[i];
        
        WebRtc_Word32 toLength = webrtc::CalcBufferSize(kI420, scaledWidth, scaledHeight);

         if (testFrame2)
         {
             delete [] testFrame2;
         }
         testFrame2 = new WebRtc_UWord8[toLength + startBufferOffset * 2];
         memset(testFrame2, 255, toLength + startBufferOffset * 2);


        retVal = webrtc::PadI420Frame(testFrame, testFrame2, width, height, scaledWidth, scaledHeight);
        PrintFrame(testFrame2, scaledWidth, scaledHeight, "Output Frame");
        
        // Validate results
        assert(retVal == toLength);

    }
    std::cout << "Do the padded frames look correct?" << std::endl
        << "(Padded dimensions which are multiples of 16 will have the" << std::endl
        << "padding applied in blocks of 16)" << std::endl
        << "Press enter to continue...";
    std::getline(std::cin, str);

    // -----------------
    // Test video sizes
    // -----------------
    const WebRtc_Word32 nr = 16;
    // currently not keeping video sizes as a type - testing scaling functions only
    WebRtc_UWord16 widths[nr] =  {128, 160, 176, 320, 352, 640, 720, 704, 800, 960, 1024, 1440, 400, 800, 1280, 1920};
    WebRtc_UWord16 heights[nr] = { 96, 120, 144, 240, 288, 480, 480, 576, 600, 720,  768, 1080, 240, 480,  720, 1080};

    for (WebRtc_Word32 j = 0; j < 3; j++)
    {
        for (WebRtc_Word32 i = 0; i < nr; i++)
        {
            width = widths[i];
            height = heights[i];
            factorY = 2;
            factorCb = 2;
            factorCr = 2;
            offset = 2;
            startBufferOffset = 10;
            length = webrtc::CalcBufferSize(kI420, width, height);

            float f = 1;
            if (j == 0)
            {
                f = 2;
            }
            else if (j == 1)
            {
                f = 1.5;
            }
            else if (j == 2)
            {
                f = 1;
            }

            if (testFrame)
            {
                delete testFrame;
                testFrame = 0;
            }
            WebRtc_Word32 frameSize = (WebRtc_Word32) ((length * f * f) + startBufferOffset * 2);
            testFrame = new WebRtc_UWord8[frameSize];
            memset(testFrame, 255, frameSize);
          
            // Create input frame
            ptrFrameY = testFrame;
            ptrFrameCb = ptrFrameY + width*height;
            ptrFrameCr = ptrFrameCb + width*height/4;
            CreateImage(width,   height,   ptrFrameY,  offset, factorY);  // Y
            CreateImage(width/2, height/2, ptrFrameCb, offset, factorCb); // Cb
            CreateImage(width/2, height/2, ptrFrameCr, offset, factorCr); // Cr
          
            scaledWidth = 0;
            scaledHeight = 0;
            if (j == 0)
            {
                retVal = ScaleI420Up2(width, height, testFrame,frameSize, scaledWidth, scaledHeight);
                length = scaledWidth*scaledHeight*3/2;
            }
            else if (j == 1)
            {
                retVal = ScaleI420Up3_2(width, height, testFrame,frameSize, scaledWidth, scaledHeight);
                length = scaledWidth*scaledHeight*3/2;
            }
            else if (j == 2)
            {
                retVal = ScaleI420Down1_3(width, height, testFrame,frameSize, scaledWidth, scaledHeight);
                length = width*height*3/2;
            }

            // Validate results
            assert((WebRtc_UWord32)retVal == scaledWidth * scaledHeight * 3 / 2);
        }
    }

    // ---------------------
    // Test mirror functions
    // ---------------------
    std::cout << "Test Mirror function" << std::endl;
    
    // 4:2:0 images can't have odd width or height
    width = 16;
    height = 8;
    factorY = 1;
    factorCb = 1;
    factorCr = 1;
    offset = 5;
    startBufferOffset = 10;
    length = webrtc::CalcBufferSize(kI420, width, height);

    delete [] testFrame;
    testFrame = new WebRtc_UWord8[length];
    memset(testFrame, 255, length);

    // Create input frame
    WebRtc_UWord8* inFrame = testFrame;
    ptrFrameCb = inFrame + width * height;
    ptrFrameCr = ptrFrameCb + (width * height) / 4;
    CreateImage(width,   height,   inFrame,  10, factorY, 1);  // Y
    CreateImage(width/2, height/2, ptrFrameCb, 100, factorCb, 1); // Cb
    CreateImage(width/2, height/2, ptrFrameCr, 200, factorCr, 1); // Cr
    PrintFrame(testFrame, width, height, "Input Frame");

    if (testFrame2)
    {
        delete [] testFrame2;
        testFrame2 = 0;
    }
    testFrame2 = new WebRtc_UWord8[length + startBufferOffset * 2];
    memset(testFrame2, 255, length + startBufferOffset * 2);
    WebRtc_UWord8* outFrame = testFrame2;
    
    // LeftRight
    std::cout << "Test Mirror function: LeftRight" << std::endl;
    retVal = MirrorI420LeftRight(inFrame, outFrame, width, height);
    PrintFrame(testFrame2, width, height, "Output Frame");
    retVal = MirrorI420LeftRight(outFrame, outFrame, width, height);

    assert(memcmp(inFrame, outFrame, length) == 0);
    //VerifyInBounds(outFrame, length, startBufferOffset, startBufferOffset);
    
    //UpDown
    std::cout << "Test Mirror function: UpDown" << std::endl;
    retVal = MirrorI420UpDown(inFrame, outFrame, width, height);
    PrintFrame(testFrame2, width, height, "Output Frame");
    retVal = MirrorI420UpDown(outFrame, outFrame, width, height);

    assert(memcmp(inFrame, outFrame, length) == 0);
    //VerifyInBounds(outFrame, length, startBufferOffset, startBufferOffset);

    std::cout << "Do the mirrored frames look correct?" << std::endl
        << "Press enter to continue...";
    std::getline(std::cin, str);
  // end Mirror Function check

    delete [] testFrame;
    testFrame = new WebRtc_UWord8[length];
    memset(testFrame,255,length);
    inFrame = testFrame;

    CreateImage(width,   height,   inFrame,  10, factorY, 1);  // Y
    CreateImage(width/2, height/2, ptrFrameCb, 100, factorCb, 1); // Cb
    CreateImage(width/2, height/2, ptrFrameCr, 200, factorCr, 1); // Cr

    PrintFrame(inFrame, width, height, "Input frame");

    delete [] testFrame2;
    testFrame2 = new WebRtc_UWord8[length];
    memset(testFrame2, 255, length);
    int yv12Size = CalcBufferSize(kI420, kYV12, length);
    WebRtc_UWord8* yv12TestFrame = new WebRtc_UWord8[yv12Size];
    memset(yv12TestFrame, 255, yv12Size);
    outFrame = testFrame2;    
    retVal = ConvertI420ToYV12(inFrame, yv12TestFrame, width, height, 0);
    assert(retVal >= 0);

    // Test convert and mirror functions
    ConvertToI420AndMirrorUpDown(yv12TestFrame, outFrame, width, height, kYV12);
    std::cout << "Test: ConvertAndMirrorUpDown" << std::endl;
    PrintFrame(outFrame, width, height, "Output Frame");
    MirrorI420UpDown(outFrame, outFrame, width, height);
    assert(memcmp(inFrame, outFrame, length) == 0);
    std::cout << "Does the converted (U and V flipped) mirrored frame look correct?" << std::endl
        << "Press enter to continue...";
    std::getline(std::cin, str);
    delete [] testFrame2;

    PrintFrame(inFrame, width, height, "Input frame");

    // Test convert and rotate functions
    testFrame2 = new WebRtc_UWord8[length];
    memset(testFrame2, 255, length);
    outFrame = testFrame2;
    WebRtc_UWord8* tempFrame = new WebRtc_UWord8[length];

    ConvertToI420(kYV12, yv12TestFrame, width, height, outFrame, false, kRotateAntiClockwise);
    std::cout << "Test: ConvertAndRotateAntiClockwise" << std::endl;
    PrintFrame(outFrame, height, width, "Output Frame");
    ConvertToI420(kI420, outFrame, height, width, tempFrame, false, kRotateAntiClockwise);
    ConvertToI420(kI420, tempFrame, width, height, outFrame, false, kRotateAntiClockwise);
    ConvertToI420(kI420, outFrame, height, width, tempFrame, false, kRotateAntiClockwise);
    assert(memcmp(inFrame, tempFrame, length) == 0);

    delete [] testFrame2;

    testFrame2 = new WebRtc_UWord8[length];
    outFrame = testFrame2;
    memset(outFrame, 255, length);
    memset(tempFrame, 255, length);
    ConvertToI420(kYV12, yv12TestFrame, width, height, outFrame, false, kRotateClockwise);
    std::cout << "Test: ConvertAndRotateClockwise" << std::endl;
    PrintFrame(outFrame, height, width, "Output Frame");
    ConvertToI420(kI420, outFrame, height, width, tempFrame, false, kRotateClockwise);
    ConvertToI420(kI420, tempFrame, width, height, outFrame, false, kRotateClockwise);
    ConvertToI420(kI420, outFrame, height, width, tempFrame, false, kRotateClockwise);
    assert(memcmp(inFrame, tempFrame, length) == 0);

    delete [] testFrame2;

    std::cout << "Do the converted (U and V flipped) and rotated frames look correct?" << std::endl
        << "Press enter to continue...";
    std::getline(std::cin, str);


    PrintFrame(inFrame, width, height, "Input frame");

    // Test rotation with padding
   
    height += 4;
    length = width * height * 3 / 2;
    testFrame2 = new WebRtc_UWord8[length];
    memset(testFrame2, 255, length);
    outFrame = testFrame2;
    webrtc::ConvertToI420(kYV12, yv12TestFrame, width, height - 4, outFrame, false, webrtc::kRotateClockwise);
    std::cout << "Test: ConvertAndRotateClockwise (width padding)" << std::endl;
    PrintFrame(outFrame, height, width, "Output Frame");

    width += 4;
    height -= 4;
    memset(testFrame2, 255, length);
    outFrame = testFrame2;
    ConvertToI420(kYV12, yv12TestFrame, width - 4, height, outFrame, false, webrtc::kRotateAntiClockwise);
    std::cout << "Test: ConvertAndRotateClockwise (height padding)" << std::endl;
    PrintFrame(outFrame, height, width, "Output Frame");

    std::cout << "Do the rotated and padded images look correct?" << std::endl
        << "Press enter to continue...";
    std::getline(std::cin, str);

    delete [] tempFrame;
    tempFrame = NULL;
    delete [] testFrame;
    testFrame = NULL;
    delete [] testFrame2;
    testFrame2 = NULL;
    delete [] yv12TestFrame;
    yv12TestFrame = NULL;

    TEST_PASSED();
    std::cout << "Press enter to quit test...";
    std::getline(std::cin, str);

    return 0;
}
