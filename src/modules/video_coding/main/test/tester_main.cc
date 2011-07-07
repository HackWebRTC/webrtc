/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "receiver_tests.h"
#include "normal_test.h"
#include "codec_database_test.h"
#include "generic_codec_test.h"
#include "../source/event.h"
#include "media_opt_test.h"
#include "quality_modes_test.h"
#include "test_util.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
//#include "vld.h"
#endif

using namespace webrtc;

/*
 * Build with TICK_TIME_DEBUG and EVENT_DEBUG defined
 * to build the tests with simulated clock.
 */

// TODO(holmer): How do we get debug time into the cmd line interface?
/* Debug time */
#if defined(TICK_TIME_DEBUG) && defined(EVENT_DEBUG)
WebRtc_Word64 VCMTickTime::_timeNowDebug = 0; // current time in ms
#endif

int ParseArguments(int argc, char **argv, CmdArgs& args)
{
    int i = 1;
    while (i < argc)
    {
        if (argv[i][0] != '-')
        {
            return -1;
        }
        switch (argv[i][1])
        {
        case 'w':
        {
            int w = atoi(argv[i+1]);
            if (w < 1)
                return -1;
            args.width = w;
            break;
        }
        case 'h':
        {
            int h = atoi(argv[i+1]);
            if (h < 1)
                return -1;
            args.height = h;
            break;
        }
        case 'b':
        {
            int b = atoi(argv[i+1]);
            if (b < 1)
                return -1;
            args.bitRate = b;
            break;
        }
        case 'f':
        {
            int f = atoi(argv[i+1]);
            if (f < 1)
                return -1;
            args.frameRate = f;
            break;
        }
        case 'c':
        {
            // TODO(holmer): This should be replaced with a map if more codecs
            // are added
            args.codecName = argv[i+1];
            if (strncmp(argv[i+1], "VP8", 3) == 0)
            {
                args.codecType = kVideoCodecVP8;
            }
            else if (strncmp(argv[i+1], "I420", 4) == 0)
            {
                args.codecType = kVideoCodecI420;
            }
            else if (strncmp(argv[i+1], "H263", 4) == 0)
            {
                args.codecType = kVideoCodecH263;
            }
            else
                return -1;

            break;
        }
        case 'i':
        {
            args.inputFile = argv[i+1];
            break;
        }
        case 'o':
            args.outputFile = argv[i+1];
            break;
        case 'n':
        {
            int n = atoi(argv[i+1]);
            if (n < 1)
                return -1;
            args.testNum = n;
            break;
        }
        default:
            return -1;
        }
        i += 2;
    }
    return 0;
}

int main(int argc, char **argv)
{
    CmdArgs args;

    if (ParseArguments(argc, argv, args) != 0)
    {
        printf("Unable to parse input arguments\n");
        printf("args: -n <test #> -w <width> -h <height> -f <fps> -b <bps> -c <codec>"
               " -i <input file> -o <output file>\n");
        return -1;
    }
    int ret = 0;
    switch (args.testNum)
    {
    case 1:
        ret = NormalTest::RunTest(args);
        break;
    case 2:
        ret = MTRxTxTest(args);
        break;
    case 3:
        ret = GenericCodecTest::RunTest(args);
        break;
    case 4:
        ret = CodecDataBaseTest::RunTest(args);
        break;
    case 5:
        // 0- normal, 1-Release test(50 runs) 2- from file
        ret = MediaOptTest::RunTest(0, args);
        break;
    case 6:
        ret = ReceiverTimingTests(args);
        break;
    case 7:
        ret = RtpPlay(args);
        break;
    case 8:
        ret = RtpPlayMT(args);
        break;
    case 9:
        ret = JitterBufferTest(args);
        break;
    case 10:
        ret = DecodeFromStorageTest(args);
        break;
    default:
        ret = -1;
        break;
    }
    if (ret != 0)
    {
        printf("Test failed!\n");
        return -1;
    }
    return 0;
}



