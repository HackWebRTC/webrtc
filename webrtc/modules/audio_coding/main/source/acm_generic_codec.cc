/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include <string.h>

#include "acm_codec_database.h"
#include "acm_common_defs.h"
#include "acm_generic_codec.h"
#include "acm_neteq.h"
#include "trace.h"
#include "webrtc_vad.h"
#include "webrtc_cng.h"

namespace webrtc {

// Enum for CNG
enum {
  kMaxPLCParamsCNG = WEBRTC_CNG_MAX_LPC_ORDER,
  kNewCNGNumPLCParams = 8
};

// Interval for sending new CNG parameters (SID frames) is 100 msec.
enum {
  kAcmSidIntervalMsec = 100
};

// We set some of the variables to invalid values as a check point
// if a proper initialization has happened. Another approach is
// to initialize to a default codec that we are sure is always included.
ACMGenericCodec::ACMGenericCodec()
    : _inAudioIxWrite(0),
      _inAudioIxRead(0),
      _inTimestampIxWrite(0),
      _inAudio(NULL),
      _inTimestamp(NULL),
      _frameLenSmpl(-1),  // invalid value
      _noChannels(1),
      _codecID(-1),  // invalid value
      _noMissedSamples(0),
      _encoderExist(false),
      _decoderExist(false),
      _encoderInitialized(false),
      _decoderInitialized(false),
      _registeredInNetEq(false),
      _hasInternalDTX(false),
      _ptrVADInst(NULL),
      _vadEnabled(false),
      _vadMode(VADNormal),
      _dtxEnabled(false),
      _ptrDTXInst(NULL),
      _numLPCParams(kNewCNGNumPLCParams),
      _sentCNPrevious(false),
      _isMaster(true),
      _prev_frame_cng(0),
      _netEqDecodeLock(NULL),
      _codecWrapperLock(*RWLockWrapper::CreateRWLock()),
      _lastEncodedTimestamp(0),
      _lastTimestamp(0xD87F3F9F),
      _isAudioBuffFresh(true),
      _uniqueID(0) {
  // Initialize VAD vector.
  for (int i = 0; i < MAX_FRAME_SIZE_10MSEC; i++) {
    _vadLabel[i] = 0;
  }

  // Nullify memory for encoder and decoder, and set payload type to an
  // invalid value.
  memset(&_encoderParams, 0, sizeof(WebRtcACMCodecParams));
  _encoderParams.codecInstant.pltype = -1;
  memset(&_decoderParams, 0, sizeof(WebRtcACMCodecParams));
  _decoderParams.codecInstant.pltype = -1;
}

ACMGenericCodec::~ACMGenericCodec() {
  // Check all the members which are pointers, and if they are not NULL
  // delete/free them.
  if (_ptrVADInst != NULL) {
    WebRtcVad_Free(_ptrVADInst);
    _ptrVADInst = NULL;
  }
  if (_inAudio != NULL) {
    delete[] _inAudio;
    _inAudio = NULL;
  }
  if (_inTimestamp != NULL) {
    delete[] _inTimestamp;
    _inTimestamp = NULL;
  }
  if (_ptrDTXInst != NULL) {
    WebRtcCng_FreeEnc(_ptrDTXInst);
    _ptrDTXInst = NULL;
  }
  delete &_codecWrapperLock;
}

int32_t ACMGenericCodec::Add10MsData(const uint32_t timestamp,
                                     const int16_t* data,
                                     const uint16_t lengthSmpl,
                                     const uint8_t audioChannel) {
  WriteLockScoped wl(_codecWrapperLock);
  return Add10MsDataSafe(timestamp, data, lengthSmpl, audioChannel);
}

int32_t ACMGenericCodec::Add10MsDataSafe(const uint32_t timestamp,
                                         const int16_t* data,
                                         const uint16_t lengthSmpl,
                                         const uint8_t audioChannel) {
  // The codec expects to get data in correct sampling rate. Get the sampling
  // frequency of the codec.
  uint16_t plFreqHz;
  if (EncoderSampFreq(plFreqHz) < 0) {
    // _codecID is not correct, perhaps the codec is not initialized yet.
    return -1;
  }

  // Sanity check to make sure the length of the input corresponds to 10 ms.
  if ((plFreqHz / 100) != lengthSmpl) {
    // This is not 10 ms of audio, given the sampling frequency of the codec.
    return -1;
  }

  if (_lastTimestamp == timestamp) {
    // Same timestamp as the last time, overwrite.
    if ((_inAudioIxWrite >= lengthSmpl * audioChannel) &&
        (_inTimestampIxWrite > 0)) {
      _inAudioIxWrite -= lengthSmpl * audioChannel;
      _inTimestampIxWrite--;
      WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _uniqueID,
          "Adding 10ms with previous timestamp, overwriting the previous 10ms");
    } else {
      WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _uniqueID,
                   "Adding 10ms with previous timestamp, this will sound bad");
    }
  }

  _lastTimestamp = timestamp;

  // If the data exceeds the buffer size, we through away the oldest data and
  // add the newly received 10 msec at the end.
  if ((_inAudioIxWrite + lengthSmpl * audioChannel) > AUDIO_BUFFER_SIZE_W16) {
    // Get the number of samples to be overwritten.
    int16_t missedSamples = _inAudioIxWrite + lengthSmpl * audioChannel -
        AUDIO_BUFFER_SIZE_W16;

    // Move the data (overwrite the old data).
    memmove(_inAudio, _inAudio + missedSamples,
            (AUDIO_BUFFER_SIZE_W16 - lengthSmpl * audioChannel) *
            sizeof(int16_t));

    // Copy the new data.
    memcpy(_inAudio + (AUDIO_BUFFER_SIZE_W16 - lengthSmpl * audioChannel), data,
           lengthSmpl * audioChannel * sizeof(int16_t));

    // Get the number of 10 ms blocks which are overwritten.
    int16_t missed10MsecBlocks =static_cast<int16_t>(
        (missedSamples / audioChannel * 100) / plFreqHz);

    // Move the timestamps.
    memmove(_inTimestamp, _inTimestamp + missed10MsecBlocks,
            (_inTimestampIxWrite - missed10MsecBlocks) * sizeof(uint32_t));
    _inTimestampIxWrite -= missed10MsecBlocks;
    _inTimestamp[_inTimestampIxWrite] = timestamp;
    _inTimestampIxWrite++;

    // Buffer is full.
    _inAudioIxWrite = AUDIO_BUFFER_SIZE_W16;
    IncreaseNoMissedSamples(missedSamples);
    _isAudioBuffFresh = false;
    return -missedSamples;
  }

  // Store the input data in our data buffer.
  memcpy(_inAudio + _inAudioIxWrite, data,
         lengthSmpl * audioChannel * sizeof(int16_t));
  _inAudioIxWrite += lengthSmpl * audioChannel;

  assert(_inTimestampIxWrite < TIMESTAMP_BUFFER_SIZE_W32);
  assert(_inTimestampIxWrite >= 0);

  _inTimestamp[_inTimestampIxWrite] = timestamp;
  _inTimestampIxWrite++;
  _isAudioBuffFresh = false;
  return 0;
}

