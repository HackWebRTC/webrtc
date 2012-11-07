/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_opus.h"

#include "acm_codec_database.h"
#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

#ifdef WEBRTC_CODEC_OPUS
#include "modules/audio_coding/codecs/opus/interface/opus_interface.h"
#endif

namespace webrtc {

#ifndef WEBRTC_CODEC_OPUS

ACMOpus::ACMOpus(int16_t /* codecID */)
    : _encoderInstPtr(NULL),
      _decoderInstPtr(NULL),
      _sampleFreq(0),
      _bitrate(0),
      _channels(1) {
  return;
}

ACMOpus::~ACMOpus() {
  return;
}

int16_t ACMOpus::InternalEncode(uint8_t* /* bitStream */,
                                int16_t* /* bitStreamLenByte */) {
  return -1;
}

int16_t ACMOpus::DecodeSafe(uint8_t* /* bitStream */,
                            int16_t /* bitStreamLenByte */,
                            int16_t* /* audio */,
                            int16_t* /* audioSamples */,
                            int8_t* /* speechType */) {
  return -1;
}

int16_t ACMOpus::InternalInitEncoder(WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

int16_t ACMOpus::InternalInitDecoder(WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

int32_t ACMOpus::CodecDef(WebRtcNetEQ_CodecDef& /* codecDef */,
                          const CodecInst& /* codecInst */) {
  return -1;
}

ACMGenericCodec* ACMOpus::CreateInstance(void) {
  return NULL;
}

int16_t ACMOpus::InternalCreateEncoder() {
  return -1;
}

void ACMOpus::DestructEncoderSafe() {
  return;
}

int16_t ACMOpus::InternalCreateDecoder() {
  return -1;
}

void ACMOpus::DestructDecoderSafe() {
  return;
}

void ACMOpus::InternalDestructEncoderInst(void* /* ptrInst */) {
  return;
}

int16_t ACMOpus::SetBitRateSafe(const int32_t /*rate*/) {
  return -1;
}

bool ACMOpus::IsTrueStereoCodec() {
  return true;
}

void ACMOpus::SplitStereoPacket(uint8_t* /*payload*/,
                                int32_t* /*payload_length*/) {}

#else  //===================== Actual Implementation =======================

ACMOpus::ACMOpus(int16_t codecID)
    : _encoderInstPtr(NULL),
      _decoderInstPtr(NULL),
      _sampleFreq(32000),  // Default sampling frequency.
      _bitrate(20000),  // Default bit-rate.
      _channels(1) {  // Default mono
  _codecID = codecID;
  // Opus has internal DTX, but we dont use it for now.
  _hasInternalDTX = false;

  if ((_codecID != ACMCodecDB::kOpus) && (_codecID != ACMCodecDB::kOpus_2ch)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "Wrong codec id for Opus.");
    _sampleFreq = -1;
    _bitrate = -1;
  }
  return;
}

ACMOpus::~ACMOpus() {
  if (_encoderInstPtr != NULL) {
    WebRtcOpus_EncoderFree(_encoderInstPtr);
    _encoderInstPtr = NULL;
  }
  if (_decoderInstPtr != NULL) {
    WebRtcOpus_DecoderFree(_decoderInstPtr);
    _decoderInstPtr = NULL;
  }
  return;
}

int16_t ACMOpus::InternalEncode(uint8_t* bitStream, int16_t* bitStreamLenByte) {
  // Call Encoder.
  *bitStreamLenByte = WebRtcOpus_Encode(_encoderInstPtr,
                                        &_inAudio[_inAudioIxRead],
                                        _frameLenSmpl,
                                        MAX_PAYLOAD_SIZE_BYTE,
                                        bitStream);
  // Check for error reported from encoder.
  if (*bitStreamLenByte < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "InternalEncode: Encode error for Opus");
    *bitStreamLenByte = 0;
    return -1;
  }

  // Increment the read index. This tells the caller how far
  // we have gone forward in reading the audio buffer.
  _inAudioIxRead += _frameLenSmpl * _channels;

  return *bitStreamLenByte;
}

int16_t ACMOpus::DecodeSafe(uint8_t* bitStream, int16_t bitStreamLenByte,
                            int16_t* audio, int16_t* audioSamples,
                            int8_t* speechType) {
  return 0;
}

int16_t ACMOpus::InternalInitEncoder(WebRtcACMCodecParams* codecParams) {
  int16_t ret;
  if (_encoderInstPtr != NULL) {
    WebRtcOpus_EncoderFree(_encoderInstPtr);
    _encoderInstPtr = NULL;
  }
  ret = WebRtcOpus_EncoderCreate(&_encoderInstPtr,
                                 codecParams->codecInstant.channels);
  // Store number of channels.
  _channels = codecParams->codecInstant.channels;

  if (ret < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "Encoder creation failed for Opus");
    return ret;
  }
  ret = WebRtcOpus_SetBitRate(_encoderInstPtr, codecParams->codecInstant.rate);
  if (ret < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "Setting initial bitrate failed for Opus");
    return ret;
  }

  // Store bitrate.
  _bitrate = codecParams->codecInstant.rate;

  return 0;
}

