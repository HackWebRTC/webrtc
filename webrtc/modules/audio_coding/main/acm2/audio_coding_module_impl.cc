/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/acm2/audio_coding_module_impl.h"

#include <assert.h>
#include <stdlib.h>
#include <vector>

#include "webrtc/base/checks.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/engine_configurations.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_generic_codec.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_resampler.h"
#include "webrtc/modules/audio_coding/main/acm2/call_statistics.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/rw_lock_wrapper.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/typedefs.h"

namespace webrtc {

namespace acm2 {

enum {
  kACMToneEnd = 999
};

// Maximum number of bytes in one packet (PCM16B, 20 ms packets, stereo).
enum {
  kMaxPacketSize = 2560
};

// Maximum number of payloads that can be packed in one RED packet. For
// regular RED, we only pack two payloads. In case of dual-streaming, in worst
// case we might pack 3 payloads in one RED packet.
enum {
  kNumRedFragmentationVectors = 2,
  kMaxNumFragmentationVectors = 3
};

// If packet N is arrived all packets prior to N - |kNackThresholdPackets| which
// are not received are considered as lost, and appear in NACK list.
enum {
  kNackThresholdPackets = 2
};

namespace {

// TODO(turajs): the same functionality is used in NetEq. If both classes
// need them, make it a static function in ACMCodecDB.
bool IsCodecRED(const CodecInst* codec) {
  return (STR_CASE_CMP(codec->plname, "RED") == 0);
}

bool IsCodecRED(int index) {
  return (IsCodecRED(&ACMCodecDB::database_[index]));
}

bool IsCodecCN(const CodecInst* codec) {
  return (STR_CASE_CMP(codec->plname, "CN") == 0);
}

bool IsCodecCN(int index) {
  return (IsCodecCN(&ACMCodecDB::database_[index]));
}

// Stereo-to-mono can be used as in-place.
int DownMix(const AudioFrame& frame, int length_out_buff, int16_t* out_buff) {
  if (length_out_buff < frame.samples_per_channel_) {
    return -1;
  }
  for (int n = 0; n < frame.samples_per_channel_; ++n)
    out_buff[n] = (frame.data_[2 * n] + frame.data_[2 * n + 1]) >> 1;
  return 0;
}

// Mono-to-stereo can be used as in-place.
int UpMix(const AudioFrame& frame, int length_out_buff, int16_t* out_buff) {
  if (length_out_buff < frame.samples_per_channel_) {
    return -1;
  }
  for (int n = frame.samples_per_channel_ - 1; n >= 0; --n) {
    out_buff[2 * n + 1] = frame.data_[n];
    out_buff[2 * n] = frame.data_[n];
  }
  return 0;
}

void ConvertEncodedInfoToFragmentationHeader(
    const AudioEncoder::EncodedInfo& info,
    RTPFragmentationHeader* frag) {
  if (info.redundant.empty()) {
    frag->fragmentationVectorSize = 0;
    return;
  }

  frag->VerifyAndAllocateFragmentationHeader(
      static_cast<uint16_t>(info.redundant.size()));
  frag->fragmentationVectorSize = static_cast<uint16_t>(info.redundant.size());
  size_t offset = 0;
  for (size_t i = 0; i < info.redundant.size(); ++i) {
    frag->fragmentationOffset[i] = offset;
    offset += info.redundant[i].encoded_bytes;
    frag->fragmentationLength[i] = info.redundant[i].encoded_bytes;
    frag->fragmentationTimeDiff[i] = rtc::checked_cast<uint16_t>(
        info.encoded_timestamp - info.redundant[i].encoded_timestamp);
    frag->fragmentationPlType[i] = info.redundant[i].payload_type;
  }
}
}  // namespace

AudioCodingModuleImpl::AudioCodingModuleImpl(
    const AudioCodingModule::Config& config)
    : acm_crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      id_(config.id),
      expected_codec_ts_(0xD87F3F9F),
      expected_in_ts_(0xD87F3F9F),
      send_codec_inst_(),
      cng_nb_pltype_(255),
      cng_wb_pltype_(255),
      cng_swb_pltype_(255),
      cng_fb_pltype_(255),
      red_pltype_(255),
      vad_enabled_(false),
      dtx_enabled_(false),
      vad_mode_(VADNormal),
      stereo_send_(false),
      current_send_codec_idx_(-1),
      send_codec_registered_(false),
      receiver_(config),
      red_enabled_(false),
      codec_fec_enabled_(false),
      previous_pltype_(255),
      aux_rtp_header_(NULL),
      receiver_initialized_(false),
      first_10ms_data_(false),
      callback_crit_sect_(CriticalSectionWrapper::CreateCriticalSection()),
      packetization_callback_(NULL),
      vad_callback_(NULL) {
  // Nullify send codec memory, set payload type and set codec name to
  // invalid values.
  const char no_name[] = "noCodecRegistered";
  strncpy(send_codec_inst_.plname, no_name, RTP_PAYLOAD_NAME_SIZE - 1);
  send_codec_inst_.pltype = -1;

  for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++) {
    codecs_[i] = NULL;
    mirror_codec_idx_[i] = -1;
  }

  // Register the default payload type for RED and for CNG at sampling rates of
  // 8, 16, 32 and 48 kHz.
  for (int i = (ACMCodecDB::kNumCodecs - 1); i >= 0; i--) {
    if (IsCodecRED(i)) {
      red_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
    } else if (IsCodecCN(i)) {
      if (ACMCodecDB::database_[i].plfreq == 8000) {
        cng_nb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      } else if (ACMCodecDB::database_[i].plfreq == 16000) {
        cng_wb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      } else if (ACMCodecDB::database_[i].plfreq == 32000) {
        cng_swb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      } else if (ACMCodecDB::database_[i].plfreq == 48000) {
        cng_fb_pltype_ = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      }
    }
  }

  if (InitializeReceiverSafe() < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Cannot initialize receiver");
  }
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceAudioCoding, id_, "Created");
}

AudioCodingModuleImpl::~AudioCodingModuleImpl() {
  {
    CriticalSectionScoped lock(acm_crit_sect_);
    current_send_codec_idx_ = -1;

    for (int i = 0; i < ACMCodecDB::kMaxNumCodecs; i++) {
      if (codecs_[i] != NULL) {
        // Mirror index holds the address of the codec memory.
        assert(mirror_codec_idx_[i] > -1);
        if (codecs_[mirror_codec_idx_[i]] != NULL) {
          delete codecs_[mirror_codec_idx_[i]];
          codecs_[mirror_codec_idx_[i]] = NULL;
        }

        codecs_[i] = NULL;
      }
    }
  }

  if (aux_rtp_header_ != NULL) {
    delete aux_rtp_header_;
    aux_rtp_header_ = NULL;
  }

  delete callback_crit_sect_;
  callback_crit_sect_ = NULL;

  delete acm_crit_sect_;
  acm_crit_sect_ = NULL;
  WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceAudioCoding, id_,
               "Destroyed");
}

