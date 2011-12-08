/**
 * Test application for core FEC algorithm. Calls encoding and decoding functions in
 * ForwardErrorCorrection directly.
 *
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "forward_error_correction.h"
#include "forward_error_correction_internal.h"

#include "list_wrapper.h"
#include "rtp_utility.h"

//#define VERBOSE_OUTPUT

void ReceivePackets(webrtc::ListWrapper& toDecodeList, webrtc::ListWrapper& receivedPacketList,
                    WebRtc_UWord32 numPacketsToDecode, float reorderRate, float duplicateRate);

int main()
{
    enum { kMaxNumberMediaPackets = 48 };
    enum { kMaxNumberFecPackets = 48 };

    const WebRtc_UWord32 kNumMaskBytesL0 = 2;
    const WebRtc_UWord32 kNumMaskBytesL1 = 6;

    // FOR UEP
    const bool kUseUnequalProtection = true;

    WebRtc_UWord32 id = 0;
    webrtc::ForwardErrorCorrection fec(id);

    webrtc::ListWrapper mediaPacketList;
    webrtc::ListWrapper fecPacketList;
    webrtc::ListWrapper toDecodeList;
    webrtc::ListWrapper receivedPacketList;
    webrtc::ListWrapper recoveredPacketList;
    webrtc::ListWrapper fecMaskList;
    webrtc::ForwardErrorCorrection::Packet* mediaPacket;
    const float lossRate[] = {0, 0.05f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f};
    const WebRtc_UWord32 lossRateSize = sizeof(lossRate)/sizeof(*lossRate);
    const float reorderRate = 0.1f;
    const float duplicateRate = 0.1f;

    WebRtc_UWord8 mediaLossMask[kMaxNumberMediaPackets];
    WebRtc_UWord8 fecLossMask[kMaxNumberFecPackets];
    WebRtc_UWord8 fecPacketMasks[kMaxNumberFecPackets][kMaxNumberMediaPackets];

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
        WebRtc_UWord8* packetMask =
            new WebRtc_UWord8[kMaxNumberMediaPackets * kNumMaskBytesL1];

        printf("Loss rate: %.2f\n", lossRate[lossRateIdx]);
        for (WebRtc_UWord32 numMediaPackets = 1; numMediaPackets <= kMaxNumberMediaPackets;
            numMediaPackets++)
        {

            for (WebRtc_UWord32 numFecPackets = 1; numFecPackets <= numMediaPackets &&
                numFecPackets <= kMaxNumberFecPackets; numFecPackets++)
            {

                // loop over all possible numImpPackets
                for (WebRtc_UWord32 numImpPackets = 0; numImpPackets <= numMediaPackets &&
                    numImpPackets <= kMaxNumberMediaPackets; numImpPackets++)
                {

                    WebRtc_UWord8 protectionFactor = static_cast<WebRtc_UWord8>
                        (numFecPackets * 255 / numMediaPackets);

                    const WebRtc_UWord32 maskBytesPerFecPacket =
                        (numMediaPackets > 16) ? kNumMaskBytesL1 :
                                                 kNumMaskBytesL0;

                    memset(packetMask, 0,
                           numMediaPackets * maskBytesPerFecPacket);

                    // Transfer packet masks from bit-mask to byte-mask.
                     webrtc::internal::GeneratePacketMasks(numMediaPackets,
                                                           numFecPackets,
                                                           numImpPackets,
                                                           kUseUnequalProtection,
                                                           packetMask);

#ifdef VERBOSE_OUTPUT
                    printf("%u media packets, %u FEC packets, %u numImpPackets, "
                        "loss rate = %.2f \n",
                        numMediaPackets, numFecPackets, numImpPackets, lossRate[lossRateIdx]);
                    printf("Packet mask matrix \n");
#endif

                    for (WebRtc_UWord32 i = 0; i < numFecPackets; i++)
                    {
                        for (WebRtc_UWord32 j = 0; j < numMediaPackets; j++)
                        {
                            const WebRtc_UWord8 byteMask =
                                packetMask[i * maskBytesPerFecPacket + j / 8];
                            const WebRtc_UWord32 bitPosition = (7 - j % 8);
                            fecPacketMasks[i][j] =
                                (byteMask & (1 << bitPosition)) >> bitPosition;
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
                    // Check for all zero rows or columns: indicates incorrect mask
                    WebRtc_UWord32 rowLimit = numMediaPackets;
                    for (WebRtc_UWord32 i = 0; i < numFecPackets; i++)
                    {
                        WebRtc_UWord32 rowSum = 0;
                        for (WebRtc_UWord32 j = 0; j < rowLimit; j++)
                        {
                            rowSum += fecPacketMasks[i][j];
                        }
                        if (rowSum == 0)
                        {
                            printf("ERROR: row is all zero %d \n",i);
                            return -1;
                        }
                    }
                    for (WebRtc_UWord32 j = 0; j < rowLimit; j++)
                    {
                        WebRtc_UWord32 columnSum = 0;
                        for (WebRtc_UWord32 i = 0; i < numFecPackets; i++)
                        {
                            columnSum += fecPacketMasks[i][j];
                        }
                        if (columnSum == 0)
                        {
                            printf("ERROR: column is all zero %d \n",j);
                            return -1;
                        }
                    }

                    // Construct media packets.
                    for (WebRtc_UWord32 i = 0; i < numMediaPackets; i++)
                    {
                        mediaPacket = new webrtc::ForwardErrorCorrection::Packet;
                        mediaPacketList.PushBack(mediaPacket);
                        mediaPacket->length =
                            static_cast<WebRtc_UWord16>((static_cast<float>(rand()) /
                            RAND_MAX) * (IP_PACKET_SIZE - 12 - 28 -
                            webrtc::ForwardErrorCorrection::PacketOverhead()));
                        if (mediaPacket->length < 12)
                        {
                            mediaPacket->length = 12;
                        }

                        // Generate random values for the first 2 bytes
                        mediaPacket->data[0] =
                            static_cast<WebRtc_UWord8>(rand() % 256);

                        mediaPacket->data[1] =
                            static_cast<WebRtc_UWord8>(rand() % 256);

                        // The first two bits are assumed to be 10 by the
                        // FEC encoder. In fact the FEC decoder will set the
                        // two first bits to 10 regardless of what they
                        // actually were. Set the first two bits to 10
                        // so that a memcmp can be performed for the
                        // whole restored packet.
                        mediaPacket->data[0] |= 0x80;
                        mediaPacket->data[0] &= 0xbf;

                        // FEC is applied to a whole frame.
                        // A frame is signaled by multiple packets without
                        // the marker bit set followed by the last packet of
                        // the frame for which the marker bit is set.
                        // Only push one (fake) frame to the FEC.
                        mediaPacket->data[1] &= 0x7f;

                        webrtc::ModuleRTPUtility::AssignUWord16ToBuffer(&mediaPacket->data[2],
                                                                        seqNum);
                        webrtc::ModuleRTPUtility::AssignUWord32ToBuffer(&mediaPacket->data[4],
                                                                        timeStamp);
                        webrtc::ModuleRTPUtility::AssignUWord32ToBuffer(&mediaPacket->data[8],
                                                                        ssrc);
                        // Generate random values for payload
                        for (WebRtc_Word32 j = 12; j < mediaPacket->length; j++)
                        {
                            mediaPacket->data[j] =
                                static_cast<WebRtc_UWord8> (rand() % 256);
                        }
                        seqNum++;
                    }
                    mediaPacket->data[1] |= 0x80;

                    if (fec.GenerateFEC(mediaPacketList, protectionFactor, numImpPackets,
                        kUseUnequalProtection, fecPacketList) != 0)
                    {
                        printf("Error: GenerateFEC() failed\n");
                        return -1;
                    }

                    if (fecPacketList.GetSize() != numFecPackets)
                    {
                        printf("Error: we requested %u FEC packets, "
                            "but GenerateFEC() produced %u\n",
                            numFecPackets, fecPacketList.GetSize());
                        return -1;
                    }
                    memset(mediaLossMask, 0, sizeof(mediaLossMask));
                    webrtc::ListItem* mediaPacketListItem = mediaPacketList.First();
                    webrtc::ForwardErrorCorrection::ReceivedPacket* receivedPacket;
                    WebRtc_UWord32 mediaPacketIdx = 0;

                    while (mediaPacketListItem != NULL)
                    {
                        mediaPacket = static_cast<webrtc::ForwardErrorCorrection::Packet*>
                            (mediaPacketListItem->GetItem());
                        const float lossRandomVariable = (static_cast<float>(rand()) /
                            (RAND_MAX));

                        if (lossRandomVariable >= lossRate[lossRateIdx])
                        {
                            mediaLossMask[mediaPacketIdx] = 1;
                            receivedPacket =
                                new webrtc::ForwardErrorCorrection::ReceivedPacket;
                            receivedPacket->pkt =
                                new webrtc::ForwardErrorCorrection::Packet;
                            receivedPacketList.PushBack(receivedPacket);

                            receivedPacket->pkt->length = mediaPacket->length;
                            memcpy(receivedPacket->pkt->data, mediaPacket->data,
                                   mediaPacket->length);
                            receivedPacket->seqNum =
                                webrtc::ModuleRTPUtility::BufferToUWord16(&mediaPacket->data[2]);
                            receivedPacket->isFec = false;
                            receivedPacket->lastMediaPktInFrame =
                                (mediaPacket->data[1] & 0x80) != 0;
                        }
                        mediaPacketIdx++;
                        mediaPacketListItem = mediaPacketList.Next(mediaPacketListItem);
                    }
                    memset(fecLossMask, 0, sizeof(fecLossMask));
                    webrtc::ListItem* fecPacketListItem = fecPacketList.First();
                    webrtc::ForwardErrorCorrection::Packet* fecPacket;
                    WebRtc_UWord32 fecPacketIdx = 0;
                    while (fecPacketListItem != NULL)
                    {
                        fecPacket = static_cast<webrtc::ForwardErrorCorrection::Packet*>
                            (fecPacketListItem->GetItem());
                        const float lossRandomVariable =
                            (static_cast<float>(rand()) / (RAND_MAX));
                        if (lossRandomVariable >= lossRate[lossRateIdx])
                        {
                            fecLossMask[fecPacketIdx] = 1;
                            receivedPacket =
                                new webrtc::ForwardErrorCorrection::ReceivedPacket;
                            receivedPacket->pkt =
                                new webrtc::ForwardErrorCorrection::Packet;
                            receivedPacketList.PushBack(receivedPacket);

                            receivedPacket->pkt->length = fecPacket->length;
                            memcpy(receivedPacket->pkt->data, fecPacket->data,
                                fecPacket->length);

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

                    webrtc::ListItem* listItem = fecMaskList.First();
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
                        webrtc::ListItem* itemToDelete = listItem;
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
                            ((static_cast<float>(rand()) / RAND_MAX) *
                            receivedPacketList.GetSize() + 0.5);
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
                                    static_cast<webrtc::ForwardErrorCorrection::
                                    ReceivedPacket*>(listItem->GetItem());
                                if (receivedPacket->isFec)
                                {
                                    fecPacketReceived = true;
                                }
                                listItem = toDecodeList.Next(listItem);
                            }
                        }

                        if (fec.DecodeFEC(toDecodeList, recoveredPacketList, seqNum,
                            complete) != 0)
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
                                printf("Error: "
                                    "it should be possible to verify full frame recovery,"
                                    " but complete parameter was set to false\n");
                                return -1;
                            }
                        }
                        else
                        {
                            if (complete == true)
                            {
                                printf("Error: "
                                    "it should not be possible to verify full frame recovery,"
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
                            webrtc::ListItem* recoveredPacketListItem =
                                recoveredPacketList.First();
                            mediaPacket =
                                static_cast<webrtc::ForwardErrorCorrection::Packet*>
                                (mediaPacketListItem->GetItem());

                            if (recoveredPacketListItem == NULL)
                            {
                                printf("Error: insufficient number of recovered packets.\n");
                                return -1;
                            }

                            webrtc::ForwardErrorCorrection::RecoveredPacket* recoveredPacket =
                                 static_cast<webrtc::ForwardErrorCorrection::RecoveredPacket*>
                                (recoveredPacketListItem->GetItem());

                            if (recoveredPacket->pkt->length != mediaPacket->length)
                            {
                                printf("Error: recovered packet length not identical to "
                                    "original media packet\n");
                                return -1;
                            }
                            if (memcmp(recoveredPacket->pkt->data, mediaPacket->data,
                                mediaPacket->length) != 0)
                            {
                                printf("Error: recovered packet payload not identical to "
                                    "original media packet\n");
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
                        delete static_cast<webrtc::ForwardErrorCorrection::Packet*>
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
                            static_cast<webrtc::ForwardErrorCorrection::ReceivedPacket*>
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
                } //loop over numImpPackets
            } //loop over FecPackets
        } //loop over numMediaPackets
        delete [] packetMask;
    } // loop over loss rates

    // Have DecodeFEC free allocated memory.
    bool complete = true;
    fec.DecodeFEC(receivedPacketList, recoveredPacketList, seqNum, complete);
    if (!recoveredPacketList.Empty())
    {
        printf("Error: recovered packet list is not empty\n");
        return -1;
    }

    printf("\nAll tests passed successfully\n");

    return 0;
}



void ReceivePackets(webrtc::ListWrapper& toDecodeList, webrtc::ListWrapper& receivedPacketList,
                    WebRtc_UWord32 numPacketsToDecode, float reorderRate, float duplicateRate)
{
    assert(toDecodeList.Empty());
    assert(numPacketsToDecode <= receivedPacketList.GetSize());

    webrtc::ListItem* listItem = receivedPacketList.First();
    for (WebRtc_UWord32 i = 0; i < numPacketsToDecode; i++)
    {
        // Reorder packets.
        float randomVariable = static_cast<float>(rand()) / RAND_MAX;
        while (randomVariable < reorderRate)
        {
            webrtc::ListItem* nextItem = receivedPacketList.Next(listItem);
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
        webrtc::ForwardErrorCorrection::ReceivedPacket* receivedPacket =
            static_cast<webrtc::ForwardErrorCorrection::ReceivedPacket*>(listItem->GetItem());
        toDecodeList.PushBack(receivedPacket);

        // Duplicate packets.
        randomVariable = static_cast<float>(rand()) / RAND_MAX;
        while (randomVariable < duplicateRate)
        {
            webrtc::ForwardErrorCorrection::ReceivedPacket* duplicatePacket =
                new webrtc::ForwardErrorCorrection::ReceivedPacket;
            memcpy(duplicatePacket, receivedPacket,
                sizeof(webrtc::ForwardErrorCorrection::ReceivedPacket));

            duplicatePacket->pkt = new webrtc::ForwardErrorCorrection::Packet;
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
