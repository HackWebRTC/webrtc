/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file includes the VAD API calls. For a specific function call description,
 * see webrtc_vad.h
 */

#include <stdlib.h>
#include <string.h>

#include "webrtc_vad.h"
#include "vad_core.h"

static const int kInitCheck = 42;

WebRtc_Word16 WebRtcVad_get_version(char *version, int length_bytes)
{
    const char my_version[] = "VAD 1.2.0";

    if (version == NULL)
    {
        return -1;
    }

    if (length_bytes < sizeof(my_version))
    {
        return -1;
    }

    memcpy(version, my_version, sizeof(my_version));
    return 0;
}

WebRtc_Word16 WebRtcVad_AssignSize(int *size_in_bytes)
{
    *size_in_bytes = sizeof(VadInstT) * 2 / sizeof(WebRtc_Word16);
    return 0;
}

WebRtc_Word16 WebRtcVad_Assign(VadInst **vad_inst, void *vad_inst_addr)
{

    if (vad_inst == NULL)
    {
        return -1;
    }

    if (vad_inst_addr != NULL)
    {
        *vad_inst = (VadInst*)vad_inst_addr;
        return 0;
    } else
    {
        return -1;
    }
}

WebRtc_Word16 WebRtcVad_Create(VadInst **vad_inst)
{

    VadInstT *vad_ptr = NULL;

    if (vad_inst == NULL)
    {
        return -1;
    }

    *vad_inst = NULL;

    vad_ptr = (VadInstT *)malloc(sizeof(VadInstT));
    *vad_inst = (VadInst *)vad_ptr;

    if (vad_ptr == NULL)
    {
        return -1;
    }

    vad_ptr->init_flag = 0;

    return 0;
}

WebRtc_Word16 WebRtcVad_Free(VadInst *vad_inst)
{

    if (vad_inst == NULL)
    {
        return -1;
    }

    free(vad_inst);
    return 0;
}

WebRtc_Word16 WebRtcVad_Init(VadInst *vad_inst)
{
    short mode = 0; // Default high quality

    if (vad_inst == NULL)
    {
        return -1;
    }

    return WebRtcVad_InitCore((VadInstT*)vad_inst, mode);
}

WebRtc_Word16 WebRtcVad_set_mode(VadInst *vad_inst, WebRtc_Word16 mode)
{
    VadInstT* vad_ptr;

    if (vad_inst == NULL)
    {
        return -1;
    }

    vad_ptr = (VadInstT*)vad_inst;
    if (vad_ptr->init_flag != kInitCheck)
    {
        return -1;
    }

    return WebRtcVad_set_mode_core((VadInstT*)vad_inst, mode);
}

WebRtc_Word16 WebRtcVad_Process(VadInst *vad_inst,
                                WebRtc_Word16 fs,
                                WebRtc_Word16 *speech_frame,
                                WebRtc_Word16 frame_length)
{
    WebRtc_Word16 vad;
    VadInstT* vad_ptr;

    if (vad_inst == NULL)
    {
        return -1;
    }

    vad_ptr = (VadInstT*)vad_inst;
    if (vad_ptr->init_flag != kInitCheck)
    {
        return -1;
    }

    if (speech_frame == NULL)
    {
        return -1;
    }

    if (fs == 32000)
    {
        if ((frame_length != 320) && (frame_length != 640) && (frame_length != 960))
        {
            return -1;
        }
        vad = WebRtcVad_CalcVad32khz((VadInstT*)vad_inst, speech_frame, frame_length);

    } else if (fs == 16000)
    {
        if ((frame_length != 160) && (frame_length != 320) && (frame_length != 480))
        {
            return -1;
        }
        vad = WebRtcVad_CalcVad16khz((VadInstT*)vad_inst, speech_frame, frame_length);

    } else if (fs == 8000)
    {
        if ((frame_length != 80) && (frame_length != 160) && (frame_length != 240))
        {
            return -1;
        }
        vad = WebRtcVad_CalcVad8khz((VadInstT*)vad_inst, speech_frame, frame_length);

    } else
    {
        return -1; // Not a supported sampling frequency
    }

    if (vad > 0)
    {
        return 1;
    } else if (vad == 0)
    {
        return 0;
    } else
    {
        return -1;
    }
}