int16_t ACMGenericCodec::Encode(uint8_t* bitStream,
                                int16_t* bitStreamLenByte,
                                uint32_t* timeStamp,
                                WebRtcACMEncodingType* encodingType) {
  WriteLockScoped lockCodec(_codecWrapperLock);
  ReadLockScoped lockNetEq(*_netEqDecodeLock);
  return EncodeSafe(bitStream, bitStreamLenByte, timeStamp, encodingType);
}

int16_t ACMGenericCodec::EncodeSafe(uint8_t* bitStream,
                                    int16_t* bitStreamLenByte,
                                    uint32_t* timeStamp,
                                    WebRtcACMEncodingType* encodingType) {
  // Only encode if we have enough data to encode. If not wait until we have a
  // full frame to encode.
  if (_inAudioIxWrite < _frameLenSmpl * _noChannels) {
    // There is not enough audio.
    *timeStamp = 0;
    *bitStreamLenByte = 0;
    // Doesn't really matter what this parameter set to.
    *encodingType = kNoEncoding;
    return 0;
  }

  // Not all codecs accept the whole frame to be pushed into encoder at once.
  // Some codecs needs to be feed with a specific number of samples different
  // from the frame size. If this is the case, |myBasicCodingBlockSmpl| will
  // report a number different from 0, and we will loop over calls to encoder
  // further down, until we have encode a complete frame.
  const int16_t myBasicCodingBlockSmpl = ACMCodecDB::BasicCodingBlock(_codecID);
  if (myBasicCodingBlockSmpl < 0 || !_encoderInitialized || !_encoderExist) {
    // This should not happen, but in case it does, report no encoding done.
    *timeStamp = 0;
    *bitStreamLenByte = 0;
    *encodingType = kNoEncoding;
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "EncodeSafe: error, basic coding sample block is negative");
    return -1;
  }

  // This makes the internal encoder read from the beginning of the buffer.
  _inAudioIxRead = 0;
  *timeStamp = _inTimestamp[0];

  // Process the audio through VAD. The function will set |_vadLabels|.
  // If VAD is disabled all entries in |_vadLabels| are set to ONE (active).
  int16_t status = 0;
  int16_t dtxProcessedSamples = 0;
  status = ProcessFrameVADDTX(bitStream, bitStreamLenByte,
                              &dtxProcessedSamples);
  if (status < 0) {
    *timeStamp = 0;
    *bitStreamLenByte = 0;
    *encodingType = kNoEncoding;
  } else {
    if (dtxProcessedSamples > 0) {
      // Dtx have processed some samples, and even if a bit-stream is generated
      // we should not do any encoding (normally there won't be enough data).

      // Setting the following makes sure that the move of audio data and
      // timestamps done correctly.
      _inAudioIxRead = dtxProcessedSamples;
      // This will let the owner of ACMGenericCodec to know that the
      // generated bit-stream is DTX to use correct payload type.
      uint16_t sampFreqHz;
      EncoderSampFreq(sampFreqHz);
      if (sampFreqHz == 8000) {
        *encodingType = kPassiveDTXNB;
      } else if (sampFreqHz == 16000) {
        *encodingType = kPassiveDTXWB;
      } else if (sampFreqHz == 32000) {
        *encodingType = kPassiveDTXSWB;
      } else if (sampFreqHz == 48000) {
        *encodingType = kPassiveDTXFB;
      } else {
        status = -1;
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                     "EncodeSafe: Wrong sampling frequency for DTX.");
      }

      // Transport empty frame if we have an empty bitstream.
      if ((*bitStreamLenByte == 0) &&
          (_sentCNPrevious || ((_inAudioIxWrite - _inAudioIxRead) <= 0))) {
        // Makes sure we transmit an empty frame.
        *bitStreamLenByte = 1;
        *encodingType = kNoEncoding;
      }
      _sentCNPrevious = true;
    } else {
      // We should encode the audio frame. Either VAD and/or DTX is off, or the
      // audio was considered "active".

      _sentCNPrevious = false;
      if (myBasicCodingBlockSmpl == 0) {
        // This codec can handle all allowed frame sizes as basic coding block.
        status = InternalEncode(bitStream, bitStreamLenByte);
        if (status < 0) {
          // TODO(tlegrand): Maybe reseting the encoder to be fresh for the next
          // frame.
          WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding,
                       _uniqueID, "EncodeSafe: error in internalEncode");
          *bitStreamLenByte = 0;
          *encodingType = kNoEncoding;
        }
      } else {
        // A basic-coding-block for this codec is defined so we loop over the
        // audio with the steps of the basic-coding-block.
        int16_t tmpBitStreamLenByte;

        // Reset the variables which will be incremented in the loop.
        *bitStreamLenByte = 0;
        bool done = false;
        while (!done) {
          status = InternalEncode(&bitStream[*bitStreamLenByte],
                                  &tmpBitStreamLenByte);
          *bitStreamLenByte += tmpBitStreamLenByte;

          // Guard Against errors and too large payloads.
          if ((status < 0) || (*bitStreamLenByte > MAX_PAYLOAD_SIZE_BYTE)) {
            // Error has happened, and even if we are in the middle of a full
            // frame we have to exit. Before exiting, whatever bits are in the
            // buffer are probably corrupted, so we ignore them.
            *bitStreamLenByte = 0;
            *encodingType = kNoEncoding;
            // We might have come here because of the second condition.
            status = -1;
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding,
                         _uniqueID, "EncodeSafe: error in InternalEncode");
            // break from the loop
            break;
          }

          // TODO(andrew): This should be multiplied by the number of
          //               channels, right?
          // http://code.google.com/p/webrtc/issues/detail?id=714
          done = _inAudioIxRead >= _frameLenSmpl;
        }
      }
      if (status >= 0) {
        *encodingType = (_vadLabel[0] == 1) ? kActiveNormalEncoded :
            kPassiveNormalEncoded;
        // Transport empty frame if we have an empty bitstream.
        if ((*bitStreamLenByte == 0) &&
            ((_inAudioIxWrite - _inAudioIxRead) <= 0)) {
          // Makes sure we transmit an empty frame.
          *bitStreamLenByte = 1;
          *encodingType = kNoEncoding;
        }
      }
    }
  }

  // Move the timestamp buffer according to the number of 10 ms blocks
  // which are read.
  uint16_t sampFreqHz;
  EncoderSampFreq(sampFreqHz);
  int16_t num10MsecBlocks = static_cast<int16_t>(
      (_inAudioIxRead / _noChannels * 100) / sampFreqHz);
  if (_inTimestampIxWrite > num10MsecBlocks) {
    memmove(_inTimestamp, _inTimestamp + num10MsecBlocks,
            (_inTimestampIxWrite - num10MsecBlocks) * sizeof(int32_t));
  }
  _inTimestampIxWrite -= num10MsecBlocks;

  // Remove encoded audio and move next audio to be encoded to the beginning
  // of the buffer. Accordingly, adjust the read and write indices.
  if (_inAudioIxRead < _inAudioIxWrite) {
    memmove(_inAudio, &_inAudio[_inAudioIxRead],
            (_inAudioIxWrite - _inAudioIxRead) * sizeof(int16_t));
  }
  _inAudioIxWrite -= _inAudioIxRead;
  _inAudioIxRead = 0;
  _lastEncodedTimestamp = *timeStamp;
  return (status < 0) ? (-1) : (*bitStreamLenByte);
}

