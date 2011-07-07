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
 * This file contains the SPL unit_test.
 *
 */

#include "unit_test.h"
#include "signal_processing_library.h"

class SplEnvironment : public ::testing::Environment {
 public:
  virtual void SetUp() {
  }
  virtual void TearDown() {
  }
};

SplTest::SplTest()
{
}

void SplTest::SetUp() {
}

void SplTest::TearDown() {
}

TEST_F(SplTest, MacroTest) {
    // Macros with inputs.
    int A = 10;
    int B = 21;
    int a = -3;
    int b = WEBRTC_SPL_WORD32_MAX;
    int nr = 2;
    int d_ptr1 = 0;
    int d_ptr2 = 0;

    EXPECT_EQ(10, WEBRTC_SPL_MIN(A, B));
    EXPECT_EQ(21, WEBRTC_SPL_MAX(A, B));

    EXPECT_EQ(3, WEBRTC_SPL_ABS_W16(a));
    EXPECT_EQ(3, WEBRTC_SPL_ABS_W32(a));
    EXPECT_EQ(0, WEBRTC_SPL_GET_BYTE(&B, nr));
    WEBRTC_SPL_SET_BYTE(&d_ptr2, 1, nr);
    EXPECT_EQ(65536, d_ptr2);

    EXPECT_EQ(-63, WEBRTC_SPL_MUL(a, B));
    EXPECT_EQ(-2147483645, WEBRTC_SPL_MUL(a, b));
    EXPECT_EQ(-2147483645, WEBRTC_SPL_UMUL(a, b));
    b = WEBRTC_SPL_WORD16_MAX >> 1;
    EXPECT_EQ(65535, WEBRTC_SPL_UMUL_RSFT16(a, b));
    EXPECT_EQ(1073627139, WEBRTC_SPL_UMUL_16_16(a, b));
    EXPECT_EQ(16382, WEBRTC_SPL_UMUL_16_16_RSFT16(a, b));
    EXPECT_EQ(-49149, WEBRTC_SPL_UMUL_32_16(a, b));
    EXPECT_EQ(65535, WEBRTC_SPL_UMUL_32_16_RSFT16(a, b));
    EXPECT_EQ(-49149, WEBRTC_SPL_MUL_16_U16(a, b));

    a = b;
    b = -3;
    EXPECT_EQ(-5461, WEBRTC_SPL_DIV(a, b));
    EXPECT_EQ(0, WEBRTC_SPL_UDIV(a, b));

    EXPECT_EQ(-1, WEBRTC_SPL_MUL_16_32_RSFT16(a, b));
    EXPECT_EQ(-1, WEBRTC_SPL_MUL_16_32_RSFT15(a, b));
    EXPECT_EQ(-3, WEBRTC_SPL_MUL_16_32_RSFT14(a, b));
    EXPECT_EQ(-24, WEBRTC_SPL_MUL_16_32_RSFT11(a, b));

    int a32 = WEBRTC_SPL_WORD32_MAX;
    int a32a = (WEBRTC_SPL_WORD32_MAX >> 16);
    int a32b = (WEBRTC_SPL_WORD32_MAX & 0x0000ffff);
    EXPECT_EQ(5, WEBRTC_SPL_MUL_32_32_RSFT32(a32a, a32b, A));
    EXPECT_EQ(5, WEBRTC_SPL_MUL_32_32_RSFT32BI(a32, A));

    EXPECT_EQ(-49149, WEBRTC_SPL_MUL_16_16(a, b));
    EXPECT_EQ(-12288, WEBRTC_SPL_MUL_16_16_RSFT(a, b, 2));

    EXPECT_EQ(-12287, WEBRTC_SPL_MUL_16_16_RSFT_WITH_ROUND(a, b, 2));
    EXPECT_EQ(-1, WEBRTC_SPL_MUL_16_16_RSFT_WITH_FIXROUND(a, b));

    EXPECT_EQ(16380, WEBRTC_SPL_ADD_SAT_W32(a, b));
    EXPECT_EQ(21, WEBRTC_SPL_SAT(a, A, B));
    EXPECT_EQ(21, WEBRTC_SPL_SAT(a, B, A));
    EXPECT_EQ(-49149, WEBRTC_SPL_MUL_32_16(a, b));

    EXPECT_EQ(16386, WEBRTC_SPL_SUB_SAT_W32(a, b));
    EXPECT_EQ(16380, WEBRTC_SPL_ADD_SAT_W16(a, b));
    EXPECT_EQ(16386, WEBRTC_SPL_SUB_SAT_W16(a, b));

    EXPECT_TRUE(WEBRTC_SPL_IS_NEG(b));

    // Shifting with negative numbers allowed
    // Positive means left shift
    EXPECT_EQ(32766, WEBRTC_SPL_SHIFT_W16(a, 1));
    EXPECT_EQ(32766, WEBRTC_SPL_SHIFT_W32(a, 1));

    // Shifting with negative numbers not allowed
    // We cannot do casting here due to signed/unsigned problem
    EXPECT_EQ(8191, WEBRTC_SPL_RSHIFT_W16(a, 1));
    EXPECT_EQ(32766, WEBRTC_SPL_LSHIFT_W16(a, 1));
    EXPECT_EQ(8191, WEBRTC_SPL_RSHIFT_W32(a, 1));
    EXPECT_EQ(32766, WEBRTC_SPL_LSHIFT_W32(a, 1));

    EXPECT_EQ(8191, WEBRTC_SPL_RSHIFT_U16(a, 1));
    EXPECT_EQ(32766, WEBRTC_SPL_LSHIFT_U16(a, 1));
    EXPECT_EQ(8191, WEBRTC_SPL_RSHIFT_U32(a, 1));
    EXPECT_EQ(32766, WEBRTC_SPL_LSHIFT_U32(a, 1));

    EXPECT_EQ(1470, WEBRTC_SPL_RAND(A));
}

