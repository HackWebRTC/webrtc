/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "bitrate_estimator.h"

namespace webrtc {

const float kBitrateAverageWindow = 500.0f;

BitRateStats::BitRateStats()
    :_dataSamples(), _accumulatedBytes(0)
{
}

BitRateStats::~BitRateStats()
{
    while (_dataSamples.size() > 0)
    {
        delete _dataSamples.front();
        _dataSamples.pop_front();
    }
}

void BitRateStats::Init()
{
    _accumulatedBytes = 0;
    while (_dataSamples.size() > 0)
    {
        delete _dataSamples.front();
        _dataSamples.pop_front();
    }
}

void BitRateStats::Update(uint32_t packetSizeBytes, int64_t nowMs)
{
    // Find an empty slot for storing the new sample and at the same time
    // accumulate the history.
    _dataSamples.push_back(new DataTimeSizeTuple(packetSizeBytes, nowMs));
    _accumulatedBytes += packetSizeBytes;
    EraseOld(nowMs);
}

void BitRateStats::EraseOld(int64_t nowMs)
{
    while (_dataSamples.size() > 0)
    {
        if (nowMs - _dataSamples.front()->_timeCompleteMs >
            kBitrateAverageWindow)
        {
            // Delete old sample
            _accumulatedBytes -= _dataSamples.front()->_sizeBytes;
            delete _dataSamples.front();
            _dataSamples.pop_front();
        }
        else
        {
            break;
        }
    }
}

uint32_t BitRateStats::BitRate(int64_t nowMs)
{
    // Calculate the average bit rate the past BITRATE_AVERAGE_WINDOW ms.
    // Removes any old samples from the list.
    EraseOld(nowMs);
    return static_cast<uint32_t>(_accumulatedBytes * 8.0f * 1000.0f /
                                       kBitrateAverageWindow + 0.5f);
}

}  // namespace webrtc
