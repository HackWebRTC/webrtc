// libjingle
// Copyright 2011 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "talk/session/media/rtcpmuxfilter.h"

#include "talk/base/gunit.h"
#include "talk/media/base/testutils.h"

TEST(RtcpMuxFilterTest, DemuxRtcpSender) {
  cricket::RtcpMuxFilter filter;
  const char data[] = { 0, 73, 0, 0 };
  const int len = 4;

  // Init state - refuse to demux
  EXPECT_FALSE(filter.DemuxRtcp(data, len));
  // After sent offer, demux should be enabled
  filter.SetOffer(true, cricket::CS_LOCAL);
  EXPECT_TRUE(filter.DemuxRtcp(data, len));
  // Remote accepted, demux should be enabled
  filter.SetAnswer(true, cricket::CS_REMOTE);
  EXPECT_TRUE(filter.DemuxRtcp(data, len));
}

TEST(RtcpMuxFilterTest, DemuxRtcpReceiver) {
  cricket::RtcpMuxFilter filter;
  const char data[] = { 0, 73, 0, 0 };
  const int len = 4;

  // Init state - refuse to demux
  EXPECT_FALSE(filter.DemuxRtcp(data, len));
  // After received offer, demux should not be enabled
  filter.SetOffer(true, cricket::CS_REMOTE);
  EXPECT_FALSE(filter.DemuxRtcp(data, len));
  // We accept, demux is now enabled
  filter.SetAnswer(true, cricket::CS_LOCAL);
  EXPECT_TRUE(filter.DemuxRtcp(data, len));
}

TEST(RtcpMuxFilterTest, DemuxRtcpSenderProvisionalAnswer) {
  cricket::RtcpMuxFilter filter;
  const char data[] = { 0, 73, 0, 0 };
  const int len = 4;

  filter.SetOffer(true, cricket::CS_REMOTE);
  // Received provisional answer without mux enabled.
  filter.SetProvisionalAnswer(false, cricket::CS_LOCAL);
  EXPECT_FALSE(filter.DemuxRtcp(data, len));
  // Received provisional answer with mux enabled.
  filter.SetProvisionalAnswer(true, cricket::CS_LOCAL);
  EXPECT_TRUE(filter.DemuxRtcp(data, len));
  // Remote accepted, demux should be enabled.
  filter.SetAnswer(true, cricket::CS_LOCAL);
  EXPECT_TRUE(filter.DemuxRtcp(data, len));
}

TEST(RtcpMuxFilterTest, DemuxRtcpReceiverProvisionalAnswer) {
  cricket::RtcpMuxFilter filter;
  const char data[] = { 0, 73, 0, 0 };
  const int len = 4;

  filter.SetOffer(true, cricket::CS_LOCAL);
  // Received provisional answer without mux enabled.
  filter.SetProvisionalAnswer(false, cricket::CS_REMOTE);
  // After sent offer, demux should be enabled until we have received a
  // final answer.
  EXPECT_TRUE(filter.DemuxRtcp(data, len));
  // Received provisional answer with mux enabled.
  filter.SetProvisionalAnswer(true, cricket::CS_REMOTE);
  EXPECT_TRUE(filter.DemuxRtcp(data, len));
  // Remote accepted, demux should be enabled.
  filter.SetAnswer(true, cricket::CS_REMOTE);
  EXPECT_TRUE(filter.DemuxRtcp(data, len));
}

TEST(RtcpMuxFilterTest, IsActiveSender) {
  cricket::RtcpMuxFilter filter;
  // Init state - not active
  EXPECT_FALSE(filter.IsActive());
  // After sent offer, demux should not be active.
  filter.SetOffer(true, cricket::CS_LOCAL);
  EXPECT_FALSE(filter.IsActive());
  // Remote accepted, filter is now active.
  filter.SetAnswer(true, cricket::CS_REMOTE);
  EXPECT_TRUE(filter.IsActive());
}

