/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/*
 * This file includes the implementation of the VAD unit tests.
 */

#include <cstring>
#include "unit_test.h"
#include "webrtc_vad.h"


class VadEnvironment : public ::testing::Environment {
 public:
  virtual void SetUp() {
  }

  virtual void TearDown() {
  }
};

VadTest::VadTest()
{
}

void VadTest::SetUp() {
}

void VadTest::TearDown() {
}

TEST_F(VadTest, ApiTest) {
    VadInst *vad_inst;
    int i, j, k;
    short zeros[960];
    short speech[960];
    char version[32];

    // Valid test cases
    int fs[3] = {8000, 16000, 32000};
    int nMode[4] = {0, 1, 2, 3};
    int framelen[3][3] = {{80, 160, 240},
    {160, 320, 480}, {320, 640, 960}} ;
    int vad_counter = 0;

    memset(zeros, 0, sizeof(short) * 960);
    memset(speech, 1, sizeof(short) * 960);
    speech[13] = 1374;
    speech[73] = -3747;



    // WebRtcVad_get_version()
    WebRtcVad_get_version(version);
    //printf("API Test for %s\n", version);

    // Null instance tests
    EXPECT_EQ(-1, WebRtcVad_Create(NULL));
    EXPECT_EQ(-1, WebRtcVad_Init(NULL));
    EXPECT_EQ(-1, WebRtcVad_Assign(NULL, NULL));
    EXPECT_EQ(-1, WebRtcVad_Free(NULL));
    EXPECT_EQ(-1, WebRtcVad_set_mode(NULL, nMode[0]));
    EXPECT_EQ(-1, WebRtcVad_Process(NULL, fs[0], speech,  framelen[0][0]));


    EXPECT_EQ(WebRtcVad_Create(&vad_inst), 0);

    // Not initialized tests
    EXPECT_EQ(-1, WebRtcVad_Process(vad_inst, fs[0], speech,  framelen[0][0]));
    EXPECT_EQ(-1, WebRtcVad_set_mode(vad_inst, nMode[0]));

    // WebRtcVad_Init() tests
    EXPECT_EQ(WebRtcVad_Init(vad_inst), 0);

    // WebRtcVad_set_mode() tests
    EXPECT_EQ(-1, WebRtcVad_set_mode(vad_inst, -1));
    EXPECT_EQ(-1, WebRtcVad_set_mode(vad_inst, 4));

    for (i = 0; i < sizeof(nMode)/sizeof(nMode[0]); i++) {
        EXPECT_EQ(WebRtcVad_set_mode(vad_inst, nMode[i]), 0);
    }

    // WebRtcVad_Process() tests
    EXPECT_EQ(-1, WebRtcVad_Process(vad_inst, fs[0], NULL,  framelen[0][0]));
    EXPECT_EQ(-1, WebRtcVad_Process(vad_inst, 12000, speech,  framelen[0][0]));
    EXPECT_EQ(-1, WebRtcVad_Process(vad_inst, fs[0], speech,  framelen[1][1]));
    EXPECT_EQ(WebRtcVad_Process(vad_inst, fs[0], zeros,  framelen[0][0]), 0);
    for (i = 0; i < sizeof(fs)/sizeof(fs[0]); i++) {
        for (j = 0; j < sizeof(framelen[0])/sizeof(framelen[0][0]); j++) {
            for (k = 0; k < sizeof(nMode)/sizeof(nMode[0]); k++) {
                EXPECT_EQ(WebRtcVad_set_mode(vad_inst, nMode[k]), 0);
//                printf("%d\n", WebRtcVad_Process(vad_inst, fs[i], speech,  framelen[i][j]));
                if (vad_counter < 9)
                {
                    EXPECT_EQ(WebRtcVad_Process(vad_inst, fs[i], speech,  framelen[i][j]), 1);
                } else
                {
                    EXPECT_EQ(WebRtcVad_Process(vad_inst, fs[i], speech,  framelen[i][j]), 0);
                }
                vad_counter++;
            }
        }
    }

    EXPECT_EQ(0, WebRtcVad_Free(vad_inst));

}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  VadEnvironment* env = new VadEnvironment;
  ::testing::AddGlobalTestEnvironment(env);

  return RUN_ALL_TESTS();
}
