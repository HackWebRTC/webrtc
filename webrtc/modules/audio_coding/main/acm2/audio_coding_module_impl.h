/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_AUDIO_CODING_MODULE_IMPL_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_AUDIO_CODING_MODULE_IMPL_H_

#include <vector>

#include "webrtc/base/buffer.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/common_types.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_receiver.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_resampler.h"
#include "webrtc/modules/audio_coding/main/acm2/codec_manager.h"

namespace webrtc {

class CriticalSectionWrapper;
class AudioCodingImpl;

namespace acm2 {

class ACMDTMFDetection;

class AudioCodingModuleImpl final : public AudioCodingModule {
 public:
  friend webrtc::AudioCodingImpl;

  explicit AudioCodingModuleImpl(const AudioCodingModule::Config& config);
  ~AudioCodingModuleImpl() override;

  /////////////////////////////////////////
  //   Sender
  //

  // Can be called multiple times for Codec, CNG, RED.
  int RegisterSendCodec(const CodecInst& send_codec) override;

  void RegisterExternalSendCodec(
      AudioEncoderMutable* external_speech_encoder) override;

  // Get current send codec.
  int SendCodec(CodecInst* current_codec) const override;

  // Get current send frequency.
  int SendFrequency() const override;

  // Sets the bitrate to the specified value in bits/sec. In case the codec does
  // not support the requested value it will choose an appropriate value
  // instead.
  void SetBitRate(int bitrate_bps) override;

  // Register a transport callback which will be
  // called to deliver the encoded buffers.
  int RegisterTransportCallback(AudioPacketizationCallback* transport) override;

  // Add 10 ms of raw (PCM) audio data to the encoder.
  int Add10MsData(const AudioFrame& audio_frame) override;

  /////////////////////////////////////////
  // (RED) Redundant Coding
  //

  // Configure RED status i.e. on/off.
  int SetREDStatus(bool enable_red) override;

  // Get RED status.
  bool REDStatus() const override;

  /////////////////////////////////////////
  // (FEC) Forward Error Correction (codec internal)
  //

  // Configure FEC status i.e. on/off.
  int SetCodecFEC(bool enabled_codec_fec) override;

  // Get FEC status.
  bool CodecFEC() const override;

  // Set target packet loss rate
  int SetPacketLossRate(int loss_rate) override;

  /////////////////////////////////////////
  //   (VAD) Voice Activity Detection
  //   and
  //   (CNG) Comfort Noise Generation
  //

  int SetVAD(bool enable_dtx = true,
             bool enable_vad = false,
             ACMVADMode mode = VADNormal) override;

  int VAD(bool* dtx_enabled,
          bool* vad_enabled,
          ACMVADMode* mode) const override;

  int RegisterVADCallback(ACMVADCallback* vad_callback) override;

  /////////////////////////////////////////
  //   Receiver
  //

  // Initialize receiver, resets codec database etc.
  int InitializeReceiver() override;

  // Get current receive frequency.
  int ReceiveFrequency() const override;

  // Get current playout frequency.
  int PlayoutFrequency() const override;

  // Register possible receive codecs, can be called multiple times,
  // for codecs, CNG, DTMF, RED.
  int RegisterReceiveCodec(const CodecInst& receive_codec) override;

  int RegisterExternalReceiveCodec(int rtp_payload_type,
                                   AudioDecoder* external_decoder,
                                   int sample_rate_hz,
                                   int num_channels) override;

  // Get current received codec.
  int ReceiveCodec(CodecInst* current_codec) const override;

  // Incoming packet from network parsed and ready for decode.
  int IncomingPacket(const uint8_t* incoming_payload,
                     const size_t payload_length,
                     const WebRtcRTPHeader& rtp_info) override;

  // Incoming payloads, without rtp-info, the rtp-info will be created in ACM.
  // One usage for this API is when pre-encoded files are pushed in ACM.
  int IncomingPayload(const uint8_t* incoming_payload,
                      const size_t payload_length,
                      uint8_t payload_type,
                      uint32_t timestamp) override;

  // Minimum playout delay.
  int SetMinimumPlayoutDelay(int time_ms) override;

  // Maximum playout delay.
  int SetMaximumPlayoutDelay(int time_ms) override;

  // Smallest latency NetEq will maintain.
  int LeastRequiredDelayMs() const override;

