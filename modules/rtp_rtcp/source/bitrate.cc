/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "Bitrate.h"
#include "rtp_utility.h"
#include "tick_util.h"

#define BITRATE_AVERAGE_WINDOW 2000

namespace webrtc {
Bitrate::Bitrate() :
    _packetRate(0),
    _bitrate(0),
    _bitrateNextIdx(0),
    _timeLastRateUpdate(0),
    _bytesCount(0),
    _packetCount(0)
{
    memset(_packetRateArray, 0, sizeof(_packetRateArray));
    memset(_bitrateDiffMS, 0, sizeof(_bitrateDiffMS));
    memset(_bitrateArray, 0, sizeof(_bitrateArray));
}

void
Bitrate::Init()
{
    _packetRate = 0;
    _bitrate = 0;
    _timeLastRateUpdate = 0;
    _bytesCount = 0;
    _packetCount = 0;
    _bitrateNextIdx = 0;

    memset(_packetRateArray, 0, sizeof(_packetRateArray));
    memset(_bitrateDiffMS, 0, sizeof(_bitrateDiffMS));
    memset(_bitrateArray, 0, sizeof(_bitrateArray));
}

void
Bitrate::Update(const WebRtc_Word32 bytes)
{
    _bytesCount += bytes;
    _packetCount++;
}

WebRtc_UWord32
Bitrate::PacketRate() const
{
    return _packetRate;
}

WebRtc_UWord32
Bitrate::BitrateLast() const
{
    return _bitrate;
}

WebRtc_UWord32
Bitrate::BitrateNow() const
{
    WebRtc_UWord32 now = ModuleRTPUtility::GetTimeInMS();
    WebRtc_UWord32 diffMS = now -_timeLastRateUpdate;

    if(diffMS > 10000) // 10 sec
    {
        // too high diff ignore
        return _bitrate; // bits/s
    }
    WebRtc_UWord64 bitsSinceLastRateUpdate = 8*_bytesCount*1000;

    // have to consider the time when the measurement was done
    // ((bits/sec * sec) + (bits)) / sec
    WebRtc_UWord64 bitrate = (((WebRtc_UWord64)_bitrate * 1000) + bitsSinceLastRateUpdate)/(1000+diffMS);
    return (WebRtc_UWord32)bitrate;
}

void
Bitrate::Process()
{
    // triggered by timer
    WebRtc_UWord32 now = ModuleRTPUtility::GetTimeInMS();
    WebRtc_UWord32 diffMS = now -_timeLastRateUpdate;

    if(diffMS > 100)
    {
        if(diffMS > 10000) // 10 sec
        {
            // too high diff ignore
            _timeLastRateUpdate = now;
            _bytesCount = 0;
            _packetCount = 0;
            return;
        }
        _packetRateArray[_bitrateNextIdx] = (_packetCount*1000)/diffMS;
        _bitrateArray[_bitrateNextIdx]    = 8*((_bytesCount*1000)/diffMS);
        // will overflow at ~34 Mbit/s
        _bitrateDiffMS[_bitrateNextIdx]   = diffMS;
        _bitrateNextIdx++;
        if(_bitrateNextIdx >= 10)
        {
            _bitrateNextIdx = 0;
        }

        WebRtc_UWord32 sumDiffMS = 0;
        WebRtc_UWord64 sumBitrateMS = 0;
        WebRtc_UWord32 sumPacketrateMS = 0;
        for(int i= 0; i <10; i++)
        {
            // sum of time
            sumDiffMS += _bitrateDiffMS[i];
            sumBitrateMS += _bitrateArray[i] * _bitrateDiffMS[i];
            sumPacketrateMS += _packetRateArray[i] * _bitrateDiffMS[i];
        }
        _timeLastRateUpdate = now;
        _bytesCount = 0;
        _packetCount = 0;

        _packetRate = sumPacketrateMS/sumDiffMS;
        _bitrate = WebRtc_UWord32(sumBitrateMS / sumDiffMS);
    }
}


BitRateStats::BitRateStats()
    :_dataSamples(), _avgSentBitRateBps(0)
{
}

BitRateStats::~BitRateStats()
{
    ListItem* item = _dataSamples.First();
    while (item != NULL)
    {
        delete static_cast<DataTimeSizeTuple*>(item->GetItem());
        _dataSamples.Erase(item);
        item = _dataSamples.First();
    }
}

void BitRateStats::Init()
{
    _avgSentBitRateBps = 0;

    ListItem* item = _dataSamples.First();
    while (item != NULL)
    {
        delete static_cast<DataTimeSizeTuple*>(item->GetItem());
        _dataSamples.Erase(item);
        item = _dataSamples.First();
    }
}

void BitRateStats::Update(WebRtc_Word64 packetSizeBytes, WebRtc_Word64 nowMs)
{
    WebRtc_UWord32 sumBytes = 0;
    WebRtc_Word64 timeOldest = nowMs;
    // Find an empty slot for storing the new sample and at the same time
    // accumulate the history.
    _dataSamples.PushFront(new DataTimeSizeTuple(packetSizeBytes, nowMs));
    ListItem* item = _dataSamples.First();
    while (item != NULL)
    {
        const DataTimeSizeTuple* sample = static_cast<DataTimeSizeTuple*>(item->GetItem());
        if (nowMs - sample->_timeCompleteMs < BITRATE_AVERAGE_WINDOW)
        {
            sumBytes += static_cast<WebRtc_UWord32>(sample->_sizeBytes);
            item = _dataSamples.Next(item);
        }
        else
        {
            // Delete old sample
            delete sample;
            ListItem* itemToErase = item;
            item = _dataSamples.Next(item);
            _dataSamples.Erase(itemToErase);
        }
    }
    const ListItem* oldest = _dataSamples.Last();
    if (oldest != NULL)
    {
        timeOldest =
           static_cast<DataTimeSizeTuple*>(oldest->GetItem())->_timeCompleteMs;
    }
    // Update average bit rate
    float denom = static_cast<float>(nowMs - timeOldest);
    if (denom < 1.0)
    {
        // Calculate with a one second window when we haven't
        // received more than one packet.
        denom = 1000.0;
    }
    _avgSentBitRateBps = static_cast<WebRtc_UWord32>(sumBytes * 8.0f * 1000.0f / denom + 0.5f);
}

WebRtc_UWord32 BitRateStats::BitRateNow()
{
    Update(-1, TickTime::MillisecondTimestamp());
    return static_cast<WebRtc_UWord32>(_avgSentBitRateBps + 0.5f);
}
} // namespace webrtc
