/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_GENERIC_CODEC_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_GENERIC_CODEC_H_

#include <map>

#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/codecs/audio_decoder.h"
#include "webrtc/modules/audio_coding/codecs/audio_encoder.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_common_defs.h"
#include "webrtc/modules/audio_coding/neteq/interface/neteq.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/system_wrappers/interface/trace.h"

#define MAX_FRAME_SIZE_10MSEC 6

// forward declaration
struct WebRtcVadInst;
struct WebRtcCngEncInst;

namespace webrtc {

struct WebRtcACMCodecParams;
struct CodecInst;

namespace acm2 {

// forward declaration
class AcmReceiver;

// Proxy for AudioDecoder
class AudioDecoderProxy final : public AudioDecoder {
 public:
  AudioDecoderProxy();
  void SetDecoder(AudioDecoder* decoder);
  bool IsSet() const;
  int Decode(const uint8_t* encoded,
             size_t encoded_len,
             int16_t* decoded,
             SpeechType* speech_type) override;
  int DecodeRedundant(const uint8_t* encoded,
                      size_t encoded_len,
                      int16_t* decoded,
                      SpeechType* speech_type) override;
  bool HasDecodePlc() const override;
  int DecodePlc(int num_frames, int16_t* decoded) override;
  int Init() override;
  int IncomingPacket(const uint8_t* payload,
                     size_t payload_len,
                     uint16_t rtp_sequence_number,
                     uint32_t rtp_timestamp,
                     uint32_t arrival_timestamp) override;
  int ErrorCode() override;
  int PacketDuration(const uint8_t* encoded, size_t encoded_len) const override;
  int PacketDurationRedundant(const uint8_t* encoded,
                              size_t encoded_len) const override;
  bool PacketHasFec(const uint8_t* encoded, size_t encoded_len) const override;
  CNG_dec_inst* CngDecoderInstance() override;

 private:
  scoped_ptr<CriticalSectionWrapper> decoder_lock_;
  AudioDecoder* decoder_ GUARDED_BY(decoder_lock_);
};

class ACMGenericCodec {
 public:
  ACMGenericCodec(const CodecInst& codec_inst,
                  int cng_pt_nb,
                  int cng_pt_wb,
                  int cng_pt_swb,
                  int cng_pt_fb,
                  bool enable_red,
                  int red_payload_type);
  ~ACMGenericCodec();

  ///////////////////////////////////////////////////////////////////////////
  // ACMGenericCodec* CreateInstance();
  // The function will be used for FEC. It is not implemented yet.
  //
  ACMGenericCodec* CreateInstance();

  ///////////////////////////////////////////////////////////////////////////
  // int16_t Encode()
  // The function is called to perform an encoding of the audio stored in
  // audio buffer. An encoding is performed only if enough audio, i.e. equal
  // to the frame-size of the codec, exist. The audio frame will be processed
  // by VAD and CN/DTX if required. There are few different cases.
  //
  // A) Neither VAD nor DTX is active; the frame is encoded by the encoder.
  //
  // B) VAD is enabled but not DTX; in this case the audio is processed by VAD
  //    and encoded by the encoder. The "*encoding_type" will be either
  //    "kActiveNormalEncode" or "kPassiveNormalEncode" if frame is active or
  //    passive, respectively.
  //
  // C) DTX is enabled; if the codec has internal VAD/DTX we just encode the
  //    frame by the encoder. Otherwise, the frame is passed through VAD and
  //    if identified as passive, then it will be processed by CN/DTX. If the
  //    frame is active it will be encoded by the encoder.
  //
  // This function acquires the appropriate locks and calls EncodeSafe() for
  // the actual processing.
  //
  // Outputs:
  //   -bitstream          : a buffer where bit-stream will be written to.
  //   -bitstream_len_byte : contains the length of the bit-stream in
  //                         bytes.
  //   -timestamp          : contains the RTP timestamp, this is the
  //                         sampling time of the first sample encoded
  //                         (measured in number of samples).
  //   -encoding_type       : contains the type of encoding applied on the
  //                         audio samples. The alternatives are
  //                         (c.f. acm_common_types.h)
  //                         -kNoEncoding:
  //                            there was not enough data to encode. or
  //                            some error has happened that we could
  //                            not do encoding.
  //                         -kActiveNormalEncoded:
  //                            the audio frame is active and encoded by
  //                            the given codec.
  //                         -kPassiveNormalEncoded:
  //                            the audio frame is passive but coded with
  //                            the given codec (NO DTX).
  //                         -kPassiveDTXWB:
  //                            The audio frame is passive and used
  //                            wide-band CN to encode.
  //                         -kPassiveDTXNB:
  //                            The audio frame is passive and used
  //                            narrow-band CN to encode.
  //
  // Return value:
  //   -1 if error is occurred, otherwise the length of the bit-stream in
  //      bytes.
  //
  int16_t Encode(uint8_t* bitstream,
                 int16_t* bitstream_len_byte,
                 uint32_t* timestamp,
                 WebRtcACMEncodingType* encoding_type,
                 AudioEncoder::EncodedInfo* encoded_info);

