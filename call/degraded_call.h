/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef CALL_DEGRADED_CALL_H_
#define CALL_DEGRADED_CALL_H_

#include <stddef.h>
#include <stdint.h>
#include <memory>

#include "absl/types/optional.h"
#include "api/call/transport.h"
#include "api/fec_controller.h"
#include "api/media_types.h"
#include "api/rtp_headers.h"
#include "api/test/simulated_network.h"
#include "api/video_codecs/video_encoder_config.h"
#include "call/audio_receive_stream.h"
#include "call/audio_send_stream.h"
#include "call/call.h"
#include "call/fake_network_pipe.h"
#include "call/flexfec_receive_stream.h"
#include "call/packet_receiver.h"
#include "call/rtp_transport_controller_send_interface.h"
#include "call/simulated_network.h"
#include "call/video_receive_stream.h"
#include "call/video_send_stream.h"
#include "modules/include/module.h"
#include "modules/utility/include/process_thread.h"
#include "rtc_base/bitrate_allocation_strategy.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/network/sent_packet.h"
#include "system_wrappers/include/clock.h"

namespace webrtc {
class FakeNetworkPipeModule : public Module {
 public:
  FakeNetworkPipeModule(
      Clock* clock,
      std::unique_ptr<NetworkBehaviorInterface> network_behavior,
      Transport* transport);
  ~FakeNetworkPipeModule() override;
  void SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options);
  void SendRtcp(const uint8_t* packet, size_t length);

  // Implements Module interface
  int64_t TimeUntilNextProcess() override;
  void ProcessThreadAttached(ProcessThread* process_thread) override;
  void Process() override;

 private:
  void MaybeResumeProcess();
  FakeNetworkPipe pipe_;
  rtc::CriticalSection process_thread_lock_;
  ProcessThread* process_thread_ RTC_GUARDED_BY(process_thread_lock_) = nullptr;
  bool pending_process_ RTC_GUARDED_BY(process_thread_lock_) = false;
};

class DegradedCall : public Call, private Transport, private PacketReceiver {
 public:
  explicit DegradedCall(
      std::unique_ptr<Call> call,
      absl::optional<BuiltInNetworkBehaviorConfig> send_config,
      absl::optional<BuiltInNetworkBehaviorConfig> receive_config);
  ~DegradedCall() override;

  // Implements Call.
  AudioSendStream* CreateAudioSendStream(
      const AudioSendStream::Config& config) override;
  void DestroyAudioSendStream(AudioSendStream* send_stream) override;

  AudioReceiveStream* CreateAudioReceiveStream(
      const AudioReceiveStream::Config& config) override;
  void DestroyAudioReceiveStream(AudioReceiveStream* receive_stream) override;

  VideoSendStream* CreateVideoSendStream(
      VideoSendStream::Config config,
      VideoEncoderConfig encoder_config) override;
  VideoSendStream* CreateVideoSendStream(
      VideoSendStream::Config config,
      VideoEncoderConfig encoder_config,
      std::unique_ptr<FecController> fec_controller) override;
  void DestroyVideoSendStream(VideoSendStream* send_stream) override;

  VideoReceiveStream* CreateVideoReceiveStream(
      VideoReceiveStream::Config configuration) override;
  void DestroyVideoReceiveStream(VideoReceiveStream* receive_stream) override;

  FlexfecReceiveStream* CreateFlexfecReceiveStream(
      const FlexfecReceiveStream::Config& config) override;
  void DestroyFlexfecReceiveStream(
      FlexfecReceiveStream* receive_stream) override;

  PacketReceiver* Receiver() override;

  RtpTransportControllerSendInterface* GetTransportControllerSend() override;

  Stats GetStats() const override;

  void SetBitrateAllocationStrategy(
      std::unique_ptr<rtc::BitrateAllocationStrategy>
          bitrate_allocation_strategy) override;

  void SignalChannelNetworkState(MediaType media, NetworkState state) override;
  void OnAudioTransportOverheadChanged(
      int transport_overhead_per_packet) override;
  void OnSentPacket(const rtc::SentPacket& sent_packet) override;

 protected:
  // Implements Transport.
  bool SendRtp(const uint8_t* packet,
               size_t length,
               const PacketOptions& options) override;

  bool SendRtcp(const uint8_t* packet, size_t length) override;

  // Implements PacketReceiver.
  DeliveryStatus DeliverPacket(MediaType media_type,
                               rtc::CopyOnWriteBuffer packet,
                               int64_t packet_time_us) override;

 private:
  Clock* const clock_;
  const std::unique_ptr<Call> call_;

  void MediaTransportChange(MediaTransportInterface* media_transport) override;
  void SetClientBitratePreferences(
      const webrtc::BitrateSettings& preferences) override {}
  const absl::optional<BuiltInNetworkBehaviorConfig> send_config_;
  const std::unique_ptr<ProcessThread> send_process_thread_;
  SimulatedNetwork* send_simulated_network_;
  std::unique_ptr<FakeNetworkPipeModule> send_pipe_;
  size_t num_send_streams_;

  const absl::optional<BuiltInNetworkBehaviorConfig> receive_config_;
  SimulatedNetwork* receive_simulated_network_;
  std::unique_ptr<FakeNetworkPipe> receive_pipe_;
};

}  // namespace webrtc

#endif  // CALL_DEGRADED_CALL_H_