int16_t ACMGenericCodec::Decode(uint8_t* bitStream,
                                int16_t bitStreamLenByte,
                                int16_t* audio,
                                int16_t* audioSamples,
                                int8_t* speechType) {
  WriteLockScoped wl(_codecWrapperLock);
  return DecodeSafe(bitStream, bitStreamLenByte, audio, audioSamples,
                    speechType);
}

bool ACMGenericCodec::EncoderInitialized() {
  ReadLockScoped rl(_codecWrapperLock);
  return _encoderInitialized;
}

bool ACMGenericCodec::DecoderInitialized() {
  ReadLockScoped rl(_codecWrapperLock);
  return _decoderInitialized;
}

int32_t ACMGenericCodec::RegisterInNetEq(ACMNetEQ* netEq,
                                         const CodecInst& codecInst) {
  WebRtcNetEQ_CodecDef codecDef;
  WriteLockScoped wl(_codecWrapperLock);

  if (CodecDef(codecDef, codecInst) < 0) {
    // Failed to register the decoder.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "RegisterInNetEq: error, failed to register");
    _registeredInNetEq = false;
    return -1;
  } else {
    if (netEq->AddCodec(&codecDef, _isMaster) < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "RegisterInNetEq: error, failed to add codec");
      _registeredInNetEq = false;
      return -1;
    }
    // Succeeded registering the decoder.
    _registeredInNetEq = true;
    return 0;
  }
}

int16_t ACMGenericCodec::EncoderParams(WebRtcACMCodecParams* encParams) {
  ReadLockScoped rl(_codecWrapperLock);
  return EncoderParamsSafe(encParams);
}

int16_t ACMGenericCodec::EncoderParamsSafe(WebRtcACMCodecParams* encParams) {
  // Codec parameters are valid only if the encoder is initialized.
  if (_encoderInitialized) {
    int32_t currentRate;
    memcpy(encParams, &_encoderParams, sizeof(WebRtcACMCodecParams));
    currentRate = encParams->codecInstant.rate;
    CurrentRate(currentRate);
    encParams->codecInstant.rate = currentRate;
    return 0;
  } else {
    encParams->codecInstant.plname[0] = '\0';
    encParams->codecInstant.pltype = -1;
    encParams->codecInstant.pacsize = 0;
    encParams->codecInstant.rate = 0;
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "EncoderParamsSafe: error, encoder not initialized");
    return -1;
  }
}