int32_t AudioCodingModuleImpl::Encode(const InputData& input_data) {
  // Make room for 1 RED payload.
  uint8_t stream[2 * MAX_PAYLOAD_SIZE_BYTE];
  // TODO(turajs): |length_bytes| & |red_length_bytes| can be of type int if
  // ACMGenericCodec::Encode() & ACMGenericCodec::GetRedPayload() allows.
  int16_t length_bytes = 2 * MAX_PAYLOAD_SIZE_BYTE;
  FrameType frame_type = kAudioFrameSpeech;
  uint8_t current_payload_type = 0;
  bool has_data_to_send = false;
  RTPFragmentationHeader my_fragmentation;
  AudioEncoder::EncodedInfo encoded_info;

  // Keep the scope of the ACM critical section limited.
  {
    CriticalSectionScoped lock(acm_crit_sect_);
    // Check if there is an encoder before.
    if (!HaveValidEncoder("Process")) {
      return -1;
    }
    codecs_[current_send_codec_idx_]->Encode(
        input_data.input_timestamp, input_data.audio,
        input_data.length_per_channel, input_data.audio_channel, stream,
        &length_bytes, &encoded_info);
    if (encoded_info.encoded_bytes == 0 && !encoded_info.send_even_if_empty) {
      // Not enough data.
      return 0;
    } else {
      if (encoded_info.encoded_bytes == 0 && encoded_info.send_even_if_empty) {
        frame_type = kFrameEmpty;
        current_payload_type = previous_pltype_;
      } else {
        DCHECK_GT(encoded_info.encoded_bytes, 0u);
        frame_type = encoded_info.speech ? kAudioFrameSpeech : kAudioFrameCN;
        current_payload_type = encoded_info.payload_type;
        previous_pltype_ = current_payload_type;
      }
      has_data_to_send = true;

      ConvertEncodedInfoToFragmentationHeader(encoded_info, &my_fragmentation);
    }
  }

  if (has_data_to_send) {
    CriticalSectionScoped lock(callback_crit_sect_);

    if (packetization_callback_ != NULL) {
      if (my_fragmentation.fragmentationVectorSize > 0) {
        // Callback with payload data, including redundant data (RED).
        packetization_callback_->SendData(
            frame_type, current_payload_type, encoded_info.encoded_timestamp,
            stream, length_bytes, &my_fragmentation);
      } else {
        // Callback with payload data.
        packetization_callback_->SendData(frame_type, current_payload_type,
                                          encoded_info.encoded_timestamp,
                                          stream, length_bytes, NULL);
      }
    }

    if (vad_callback_ != NULL) {
      // Callback with VAD decision.
      vad_callback_->InFrameType(frame_type);
    }
  }
  return length_bytes;
}

/////////////////////////////////////////
//   Sender
//

// Initialize send codec.
int AudioCodingModuleImpl::InitializeSender() {
  CriticalSectionScoped lock(acm_crit_sect_);

  // Start with invalid values.
  send_codec_registered_ = false;
  current_send_codec_idx_ = -1;
  send_codec_inst_.plname[0] = '\0';

  return 0;
}

// TODO(henrik.lundin): Remove this method; only used in tests.
int AudioCodingModuleImpl::ResetEncoder() {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (!HaveValidEncoder("ResetEncoder")) {
    return -1;
  }
  return 0;
}

ACMGenericCodec* AudioCodingModuleImpl::CreateCodec(const CodecInst& codec) {
  ACMGenericCodec* my_codec = NULL;
  CriticalSectionScoped lock(acm_crit_sect_);
  my_codec = ACMCodecDB::CreateCodecInstance(
      codec, cng_nb_pltype_, cng_wb_pltype_, cng_swb_pltype_, cng_fb_pltype_,
      red_enabled_, red_pltype_);
  if (my_codec == NULL) {
    // Error, could not create the codec.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "ACMCodecDB::CreateCodecInstance() failed in CreateCodec()");
    return my_codec;
  }

  return my_codec;
}

// Check if the given codec is a valid to be registered as send codec.
static int IsValidSendCodec(const CodecInst& send_codec,
                            bool is_primary_encoder,
                            int acm_id,
                            int* mirror_id) {
  if ((send_codec.channels != 1) && (send_codec.channels != 2)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                 "Wrong number of channels (%d, only mono and stereo are "
                 "supported) for %s encoder", send_codec.channels,
                 is_primary_encoder ? "primary" : "secondary");
    return -1;
  }

  int codec_id = ACMCodecDB::CodecNumber(send_codec, mirror_id);
  if (codec_id < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                 "Invalid codec setting for the send codec.");
    return -1;
  }

  // TODO(tlegrand): Remove this check. Already taken care of in
  // ACMCodecDB::CodecNumber().
  // Check if the payload-type is valid
  if (!ACMCodecDB::ValidPayloadType(send_codec.pltype)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                 "Invalid payload-type %d for %s.", send_codec.pltype,
                 send_codec.plname);
    return -1;
  }

  // Telephone-event cannot be a send codec.
  if (!STR_CASE_CMP(send_codec.plname, "telephone-event")) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                 "telephone-event cannot be a send codec");
    *mirror_id = -1;
    return -1;
  }

  if (ACMCodecDB::codec_settings_[codec_id].channel_support
      < send_codec.channels) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                 "%d number of channels not supportedn for %s.",
                 send_codec.channels, send_codec.plname);
    *mirror_id = -1;
    return -1;
  }

  if (!is_primary_encoder) {
    // If registering the secondary encoder, then RED and CN are not valid
    // choices as encoder.
    if (IsCodecRED(&send_codec)) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                   "RED cannot be secondary codec");
      *mirror_id = -1;
      return -1;
    }

    if (IsCodecCN(&send_codec)) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, acm_id,
                   "DTX cannot be secondary codec");
      *mirror_id = -1;
      return -1;
    }
  }
  return codec_id;
}

