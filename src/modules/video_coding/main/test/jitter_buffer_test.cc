/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_types.h"
#include "jitter_buffer.h"
#include "jitter_estimator.h"
#include "inter_frame_delay.h"
#include "packet.h"
#include "tick_time.h"
#include "../source/event.h"
#include "frame_buffer.h"
#include "jitter_estimate_test.h"
#include "test_util.h"
#include "test_macros.h"
#include <stdio.h>
#include <math.h>

using namespace webrtc;

int CheckOutFrame(VCMEncodedFrame* frameOut, unsigned int size, bool startCode)
{
    if (frameOut == 0)
    {
        return -1;
    }

    const WebRtc_UWord8* outData = frameOut->Buffer();

    unsigned int i = 0;

    if(startCode)
    {
        TEST(outData[0] == 0);
        TEST(outData[1] == 0);
        TEST(outData[2] == 0);
        TEST(outData[3] == 1);
        i+= 4;
    }

    // check the frame data
    int count = 3;

    // check the frame length
    TEST(frameOut->Length() == size);

    for(; i < size; i++)
    {
        if (outData[i] == 0 && outData[i+1] == 0 && outData[i+2] == 0x80)
        {
            i += 2;
        }
        else if(startCode && outData[i] == 0 && outData[i+1] == 0)
        {
            TEST(outData[i] == 0);
            TEST(outData[i+1] == 0);
            TEST(outData[i+2] == 0);
            TEST(outData[i+3] == 1);
            i += 3;
        }
        else
        {
            TEST(outData[i] == count);
            count++;
            if(count == 10)
            {
                count = 3;
            }
        }
    }
    return 0;
}


