/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "audio_frame_manipulator.h"
#include "module_common_types.h"
#include "typedefs.h"

namespace webrtc {
void CalculateEnergy(AudioFrame& audioFrame)
{
    if(audioFrame._energy != 0xffffffff)
    {
        return;
    }
    audioFrame._energy = 0;
    for(int position = 0; position < audioFrame._payloadDataLengthInSamples;
        position++)
    {
        audioFrame._energy += audioFrame._payloadData[position] *
                              audioFrame._payloadData[position];
    }
}
} // namespace webrtc
