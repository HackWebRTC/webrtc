/*
 * libjingle
 * Copyright 2011, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/base/gunit.h"
#include "talk/base/rollingaccumulator.h"

namespace talk_base {

namespace {

const double kLearningRate = 0.5;

}  // namespace

TEST(RollingAccumulatorTest, ZeroSamples) {
  RollingAccumulator<int> accum(10);

  EXPECT_EQ(0U, accum.count());
  EXPECT_DOUBLE_EQ(0.0, accum.ComputeMean());
  EXPECT_DOUBLE_EQ(0.0, accum.ComputeVariance());
  EXPECT_EQ(0, accum.ComputeMin());
  EXPECT_EQ(0, accum.ComputeMax());
}

TEST(RollingAccumulatorTest, SomeSamples) {
  RollingAccumulator<int> accum(10);
  for (int i = 0; i < 4; ++i) {
    accum.AddSample(i);
  }

  EXPECT_EQ(4U, accum.count());
  EXPECT_EQ(6, accum.ComputeSum());
  EXPECT_DOUBLE_EQ(1.5, accum.ComputeMean());
  EXPECT_NEAR(2.26666, accum.ComputeWeightedMean(kLearningRate), 0.01);
  EXPECT_DOUBLE_EQ(1.25, accum.ComputeVariance());
  EXPECT_EQ(0, accum.ComputeMin());
  EXPECT_EQ(3, accum.ComputeMax());
}

TEST(RollingAccumulatorTest, RollingSamples) {
  RollingAccumulator<int> accum(10);
  for (int i = 0; i < 12; ++i) {
    accum.AddSample(i);
  }

  EXPECT_EQ(10U, accum.count());
  EXPECT_EQ(65, accum.ComputeSum());
  EXPECT_DOUBLE_EQ(6.5, accum.ComputeMean());
  EXPECT_NEAR(10.0, accum.ComputeWeightedMean(kLearningRate), 0.01);
  EXPECT_NEAR(9.0, accum.ComputeVariance(), 1.0);
  EXPECT_EQ(2, accum.ComputeMin());
  EXPECT_EQ(11, accum.ComputeMax());
}

TEST(RollingAccumulatorTest, ResetSamples) {
  RollingAccumulator<int> accum(10);

  for (int i = 0; i < 10; ++i) {
    accum.AddSample(100);
  }
  EXPECT_EQ(10U, accum.count());
  EXPECT_DOUBLE_EQ(100.0, accum.ComputeMean());
  EXPECT_EQ(100, accum.ComputeMin());
  EXPECT_EQ(100, accum.ComputeMax());

  accum.Reset();
  EXPECT_EQ(0U, accum.count());

  for (int i = 0; i < 5; ++i) {
    accum.AddSample(i);
  }

  EXPECT_EQ(5U, accum.count());
  EXPECT_EQ(10, accum.ComputeSum());
  EXPECT_DOUBLE_EQ(2.0, accum.ComputeMean());
  EXPECT_EQ(0, accum.ComputeMin());
  EXPECT_EQ(4, accum.ComputeMax());
}

TEST(RollingAccumulatorTest, RollingSamplesDouble) {
  RollingAccumulator<double> accum(10);
  for (int i = 0; i < 23; ++i) {
    accum.AddSample(5 * i);
  }

  EXPECT_EQ(10u, accum.count());
  EXPECT_DOUBLE_EQ(875.0, accum.ComputeSum());
  EXPECT_DOUBLE_EQ(87.5, accum.ComputeMean());
  EXPECT_NEAR(105.049, accum.ComputeWeightedMean(kLearningRate), 0.1);
  EXPECT_NEAR(229.166667, accum.ComputeVariance(), 25);
  EXPECT_DOUBLE_EQ(65.0, accum.ComputeMin());
  EXPECT_DOUBLE_EQ(110.0, accum.ComputeMax());
}

TEST(RollingAccumulatorTest, ComputeWeightedMeanCornerCases) {
  RollingAccumulator<int> accum(10);
  EXPECT_DOUBLE_EQ(0.0, accum.ComputeWeightedMean(kLearningRate));
  EXPECT_DOUBLE_EQ(0.0, accum.ComputeWeightedMean(0.0));
  EXPECT_DOUBLE_EQ(0.0, accum.ComputeWeightedMean(1.1));

  for (int i = 0; i < 8; ++i) {
    accum.AddSample(i);
  }

  EXPECT_DOUBLE_EQ(3.5, accum.ComputeMean());
  EXPECT_DOUBLE_EQ(3.5, accum.ComputeWeightedMean(0));
  EXPECT_DOUBLE_EQ(3.5, accum.ComputeWeightedMean(1.1));
  EXPECT_NEAR(6.0, accum.ComputeWeightedMean(kLearningRate), 0.1);
}

}  // namespace talk_base