// Can be called multiple times for Codec, CNG, RED.
int AudioCodingModuleImpl::RegisterSendCodec(const CodecInst& send_codec) {
  int mirror_id;
  int codec_id = IsValidSendCodec(send_codec, true, id_, &mirror_id);

  CriticalSectionScoped lock(acm_crit_sect_);

  // Check for reported errors from function IsValidSendCodec().
  if (codec_id < 0) {
    if (!send_codec_registered_) {
      // This values has to be NULL if there is no codec registered.
      current_send_codec_idx_ = -1;
    }
    return -1;
  }

  // RED can be registered with other payload type. If not registered a default
  // payload type is used.
  if (IsCodecRED(&send_codec)) {
    // TODO(tlegrand): Remove this check. Already taken care of in
    // ACMCodecDB::CodecNumber().
    // Check if the payload-type is valid
    if (!ACMCodecDB::ValidPayloadType(send_codec.pltype)) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "Invalid payload-type %d for %s.", send_codec.pltype,
                   send_codec.plname);
      return -1;
    }
    // Set RED payload type.
    red_pltype_ = static_cast<uint8_t>(send_codec.pltype);
    return 0;
  }

  // CNG can be registered with other payload type. If not registered the
  // default payload types from codec database will be used.
  if (IsCodecCN(&send_codec)) {
    // CNG is registered.
    switch (send_codec.plfreq) {
      case 8000: {
        cng_nb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      case 16000: {
        cng_wb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      case 32000: {
        cng_swb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      case 48000: {
        cng_fb_pltype_ = static_cast<uint8_t>(send_codec.pltype);
        break;
      }
      default: {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "RegisterSendCodec() failed, invalid frequency for CNG "
                     "registration");
        return -1;
      }
    }
    SetCngPayloadType(send_codec.plfreq, send_codec.pltype);
    return 0;
  }

  // Set Stereo, and make sure VAD and DTX is turned off.
  if (send_codec.channels == 2) {
    stereo_send_ = true;
    if (vad_enabled_ || dtx_enabled_) {
      WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, id_,
                   "VAD/DTX is turned off, not supported when sending stereo.");
    }
    vad_enabled_ = false;
    dtx_enabled_ = false;
  } else {
    stereo_send_ = false;
  }

  // Check if the codec is already registered as send codec.
  bool is_send_codec;
  if (send_codec_registered_) {
    int send_codec_mirror_id;
    int send_codec_id = ACMCodecDB::CodecNumber(send_codec_inst_,
                                                &send_codec_mirror_id);
    assert(send_codec_id >= 0);
    is_send_codec = (send_codec_id == codec_id) ||
        (mirror_id == send_codec_mirror_id);
  } else {
    is_send_codec = false;
  }

  // If new codec, or new settings, register.
  if (!is_send_codec) {
    if (codecs_[mirror_id] == NULL) {
      codecs_[mirror_id] = CreateCodec(send_codec);
      if (codecs_[mirror_id] == NULL) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Cannot Create the codec");
        return -1;
      }
      mirror_codec_idx_[mirror_id] = mirror_id;
    }

    if (mirror_id != codec_id) {
      codecs_[codec_id] = codecs_[mirror_id];
      mirror_codec_idx_[codec_id] = mirror_id;
    }

    ACMGenericCodec* codec_ptr = codecs_[codec_id];
    WebRtcACMCodecParams codec_params;

    memcpy(&(codec_params.codec_inst), &send_codec, sizeof(CodecInst));
    codec_params.enable_vad = vad_enabled_;
    codec_params.enable_dtx = dtx_enabled_;
    codec_params.vad_mode = vad_mode_;
    // Force initialization.
    if (codec_ptr->InitEncoder(&codec_params, true) < 0) {
      // Could not initialize the encoder.

      // Check if already have a registered codec.
      // Depending on that different messages are logged.
      if (!send_codec_registered_) {
        current_send_codec_idx_ = -1;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Cannot Initialize the encoder No Encoder is registered");
      } else {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Cannot Initialize the encoder, continue encoding with "
                     "the previously registered codec");
      }
      return -1;
    }

    // Update states.
    dtx_enabled_ = codec_params.enable_dtx;
    vad_enabled_ = codec_params.enable_vad;
    vad_mode_ = codec_params.vad_mode;

    // Everything is fine so we can replace the previous codec with this one.
    if (send_codec_registered_) {
      // If we change codec we start fresh with RED.
      // This is not strictly required by the standard.
      codec_ptr->SetVAD(&dtx_enabled_, &vad_enabled_, &vad_mode_);

      if (!codec_ptr->HasInternalFEC()) {
        codec_fec_enabled_ = false;
      } else {
        if (codec_ptr->SetFEC(codec_fec_enabled_) < 0) {
          WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                       "Cannot set codec FEC");
          return -1;
        }
      }
    }

    current_send_codec_idx_ = codec_id;
    send_codec_registered_ = true;
    memcpy(&send_codec_inst_, &send_codec, sizeof(CodecInst));
    return 0;
  } else {
    // If codec is the same as already registered check if any parameters
    // has changed compared to the current values.
    // If any parameter is valid then apply it and record.
    bool force_init = false;

    if (mirror_id != codec_id) {
      codecs_[codec_id] = codecs_[mirror_id];
      mirror_codec_idx_[codec_id] = mirror_id;
    }

    // Check the payload type.
    if (send_codec.pltype != send_codec_inst_.pltype) {
      // At this point check if the given payload type is valid.
      // Record it later when the sampling frequency is changed
      // successfully.
      if (!ACMCodecDB::ValidPayloadType(send_codec.pltype)) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Out of range payload type");
        return -1;
      }
    }

    // If there is a codec that ONE instance of codec supports multiple
    // sampling frequencies, then we need to take care of it here.
    // one such a codec is iSAC. Both WB and SWB are encoded and decoded
    // with one iSAC instance. Therefore, we need to update the encoder
    // frequency if required.
    if (send_codec_inst_.plfreq != send_codec.plfreq) {
      force_init = true;
    }

    // If packet size or number of channels has changed, we need to
    // re-initialize the encoder.
    if (send_codec_inst_.pacsize != send_codec.pacsize) {
      force_init = true;
    }
    if (send_codec_inst_.channels != send_codec.channels) {
      force_init = true;
    }

    if (force_init) {
      WebRtcACMCodecParams codec_params;

      memcpy(&(codec_params.codec_inst), &send_codec, sizeof(CodecInst));
      codec_params.enable_vad = vad_enabled_;
      codec_params.enable_dtx = dtx_enabled_;
      codec_params.vad_mode = vad_mode_;

      // Force initialization.
      if (codecs_[current_send_codec_idx_]->InitEncoder(&codec_params,
                                                        true) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Could not change the codec packet-size.");
        return -1;
      }

      send_codec_inst_.plfreq = send_codec.plfreq;
      send_codec_inst_.pacsize = send_codec.pacsize;
      send_codec_inst_.channels = send_codec.channels;
    }

    // If the change of sampling frequency has been successful then
    // we store the payload-type.
    send_codec_inst_.pltype = send_codec.pltype;

    // Check if a change in Rate is required.
    if (send_codec.rate != send_codec_inst_.rate) {
      if (codecs_[codec_id]->SetBitRate(send_codec.rate) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Could not change the codec rate.");
        return -1;
      }
      send_codec_inst_.rate = send_codec.rate;
    }

    if (!codecs_[codec_id]->HasInternalFEC()) {
      codec_fec_enabled_ = false;
    } else {
      if (codecs_[codec_id]->SetFEC(codec_fec_enabled_) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Cannot set codec FEC");
        return -1;
      }
    }

    return 0;
  }
}

// Get current send codec.
int AudioCodingModuleImpl::SendCodec(
    CodecInst* current_codec) const {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
               "SendCodec()");
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!send_codec_registered_) {
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
                 "SendCodec Failed, no codec is registered");
    return -1;
  }
  WebRtcACMCodecParams encoder_param;
  codecs_[current_send_codec_idx_]->EncoderParams(&encoder_param);
  encoder_param.codec_inst.pltype = send_codec_inst_.pltype;
  memcpy(current_codec, &(encoder_param.codec_inst), sizeof(CodecInst));

  return 0;
}

// Get current send frequency.
int AudioCodingModuleImpl::SendFrequency() const {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
               "SendFrequency()");
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!send_codec_registered_) {
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
                 "SendFrequency Failed, no codec is registered");
    return -1;
  }

  return send_codec_inst_.plfreq;
}

// Get encode bitrate.
// Adaptive rate codecs return their current encode target rate, while other
// codecs return there longterm avarage or their fixed rate.
// TODO(henrik.lundin): Remove; not used.
int AudioCodingModuleImpl::SendBitrate() const {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!send_codec_registered_) {
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
                 "SendBitrate Failed, no codec is registered");
    return -1;
  }

  WebRtcACMCodecParams encoder_param;
  codecs_[current_send_codec_idx_]->EncoderParams(&encoder_param);

  return encoder_param.codec_inst.rate;
}

// Set available bandwidth, inform the encoder about the estimated bandwidth
// received from the remote party.
// TODO(henrik.lundin): Remove; not used.
int AudioCodingModuleImpl::SetReceivedEstimatedBandwidth(int bw) {
  CriticalSectionScoped lock(acm_crit_sect_);
  FATAL() << "Dead code?";
  return -1;
//  return codecs_[current_send_codec_idx_]->SetEstimatedBandwidth(bw);
}

