/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <stdio.h>
#include <vector>

#include "NETEQTEST_RTPpacket.h"


/*********************/
/* Misc. definitions */
/*********************/

enum {kRedPayloadType = 127};

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
    fprintf(outFile, "SeqNo  TimeStamp   SendTime  Size    PT  M\n");

    // read file header 
    NETEQTEST_RTPpacket::skipFileHeader(inFile);
    NETEQTEST_RTPpacket packet;

    while (packet.readFromFile(inFile) >= 0)
    {
        // write packet data to file
        fprintf(outFile, "%5u %10u %10u %5i %5i %2i\n",
            packet.sequenceNumber(), packet.timeStamp(), packet.time(),
            packet.dataLen(), packet.payloadType(), packet.markerBit());
        if (packet.payloadType() == kRedPayloadType) {
            WebRtcNetEQ_RTPInfo redHdr;
            int len;
            int redIndex = 0;
            while ((len = packet.extractRED(redIndex++, redHdr)) >= 0)
            {
                fprintf(outFile, "* %5u %10u %10u %5i %5i\n",
                    redHdr.sequenceNumber, redHdr.timeStamp, packet.time(),
                    len, redHdr.payloadType);
            }
            assert(redIndex > 1);  // We must get at least one payload.
        }
    }

    fclose(inFile);
    fclose(outFile);

    return 0;
}