  // Impose an initial delay on playout. ACM plays silence until |delay_ms|
  // audio is accumulated in NetEq buffer, then starts decoding payloads.
  int SetInitialPlayoutDelay(int delay_ms) override;

  // TODO(turajs): DTMF playout is always activated in NetEq these APIs should
  // be removed, as well as all VoE related APIs and methods.
  //
  // Configure Dtmf playout status i.e on/off playout the incoming outband Dtmf
  // tone.
  int SetDtmfPlayoutStatus(bool enable) override;

  // Get Dtmf playout status.
  bool DtmfPlayoutStatus() const override;

  // Set playout mode voice, fax.
  int SetPlayoutMode(AudioPlayoutMode mode) override;

  // Get playout mode voice, fax.
  AudioPlayoutMode PlayoutMode() const override;

  // Get playout timestamp.
  int PlayoutTimestamp(uint32_t* timestamp) override;

  // Get 10 milliseconds of raw audio data to play out, and
  // automatic resample to the requested frequency if > 0.
  int PlayoutData10Ms(int desired_freq_hz, AudioFrame* audio_frame) override;

  /////////////////////////////////////////
  //   Statistics
  //

  int GetNetworkStatistics(NetworkStatistics* statistics) override;

  int SetISACMaxRate(int max_bit_per_sec) override;

  int SetISACMaxPayloadSize(int max_size_bytes) override;

  int SetOpusApplication(OpusApplicationMode application) override;

  // If current send codec is Opus, informs it about the maximum playback rate
  // the receiver will render.
  int SetOpusMaxPlaybackRate(int frequency_hz) override;

  int EnableOpusDtx() override;

  int DisableOpusDtx() override;

  int UnregisterReceiveCodec(uint8_t payload_type) override;

  int EnableNack(size_t max_nack_list_size) override;

  void DisableNack() override;

  std::vector<uint16_t> GetNackList(int64_t round_trip_time_ms) const override;

  void GetDecodingCallStatistics(AudioDecodingCallStats* stats) const override;

 private:
  struct InputData {
    uint32_t input_timestamp;
    const int16_t* audio;
    size_t length_per_channel;
    uint8_t audio_channel;
    // If a re-mix is required (up or down), this buffer will store a re-mixed
    // version of the input.
    int16_t buffer[WEBRTC_10MS_PCM_AUDIO];
  };

  // This member class writes values to the named UMA histogram, but only if
  // the value has changed since the last time (and always for the first call).
  class ChangeLogger {
   public:
    explicit ChangeLogger(const std::string& histogram_name)
        : histogram_name_(histogram_name) {}
    // Logs the new value if it is different from the last logged value, or if
    // this is the first call.
    void MaybeLog(int value);

   private:
    int last_value_ = 0;
    int first_time_ = true;
    const std::string histogram_name_;
  };

  int Add10MsDataInternal(const AudioFrame& audio_frame, InputData* input_data)
      EXCLUSIVE_LOCKS_REQUIRED(acm_crit_sect_);
  int Encode(const InputData& input_data)
      EXCLUSIVE_LOCKS_REQUIRED(acm_crit_sect_);

  int InitializeReceiverSafe() EXCLUSIVE_LOCKS_REQUIRED(acm_crit_sect_);

  bool HaveValidEncoder(const char* caller_name) const
      EXCLUSIVE_LOCKS_REQUIRED(acm_crit_sect_);

  // Preprocessing of input audio, including resampling and down-mixing if
  // required, before pushing audio into encoder's buffer.
  //
  // in_frame: input audio-frame
  // ptr_out: pointer to output audio_frame. If no preprocessing is required
  //          |ptr_out| will be pointing to |in_frame|, otherwise pointing to
  //          |preprocess_frame_|.
  //
  // Return value:
  //   -1: if encountering an error.
  //    0: otherwise.
  int PreprocessToAddData(const AudioFrame& in_frame,
                          const AudioFrame** ptr_out)
      EXCLUSIVE_LOCKS_REQUIRED(acm_crit_sect_);

  // Change required states after starting to receive the codec corresponding
  // to |index|.
  int UpdateUponReceivingCodec(int index);

