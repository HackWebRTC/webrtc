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


#include "video_image.h"
#include "jpeg.h"

using namespace webrtc;

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
    size_t length = ftell(openFile);
    fseek(openFile, 0, SEEK_SET);


    // Read input file to buffer
    EncodedImage encodedBuffer;
    encodedBuffer._buffer = new WebRtc_UWord8[length];
    encodedBuffer._size = length;
    encodedBuffer._length = length;
    if (fread(encodedBuffer._buffer, 1, length, openFile) != length)
    {
        printf("Error reading file %s\n", fileName.c_str());
        exit(1);
    }
    fclose(openFile);

    // ------------------
    // Decode
    // ------------------

    RawImage imageBuffer;
    int error = JpgDecPtr->Decode(encodedBuffer, imageBuffer);

    std::cout << error << " = Decode(" << fileName.c_str() << ", "
        "(" << imageBuffer._width <<
        "x" << imageBuffer._height << "))" << std::endl;

    if (error == 0)
    {
        // Save decoded image to file
        FILE* saveFile = fopen(fileNameDec, "wb");
        fwrite(imageBuffer._buffer, 1, imageBuffer._length, saveFile);
        fclose(saveFile);

        // ------------------
        // Encode
        // ------------------

        JpegEncoder* JpegEncoderPtr = new JpegEncoder();

        // Test invalid inputs
        RawImage empty;
        empty._width = 164;
        empty._height = 164;
        int error = JpegEncoderPtr->SetFileName(0);
        assert(error == -1);
        error = JpegEncoderPtr->Encode(empty);
        assert(error == -1);
        empty._buffer = new WebRtc_UWord8[10];
        empty._size = 0;
        error = JpegEncoderPtr->Encode(empty);
        assert(error == -1);
        empty._size = 10;
        empty._height = 0;
        error = JpegEncoderPtr->Encode(empty);
        assert(error == -1);
        empty._height = 164;
        empty._width = 0;
        error = JpegEncoderPtr->Encode(empty);
        assert(error == -1);

        error = JpegEncoderPtr->SetFileName(fileNameEnc);
        assert(error == 0);

        delete [] empty._buffer;

        // Actual Encode
        error = JpegEncoderPtr->Encode(imageBuffer);
        assert(error == 0);
        std::cout << error << " = Encode(" << fileNameDec << ")" << std::endl;
        delete JpegEncoderPtr;
    }

    delete [] imageBuffer._buffer;
    delete [] encodedBuffer._buffer;
    delete JpgDecPtr;

    std::cout << "Verify that the encoded and decoded images look correct."
        << std::endl;
    std::cout << "Press enter to quit test...";
    std::getline(std::cin, str);

    return 0;
}

