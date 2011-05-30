/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "benchmark.h"
#include "vp8.h"

using namespace webrtc;

VP8Benchmark::VP8Benchmark()
:
Benchmark("VP8Benchmark", "VP8 benchmark over a range of test cases", "../../VP8Benchmark.txt", "VP8")
{
}

VP8Benchmark::VP8Benchmark(std::string name, std::string description)
:
Benchmark(name, description, "../../VP8Benchmark.txt", "VP8")
{
}

VP8Benchmark::VP8Benchmark(std::string name, std::string description, std::string resultsFileName)
:
Benchmark(name, description, resultsFileName, "VP8")
{
}

VideoEncoder*
VP8Benchmark::GetNewEncoder()
{
    return new VP8Encoder();
}

VideoDecoder*
VP8Benchmark::GetNewDecoder()
{
    return new VP8Decoder();
}
