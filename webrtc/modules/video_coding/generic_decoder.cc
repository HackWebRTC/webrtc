/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_coding/generic_decoder.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/trace_event.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/internal_defines.h"
#include "webrtc/system_wrappers/include/clock.h"

namespace webrtc {

VCMDecodedFrameCallback::VCMDecodedFrameCallback(VCMTiming* timing,
                                                 Clock* clock)
    : _clock(clock),
      _timing(timing),
      _timestampMap(kDecoderFrameMemoryLength),
      _lastReceivedPictureID(0) {
  decoder_thread_.DetachFromThread();
}

VCMDecodedFrameCallback::~VCMDecodedFrameCallback() {
  RTC_DCHECK(construction_thread_.CalledOnValidThread());
}

void VCMDecodedFrameCallback::SetUserReceiveCallback(
    VCMReceiveCallback* receiveCallback) {
  RTC_DCHECK(construction_thread_.CalledOnValidThread());
  RTC_DCHECK((!_receiveCallback && receiveCallback) ||
             (_receiveCallback && !receiveCallback));
  _receiveCallback = receiveCallback;
}

VCMReceiveCallback* VCMDecodedFrameCallback::UserReceiveCallback() {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  // Called on the decode thread via VCMCodecDataBase::GetDecoder.
  // The callback must always have been set before this happens.
  RTC_DCHECK(_receiveCallback);
  return _receiveCallback;
}

int32_t VCMDecodedFrameCallback::Decoded(VideoFrame& decodedImage) {
  return Decoded(decodedImage, -1);
}

int32_t VCMDecodedFrameCallback::Decoded(VideoFrame& decodedImage,
                                         int64_t decode_time_ms) {
  Decoded(decodedImage,
          decode_time_ms >= 0 ? rtc::Optional<int32_t>(decode_time_ms)
                              : rtc::Optional<int32_t>(),
          rtc::Optional<uint8_t>());
  return WEBRTC_VIDEO_CODEC_OK;
}

void VCMDecodedFrameCallback::Decoded(VideoFrame& decodedImage,
                                      rtc::Optional<int32_t> decode_time_ms,
                                      rtc::Optional<uint8_t> qp) {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  RTC_DCHECK(_receiveCallback) << "Callback must not be null at this point";

  TRACE_EVENT_INSTANT1("webrtc", "VCMDecodedFrameCallback::Decoded",
                       "timestamp", decodedImage.timestamp());
  // TODO(holmer): We should improve this so that we can handle multiple
  // callbacks from one call to Decode().
  VCMFrameInformation* frameInfo = _timestampMap.Pop(decodedImage.timestamp());

  if (frameInfo == NULL) {
    LOG(LS_WARNING) << "Too many frames backed up in the decoder, dropping "
                       "this one.";
    return;
  }

  const int64_t now_ms = _clock->TimeInMilliseconds();
  if (!decode_time_ms) {
    decode_time_ms =
        rtc::Optional<int32_t>(now_ms - frameInfo->decodeStartTimeMs);
  }
  _timing->StopDecodeTimer(decodedImage.timestamp(), *decode_time_ms, now_ms,
                           frameInfo->renderTimeMs);

  decodedImage.set_timestamp_us(
      frameInfo->renderTimeMs * rtc::kNumMicrosecsPerMillisec);
  decodedImage.set_rotation(frameInfo->rotation);
  _receiveCallback->FrameToRender(decodedImage, qp);
}

int32_t VCMDecodedFrameCallback::ReceivedDecodedReferenceFrame(
    const uint64_t pictureId) {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  return _receiveCallback->ReceivedDecodedReferenceFrame(pictureId);
}

int32_t VCMDecodedFrameCallback::ReceivedDecodedFrame(
    const uint64_t pictureId) {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  _lastReceivedPictureID = pictureId;
  return 0;
}

uint64_t VCMDecodedFrameCallback::LastReceivedPictureID() const {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  return _lastReceivedPictureID;
}

void VCMDecodedFrameCallback::OnDecoderImplementationName(
    const char* implementation_name) {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  _receiveCallback->OnDecoderImplementationName(implementation_name);
}

void VCMDecodedFrameCallback::Map(uint32_t timestamp,
                                  VCMFrameInformation* frameInfo) {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  _timestampMap.Add(timestamp, frameInfo);
}

int32_t VCMDecodedFrameCallback::Pop(uint32_t timestamp) {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  return _timestampMap.Pop(timestamp) == nullptr ? VCM_GENERAL_ERROR : VCM_OK;
}

VCMGenericDecoder::VCMGenericDecoder(VideoDecoder* decoder, bool isExternal)
    : _callback(NULL),
      _frameInfos(),
      _nextFrameInfoIdx(0),
      decoder_(decoder),
      _codecType(kVideoCodecUnknown),
      _isExternal(isExternal) {}

VCMGenericDecoder::~VCMGenericDecoder() {
  decoder_->Release();
  if (_isExternal)
    decoder_.release();

  RTC_DCHECK(_isExternal || !decoder_);
}

int32_t VCMGenericDecoder::InitDecode(const VideoCodec* settings,
                                      int32_t numberOfCores) {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  TRACE_EVENT0("webrtc", "VCMGenericDecoder::InitDecode");
  _codecType = settings->codecType;

  return decoder_->InitDecode(settings, numberOfCores);
}

int32_t VCMGenericDecoder::Decode(const VCMEncodedFrame& frame, int64_t nowMs) {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  TRACE_EVENT2("webrtc", "VCMGenericDecoder::Decode", "timestamp",
               frame.EncodedImage()._timeStamp, "decoder",
               decoder_->ImplementationName());
  _frameInfos[_nextFrameInfoIdx].decodeStartTimeMs = nowMs;
  _frameInfos[_nextFrameInfoIdx].renderTimeMs = frame.RenderTimeMs();
  _frameInfos[_nextFrameInfoIdx].rotation = frame.rotation();
  _callback->Map(frame.TimeStamp(), &_frameInfos[_nextFrameInfoIdx]);

  _nextFrameInfoIdx = (_nextFrameInfoIdx + 1) % kDecoderFrameMemoryLength;
  const RTPFragmentationHeader dummy_header;
  int32_t ret = decoder_->Decode(frame.EncodedImage(), frame.MissingFrame(),
                                 &dummy_header, frame.CodecSpecific(),
                                 frame.RenderTimeMs());

  // TODO(tommi): Necessary every time?
  // Maybe this should be the first thing the function does, and only the first
  // time around?
  _callback->OnDecoderImplementationName(decoder_->ImplementationName());

  if (ret != WEBRTC_VIDEO_CODEC_OK) {
    if (ret < WEBRTC_VIDEO_CODEC_OK) {
      LOG(LS_WARNING) << "Failed to decode frame with timestamp "
                      << frame.TimeStamp() << ", error code: " << ret;
    }
    // We pop the frame for all non-'OK', failure or success codes such as
    // WEBRTC_VIDEO_CODEC_NO_OUTPUT and WEBRTC_VIDEO_CODEC_REQUEST_SLI.
    _callback->Pop(frame.TimeStamp());
  }

  return ret;
}

int32_t VCMGenericDecoder::RegisterDecodeCompleteCallback(
    VCMDecodedFrameCallback* callback) {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  _callback = callback;
  return decoder_->RegisterDecodeCompleteCallback(callback);
}

bool VCMGenericDecoder::PrefersLateDecoding() const {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  return decoder_->PrefersLateDecoding();
}

#if defined(WEBRTC_ANDROID)
void VCMGenericDecoder::PollDecodedFrames() {
  RTC_DCHECK_RUN_ON(&decoder_thread_);
  decoder_->PollDecodedFrames();
}
#endif

}  // namespace webrtc
