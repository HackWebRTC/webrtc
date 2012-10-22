/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "color_enhancement.h"
#include "color_enhancement_private.h"
#include "trace.h"
#include <cstdlib>  // NULL

namespace webrtc {

namespace VideoProcessing
{ 
    WebRtc_Word32
    ColorEnhancement(VideoFrame* frame)
    {
        assert(frame);
        // pointers to U and V color pixels
        WebRtc_UWord8* ptrU;
        WebRtc_UWord8* ptrV;
        WebRtc_UWord8 tempChroma;
        const unsigned int size_y = frame->Width() * frame->Height();
        const unsigned int size_uv = ((frame->Width() + 1) / 2) *
            ((frame->Height() + 1 ) / 2);


        if (frame->Buffer() == NULL)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing,
                         -1, "Null frame pointer");
            return VPM_GENERAL_ERROR;
        }

        if (frame->Width() == 0 || frame->Height() == 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing,
                         -1, "Invalid frame size");
            return VPM_GENERAL_ERROR;
        }
        
        // set pointers to first U and V pixels (skip luminance)
        ptrU = frame->Buffer() + size_y;
        ptrV = ptrU + size_uv;

        // loop through all chrominance pixels and modify color
        for (unsigned int ix = 0; ix < size_uv; ix++)
        {
            tempChroma = colorTable[*ptrU][*ptrV];
            *ptrV = colorTable[*ptrV][*ptrU];
            *ptrU = tempChroma;
            
            // increment pointers
            ptrU++;
            ptrV++;
        }
        return VPM_OK;
    }

} //namespace

} //namespace webrtc