// Register a transport callback which will be called to deliver
// the encoded buffers.
int AudioCodingModuleImpl::RegisterTransportCallback(
    AudioPacketizationCallback* transport) {
  CriticalSectionScoped lock(callback_crit_sect_);
  packetization_callback_ = transport;
  return 0;
}

// Add 10MS of raw (PCM) audio data to the encoder.
int AudioCodingModuleImpl::Add10MsData(const AudioFrame& audio_frame) {
  InputData input_data;
  int r = Add10MsDataInternal(audio_frame, &input_data);
  return r < 0 ? r : Encode(input_data);
}

int AudioCodingModuleImpl::Add10MsDataInternal(const AudioFrame& audio_frame,
                                               InputData* input_data) {
  if (audio_frame.samples_per_channel_ <= 0) {
    assert(false);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Cannot Add 10 ms audio, payload length is negative or "
                 "zero");
    return -1;
  }

  if (audio_frame.sample_rate_hz_ > 48000) {
    assert(false);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Cannot Add 10 ms audio, input frequency not valid");
    return -1;
  }

  // If the length and frequency matches. We currently just support raw PCM.
  if ((audio_frame.sample_rate_hz_ / 100)
      != audio_frame.samples_per_channel_) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Cannot Add 10 ms audio, input frequency and length doesn't"
                 " match");
    return -1;
  }

  if (audio_frame.num_channels_ != 1 && audio_frame.num_channels_ != 2) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Cannot Add 10 ms audio, invalid number of channels.");
    return -1;
  }

  CriticalSectionScoped lock(acm_crit_sect_);
  // Do we have a codec registered?
  if (!HaveValidEncoder("Add10MsData")) {
    return -1;
  }

  const AudioFrame* ptr_frame;
  // Perform a resampling, also down-mix if it is required and can be
  // performed before resampling (a down mix prior to resampling will take
  // place if both primary and secondary encoders are mono and input is in
  // stereo).
  if (PreprocessToAddData(audio_frame, &ptr_frame) < 0) {
    return -1;
  }

  // Check whether we need an up-mix or down-mix?
  bool remix = ptr_frame->num_channels_ != send_codec_inst_.channels;

  if (remix) {
    if (ptr_frame->num_channels_ == 1) {
      if (UpMix(*ptr_frame, WEBRTC_10MS_PCM_AUDIO, input_data->buffer) < 0)
        return -1;
    } else {
      if (DownMix(*ptr_frame, WEBRTC_10MS_PCM_AUDIO, input_data->buffer) < 0)
        return -1;
    }
  }

  // When adding data to encoders this pointer is pointing to an audio buffer
  // with correct number of channels.
  const int16_t* ptr_audio = ptr_frame->data_;

  // For pushing data to primary, point the |ptr_audio| to correct buffer.
  if (send_codec_inst_.channels != ptr_frame->num_channels_)
    ptr_audio = input_data->buffer;

  input_data->input_timestamp = ptr_frame->timestamp_;
  input_data->audio = ptr_audio;
  input_data->length_per_channel = ptr_frame->samples_per_channel_;
  input_data->audio_channel = send_codec_inst_.channels;

  return 0;
}

// Perform a resampling and down-mix if required. We down-mix only if
// encoder is mono and input is stereo. In case of dual-streaming, both
// encoders has to be mono for down-mix to take place.
// |*ptr_out| will point to the pre-processed audio-frame. If no pre-processing
// is required, |*ptr_out| points to |in_frame|.
int AudioCodingModuleImpl::PreprocessToAddData(const AudioFrame& in_frame,
                                               const AudioFrame** ptr_out) {
  bool resample = (in_frame.sample_rate_hz_ != send_codec_inst_.plfreq);

  // This variable is true if primary codec and secondary codec (if exists)
  // are both mono and input is stereo.
  bool down_mix =
      (in_frame.num_channels_ == 2) && (send_codec_inst_.channels == 1);

  if (!first_10ms_data_) {
    expected_in_ts_ = in_frame.timestamp_;
    expected_codec_ts_ = in_frame.timestamp_;
    first_10ms_data_ = true;
  } else if (in_frame.timestamp_ != expected_in_ts_) {
    // TODO(turajs): Do we need a warning here.
    expected_codec_ts_ += (in_frame.timestamp_ - expected_in_ts_) *
        static_cast<uint32_t>((static_cast<double>(send_codec_inst_.plfreq) /
                    static_cast<double>(in_frame.sample_rate_hz_)));
    expected_in_ts_ = in_frame.timestamp_;
  }


  if (!down_mix && !resample) {
    // No pre-processing is required.
    expected_in_ts_ += in_frame.samples_per_channel_;
    expected_codec_ts_ += in_frame.samples_per_channel_;
    *ptr_out = &in_frame;
    return 0;
  }

  *ptr_out = &preprocess_frame_;
  preprocess_frame_.num_channels_ = in_frame.num_channels_;
  int16_t audio[WEBRTC_10MS_PCM_AUDIO];
  const int16_t* src_ptr_audio = in_frame.data_;
  int16_t* dest_ptr_audio = preprocess_frame_.data_;
  if (down_mix) {
    // If a resampling is required the output of a down-mix is written into a
    // local buffer, otherwise, it will be written to the output frame.
    if (resample)
      dest_ptr_audio = audio;
    if (DownMix(in_frame, WEBRTC_10MS_PCM_AUDIO, dest_ptr_audio) < 0)
      return -1;
    preprocess_frame_.num_channels_ = 1;
    // Set the input of the resampler is the down-mixed signal.
    src_ptr_audio = audio;
  }

  preprocess_frame_.timestamp_ = expected_codec_ts_;
  preprocess_frame_.samples_per_channel_ = in_frame.samples_per_channel_;
  preprocess_frame_.sample_rate_hz_ = in_frame.sample_rate_hz_;
  // If it is required, we have to do a resampling.
  if (resample) {
    // The result of the resampler is written to output frame.
    dest_ptr_audio = preprocess_frame_.data_;

    preprocess_frame_.samples_per_channel_ =
        resampler_.Resample10Msec(src_ptr_audio,
                                  in_frame.sample_rate_hz_,
                                  send_codec_inst_.plfreq,
                                  preprocess_frame_.num_channels_,
                                  AudioFrame::kMaxDataSizeSamples,
                                  dest_ptr_audio);

    if (preprocess_frame_.samples_per_channel_ < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "Cannot add 10 ms audio, resampling failed");
      return -1;
    }
    preprocess_frame_.sample_rate_hz_ = send_codec_inst_.plfreq;
  }

  expected_codec_ts_ += preprocess_frame_.samples_per_channel_;
  expected_in_ts_ += in_frame.samples_per_channel_;

  return 0;
}

/////////////////////////////////////////
//   (RED) Redundant Coding
//

bool AudioCodingModuleImpl::REDStatus() const {
  CriticalSectionScoped lock(acm_crit_sect_);

  return red_enabled_;
}

// Configure RED status i.e on/off.
int AudioCodingModuleImpl::SetREDStatus(
#ifdef WEBRTC_CODEC_RED
    bool enable_red) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (enable_red == true && codec_fec_enabled_ == true) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, id_,
                 "Codec internal FEC and RED cannot be co-enabled.");
    return -1;
  }

  EnableCopyRedForAllCodecs(enable_red);
  red_enabled_ = enable_red;
  return 0;
