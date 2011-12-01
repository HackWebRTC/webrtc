/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <fstream>
#include <iostream>
#include <vector>

#include "benchmark.h"
#include "dual_decoder_test.h"
#include "normal_async_test.h"
#include "packet_loss_test.h"
#include "unit_test.h"
#include "testsupport/fileutils.h"
#include "vp8.h"

using namespace webrtc;

void PopulateTests(std::vector<Test*>* tests)
{
    tests->push_back(new VP8UnitTest());
//    tests->push_back(new VP8DualDecoderTest());
//    tests->push_back(new VP8Benchmark());
//    tests->push_back(new VP8PacketLossTest());
//    tests->push_back(new VP8NormalAsyncTest());
}

int main()
{
    VP8Encoder* enc;
    VP8Decoder* dec;
    std::vector<Test*> tests;
    PopulateTests(&tests);
    std::fstream log;
    std::string log_file = webrtc::test::OutputPath() + "VP8_test_log.txt";
    log.open(log_file.c_str(), std::fstream::out | std::fstream::app);
    std::vector<Test*>::iterator it;
    for (it = tests.begin() ; it < tests.end(); it++)
    {
        enc = new VP8Encoder();
        dec = new VP8Decoder();
        (*it)->SetEncoder(enc);
        (*it)->SetDecoder(dec);
        (*it)->SetLog(&log);
        (*it)->Perform();
        (*it)->Print();
        delete enc;
        delete dec;
        delete *it;
    }
   log.close();
   tests.pop_back();
   return 0;
}
