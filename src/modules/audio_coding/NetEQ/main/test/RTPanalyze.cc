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
#include <vector>

#include "NETEQTEST_RTPpacket.h"


/*********************/
/* Misc. definitions */
/*********************/

#define FIRSTLINELEN 40


int main(int argc, char* argv[])
{
	FILE *inFile=fopen(argv[1],"rb");
	if (!inFile)
    {
        printf("Cannot open input file %s\n", argv[1]);
        return(-1);
    }
    printf("Input file: %s\n",argv[1]);

	FILE *outFile=fopen(argv[2],"wt");
	if (!outFile)
    {
        printf("Cannot open output file %s\n", argv[2]);
        return(-1);
    }
	printf("Output file: %s\n\n",argv[2]);

    // print file header
    fprintf(outFile, "SeqNo  TimeStamp   SendTime  Size\n");


    // read file header 
	char firstline[FIRSTLINELEN];
	fgets(firstline, FIRSTLINELEN, inFile);
	fread(firstline, 4+4+4+2+2, 1, inFile); // start_sec + start_usec	+ source + port + padding
	
    NETEQTEST_RTPpacket packet;

    while (packet.readFromFile(inFile) >= 0)
    {
        // write packet data to file
        fprintf(outFile, "%5hu %10lu %10lu %5hi\n", 
            packet.sequenceNumber(), packet.timeStamp(), packet.time(), packet.dataLen());
    }

    fclose(inFile);
    fclose(outFile);

    return 0;
}