#else
    bool /* enable_red */) {
  WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, id_,
               "  WEBRTC_CODEC_RED is undefined");
  return -1;
#endif
}

/////////////////////////////////////////
//   (FEC) Forward Error Correction (codec internal)
//

bool AudioCodingModuleImpl::CodecFEC() const {
  CriticalSectionScoped lock(acm_crit_sect_);
  return codec_fec_enabled_;
}

int AudioCodingModuleImpl::SetCodecFEC(bool enable_codec_fec) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (enable_codec_fec == true && red_enabled_ == true) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, id_,
                 "Codec internal FEC and RED cannot be co-enabled.");
    return -1;
  }

  // Set codec FEC.
  if (HaveValidEncoder("SetCodecFEC") &&
      codecs_[current_send_codec_idx_]->SetFEC(enable_codec_fec) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "Set codec internal FEC failed.");
    return -1;
  }
  codec_fec_enabled_ = enable_codec_fec;
  return 0;
}

int AudioCodingModuleImpl::SetPacketLossRate(int loss_rate) {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (HaveValidEncoder("SetPacketLossRate") &&
      codecs_[current_send_codec_idx_]->SetPacketLossRate(loss_rate) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "Set packet loss rate failed.");
    return -1;
  }
  return 0;
}

/////////////////////////////////////////
//   (VAD) Voice Activity Detection
//
int AudioCodingModuleImpl::SetVAD(bool enable_dtx,
                                  bool enable_vad,
                                  ACMVADMode mode) {
  CriticalSectionScoped lock(acm_crit_sect_);
  return SetVADSafe(enable_dtx, enable_vad, mode);
}

int AudioCodingModuleImpl::SetVADSafe(bool enable_dtx,
                                      bool enable_vad,
                                      ACMVADMode mode) {
  // Sanity check of the mode.
  if ((mode != VADNormal) && (mode != VADLowBitrate)
      && (mode != VADAggr) && (mode != VADVeryAggr)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Invalid VAD Mode %d, no change is made to VAD/DTX status",
                 mode);
    return -1;
  }

  // Check that the send codec is mono. We don't support VAD/DTX for stereo
  // sending.
  if ((enable_dtx || enable_vad) && stereo_send_) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "VAD/DTX not supported for stereo sending");
    dtx_enabled_ = false;
    vad_enabled_ = false;
    vad_mode_ = mode;
    return -1;
  }

  // Store VAD/DTX settings. Values can be changed in the call to "SetVAD"
  // below.
  dtx_enabled_ = enable_dtx;
  vad_enabled_ = enable_vad;
  vad_mode_ = mode;

  // If a send codec is registered, set VAD/DTX for the codec.
  if (HaveValidEncoder("SetVAD") && codecs_[current_send_codec_idx_]->SetVAD(
      &dtx_enabled_, &vad_enabled_,  &vad_mode_) < 0) {
      // SetVAD failed.
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "SetVAD failed");
      vad_enabled_ = false;
      dtx_enabled_ = false;
      return -1;
  }
  return 0;
}

// Get VAD/DTX settings.
int AudioCodingModuleImpl::VAD(bool* dtx_enabled, bool* vad_enabled,
                               ACMVADMode* mode) const {
  CriticalSectionScoped lock(acm_crit_sect_);

  *dtx_enabled = dtx_enabled_;
  *vad_enabled = vad_enabled_;
  *mode = vad_mode_;

  return 0;
}

/////////////////////////////////////////
//   Receiver
//

int AudioCodingModuleImpl::InitializeReceiver() {
  CriticalSectionScoped lock(acm_crit_sect_);
  return InitializeReceiverSafe();
}

// Initialize receiver, resets codec database etc.
int AudioCodingModuleImpl::InitializeReceiverSafe() {
  // If the receiver is already initialized then we want to destroy any
  // existing decoders. After a call to this function, we should have a clean
  // start-up.
  if (receiver_initialized_) {
    if (receiver_.RemoveAllCodecs() < 0)
      return -1;
  }
  receiver_.set_id(id_);
  receiver_.ResetInitialDelay();
  receiver_.SetMinimumDelay(0);
  receiver_.SetMaximumDelay(0);
  receiver_.FlushBuffers();

  // Register RED and CN.
  for (int i = 0; i < ACMCodecDB::kNumCodecs; i++) {
    if (IsCodecRED(i) || IsCodecCN(i)) {
      uint8_t pl_type = static_cast<uint8_t>(ACMCodecDB::database_[i].pltype);
      if (receiver_.AddCodec(i, pl_type, 1, NULL) < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Cannot register master codec.");
        return -1;
      }
    }
  }
  receiver_initialized_ = true;
  return 0;
}

// TODO(turajs): If NetEq opens an API for reseting the state of decoders then
// implement this method. Otherwise it should be removed. I might be that by
// removing and registering a decoder we can achieve the effect of resetting.
// Reset the decoder state.
// TODO(henrik.lundin): Remove; only used in one test, and does nothing.
int AudioCodingModuleImpl::ResetDecoder() {
  return 0;
}

// Get current receive frequency.
int AudioCodingModuleImpl::ReceiveFrequency() const {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
               "ReceiveFrequency()");

  CriticalSectionScoped lock(acm_crit_sect_);

  int codec_id = receiver_.last_audio_codec_id();

  return codec_id < 0 ? receiver_.current_sample_rate_hz() :
                        ACMCodecDB::database_[codec_id].plfreq;
}

// Get current playout frequency.
int AudioCodingModuleImpl::PlayoutFrequency() const {
  WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceAudioCoding, id_,
               "PlayoutFrequency()");

  CriticalSectionScoped lock(acm_crit_sect_);

  return receiver_.current_sample_rate_hz();
}

// Register possible receive codecs, can be called multiple times,
// for codecs, CNG (NB, WB and SWB), DTMF, RED.
int AudioCodingModuleImpl::RegisterReceiveCodec(const CodecInst& codec) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (codec.channels > 2 || codec.channels < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Unsupported number of channels, %d.", codec.channels);
    return -1;
  }

  // TODO(turajs) do we need this for NetEq 4?
  if (!receiver_initialized_) {
    if (InitializeReceiverSafe() < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                   "Cannot initialize receiver, failed registering codec.");
      return -1;
    }
  }

  int mirror_id;
  int codec_id = ACMCodecDB::ReceiverCodecNumber(codec, &mirror_id);

  if (codec_id < 0 || codec_id >= ACMCodecDB::kNumCodecs) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Wrong codec params to be registered as receive codec");
    return -1;
  }

  // Check if the payload-type is valid.
  if (!ACMCodecDB::ValidPayloadType(codec.pltype)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Invalid payload-type %d for %s.", codec.pltype,
                 codec.plname);
    return -1;
  }

  AudioDecoder* decoder = NULL;
  // Get |decoder| associated with |codec|. |decoder| can be NULL if |codec|
  // does not own its decoder.
  if (GetAudioDecoder(codec, codec_id, mirror_id, &decoder) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Wrong codec params to be registered as receive codec");
    return -1;
  }
  uint8_t payload_type = static_cast<uint8_t>(codec.pltype);
  return receiver_.AddCodec(codec_id, payload_type, codec.channels, decoder);
}

// Get current received codec.
int AudioCodingModuleImpl::ReceiveCodec(CodecInst* current_codec) const {
  return receiver_.LastAudioCodec(current_codec);
}