  ///////////////////////////////////////////////////////////////////////////
  // bool EncoderInitialized();
  //
  // Return value:
  //   True if the encoder is successfully initialized,
  //   false otherwise.
  //
  bool EncoderInitialized();

  ///////////////////////////////////////////////////////////////////////////
  // int16_t EncoderParams()
  // It is called to get encoder parameters. It will call
  // EncoderParamsSafe() in turn.
  //
  // Output:
  //   -enc_params         : a buffer where the encoder parameters is
  //                         written to. If the encoder is not
  //                         initialized this buffer is filled with
  //                         invalid values
  // Return value:
  //   -1 if the encoder is not initialized,
  //    0 otherwise.
  //
  int16_t EncoderParams(WebRtcACMCodecParams* enc_params);

  ///////////////////////////////////////////////////////////////////////////
  // int16_t InitEncoder(...)
  // This function is called to initialize the encoder with the given
  // parameters.
  //
  // Input:
  //   -codec_params        : parameters of encoder.
  //   -force_initialization: if false the initialization is invoked only if
  //                          the encoder is not initialized. If true the
  //                          encoder is forced to (re)initialize.
  //
  // Return value:
  //   0 if could initialize successfully,
  //  -1 if failed to initialize.
  //
  //
  int16_t InitEncoder(WebRtcACMCodecParams* codec_params,
                      bool force_initialization);

  ///////////////////////////////////////////////////////////////////////////
  // int32_t Add10MsData(...)
  // This function is called to add 10 ms of audio to the audio buffer of
  // the codec.
  //
  // Inputs:
  //   -timestamp          : the timestamp of the 10 ms audio. the timestamp
  //                         is the sampling time of the
  //                         first sample measured in number of samples.
  //   -data               : a buffer that contains the audio. The codec
  //                         expects to get the audio in correct sampling
  //                         frequency
  //   -length             : the length of the audio buffer
  //   -audio_channel      : 0 for mono, 1 for stereo (not supported yet)
  //
  // Return values:
  //   -1 if failed
  //    0 otherwise.
  //
  int32_t Add10MsData(const uint32_t timestamp,
                      const int16_t* data,
                      const uint16_t length,
                      const uint8_t audio_channel);

  ///////////////////////////////////////////////////////////////////////////
  // uint32_t NoMissedSamples()
  // This function returns the number of samples which are overwritten in
  // the audio buffer. The audio samples are overwritten if the input audio
  // buffer is full, but Add10MsData() is called. (We might remove this
  // function if it is not used)
  //
  // Return Value:
  //   Number of samples which are overwritten.
  //
  uint32_t NoMissedSamples() const;

  ///////////////////////////////////////////////////////////////////////////
  // void ResetNoMissedSamples()
  // This function resets the number of overwritten samples to zero.
  // (We might remove this function if we remove NoMissedSamples())
  //
  void ResetNoMissedSamples();

  ///////////////////////////////////////////////////////////////////////////
  // int16_t SetBitRate()
  // The function is called to set the encoding rate.
  //
  // Input:
  //   -bitrate_bps        : encoding rate in bits per second
  //
  // Return value:
  //   -1 if failed to set the rate, due to invalid input or given
  //      codec is not rate-adjustable.
  //    0 if the rate is adjusted successfully
  //
  int16_t SetBitRate(const int32_t bitrate_bps);

