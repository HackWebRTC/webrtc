/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_BITRATE_ESTIMATOR_H_
#define WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_BITRATE_ESTIMATOR_H_

#include <list>

#include "typedefs.h"

namespace webrtc {

class BitRateStats
{
public:
    BitRateStats();
    ~BitRateStats();

    void Init();
    void Update(uint32_t packetSizeBytes, int64_t nowMs);
    uint32_t BitRate(int64_t nowMs);

private:
    struct DataTimeSizeTuple
    {
        DataTimeSizeTuple(uint32_t sizeBytes, int64_t timeCompleteMs)
            :
              _sizeBytes(sizeBytes),
              _timeCompleteMs(timeCompleteMs) {}

        uint32_t    _sizeBytes;
        int64_t     _timeCompleteMs;
    };

    void EraseOld(int64_t nowMs);

    std::list<DataTimeSizeTuple*> _dataSamples;
    uint32_t _accumulatedBytes;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_REMOTE_BITRATE_ESTIMATOR_BITRATE_ESTIMATOR_H_
