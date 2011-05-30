/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "../interface/vie_autotest_android.h"

#include <android/log.h>
#include <stdio.h>

#include "vie_autotest.h"
#include "vie_autotest_defines.h"

int ViEAutoTestAndroid::RunAutotest(int testSelection, int subTestSelection,
                                    void* window1, void* window2, void* javaVM,
                                    void* env, void* context)
{
    ViEAutoTest vieAutoTest(window1, window2);
    ViETest::Log("RunAutoTest(%d, %d)", testSelection, subTestSelection);
    VideoEngine::SetAndroidObjects(javaVM, context);
    VoiceEngine::SetAndroidObjects(javaVM, env, context);
    int testErrors = 0;

    if (subTestSelection == 0)
    {
        // Run all selected test
        switch (testSelection)
        {
            case 0:
                testErrors += vieAutoTest.ViEStandardTest();
                if (testErrors == 0)
                {
                    // No errors found in delivery test, create delivery
                    ViETest::Log("Standard/delivery passed. ");
                }
                else
                {
                    // Didn't pass
                    ViETest::Log("\nStandard/delivery test failed.");
                }
                break;
            case 1:
                testErrors += vieAutoTest.ViEAPITest();
                break;
            case 2:
                testErrors += vieAutoTest.ViEExtendedTest();
                break;
            case 3:
                testErrors += vieAutoTest.ViELoopbackCall();
                break;
            default:
                break;
        }
    }

    switch (testSelection)
    {
        case 0: // Specific standard test
            switch (subTestSelection)
            {
                case 1: // base
                    testErrors += vieAutoTest.ViEBaseStandardTest();
                    break;

                case 2: // capture
                    testErrors += vieAutoTest.ViECaptureStandardTest();
                    break;

                case 3: // codec
                    testErrors += vieAutoTest.ViECodecStandardTest();
                    break;

                case 5: //encryption
                    testErrors += vieAutoTest.ViEEncryptionStandardTest();
                    break;

                case 6: // file
                    testErrors += vieAutoTest.ViEFileStandardTest();
                    break;

                case 7: // image process
                    testErrors += vieAutoTest.ViEImageProcessStandardTest();
                    break;

                case 8: // network
                    testErrors += vieAutoTest.ViENetworkStandardTest();
                    break;

                case 9: // Render
                    testErrors += vieAutoTest.ViERenderStandardTest();
                    break;

                case 10: // RTP/RTCP
                    testErrors += vieAutoTest.ViERtpRtcpStandardTest();
                    break;

                default:
                    break;
            }
            break;

        case 1:// specific API
            switch (subTestSelection)
            {
                case 1: // base
                    testErrors += vieAutoTest.ViEBaseAPITest();
                    break;

                case 2: // capture
                    testErrors += vieAutoTest.ViECaptureAPITest();
                    break;

                case 3: // codec
                    testErrors += vieAutoTest.ViECodecAPITest();
                    break;

                case 5: //encryption
                    testErrors += vieAutoTest.ViEEncryptionAPITest();
                    break;

                case 6: // file
                    testErrors += vieAutoTest.ViEFileAPITest();
                    break;

                case 7: // image process
                    testErrors += vieAutoTest.ViEImageProcessAPITest();
                    break;

                case 8: // network
                    testErrors += vieAutoTest.ViENetworkAPITest();
                    break;

                case 9: // Render
                    testErrors += vieAutoTest.ViERenderAPITest();
                    break;

                case 10: // RTP/RTCP
                    testErrors += vieAutoTest.ViERtpRtcpAPITest();
                    break;
                case 11:
                    break;

                default:
                    break;
            }
            break;

        case 2:// specific extended

            switch (subTestSelection)
            {
                case 1: // base
                    testErrors += vieAutoTest.ViEBaseExtendedTest();
                    break;

                case 2: // capture
                    testErrors += vieAutoTest.ViECaptureExtendedTest();
                    break;

                case 3: // codec
                    testErrors += vieAutoTest.ViECodecExtendedTest();
                    break;

                case 5: //encryption
                    testErrors += vieAutoTest.ViEEncryptionExtendedTest();
                    break;

                case 6: // file
                    testErrors += vieAutoTest.ViEFileExtendedTest();
                    break;

                case 7: // image process
                    testErrors += vieAutoTest.ViEImageProcessExtendedTest();
                    break;

                case 8: // network
                    testErrors += vieAutoTest.ViENetworkExtendedTest();
                    break;

                case 9: // Render
                    testErrors += vieAutoTest.ViERenderExtendedTest();
                    break;

                case 10: // RTP/RTCP
                    testErrors += vieAutoTest.ViERtpRtcpExtendedTest();
                    break;
                case 11:
                    break;

                default:
                    break;
            }
            break;
        case 3:
            testErrors += vieAutoTest.ViELoopbackCall();
            break;
        default:
            break;
    }

    if (testErrors)
    {
        ViETest::Log("Test done with %d errors!\n", testErrors);
    }
    else
    {
        ViETest::Log("Test passed!\n");
    }
    return testErrors;
}
