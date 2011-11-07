/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <stdio.h>
#include <vector>

#include "NETEQTEST_RTPpacket.h"

/*********************/
/* Misc. definitions */
/*********************/

#define FIRSTLINELEN 40

int main(int argc, char* argv[])
{
    if (argc < 3) {
        printf("Usage: RTPcat in1.rtp int2.rtp [...] out.rtp\n");
        exit(1);
    }

    FILE *inFile = fopen(argv[1], "rb");
    if (!inFile) {
        printf("Cannot open input file %s\n", argv[1]);
        return (-1);
    }

    FILE *outFile = fopen(argv[argc - 1], "wb"); // last parameter is output file
    if (!outFile) {
        printf("Cannot open output file %s\n", argv[argc - 1]);
        return (-1);
    }
    printf("Output RTP file: %s\n\n", argv[argc - 1]);

    // read file header and write directly to output file
    char firstline[FIRSTLINELEN];
    fgets(firstline, FIRSTLINELEN, inFile);
    fputs(firstline, outFile);
    fread(firstline, 4 + 4 + 4 + 2 + 2, 1, inFile); // start_sec + start_usec	+ source + port + padding
    fwrite(firstline, 4 + 4 + 4 + 2 + 2, 1, outFile);

    // close input file and re-open it later (easier to write the loop below)
    fclose(inFile);

    for (int i = 1; i < argc - 1; i++) {

        inFile = fopen(argv[i], "rb");
        if (!inFile) {
            printf("Cannot open input file %s\n", argv[i]);
            return (-1);
        }
        printf("Input RTP file: %s\n", argv[i]);

        // skip file header
        fgets(firstline, FIRSTLINELEN, inFile);
        fread(firstline, 4 + 4 + 4 + 2 + 2, 1, inFile); // start_sec + start_usec   + source + port + padding

        NETEQTEST_RTPpacket packet;
        int packLen = packet.readFromFile(inFile);
        if (packLen < 0) {
            exit(1);
        }

        while (packLen >= 0) {

            packet.writeToFile(outFile);

            packLen = packet.readFromFile(inFile);

        }

        fclose(inFile);

    }

    fclose(outFile);

    return 0;
}
