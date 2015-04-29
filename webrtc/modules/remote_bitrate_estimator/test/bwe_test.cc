/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test.h"

#include "webrtc/base/common.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/interface/module_common_types.h"
#include "webrtc/modules/remote_bitrate_estimator/test/bwe_test_framework.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_receiver.h"
#include "webrtc/modules/remote_bitrate_estimator/test/packet_sender.h"
#include "webrtc/system_wrappers/interface/clock.h"
#include "webrtc/test/testsupport/perf_test.h"

using std::string;
using std::vector;

namespace webrtc {
namespace testing {
namespace bwe {

PacketProcessorRunner::PacketProcessorRunner(PacketProcessor* processor)
    : processor_(processor) {
}

PacketProcessorRunner::~PacketProcessorRunner() {
  for (Packet* packet : queue_)
    delete packet;
}

bool PacketProcessorRunner::RunsProcessor(
    const PacketProcessor* processor) const {
  return processor == processor_;
}

void PacketProcessorRunner::RunFor(int64_t time_ms,
                                   int64_t time_now_ms,
                                   Packets* in_out) {
  Packets to_process;
  FindPacketsToProcess(processor_->flow_ids(), in_out, &to_process);
  processor_->RunFor(time_ms, &to_process);
  QueuePackets(&to_process, time_now_ms * 1000);
  if (!to_process.empty()) {
    processor_->Plot((to_process.back()->send_time_us() + 500) / 1000);
  }
  in_out->merge(to_process, DereferencingComparator<Packet>);
}

void PacketProcessorRunner::FindPacketsToProcess(const FlowIds& flow_ids,
                                                 Packets* in,
                                                 Packets* out) {
  assert(out->empty());
  for (Packets::iterator it = in->begin(); it != in->end();) {
    // TODO(holmer): Further optimize this by looking for consecutive flow ids
    // in the packet list and only doing the binary search + splice once for a
    // sequence.
    if (flow_ids.find((*it)->flow_id()) != flow_ids.end()) {
      Packets::iterator next = it;
      ++next;
      out->splice(out->end(), *in, it);
      it = next;
    } else {
      ++it;
    }
  }
}

void PacketProcessorRunner::QueuePackets(Packets* batch,
                                         int64_t end_of_batch_time_us) {
  queue_.merge(*batch, DereferencingComparator<Packet>);
  if (queue_.empty()) {
    return;
  }
  Packets::iterator it = queue_.begin();
  for (; it != queue_.end(); ++it) {
    if ((*it)->send_time_us() > end_of_batch_time_us) {
      break;
    }
  }
  Packets to_transfer;
  to_transfer.splice(to_transfer.begin(), queue_, queue_.begin(), it);
  batch->merge(to_transfer, DereferencingComparator<Packet>);
}

BweTest::BweTest()
    : run_time_ms_(0), time_now_ms_(-1), simulation_interval_ms_(-1) {
  links_.push_back(&uplink_);
  links_.push_back(&downlink_);
}

BweTest::~BweTest() {
  for (Packet* packet : packets_)
    delete packet;
}

void BweTest::SetUp() {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  string test_name =
      string(test_info->test_case_name()) + "_" + string(test_info->name());
  BWE_TEST_LOGGING_GLOBAL_CONTEXT(test_name);
  BWE_TEST_LOGGING_GLOBAL_ENABLE(false);
}

void Link::AddPacketProcessor(PacketProcessor* processor,
                              ProcessorType processor_type) {
  assert(processor);
  switch (processor_type) {
    case kSender:
      senders_.push_back(static_cast<PacketSender*>(processor));
      break;
    case kReceiver:
      receivers_.push_back(static_cast<PacketReceiver*>(processor));
      break;
    case kRegular:
      break;
  }
  processors_.push_back(PacketProcessorRunner(processor));
}

void Link::RemovePacketProcessor(PacketProcessor* processor) {
  for (vector<PacketProcessorRunner>::iterator it = processors_.begin();
       it != processors_.end(); ++it) {
    if (it->RunsProcessor(processor)) {
      processors_.erase(it);
      return;
    }
  }
  assert(false);
}

// Ownership of the created packets is handed over to the caller.
void Link::Run(int64_t run_for_ms, int64_t now_ms, Packets* packets) {
  for (auto& processor : processors_) {
    processor.RunFor(run_for_ms, now_ms, packets);
  }
}

void BweTest::VerboseLogging(bool enable) {
  BWE_TEST_LOGGING_GLOBAL_ENABLE(enable);
}

void BweTest::RunFor(int64_t time_ms) {
  // Set simulation interval from first packet sender.
  // TODO(holmer): Support different feedback intervals for different flows.
  if (!uplink_.senders().empty()) {
    simulation_interval_ms_ = uplink_.senders()[0]->GetFeedbackIntervalMs();
  } else if (!downlink_.senders().empty()) {
    simulation_interval_ms_ = downlink_.senders()[0]->GetFeedbackIntervalMs();
  }
  assert(simulation_interval_ms_ > 0);
  if (time_now_ms_ == -1) {
    time_now_ms_ = simulation_interval_ms_;
  }
  for (run_time_ms_ += time_ms;
       time_now_ms_ <= run_time_ms_ - simulation_interval_ms_;
       time_now_ms_ += simulation_interval_ms_) {
    // Packets are first generated on the first link, passed through all the
    // PacketProcessors and PacketReceivers. The PacketReceivers produces
    // FeedbackPackets which are then processed by the next link, where they
    // at some point will be consumed by a PacketSender.
    for (Link* link : links_)
      link->Run(simulation_interval_ms_, time_now_ms_, &packets_);
  }
}

string BweTest::GetTestName() const {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  return string(test_info->name());
}

void BweTest::PrintResults(double max_throughput_kbps,
                           Stats<double> throughput_kbps,
                           int flow_id,
                           Stats<double> flow_delay_ms,
                           Stats<double> flow_throughput_kbps) {
  std::map<int, Stats<double>> flow_delays_ms;
  flow_delays_ms[flow_id] = flow_delay_ms;
  std::map<int, Stats<double>> flow_throughputs_kbps;
  flow_throughputs_kbps[flow_id] = flow_throughput_kbps;
  PrintResults(max_throughput_kbps, throughput_kbps, flow_delays_ms,
               flow_throughputs_kbps);
}

void BweTest::PrintResults(double max_throughput_kbps,
                           Stats<double> throughput_kbps,
                           std::map<int, Stats<double>> flow_delay_ms,
                           std::map<int, Stats<double>> flow_throughput_kbps) {
  double utilization = throughput_kbps.GetMean() / max_throughput_kbps;
  webrtc::test::PrintResult("BwePerformance", GetTestName(), "Utilization",
                            utilization * 100.0, "%", false);
  std::stringstream ss;
  ss << throughput_kbps.GetStdDev() / throughput_kbps.GetMean();
  webrtc::test::PrintResult("BwePerformance", GetTestName(),
                            "Utilization var coeff", ss.str(), "", false);
  for (auto& kv : flow_throughput_kbps) {
    ss.str("");
    ss << "Throughput flow " << kv.first;
    webrtc::test::PrintResultMeanAndError("BwePerformance", GetTestName(),
                                          ss.str(), kv.second.AsString(),
                                          "kbps", false);
  }
  for (auto& kv : flow_delay_ms) {
    ss.str("");
    ss << "Delay flow " << kv.first;
    webrtc::test::PrintResultMeanAndError("BwePerformance", GetTestName(),
                                          ss.str(), kv.second.AsString(), "ms",
                                          false);
  }
  double fairness_index = 1.0;
  if (!flow_throughput_kbps.empty()) {
    double squared_bitrate_sum = 0.0;
    fairness_index = 0.0;
    for (auto kv : flow_throughput_kbps) {
      squared_bitrate_sum += kv.second.GetMean() * kv.second.GetMean();
      fairness_index += kv.second.GetMean();
    }
    fairness_index *= fairness_index;
    fairness_index /= flow_throughput_kbps.size() * squared_bitrate_sum;
  }
  webrtc::test::PrintResult("BwePerformance", GetTestName(), "Fairness",
                            fairness_index * 100, "%", false);
}

void BweTest::RunFairnessTest(BandwidthEstimatorType bwe_type,
                              size_t num_media_flows,
                              size_t num_tcp_flows,
                              int64_t run_time_seconds,
                              int capacity_kbps,
                              int max_delay_ms) {
  std::set<int> all_flow_ids;
  std::set<int> media_flow_ids;
  std::set<int> tcp_flow_ids;
  int next_flow_id = 0;
  for (size_t i = 0; i < num_media_flows; ++i) {
    media_flow_ids.insert(next_flow_id);
    all_flow_ids.insert(next_flow_id);
    ++next_flow_id;
  }
  for (size_t i = 0; i < num_tcp_flows; ++i) {
    tcp_flow_ids.insert(next_flow_id);
    all_flow_ids.insert(next_flow_id);
    ++next_flow_id;
  }

  std::vector<VideoSource*> sources;
  std::vector<PacketSender*> senders;

  size_t i = 0;
  for (int media_flow : media_flow_ids) {
    // Streams started 20 seconds apart to give them different advantage when
    // competing for the bandwidth.
    const int64_t kFlowStartOffsetMs = i++ * (rand() % 10000);
    sources.push_back(new AdaptiveVideoSource(media_flow, 30, 300, 0,
                                              kFlowStartOffsetMs));
    senders.push_back(new PacedVideoSender(&uplink_, sources.back(), bwe_type));
  }

  const int64_t kTcpStartOffsetMs = 5000;
  for (int tcp_flow : tcp_flow_ids)
    senders.push_back(new TcpSender(&uplink_, tcp_flow, kTcpStartOffsetMs));

  ChokeFilter choke(&uplink_, all_flow_ids);
  choke.SetCapacity(capacity_kbps);
  choke.SetMaxDelay(max_delay_ms);

  DelayFilter delay_uplink(&uplink_, all_flow_ids);
  delay_uplink.SetDelayMs(25);

  std::vector<RateCounterFilter*> rate_counters;
  for (int flow : all_flow_ids) {
    rate_counters.push_back(
        new RateCounterFilter(&uplink_, flow, "receiver_input"));
  }

  RateCounterFilter total_utilization(&uplink_, all_flow_ids,
                                      "total_utilization");

  std::vector<PacketReceiver*> receivers;
  i = 0;
  for (int media_flow : media_flow_ids) {
    receivers.push_back(
        new PacketReceiver(&uplink_, media_flow, bwe_type, i++ == 0, false));
  }
  for (int tcp_flow : tcp_flow_ids) {
    receivers.push_back(
        new PacketReceiver(&uplink_, tcp_flow, kTcpEstimator, false, false));
  }

  DelayFilter delay_downlink(&downlink_, all_flow_ids);
  delay_downlink.SetDelayMs(25);

  RunFor(run_time_seconds * 1000);

  std::map<int, Stats<double>> flow_throughput_kbps;
  for (RateCounterFilter* rate_counter : rate_counters) {
    int flow_id = *rate_counter->flow_ids().begin();
    flow_throughput_kbps[flow_id] = rate_counter->GetBitrateStats();
  }

  std::map<int, Stats<double>> flow_delay_ms;
  for (PacketReceiver* receiver : receivers) {
    int flow_id = *receiver->flow_ids().begin();
    flow_delay_ms[flow_id] = receiver->GetDelayStats();
  }

  PrintResults(capacity_kbps, total_utilization.GetBitrateStats(),
               flow_delay_ms, flow_throughput_kbps);

  for (VideoSource* source : sources)
    delete source;
  for (PacketSender* sender : senders)
    delete sender;
  for (RateCounterFilter* rate_counter : rate_counters)
    delete rate_counter;
  for (PacketReceiver* receiver : receivers)
    delete receiver;
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
