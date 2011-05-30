/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/**
 * \file testFec.cpp
 * Test application for core FEC algorithm. Calls encoding and decoding functions in
 * ForwardErrorCorrection directly.
 *
 */

#include "forward_error_correction.h"
#include "list_wrapper.h"
#include "rtp_utility.h"

#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <ctime>

#include <windows.h>

//#include "vld.h"

#include "fec_private_tables.h"

//#define VERBOSE_OUTPUT

void ReceivePackets(ListWrapper& toDecodeList, ListWrapper& receivedPacketList,
                    WebRtc_UWord32 numPacketsToDecode, float reorderRate, float duplicateRate)
{
    assert(toDecodeList.Empty());
    assert(numPacketsToDecode <= receivedPacketList.GetSize());

    ListItem* listItem = receivedPacketList.First();
    for (WebRtc_UWord32 i = 0; i < numPacketsToDecode; i++)
    {
        // Reorder packets.
        float randomVariable = static_cast<float>(rand()) / RAND_MAX;
        while (randomVariable < reorderRate)
        {
            ListItem* nextItem = receivedPacketList.Next(listItem);
            if (nextItem == NULL)
            {
                break;
            }
            else
            {
                listItem = nextItem;
            }
            randomVariable = static_cast<float>(rand()) / RAND_MAX;
        }

        assert(listItem != NULL);
        ForwardErrorCorrection::ReceivedPacket* receivedPacket =
            static_cast<ForwardErrorCorrection::ReceivedPacket*>(listItem->GetItem());
        toDecodeList.PushBack(receivedPacket);

        // Duplicate packets.
        randomVariable = static_cast<float>(rand()) / RAND_MAX;
        while (randomVariable < duplicateRate)
        {
            ForwardErrorCorrection::ReceivedPacket* duplicatePacket =
                new ForwardErrorCorrection::ReceivedPacket;
            memcpy(duplicatePacket, receivedPacket,
                sizeof(ForwardErrorCorrection::ReceivedPacket));

            duplicatePacket->pkt = new ForwardErrorCorrection::Packet;
            memcpy(duplicatePacket->pkt->data, receivedPacket->pkt->data,
                receivedPacket->pkt->length);
            duplicatePacket->pkt->length = receivedPacket->pkt->length;

            toDecodeList.PushBack(duplicatePacket);
            randomVariable = static_cast<float>(rand()) / RAND_MAX;
        }

        receivedPacketList.Erase(listItem);
        listItem = receivedPacketList.First();
    }
}

