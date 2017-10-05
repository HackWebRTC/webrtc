/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef LOGGING_RTC_EVENT_LOG_MOCK_MOCK_RTC_EVENT_LOG_H_
#define LOGGING_RTC_EVENT_LOG_MOCK_MOCK_RTC_EVENT_LOG_H_

#include <memory>
#include <string>

#include "logging/rtc_event_log/rtc_event_log.h"
#include "logging/rtc_event_log/rtc_stream_config.h"
#include "modules/audio_coding/audio_network_adaptor/include/audio_network_adaptor.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"
#include "modules/rtp_rtcp/source/rtp_packet_to_send.h"
#include "test/gmock.h"

namespace webrtc {

class MockRtcEventLog : public RtcEventLog {
 public:
  virtual bool StartLogging(std::unique_ptr<RtcEventLogOutput> output) {
    return StartLoggingProxy(output.get());
  }
  MOCK_METHOD1(StartLoggingProxy, bool(RtcEventLogOutput*));

  MOCK_METHOD2(StartLogging,
               bool(const std::string& file_name, int64_t max_size_bytes));

  MOCK_METHOD2(StartLogging,
               bool(rtc::PlatformFile log_file, int64_t max_size_bytes));

  MOCK_METHOD0(StopLogging, void());

  virtual void Log(std::unique_ptr<RtcEvent> event) {
    return LogProxy(event.get());
  }
  MOCK_METHOD1(LogProxy, void(RtcEvent*));

  MOCK_METHOD1(LogVideoReceiveStreamConfig,
               void(const rtclog::StreamConfig& config));

  MOCK_METHOD1(LogVideoSendStreamConfig,
               void(const rtclog::StreamConfig& config));

  MOCK_METHOD1(LogAudioReceiveStreamConfig,
               void(const rtclog::StreamConfig& config));

  MOCK_METHOD1(LogAudioSendStreamConfig,
               void(const rtclog::StreamConfig& config));
  MOCK_METHOD3(LogRtpHeader,
               void(PacketDirection direction,
                    const uint8_t* header,
                    size_t packet_length));

  MOCK_METHOD4(LogRtpHeader,
               void(PacketDirection direction,
                    const uint8_t* header,
                    size_t packet_length,
                    int probe_cluster_id));

  MOCK_METHOD3(LogRtcpPacket,
               void(PacketDirection direction,
                    const uint8_t* packet,
                    size_t length));

  MOCK_METHOD1(LogIncomingRtpHeader, void(const RtpPacketReceived& packet));

  MOCK_METHOD2(LogOutgoingRtpHeader,
               void(const RtpPacketToSend& packet, int probe_cluster_id));

  MOCK_METHOD1(LogIncomingRtcpPacket,
               void(rtc::ArrayView<const uint8_t> packet));

  MOCK_METHOD1(LogOutgoingRtcpPacket,
               void(rtc::ArrayView<const uint8_t> packet));

  MOCK_METHOD1(LogAudioPlayout, void(uint32_t ssrc));

  MOCK_METHOD3(LogLossBasedBweUpdate,
               void(int32_t bitrate_bps,
                    uint8_t fraction_loss,
                    int32_t total_packets));

  MOCK_METHOD2(LogDelayBasedBweUpdate,
               void(int32_t bitrate_bps, BandwidthUsage detector_state));

  MOCK_METHOD1(LogAudioNetworkAdaptation,
               void(const AudioEncoderRuntimeConfig& config));

  MOCK_METHOD4(LogProbeClusterCreated,
               void(int id, int bitrate_bps, int min_probes, int min_bytes));

  MOCK_METHOD2(LogProbeResultSuccess, void(int id, int bitrate_bps));
  MOCK_METHOD2(LogProbeResultFailure,
               void(int id, ProbeFailureReason failure_reason));
};

}  // namespace webrtc

#endif  // LOGGING_RTC_EVENT_LOG_MOCK_MOCK_RTC_EVENT_LOG_H_