// Incoming packet from network parsed and ready for decode.
int AudioCodingModuleImpl::IncomingPacket(const uint8_t* incoming_payload,
                                          const size_t payload_length,
                                          const WebRtcRTPHeader& rtp_header) {
  return receiver_.InsertPacket(rtp_header, incoming_payload, payload_length);
}

// Minimum playout delay (Used for lip-sync).
int AudioCodingModuleImpl::SetMinimumPlayoutDelay(int time_ms) {
  if ((time_ms < 0) || (time_ms > 10000)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Delay must be in the range of 0-1000 milliseconds.");
    return -1;
  }
  return receiver_.SetMinimumDelay(time_ms);
}

int AudioCodingModuleImpl::SetMaximumPlayoutDelay(int time_ms) {
  if ((time_ms < 0) || (time_ms > 10000)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "Delay must be in the range of 0-1000 milliseconds.");
    return -1;
  }
  return receiver_.SetMaximumDelay(time_ms);
}

// Estimate the Bandwidth based on the incoming stream, needed for one way
// audio where the RTCP send the BW estimate.
// This is also done in the RTP module.
int AudioCodingModuleImpl::DecoderEstimatedBandwidth() const {
  // We can estimate far-end to near-end bandwidth if the iSAC are sent. Check
  // if the last received packets were iSAC packet then retrieve the bandwidth.
  int last_audio_codec_id = receiver_.last_audio_codec_id();
  if (last_audio_codec_id >= 0 &&
      STR_CASE_CMP("ISAC", ACMCodecDB::database_[last_audio_codec_id].plname)) {
    CriticalSectionScoped lock(acm_crit_sect_);
    FATAL() << "Dead code?";
//    return codecs_[last_audio_codec_id]->GetEstimatedBandwidth();
  }
  return -1;
}

// Set playout mode for: voice, fax, streaming or off.
int AudioCodingModuleImpl::SetPlayoutMode(AudioPlayoutMode mode) {
  receiver_.SetPlayoutMode(mode);
  return 0;  // TODO(turajs): return value is for backward compatibility.
}

// Get playout mode voice, fax, streaming or off.
AudioPlayoutMode AudioCodingModuleImpl::PlayoutMode() const {
  return receiver_.PlayoutMode();
}

// Get 10 milliseconds of raw audio data to play out.
// Automatic resample to the requested frequency.
int AudioCodingModuleImpl::PlayoutData10Ms(int desired_freq_hz,
                                           AudioFrame* audio_frame) {
  // GetAudio always returns 10 ms, at the requested sample rate.
  if (receiver_.GetAudio(desired_freq_hz, audio_frame) != 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "PlayoutData failed, RecOut Failed");
    return -1;
  }

  audio_frame->id_ = id_;
  return 0;
}

/////////////////////////////////////////
//   Statistics
//

// TODO(turajs) change the return value to void. Also change the corresponding
// NetEq function.
int AudioCodingModuleImpl::GetNetworkStatistics(NetworkStatistics* statistics) {
  receiver_.GetNetworkStatistics(statistics);
  return 0;
}

int AudioCodingModuleImpl::RegisterVADCallback(ACMVADCallback* vad_callback) {
  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, id_,
               "RegisterVADCallback()");
  CriticalSectionScoped lock(callback_crit_sect_);
  vad_callback_ = vad_callback;
  return 0;
}

// TODO(tlegrand): Modify this function to work for stereo, and add tests.
int AudioCodingModuleImpl::IncomingPayload(const uint8_t* incoming_payload,
                                           size_t payload_length,
                                           uint8_t payload_type,
                                           uint32_t timestamp) {
  // We are not acquiring any lock when interacting with |aux_rtp_header_| no
  // other method uses this member variable.
  if (aux_rtp_header_ == NULL) {
    // This is the first time that we are using |dummy_rtp_header_|
    // so we have to create it.
    aux_rtp_header_ = new WebRtcRTPHeader;
    aux_rtp_header_->header.payloadType = payload_type;
    // Don't matter in this case.
    aux_rtp_header_->header.ssrc = 0;
    aux_rtp_header_->header.markerBit = false;
    // Start with random numbers.
    aux_rtp_header_->header.sequenceNumber = 0x1234;  // Arbitrary.
    aux_rtp_header_->type.Audio.channel = 1;
  }

  aux_rtp_header_->header.timestamp = timestamp;
  IncomingPacket(incoming_payload, payload_length, *aux_rtp_header_);
  // Get ready for the next payload.
  aux_rtp_header_->header.sequenceNumber++;
  return 0;
}

int AudioCodingModuleImpl::ReplaceInternalDTXWithWebRtc(bool use_webrtc_dtx) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!HaveValidEncoder("ReplaceInternalDTXWithWebRtc")) {
    WEBRTC_TRACE(
        webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
        "Cannot replace codec internal DTX when no send codec is registered.");
    return -1;
  }

  FATAL() << "Dead code?";
//  int res = codecs_[current_send_codec_idx_]->ReplaceInternalDTX(
//      use_webrtc_dtx);
  // Check if VAD is turned on, or if there is any error.
//  if (res == 1) {
//    vad_enabled_ = true;
//  } else if (res < 0) {
//    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
//                 "Failed to set ReplaceInternalDTXWithWebRtc(%d)",
//                 use_webrtc_dtx);
//    return res;
//  }

  return 0;
}

int AudioCodingModuleImpl::IsInternalDTXReplacedWithWebRtc(
    bool* uses_webrtc_dtx) {
  *uses_webrtc_dtx = true;
  return 0;
}

// TODO(henrik.lundin): Remove? Only used in tests. Deprecated in VoiceEngine.
int AudioCodingModuleImpl::SetISACMaxRate(int max_bit_per_sec) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!HaveValidEncoder("SetISACMaxRate")) {
    return -1;
  }

  return codecs_[current_send_codec_idx_]->SetISACMaxRate(max_bit_per_sec);
}

// TODO(henrik.lundin): Remove? Only used in tests. Deprecated in VoiceEngine.
int AudioCodingModuleImpl::SetISACMaxPayloadSize(int max_size_bytes) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!HaveValidEncoder("SetISACMaxPayloadSize")) {
    return -1;
  }

  return codecs_[current_send_codec_idx_]->SetISACMaxPayloadSize(
      max_size_bytes);
}

// TODO(henrik.lundin): Remove? Only used in tests.
int AudioCodingModuleImpl::ConfigISACBandwidthEstimator(
    int frame_size_ms,
    int rate_bit_per_sec,
    bool enforce_frame_size) {
  CriticalSectionScoped lock(acm_crit_sect_);

  if (!HaveValidEncoder("ConfigISACBandwidthEstimator")) {
    return -1;
  }

  FATAL() << "Dead code?";
  return -1;
//  return codecs_[current_send_codec_idx_]->ConfigISACBandwidthEstimator(
//      frame_size_ms, rate_bit_per_sec, enforce_frame_size);
}

int AudioCodingModuleImpl::SetOpusApplication(OpusApplicationMode application,
                                              bool disable_dtx_if_needed) {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (!HaveValidEncoder("SetOpusApplication")) {
    return -1;
  }
  return codecs_[current_send_codec_idx_]->SetOpusApplication(
      application, disable_dtx_if_needed);
}

