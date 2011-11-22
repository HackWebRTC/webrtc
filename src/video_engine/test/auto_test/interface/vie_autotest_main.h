/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_MAIN_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_MAIN_H_

#include <string>

class ViEAutoTestMain
{
public:
    ViEAutoTestMain();
    bool BeginOSIndependentTesting();
    bool GetAnswer(int index, std::string* answer);
    int GetClassTestSelection();
    bool GetNextAnswer(std::string& answer);
    bool IsUsingAnswerFile();
    bool UseAnswerFile(const char* fileName);

private:

    std::string _answers[1024];
    int _answersCount;
    int _answersIndex;
    bool _useAnswerFile;
};

#endif  // WEBRTC_VIDEO_ENGINE_MAIN_TEST_AUTOTEST_INTERFACE_VIE_AUTOTEST_MAIN_H_