  ///////////////////////////////////////////////////////////////////////////
  // uint32_t EarliestTimestamp()
  // Returns the timestamp of the first 10 ms in audio buffer. This is used
  // to identify if a synchronization of two encoders is required.
  //
  // Return value:
  //   timestamp of the first 10 ms audio in the audio buffer.
  //
  uint32_t EarliestTimestamp() const;

  ///////////////////////////////////////////////////////////////////////////
  // int16_t SetVAD()
  // This is called to set VAD & DTX. If the codec has internal DTX, it will
  // be used. If DTX is enabled and the codec does not have internal DTX,
  // WebRtc-VAD will be used to decide if the frame is active. If DTX is
  // disabled but VAD is enabled, the audio is passed through VAD to label it
  // as active or passive, but the frame is  encoded normally. However the
  // bit-stream is labeled properly so that ACM::Process() can use this
  // information. In case of failure, the previous states of the VAD & DTX
  // are kept.
  //
  // Inputs/Output:
  //   -enable_dtx         : if true DTX will be enabled otherwise the DTX is
  //                         disabled. If codec has internal DTX that will be
  //                         used, otherwise WebRtc-CNG is used. In the latter
  //                         case VAD is automatically activated.
  //   -enable_vad         : if true WebRtc-VAD is enabled, otherwise VAD is
  //                         disabled, except for the case that DTX is enabled
  //                         but codec doesn't have internal DTX. In this case
  //                         VAD is enabled regardless of the value of
  //                         |enable_vad|.
  //   -mode               : this specifies the aggressiveness of VAD.
  //
  // Return value
  //   -1 if failed to set DTX & VAD as specified,
  //    0 if succeeded.
  //
  int16_t SetVAD(bool* enable_dtx, bool* enable_vad, ACMVADMode* mode);

  // Registers comfort noise at |sample_rate_hz| to use |payload_type|.
  void SetCngPt(int sample_rate_hz, int payload_type);

  ///////////////////////////////////////////////////////////////////////////
  // bool HasInternalDTX()
  // Used to check if the codec has internal DTX.
  //
  // Return value:
  //   true if the codec has an internal DTX, e.g. G729,
  //   false otherwise.
  //
  bool HasInternalDTX() const {
    ReadLockScoped rl(codec_wrapper_lock_);
    return has_internal_dtx_;
  }

  ///////////////////////////////////////////////////////////////////////////
  // int32_t GetRedPayload()
  // Used to get codec specific RED payload (if such is implemented).
  // Currently only done in iSAC.
  //
  // Outputs:
  //   -red_payload       : a pointer to the data for RED payload.
  //   -payload_bytes     : number of bytes in RED payload.
  //
  // Return value:
  //   -1 if fails to get codec specific RED,
  //    0 if succeeded.
  //
  int32_t GetRedPayload(uint8_t* red_payload, int16_t* payload_bytes);

  ///////////////////////////////////////////////////////////////////////////
  // int16_t ResetEncoder()
  // By calling this function you would re-initialize the encoder with the
  // current parameters. All the settings, e.g. VAD/DTX, frame-size... should
  // remain unchanged. (In case of iSAC we don't want to lose BWE history.)
  //
  // Return value
  //   -1 if failed,
  //    0 if succeeded.
  //
  int16_t ResetEncoder();

  ///////////////////////////////////////////////////////////////////////////
  // void DestructEncoder()
  // This function is called to delete the encoder instance, if possible, to
  // have a fresh start. For codecs where encoder and decoder share the same
  // instance we cannot delete the encoder and instead we will initialize the
  // encoder. We also delete VAD and DTX if they have been created.
  //
  void DestructEncoder();

  ///////////////////////////////////////////////////////////////////////////
  // SetUniqueID()
  // Set a unique ID for the codec to be used for tracing and debugging
  //
  // Input
  //   -id                 : A number to identify the codec.
  //
  void SetUniqueID(const uint32_t id);