bool ACMGenericCodec::DecoderParams(WebRtcACMCodecParams* decParams,
                                    const uint8_t payloadType) {
  ReadLockScoped rl(_codecWrapperLock);
  return DecoderParamsSafe(decParams, payloadType);
}

bool ACMGenericCodec::DecoderParamsSafe(WebRtcACMCodecParams* decParams,
                                        const uint8_t payloadType) {
  // Decoder parameters are valid only if decoder is initialized.
  if (_decoderInitialized) {
    if (payloadType == _decoderParams.codecInstant.pltype) {
      memcpy(decParams, &_decoderParams, sizeof(WebRtcACMCodecParams));
      return true;
    }
  }

  decParams->codecInstant.plname[0] = '\0';
  decParams->codecInstant.pltype = -1;
  decParams->codecInstant.pacsize = 0;
  decParams->codecInstant.rate = 0;
  return false;
}

int16_t ACMGenericCodec::ResetEncoder() {
  WriteLockScoped lockCodec(_codecWrapperLock);
  ReadLockScoped lockNetEq(*_netEqDecodeLock);
  return ResetEncoderSafe();
}

int16_t ACMGenericCodec::ResetEncoderSafe() {
  if (!_encoderExist || !_encoderInitialized) {
    // We don't reset if encoder doesn't exists or isn't initialized yet.
    return 0;
  }

  _inAudioIxWrite = 0;
  _inAudioIxRead = 0;
  _inTimestampIxWrite = 0;
  _noMissedSamples = 0;
  _isAudioBuffFresh = true;
  memset(_inAudio, 0, AUDIO_BUFFER_SIZE_W16 * sizeof(int16_t));
  memset(_inTimestamp, 0, TIMESTAMP_BUFFER_SIZE_W32 * sizeof(int32_t));

  // Store DTX/VAD parameters.
  bool enableVAD = _vadEnabled;
  bool enableDTX = _dtxEnabled;
  ACMVADMode mode = _vadMode;

  // Reset the encoder.
  if (InternalResetEncoder() < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "ResetEncoderSafe: error in reset encoder");
    return -1;
  }

  // Disable DTX & VAD to delete the states and have a fresh start.
  DisableDTX();
  DisableVAD();

  // Set DTX/VAD.
  return SetVADSafe(enableDTX, enableVAD, mode);
}

int16_t ACMGenericCodec::InternalResetEncoder() {
  // Call the codecs internal encoder initialization/reset function.
  return InternalInitEncoder(&_encoderParams);
}

int16_t ACMGenericCodec::InitEncoder(WebRtcACMCodecParams* codecParams,
                                     bool forceInitialization) {
  WriteLockScoped lockCodec(_codecWrapperLock);
  ReadLockScoped lockNetEq(*_netEqDecodeLock);
  return InitEncoderSafe(codecParams, forceInitialization);
}

int16_t ACMGenericCodec::InitEncoderSafe(WebRtcACMCodecParams* codecParams,
                                         bool forceInitialization) {
  // Check if we got a valid set of parameters.
  int mirrorID;
  int codecNumber = ACMCodecDB::CodecNumber(&(codecParams->codecInstant),
                                            &mirrorID);
  if (codecNumber < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "InitEncoderSafe: error, codec number negative");
    return -1;
  }
  // Check if the parameters are for this codec.
  if ((_codecID >= 0) && (_codecID != codecNumber) && (_codecID != mirrorID)) {
    // The current codec is not the same as the one given by codecParams.
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "InitEncoderSafe: current codec is not the same as the one given by "
        "codecParams");
    return -1;
  }

  if (!CanChangeEncodingParam(codecParams->codecInstant)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "InitEncoderSafe: cannot change encoding parameters");
    return -1;
  }

  if (_encoderInitialized && !forceInitialization) {
    // The encoder is already initialized, and we don't want to force
    // initialization.
    return 0;
  }
  int16_t status;
  if (!_encoderExist) {
    // New encoder, start with creating.
    _encoderInitialized = false;
    status = CreateEncoder();
    if (status < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "InitEncoderSafe: cannot create encoder");
      return -1;
    } else {
      _encoderExist = true;
    }
  }
  _frameLenSmpl = (codecParams->codecInstant).pacsize;
  _noChannels = codecParams->codecInstant.channels;
  status = InternalInitEncoder(codecParams);
  if (status < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "InitEncoderSafe: error in init encoder");
    _encoderInitialized = false;
    return -1;
  } else {
    // Store encoder parameters.
    memcpy(&_encoderParams, codecParams, sizeof(WebRtcACMCodecParams));
    _encoderInitialized = true;
    if (_inAudio == NULL) {
      _inAudio = new int16_t[AUDIO_BUFFER_SIZE_W16];
      if (_inAudio == NULL) {
        return -1;
      }
      memset(_inAudio, 0, AUDIO_BUFFER_SIZE_W16 * sizeof(int16_t));
    }
    if (_inTimestamp == NULL) {
      _inTimestamp = new uint32_t[TIMESTAMP_BUFFER_SIZE_W32];
      if (_inTimestamp == NULL) {
        return -1;
      }
      memset(_inTimestamp, 0, sizeof(uint32_t) * TIMESTAMP_BUFFER_SIZE_W32);
    }
    _isAudioBuffFresh = true;
  }
  status = SetVADSafe(codecParams->enableDTX, codecParams->enableVAD,
                      codecParams->vadMode);

  return status;
}

// TODO(tlegrand): Remove the function CanChangeEncodingParam. Returns true
// for all codecs.
bool ACMGenericCodec::CanChangeEncodingParam(CodecInst& /*codecInst*/) {
  return true;
}

