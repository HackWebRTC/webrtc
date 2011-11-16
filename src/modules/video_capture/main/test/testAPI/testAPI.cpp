/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testAPI.h"

#if defined(_WIN32)
#include <tchar.h>
#include <windows.h>
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

#elif defined(WEBRTC_LINUX)
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <sys/time.h>
#endif

using namespace std;

#include <stdio.h>
#include "testExternalCapture.h"
#include "testPlatformDependent.h"
#include "testCameraEncoder.h"

void RunApiTest() {
    int test_result = 0;

    webrtc::testExternalCapture test;
    test_result = test.DoTest();
    printf("\nExternal capture test result %d\n", test_result);

    webrtc::testPlatformDependent platform_dependent;
    test_result = platform_dependent.DoTest();
    printf("\nPlatform dependent test result %d\n", test_result);

    webrtc::testCameraEncoder camera_encoder;
    test_result = camera_encoder.DoTest();
    printf("\nCamera encoder test result %d\n", test_result);

    getchar();
}

// Note: The Mac main is implemented in testApi.mm.
#if defined(_WIN32)
int _tmain(int argc, _TCHAR* argv[])
#elif defined(WEBRTC_LINUX)
int main(int argc, char* argv[])
#endif // WEBRTC LINUX
#if !defined(WEBRTC_MAC)
{
    RunApiTest();
    return 0;
}
#endif // !WEBRTC_MAC
