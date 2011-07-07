/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "unit_test.h"
#include "video_processing.h"
#include "tick_util.h"

#include <cstdio>
#include <cstdlib>

using namespace webrtc;

TEST_F(VideoProcessingModuleTest, Deflickering)
{
    enum { NumRuns = 30 };
    WebRtc_UWord32 frameNum = 0;
    const WebRtc_UWord32 frameRate = 15;

    WebRtc_Word64 minRuntime = 0;
    WebRtc_Word64 avgRuntime = 0;

    // Close automatically opened Foreman.
    fclose(_sourceFile);
    _sourceFile  = fopen("deflicker_testfile_before.yuv", "rb");
    ASSERT_TRUE(_sourceFile != NULL) <<
        "Cannot read input file deflicker_testfile_before.yuv\n";

    FILE* deflickerFile = fopen("deflicker_testfile.yuv", "wb");
    ASSERT_TRUE(deflickerFile != NULL) << "Could not open output file.\n";

    printf("\nRun time [us / frame]:\n");
    for (WebRtc_UWord32 runIdx = 0; runIdx < NumRuns; runIdx++)
    {
        TickTime t0;
        TickTime t1;
        TickInterval accTicks;
        WebRtc_UWord32 timeStamp = 1;

        frameNum = 0;
        while (fread(_videoFrame.Buffer(), 1, _frameLength, _sourceFile) == _frameLength)
        {
            frameNum++;
            _videoFrame.SetTimeStamp(timeStamp);

            t0 = TickTime::Now();           
            VideoProcessingModule::FrameStats stats;
            ASSERT_EQ(0, _vpm->GetFrameStats(stats, _videoFrame));
            ASSERT_EQ(0, _vpm->Deflickering(_videoFrame, stats));
            t1 = TickTime::Now();
            accTicks += t1 - t0;
            
            if (runIdx == 0)
            {
                fwrite(_videoFrame.Buffer(), 1, _frameLength, deflickerFile);
            }
            timeStamp += (90000 / frameRate);
        }
        ASSERT_NE(0, feof(_sourceFile)) << "Error reading source file";

        printf("%u\n", static_cast<int>(accTicks.Microseconds() / frameNum));
        if (accTicks.Microseconds() < minRuntime || runIdx == 0)
        {
            minRuntime = accTicks.Microseconds();
        }
        avgRuntime += accTicks.Microseconds();

        rewind(_sourceFile);
    }

    printf("\nAverage run time = %d us / frame\n", 
        static_cast<int>(avgRuntime / frameNum / NumRuns));
    printf("Min run time = %d us / frame\n\n", 
        static_cast<int>(minRuntime / frameNum));
}