  ///////////////////////////////////////////////////////////////////////////
  // UpdateDecoderSampFreq()
  // For most of the codecs this function does nothing. It must be
  // implemented for those codecs that one codec instance serves as the
  // decoder for different flavors of the codec. One example is iSAC. there,
  // iSAC 16 kHz and iSAC 32 kHz are treated as two different codecs with
  // different payload types, however, there is only one iSAC instance to
  // decode. The reason for that is we would like to decode and encode with
  // the same codec instance for bandwidth estimator to work.
  //
  // Each time that we receive a new payload type, we call this function to
  // prepare the decoder associated with the new payload. Normally, decoders
  // doesn't have to do anything. For iSAC the decoder has to change it's
  // sampling rate. The input parameter specifies the current flavor of the
  // codec in codec database. For instance, if we just got a SWB payload then
  // the input parameter is ACMCodecDB::isacswb.
  //
  // Input:
  //   -codec_id           : the ID of the codec associated with the
  //                         payload type that we just received.
  //
  // Return value:
  //    0 if succeeded in updating the decoder.
  //   -1 if failed to update.
  //
  int16_t UpdateDecoderSampFreq(int16_t /* codec_id */);

  ///////////////////////////////////////////////////////////////////////////
  // UpdateEncoderSampFreq()
  // Call this function to update the encoder sampling frequency. This
  // is for codecs where one payload-name supports several encoder sampling
  // frequencies. Otherwise, to change the sampling frequency we need to
  // register new codec. ACM will consider that as registration of a new
  // codec, not a change in parameter. For iSAC, switching from WB to SWB
  // is treated as a change in parameter. Therefore, we need this function.
  //
  // Input:
  //   -samp_freq_hz        : encoder sampling frequency.
  //
  // Return value:
  //   -1 if failed, or if this is meaningless for the given codec.
  //    0 if succeeded.
  //
  int16_t UpdateEncoderSampFreq(uint16_t samp_freq_hz)
      EXCLUSIVE_LOCKS_REQUIRED(codec_wrapper_lock_);

  ///////////////////////////////////////////////////////////////////////////
  // EncoderSampFreq()
  // Get the sampling frequency that the encoder (WebRtc wrapper) expects.
  //
  // Output:
  //   -samp_freq_hz       : sampling frequency, in Hertz, which the encoder
  //                         should be fed with.
  //
  // Return value:
  //   -1 if failed to output sampling rate.
  //    0 if the sample rate is returned successfully.
  //
  int16_t EncoderSampFreq(uint16_t* samp_freq_hz)
      SHARED_LOCKS_REQUIRED(codec_wrapper_lock_);

  ///////////////////////////////////////////////////////////////////////////
  // SetISACMaxPayloadSize()
  // Set the maximum payload size of iSAC packets. No iSAC payload,
  // regardless of its frame-size, may exceed the given limit. For
  // an iSAC payload of size B bits and frame-size T sec we have;
  // (B < max_payload_len_bytes * 8) and (B/T < max_rate_bit_per_sec), c.f.
  // SetISACMaxRate().
  //
  // Input:
  //   -max_payload_len_bytes : maximum payload size in bytes.
  //
  // Return value:
  //   -1 if failed to set the maximum  payload-size.
  //    0 if the given length is set successfully.
  //
  int32_t SetISACMaxPayloadSize(const uint16_t max_payload_len_bytes);

  ///////////////////////////////////////////////////////////////////////////
  // SetISACMaxRate()
  // Set the maximum instantaneous rate of iSAC. For a payload of B bits
  // with a frame-size of T sec the instantaneous rate is B/T bits per
  // second. Therefore, (B/T < max_rate_bit_per_sec) and
  // (B < max_payload_len_bytes * 8) are always satisfied for iSAC payloads,
  // c.f SetISACMaxPayloadSize().
  //
  // Input:
  //   -max_rate_bps       : maximum instantaneous bit-rate given in bits/sec.
  //
  // Return value:
  //   -1 if failed to set the maximum rate.
  //    0 if the maximum rate is set successfully.
  //
  int32_t SetISACMaxRate(const uint32_t max_rate_bps);

  ///////////////////////////////////////////////////////////////////////////
  // int SetOpusApplication()
  // Sets the intended application for the Opus encoder. Opus uses this to
  // optimize the encoding for applications like VOIP and music.
  //
  // Input:
  //   - application      : intended application.
  //
  // Return value:
  //   -1 if failed or on codecs other than Opus.
  //    0 if succeeded.
  //
  int SetOpusApplication(OpusApplicationMode /*application*/);

