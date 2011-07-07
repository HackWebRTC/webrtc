/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//
// vie_autotest_encryption.cc
//

#include "vie_autotest_defines.h"
#include "vie_autotest.h"
#include "engine_configurations.h"

#include "tb_capture_device.h"
#include "tb_external_transport.h"
#include "tb_interfaces.h"
#include "tb_video_channel.h"

class ViEAutotestEncryption: public Encryption
{
public:
    ViEAutotestEncryption()
    {
    }
    ~ViEAutotestEncryption()
    {
    }

    virtual void encrypt(int channel_no, unsigned char * in_data,
                         unsigned char * out_data, int bytes_in, int* bytes_out)
    {
        for (int i = 0; i < bytes_in; i++)
        {
            out_data[i] = ~in_data[i];
        }
        *bytes_out = bytes_in + 2;
    }

    virtual void decrypt(int channel_no, unsigned char * in_data,
                         unsigned char * out_data, int bytes_in, int* bytes_out)
    {
        for (int i = 0; i < bytes_in - 2; i++)
        {
            out_data[i] = ~in_data[i];
        }
        *bytes_out = bytes_in - 2;
    }

    virtual void encrypt_rtcp(int channel_no, unsigned char * in_data,
                              unsigned char * out_data, int bytes_in,
                              int* bytes_out)
    {
        for (int i = 0; i < bytes_in; i++)
        {
            out_data[i] = ~in_data[i];
        }
        *bytes_out = bytes_in + 2;
    }

    virtual void decrypt_rtcp(int channel_no, unsigned char * in_data,
                              unsigned char * out_data, int bytes_in,
                              int* bytes_out)
    {
        for (int i = 0; i < bytes_in - 2; i++)
        {
            out_data[i] = ~in_data[i];
        }
        *bytes_out = bytes_in - 2;
    }
};

