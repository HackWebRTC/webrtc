/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>
#include <iostream>

#ifdef _WIN32
    #include <windows.h>
    #include <tchar.h>
#else
    #include <stdio.h>
    #define Sleep(x) usleep(x*1000)
#endif

#include "common_types.h"
#include "trace.h"
#include "cpu_wrapper.h"


#ifdef _WIN32
int _tmain(int argc, _TCHAR* argv[])
#else
int main(int argc, char* argv[])
#endif
{
    Trace::CreateTrace();
    Trace::SetTraceFile("testTrace.txt");
    Trace::SetLevelFilter(webrtc::kTraceAll);

    printf("Start system wrapper test\n");

    printf("Number of cores detected:%u\n", (unsigned int)CpuWrapper::DetectNumberOfCores());

    CpuWrapper* cpu = CpuWrapper::CreateCpu();

    WebRtc_UWord32 numCores;
    WebRtc_UWord32* cores;

    for(int i = 0; i< 10;i++)
    {
        WebRtc_Word32 total = cpu->CpuUsageMultiCore(numCores, cores);

        printf("\nNumCores:%d\n", (int)numCores);
        printf("Total cpu:%d\n", (int)total);

        for (WebRtc_UWord32 i = 0; i< numCores;i++)
        {
            printf("Core:%lu CPU:%lu \n", i, cores[i]);
        }
        Sleep(1000);
    }

    printf("Done system wrapper test\n");

    delete cpu;

    Trace::ReturnTrace();
};