// Informs Opus encoder of the maximum playback rate the receiver will render.
int AudioCodingModuleImpl::SetOpusMaxPlaybackRate(int frequency_hz) {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (!HaveValidEncoder("SetOpusMaxPlaybackRate")) {
    return -1;
  }
  return codecs_[current_send_codec_idx_]->SetOpusMaxPlaybackRate(frequency_hz);
}

int AudioCodingModuleImpl::EnableOpusDtx(bool force_voip) {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (!HaveValidEncoder("EnableOpusDtx")) {
    return -1;
  }
  return codecs_[current_send_codec_idx_]->EnableOpusDtx(force_voip);
}

int AudioCodingModuleImpl::DisableOpusDtx() {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (!HaveValidEncoder("DisableOpusDtx")) {
    return -1;
  }
  return codecs_[current_send_codec_idx_]->DisableOpusDtx();
}

int AudioCodingModuleImpl::PlayoutTimestamp(uint32_t* timestamp) {
  return receiver_.GetPlayoutTimestamp(timestamp) ? 0 : -1;
}

bool AudioCodingModuleImpl::HaveValidEncoder(const char* caller_name) const {
  if ((!send_codec_registered_) || (current_send_codec_idx_ < 0) ||
      (current_send_codec_idx_ >= ACMCodecDB::kNumCodecs)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "%s failed: No send codec is registered.", caller_name);
    return false;
  }
  if ((current_send_codec_idx_ < 0) ||
      (current_send_codec_idx_ >= ACMCodecDB::kNumCodecs)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "%s failed: Send codec index out of range.", caller_name);
    return false;
  }
  if (codecs_[current_send_codec_idx_] == NULL) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                 "%s failed: Send codec is NULL pointer.", caller_name);
    return false;
  }
  return true;
}

int AudioCodingModuleImpl::UnregisterReceiveCodec(uint8_t payload_type) {
  return receiver_.RemoveCodec(payload_type);
}

// TODO(turajs): correct the type of |length_bytes| when it is corrected in
// GenericCodec.
int AudioCodingModuleImpl::REDPayloadISAC(int isac_rate,
                                          int isac_bw_estimate,
                                          uint8_t* payload,
                                          int16_t* length_bytes) {
  CriticalSectionScoped lock(acm_crit_sect_);
  if (!HaveValidEncoder("EncodeData")) {
    return -1;
  }
  FATAL() << "Dead code?";
  return -1;
//  int status;
//  status = codecs_[current_send_codec_idx_]->REDPayloadISAC(isac_rate,
//                                                            isac_bw_estimate,
//                                                            payload,
//                                                            length_bytes);
//  return status;
}

int AudioCodingModuleImpl::GetAudioDecoder(const CodecInst& codec, int codec_id,
                                           int mirror_id,
                                           AudioDecoder** decoder) {
  if (ACMCodecDB::OwnsDecoder(codec_id)) {
    // This codec has to own its own decoder. Therefore, it should create the
    // corresponding AudioDecoder class and insert it into NetEq. If the codec
    // does not exist create it.
    //
    // TODO(turajs): this part of the code is common with RegisterSendCodec(),
    //               make a method for it.
    if (codecs_[mirror_id] == NULL) {
      codecs_[mirror_id] = CreateCodec(codec);
      if (codecs_[mirror_id] == NULL) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, id_,
                     "Cannot Create the codec");
        return -1;
      }
      mirror_codec_idx_[mirror_id] = mirror_id;
    }

    if (mirror_id != codec_id) {
      codecs_[codec_id] = codecs_[mirror_id];
      mirror_codec_idx_[codec_id] = mirror_id;
    }
    *decoder = codecs_[codec_id]->Decoder();
    if (!*decoder) {
      assert(false);
      return -1;
    }
  } else {
    *decoder = NULL;
  }

  return 0;
}

void AudioCodingModuleImpl::SetCngPayloadType(int sample_rate_hz,
                                              int payload_type) {
  for (auto* codec : codecs_) {
    if (codec) {
      codec->SetCngPt(sample_rate_hz, payload_type);
    }
  }
}

void AudioCodingModuleImpl::EnableCopyRedForAllCodecs(bool enable) {
  for (auto* codec : codecs_) {
    if (codec) {
      codec->EnableCopyRed(enable, red_pltype_);
    }
  }
}

int AudioCodingModuleImpl::SetInitialPlayoutDelay(int delay_ms) {
  {
    CriticalSectionScoped lock(acm_crit_sect_);
    // Initialize receiver, if it is not initialized. Otherwise, initial delay
    // is reset upon initialization of the receiver.
    if (!receiver_initialized_)
      InitializeReceiverSafe();
  }
  return receiver_.SetInitialDelay(delay_ms);
}

int AudioCodingModuleImpl::EnableNack(size_t max_nack_list_size) {
  return receiver_.EnableNack(max_nack_list_size);
}

void AudioCodingModuleImpl::DisableNack() {
  receiver_.DisableNack();
}

std::vector<uint16_t> AudioCodingModuleImpl::GetNackList(
    int64_t round_trip_time_ms) const {
  return receiver_.GetNackList(round_trip_time_ms);
}

int AudioCodingModuleImpl::LeastRequiredDelayMs() const {
  return receiver_.LeastRequiredDelayMs();
}

void AudioCodingModuleImpl::GetDecodingCallStatistics(
      AudioDecodingCallStats* call_stats) const {
  receiver_.GetDecodingCallStatistics(call_stats);
}

}  // namespace acm2

bool AudioCodingImpl::RegisterSendCodec(AudioEncoder* send_codec) {
  FATAL() << "Not implemented yet.";
  return false;
}

bool AudioCodingImpl::RegisterSendCodec(int encoder_type,
                                        uint8_t payload_type,
                                        int frame_size_samples) {
  std::string codec_name;
  int sample_rate_hz;
  int channels;
  if (!MapCodecTypeToParameters(
          encoder_type, &codec_name, &sample_rate_hz, &channels)) {
    return false;
  }
  webrtc::CodecInst codec;
  AudioCodingModule::Codec(
      codec_name.c_str(), &codec, sample_rate_hz, channels);
  codec.pltype = payload_type;
  if (frame_size_samples > 0) {
    codec.pacsize = frame_size_samples;
  }
  return acm_old_->RegisterSendCodec(codec) == 0;
}

const AudioEncoder* AudioCodingImpl::GetSenderInfo() const {
  FATAL() << "Not implemented yet.";
  return reinterpret_cast<const AudioEncoder*>(NULL);
}

const CodecInst* AudioCodingImpl::GetSenderCodecInst() {
  if (acm_old_->SendCodec(&current_send_codec_) != 0) {
    return NULL;
  }
  return &current_send_codec_;
}

int AudioCodingImpl::Add10MsAudio(const AudioFrame& audio_frame) {
  acm2::AudioCodingModuleImpl::InputData input_data;
  if (acm_old_->Add10MsDataInternal(audio_frame, &input_data) != 0)
    return -1;
  return acm_old_->Encode(input_data);
}

const ReceiverInfo* AudioCodingImpl::GetReceiverInfo() const {
  FATAL() << "Not implemented yet.";
  return reinterpret_cast<const ReceiverInfo*>(NULL);
}

bool AudioCodingImpl::RegisterReceiveCodec(AudioDecoder* receive_codec) {
  FATAL() << "Not implemented yet.";
  return false;
}