  ///////////////////////////////////////////////////////////////////////////
  // int SetOpusMaxPlaybackRate()
  // Sets maximum playback rate the receiver will render, if the codec is Opus.
  // This is to tell Opus that it is enough to code the input audio up to a
  // bandwidth. Opus can take this information to optimize the bit rate and
  // increase the computation efficiency.
  //
  // Input:
  //   -frequency_hz      : maximum playback rate in Hz.
  //
  // Return value:
  //   -1 if failed or on codecs other than Opus
  //    0 if succeeded.
  //
  int SetOpusMaxPlaybackRate(int /* frequency_hz */);

  ///////////////////////////////////////////////////////////////////////////
  // HasFrameToEncode()
  // Returns true if there is enough audio buffered for encoding, such that
  // calling Encode() will return a payload.
  //
  bool HasFrameToEncode() const;

  //
  // Returns pointer to the AudioDecoder class of this codec. A codec which
  // should own its own decoder (e.g. iSAC which need same instance for encoding
  // and decoding, or a codec which should access decoder instance for specific
  // decoder setting) should implement this method. This method is called if
  // and only if the ACMCodecDB::codec_settings[codec_id].owns_decoder is true.
  //
  AudioDecoder* Decoder(int /* codec_id */);

  ///////////////////////////////////////////////////////////////////////////
  // bool HasInternalFEC()
  // Used to check if the codec has internal FEC.
  //
  // Return value:
  //   true if the codec has an internal FEC, e.g. Opus.
  //   false otherwise.
  //
  bool HasInternalFEC() const {
    ReadLockScoped rl(codec_wrapper_lock_);
    return has_internal_fec_;
  }

  ///////////////////////////////////////////////////////////////////////////
  // int SetFEC();
  // Sets the codec internal FEC. No effects on codecs that do not provide
  // internal FEC.
  //
  // Input:
  //   -enable_fec         : if true FEC will be enabled otherwise the FEC is
  //                         disabled.
  //
  // Return value:
  //   -1 if failed,
  //    0 if succeeded.
  //
  int SetFEC(bool enable_fec);

  ///////////////////////////////////////////////////////////////////////////
  // int SetPacketLossRate()
  // Sets expected packet loss rate for encoding. Some encoders provide packet
  // loss gnostic encoding to make stream less sensitive to packet losses,
  // through e.g., FEC. No effects on codecs that do not provide such encoding.
  //
  // Input:
  //   -loss_rate          : expected packet loss rate (0 -- 100 inclusive).
  //
  // Return value:
  //   -1 if failed,
  //    0 if succeeded or packet loss rate is ignored.
  //
  int SetPacketLossRate(int /* loss_rate */);

  // Sets if CopyRed should be enabled.
  void EnableCopyRed(bool enable, int red_payload_type);

  // Returns true if the caller needs to produce RED data manually (that is, if
  // RED has been enabled but the codec isn't able to produce the data itself).
  bool ExternalRedNeeded();

  // This method is only for testing.
  const AudioEncoder* GetAudioEncoder() const;

 private:
  // &in_audio_[in_audio_ix_write_] always point to where new audio can be
  // written to
  int16_t in_audio_ix_write_ GUARDED_BY(codec_wrapper_lock_);

  // &in_audio_[in_audio_ix_read_] points to where audio has to be read from
  int16_t in_audio_ix_read_ GUARDED_BY(codec_wrapper_lock_);

  int16_t in_timestamp_ix_write_ GUARDED_BY(codec_wrapper_lock_);

  // Where the audio is stored before encoding,
  // To save memory the following buffer can be allocated
  // dynamically for 80 ms depending on the sampling frequency
  // of the codec.
  int16_t* in_audio_ GUARDED_BY(codec_wrapper_lock_);
  uint32_t* in_timestamp_ GUARDED_BY(codec_wrapper_lock_);

  int16_t frame_len_smpl_ GUARDED_BY(codec_wrapper_lock_);
  uint16_t num_channels_ GUARDED_BY(codec_wrapper_lock_);

  // This will point to a static database of the supported codecs
  int16_t codec_id_ GUARDED_BY(codec_wrapper_lock_);

  // This will account for the number of samples  were not encoded
  // the case is rare, either samples are missed due to overwrite
  // at input buffer or due to encoding error
  uint32_t num_missed_samples_ GUARDED_BY(codec_wrapper_lock_);

  // True if the encoder instance created
  bool encoder_exist_ GUARDED_BY(codec_wrapper_lock_);

  // True if the encoder instance initialized
  bool encoder_initialized_ GUARDED_BY(codec_wrapper_lock_);