int ViEAutoTest::ViEEncryptionStandardTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEEncryption Standard Test\n");

    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************

    int error = 0;
    bool succeeded = true;
    int numberOfErrors = 0;

    // Create VIE
    tbInterfaces ViE("ViEEncryptionStandardTest", numberOfErrors);
    // Create a video channel
    tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);

    // Create a capture device
    tbCaptureDevice tbCapture(ViE, numberOfErrors);
    tbCapture.ConnectTo(tbChannel.videoChannel);

    tbChannel.StartReceive();

    tbChannel.StartSend();

    error = ViE.ptrViERender->AddRenderer(tbCapture.captureId, _window1, 0,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->StartRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->AddRenderer(tbChannel.videoChannel, _window2, 1,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->StartRender(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

#ifdef WEBRTC_SRTP
    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************


    //
    // SRTP
    //
    unsigned char srtpKey1[30] =
    {   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3,
        4, 5, 6, 7, 8, 9};

    // Encryption only
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    ViETest::Log("SRTP encryption only");
    AutoTestSleep(KAutoTestSleepTimeMs);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Authentication only
    error = ViE.ptrViEEncryption->EnableSRTPReceive(tbChannel.videoChannel,
                                                    webrtc::kCipherNull, 0,
                                                    webrtc::kAuthHmacSha1, 20,
                                                    4, webrtc::kAuthentication,
                                                    srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(tbChannel.videoChannel,
                                                 webrtc::kCipherNull, 0,
                                                 webrtc::kAuthHmacSha1, 20, 4,
                                                 webrtc::kAuthentication,
                                                 srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    ViETest::Log("SRTP authentication only");
    AutoTestSleep(KAutoTestSleepTimeMs);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Full protection
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    ViETest::Log("SRTP full protection");
    AutoTestSleep(KAutoTestSleepTimeMs);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
#endif  // WEBRTC_SRTP
    //
    // External encryption
    //
    ViEAutotestEncryption testEncryption;
    error = ViE.ptrViEBase->StartSend(tbChannel.videoChannel);
    error = ViE.ptrViEEncryption->RegisterExternalEncryption(
        tbChannel.videoChannel, testEncryption);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    ViETest::Log(
        "External encryption/decryption added, you should still see video");
    AutoTestSleep(KAutoTestSleepTimeMs);
    error = ViE.ptrViEEncryption->DeregisterExternalEncryption(
        tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************


    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViEEncryption Standard Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViEEncryption Standard Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;

}

int ViEAutoTest::ViEEncryptionExtendedTest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEEncryption Extended Test\n");

    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************

    int error = 0;
    bool succeeded = true;
    int numberOfErrors = 0;

    // Create VIE
    tbInterfaces ViE("ViEEncryptionExtendedTest", numberOfErrors);
    // Create a video channel
    tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);

    // Create a capture device
    tbCaptureDevice tbCapture(ViE, numberOfErrors);
    tbCapture.ConnectTo(tbChannel.videoChannel);

    tbChannel.StartReceive();
    tbChannel.StartSend();

    error = ViE.ptrViERender->AddRenderer(tbCapture.captureId, _window1, 0,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->StartRender(tbCapture.captureId);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->AddRenderer(tbChannel.videoChannel, _window2, 1,
                                          0.0, 0.0, 1.0, 1.0);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViERender->StartRender(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************

#ifdef WEBRTC_SRTP

    //
    // SRTP
    //
    unsigned char srtpKey1[30] =
    {   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3,
        4, 5, 6, 7, 8, 9};
    unsigned char srtpKey2[30] =
    {   9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, 9, 8, 7, 6,
        5, 4, 3, 2, 1, 0};
    // NULL
    error = ViE.ptrViEEncryption->EnableSRTPReceive(tbChannel.videoChannel,
                                                    webrtc::kCipherNull, 0,
                                                    webrtc::kAuthNull, 0, 0,
                                                    webrtc::kNoProtection,
                                                    srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(tbChannel.videoChannel,
                                                 webrtc::kCipherNull, 0,
                                                 webrtc::kAuthNull, 0, 0,
                                                 webrtc::kNoProtection,
                                                 srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    ViETest::Log("SRTP NULL encryption/authentication");
    AutoTestSleep(KAutoTestSleepTimeMs);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Encryption only
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    ViETest::Log("SRTP encryption only");
    AutoTestSleep(KAutoTestSleepTimeMs);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Authentication only
    error = ViE.ptrViEEncryption->EnableSRTPReceive(tbChannel.videoChannel,
                                                    webrtc::kCipherNull, 0,
                                                    webrtc::kAuthHmacSha1, 20,
                                                    4, webrtc::kAuthentication,
                                                    srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(tbChannel.videoChannel,
                                                 webrtc::kCipherNull, 0,
                                                 webrtc::kAuthHmacSha1, 20, 4,
                                                 webrtc::kAuthentication,
                                                 srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    ViETest::Log("SRTP authentication only");
    AutoTestSleep(KAutoTestSleepTimeMs);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Full protection
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    ViETest::Log("SRTP full protection");
    AutoTestSleep(KAutoTestSleepTimeMs);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Change receive key, but not send key...
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey2);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey1);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    ViETest::Log(
        "\nSRTP receive key changed, you should not see any remote images");
    AutoTestSleep(KAutoTestSleepTimeMs);

    // Change send key too
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey2);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    ViETest::Log("\nSRTP send key changed too, you should see remote video "
                 "again with some decoding artefacts at start");
    AutoTestSleep(KAutoTestSleepTimeMs);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Disable receive, keep send
    ViETest::Log("SRTP receive disabled , you shouldn't see any video");
    AutoTestSleep(KAutoTestSleepTimeMs);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

#endif //WEBRTC_SRTP
    //
    // External encryption
    //
    ViEAutotestEncryption testEncryption;
    error
        = ViE.ptrViEEncryption->RegisterExternalEncryption(
            tbChannel.videoChannel, testEncryption);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    ViETest::Log(
        "External encryption/decryption added, you should still see video");
    AutoTestSleep(KAutoTestSleepTimeMs);
    error
        = ViE.ptrViEEncryption->DeregisterExternalEncryption(
            tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************


    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViEEncryption Extended Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViEEncryption Extended Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}

int ViEAutoTest::ViEEncryptionAPITest()
{
    ViETest::Log(" ");
    ViETest::Log("========================================");
    ViETest::Log(" ViEEncryption API Test\n");

    //***************************************************************
    //	Begin create/initialize WebRTC Video Engine for testing
    //***************************************************************


    int error = 0;
    bool succeeded = true;
    int numberOfErrors = 0;

    //***************************************************************
    //	Engine ready. Begin testing class
    //***************************************************************

    // Create VIE
    tbInterfaces ViE("ViEEncryptionAPITest", numberOfErrors);
    // Create a video channel
    tbVideoChannel tbChannel(ViE, numberOfErrors, webrtc::kVideoCodecVP8);

    // Create a capture device
    tbCaptureDevice tbCapture(ViE, numberOfErrors);
    // Connect to channel
    tbCapture.ConnectTo(tbChannel.videoChannel);

#ifdef WEBRTC_SRTP
    unsigned char srtpKey[30] =
    {   0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3,
        4, 5, 6, 7, 8, 9};

    //
    // EnableSRTPSend and DisableSRTPSend
    //

    // Incorrect input argument, complete protection not enabled
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kNoProtection, srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryption, srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kAuthentication, srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Incorrect cipher key length
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 15,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 257,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherNull, 15, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kEncryptionAndAuthentication, srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherNull, 257, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kEncryptionAndAuthentication, srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Incorrect auth key length
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode,
        30, webrtc::kAuthHmacSha1, 21, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 257, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 21, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 20, 13, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // NULL input
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        NULL);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Double enable and disable
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // No protection
    error = ViE.ptrViEEncryption->EnableSRTPSend(tbChannel.videoChannel,
                                                 webrtc::kCipherNull, 0,
                                                 webrtc::kAuthNull, 0, 0,
                                                 webrtc::kNoProtection,
                                                 srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Authentication only
    error = ViE.ptrViEEncryption->EnableSRTPSend(tbChannel.videoChannel,
                                                 webrtc::kCipherNull, 0,
                                                 webrtc::kAuthHmacSha1, 20, 4,
                                                 webrtc::kAuthentication,
                                                 srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(tbChannel.videoChannel,
                                                 webrtc::kCipherNull, 0,
                                                 webrtc::kAuthHmacSha1, 1, 4,
                                                 webrtc::kAuthentication,
                                                 srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(tbChannel.videoChannel,
                                                 webrtc::kCipherNull, 0,
                                                 webrtc::kAuthHmacSha1, 20, 20,
                                                 webrtc::kAuthentication,
                                                 srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(tbChannel.videoChannel,
                                                 webrtc::kCipherNull, 0,
                                                 webrtc::kAuthHmacSha1, 1, 1,
                                                 webrtc::kAuthentication,
                                                 srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Encryption only
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 16,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Full protection
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //
    // EnableSRTPReceive and DisableSRTPReceive
    //

    // Incorrect input argument, complete protection not enabled
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kNoProtection, srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryption, srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kAuthentication, srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Incorrect cipher key length
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 15,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 257,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherNull, 15, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kEncryptionAndAuthentication, srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherNull, 257, webrtc::kAuthHmacSha1,
        20, 4, webrtc::kEncryptionAndAuthentication, srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Incorrect auth key length
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 21, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 257, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 21, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 20, 13, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // NULL input
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        NULL);
    numberOfErrors += ViETest::TestError(error != 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Double enable and disable
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPSend(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPSend(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // No protection
    error = ViE.ptrViEEncryption->EnableSRTPReceive(tbChannel.videoChannel,
                                                    webrtc::kCipherNull, 0,
                                                    webrtc::kAuthNull, 0, 0,
                                                    webrtc::kNoProtection,
                                                    srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Authentication only

    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPReceive(tbChannel.videoChannel,
                                                    webrtc::kCipherNull, 0,
                                                    webrtc::kAuthHmacSha1, 1, 4,
                                                    webrtc::kAuthentication,
                                                    srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPReceive(tbChannel.videoChannel,
                                                    webrtc::kCipherNull, 0,
                                                    webrtc::kAuthHmacSha1, 20,
                                                    20, webrtc::kAuthentication,
                                                    srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPReceive(tbChannel.videoChannel,
                                                    webrtc::kCipherNull, 0,
                                                    webrtc::kAuthHmacSha1, 1, 1,
                                                    webrtc::kAuthentication,
                                                    srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Encryption only
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 16,
        webrtc::kAuthNull, 0, 0, webrtc::kEncryption, srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    // Full protection
    error = ViE.ptrViEEncryption->EnableSRTPReceive(
        tbChannel.videoChannel, webrtc::kCipherAes128CounterMode, 30,
        webrtc::kAuthHmacSha1, 20, 4, webrtc::kEncryptionAndAuthentication,
        srtpKey);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DisableSRTPReceive(tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
#endif //WEBRTC_SRTP
    //
    // External encryption
    //

    ViEAutotestEncryption testEncryption;
    error = ViE.ptrViEEncryption->RegisterExternalEncryption(
        tbChannel.videoChannel, testEncryption);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->RegisterExternalEncryption(
        tbChannel.videoChannel, testEncryption);
    numberOfErrors += ViETest::TestError(error == -1, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DeregisterExternalEncryption(
        tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);
    error = ViE.ptrViEEncryption->DeregisterExternalEncryption(
        tbChannel.videoChannel);
    numberOfErrors += ViETest::TestError(error == 0, "ERROR: %s at line %d",
                                         __FUNCTION__, __LINE__);

    //***************************************************************
    //	Testing finished. Tear down Video Engine
    //***************************************************************

    if (numberOfErrors > 0)
    {
        // Test failed
        ViETest::Log(" ");
        ViETest::Log(" ERROR ViEEncryption API Test FAILED!");
        ViETest::Log(" Number of errors: %d", numberOfErrors);
        ViETest::Log("========================================");
        ViETest::Log(" ");
        return numberOfErrors;
    }

    ViETest::Log(" ");
    ViETest::Log(" ViEEncryption API Test PASSED!");
    ViETest::Log("========================================");
    ViETest::Log(" ");
    return 0;
}
