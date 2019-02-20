/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "test/scenario/call_client.h"

#include <utility>

#include "absl/memory/memory.h"
#include "modules/audio_mixer/audio_mixer_impl.h"
#include "modules/congestion_controller/goog_cc/test/goog_cc_printer.h"

namespace webrtc {
namespace test {
namespace {
static constexpr size_t kNumSsrcs = 6;
const uint32_t kSendRtxSsrcs[kNumSsrcs] = {0xBADCAFD, 0xBADCAFE, 0xBADCAFF,
                                           0xBADCB00, 0xBADCB01, 0xBADCB02};
const uint32_t kVideoSendSsrcs[kNumSsrcs] = {0xC0FFED, 0xC0FFEE, 0xC0FFEF,
                                             0xC0FFF0, 0xC0FFF1, 0xC0FFF2};
const uint32_t kVideoRecvLocalSsrcs[kNumSsrcs] = {0xDAB001, 0xDAB002, 0xDAB003,
                                                  0xDAB004, 0xDAB005, 0xDAB006};
const uint32_t kAudioSendSsrc = 0xDEADBEEF;
const uint32_t kReceiverLocalAudioSsrc = 0x1234567;

const char* kPriorityStreamId = "priority-track";

CallClientFakeAudio InitAudio() {
  CallClientFakeAudio setup;
  auto capturer = TestAudioDeviceModule::CreatePulsedNoiseCapturer(256, 48000);
  auto renderer = TestAudioDeviceModule::CreateDiscardRenderer(48000);
  setup.fake_audio_device = TestAudioDeviceModule::CreateTestAudioDeviceModule(
      std::move(capturer), std::move(renderer), 1.f);
  setup.apm = AudioProcessingBuilder().Create();
  setup.fake_audio_device->Init();
  AudioState::Config audio_state_config;
  audio_state_config.audio_mixer = AudioMixerImpl::Create();
  audio_state_config.audio_processing = setup.apm;
  audio_state_config.audio_device_module = setup.fake_audio_device;
  setup.audio_state = AudioState::Create(audio_state_config);
  setup.fake_audio_device->RegisterAudioCallback(
      setup.audio_state->audio_transport());
  return setup;
}

Call* CreateCall(CallClientConfig config,
                 LoggingNetworkControllerFactory* network_controller_factory_,
                 rtc::scoped_refptr<AudioState> audio_state) {
  CallConfig call_config(network_controller_factory_->GetEventLog());
  call_config.bitrate_config.max_bitrate_bps =
      config.transport.rates.max_rate.bps_or(-1);
  call_config.bitrate_config.min_bitrate_bps =
      config.transport.rates.min_rate.bps();
  call_config.bitrate_config.start_bitrate_bps =
      config.transport.rates.start_rate.bps();
  call_config.network_controller_factory = network_controller_factory_;
  call_config.audio_state = audio_state;
  return Call::Create(call_config);
}
}

LoggingNetworkControllerFactory::LoggingNetworkControllerFactory(
    LogWriterFactoryInterface* log_writer_factory,
    TransportControllerConfig config) {
  std::unique_ptr<RtcEventLogOutput> cc_out;
  if (!log_writer_factory) {
    event_log_ = RtcEventLog::CreateNull();
  } else {
    event_log_ = RtcEventLog::Create(RtcEventLog::EncodingType::Legacy);
    bool success = event_log_->StartLogging(
        log_writer_factory->Create(".rtc.dat"), RtcEventLog::kImmediateOutput);
    RTC_CHECK(success);
    cc_out = log_writer_factory->Create(".cc_state.txt");
  }
  switch (config.cc) {
    case TransportControllerConfig::CongestionController::kGoogCc:
      if (cc_out) {
        auto goog_printer = absl::make_unique<GoogCcStatePrinter>();
        owned_cc_factory_.reset(
            new GoogCcDebugFactory(event_log_.get(), goog_printer.get()));
        cc_printer_.reset(new ControlStatePrinter(std::move(cc_out),
                                                  std::move(goog_printer)));
      } else {
        owned_cc_factory_.reset(
            new GoogCcNetworkControllerFactory(event_log_.get()));
      }
      break;
    case TransportControllerConfig::CongestionController::kGoogCcFeedback:
      if (cc_out) {
        auto goog_printer = absl::make_unique<GoogCcStatePrinter>();
        owned_cc_factory_.reset(new GoogCcFeedbackDebugFactory(
            event_log_.get(), goog_printer.get()));
        cc_printer_.reset(new ControlStatePrinter(std::move(cc_out),
                                                  std::move(goog_printer)));
      } else {
        owned_cc_factory_.reset(
            new GoogCcFeedbackNetworkControllerFactory(event_log_.get()));
      }
      break;
    case TransportControllerConfig::CongestionController::kInjected:
      cc_factory_ = config.cc_factory;
      if (cc_out)
        RTC_LOG(LS_WARNING)
            << "Can't log controller state for injected network controllers";
      break;
  }
  if (cc_printer_)
    cc_printer_->PrintHeaders();
  if (owned_cc_factory_) {
    RTC_DCHECK(!cc_factory_);
    cc_factory_ = owned_cc_factory_.get();
  }
}

LoggingNetworkControllerFactory::~LoggingNetworkControllerFactory() {
}

void LoggingNetworkControllerFactory::LogCongestionControllerStats(
    Timestamp at_time) {
  if (cc_printer_)
    cc_printer_->PrintState(at_time);
}

RtcEventLog* LoggingNetworkControllerFactory::GetEventLog() const {
  return event_log_.get();
}

std::unique_ptr<NetworkControllerInterface>
LoggingNetworkControllerFactory::Create(NetworkControllerConfig config) {
  return cc_factory_->Create(config);
}

TimeDelta LoggingNetworkControllerFactory::GetProcessInterval() const {
  return cc_factory_->GetProcessInterval();
}

CallClient::CallClient(
    Clock* clock,
    std::unique_ptr<LogWriterFactoryInterface> log_writer_factory,
    CallClientConfig config)
    : clock_(clock),
      log_writer_factory_(std::move(log_writer_factory)),
      network_controller_factory_(log_writer_factory_.get(), config.transport),
      fake_audio_setup_(InitAudio()),
      call_(CreateCall(config,
                       &network_controller_factory_,
                       fake_audio_setup_.audio_state)),
      transport_(clock_, call_.get()),
      header_parser_(RtpHeaderParser::Create()) {}

CallClient::~CallClient() {
  delete header_parser_;
}

ColumnPrinter CallClient::StatsPrinter() {
  return ColumnPrinter::Lambda(
      "pacer_delay call_send_bw",
      [this](rtc::SimpleStringBuilder& sb) {
        Call::Stats call_stats = call_->GetStats();
        sb.AppendFormat("%.3lf %.0lf", call_stats.pacer_delay_ms / 1000.0,
                        call_stats.send_bandwidth_bps / 8.0);
      },
      64);
}

Call::Stats CallClient::GetStats() {
  return call_->GetStats();
}

void CallClient::OnPacketReceived(EmulatedIpPacket packet) {
  // Removes added overhead before delivering packet to sender.
  RTC_DCHECK_GE(packet.data.size(),
                route_overhead_.at(packet.dest_endpoint_id).bytes());
  packet.data.SetSize(packet.data.size() -
                      route_overhead_.at(packet.dest_endpoint_id).bytes());

  MediaType media_type = MediaType::ANY;
  if (!RtpHeaderParser::IsRtcp(packet.cdata(), packet.data.size())) {
    RTPHeader header;
    bool success =
        header_parser_->Parse(packet.cdata(), packet.data.size(), &header);
    if (!success) {
      RTC_DLOG(LS_ERROR) << "Failed to parse RTP header of packet";
      return;
    }
    media_type = ssrc_media_types_[header.ssrc];
  }
  call_->Receiver()->DeliverPacket(media_type, packet.data,
                                   packet.arrival_time.us());
}

std::unique_ptr<RtcEventLogOutput> CallClient::GetLogWriter(std::string name) {
  if (!log_writer_factory_ || name.empty())
    return nullptr;
  return log_writer_factory_->Create(name);
}

uint32_t CallClient::GetNextVideoSsrc() {
  RTC_CHECK_LT(next_video_ssrc_index_, kNumSsrcs);
  return kVideoSendSsrcs[next_video_ssrc_index_++];
}

uint32_t CallClient::GetNextVideoLocalSsrc() {
  RTC_CHECK_LT(next_video_local_ssrc_index_, kNumSsrcs);
  return kVideoRecvLocalSsrcs[next_video_local_ssrc_index_++];
}

uint32_t CallClient::GetNextAudioSsrc() {
  RTC_CHECK_LT(next_audio_ssrc_index_, 1);
  next_audio_ssrc_index_++;
  return kAudioSendSsrc;
}

uint32_t CallClient::GetNextAudioLocalSsrc() {
  RTC_CHECK_LT(next_audio_local_ssrc_index_, 1);
  next_audio_local_ssrc_index_++;
  return kReceiverLocalAudioSsrc;
}

uint32_t CallClient::GetNextRtxSsrc() {
  RTC_CHECK_LT(next_rtx_ssrc_index_, kNumSsrcs);
  return kSendRtxSsrcs[next_rtx_ssrc_index_++];
}

std::string CallClient::GetNextPriorityId() {
  RTC_CHECK_LT(next_priority_index_++, 1);
  return kPriorityStreamId;
}

void CallClient::AddExtensions(std::vector<RtpExtension> extensions) {
  for (const auto& extension : extensions)
    header_parser_->RegisterRtpHeaderExtension(extension);
}

CallClientPair::~CallClientPair() = default;

}  // namespace test
}  // namespace webrtc
