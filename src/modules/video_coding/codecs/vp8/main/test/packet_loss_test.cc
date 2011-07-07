/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "packet_loss_test.h"
#include <cassert>

VP8PacketLossTest::VP8PacketLossTest()
:
PacketLossTest("VP8PacketLossTest", "Encode, remove lost packets, decode")
{
}

VP8PacketLossTest::VP8PacketLossTest(std::string name, std::string description)
:
PacketLossTest(name, description)
{
}

VP8PacketLossTest::VP8PacketLossTest(double lossRate, bool useNack)
:
PacketLossTest("VP8PacketLossTest", "Encode, remove lost packets, decode", lossRate, useNack)
{
}

void
VP8PacketLossTest::CodecSpecific_InitBitrate()
{
    assert(_bitRate > 0);
    WebRtc_UWord32 simulatedBitRate;
    if (_lossProbability != _lossRate)
    {
        // Simulating NACK
        simulatedBitRate = (WebRtc_UWord32)(_bitRate / (1 + _lossRate));
    }
    else
    {
        simulatedBitRate = _bitRate;
    }
    _encoder->SetRates(simulatedBitRate, _inst.maxFramerate);
}

int VP8PacketLossTest::ByteLoss(int size, unsigned char* /* pkg */, int bytesToLose)
{
    int retLength = size - bytesToLose;
    if (retLength < 4)
    {
        retLength = 4;
    }
    return retLength;
}