  const bool registered_in_neteq_
      GUARDED_BY(codec_wrapper_lock_);  // TODO(henrik.lundin) Remove?

  // VAD/DTX
  bool has_internal_dtx_ GUARDED_BY(codec_wrapper_lock_);
  WebRtcVadInst* ptr_vad_inst_ GUARDED_BY(codec_wrapper_lock_);
  bool vad_enabled_ GUARDED_BY(codec_wrapper_lock_);
  ACMVADMode vad_mode_ GUARDED_BY(codec_wrapper_lock_);
  int16_t vad_label_[MAX_FRAME_SIZE_10MSEC] GUARDED_BY(codec_wrapper_lock_);
  bool dtx_enabled_ GUARDED_BY(codec_wrapper_lock_);
  WebRtcCngEncInst* ptr_dtx_inst_ GUARDED_BY(codec_wrapper_lock_);
  uint8_t num_lpc_params_               // TODO(henrik.lundin) Delete and
      GUARDED_BY(codec_wrapper_lock_);  // replace with kNewCNGNumLPCParams.
  bool sent_cn_previous_ GUARDED_BY(codec_wrapper_lock_);
  int16_t prev_frame_cng_ GUARDED_BY(codec_wrapper_lock_);

  // FEC.
  bool has_internal_fec_ GUARDED_BY(codec_wrapper_lock_);

  bool copy_red_enabled_ GUARDED_BY(codec_wrapper_lock_);

  WebRtcACMCodecParams encoder_params_ GUARDED_BY(codec_wrapper_lock_);

  // Used to lock wrapper internal data
  // such as buffers and state variables.
  RWLockWrapper& codec_wrapper_lock_;

  uint32_t last_timestamp_ GUARDED_BY(codec_wrapper_lock_);
  uint32_t unique_id_;

  void ResetAudioEncoder() EXCLUSIVE_LOCKS_REQUIRED(codec_wrapper_lock_);

  OpusApplicationMode GetOpusApplication(int num_channels) const
      EXCLUSIVE_LOCKS_REQUIRED(codec_wrapper_lock_);

  scoped_ptr<AudioEncoder> audio_encoder_ GUARDED_BY(codec_wrapper_lock_);
  scoped_ptr<AudioEncoder> cng_encoder_ GUARDED_BY(codec_wrapper_lock_);
  scoped_ptr<AudioEncoder> red_encoder_ GUARDED_BY(codec_wrapper_lock_);
  AudioEncoder* encoder_ GUARDED_BY(codec_wrapper_lock_);
  AudioDecoderProxy decoder_proxy_ GUARDED_BY(codec_wrapper_lock_);
  std::vector<int16_t> input_ GUARDED_BY(codec_wrapper_lock_);
  WebRtcACMCodecParams acm_codec_params_ GUARDED_BY(codec_wrapper_lock_);
  int bitrate_bps_ GUARDED_BY(codec_wrapper_lock_);
  bool fec_enabled_ GUARDED_BY(codec_wrapper_lock_);
  int loss_rate_ GUARDED_BY(codec_wrapper_lock_);
  int max_playback_rate_hz_ GUARDED_BY(codec_wrapper_lock_);
  int max_payload_size_bytes_ GUARDED_BY(codec_wrapper_lock_);
  int max_rate_bps_ GUARDED_BY(codec_wrapper_lock_);
  bool is_opus_ GUARDED_BY(codec_wrapper_lock_);
  bool is_isac_ GUARDED_BY(codec_wrapper_lock_);
  bool first_frame_ GUARDED_BY(codec_wrapper_lock_);
  uint32_t rtp_timestamp_ GUARDED_BY(codec_wrapper_lock_);
  uint32_t last_rtp_timestamp_ GUARDED_BY(codec_wrapper_lock_);
  // Map from payload type to sample rate (Hz) and encoding type.
  std::map<int, std::pair<int, WebRtcACMEncodingType>> cng_pt_
      GUARDED_BY(codec_wrapper_lock_);
  int red_payload_type_ GUARDED_BY(codec_wrapper_lock_);
  OpusApplicationMode opus_application_ GUARDED_BY(codec_wrapper_lock_);
  bool opus_application_set_ GUARDED_BY(codec_wrapper_lock_);
};

}  // namespace acm2

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_GENERIC_CODEC_H_