  const rtc::scoped_ptr<CriticalSectionWrapper> acm_crit_sect_;
  rtc::Buffer encode_buffer_ GUARDED_BY(acm_crit_sect_);
  int id_;  // TODO(henrik.lundin) Make const.
  uint32_t expected_codec_ts_ GUARDED_BY(acm_crit_sect_);
  uint32_t expected_in_ts_ GUARDED_BY(acm_crit_sect_);
  ACMResampler resampler_ GUARDED_BY(acm_crit_sect_);
  AcmReceiver receiver_;  // AcmReceiver has it's own internal lock.
  ChangeLogger bitrate_logger_ GUARDED_BY(acm_crit_sect_);
  CodecManager codec_manager_ GUARDED_BY(acm_crit_sect_);

  // This is to keep track of CN instances where we can send DTMFs.
  uint8_t previous_pltype_ GUARDED_BY(acm_crit_sect_);

  // Used when payloads are pushed into ACM without any RTP info
  // One example is when pre-encoded bit-stream is pushed from
  // a file.
  // IMPORTANT: this variable is only used in IncomingPayload(), therefore,
  // no lock acquired when interacting with this variable. If it is going to
  // be used in other methods, locks need to be taken.
  rtc::scoped_ptr<WebRtcRTPHeader> aux_rtp_header_;

  bool receiver_initialized_ GUARDED_BY(acm_crit_sect_);

  AudioFrame preprocess_frame_ GUARDED_BY(acm_crit_sect_);
  bool first_10ms_data_ GUARDED_BY(acm_crit_sect_);

  bool first_frame_ GUARDED_BY(acm_crit_sect_);
  uint32_t last_timestamp_ GUARDED_BY(acm_crit_sect_);
  uint32_t last_rtp_timestamp_ GUARDED_BY(acm_crit_sect_);

  const rtc::scoped_ptr<CriticalSectionWrapper> callback_crit_sect_;
  AudioPacketizationCallback* packetization_callback_
      GUARDED_BY(callback_crit_sect_);
  ACMVADCallback* vad_callback_ GUARDED_BY(callback_crit_sect_);
};

}  // namespace acm2

class AudioCodingImpl : public AudioCoding {
 public:
  AudioCodingImpl(const Config& config);
  ~AudioCodingImpl() override;

  bool RegisterSendCodec(AudioEncoder* send_codec) override;

  bool RegisterSendCodec(int encoder_type,
                         uint8_t payload_type,
                         int frame_size_samples = 0) override;

  const AudioEncoder* GetSenderInfo() const override;

  const CodecInst* GetSenderCodecInst() override;

  int Add10MsAudio(const AudioFrame& audio_frame) override;

  const ReceiverInfo* GetReceiverInfo() const override;

  bool RegisterReceiveCodec(AudioDecoder* receive_codec) override;

  bool RegisterReceiveCodec(int decoder_type, uint8_t payload_type) override;

  bool InsertPacket(const uint8_t* incoming_payload,
                    size_t payload_len_bytes,
                    const WebRtcRTPHeader& rtp_info) override;

  bool InsertPayload(const uint8_t* incoming_payload,
                     size_t payload_len_byte,
                     uint8_t payload_type,
                     uint32_t timestamp) override;

  bool SetMinimumPlayoutDelay(int time_ms) override;

  bool SetMaximumPlayoutDelay(int time_ms) override;

  int LeastRequiredDelayMs() const override;

  bool PlayoutTimestamp(uint32_t* timestamp) override;

  bool Get10MsAudio(AudioFrame* audio_frame) override;

  bool GetNetworkStatistics(NetworkStatistics* network_statistics) override;

  bool EnableNack(size_t max_nack_list_size) override;

  void DisableNack() override;

  bool SetVad(bool enable_dtx, bool enable_vad, ACMVADMode vad_mode) override;

  std::vector<uint16_t> GetNackList(int round_trip_time_ms) const override;

  void GetDecodingCallStatistics(
      AudioDecodingCallStats* call_stats) const override;

 private:
  // Temporary method to be used during redesign phase.
  // Maps |codec_type| (a value from the anonymous enum in acm2::ACMCodecDB) to
  // |codec_name|, |sample_rate_hz|, and |channels|.
  // TODO(henrik.lundin) Remove this when no longer needed.
  static bool MapCodecTypeToParameters(int codec_type,
                                       std::string* codec_name,
                                       int* sample_rate_hz,
                                       int* channels);

  int playout_frequency_hz_;
  // TODO(henrik.lundin): All members below this line are temporary and should
  // be removed after refactoring is completed.
  rtc::scoped_ptr<acm2::AudioCodingModuleImpl> acm_old_;
  CodecInst current_send_codec_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_AUDIO_CODING_MODULE_IMPL_H_