bool AudioCodingImpl::RegisterReceiveCodec(int decoder_type,
                                           uint8_t payload_type) {
  std::string codec_name;
  int sample_rate_hz;
  int channels;
  if (!MapCodecTypeToParameters(
          decoder_type, &codec_name, &sample_rate_hz, &channels)) {
    return false;
  }
  webrtc::CodecInst codec;
  AudioCodingModule::Codec(
      codec_name.c_str(), &codec, sample_rate_hz, channels);
  codec.pltype = payload_type;
  return acm_old_->RegisterReceiveCodec(codec) == 0;
}

bool AudioCodingImpl::InsertPacket(const uint8_t* incoming_payload,
                                   size_t payload_len_bytes,
                                   const WebRtcRTPHeader& rtp_info) {
  return acm_old_->IncomingPacket(
             incoming_payload, payload_len_bytes, rtp_info) == 0;
}

bool AudioCodingImpl::InsertPayload(const uint8_t* incoming_payload,
                                    size_t payload_len_byte,
                                    uint8_t payload_type,
                                    uint32_t timestamp) {
  FATAL() << "Not implemented yet.";
  return false;
}

bool AudioCodingImpl::SetMinimumPlayoutDelay(int time_ms) {
  FATAL() << "Not implemented yet.";
  return false;
}

bool AudioCodingImpl::SetMaximumPlayoutDelay(int time_ms) {
  FATAL() << "Not implemented yet.";
  return false;
}

int AudioCodingImpl::LeastRequiredDelayMs() const {
  FATAL() << "Not implemented yet.";
  return -1;
}

bool AudioCodingImpl::PlayoutTimestamp(uint32_t* timestamp) {
  FATAL() << "Not implemented yet.";
  return false;
}

bool AudioCodingImpl::Get10MsAudio(AudioFrame* audio_frame) {
  return acm_old_->PlayoutData10Ms(playout_frequency_hz_, audio_frame) == 0;
}

bool AudioCodingImpl::GetNetworkStatistics(
    NetworkStatistics* network_statistics) {
  FATAL() << "Not implemented yet.";
  return false;
}

bool AudioCodingImpl::EnableNack(size_t max_nack_list_size) {
  FATAL() << "Not implemented yet.";
  return false;
}

void AudioCodingImpl::DisableNack() {
  // A bug in the linker of Visual Studio 2013 Update 3 prevent us from using
  // FATAL() here, if we do so then the linker hang when the WPO is turned on.
  // TODO(sebmarchand): Re-evaluate this when we upgrade the toolchain.
}

bool AudioCodingImpl::SetVad(bool enable_dtx,
                             bool enable_vad,
                             ACMVADMode vad_mode) {
  return acm_old_->SetVAD(enable_dtx, enable_vad, vad_mode) == 0;
}

std::vector<uint16_t> AudioCodingImpl::GetNackList(
    int round_trip_time_ms) const {
  return acm_old_->GetNackList(round_trip_time_ms);
}

void AudioCodingImpl::GetDecodingCallStatistics(
    AudioDecodingCallStats* call_stats) const {
  acm_old_->GetDecodingCallStatistics(call_stats);
}

bool AudioCodingImpl::MapCodecTypeToParameters(int codec_type,
                                               std::string* codec_name,
                                               int* sample_rate_hz,
                                               int* channels) {
  switch (codec_type) {
#ifdef WEBRTC_CODEC_PCM16
    case acm2::ACMCodecDB::kPCM16B:
      *codec_name = "L16";
      *sample_rate_hz = 8000;
      *channels = 1;
      break;
    case acm2::ACMCodecDB::kPCM16Bwb:
      *codec_name = "L16";
      *sample_rate_hz = 16000;
      *channels = 1;
      break;
    case acm2::ACMCodecDB::kPCM16Bswb32kHz:
      *codec_name = "L16";
      *sample_rate_hz = 32000;
      *channels = 1;
      break;
    case acm2::ACMCodecDB::kPCM16B_2ch:
      *codec_name = "L16";
      *sample_rate_hz = 8000;
      *channels = 2;
      break;
    case acm2::ACMCodecDB::kPCM16Bwb_2ch:
      *codec_name = "L16";
      *sample_rate_hz = 16000;
      *channels = 2;
      break;
    case acm2::ACMCodecDB::kPCM16Bswb32kHz_2ch:
      *codec_name = "L16";
      *sample_rate_hz = 32000;
      *channels = 2;
      break;
#endif
#if (defined(WEBRTC_CODEC_ISAC) || defined(WEBRTC_CODEC_ISACFX))
    case acm2::ACMCodecDB::kISAC:
      *codec_name = "ISAC";
      *sample_rate_hz = 16000;
      *channels = 1;
      break;
#endif
#ifdef WEBRTC_CODEC_ISAC
    case acm2::ACMCodecDB::kISACSWB:
      *codec_name = "ISAC";
      *sample_rate_hz = 32000;
      *channels = 1;
      break;
    case acm2::ACMCodecDB::kISACFB:
      *codec_name = "ISAC";
      *sample_rate_hz = 48000;
      *channels = 1;
      break;
#endif
#ifdef WEBRTC_CODEC_ILBC
    case acm2::ACMCodecDB::kILBC:
      *codec_name = "ILBC";
      *sample_rate_hz = 8000;
      *channels = 1;
      break;
#endif
    case acm2::ACMCodecDB::kPCMA:
      *codec_name = "PCMA";
      *sample_rate_hz = 8000;
      *channels = 1;
      break;
    case acm2::ACMCodecDB::kPCMA_2ch:
      *codec_name = "PCMA";
      *sample_rate_hz = 8000;
      *channels = 2;
      break;
    case acm2::ACMCodecDB::kPCMU:
      *codec_name = "PCMU";
      *sample_rate_hz = 8000;
      *channels = 1;
      break;
    case acm2::ACMCodecDB::kPCMU_2ch:
      *codec_name = "PCMU";
      *sample_rate_hz = 8000;
      *channels = 2;
      break;
#ifdef WEBRTC_CODEC_G722
    case acm2::ACMCodecDB::kG722:
      *codec_name = "G722";
      *sample_rate_hz = 16000;
      *channels = 1;
      break;
    case acm2::ACMCodecDB::kG722_2ch:
      *codec_name = "G722";
      *sample_rate_hz = 16000;
      *channels = 2;
      break;
#endif
#ifdef WEBRTC_CODEC_OPUS
    case acm2::ACMCodecDB::kOpus:
      *codec_name = "opus";
      *sample_rate_hz = 48000;
      *channels = 2;
      break;
#endif
    case acm2::ACMCodecDB::kCNNB:
      *codec_name = "CN";
      *sample_rate_hz = 8000;
      *channels = 1;
      break;
    case acm2::ACMCodecDB::kCNWB:
      *codec_name = "CN";
      *sample_rate_hz = 16000;
      *channels = 1;
      break;
    case acm2::ACMCodecDB::kCNSWB:
      *codec_name = "CN";
      *sample_rate_hz = 32000;
      *channels = 1;
      break;
    case acm2::ACMCodecDB::kRED:
      *codec_name = "red";
      *sample_rate_hz = 8000;
      *channels = 1;
      break;
#ifdef WEBRTC_CODEC_AVT
    case acm2::ACMCodecDB::kAVT:
      *codec_name = "telephone-event";
      *sample_rate_hz = 8000;
      *channels = 1;
      break;
#endif
    default:
      FATAL() << "Codec type " << codec_type << " not supported.";
  }
  return true;
}

}  // namespace webrtc