TEST_F(SplTest, InlineTest) {

    WebRtc_Word16 a = 121;
    WebRtc_Word16 b = -17;
    WebRtc_Word32 A = 111121;
    WebRtc_Word32 B = -1711;
    char* bVersion = (char*) malloc(8);

    EXPECT_EQ(104, WebRtcSpl_AddSatW16(a, b));
    EXPECT_EQ(138, WebRtcSpl_SubSatW16(a, b));

    EXPECT_EQ(109410, WebRtcSpl_AddSatW32(A, B));
    EXPECT_EQ(112832, WebRtcSpl_SubSatW32(A, B));

    EXPECT_EQ(17, WebRtcSpl_GetSizeInBits(A));
    EXPECT_EQ(14, WebRtcSpl_NormW32(A));
    EXPECT_EQ(4, WebRtcSpl_NormW16(B));
    EXPECT_EQ(15, WebRtcSpl_NormU32(A));

    EXPECT_EQ(0, WebRtcSpl_get_version(bVersion, 8));
}

TEST_F(SplTest, MathOperationsTest) {

    int A = 117;
    WebRtc_Word32 num = 117;
    WebRtc_Word32 den = -5;
    WebRtc_UWord16 denU = 5;
    EXPECT_EQ(10, WebRtcSpl_Sqrt(A));
    EXPECT_EQ(10, WebRtcSpl_SqrtFloor(A));


    EXPECT_EQ(-91772805, WebRtcSpl_DivResultInQ31(den, num));
    EXPECT_EQ(-23, WebRtcSpl_DivW32W16ResW16(num, (WebRtc_Word16)den));
    EXPECT_EQ(-23, WebRtcSpl_DivW32W16(num, (WebRtc_Word16)den));
    EXPECT_EQ(23, WebRtcSpl_DivU32U16(num, denU));
    EXPECT_EQ(0, WebRtcSpl_DivW32HiLow(128, 0, 256));
}

