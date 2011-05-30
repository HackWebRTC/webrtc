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
 * test_jpeg.cc
 */

#include <cassert>
#include <iostream>
#include <cmath>
#include <string>
#include <stdio.h>


#include "test_buffer.h"
#include "jpeg.h"

using namespace webrtc;

#define PRINT_LINE std::cout << "-------------------------------" << std::endl;

int
main(int argc, char **argv)
{
    if (argc < 1)
    {
        return -1;
    }
    std::string fileName = argv[1];
    const char* fileNameDec = "TestJpegDec.yuv";
    const char* fileNameEnc = "TestJpegEnc.jpg";

    std::string str;
    std::cout << "---------------------" << std::endl;
    std::cout << "----- Test JPEG -----" << std::endl;
    std::cout << "---------------------" << std::endl;
    std::cout << "  "  << std::endl;


    JpegDecoder* JpgDecPtr = new JpegDecoder( );

    // Open input file
    FILE* openFile = fopen(fileName.c_str(), "rb");
    assert(openFile != NULL);

    // Get file length
    fseek(openFile, 0, SEEK_END);
    int length = ftell(openFile);
    fseek(openFile, 0, SEEK_SET);

    // Read input file to buffer
    TestBuffer encodedBuffer;
    encodedBuffer.VerifyAndAllocate(length);
    encodedBuffer.UpdateLength(length);
    fread(encodedBuffer.GetBuffer(), 1, length, openFile);
    fclose(openFile);

    // ------------------
    // Decode
    // ------------------

    TestBuffer imageBuffer;
    WebRtc_UWord32 width = 0;
    WebRtc_UWord32 height = 0;
    WebRtc_UWord8* tmp = NULL;
    int error = JpgDecPtr->Decode(encodedBuffer.GetBuffer(),
                                  encodedBuffer.GetSize(), tmp, width, height);

    std::cout << error << " = Decode(" << fileName.c_str() << ", (" << width <<
        "x" << height << "))" << std::endl;
    PRINT_LINE;

    if (error == 0)
    {
        int imageBufferSize = width*height*3/2;
        //update buffer info
        imageBuffer.VerifyAndAllocate( imageBufferSize);
        imageBuffer.CopyBuffer(imageBufferSize, tmp);
        delete [] tmp;
        // Save decoded image to file
        FILE* saveFile = fopen(fileNameDec, "wb");
        fwrite(imageBuffer.GetBuffer(), 1, imageBuffer.GetLength(), saveFile);
        fclose(saveFile);

        // ------------------
        // Encode
        // ------------------

        JpegEncoder* JpegEncoderPtr = new JpegEncoder();

        // Test invalid inputs

        // Test buffer
        TestBuffer empty;

        int error = JpegEncoderPtr->SetFileName(0);
        assert(error == -1);
        error = JpegEncoderPtr->Encode(empty.GetBuffer(), empty.GetSize(),
                                       164, 164);
        assert(error == -1);
        error = JpegEncoderPtr->Encode(empty.GetBuffer(), empty.GetSize(),
                                       0, height);
        assert(error == -1);
        error = JpegEncoderPtr->Encode(empty.GetBuffer(), empty.GetSize(),
                                       width, 0);
        assert(error == -1);

        error = JpegEncoderPtr->SetFileName(fileNameEnc);
        assert(error == 0);

        // Actual Encode
        error = JpegEncoderPtr->Encode(imageBuffer.GetBuffer(),
                                       imageBuffer.GetSize(), width, height);
        assert(error == 0);

        std::cout << error << " = Encode(" << fileNameDec << ")" << std::endl;

        PRINT_LINE;

        delete JpegEncoderPtr;
    }

    imageBuffer.Free();
    encodedBuffer.Free();
    delete JpgDecPtr;

    std::cout << "Verify that the encoded and decoded images look correct."
        << std::endl;
    std::cout << "Press enter to quit test...";
    std::getline(std::cin, str);

    return 0;
}

