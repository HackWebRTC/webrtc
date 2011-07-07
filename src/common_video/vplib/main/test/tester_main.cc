/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vplib.h"
#include "test_util.h"

using namespace webrtc;

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
        case 'x':
        {
            int x = atoi(argv[i+1]);
            if (x < 1)
                return -1;
            args.dstWidth = x;
            break;
        }
        case 'y':
        {
            int y = atoi(argv[i+1]);
            if (y < 1)
                return -1;
            args.dstHeight = y;
            break;
        }
        case 'm': // interpolation method
        {
            int m = atoi(argv[i+1]);
            if (m < 0)
                return -1;
            args.intMethod = m;
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
        printf("args: -n <test #> -w <width> -h <height>  "
               "-x <destination width> -y <destination height> -f <fps> "
               "-b <bps> -m <method> -i <input file> -o <output file>\n");
        return -1;
    }
    int ret = -1;
    switch (args.testNum)
    {
        printf("\n");
        case 1:
            printf("VPLIB Interpolation Test\n");
            ret = interpolation_test(args);
            break;
        case 2:
            printf("VPLIB Scale Test\n");
            ret = scale_test();
            break;
        case 3:
            printf("VPLIB Convert Test\n");
            ret = convert_test(args);
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