TEST_F(SplTest, BasicArrayOperationsTest) {


    int B[] = {4, 12, 133, 1100};
    int Bs[] = {2, 6, 66, 550};
    WebRtc_UWord8* b8 = (WebRtc_UWord8*) malloc(4);
    WebRtc_Word16* b16 = (WebRtc_Word16*) malloc(4);
    WebRtc_Word32* b32 = (WebRtc_Word32*) malloc(4);

    WebRtc_UWord8* bTmp8 = (WebRtc_UWord8*) malloc(4);
    WebRtc_Word16* bTmp16 = (WebRtc_Word16*) malloc(4);
    WebRtc_Word32* bTmp32 = (WebRtc_Word32*) malloc(4);

    WebRtcSpl_MemSetW16(b16, 3, 4);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(3, b16[kk]);
    }
    EXPECT_EQ(4, WebRtcSpl_ZerosArrayW16(b16, 4));
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(0, b16[kk]);
    }
    EXPECT_EQ(4, WebRtcSpl_OnesArrayW16(b16, 4));
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(1, b16[kk]);
    }
    WebRtcSpl_MemSetW32(b32, 3, 4);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(3, b32[kk]);
    }
    EXPECT_EQ(4, WebRtcSpl_ZerosArrayW32(b32, 4));
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(0, b32[kk]);
    }
    EXPECT_EQ(4, WebRtcSpl_OnesArrayW32(b32, 4));
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(1, b32[kk]);
    }
    for (int kk = 0; kk < 4; ++kk) {
        bTmp8[kk] = (WebRtc_Word8)kk;
        bTmp16[kk] = (WebRtc_Word16)kk;
        bTmp32[kk] = (WebRtc_Word32)kk;
    }
    WEBRTC_SPL_MEMCPY_W8(b8, bTmp8, 4);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(b8[kk], bTmp8[kk]);
    }
    WEBRTC_SPL_MEMCPY_W16(b16, bTmp16, 4);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(b16[kk], bTmp16[kk]);
    }
//    WEBRTC_SPL_MEMCPY_W32(b32, bTmp32, 4);
//    for (int kk = 0; kk < 4; ++kk) {
//        EXPECT_EQ(b32[kk], bTmp32[kk]);
//    }
    EXPECT_EQ(2, WebRtcSpl_CopyFromEndW16(b16, 4, 2, bTmp16));
    for (int kk = 0; kk < 2; ++kk) {
        EXPECT_EQ(kk+2, bTmp16[kk]);
    }

    for (int kk = 0; kk < 4; ++kk) {
        b32[kk] = B[kk];
        b16[kk] = (WebRtc_Word16)B[kk];
    }
    WebRtcSpl_VectorBitShiftW32ToW16(bTmp16, 4, b32, 1);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ((B[kk]>>1), bTmp16[kk]);
    }
    WebRtcSpl_VectorBitShiftW16(bTmp16, 4, b16, 1);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ((B[kk]>>1), bTmp16[kk]);
    }
    WebRtcSpl_VectorBitShiftW32(bTmp32, 4, b32, 1);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ((B[kk]>>1), bTmp32[kk]);
    }

    WebRtcSpl_MemCpyReversedOrder(&bTmp16[3], b16, 4);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(b16[3-kk], bTmp16[kk]);
    }

}

TEST_F(SplTest, MinMaxOperationsTest) {


    int B[] = {4, 12, 133, -1100};
    WebRtc_Word16* b16 = (WebRtc_Word16*) malloc(4);
    WebRtc_Word32* b32 = (WebRtc_Word32*) malloc(4);

    for (int kk = 0; kk < 4; ++kk) {
        b16[kk] = B[kk];
        b32[kk] = B[kk];
    }

    EXPECT_EQ(1100, WebRtcSpl_MaxAbsValueW16(b16, 4));
    EXPECT_EQ(1100, WebRtcSpl_MaxAbsValueW32(b32, 4));
    EXPECT_EQ(133, WebRtcSpl_MaxValueW16(b16, 4));
    EXPECT_EQ(133, WebRtcSpl_MaxValueW32(b32, 4));
    EXPECT_EQ(3, WebRtcSpl_MaxAbsIndexW16(b16, 4));
    EXPECT_EQ(2, WebRtcSpl_MaxIndexW16(b16, 4));
    EXPECT_EQ(2, WebRtcSpl_MaxIndexW32(b32, 4));

    EXPECT_EQ(-1100, WebRtcSpl_MinValueW16(b16, 4));
    EXPECT_EQ(-1100, WebRtcSpl_MinValueW32(b32, 4));
    EXPECT_EQ(3, WebRtcSpl_MinIndexW16(b16, 4));
    EXPECT_EQ(3, WebRtcSpl_MinIndexW32(b32, 4));

    EXPECT_EQ(0, WebRtcSpl_GetScalingSquare(b16, 4, 1));

}