int16_t ACMGenericCodec::InitDecoder(WebRtcACMCodecParams* codecParams,
                                     bool forceInitialization) {
  WriteLockScoped lockCodc(_codecWrapperLock);
  WriteLockScoped lockNetEq(*_netEqDecodeLock);
  return InitDecoderSafe(codecParams, forceInitialization);
}

int16_t ACMGenericCodec::InitDecoderSafe(WebRtcACMCodecParams* codecParams,
                                         bool forceInitialization) {
  int mirrorID;
  // Check if we got a valid set of parameters.
  int codecNumber = ACMCodecDB::ReceiverCodecNumber(&codecParams->codecInstant,
                                                    &mirrorID);
  if (codecNumber < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "InitDecoderSafe: error, invalid codec number");
    return -1;
  }
  // Check if the parameters are for this codec.
  if ((_codecID >= 0) && (_codecID != codecNumber) && (_codecID != mirrorID)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
        "InitDecoderSafe: current codec is not the same as the one given "
        "by codecParams");
    // The current codec is not the same as the one given by codecParams.
    return -1;
  }

  if (_decoderInitialized && !forceInitialization) {
    // The decoder is already initialized, and we don't want to force
    // initialization.
    return 0;
  }

  int16_t status;
  if (!_decoderExist) {
    // New decoder, start with creating.
    _decoderInitialized = false;
    status = CreateDecoder();
    if (status < 0) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "InitDecoderSafe: cannot create decoder");
      return -1;
    } else {
      _decoderExist = true;
    }
  }

  status = InternalInitDecoder(codecParams);
  if (status < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "InitDecoderSafe: cannot init decoder");
    _decoderInitialized = false;
    return -1;
  } else {
    // Store decoder parameters.
    SaveDecoderParamSafe(codecParams);
    _decoderInitialized = true;
  }
  return 0;
}

int16_t ACMGenericCodec::ResetDecoder(int16_t payloadType) {
  WriteLockScoped lockCodec(_codecWrapperLock);
  WriteLockScoped lockNetEq(*_netEqDecodeLock);
  return ResetDecoderSafe(payloadType);
}

int16_t ACMGenericCodec::ResetDecoderSafe(int16_t payloadType) {
  WebRtcACMCodecParams decoderParams;
  if (!_decoderExist || !_decoderInitialized) {
    return 0;
  }
  // Initialization of the decoder should work for all the codec. For codecs
  // that needs to keep some states an overloading implementation of
  // |DecoderParamsSafe| exists.
  DecoderParamsSafe(&decoderParams, static_cast<uint8_t>(payloadType));
  return InternalInitDecoder(&decoderParams);
}

void ACMGenericCodec::ResetNoMissedSamples() {
  WriteLockScoped cs(_codecWrapperLock);
  _noMissedSamples = 0;
}

void ACMGenericCodec::IncreaseNoMissedSamples(const int16_t noSamples) {
  _noMissedSamples += noSamples;
}

// Get the number of missed samples, this can be public.
uint32_t ACMGenericCodec::NoMissedSamples() const {
  ReadLockScoped cs(_codecWrapperLock);
  return _noMissedSamples;
}

void ACMGenericCodec::DestructEncoder() {
  WriteLockScoped wl(_codecWrapperLock);

  // Disable VAD and delete the instance.
  if (_ptrVADInst != NULL) {
    WebRtcVad_Free(_ptrVADInst);
    _ptrVADInst = NULL;
  }
  _vadEnabled = false;
  _vadMode = VADNormal;

  // Disable DTX and delete the instance.
  _dtxEnabled = false;
  if (_ptrDTXInst != NULL) {
    WebRtcCng_FreeEnc(_ptrDTXInst);
    _ptrDTXInst = NULL;
  }
  _numLPCParams = kNewCNGNumPLCParams;

  DestructEncoderSafe();
}

void ACMGenericCodec::DestructDecoder() {
  WriteLockScoped wl(_codecWrapperLock);
  _decoderParams.codecInstant.pltype = -1;
  DestructDecoderSafe();
}

int16_t ACMGenericCodec::SetBitRate(const int32_t bitRateBPS) {
  WriteLockScoped wl(_codecWrapperLock);
  return SetBitRateSafe(bitRateBPS);
}

int16_t ACMGenericCodec::SetBitRateSafe(const int32_t bitRateBPS) {
  // If the codec can change the bit-rate this function is overloaded.
  // Otherwise the only acceptable value is the one that is in the database.
  CodecInst codecParams;
  if (ACMCodecDB::Codec(_codecID, &codecParams) < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "SetBitRateSafe: error in ACMCodecDB::Codec");
    return -1;
  }
  if (codecParams.rate != bitRateBPS) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "SetBitRateSafe: rate value is not acceptable");
    return -1;
  } else {
    return 0;
  }
}

// iSAC specific functions:
int32_t ACMGenericCodec::GetEstimatedBandwidth() {
  WriteLockScoped wl(_codecWrapperLock);
  return GetEstimatedBandwidthSafe();
}

int32_t ACMGenericCodec::GetEstimatedBandwidthSafe() {
  // All codecs but iSAC will return -1.
  return -1;
}

int32_t ACMGenericCodec::SetEstimatedBandwidth(int32_t estimatedBandwidth) {
  WriteLockScoped wl(_codecWrapperLock);
  return SetEstimatedBandwidthSafe(estimatedBandwidth);
}

int32_t ACMGenericCodec::SetEstimatedBandwidthSafe(
    int32_t /*estimatedBandwidth*/) {
  // All codecs but iSAC will return -1.
  return -1;
}
// End of iSAC specific functions.