// Test that we can receive provisional answer and final answer.
TEST(RtcpMuxFilterTest, ReceivePrAnswer) {
  cricket::RtcpMuxFilter filter;
  filter.SetOffer(true, cricket::CS_LOCAL);
  // Received provisional answer with mux enabled.
  EXPECT_TRUE(filter.SetProvisionalAnswer(true, cricket::CS_REMOTE));
  // We are now active since both sender and receiver support mux.
  EXPECT_TRUE(filter.IsActive());
  // Received provisional answer with mux disabled.
  EXPECT_TRUE(filter.SetProvisionalAnswer(false, cricket::CS_REMOTE));
  // We are now inactive since the receiver doesn't support mux.
  EXPECT_FALSE(filter.IsActive());
  // Received final answer with mux enabled.
  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.IsActive());
}

TEST(RtcpMuxFilterTest, IsActiveReceiver) {
  cricket::RtcpMuxFilter filter;
  // Init state - not active.
  EXPECT_FALSE(filter.IsActive());
  // After received offer, demux should not be active
  filter.SetOffer(true, cricket::CS_REMOTE);
  EXPECT_FALSE(filter.IsActive());
  // We accept, filter is now active
  filter.SetAnswer(true, cricket::CS_LOCAL);
  EXPECT_TRUE(filter.IsActive());
}

// Test that we can send provisional answer and final answer.
TEST(RtcpMuxFilterTest, SendPrAnswer) {
  cricket::RtcpMuxFilter filter;
  filter.SetOffer(true, cricket::CS_REMOTE);
  // Send provisional answer with mux enabled.
  EXPECT_TRUE(filter.SetProvisionalAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
  // Received provisional answer with mux disabled.
  EXPECT_TRUE(filter.SetProvisionalAnswer(false, cricket::CS_LOCAL));
  EXPECT_FALSE(filter.IsActive());
  // Send final answer with mux enabled.
  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
}

// Test that we can enable the filter in an update.
// We can not disable the filter later since that would mean we need to
// recreate a rtcp transport channel.
TEST(RtcpMuxFilterTest, EnableFilterDuringUpdate) {
  cricket::RtcpMuxFilter filter;
  EXPECT_FALSE(filter.IsActive());
  EXPECT_TRUE(filter.SetOffer(false, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(false, cricket::CS_LOCAL));
  EXPECT_FALSE(filter.IsActive());

  EXPECT_TRUE(filter.SetOffer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_FALSE(filter.SetOffer(false, cricket::CS_REMOTE));
  EXPECT_FALSE(filter.SetAnswer(false, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
}

// Test that SetOffer can be called twice.
TEST(RtcpMuxFilterTest, SetOfferTwice) {
  cricket::RtcpMuxFilter filter;

  EXPECT_TRUE(filter.SetOffer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetOffer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  cricket::RtcpMuxFilter filter2;
  EXPECT_TRUE(filter2.SetOffer(false, cricket::CS_LOCAL));
  EXPECT_TRUE(filter2.SetOffer(false, cricket::CS_LOCAL));
  EXPECT_TRUE(filter2.SetAnswer(false, cricket::CS_REMOTE));
  EXPECT_FALSE(filter2.IsActive());
}

// Test that the filter can be enabled twice.
TEST(RtcpMuxFilterTest, EnableFilterTwiceDuringUpdate) {
  cricket::RtcpMuxFilter filter;

  EXPECT_TRUE(filter.SetOffer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());

  EXPECT_TRUE(filter.SetOffer(true, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(true, cricket::CS_LOCAL));
  EXPECT_TRUE(filter.IsActive());
}

// Test that the filter can be kept disabled during updates.
TEST(RtcpMuxFilterTest, KeepFilterDisabledDuringUpdate) {
  cricket::RtcpMuxFilter filter;

  EXPECT_TRUE(filter.SetOffer(false, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(false, cricket::CS_LOCAL));
  EXPECT_FALSE(filter.IsActive());

  EXPECT_TRUE(filter.SetOffer(false, cricket::CS_REMOTE));
  EXPECT_TRUE(filter.SetAnswer(false, cricket::CS_LOCAL));
  EXPECT_FALSE(filter.IsActive());
}