TEST_F(SplTest, VectorOperationsTest) {


    int B[] = {4, 12, 133, 1100};
    WebRtc_Word16* a16 = (WebRtc_Word16*) malloc(4);
    WebRtc_Word16* b16 = (WebRtc_Word16*) malloc(4);
    WebRtc_Word32* b32 = (WebRtc_Word32*) malloc(4);
    WebRtc_Word16* bTmp16 = (WebRtc_Word16*) malloc(4);

    for (int kk = 0; kk < 4; ++kk) {
        a16[kk] = B[kk];
        b16[kk] = B[kk];
    }

    WebRtcSpl_AffineTransformVector(bTmp16, b16, 3, 7, 2, 4);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ((B[kk]*3+7)>>2, bTmp16[kk]);
    }
    WebRtcSpl_ScaleAndAddVectorsWithRound(b16, 3, b16, 2, 2, bTmp16, 4);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ((B[kk]*3+B[kk]*2+2)>>2, bTmp16[kk]);
    }

    WebRtcSpl_AddAffineVectorToVector(bTmp16, b16, 3, 7, 2, 4);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(((B[kk]*3+B[kk]*2+2)>>2)+((b16[kk]*3+7)>>2), bTmp16[kk]);
    }

    WebRtcSpl_CrossCorrelation(b32, b16, bTmp16, 4, 2, 2, 0);
    for (int kk = 0; kk < 2; ++kk) {
        EXPECT_EQ(614236, b32[kk]);
    }
//    EXPECT_EQ(, WebRtcSpl_DotProduct(b16, bTmp16, 4));
    EXPECT_EQ(306962, WebRtcSpl_DotProductWithScale(b16, b16, 4, 2));

    WebRtcSpl_ScaleVector(b16, bTmp16, 13, 4, 2);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ((b16[kk]*13)>>2, bTmp16[kk]);
    }
    WebRtcSpl_ScaleVectorWithSat(b16, bTmp16, 13, 4, 2);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ((b16[kk]*13)>>2, bTmp16[kk]);
    }
    WebRtcSpl_ScaleAndAddVectors(a16, 13, 2, b16, 7, 2, bTmp16, 4);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(((a16[kk]*13)>>2)+((b16[kk]*7)>>2), bTmp16[kk]);
    }

    WebRtcSpl_AddVectorsAndShift(bTmp16, a16, b16, 4, 2);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(B[kk] >> 1, bTmp16[kk]);
    }
    WebRtcSpl_ReverseOrderMultArrayElements(bTmp16, a16, &b16[3], 4, 2);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ((a16[kk]*b16[3-kk])>>2, bTmp16[kk]);
    }
    WebRtcSpl_ElementwiseVectorMult(bTmp16, a16, b16, 4, 6);
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ((a16[kk]*b16[kk])>>6, bTmp16[kk]);
    }

    WebRtcSpl_SqrtOfOneMinusXSquared(b16, 4, bTmp16);
    for (int kk = 0; kk < 3; ++kk) {
        EXPECT_EQ(32767, bTmp16[kk]);
    }
    EXPECT_EQ(32749, bTmp16[3]);
}

TEST_F(SplTest, EstimatorsTest) {


    int B[] = {4, 12, 133, 1100};
    WebRtc_Word16* b16 = (WebRtc_Word16*) malloc(4);
    WebRtc_Word32* b32 = (WebRtc_Word32*) malloc(4);
    WebRtc_Word16* bTmp16 = (WebRtc_Word16*) malloc(4);

    for (int kk = 0; kk < 4; ++kk) {
        b16[kk] = B[kk];
        b32[kk] = B[kk];
    }

    EXPECT_EQ(0, WebRtcSpl_LevinsonDurbin(b32, b16, bTmp16, 2));

}

TEST_F(SplTest, FilterTest) {


    WebRtc_Word16 A[] = {1, 2, 33, 100};
    WebRtc_Word16 A5[] = {1, 2, 33, 100, -5};
    WebRtc_Word16 B[] = {4, 12, 133, 110};
    WebRtc_Word16* b16 = (WebRtc_Word16*) malloc(4);
    WebRtc_Word16* bTmp16 = (WebRtc_Word16*) malloc(4);
    WebRtc_Word16* bTmp16Low = (WebRtc_Word16*) malloc(4);
    WebRtc_Word16* bState = (WebRtc_Word16*) malloc(4);
    WebRtc_Word16* bStateLow = (WebRtc_Word16*) malloc(4);

    WebRtcSpl_ZerosArrayW16(bState, 4);
    WebRtcSpl_ZerosArrayW16(bStateLow, 4);

    for (int kk = 0; kk < 4; ++kk) {
        b16[kk] = A[kk];
    }

    // MA filters
    WebRtcSpl_FilterMAFastQ12(b16, bTmp16, B, 4, 4);
    for (int kk = 0; kk < 4; ++kk) {
        //EXPECT_EQ(aTmp16[kk], bTmp16[kk]);
    }
    // AR filters
    WebRtcSpl_FilterARFastQ12(b16, bTmp16, A, 4, 4);
    for (int kk = 0; kk < 4; ++kk) {
//        EXPECT_EQ(aTmp16[kk], bTmp16[kk]);
    }
    EXPECT_EQ(4, WebRtcSpl_FilterAR(A5, 5, b16, 4, bState, 4, bStateLow, 4, bTmp16, bTmp16Low, 4));

}

