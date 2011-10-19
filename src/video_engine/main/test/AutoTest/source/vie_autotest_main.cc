/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_autotest_main.h"

#include "vie_autotest_window_manager_interface.h"
#include "vie_window_creator.h"
#include "vie_autotest.h"

ViEAutoTestMain::ViEAutoTestMain()
    : _answers(),
      _answersCount(0),
      _useAnswerFile() {
}

bool ViEAutoTestMain::BeginOSIndependentTesting() {
    // Create the windows
    ViEWindowCreator windowCreator;
    ViEAutoTestWindowManagerInterface* windowManager =
        windowCreator.CreateTwoWindows();

    // Create the test cases
    ViEAutoTest
        vieAutoTest(windowManager->GetWindow1(),
                    windowManager->GetWindow2(),
                    ViETest::kUseAssertsForTestErrors);

    ViETest::Log(" ============================== ");
    ViETest::Log("    WebRTC ViE 3.x Autotest     ");
    ViETest::Log(" ============================== \n");

    int testType = 0;
    int testErrors = 0;
    do {
        ViETest::Log("Test types: ");
        ViETest::Log("\t 0. Quit");
        ViETest::Log("\t 1. All standard tests (delivery test)");
        ViETest::Log("\t 2. All API tests");
        ViETest::Log("\t 3. All extended test");
        ViETest::Log("\t 4. Specific standard test");
        ViETest::Log("\t 5. Specific API test");
        ViETest::Log("\t 6. Specific extended test");
        ViETest::Log("\t 7. Simple loopback call");
        ViETest::Log("\t 8. Custom configure a call");
        ViETest::Log("\t 9. Simulcast in loopback");
        ViETest::Log("Select type of test: ");

        if (_useAnswerFile) {
            // GetNextAnswer(str);
        } else {
            if (scanf("%d", &testType) <= 0) {
                ViETest::Log("ERROR: unable to read selection. Try again\n");
                testType = -1;
                getchar();
                continue;
            }
            getchar();
        }
        ViETest::Log("");

        if (testType < 0 || testType > 8) {
            ViETest::Log("ERROR: Invalid selection. Try again\n");
            continue;
        }

        switch (testType) {
            case 0:
                break;

            case 1:
            {
                int deliveryErrors = testErrors;
                testErrors += vieAutoTest.ViEStandardTest();
                if (testErrors == deliveryErrors) {
                    // No errors found in delivery test, create delivery
                    ViETest::Log("Standard/delivery passed.");
                } else {
                    // Didn't pass, don't create delivery files
                    ViETest::Log("\nStandard/delivery test failed!\n");
                }
                break;
            }
            case 2:
                testErrors += vieAutoTest.ViEAPITest();
                break;

            case 3:
                testErrors += vieAutoTest.ViEExtendedTest();
                break;

            case 4:  // Specific Standard
                testType = GetClassTestSelection();

                switch (testType) {
                    case 1:  // base
                        testErrors += vieAutoTest.ViEBaseStandardTest();
                        break;

                    case 2:  // capture
                        testErrors += vieAutoTest.ViECaptureStandardTest();
                        break;

                    case 3:  // codec
                        testErrors += vieAutoTest.ViECodecStandardTest();
                        break;

                    case 5:  // encryption
                        testErrors += vieAutoTest.ViEEncryptionStandardTest();
                        break;

                    case 6:  // file
                        testErrors += vieAutoTest.ViEFileStandardTest();
                        break;

                    case 7:  // image process
                        testErrors += vieAutoTest.ViEImageProcessStandardTest();
                        break;

                    case 8:  // network
                        testErrors += vieAutoTest.ViENetworkStandardTest();
                        break;

                    case 9:  // Render
                        testErrors += vieAutoTest.ViERenderStandardTest();
                        break;

                    case 10:  // RTP/RTCP
                        testErrors += vieAutoTest.ViERtpRtcpStandardTest();
                        break;
                    case 11:
                        break;

                    default:
                        break;
                }
                break;

            case 5:  // specific API
                testType = GetClassTestSelection();

                switch (testType) {
                    case 1:  // base
                        testErrors += vieAutoTest.ViEBaseAPITest();
                        break;

                    case 2:  // capture
                        testErrors += vieAutoTest.ViECaptureAPITest();
                        break;

                    case 3:  // codec
                        testErrors += vieAutoTest.ViECodecAPITest();
                        break;

                    case 5:  // encryption
                        testErrors += vieAutoTest.ViEEncryptionAPITest();
                        break;

                    case 6:  // file
                        testErrors += vieAutoTest.ViEFileAPITest();
                        break;

                    case 7:  // image process
                        testErrors += vieAutoTest.ViEImageProcessAPITest();
                        break;

                    case 8:  // network
                        testErrors += vieAutoTest.ViENetworkAPITest();
                        break;

                    case 9:  // Render
                        testErrors += vieAutoTest.ViERenderAPITest();
                        break;

                    case 10:  // RTP/RTCP
                        testErrors += vieAutoTest.ViERtpRtcpAPITest();
                        break;
                    case 11:
                        break;
                    default:
                        break;
                }
                break;
            case 6:  // specific extended
                testType = GetClassTestSelection();

                switch (testType) {
                    case 1:  // base
                        testErrors += vieAutoTest.ViEBaseExtendedTest();
                        break;

                    case 2:  // capture
                        testErrors += vieAutoTest.ViECaptureExtendedTest();
                        break;

                    case 3:  // codec
                        testErrors += vieAutoTest.ViECodecExtendedTest();
                        break;

                    case 5:  // encryption
                        testErrors += vieAutoTest.ViEEncryptionExtendedTest();
                        break;

                    case 6:  // file
                        testErrors += vieAutoTest.ViEFileExtendedTest();
                        break;

                    case 7:  // image process
                        testErrors += vieAutoTest.ViEImageProcessExtendedTest();
                        break;

                    case 8:  // network
                        testErrors += vieAutoTest.ViENetworkExtendedTest();
                        break;

                    case 9:  // Render
                        testErrors += vieAutoTest.ViERenderExtendedTest();
                        break;

                    case 10:  // RTP/RTCP
                        testErrors += vieAutoTest.ViERtpRtcpExtendedTest();
                        break;
                    case 11:
                        break;

                    default:
                        break;
                }
                break;
            case 7:
                testErrors += vieAutoTest.ViELoopbackCall();
                break;
            case 8:
                testErrors += vieAutoTest.ViECustomCall();
                break;
            case 9:
                testErrors += vieAutoTest.ViESimulcastCall();
                break;
            default:
                ViETest::Log("ERROR: Invalid selection. Try again\n");
                continue;
        }
    } while (testType != 0);

    windowCreator.TerminateWindows();

    if (testErrors) {
        ViETest::Log("Test done with errors, see ViEAutotestLog.txt for test "
                     "result.\n");
    } else {
        ViETest::Log("Test done without errors, see ViEAutotestLog.txt for "
                     "test result.\n");
    }
    printf("Press enter to quit...");
    char c;
    while ((c = getchar()) != '\n' && c != EOF) {
        /* discard */
    }

    return true;
}