int main()
{
    enum { MaxNumberMediaPackets = 48 };
    enum { MaxNumberFecPackets = 48 };
    WebRtc_UWord32 id = 0;
    ForwardErrorCorrection fec(id);

    ListWrapper mediaPacketList;
    ListWrapper fecPacketList;
    ListWrapper toDecodeList;
    ListWrapper receivedPacketList;
    ListWrapper recoveredPacketList;
    ListWrapper fecMaskList;
    ForwardErrorCorrection::Packet* mediaPacket;
    const float lossRate[] = {0, 0.05f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f};
    const WebRtc_UWord32 lossRateSize = sizeof(lossRate)/sizeof(*lossRate);
    const float reorderRate = 0.1f;
    const float duplicateRate = 0.1f;

    WebRtc_UWord8 mediaLossMask[MaxNumberMediaPackets];
    WebRtc_UWord8 fecLossMask[MaxNumberFecPackets];
    WebRtc_UWord8 fecPacketMasks[MaxNumberFecPackets][MaxNumberMediaPackets];

    // Seed the random number generator, storing the seed to file in order to reproduce
    // past results.
    const unsigned int randomSeed = static_cast<unsigned int>(time(NULL));
    srand(randomSeed);
    FILE* randomSeedFile = fopen("randomSeedLog.txt", "a");
    fprintf(randomSeedFile, "%u\n", randomSeed);
    fclose(randomSeedFile);
    randomSeedFile = NULL;

    WebRtc_UWord16 seqNum = static_cast<WebRtc_UWord16>(rand());
    WebRtc_UWord32 timeStamp = static_cast<WebRtc_UWord32>(rand());
    const WebRtc_UWord32 ssrc = static_cast<WebRtc_UWord32>(rand());

    for (WebRtc_UWord32 lossRateIdx = 0; lossRateIdx < lossRateSize; lossRateIdx++)
    {
        printf("Loss rate: %.2f\n", lossRate[lossRateIdx]);
        for (WebRtc_UWord32 numMediaPackets = 1; numMediaPackets <= MaxNumberMediaPackets;
            numMediaPackets++)
        {
            for (WebRtc_UWord32 numFecPackets = 1; numFecPackets <= numMediaPackets &&
                numFecPackets <= MaxNumberFecPackets; numFecPackets++)
            {
#ifdef VERBOSE_OUTPUT
                printf("%u media packets, %u FEC packets\n", numMediaPackets, numFecPackets);
                printf("Packet mask matrix:\n");
#endif

                // Transfer packet masks from bit-mask to byte-mask.
                const WebRtc_UWord8* packetMask = packetMaskTbl[numMediaPackets - 1][numFecPackets - 1];
                WebRtc_UWord32 maskBytesPerFecPacket = 2;
                if (numMediaPackets > 16)
                {
                    maskBytesPerFecPacket = 6;
                }

                for (WebRtc_UWord32 i = 0; i < numFecPackets; i++)
                {
                    for (WebRtc_UWord32 j = 0; j < numMediaPackets; j++)
                    {
                        const WebRtc_UWord8 byteMask = packetMask[i * maskBytesPerFecPacket + j / 8];
                        const WebRtc_UWord32 bitPosition = (7 - j % 8);
                        fecPacketMasks[i][j] = (byteMask & (1 << bitPosition)) >> bitPosition;
#ifdef VERBOSE_OUTPUT
                        printf("%u ", fecPacketMasks[i][j]);
#endif
                    }
#ifdef VERBOSE_OUTPUT
                    printf("\n");
#endif
                }
#ifdef VERBOSE_OUTPUT
                printf("\n");
#endif

                // Construct media packets.
                for (WebRtc_UWord32 i = 0; i < numMediaPackets; i++)
                {
                    mediaPacket = new ForwardErrorCorrection::Packet;
                    mediaPacketList.PushBack(mediaPacket);
                    mediaPacket->length = static_cast<WebRtc_UWord16>((static_cast<float>(rand()) /
                        RAND_MAX) * (IP_PACKET_SIZE - 12 - 28 -
                        ForwardErrorCorrection::PacketOverhead()));
                    if (mediaPacket->length < 12)
                    {
                        mediaPacket->length = 12;
                    }

                    // Set the RTP version to 2.
                    mediaPacket->data[0] |= 0x80; // Set the 1st bit.
                    mediaPacket->data[0] &= 0xbf; // Clear the 2nd bit.

                    mediaPacket->data[1] &= 0x7f; // Clear marker bit.
                    ModuleRTPUtility::AssignUWord16ToBuffer(&mediaPacket->data[2], seqNum);
                    ModuleRTPUtility::AssignUWord32ToBuffer(&mediaPacket->data[4], timeStamp);
                    ModuleRTPUtility::AssignUWord32ToBuffer(&mediaPacket->data[8], ssrc);

                    for (WebRtc_Word32 j = 12; j < mediaPacket->length; j++)
                    {
                        mediaPacket->data[j] = static_cast<WebRtc_UWord8>((static_cast<float>(rand()) /
                            RAND_MAX) * 255);
                    }

                    seqNum++;
                }
                mediaPacket->data[1] |= 0x80; // Set the marker bit of the last packet.

                WebRtc_UWord8 protectionFactor = static_cast<WebRtc_UWord8>(numFecPackets * 255 / numMediaPackets);
                if (fec.GenerateFEC(mediaPacketList, fecPacketList, protectionFactor) != 0)
                {
                    printf("Error: GenerateFEC() failed\n");
                    return -1;
                }

                if (fecPacketList.GetSize() != numFecPackets)
                {
                    printf("Error: we requested %u FEC packets, but GenerateFEC() produced %u\n",
                        numFecPackets, fecPacketList.GetSize());
                    return -1;
                }

                memset(mediaLossMask, 0, sizeof(mediaLossMask));
                ListItem* mediaPacketListItem = mediaPacketList.First();
                ForwardErrorCorrection::ReceivedPacket* receivedPacket;
                WebRtc_UWord32 mediaPacketIdx = 0;
                while (mediaPacketListItem != NULL)
                {
                    mediaPacket = static_cast<ForwardErrorCorrection::Packet*>
                        (mediaPacketListItem->GetItem());
                    const float lossRandomVariable = (static_cast<float>(rand()) /
                        (RAND_MAX + 1)); // +1 to get [0, 1)
                    if (lossRandomVariable >= lossRate[lossRateIdx])
                    {
                        mediaLossMask[mediaPacketIdx] = 1;
                        receivedPacket = new ForwardErrorCorrection::ReceivedPacket;
                        receivedPacket->pkt = new ForwardErrorCorrection::Packet;
                        receivedPacketList.PushBack(receivedPacket);

                        receivedPacket->pkt->length = mediaPacket->length;
                        memcpy(receivedPacket->pkt->data, mediaPacket->data, mediaPacket->length);
                        receivedPacket->seqNum =
                            ModuleRTPUtility::BufferToUWord16(&mediaPacket->data[2]);
                        receivedPacket->isFec = false;
                        receivedPacket->lastMediaPktInFrame = mediaPacket->data[1] & 0x80 ?
                            true : false; // Check for marker bit.
                    }
                    mediaPacketIdx++;
                    mediaPacketListItem = mediaPacketList.Next(mediaPacketListItem);
                }

                memset(fecLossMask, 0, sizeof(fecLossMask));
                ListItem* fecPacketListItem = fecPacketList.First();
                ForwardErrorCorrection::Packet* fecPacket;
                WebRtc_UWord32 fecPacketIdx = 0;
                while (fecPacketListItem != NULL)
                {
                    fecPacket = static_cast<ForwardErrorCorrection::Packet*>
                        (fecPacketListItem->GetItem());
                    const float lossRandomVariable = (static_cast<float>(rand()) /
                        (RAND_MAX + 1)); // +1 to get [0, 1)
                    if (lossRandomVariable >= lossRate[lossRateIdx])
                    {
                        fecLossMask[fecPacketIdx] = 1;
                        receivedPacket = new ForwardErrorCorrection::ReceivedPacket;
                        receivedPacket->pkt = new ForwardErrorCorrection::Packet;
                        receivedPacketList.PushBack(receivedPacket);

                        receivedPacket->pkt->length = fecPacket->length;
                        memcpy(receivedPacket->pkt->data, fecPacket->data, fecPacket->length);

                        receivedPacket->seqNum = seqNum;
                        receivedPacket->isFec = true;
                        receivedPacket->lastMediaPktInFrame = false;
                        receivedPacket->ssrc = ssrc;

                        fecMaskList.PushBack(fecPacketMasks[fecPacketIdx]);
                    }
                    fecPacketIdx++;
                    seqNum++;
                    fecPacketListItem = fecPacketList.Next(fecPacketListItem);
                }

#ifdef VERBOSE_OUTPUT
                printf("Media loss mask:\n");
                for (WebRtc_UWord32 i = 0; i < numMediaPackets; i++)
                {
                    printf("%u ", mediaLossMask[i]);
                }
                printf("\n\n");

                printf("FEC loss mask:\n");
                for (WebRtc_UWord32 i = 0; i < numFecPackets; i++)
                {
                    printf("%u ", fecLossMask[i]);
                }
                printf("\n\n");
#endif

                ListItem* listItem = fecMaskList.First();
                WebRtc_UWord8* fecMask;
                while (listItem != NULL)
                {
                    fecMask = static_cast<WebRtc_UWord8*>(listItem->GetItem());
                    WebRtc_UWord32 hammingDist = 0;
                    WebRtc_UWord32 recoveryPosition = 0;
                    for (WebRtc_UWord32 i = 0; i < numMediaPackets; i++)
                    {
                        if (mediaLossMask[i] == 0 && fecMask[i] == 1)
                        {
                            recoveryPosition = i;
                            hammingDist++;
                        }
                    }

                    ListItem* itemToDelete = listItem;
                    listItem = fecMaskList.Next(listItem);

                    if (hammingDist == 1)
                    {
                        // Recovery possible. Restart search.
                        mediaLossMask[recoveryPosition] = 1;
                        listItem = fecMaskList.First();
                    }
                    else if (hammingDist == 0)
                    {
                        // FEC packet cannot provide further recovery.
                        fecMaskList.Erase(itemToDelete);
                    }
                }

#ifdef VERBOSE_OUTPUT
                printf("Recovery mask:\n");
                for (WebRtc_UWord32 i = 0; i < numMediaPackets; i++)
                {
                    printf("%u ", mediaLossMask[i]);
                }
                printf("\n\n");
#endif

                bool complete = true; // Marks start of new frame.
                bool fecPacketReceived = false; // For error-checking frame completion.
                while (!receivedPacketList.Empty())
                {
                    WebRtc_UWord32 numPacketsToDecode = static_cast<WebRtc_UWord32>
                        ((static_cast<float>(rand()) / RAND_MAX) * receivedPacketList.GetSize() + 0.5);
                    if (numPacketsToDecode < 1)
                    {
                        numPacketsToDecode = 1;
                    }

                    ReceivePackets(toDecodeList, receivedPacketList, numPacketsToDecode,
                        reorderRate, duplicateRate);

                    if (fecPacketReceived == false)
                    {
                        listItem = toDecodeList.First();
                        while (listItem != NULL)
                        {
                            receivedPacket =
                                static_cast<ForwardErrorCorrection::ReceivedPacket*>
                                (listItem->GetItem());
                            if (receivedPacket->isFec)
                            {
                                fecPacketReceived = true;
                            }

                            listItem = toDecodeList.Next(listItem);
                        }
                    }

                    if (fec.DecodeFEC(toDecodeList, recoveredPacketList, seqNum, complete) != 0)
                    {
                        printf("Error: DecodeFEC() failed\n");
                        return -1;
                    }

                    if (!toDecodeList.Empty())
                    {
                        printf("Error: received packet list is not empty\n");
                        return -1;
                    }

                    if (recoveredPacketList.GetSize() == numMediaPackets &&
                        fecPacketReceived == true)
                    {
                        if (complete == true)
                        {
#ifdef VERBOSE_OUTPUT
                            printf("Full frame recovery correctly marked\n\n");
#endif
                            break;
                        }
                        else
                        {
                            printf("Error: it should be possible to verify full frame recovery,"
                                " but complete parameter was set to false\n");
                            return -1;
                        }
                    }
                    else
                    {
                        if (complete == true)
                        {
                            printf("Error: it should not be possible to verify full frame recovery,"
                                " but complete parameter was set to true\n");
                            return -1;
                        }
                    }
                }

                mediaPacketListItem = mediaPacketList.First();
                mediaPacketIdx = 0;
                while (mediaPacketListItem != NULL)
                {
                    if (mediaLossMask[mediaPacketIdx] == 1)
                    {
                        // Should have recovered this packet.
                        ListItem* recoveredPacketListItem = recoveredPacketList.First();
                        mediaPacket = static_cast<ForwardErrorCorrection::Packet*>
                            (mediaPacketListItem->GetItem());

                        if (recoveredPacketListItem == NULL)
                        {
                            printf("Error: insufficient number of recovered packets.\n");
                            return -1;
                        }

                        ForwardErrorCorrection::RecoveredPacket* recoveredPacket =
                            static_cast<ForwardErrorCorrection::RecoveredPacket*>
                            (recoveredPacketListItem->GetItem());

                        if (recoveredPacket->pkt->length != mediaPacket->length)
                        {
                            printf("Error: recovered packet length not identical to original media packet\n");
                            return -1;
                        }

                        if (memcmp(recoveredPacket->pkt->data, mediaPacket->data,
                            mediaPacket->length) != 0)
                        {
                            printf("Error: recovered packet payload not identical to original media packet\n");
                            return -1;
                        }

                        delete recoveredPacket->pkt;
                        delete recoveredPacket;
                        recoveredPacket = NULL;
                        recoveredPacketList.PopFront();
                    }

                    mediaPacketIdx++;
                    mediaPacketListItem = mediaPacketList.Next(mediaPacketListItem);
                }

                if (!recoveredPacketList.Empty())
                {
                    printf("Error: excessive number of recovered packets.\n");
                    return -1;
                }

                // -- Teardown --
                mediaPacketListItem = mediaPacketList.First();
                while (mediaPacketListItem != NULL)
                {
                    delete static_cast<ForwardErrorCorrection::Packet*>
                        (mediaPacketListItem->GetItem());
                    mediaPacketListItem = mediaPacketList.Next(mediaPacketListItem);
                    mediaPacketList.PopFront();
                }
                assert(mediaPacketList.Empty());

                fecPacketListItem = fecPacketList.First();
                while (fecPacketListItem != NULL)
                {
                    fecPacketListItem = fecPacketList.Next(fecPacketListItem);
                    fecPacketList.PopFront();
                }

                // Delete received packets we didn't pass to DecodeFEC(), due to early
                // frame completion.
                listItem = receivedPacketList.First();
                while (listItem != NULL)
                {
                    receivedPacket =
                        static_cast<ForwardErrorCorrection::ReceivedPacket*>
                        (listItem->GetItem());
                    delete receivedPacket->pkt;
                    delete receivedPacket;
                    receivedPacket = NULL;
                    listItem = receivedPacketList.Next(listItem);
                    receivedPacketList.PopFront();
                }
                assert(receivedPacketList.Empty());

                while (fecMaskList.First() != NULL)
                {
                    fecMaskList.PopFront();
                }

                timeStamp += 90000 / 30;
            }
        }
    }

    // Have DecodeFEC free allocated memory.
    bool complete = true;
    fec.DecodeFEC(receivedPacketList, recoveredPacketList, seqNum, complete);
    if (!recoveredPacketList.Empty())
    {
        printf("Error: recovered packet list is not empty\n");
        return -1;
    }

    printf("\nAll tests passed successfully\n");
    Sleep(5000);

    return 0;
}
