/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <map>
#include <string>
#include <vector>
#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/remote_bitrate_estimator/include/remote_bitrate_estimator.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"

namespace webrtc {

namespace testing {
namespace bwe {

class BweReceiver;
class PacketReceiver;
class PacketProcessorRunner;

class PacketReceiver : public PacketProcessor {
 public:
  PacketReceiver(PacketProcessorListener* listener,
                 int flow_id,
                 BandwidthEstimatorType bwe_type,
                 bool plot_delay,
                 bool plot_bwe);
  ~PacketReceiver();

  // Implements PacketProcessor.
  virtual void RunFor(int64_t time_ms, Packets* in_out) OVERRIDE;

  FeedbackPacket* GetFeedback();

  void LogStats();

 protected:
  void PlotDelay(int64_t arrival_time_ms, int64_t send_time_ms);

  std::string delay_log_prefix_;
  int64_t last_delay_plot_ms_;
  bool plot_delay_;
  scoped_ptr<BweReceiver> bwe_receiver_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PacketReceiver);
};

class BweTest : public PacketProcessorListener {
 public:
  BweTest();
  virtual ~BweTest();

  virtual void AddPacketProcessor(PacketProcessor* processor,
                                  ProcessorType type);
  virtual void RemovePacketProcessor(PacketProcessor* processor);

 protected:
  virtual void SetUp();

  void VerboseLogging(bool enable);
  void RunFor(int64_t time_ms);
  std::string GetTestName() const;

 private:
  void FindPacketsToProcess(const FlowIds& flow_ids, Packets* in,
                            Packets* out);
  void GiveFeedbackToAffectedSenders(PacketReceiver* receiver);

  int64_t run_time_ms_;
  int64_t time_now_ms_;
  int64_t simulation_interval_ms_;
  Packets previous_packets_;
  std::vector<PacketSender*> senders_;
  std::vector<PacketReceiver*> receivers_;
  std::vector<PacketProcessorRunner> processors_;

  DISALLOW_COPY_AND_ASSIGN(BweTest);
};
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