int32_t ACMGenericCodec::GetRedPayload(uint8_t* redPayload,
                                       int16_t* payloadBytes) {
  WriteLockScoped wl(_codecWrapperLock);
  return GetRedPayloadSafe(redPayload, payloadBytes);
}

int32_t ACMGenericCodec::GetRedPayloadSafe(uint8_t* /* redPayload */,
                                           int16_t* /* payloadBytes */) {
  return -1;  // Do nothing by default.
}

int16_t ACMGenericCodec::CreateEncoder() {
  int16_t status = 0;
  if (!_encoderExist) {
    status = InternalCreateEncoder();
    // We just created the codec and obviously it is not initialized.
    _encoderInitialized = false;
  }
  if (status < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "CreateEncoder: error in internal create encoder");
    _encoderExist = false;
  } else {
    _encoderExist = true;
  }
  return status;
}

int16_t ACMGenericCodec::CreateDecoder() {
  int16_t status = 0;
  if (!_decoderExist) {
    status = InternalCreateDecoder();
    // Decoder just created and obviously it is not initialized.
    _decoderInitialized = false;
  }
  if (status < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "CreateDecoder: error in internal create decoder");
    _decoderExist = false;
  } else {
    _decoderExist = true;
  }
  return status;
}

void ACMGenericCodec::DestructEncoderInst(void* ptrInst) {
  if (ptrInst != NULL) {
    WriteLockScoped lockCodec(_codecWrapperLock);
    ReadLockScoped lockNetEq(*_netEqDecodeLock);
    InternalDestructEncoderInst(ptrInst);
  }
}

// Get the current audio buffer including read and write states, and timestamps.
int16_t ACMGenericCodec::AudioBuffer(WebRtcACMAudioBuff& audioBuff) {
  ReadLockScoped cs(_codecWrapperLock);
  memcpy(audioBuff.inAudio, _inAudio,
         AUDIO_BUFFER_SIZE_W16 * sizeof(int16_t));
  audioBuff.inAudioIxRead = _inAudioIxRead;
  audioBuff.inAudioIxWrite = _inAudioIxWrite;
  memcpy(audioBuff.inTimestamp, _inTimestamp,
         TIMESTAMP_BUFFER_SIZE_W32 * sizeof(uint32_t));
  audioBuff.inTimestampIxWrite = _inTimestampIxWrite;
  audioBuff.lastTimestamp = _lastTimestamp;
  return 0;
}

// Set the audio buffer.
int16_t ACMGenericCodec::SetAudioBuffer(WebRtcACMAudioBuff& audioBuff) {
  WriteLockScoped cs(_codecWrapperLock);
  memcpy(_inAudio, audioBuff.inAudio,
         AUDIO_BUFFER_SIZE_W16 * sizeof(int16_t));
  _inAudioIxRead = audioBuff.inAudioIxRead;
  _inAudioIxWrite = audioBuff.inAudioIxWrite;
  memcpy(_inTimestamp, audioBuff.inTimestamp,
         TIMESTAMP_BUFFER_SIZE_W32 * sizeof(uint32_t));
  _inTimestampIxWrite = audioBuff.inTimestampIxWrite;
  _lastTimestamp = audioBuff.lastTimestamp;
  _isAudioBuffFresh = false;
  return 0;
}

uint32_t ACMGenericCodec::LastEncodedTimestamp() const {
  ReadLockScoped cs(_codecWrapperLock);
  return _lastEncodedTimestamp;
}

uint32_t ACMGenericCodec::EarliestTimestamp() const {
  ReadLockScoped cs(_codecWrapperLock);
  return _inTimestamp[0];
}

int16_t ACMGenericCodec::SetVAD(const bool enableDTX, const bool enableVAD,
                                const ACMVADMode mode) {
  WriteLockScoped cs(_codecWrapperLock);
  return SetVADSafe(enableDTX, enableVAD, mode);
}

int16_t ACMGenericCodec::SetVADSafe(const bool enableDTX, const bool enableVAD,
                                    const ACMVADMode mode) {
  if (enableDTX) {
    // Make G729 AnnexB a special case.
    if (!STR_CASE_CMP(_encoderParams.codecInstant.plname, "G729")
        && !_hasInternalDTX) {
      if (ACMGenericCodec::EnableDTX() < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                     "SetVADSafe: error in enable DTX");
        return -1;
      }
    } else {
      if (EnableDTX() < 0) {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                     "SetVADSafe: error in enable DTX");
        return -1;
      }
    }

    if (_hasInternalDTX) {
      // Codec has internal DTX, practically we don't need WebRtc VAD, however,
      // we let the user to turn it on if they need call-backs on silence.
      // Store VAD mode for future even if VAD is off.
      _vadMode = mode;
      return (enableVAD) ? EnableVAD(mode) : DisableVAD();
    } else {
      // Codec does not have internal DTX so enabling DTX requires an active
      // VAD. 'enableDTX == true' overwrites VAD status.
      if (EnableVAD(mode) < 0) {
        // If we cannot create VAD we have to disable DTX.
        if (!_vadEnabled) {
          DisableDTX();
        }
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                     "SetVADSafe: error in enable VAD");
        return -1;
      }

      // Return '1', to let the caller know VAD was turned on, even if the
      // function was called with VAD='false'.
      if (enableVAD == false) {
        return 1;
      } else {
        return 0;
      }
    }
  } else {
    // Make G729 AnnexB a special case.
    if (!STR_CASE_CMP(_encoderParams.codecInstant.plname, "G729")
        && !_hasInternalDTX) {
      ACMGenericCodec::DisableDTX();
    } else {
      DisableDTX();
    }
    return (enableVAD) ? EnableVAD(mode) : DisableVAD();
  }
}

