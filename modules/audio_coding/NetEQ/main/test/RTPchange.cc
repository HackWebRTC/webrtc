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

bool pktCmp (NETEQTEST_RTPpacket *a, NETEQTEST_RTPpacket *b) 
{
    return (a->time() < b->time());
}


int main(int argc, char* argv[])
{
	FILE *inFile=fopen(argv[1],"rb");
	if (!inFile)
    {
        printf("Cannot open input file %s\n", argv[1]);
        return(-1);
    }
    printf("Input RTP file: %s\n",argv[1]);

    FILE *statFile=fopen(argv[2],"rt");
	if (!statFile)
    {
        printf("Cannot open timing file %s\n", argv[2]);
        return(-1);
    }
    printf("Timing file: %s\n",argv[2]);

	FILE *outFile=fopen(argv[3],"wb");
	if (!outFile)
    {
        printf("Cannot open output file %s\n", argv[3]);
        return(-1);
    }
	printf("Output RTP file: %s\n\n",argv[3]);

    // read all statistics and insert into map
    // read first line
    char tempStr[100];
    fgets(tempStr, 100, statFile);

    // define map
    std::map<std::pair<WebRtc_UWord16, WebRtc_UWord32>, WebRtc_Word32> packetStats;
    WebRtc_UWord16 seqNo;
    WebRtc_UWord32 ts;
    WebRtc_Word32 sendTime;

    while(fscanf(statFile, "%hu %lu %ld\n", &seqNo, &ts, &sendTime) == 3)
    {
        std::pair<WebRtc_UWord16, WebRtc_UWord32> tempPair = std::pair<WebRtc_UWord16, WebRtc_UWord32>(seqNo, ts);

        packetStats[tempPair] = sendTime;
    }

    fclose(statFile);

    // read file header and write directly to output file
	char firstline[FIRSTLINELEN];
	fgets(firstline, FIRSTLINELEN, inFile);
	fputs(firstline, outFile);
	fread(firstline, 4+4+4+2+2, 1, inFile); // start_sec + start_usec	+ source + port + padding
	fwrite(firstline, 4+4+4+2+2, 1, outFile);

    std::vector<NETEQTEST_RTPpacket *> packetVec;
    int i = 0;

    while (1)
    {
        // insert in vector
        NETEQTEST_RTPpacket *newPacket = new NETEQTEST_RTPpacket();
        if (newPacket->readFromFile(inFile) < 0)
        {
            // end of file
            break;
        }
        
        // look for new send time in statistics vector
        std::pair<WebRtc_UWord16, WebRtc_UWord32> tempPair = 
            std::pair<WebRtc_UWord16, WebRtc_UWord32>(newPacket->sequenceNumber(), newPacket->timeStamp());

        WebRtc_Word32 newSendTime = packetStats[tempPair];
        if (newSendTime >= 0) 
        {
            newPacket->setTime(newSendTime); // set new send time
            packetVec.push_back(newPacket); // insert in vector
        }
        else
        {
            // negative value represents lost packet
            // don't insert, but delete packet object
            delete newPacket;
        }

    }

    // sort the vector according to send times
    std::sort(packetVec.begin(), packetVec.end(), pktCmp);

    std::vector<NETEQTEST_RTPpacket *>::iterator it;
    for (it = packetVec.begin(); it != packetVec.end(); it++)
    {
        // write to out file
        if ((*it)->writeToFile(outFile) < 0)
        {
            printf("Error writing to file\n");
            return(-1);
        }

        // delete packet
        delete *it;
    }

    fclose(inFile);
    fclose(outFile);

    return 0;
}
