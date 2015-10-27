/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_AUDIO_FAKE_VOICE_ENGINE_H_
#define WEBRTC_AUDIO_FAKE_VOICE_ENGINE_H_

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc {
namespace test {

// NOTE: This class inherits from VoiceEngineImpl so that its clients will be
// able to get the various interfaces as usual, via T::GetInterface().
class FakeVoiceEngine final : public VoiceEngineImpl {
 public:
  static const int kSendChannelId;
  static const int kRecvChannelId;
  static const uint32_t kSendSsrc;
  static const uint32_t kRecvSsrc;
  static const int kSendEchoDelayMedian;
  static const int kSendEchoDelayStdDev;
  static const int kSendEchoReturnLoss;
  static const int kSendEchoReturnLossEnhancement;
  static const int kRecvJitterBufferDelay;
  static const int kRecvPlayoutBufferDelay;
  static const unsigned int kSendSpeechInputLevel;
  static const unsigned int kRecvSpeechOutputLevel;
  static const CallStatistics kSendCallStats;
  static const CodecInst kSendCodecInst;
  static const ReportBlock kSendReportBlock;
  static const CallStatistics kRecvCallStats;
  static const CodecInst kRecvCodecInst;
  static const NetworkStatistics kRecvNetworkStats;
  static const AudioDecodingCallStats kRecvAudioDecodingCallStats;

  FakeVoiceEngine() : VoiceEngineImpl(new Config(), true) {
    // Increase ref count so this object isn't automatically deleted whenever
    // interfaces are Release():d.
    ++_ref_count;
  }
  ~FakeVoiceEngine() override {
    // Decrease ref count before base class d-tor is called; otherwise it will
    // trigger an assertion.
    --_ref_count;
  }

  // VoEAudioProcessing
  int SetNsStatus(bool enable, NsModes mode = kNsUnchanged) override {
    return -1;
  }
  int GetNsStatus(bool& enabled, NsModes& mode) override { return -1; }
  int SetAgcStatus(bool enable, AgcModes mode = kAgcUnchanged) override {
    return -1;
  }
  int GetAgcStatus(bool& enabled, AgcModes& mode) override { return -1; }
  int SetAgcConfig(AgcConfig config) override { return -1; }
  int GetAgcConfig(AgcConfig& config) override { return -1; }
  int SetEcStatus(bool enable, EcModes mode = kEcUnchanged) override {
    return -1;
  }
  int GetEcStatus(bool& enabled, EcModes& mode) override { return -1; }
  int EnableDriftCompensation(bool enable) override { return -1; }
  bool DriftCompensationEnabled() override { return false; }
  void SetDelayOffsetMs(int offset) override {}
  int DelayOffsetMs() override { return -1; }
  int SetAecmMode(AecmModes mode = kAecmSpeakerphone,
                  bool enableCNG = true) override { return -1; }
  int GetAecmMode(AecmModes& mode, bool& enabledCNG) override { return -1; }
  int EnableHighPassFilter(bool enable) override { return -1; }
  bool IsHighPassFilterEnabled() override { return false; }
  int SetRxNsStatus(int channel,
                    bool enable,
                    NsModes mode = kNsUnchanged) override { return -1; }
  int GetRxNsStatus(int channel, bool& enabled, NsModes& mode) override {
    return -1;
  }
  int SetRxAgcStatus(int channel,
                     bool enable,
                     AgcModes mode = kAgcUnchanged) override { return -1; }
  int GetRxAgcStatus(int channel, bool& enabled, AgcModes& mode) override {
    return -1;
  }
  int SetRxAgcConfig(int channel, AgcConfig config) override { return -1; }
  int GetRxAgcConfig(int channel, AgcConfig& config) override { return -1; }
  int RegisterRxVadObserver(int channel,
                            VoERxVadCallback& observer) override { return -1; }
  int DeRegisterRxVadObserver(int channel) override { return -1; }
  int VoiceActivityIndicator(int channel) override { return -1; }
  int SetEcMetricsStatus(bool enable) override { return -1; }
  int GetEcMetricsStatus(bool& enabled) override {
    enabled = true;
    return 0;
  }
  int GetEchoMetrics(int& ERL, int& ERLE, int& RERL, int& A_NLP) override {
    ERL = kSendEchoReturnLoss;
    ERLE = kSendEchoReturnLossEnhancement;
    RERL = -123456789;
    A_NLP = 123456789;
    return 0;
  }
  int GetEcDelayMetrics(int& delay_median,
                        int& delay_std,
                        float& fraction_poor_delays) override {
    delay_median = kSendEchoDelayMedian;
    delay_std = kSendEchoDelayStdDev;
    fraction_poor_delays = -12345.7890f;
    return 0;
  }
  int StartDebugRecording(const char* fileNameUTF8) override { return -1; }
  int StartDebugRecording(FILE* file_handle) override { return -1; }
  int StopDebugRecording() override { return -1; }
  int SetTypingDetectionStatus(bool enable) override { return -1; }
  int GetTypingDetectionStatus(bool& enabled) override { return -1; }
  int TimeSinceLastTyping(int& seconds) override { return -1; }
  int SetTypingDetectionParameters(int timeWindow,
                                   int costPerTyping,
                                   int reportingThreshold,
                                   int penaltyDecay,
                                   int typeEventDelay = 0) override {
    return -1;
  }
  void EnableStereoChannelSwapping(bool enable) override {}
  bool IsStereoChannelSwappingEnabled() override { return false; }

  // VoEBase
  int RegisterVoiceEngineObserver(VoiceEngineObserver& observer) override {
    return -1;
  }
  int DeRegisterVoiceEngineObserver() override { return -1; }
  int Init(AudioDeviceModule* external_adm = NULL,
           AudioProcessing* audioproc = NULL) override { return -1; }
  AudioProcessing* audio_processing() override { return nullptr; }
  int Terminate() override { return -1; }
  int CreateChannel() override { return -1; }
  int CreateChannel(const Config& config) override { return -1; }
  int DeleteChannel(int channel) override { return -1; }
  int StartReceive(int channel) override { return -1; }
  int StopReceive(int channel) override { return -1; }
  int StartPlayout(int channel) override { return -1; }
  int StopPlayout(int channel) override { return -1; }
  int StartSend(int channel) override { return -1; }
  int StopSend(int channel) override { return -1; }
  int GetVersion(char version[1024]) override { return -1; }
  int LastError() override { return -1; }
  AudioTransport* audio_transport() { return nullptr; }
  int AssociateSendChannel(int channel, int accociate_send_channel) override {
    return -1;
  }

  // VoECodec
  int NumOfCodecs() override { return -1; }
  int GetCodec(int index, CodecInst& codec) override { return -1; }
  int SetSendCodec(int channel, const CodecInst& codec) override { return -1; }
  int GetSendCodec(int channel, CodecInst& codec) override {
    EXPECT_EQ(channel, kSendChannelId);
    codec = kSendCodecInst;
    return 0;
  }
  int SetBitRate(int channel, int bitrate_bps) override { return -1; }
  int GetRecCodec(int channel, CodecInst& codec) override {
    EXPECT_EQ(channel, kRecvChannelId);
    codec = kRecvCodecInst;
    return 0;
  }
  int SetRecPayloadType(int channel, const CodecInst& codec) override {
    return -1;
  }
  int GetRecPayloadType(int channel, CodecInst& codec) override { return -1; }
  int SetSendCNPayloadType(int channel, int type,
      PayloadFrequencies frequency = kFreq16000Hz) override { return -1; }
  int SetVADStatus(int channel,
                   bool enable,
                   VadModes mode = kVadConventional,
                   bool disableDTX = false) override { return -1; }
  int GetVADStatus(int channel,
                   bool& enabled,
                   VadModes& mode,
                   bool& disabledDTX) override { return -1; }
  int SetOpusMaxPlaybackRate(int channel, int frequency_hz) override {
    return -1;
  }
  int SetOpusDtx(int channel, bool enable_dtx) override { return -1; }
  RtcEventLog* GetEventLog() override { return nullptr; }

  // VoEDtmf
  int SendTelephoneEvent(int channel,
                         int eventCode,
                         bool outOfBand = true,
                         int lengthMs = 160,
                         int attenuationDb = 10) override { return -1; }
  int SetSendTelephoneEventPayloadType(int channel,
                                       unsigned char type) override {
    return -1;
  }
  int GetSendTelephoneEventPayloadType(int channel,
                                       unsigned char& type) override {
    return -1;
  }
  int SetDtmfFeedbackStatus(bool enable,
                            bool directFeedback = false) override { return -1; }
  int GetDtmfFeedbackStatus(bool& enabled, bool& directFeedback) override {
    return -1;
  }
  int PlayDtmfTone(int eventCode,
                   int lengthMs = 200,
                   int attenuationDb = 10) override { return -1; }

  // VoEExternalMedia
  int RegisterExternalMediaProcessing(
      int channel,
      ProcessingTypes type,
      VoEMediaProcess& processObject) override { return -1; }
  int DeRegisterExternalMediaProcessing(int channel,
                                        ProcessingTypes type) override {
    return -1;
  }
  int GetAudioFrame(int channel,
                    int desired_sample_rate_hz,
                    AudioFrame* frame) override { return -1; }
  int SetExternalMixing(int channel, bool enable) override { return -1; }

  // VoEFile
  int StartPlayingFileLocally(
      int channel,
      const char fileNameUTF8[1024],
      bool loop = false,
      FileFormats format = kFileFormatPcm16kHzFile,
      float volumeScaling = 1.0,
      int startPointMs = 0,
      int stopPointMs = 0) override { return -1; }
  int StartPlayingFileLocally(
      int channel,
      InStream* stream,
      FileFormats format = kFileFormatPcm16kHzFile,
      float volumeScaling = 1.0,
      int startPointMs = 0,
      int stopPointMs = 0) override { return -1; }
  int StopPlayingFileLocally(int channel) override { return -1; }
  int IsPlayingFileLocally(int channel) override { return -1; }
  int StartPlayingFileAsMicrophone(
      int channel,
      const char fileNameUTF8[1024],
      bool loop = false,
      bool mixWithMicrophone = false,
      FileFormats format = kFileFormatPcm16kHzFile,
      float volumeScaling = 1.0) override { return -1; }
  int StartPlayingFileAsMicrophone(
      int channel,
      InStream* stream,
      bool mixWithMicrophone = false,
      FileFormats format = kFileFormatPcm16kHzFile,
      float volumeScaling = 1.0) override { return -1; }
  int StopPlayingFileAsMicrophone(int channel) override { return -1; }
  int IsPlayingFileAsMicrophone(int channel) override { return -1; }
  int StartRecordingPlayout(int channel,
                            const char* fileNameUTF8,
                            CodecInst* compression = NULL,
                            int maxSizeBytes = -1) override { return -1; }
  int StopRecordingPlayout(int channel) override { return -1; }
  int StartRecordingPlayout(int channel,
                            OutStream* stream,
                            CodecInst* compression = NULL) override {
    return -1;
  }
  int StartRecordingMicrophone(const char* fileNameUTF8,
                               CodecInst* compression = NULL,
                               int maxSizeBytes = -1) override { return -1; }
  int StartRecordingMicrophone(OutStream* stream,
                                       CodecInst* compression = NULL) override {
    return -1;
  }
  int StopRecordingMicrophone() override { return -1; }

  // VoEHardware
  int GetNumOfRecordingDevices(int& devices) override { return -1; }

  // Gets the number of audio devices available for playout.
  int GetNumOfPlayoutDevices(int& devices) override { return -1; }

  // Gets the name of a specific recording device given by an |index|.
  // On Windows Vista/7, it also retrieves an additional unique ID
  // (GUID) for the recording device.
  int GetRecordingDeviceName(int index,
                             char strNameUTF8[128],
                             char strGuidUTF8[128]) override { return -1; }

  // Gets the name of a specific playout device given by an |index|.
  // On Windows Vista/7, it also retrieves an additional unique ID
  // (GUID) for the playout device.
  int GetPlayoutDeviceName(int index,
                           char strNameUTF8[128],
                           char strGuidUTF8[128]) override { return -1; }

  // Sets the audio device used for recording.
  int SetRecordingDevice(
      int index,
      StereoChannel recordingChannel = kStereoBoth) override { return -1; }

  // Sets the audio device used for playout.
  int SetPlayoutDevice(int index) override { return -1; }

  // Sets the type of audio device layer to use.
  int SetAudioDeviceLayer(AudioLayers audioLayer) override { return -1; }

  // Gets the currently used (active) audio device layer.
  int GetAudioDeviceLayer(AudioLayers& audioLayer) override { return -1; }

  // Native sample rate controls (samples/sec)
  int SetRecordingSampleRate(unsigned int samples_per_sec) override {
    return -1;
  }
  int RecordingSampleRate(unsigned int* samples_per_sec) const override {
    return -1;
  }
  int SetPlayoutSampleRate(unsigned int samples_per_sec) override {
    return -1;
  }
  int PlayoutSampleRate(unsigned int* samples_per_sec) const override {
    return -1;
  }

  // Queries and controls platform audio effects on Android devices.
  bool BuiltInAECIsAvailable() const override { return false; }
  int EnableBuiltInAEC(bool enable) override { return -1; }
  bool BuiltInAGCIsAvailable() const override { return false; }
  int EnableBuiltInAGC(bool enable) override { return -1; }
  bool BuiltInNSIsAvailable() const override { return false; }
  int EnableBuiltInNS(bool enable) override { return -1; }

  // VoENetwork
  int RegisterExternalTransport(int channel, Transport& transport) override {
    return -1;
  }
  int DeRegisterExternalTransport(int channel) override { return -1; }
  int ReceivedRTPPacket(int channel,
                        const void* data,
                        size_t length) override { return -1; }
  int ReceivedRTPPacket(int channel,
                        const void* data,
                        size_t length,
                        const PacketTime& packet_time) override { return -1; }
  int ReceivedRTCPPacket(int channel,
                         const void* data,
                         size_t length) { return -1; }

  // VoENetEqStats
  int GetNetworkStatistics(int channel, NetworkStatistics& stats) override {
    EXPECT_EQ(channel, kRecvChannelId);
    stats = kRecvNetworkStats;
    return 0;
  }
  int GetDecodingCallStatistics(int channel,
                                AudioDecodingCallStats* stats) const override {
    EXPECT_EQ(channel, kRecvChannelId);
    EXPECT_NE(nullptr, stats);
    *stats = kRecvAudioDecodingCallStats;
    return 0;
  }

  // VoERTP_RTCP
  int SetLocalSSRC(int channel, unsigned int ssrc) override { return -1; }
  int GetLocalSSRC(int channel, unsigned int& ssrc) override {
    EXPECT_EQ(channel, kSendChannelId);
    ssrc = 0;
    return 0;
  }
  int GetRemoteSSRC(int channel, unsigned int& ssrc) override {
    EXPECT_EQ(channel, kRecvChannelId);
    ssrc = 0;
    return 0;
  }
  int SetSendAudioLevelIndicationStatus(int channel,
                                        bool enable,
                                        unsigned char id = 1) override {
    return -1;
  }
  int SetSendAbsoluteSenderTimeStatus(int channel,
                                      bool enable,
                                      unsigned char id) override { return -1; }
  int SetReceiveAbsoluteSenderTimeStatus(int channel,
                                         bool enable,
                                         unsigned char id) override {
    return -1;
  }
  int SetRTCPStatus(int channel, bool enable) override { return -1; }
  int GetRTCPStatus(int channel, bool& enabled) override { return -1; }
  int SetRTCP_CNAME(int channel, const char cName[256]) override { return -1; }
  int GetRTCP_CNAME(int channel, char cName[256]) { return -1; }
  int GetRemoteRTCP_CNAME(int channel, char cName[256]) override { return -1; }
  int GetRemoteRTCPData(int channel,
                        unsigned int& NTPHigh,
                        unsigned int& NTPLow,
                        unsigned int& timestamp,
                        unsigned int& playoutTimestamp,
                        unsigned int* jitter = NULL,
                        unsigned short* fractionLost = NULL) override {
    return -1;
  }
  int GetRTPStatistics(int channel,
                       unsigned int& averageJitterMs,
                       unsigned int& maxJitterMs,
                       unsigned int& discardedPackets) override { return -1; }
  int GetRTCPStatistics(int channel, CallStatistics& stats) override {
    if (channel == kSendChannelId) {
      stats = kSendCallStats;
    } else {
      EXPECT_EQ(channel, kRecvChannelId);
      stats = kRecvCallStats;
    }
    return 0;
  }
  int GetRemoteRTCPReportBlocks(
      int channel,
      std::vector<ReportBlock>* receive_blocks) override {
    EXPECT_EQ(channel, kSendChannelId);
    EXPECT_NE(receive_blocks, nullptr);
    EXPECT_EQ(receive_blocks->size(), 0u);
    webrtc::ReportBlock block = kSendReportBlock;
    receive_blocks->push_back(block);   // Has wrong SSRC.
    block.source_SSRC = kSendSsrc;
    receive_blocks->push_back(block);   // Correct block.
    block.fraction_lost = 0;
    receive_blocks->push_back(block);   // Duplicate SSRC, bad fraction_lost.
    return 0;
  }
  int SetNACKStatus(int channel, bool enable, int maxNoPackets) override {
    return -1;
  }

  // VoEVideoSync
  int GetPlayoutBufferSize(int& buffer_ms) override { return -1; }
  int SetMinimumPlayoutDelay(int channel, int delay_ms) override { return -1; }
  int SetInitialPlayoutDelay(int channel, int delay_ms) override { return -1; }
  int GetDelayEstimate(int channel,
                       int* jitter_buffer_delay_ms,
                       int* playout_buffer_delay_ms) override {
    EXPECT_EQ(channel, kRecvChannelId);
    *jitter_buffer_delay_ms = kRecvJitterBufferDelay;
    *playout_buffer_delay_ms = kRecvPlayoutBufferDelay;
    return 0;
  }
  int GetLeastRequiredDelayMs(int channel) const override { return -1; }
  int SetInitTimestamp(int channel, unsigned int timestamp) override {
    return -1;
  }
  int SetInitSequenceNumber(int channel, short sequenceNumber) override {
    return -1;
  }
  int GetPlayoutTimestamp(int channel, unsigned int& timestamp) override {
    return -1;
  }
  int GetRtpRtcp(int channel,
                 RtpRtcp** rtpRtcpModule,
                 RtpReceiver** rtp_receiver) override { return -1; }

  // VoEVolumeControl
  int SetSpeakerVolume(unsigned int volume) override { return -1; }
  int GetSpeakerVolume(unsigned int& volume) override { return -1; }
  int SetMicVolume(unsigned int volume) override { return -1; }
  int GetMicVolume(unsigned int& volume) override { return -1; }
  int SetInputMute(int channel, bool enable) override { return -1; }
  int GetInputMute(int channel, bool& enabled) override { return -1; }
  int GetSpeechInputLevel(unsigned int& level) override { return -1; }
  int GetSpeechOutputLevel(int channel, unsigned int& level) override {
    return -1;
  }
  int GetSpeechInputLevelFullRange(unsigned int& level) override {
    level = kSendSpeechInputLevel;
    return 0;
  }
  int GetSpeechOutputLevelFullRange(int channel,
                                    unsigned int& level) override {
    EXPECT_EQ(channel, kRecvChannelId);
    level = kRecvSpeechOutputLevel;
    return 0;
  }
  int SetChannelOutputVolumeScaling(int channel, float scaling) override {
    return -1;
  }
  int GetChannelOutputVolumeScaling(int channel, float& scaling) override {
    return -1;
  }
  int SetOutputVolumePan(int channel, float left, float right) override {
    return -1;
  }
  int GetOutputVolumePan(int channel, float& left, float& right) override {
    return -1;
  }
};
}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_AUDIO_FAKE_VOICE_ENGINE_H_