int16_t ACMGenericCodec::EnableDTX() {
  if (_hasInternalDTX) {
    // We should not be here if we have internal DTX this function should be
    // overloaded by the derived class in this case.
    return -1;
  }
  if (!_dtxEnabled) {
    if (WebRtcCng_CreateEnc(&_ptrDTXInst) < 0) {
      _ptrDTXInst = NULL;
      return -1;
    }
    uint16_t freqHz;
    EncoderSampFreq(freqHz);
    if (WebRtcCng_InitEnc(_ptrDTXInst, freqHz, kAcmSidIntervalMsec,
                          _numLPCParams) < 0) {
      // Couldn't initialize, has to return -1, and free the memory.
      WebRtcCng_FreeEnc(_ptrDTXInst);
      _ptrDTXInst = NULL;
      return -1;
    }
    _dtxEnabled = true;
  }
  return 0;
}

int16_t ACMGenericCodec::DisableDTX() {
  if (_hasInternalDTX) {
    // We should not be here if we have internal DTX this function should be
    // overloaded by the derived class in this case.
    return -1;
  }
  if (_ptrDTXInst != NULL) {
    WebRtcCng_FreeEnc(_ptrDTXInst);
    _ptrDTXInst = NULL;
  }
  _dtxEnabled = false;
  return 0;
}

int16_t ACMGenericCodec::EnableVAD(ACMVADMode mode) {
  if ((mode < VADNormal) || (mode > VADVeryAggr)) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "EnableVAD: error in VAD mode range");
    return -1;
  }

  if (!_vadEnabled) {
    if (WebRtcVad_Create(&_ptrVADInst) < 0) {
      _ptrVADInst = NULL;
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "EnableVAD: error in create VAD");
      return -1;
    }
    if (WebRtcVad_Init(_ptrVADInst) < 0) {
      WebRtcVad_Free(_ptrVADInst);
      _ptrVADInst = NULL;
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                   "EnableVAD: error in init VAD");
      return -1;
    }
  }

  // Set the VAD mode to the given value.
  if (WebRtcVad_set_mode(_ptrVADInst, mode) < 0) {
    // We failed to set the mode and we have to return -1. If we already have a
    // working VAD (_vadEnabled == true) then we leave it to work. Otherwise,
    // the following will be executed.
    if (!_vadEnabled) {
      // We just created the instance but cannot set the mode we have to free
      // the memory.
      WebRtcVad_Free(_ptrVADInst);
      _ptrVADInst = NULL;
    }
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceAudioCoding, _uniqueID,
                 "EnableVAD: failed to set the VAD mode");
    return -1;
  }
  _vadMode = mode;
  _vadEnabled = true;
  return 0;
}

int16_t ACMGenericCodec::DisableVAD() {
  if (_ptrVADInst != NULL) {
    WebRtcVad_Free(_ptrVADInst);
    _ptrVADInst = NULL;
  }
  _vadEnabled = false;
  return 0;
}

int32_t ACMGenericCodec::ReplaceInternalDTX(const bool replaceInternalDTX) {
  WriteLockScoped cs(_codecWrapperLock);
  return ReplaceInternalDTXSafe(replaceInternalDTX);
}

int32_t ACMGenericCodec::ReplaceInternalDTXSafe(
    const bool /* replaceInternalDTX */) {
  return -1;
}

int32_t ACMGenericCodec::IsInternalDTXReplaced(bool* internalDTXReplaced) {
  WriteLockScoped cs(_codecWrapperLock);
  return IsInternalDTXReplacedSafe(internalDTXReplaced);
}

int32_t ACMGenericCodec::IsInternalDTXReplacedSafe(bool* internalDTXReplaced) {
  *internalDTXReplaced = false;
  return 0;
}