int16_t ACMOpus::InternalInitDecoder(WebRtcACMCodecParams* codecParams) {
  if (_decoderInstPtr != NULL) {
    WebRtcOpus_DecoderFree(_decoderInstPtr);
    _decoderInstPtr = NULL;
  }
  if (WebRtcOpus_DecoderCreate(&_decoderInstPtr,
                               codecParams->codecInstant.channels) < 0) {
    return -1;
  }

  if (WebRtcOpus_DecoderInit(_decoderInstPtr) < 0) {
    return -1;
  }
  if (WebRtcOpus_DecoderInitSlave(_decoderInstPtr) < 0) {
    return -1;
  }
  return 0;
}

int32_t ACMOpus::CodecDef(WebRtcNetEQ_CodecDef& codecDef,
                          const CodecInst& codecInst) {
  if (!_decoderInitialized) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "CodeDef: Decoder uninitialized for Opus");
    return -1;
  }

  // Fill up the structure by calling "SET_CODEC_PAR" & "SET_OPUS_FUNCTION."
  // Then call NetEQ to add the codec to its database.
  // TODO(tlegrand): Decoder is registered in NetEQ as a 32 kHz decoder, which
  // is true until we have a full 48 kHz system, and remove the downsampling
  // in the Opus decoder wrapper.
  if (codecInst.channels == 1) {
    SET_CODEC_PAR(codecDef, kDecoderOpus, codecInst.pltype, _decoderInstPtr,
                  32000);
  } else {
    SET_CODEC_PAR(codecDef, kDecoderOpus_2ch, codecInst.pltype,
                  _decoderInstPtr, 32000);
  }

  // If this is the master of NetEQ, regular decoder will be added, otherwise
  // the slave decoder will be used.
  if (_isMaster) {
    SET_OPUS_FUNCTIONS(codecDef);
  } else {
    SET_OPUSSLAVE_FUNCTIONS(codecDef);
  }

  return 0;
}


ACMGenericCodec* ACMOpus::CreateInstance(void) {
  return NULL;
}

int16_t ACMOpus::InternalCreateEncoder() {
  // Real encoder will be created in InternalInitEncoder.
  return 0;
}

void ACMOpus::DestructEncoderSafe() {
  if (_encoderInstPtr) {
    WebRtcOpus_EncoderFree(_encoderInstPtr);
    _encoderInstPtr = NULL;
  }
}

int16_t ACMOpus::InternalCreateDecoder() {
  // Real decoder will be created in InternalInitDecoder
  return 0;
}

void ACMOpus::DestructDecoderSafe() {
  _decoderInitialized = false;
  if (_decoderInstPtr) {
    WebRtcOpus_DecoderFree(_decoderInstPtr);
    _decoderInstPtr = NULL;
  }
}

void ACMOpus::InternalDestructEncoderInst(void* ptrInst) {
  if (ptrInst != NULL) {
    WebRtcOpus_EncoderFree((OpusEncInst*) ptrInst);
  }
  return;
}

int16_t ACMOpus::SetBitRateSafe(const int32_t rate) {
  if (rate < 6000 || rate > 510000) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "SetBitRateSafe: Invalid rate Opus");
    return -1;
  }

  _bitrate = rate;

  // Ask the encoder for the new rate.
  if (WebRtcOpus_SetBitRate(_encoderInstPtr, _bitrate) >= 0) {
    _encoderParams.codecInstant.rate = _bitrate;
    return 0;
  }

  return -1;
}

bool ACMOpus::IsTrueStereoCodec() {
  return true;
}

// Copy the stereo packet so that NetEq will insert into both master and slave.
void ACMOpus::SplitStereoPacket(uint8_t* payload, int32_t* payload_length) {
  // Check for valid inputs.
  assert(payload != NULL);
  assert(*payload_length > 0);

  // Duplicate the payload.
  memcpy(&payload[*payload_length], &payload[0],
         sizeof(uint8_t) * (*payload_length));
  // Double the size of the packet.
  *payload_length *= 2;
}

#endif  // WEBRTC_CODEC_OPUS

}  // namespace webrtc