int JitterBufferTest(CmdArgs& args)
{
    // Don't run these tests with debug time
#if defined(TICK_TIME_DEBUG) || defined(EVENT_DEBUG)
    return -1;
#endif

    // Start test
    WebRtc_UWord16 seqNum = 1234;
    WebRtc_UWord32 timeStamp = 0;
    int size = 1400;
    WebRtc_UWord8 data[1500];
    VCMPacket packet(data, size, seqNum, timeStamp, true);

    VCMFrameListTimestampOrderAsc frameList;
    VCMFrameBuffer* fb = NULL;
    for (int i=0; i < 100; i++)
    {
        fb = new VCMFrameBuffer();
        fb->SetState(kStateEmpty);
        packet.timestamp = 0xfffffff0 + i;
        packet.seqNum = seqNum;
        packet.payloadType = 126;
        seqNum++;
        fb->InsertPacket(packet, VCMTickTime::MillisecondTimestamp());
        TEST(frameList.Insert(fb) == 0);
    }
    VCMFrameListItem* item = NULL;
    WebRtc_UWord32 prevTimestamp = 0;
    int i = 0;
    for (i=0; !frameList.Empty(); i++)
    {
        item = frameList.First();
        fb = static_cast<VCMFrameBuffer*>(item->GetItem());
        TEST(i > 0 || fb->TimeStamp() == 0xfffffff0); // Frame 0 has no prev
        TEST(prevTimestamp - fb->TimeStamp() == static_cast<WebRtc_UWord32>(-1)
             || i == 0);
        prevTimestamp = fb->TimeStamp();
        frameList.Erase(item);
        delete fb;
    }
    TEST(i == 100);

    //printf("DONE timestamp ordered frame list\n");

    VCMJitterBuffer jb;

    seqNum = 1234;
    timeStamp = 123*90;
    FrameType incomingFrameType(kVideoFrameKey);
    VCMEncodedFrame* frameOut=NULL;
    WebRtc_Word64 renderTimeMs = 0;
    packet.timestamp = timeStamp;
    packet.seqNum = seqNum;

    // build a data vector with 0, 0, 0x80, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0x80, 3....
    data[0] = 0;
    data[1] = 0;
    data[2] = 0x80;
    int count = 3;
    for (unsigned int i = 3; i < sizeof(data) - 3; ++i)
    {
        data[i] = count;
        count++;
        if(count == 10)
        {
            data[i+1] = 0;
            data[i+2] = 0;
            data[i+3] = 0x80;
            count = 3;
            i += 3;
        }
    }

    // Test out of range inputs
    TEST(kSizeError == jb.InsertPacket(0, packet));
    jb.ReleaseFrame(0);

    // Not started
    TEST(0 == jb.GetFrame(packet));
    TEST(-1 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(0 == jb.GetCompleteFrameForDecoding(10));
    TEST(0 == jb.GetFrameForDecoding());

    // Start
    jb.Start();

    // Get frame to use for this timestamp
    VCMEncodedFrame* frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // No packets inserted
    TEST(0 == jb.GetCompleteFrameForDecoding(10));


    //
    // TEST single packet frame
    //
    //  --------
    // |  1234  |
    //  --------

    // packet.frameType;
    // packet.dataPtr;
    // packet.sizeBytes;
    // packet.timestamp;
    // packet.seqNum;
    // packet.isFirstPacket;
    // packet.markerBit;
    //
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 1 packet\n");

    //
    // TEST dual packet frame
    //
    //  -----------------
    // |  1235  |  1236  |
    //  -----------------
    //

    seqNum++;
    timeStamp += 33*90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 2 packets\n");


    //
    // TEST 100 packets frame Key frame
    //
    //  ----------------------------------
    // |  1237  |  1238  |  .... |  1336  |
    //  ----------------------------------

    // insert first packet
    timeStamp += 33*90;
    seqNum++;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameKey);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert 98 frames
    int loop = 0;
    do
    {
        seqNum++;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert a packet into a frame
        TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
        loop++;
    } while (loop < 98);

    // insert last packet
    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*100, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameKey);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE key frame 100 packets\n");

    //
    // TEST 100 packets frame Delta frame
    //
    //  ----------------------------------
    // |  1337  |  1238  |  .... |  1436  |
    //  ----------------------------------

    // insert first packet
    timeStamp += 33*90;
    seqNum++;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert 98 frames
    loop = 0;
    do
    {
        seqNum++;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert a packet into a frame
        TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
        loop++;
    } while (loop < 98);

    // insert last packet
    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*100, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 100 packets\n");

    //
    // TEST packet re-ordering reverse order
    //
    //  ----------------------------------
    // |  1437  |  1238  |  .... |  1536  |
    //  ----------------------------------
    //            <----------

    // insert "first" packet last seqnum
    timeStamp += 33*90;
    seqNum += 100;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert 98 packets
    loop = 0;
    do
    {
        seqNum--;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert a packet into a frame
        TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
        loop++;
    } while (loop < 98);

    // insert last packet
    seqNum--;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*100, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 100 packets reverse order\n");

    seqNum+= 100;

    //
    // TEST frame re-ordering 2 frames 2 packets each
    //
    //  -----------------     -----------------
    // |  1539  |  1540  |   |  1537  |  1538  |
    //  -----------------     -----------------

    seqNum += 2;
    timeStamp += 2* 33 * 90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // check that we fail to get frame since seqnum is not continuous
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(frameOut == 0);

    seqNum -= 3;
    timeStamp -= 33*90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    seqNum += 2;
    //printf("DONE frame re-ordering 2 frames 2 packets\n");

    //
    // TEST H.263 bits
    //
    //  -----------------
    // |  1541  |  1542  |
    //  -----------------
    //            sBits

    seqNum++;
    timeStamp += 2*33*90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.bits = false;
    packet.codec = kVideoCodecH263;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.bits = true;
    packet.dataPtr = &(data[9]);

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, (size*2)-1, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    // restore
    packet.dataPtr = data;
    packet.bits = false;
    packet.codec = kVideoCodecUnknown;
    //printf("DONE H.263 frame 2 packets with bits\n");

    //
    // TEST duplicate packets
    //
    //  -----------------
    // |  1543  |  1543  |
    //  -----------------
    //

    seqNum++;
    timeStamp += 33*90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    packet.isFirstPacket = false;
    packet.markerBit = true;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kDuplicatePacket == jb.InsertPacket(frameIn, packet));

    seqNum++;
    packet.seqNum = seqNum;

    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE test duplicate packets\n");

    //
    // TEST H.264 insert start code
    //
    //  -----------------
    // |  1544  |  1545  |
    //  -----------------
    // insert start code, both packets

    seqNum++;
    timeStamp += 33*90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.insertStartCode = true;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2+4*2, true) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    // reset
    packet.insertStartCode = false;
    //printf("DONE H.264 insert start code test 2 packets\n");

    //
    // TEST statistics
    //
    WebRtc_UWord32 numDeltaFrames = 0;
    WebRtc_UWord32 numKeyFrames = 0;
    TEST(jb.GetFrameStatistics(numDeltaFrames, numKeyFrames) == 0);

    TEST(numDeltaFrames == 9);
    TEST(numKeyFrames == 1);

    WebRtc_UWord32 frameRate;
    WebRtc_UWord32 bitRate;
    TEST(jb.GetUpdate(frameRate, bitRate) == 0);

    // these depend on CPU speed works on a T61
    TEST(frameRate > 30);
    TEST(bitRate > 10000000);


    jb.Flush();

    //
    // TEST packet loss. Verify missing packets statistics and not decodable
    // packets statistics.
    // Insert 10 frames consisting of 4 packets and remove one from all of them.
    // The last packet is an empty (non-media) packet
    //

    // Select a start seqNum which triggers a difficult wrap situation
    seqNum = 0xffff - 4;
    for (int i=0; i < 10; ++i) {
      webrtc::FrameType frametype = kVideoFrameDelta;
      if (i == 0)
        frametype = kVideoFrameKey;
      seqNum++;
      timeStamp += 33*90;
      packet.frameType = frametype;
      if (i == 0)
        packet.frameType = frametype;
      packet.isFirstPacket = true;
      packet.markerBit = false;
      packet.seqNum = seqNum;
      packet.timestamp = timeStamp;
      packet.completeNALU = kNaluStart;

      frameIn = jb.GetFrame(packet);
      TEST(frameIn != 0);

      // Insert a packet into a frame
      TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

      // Get packet notification
      TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

      // Check incoming frame type
      TEST(incomingFrameType == frametype);

      // Get the frame
      frameOut = jb.GetCompleteFrameForDecoding(10);

      // Should not be complete
      TEST(frameOut == 0);

      seqNum += 2;
      packet.isFirstPacket = false;
      packet.markerBit = true;
      packet.seqNum = seqNum;
      packet.completeNALU = kNaluEnd;

      frameIn = jb.GetFrame(packet);
      TEST(frameIn != 0);

      // Insert a packet into a frame
      TEST(kIncomplete == jb.InsertPacket(frameIn, packet));


      // Insert an empty (non-media) packet
      seqNum++;
      packet.isFirstPacket = false;
      packet.markerBit = false;
      packet.seqNum = seqNum;
      packet.completeNALU = kNaluEnd;
      packet.frameType = kFrameEmpty;

      frameIn = jb.GetFrame(packet);
      TEST(frameIn != 0);

      // Insert a packet into a frame
      TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

      // Get the frame
      frameOut = jb.GetFrameForDecoding();

      // One of the packets has been discarded by the jitter buffer
      TEST(CheckOutFrame(frameOut, size, false) == 0);

      // check the frame type
      TEST(frameOut->FrameType() == frametype);
      TEST(frameOut->Complete() == false);
      TEST(frameOut->MissingFrame() == false);

      // Release frame (when done with decoding)
      jb.ReleaseFrame(frameOut);
    }

    TEST(jb.NumNotDecodablePackets() == 10);

    // Insert 3 old packets and verify that we have 3 discarded packets
    packet.timestamp = timeStamp - 1000;
    frameIn = jb.GetFrame(packet);
    TEST(frameIn == NULL);

    packet.timestamp = timeStamp - 500;
    frameIn = jb.GetFrame(packet);
    TEST(frameIn == NULL);

    packet.timestamp = timeStamp - 100;
    frameIn = jb.GetFrame(packet);
    TEST(frameIn == NULL);

    TEST(jb.DiscardedPackets() == 3);

    jb.Flush();

    // This statistic shouldn't be reset by a flush.
    TEST(jb.DiscardedPackets() == 3);

    //printf("DONE Statistics\n");


    // Temporarily do this to make the rest of the test work:
    timeStamp += 33*90;
    seqNum += 4;


    //
    // TEST delta frame 100 packets with seqNum wrap
    //
    //  ---------------------------------------
    // |  65520  |  65521  | ... |  82  |  83  |
    //  ---------------------------------------
    //

    jb.Flush();

    // insert first packet
    timeStamp += 33*90;
    seqNum = 0xfff0;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert 98 packets
    loop = 0;
    do
    {
        seqNum++;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert a packet into a frame
        TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

        // get packet notification
        TEST(timeStamp == jb.GetNextTimeStamp(2, incomingFrameType, renderTimeMs));

        // check incoming frame type
        TEST(incomingFrameType == kVideoFrameDelta);

        // get the frame
        frameOut = jb.GetCompleteFrameForDecoding(2);

        // it should not be complete
        TEST(frameOut == 0);

        loop++;
    } while (loop < 98);

    // insert last packet
    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*100, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 100 packets with wrap in seqNum\n");

    //
    // TEST packet re-ordering reverse order with neg seqNum warp
    //
    //  ----------------------------------------
    // |  65447  |  65448  | ... |   9   |  10  |
    //  ----------------------------------------
    //              <-------------

    // test flush
    jb.Flush();

    // insert "first" packet last seqnum
    timeStamp += 33*90;
    seqNum = 10;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert 98 frames
    loop = 0;
    do
    {
        seqNum--;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert a packet into a frame
        TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

        // get packet notification
        TEST(timeStamp == jb.GetNextTimeStamp(2, incomingFrameType, renderTimeMs));

        // check incoming frame type
        TEST(incomingFrameType == kVideoFrameDelta);

        // get the frame
        frameOut = jb.GetCompleteFrameForDecoding(2);

        // it should not be complete
        TEST(frameOut == 0);

        loop++;
    } while (loop < 98);

    // insert last packet
    seqNum--;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*100, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 100 packets reverse order with wrap in seqNum \n");

    // test flush
    jb.Flush();

    //
    // TEST packet re-ordering with seqNum wrap
    //
    //  -----------------------
    // |   1   | 65535 |   0   |
    //  -----------------------

    // insert "first" packet last seqnum
    timeStamp += 33*90;
    seqNum = 1;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert last packet
    seqNum -= 2;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = false;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*3, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE delta frame 3 packets re-ordering with wrap in seqNum \n");

    // test flush
    jb.Flush();

    //
    // TEST insert old frame
    //
    //   -------      -------
    //  |   2   |    |   1   |
    //   -------      -------
    //  t = 3000     t = 2000

    seqNum = 2;
    timeStamp = 3000;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(3000 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(3000 == frameOut->TimeStamp());

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    seqNum--;
    timeStamp = 2000;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    // Changed behavior, never insert packets into frames older than the
    // last decoded frame.
    TEST(frameIn == 0);

    //printf("DONE insert old frame\n");

    jb.Flush();

   //
    // TEST insert old frame with wrap in timestamp
    //
    //   -------      -------
    //  |   2   |    |   1   |
    //   -------      -------
    //  t = 3000     t = 0xffffff00

    seqNum = 2;
    timeStamp = 3000;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(timeStamp == frameOut->TimeStamp());

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    seqNum--;
    timeStamp = 0xffffff00;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    // This timestamp is old
    TEST(frameIn == 0);

    jb.Flush();

    //
    // TEST wrap in timeStamp
    //
    //  ---------------     ---------------
    // |   1   |   2   |   |   3   |   4   |
    //  ---------------     ---------------
    //  t = 0xffffff00        t = 33*90

    seqNum = 1;
    timeStamp = 0xffffff00;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2, false) == 0);

    seqNum++;
    timeStamp += 33*90;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size*2, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    //printf("DONE time stamp wrap 2 frames 2 packets\n");

    jb.Flush();

    //
    // TEST insert 2 frames with wrap in timeStamp
    //
    //   -------          -------
    //  |   1   |        |   2   |
    //   -------          -------
    // t = 0xffffff00    t = 2700

    seqNum = 1;
    timeStamp = 0xffffff00;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert first frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // Get packet notification
    TEST(0xffffff00 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Insert next frame
    seqNum++;
    timeStamp = 2700;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // Get packet notification
    TEST(0xffffff00 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Get frame
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(0xffffff00 == frameOut->TimeStamp());

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // Get packet notification
    TEST(2700 == jb.GetNextTimeStamp(0, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Get frame
    VCMEncodedFrame* frameOut2 = jb.GetCompleteFrameForDecoding(10);
    TEST(2700 == frameOut2->TimeStamp());

    TEST(CheckOutFrame(frameOut2, size, false) == 0);

    // check the frame type
    TEST(frameOut2->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);
    jb.ReleaseFrame(frameOut2);

    //printf("DONE insert 2 frames (1 packet) with wrap in timestamp\n");

    jb.Flush();

    //
    // TEST insert 2 frames re-ordered with wrap in timeStamp
    //
    //   -------          -------
    //  |   2   |        |   1   |
    //   -------          -------
    //  t = 2700        t = 0xffffff00

    seqNum = 2;
    timeStamp = 2700;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert first frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // Get packet notification
    TEST(2700 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Insert second frame
    seqNum--;
    timeStamp = 0xffffff00;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // Get packet notification
    TEST(0xffffff00 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Get frame
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(0xffffff00 == frameOut->TimeStamp());

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameDelta);

    // get packet notification
    TEST(2700 == jb.GetNextTimeStamp(0, incomingFrameType, renderTimeMs));
    TEST(kVideoFrameDelta == incomingFrameType);

    // Get frame
    frameOut2 = jb.GetCompleteFrameForDecoding(10);
    TEST(2700 == frameOut2->TimeStamp());

    TEST(CheckOutFrame(frameOut2, size, false) == 0);

    // check the frame type
    TEST(frameOut2->FrameType() == kVideoFrameDelta);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);
    jb.ReleaseFrame(frameOut2);

    //printf("DONE insert 2 frames (1 packet) re-ordered with wrap in timestamp\n");

    //
    // TEST NACK
    //
    //  ---------------------------------------------------------------------------------------------
    // | 3 | 4 | 5 | 6 | 7 | 9 | x | 11 | 12 | 13 | 14 | 15 | 16 | 17 | 18 | 19 | x | 21 |.....| 102 |
    //  ---------------------------------------------------------------------------------------------
    jb.SetNackMode(kNackInfinite);

    TEST(jb.GetNackMode() == kNackInfinite);

    // insert first packet
    timeStamp += 33*90;
    seqNum += 2;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameKey);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert 98 packets
    loop = 0;
    do
    {
        seqNum++;
        if(seqNum % 10 != 0)
        {
            packet.isFirstPacket = false;
            packet.markerBit = false;
            packet.seqNum = seqNum;

            frameIn = jb.GetFrame(packet);
            TEST(frameIn != 0);

            // Insert a packet into a frame
            TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
        }
        loop++;
    } while (loop < 98);

    // insert last packet
    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

    // try to get the frame, should fail
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(frameOut == 0);

    // try to get the frame, should fail
    frameOut = jb.GetFrameForDecoding();
    TEST(frameOut == 0);

    WebRtc_UWord16 nackSize = 0;
    bool extended = false;
    WebRtc_UWord16* list = jb.GetNackList(nackSize, extended);

    TEST(nackSize == 10);

    for(int i = 0; i <  nackSize; i++)
    {
        TEST(list[i] == (1+i)*10);
    }

    jb.Stop();

    //printf("DONE NACK\n");

    //
    // TEST NACK with wrap in seqNum
    //
    //  -------   -----------------------------------------------------------------------------------
    // | 65532 | | 65533 | 65534 | 65535 | x | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | x | 11 |.....| 96 |
    //  -------   -----------------------------------------------------------------------------------

    jb.Flush();
    jb.Start();

    // insert first frame
    timeStamp = 33*90;
    seqNum = 65532;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameKey);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameKey);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);


    // insert first packet
    timeStamp += 33*90;
    seqNum++;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameDelta);

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);

    // it should not be complete
    TEST(frameOut == 0);

    // insert 98 packets
    loop = 0;
    do
    {
        seqNum++;
        if (seqNum % 10 != 0)
        {
            packet.isFirstPacket = false;
            packet.markerBit = false;
            packet.seqNum = seqNum;

            frameIn = jb.GetFrame(packet);
            TEST(frameIn != 0);

            // Insert a packet into a frame
            TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

            // try to get the frame, should fail
            frameOut = jb.GetCompleteFrameForDecoding(1);
            TEST(frameOut == 0);

            // try to get the frame, should fail
            frameOut = jb.GetFrameForDecoding();
            TEST(frameOut == 0);
        }
        loop++;
    } while (loop < 98);

    // insert last packet
    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert a packet into a frame
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

    // try to get the frame, should fail
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(frameOut == 0);

    // try to get the frame, should fail
    frameOut = jb.GetFrameForDecoding();
    TEST(frameOut == 0);

    nackSize = 0;
    list = jb.GetNackList(nackSize, extended);

    TEST(nackSize == 10);

    for(int i = 0; i <  nackSize; i++)
    {
        TEST(list[i] == i*10);
    }

    jb.Stop();

    //printf("DONE NACK with wrap in seqNum\n");

    //
    // TEST delta frame with more than max number of packets
    //

    jb.Start();

    loop = 0;
    packet.timestamp += 33*90;
    bool firstPacket = true;
    // insert kMaxPacketsInJitterBuffer into frame
    do
    {
        seqNum++;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert frame
        if (firstPacket)
        {
            TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));
            firstPacket = false;
        }
        else
        {
            TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
        }

        // get packet notification
        TEST(packet.timestamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

        // check incoming frame type
        TEST(incomingFrameType == kVideoFrameDelta);

        loop++;
    } while (loop < kMaxPacketsInJitterBuffer);

    // Max number of packets inserted

    // Insert one more packet
    seqNum++;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    frameIn = jb.GetFrame(packet);
    TEST(frameIn != 0);

    // Insert the packet -> frame recycled
    TEST(kSizeError == jb.InsertPacket(frameIn, packet));

    // should fail
    TEST(-1 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(0 == jb.GetCompleteFrameForDecoding(10));

    //printf("DONE fill frame - packets > max number of packets\n");

    //
    // TEST fill JB with more than max number of delta frame
    //

    loop = 0;
    WebRtc_UWord32 timeStampStart = timeStamp + 33*90;
    // insert MAX_NUMBER_OF_FRAMES frames
    do
    {
        timeStamp += 33*90;
        seqNum++;
        packet.isFirstPacket = true;
        packet.markerBit = true;
        packet.seqNum = seqNum;
        packet.timestamp = timeStamp;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        // Insert frame
        TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

        // get packet notification
        TEST(timeStampStart == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));

        // check incoming frame type
        TEST(incomingFrameType == kVideoFrameDelta);

        loop++;
    } while (loop < kMaxNumberOfFrames);

    // Max number of frames inserted

    // Insert one more frame
    timeStamp += 33*90;
    seqNum++;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;

    // Now, no free frame - frames will be recycled until first key frame
    frameIn = jb.GetFrame(packet);
    TEST(frameIn);  // no key, so we have recycled all frames

    //printf("DONE fill JB - number of delta frames > max number of frames\n");

    //
    // TEST fill JB with more than max number of frame (50 delta frames +
    // 51 key frames) with wrap in seqNum
    //
    //  --------------------------------------------------------------
    // | 65485 | 65486 | 65487 | .... | 65535 | 0 | 1 | 2 | .....| 50 |
    //  --------------------------------------------------------------
    // |<-----------delta frames------------->|<------key frames----->|

    jb.Flush();

    loop = 0;
    seqNum = 65485;
    timeStampStart = timeStamp +  33*90;
    WebRtc_UWord32 timeStampFirstKey = 0;
    VCMEncodedFrame* ptrLastDeltaFrame;
    VCMEncodedFrame* ptrFirstKeyFrame;
    // insert MAX_NUMBER_OF_FRAMES frames
    do
    {
        timeStamp += 33*90;
        seqNum++;
        packet.isFirstPacket = true;
        packet.markerBit = true;
        packet.seqNum = seqNum;
        packet.timestamp = timeStamp;

        frameIn = jb.GetFrame(packet);
        TEST(frameIn != 0);

        if (loop == 49)  // last delta
        {
            ptrLastDeltaFrame = frameIn;
        }
        if (loop == 50)  // first key
        {
            ptrFirstKeyFrame = frameIn;
            packet.frameType = kVideoFrameKey;
            timeStampFirstKey = packet.timestamp;
        }

        // Insert frame
        TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

        // Get packet notification, should be first inserted frame
        TEST(timeStampStart == jb.GetNextTimeStamp(10, incomingFrameType,
                                                   renderTimeMs));

        // check incoming frame type
        TEST(incomingFrameType == kVideoFrameDelta);

        loop++;
    } while (loop < kMaxNumberOfFrames);

    // Max number of frames inserted

    // Insert one more frame
    timeStamp += 33*90;
    seqNum++;
    packet.isFirstPacket = true;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;

    // Now, no free frame - frames will be recycled until first key frame
    frameIn = jb.GetFrame(packet);
    // ptr to last inserted delta frame should be returned
    TEST(frameIn != 0 && frameIn && ptrLastDeltaFrame);

    // Insert frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    // First inserted key frame should be oldest in buffer
    TEST(timeStampFirstKey == jb.GetNextTimeStamp(10, incomingFrameType,
                                                  renderTimeMs));

    // check incoming frame type
    TEST(incomingFrameType == kVideoFrameKey);

    // get the first key frame
    frameOut = jb.GetCompleteFrameForDecoding(10);
    TEST(ptrFirstKeyFrame == frameOut);

    TEST(CheckOutFrame(frameOut, size, false) == 0);

    // check the frame type
    TEST(frameOut->FrameType() == kVideoFrameKey);

    // Release frame (when done with decoding)
    jb.ReleaseFrame(frameOut);

    jb.Flush();

    // printf("DONE fill JB - nr of delta + key frames (w/ wrap in seqNum) >
    // max nr of frames\n");

    // Test handling empty packets
    // first insert 2 empty packets
    jb.ReleaseFrame(frameIn);
    timeStamp = 33 * 90;
    seqNum = 5;
    packet.isFirstPacket = false;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kFrameEmpty;
    frameIn = jb.GetFrame(packet);
    TEST(frameIn);
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    seqNum = 6;
    packet.isFirstPacket = false;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kFrameEmpty;
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
    // now insert the first data packet
    seqNum = 1;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kVideoFrameDelta;
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
    // insert an additional data packet
    seqNum = 2;
    packet.isFirstPacket = false;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kVideoFrameDelta;
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

    // insert the last packet and verify frame completness
    // (even though packet 4 (empty) is missing)
    seqNum = 3;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kVideoFrameDelta;
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));
    jb.Flush();

    // testing that empty packets do not clog the jitter buffer
    // Set hybrid mode
    jb.SetNackMode(kNackHybrid);
    TEST(jb.GetNackMode() == kNackHybrid);

    int maxSize = 100;
    seqNum = 3;
    VCMEncodedFrame* testFrame;
    for (int i = 0; i < maxSize + 10; i++)
    {
        timeStamp += 33 * 90;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;
        packet.timestamp = timeStamp;
        packet.frameType = kFrameEmpty;
        testFrame = jb.GetFrame(packet);
        TEST(frameIn != 0);
        TEST(kFirstPacket == jb.InsertPacket(testFrame, packet));
    }
    // verify insertion of a data packet (old empty frames will be flushed)
    timeStamp += 33 * 90;
    packet.isFirstPacket = true;
    packet.markerBit = false;
    packet.seqNum = seqNum;
    packet.timestamp = timeStamp;
    packet.frameType = kFrameEmpty;
    testFrame = jb.GetFrame(packet);
    TEST(frameIn != 0);

    jb.SetNackMode(kNoNack);
    jb.Flush();

    // Testing that 1 empty packet inserted last will not be set for decoding
    seqNum = 3;
    // Insert one empty packet per frame, should never return the last timestamp
    // inserted. Only return empty frames in the presence of subsequent frames.
    maxSize = 1000;
    for (int i = 0; i < maxSize + 10; i++)
    {
        timeStamp += 33 * 90;
        seqNum++;
        packet.isFirstPacket = false;
        packet.markerBit = false;
        packet.seqNum = seqNum;
        packet.timestamp = timeStamp;
        packet.frameType = kFrameEmpty;
        testFrame = jb.GetFrameForDecoding();
        // timestamp should bever be the last TS inserted
        if (testFrame != NULL)
        {
            TEST(testFrame->TimeStamp() < timeStamp);
            printf("Not null TS = %d\n",testFrame->TimeStamp());
        }
    }

    jb.Flush();


    // printf(DONE testing inserting empty packets to the JB)


    // H.264 tests
    // Test incomplete NALU frames

    jb.Flush();
    jb.SetNackMode(kNoNack);
    seqNum ++;
    timeStamp += 33*90;
    int insertedLength=0;
    packet.seqNum=seqNum;
    packet.timestamp=timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = true;
    packet.completeNALU=kNaluStart;
    packet.markerBit=false;

    frameIn=jb.GetFrame(packet);

     // Insert a packet into a frame
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    seqNum+=2; // Skip one packet
    packet.seqNum=seqNum;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = false;
    packet.completeNALU=kNaluIncomplete;
    packet.markerBit=false;

     // Insert a packet into a frame
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

    seqNum++;
    packet.seqNum=seqNum;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = false;
    packet.completeNALU=kNaluEnd;
    packet.markerBit=false;

    // Insert a packet into a frame
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));

    seqNum++;
    packet.seqNum=seqNum;
    packet.completeNALU=kNaluComplete;
    packet.markerBit=true; // Last packet
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));


    // get packet notification
    TEST(timeStamp == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    frameOut = jb.GetFrameForDecoding();

    // We can decode everything from a NALU until a packet has been lost.
    // Thus we can decode the first packet of the first NALU and the second NALU
    // which consists of one packet.
    TEST(CheckOutFrame(frameOut, packet.sizeBytes * 2, false) == 0);
    jb.ReleaseFrame(frameOut);

    // Test reordered start frame + 1 lost
    seqNum +=2; // Reoreder 1 frame
    timeStamp += 33*90;
    insertedLength=0;

    packet.seqNum=seqNum;
    packet.timestamp=timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = false;
    packet.completeNALU=kNaluEnd;
    packet.markerBit=false;

    TEST(frameIn=jb.GetFrame(packet));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));
    insertedLength+=packet.sizeBytes; // This packet should be decoded

    seqNum--;
    packet.seqNum=seqNum;
    packet.timestamp=timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = true;
    packet.completeNALU=kNaluStart;
    packet.markerBit=false;
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
    insertedLength+=packet.sizeBytes; // This packet should be decoded

    seqNum+=3; // One packet drop
    packet.seqNum=seqNum;
    packet.timestamp=timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = false;
    packet.completeNALU=kNaluComplete;
    packet.markerBit=false;
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
    insertedLength+=packet.sizeBytes; // This packet should be decoded

    seqNum+=1;
    packet.seqNum=seqNum;
    packet.timestamp=timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = false;
    packet.completeNALU=kNaluStart;
    packet.markerBit=false;
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
    // This packet should be decoded since it's the beginning of a NAL
    insertedLength+=packet.sizeBytes;

    seqNum+=2;
    packet.seqNum=seqNum;
    packet.timestamp=timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = false;
    packet.completeNALU=kNaluEnd;
    packet.markerBit=true;
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
    // This packet should not be decoded because it is an incomplete NAL if it
    // is the last
    insertedLength+=0;

    frameOut = jb.GetFrameForDecoding();
    // Only last NALU is complete
    TEST(CheckOutFrame(frameOut, insertedLength, false) == 0);
    jb.ReleaseFrame(frameOut);


    //Test to insert empty packet
    seqNum+=1;
    timeStamp += 33*90;
    VCMPacket emptypacket(data, 0, seqNum, timeStamp, true);
    emptypacket.seqNum=seqNum;
    emptypacket.timestamp=timeStamp;
    emptypacket.frameType = kVideoFrameKey;
    emptypacket.isFirstPacket = true;
    emptypacket.completeNALU=kNaluComplete;
    emptypacket.markerBit=true;
    TEST(frameIn=jb.GetFrame(emptypacket));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, emptypacket));
    // This packet should not be decoded because it is an incomplete NAL if it
    // is the last
    insertedLength+=0;

    TEST(-1 == jb.GetNextTimeStamp(10, incomingFrameType, renderTimeMs));
    TEST(NULL==jb.GetFrameForDecoding());


    // Test that a frame can include an empty packet.
    seqNum+=1;
    timeStamp += 33*90;

    packet.seqNum=seqNum;
    packet.timestamp=timeStamp;
    packet.frameType = kVideoFrameKey;
    packet.isFirstPacket = true;
    packet.completeNALU=kNaluComplete;
    packet.markerBit=false;
    TEST(frameIn=jb.GetFrame(packet));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    seqNum+=1;
    emptypacket.seqNum=seqNum;
    emptypacket.timestamp=timeStamp;
    emptypacket.frameType = kVideoFrameKey;
    emptypacket.isFirstPacket = true;
    emptypacket.completeNALU=kNaluComplete;
    emptypacket.markerBit=true;
    TEST(kCompleteSession == jb.InsertPacket(frameIn, emptypacket));

    // get the frame
    frameOut = jb.GetCompleteFrameForDecoding(10);
    // Only last NALU is complete
    TEST(CheckOutFrame(frameOut, packet.sizeBytes, false) == 0);
    jb.Flush();

    // Three reordered H263 packets with bits.

    packet.codec = kVideoCodecH263;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = false;
    packet.markerBit = false;
    packet.bits = true;
    packet.seqNum += 1;
    WebRtc_UWord8 oldData1 = data[0];
    WebRtc_UWord8 oldData2 = data[packet.sizeBytes - 1];
    unsigned char startByte = 0x07;
    unsigned char endByte = 0xF8;
    data[0] = startByte;
    TEST(frameIn = jb.GetFrame(packet));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));
    frameOut = jb.GetFrameForDecoding();
    TEST(frameOut == NULL);

    packet.seqNum -= 1;
    packet.isFirstPacket = true;
    packet.bits = false;
    data[0] = oldData1;
    data[packet.sizeBytes - 1] = endByte;
    TEST(frameIn = jb.GetFrame(packet));
    TEST(kIncomplete == jb.InsertPacket(frameIn, packet));
    frameOut = jb.GetFrameForDecoding();
    TEST(frameOut == NULL);

    packet.seqNum += 2;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    data[packet.sizeBytes - 1] = oldData2;
    TEST(frameIn = jb.GetFrame(packet));
    TEST(kCompleteSession == jb.InsertPacket(frameIn, packet));
    frameOut = jb.GetFrameForDecoding();
    TEST(frameOut != NULL);
    //CheckOutFrame(frameOut, packet.sizeBytes * 3 - 1, false);
    const WebRtc_UWord8* buf = frameOut->Buffer();
    TEST(buf[packet.sizeBytes - 1] == (startByte | endByte));

    // First packet lost, second packet with bits.

    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = false;
    packet.markerBit = true;
    packet.bits = true;
    packet.seqNum += 2;
    packet.timestamp += 33*90;
    data[0] = 0x07;
    data[packet.sizeBytes - 1] = 0xF8;
    //unsigned char startByte = packet.dataPtr[0];
    //unsigned char endByte = packet.dataPtr[packet.sizeBytes-1];
    TEST(frameIn = jb.GetFrame(packet));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));
    frameOut = jb.GetFrameForDecoding();
    TEST(frameOut != NULL);
    TEST(frameOut->Length() == 0);

    data[0] = oldData1;
    data[packet.sizeBytes - 1] = oldData2;
    packet.codec = kVideoCodecUnknown;
    jb.Flush();

    // Test that a we cannot get incomplete frames from the JB if we haven't received
    // the marker bit, unless we have received a packet from a later timestamp.

    packet.seqNum +=2;
    packet.bits = false;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = false;
    packet.markerBit = false;

    TEST(frameIn = jb.GetFrame(packet));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    frameOut = jb.GetFrameForDecoding();
    TEST(frameOut == NULL);

    packet.seqNum += 2;
    packet.timestamp += 33*90;

    TEST(frameIn = jb.GetFrame(packet));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    frameOut = jb.GetFrameForDecoding();

    TEST(frameOut != NULL);
    TEST(CheckOutFrame(frameOut, packet.sizeBytes, false) == 0);

    jb.Flush();

    // Test that a we can get incomplete frames from the JB if we have received
    // the marker bit.
    packet.seqNum +=2;
    packet.frameType = kVideoFrameDelta;
    packet.isFirstPacket = false;
    packet.markerBit = true;

    TEST(frameIn = jb.GetFrame(packet));
    TEST(kFirstPacket == jb.InsertPacket(frameIn, packet));

    frameOut = jb.GetFrameForDecoding();
    TEST(frameOut != NULL);

    // ---
    jb.Stop();

    printf("DONE !!!\n");

    printf("\nVCM Jitter Buffer Test: \n\n%i tests completed\n",
           vcmMacrosTests);
    if (vcmMacrosErrors > 0)
    {
        printf("%i FAILED\n\n", vcmMacrosErrors);
    }
    else
    {
        printf("ALL PASSED\n\n");
    }

    EventWrapper* waitEvent = EventWrapper::Create();
    waitEvent->Wait(5000);

    return 0;

}