int16_t ACMGenericCodec::ProcessFrameVADDTX(uint8_t* bitStream,
                                            int16_t* bitStreamLenByte,
                                            int16_t* samplesProcessed) {
  if (!_vadEnabled) {
    // VAD not enabled, set all vadLable[] to 1 (speech detected).
    for (int n = 0; n < MAX_FRAME_SIZE_10MSEC; n++) {
      _vadLabel[n] = 1;
    }
    *samplesProcessed = 0;
    return 0;
  }

  uint16_t freqHz;
  EncoderSampFreq(freqHz);

  // Calculate number of samples in 10 ms blocks, and number ms in one frame.
  int16_t samplesIn10Msec = static_cast<int16_t>(freqHz / 100);
  int32_t frameLenMsec = static_cast<int32_t>(_frameLenSmpl) * 1000 / freqHz;
  int16_t status;

  // Vector for storing maximum 30 ms of mono audio at 48 kHz.
  int16_t audio[1440];

  // Calculate number of VAD-blocks to process, and number of samples in each
  // block.
  int noSamplesToProcess[2];
  if (frameLenMsec == 40) {
    // 20 ms in each VAD block.
    noSamplesToProcess[0] = noSamplesToProcess[1] = 2 * samplesIn10Msec;
  } else {
    // For 10-30 ms framesizes, second VAD block will be size zero ms,
    // for 50 and 60 ms first VAD block will be 30 ms.
    noSamplesToProcess[0] =
        (frameLenMsec > 30) ? 3 * samplesIn10Msec : _frameLenSmpl;
    noSamplesToProcess[1] = _frameLenSmpl - noSamplesToProcess[0];
  }

  int offSet = 0;
  int loops = (noSamplesToProcess[1] > 0) ? 2 : 1;
  for (int i = 0; i < loops; i++) {
    // If stereo, calculate mean of the two channels.
    if (_noChannels == 2) {
      for (int j = 0; j < noSamplesToProcess[i]; j++) {
        audio[j] = (_inAudio[(offSet + j) * 2] +
            _inAudio[(offSet + j) * 2 + 1]) / 2;
      }
      offSet = noSamplesToProcess[0];
    } else {
      // Mono, copy data from _inAudio to continue work on.
      memcpy(audio, _inAudio, sizeof(int16_t) * noSamplesToProcess[i]);
    }

    // Call VAD.
    status = static_cast<int16_t>(WebRtcVad_Process(_ptrVADInst,
                                                    static_cast<int>(freqHz),
                                                    audio,
                                                    noSamplesToProcess[i]));
    _vadLabel[i] = status;

    if (status < 0) {
      // This will force that the data be removed from the buffer.
      *samplesProcessed += noSamplesToProcess[i];
      return -1;
    }

    // If VAD decision non-active, update DTX. NOTE! We only do this if the
    // first part of a frame gets the VAD decision "inactive". Otherwise DTX
    // might say it is time to transmit SID frame, but we will encode the whole
    // frame, because the first part is active.
    *samplesProcessed = 0;
    if ((status == 0) && (i == 0) && _dtxEnabled && !_hasInternalDTX) {
      int16_t bitStreamLen;
      int num10MsecFrames = noSamplesToProcess[i] / samplesIn10Msec;
      *bitStreamLenByte = 0;
      for (int n = 0; n < num10MsecFrames; n++) {
        // This block is (passive) && (vad enabled). If first CNG after
        // speech, force SID by setting last parameter to "1".
        status = WebRtcCng_Encode(_ptrDTXInst, &audio[n * samplesIn10Msec],
                                  samplesIn10Msec, bitStream, &bitStreamLen,
                                  !_prev_frame_cng);
        if (status < 0) {
          return -1;
        }

        // Update previous frame was CNG.
        _prev_frame_cng = 1;

        *samplesProcessed += samplesIn10Msec * _noChannels;

        // |bitStreamLen| will only be > 0 once per 100 ms.
        *bitStreamLenByte += bitStreamLen;
      }

      // Check if all samples got processed by the DTX.
      if (*samplesProcessed != noSamplesToProcess[i] * _noChannels) {
        // Set to zero since something went wrong. Shouldn't happen.
        *samplesProcessed = 0;
      }
    } else {
      // Update previous frame was not CNG.
      _prev_frame_cng = 0;
    }

    if (*samplesProcessed > 0) {
      // The block contains inactive speech, and is processed by DTX.
      // Discontinue running VAD.
      break;
    }
  }

  return status;
}

int16_t ACMGenericCodec::SamplesLeftToEncode() {
  ReadLockScoped rl(_codecWrapperLock);
  return (_frameLenSmpl <= _inAudioIxWrite) ? 0 :
      (_frameLenSmpl - _inAudioIxWrite);
}

void ACMGenericCodec::SetUniqueID(const uint32_t id) {
  _uniqueID = id;
}

bool ACMGenericCodec::IsAudioBufferFresh() const {
  ReadLockScoped rl(_codecWrapperLock);
  return _isAudioBuffFresh;
}

// This function is replaced by codec specific functions for some codecs.
int16_t ACMGenericCodec::EncoderSampFreq(uint16_t& sampFreqHz) {
  int32_t f;
  f = ACMCodecDB::CodecFreq(_codecID);
  if (f < 0) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
                 "EncoderSampFreq: codec frequency is negative");
    return -1;
  } else {
    sampFreqHz = static_cast<uint16_t>(f);
    return 0;
  }
}

int32_t ACMGenericCodec::ConfigISACBandwidthEstimator(
    const uint8_t /* initFrameSizeMsec */,
    const uint16_t /* initRateBitPerSec */,
    const bool /* enforceFrameSize  */) {
  WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _uniqueID,
      "The send-codec is not iSAC, failed to config iSAC bandwidth estimator.");
  return -1;
}

int32_t ACMGenericCodec::SetISACMaxRate(const uint32_t /* maxRateBitPerSec */) {
  WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _uniqueID,
               "The send-codec is not iSAC, failed to set iSAC max rate.");
  return -1;
}

int32_t ACMGenericCodec::SetISACMaxPayloadSize(
    const uint16_t /* maxPayloadLenBytes */) {
  WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceAudioCoding, _uniqueID,
      "The send-codec is not iSAC, failed to set iSAC max payload-size.");
  return -1;
}

void ACMGenericCodec::SaveDecoderParam(
    const WebRtcACMCodecParams* codecParams) {
  WriteLockScoped wl(_codecWrapperLock);
  SaveDecoderParamSafe(codecParams);
}

void ACMGenericCodec::SaveDecoderParamSafe(
    const WebRtcACMCodecParams* codecParams) {
  memcpy(&_decoderParams, codecParams, sizeof(WebRtcACMCodecParams));
}

int16_t ACMGenericCodec::UpdateEncoderSampFreq(
    uint16_t /* encoderSampFreqHz */) {
  WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
      "It is asked for a change in smapling frequency while the current "
      "send-codec supports only one sampling rate.");
  return -1;
}

void ACMGenericCodec::SetIsMaster(bool isMaster) {
  WriteLockScoped wl(_codecWrapperLock);
  _isMaster = isMaster;
}

int16_t ACMGenericCodec::REDPayloadISAC(const int32_t /* isacRate */,
                                        const int16_t /* isacBwEstimate */,
                                        uint8_t* /* payload */,
                                        int16_t* /* payloadLenBytes */) {
  WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceAudioCoding, _uniqueID,
               "Error: REDPayloadISAC is an iSAC specific function");
  return -1;
}

}  // namespace webrtc