int ViEAutoTestMain::GetClassTestSelection() {
    int testType = 0;
    std::string answer;

    while (1) {
        ViETest::Log("Choose specific test: ");
        ViETest::Log("\t 1. Base ");
        ViETest::Log("\t 2. Capture");
        ViETest::Log("\t 3. Codec");
        ViETest::Log("\t 5. Encryption");
        ViETest::Log("\t 6. File");
        ViETest::Log("\t 7. Image Process");
        ViETest::Log("\t 8. Network");
        ViETest::Log("\t 9. Render");
        ViETest::Log("\t 10. RTP/RTCP");
        ViETest::Log("\t 11. Go back to previous menu");
        ViETest::Log("Select type of test: ");

        int items_read = 0;
        if (_useAnswerFile) {
            // GetNextAnswer(answer);
        } else {
            items_read = scanf("%d", &testType);
            getchar();
        }
        ViETest::Log("\n");
        if (items_read == 1 && testType >= 1 && testType <= 13) {
            return testType;
        }
        ViETest::Log("ERROR: Invalid selection. Try again");
    }

    return -1;
}

bool ViEAutoTestMain::GetAnswer(int index, std::string* answer) {
    if (!_useAnswerFile || index > _answersCount) {
        return false;
    }
    *answer = _answers[index];
    return true;
}

bool ViEAutoTestMain::IsUsingAnswerFile() {
    return _useAnswerFile;
}

// TODO(unknown): write without stl
bool ViEAutoTestMain::UseAnswerFile(const char* fileName) {
    return false;
    /*
     _useAnswerFile = false;

     ViETest::Log("Opening answer file:  %s...", fileName);

     ifstream answerFile(fileName);
     if(!answerFile)
     {
     ViETest::Log("failed! X(\n");
     return false;
     }

     _answersCount = 1;
     _answersIndex = 1;
     char lineContent[128] = "";
     while(!answerFile.eof())
     {
     answerFile.getline(lineContent, 128);
     _answers[_answersCount++] = string(lineContent);
     }
     answerFile.close();

     cout << "Success :)" << endl << endl;

     _useAnswerFile = true;

     return _useAnswerFile;
     */
}