TEST_F(SplTest, RandTest) {


    WebRtc_Word16 BU[] = {3653, 12446, 8525, 30691};
    WebRtc_Word16 BN[] = {3459, -11689, -258, -3738};
    WebRtc_Word16* b16 = (WebRtc_Word16*) malloc(4);
    WebRtc_UWord32* bSeed = (WebRtc_UWord32*) malloc(1);

    bSeed[0] = 100000;

    EXPECT_EQ(464449057, WebRtcSpl_IncreaseSeed(bSeed));
    EXPECT_EQ(31565, WebRtcSpl_RandU(bSeed));
    EXPECT_EQ(-9786, WebRtcSpl_RandN(bSeed));
    EXPECT_EQ(4, WebRtcSpl_RandUArray(b16, 4, bSeed));
    for (int kk = 0; kk < 4; ++kk) {
        EXPECT_EQ(BU[kk], b16[kk]);
    }
}

TEST_F(SplTest, SignalProcessingTest) {


    int A[] = {1, 2, 33, 100};
    WebRtc_Word16* b16 = (WebRtc_Word16*) malloc(4);
    WebRtc_Word32* b32 = (WebRtc_Word32*) malloc(4);

    WebRtc_Word16* bTmp16 = (WebRtc_Word16*) malloc(4);
    WebRtc_Word32* bTmp32 = (WebRtc_Word32*) malloc(4);

    int bScale = 0;

    for (int kk = 0; kk < 4; ++kk) {
        b16[kk] = A[kk];
        b32[kk] = A[kk];
    }

    EXPECT_EQ(2, WebRtcSpl_AutoCorrelation(b16, 4, 1, bTmp32, &bScale));
    WebRtcSpl_ReflCoefToLpc(b16, 4, bTmp16);
//    for (int kk = 0; kk < 4; ++kk) {
//        EXPECT_EQ(aTmp16[kk], bTmp16[kk]);
//    }
    WebRtcSpl_LpcToReflCoef(bTmp16, 4, b16);
//    for (int kk = 0; kk < 4; ++kk) {
//        EXPECT_EQ(a16[kk], b16[kk]);
//    }
    WebRtcSpl_AutoCorrToReflCoef(b32, 4, bTmp16);
//    for (int kk = 0; kk < 4; ++kk) {
//        EXPECT_EQ(aTmp16[kk], bTmp16[kk]);
//    }
    WebRtcSpl_GetHanningWindow(bTmp16, 4);
//    for (int kk = 0; kk < 4; ++kk) {
//        EXPECT_EQ(aTmp16[kk], bTmp16[kk]);
//    }

    for (int kk = 0; kk < 4; ++kk) {
        b16[kk] = A[kk];
    }
    EXPECT_EQ(11094 , WebRtcSpl_Energy(b16, 4, &bScale));
    EXPECT_EQ(0, bScale);
}

TEST_F(SplTest, FFTTest) {


    WebRtc_Word16 B[] = {1, 2, 33, 100,
            2, 3, 34, 101,
            3, 4, 35, 102,
            4, 5, 36, 103};

    EXPECT_EQ(0, WebRtcSpl_ComplexFFT(B, 3, 1));
//    for (int kk = 0; kk < 16; ++kk) {
//        EXPECT_EQ(A[kk], B[kk]);
//    }
    EXPECT_EQ(0, WebRtcSpl_ComplexIFFT(B, 3, 1));
//    for (int kk = 0; kk < 16; ++kk) {
//        EXPECT_EQ(A[kk], B[kk]);
//    }
    WebRtcSpl_ComplexBitReverse(B, 3);
    for (int kk = 0; kk < 16; ++kk) {
        //EXPECT_EQ(A[kk], B[kk]);
    }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  SplEnvironment* env = new SplEnvironment;
  ::testing::AddGlobalTestEnvironment(env);

  return RUN_ALL_TESTS();
}
