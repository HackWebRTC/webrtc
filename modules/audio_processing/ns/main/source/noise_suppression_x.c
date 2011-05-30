/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <string.h>

#include "noise_suppression_x.h"
#include "nsx_core.h"
#include "nsx_defines.h"

int WebRtcNsx_get_version(char *versionStr, short length)
{
    const char version[] = "NS\t3.1.0";
    const short versionLen = (short)strlen(version) + 1; // +1 for null-termination

    if (versionStr == NULL)
    {
        return -1;
    }

    if (versionLen > length)
    {
        return -1;
    }

    strncpy(versionStr, version, versionLen);

    return 0;
}

int WebRtcNsx_Create(NsxHandle **nsxInst)
{
    *nsxInst = (NsxHandle*)malloc(sizeof(NsxInst_t));
    if (*nsxInst != NULL)
    {
        (*(NsxInst_t**)nsxInst)->initFlag = 0;
        return 0;
    } else
    {
        return -1;
    }

}

int WebRtcNsx_Free(NsxHandle *nsxInst)
{
    free(nsxInst);
    return 0;
}

int WebRtcNsx_Init(NsxHandle *nsxInst, WebRtc_UWord32 fs)
{
    return WebRtcNsx_InitCore((NsxInst_t*)nsxInst, fs);
}

int WebRtcNsx_set_policy(NsxHandle *nsxInst, int mode)
{
    return WebRtcNsx_set_policy_core((NsxInst_t*)nsxInst, mode);
}

int WebRtcNsx_Process(NsxHandle *nsxInst, short *speechFrame, short *speechFrameHB,
                      short *outFrame, short *outFrameHB)
{
    return WebRtcNsx_ProcessCore((NsxInst_t*)nsxInst, speechFrame, speechFrameHB, outFrame,
                              outFrameHB);
}

