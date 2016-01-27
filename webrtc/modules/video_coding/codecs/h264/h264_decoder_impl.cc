/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#include "webrtc/modules/video_coding/codecs/h264/h264_decoder_impl.h"

#include <algorithm>
#include <limits>

extern "C" {
#include "third_party/ffmpeg/libavcodec/avcodec.h"
#include "third_party/ffmpeg/libavformat/avformat.h"
#include "third_party/ffmpeg/libavutil/imgutils.h"
}  // extern "C"

#include "webrtc/base/checks.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/keep_ref_until_done.h"
#include "webrtc/base/logging.h"

namespace webrtc {

namespace {

const AVPixelFormat kPixelFormat = AV_PIX_FMT_YUV420P;
const size_t kYPlaneIndex = 0;
const size_t kUPlaneIndex = 1;
const size_t kVPlaneIndex = 2;

#if !defined(WEBRTC_CHROMIUM_BUILD)

bool ffmpeg_initialized = false;

// Called by FFmpeg to do mutex operations if initialized using
// |InitializeFFmpeg|.
int LockManagerOperation(void** lock, AVLockOp op)
    EXCLUSIVE_LOCK_FUNCTION() UNLOCK_FUNCTION() {
  switch (op) {
    case AV_LOCK_CREATE:
      *lock = new rtc::CriticalSection();
      return 0;
    case AV_LOCK_OBTAIN:
      static_cast<rtc::CriticalSection*>(*lock)->Enter();
      return 0;
    case AV_LOCK_RELEASE:
      static_cast<rtc::CriticalSection*>(*lock)->Leave();
      return 0;
    case AV_LOCK_DESTROY:
      delete static_cast<rtc::CriticalSection*>(*lock);
      *lock = nullptr;
      return 0;
  }
  RTC_NOTREACHED() << "Unrecognized AVLockOp.";
  return -1;
}

// TODO(hbos): Assumed to be called on a single thread. Should DCHECK that
// InitializeFFmpeg is only called on one thread or make it thread safe.
// See https://bugs.chromium.org/p/webrtc/issues/detail?id=5427.
void InitializeFFmpeg() {
  if (!ffmpeg_initialized) {
    if (av_lockmgr_register(LockManagerOperation) < 0) {
      RTC_NOTREACHED() << "av_lockmgr_register failed.";
      return;
    }
    av_register_all();
    ffmpeg_initialized = true;
  }
}

#endif  // !defined(WEBRTC_CHROMIUM_BUILD)

// Called by FFmpeg when it is done with a frame buffer, see AVGetBuffer2.
void AVFreeBuffer2(void* opaque, uint8_t* data) {
  VideoFrame* video_frame = static_cast<VideoFrame*>(opaque);
  delete video_frame;
}

// Called by FFmpeg when it needs a frame buffer to store decoded frames in.
// The VideoFrames returned by FFmpeg at |Decode| originate from here. They are
// reference counted and freed by FFmpeg using |AVFreeBuffer2|.
// TODO(hbos): Use a frame pool for better performance instead of create/free.
// Could be owned by decoder, |static_cast<H264DecoderImpl*>(context->opaque)|.
// Consider verifying that the buffer was allocated by us to avoid unsafe type
// cast. See https://bugs.chromium.org/p/webrtc/issues/detail?id=5428.
int AVGetBuffer2(AVCodecContext* context, AVFrame* av_frame, int flags) {
  RTC_CHECK_EQ(context->pix_fmt, kPixelFormat);  // Same as in InitDecode.
  // Necessary capability to be allowed to provide our own buffers.
  RTC_CHECK(context->codec->capabilities | AV_CODEC_CAP_DR1);
  // |av_frame->width| and |av_frame->height| are set by FFmpeg. These are the
  // actual image's dimensions and may be different from |context->width| and
  // |context->coded_width| due to reordering.
  int width = av_frame->width;
  int height = av_frame->height;
  // See |lowres|, if used the decoder scales the image by 1/2^(lowres). This
  // has implications on which resolutions are valid, but we don't use it.
  RTC_CHECK_EQ(context->lowres, 0);
  // Adjust the |width| and |height| to values acceptable by the decoder.
  // Without this, FFmpeg may overflow the buffer. If modified, |width| and/or
  // |height| are larger than the actual image and the image has to be cropped
  // (top-left corner) after decoding to avoid visible borders to the right and
  // bottom of the actual image.
  avcodec_align_dimensions(context, &width, &height);

  RTC_CHECK_GE(width, 0);
  RTC_CHECK_GE(height, 0);
  int ret = av_image_check_size(static_cast<unsigned int>(width),
                                static_cast<unsigned int>(height), 0, nullptr);
  if (ret < 0) {
    LOG(LS_ERROR) << "Invalid picture size " << width << "x" << height;
    return ret;
  }

  // The video frame is stored in |video_frame|. |av_frame| is FFmpeg's version
  // of a video frame and will be set up to reference |video_frame|'s buffers.
  VideoFrame* video_frame = new VideoFrame();
  int stride_y = width;
  int stride_uv = (width + 1) / 2;
  RTC_CHECK_EQ(0, video_frame->CreateEmptyFrame(
      width, height, stride_y, stride_uv, stride_uv));
  int total_size = video_frame->allocated_size(kYPlane) +
                   video_frame->allocated_size(kUPlane) +
                   video_frame->allocated_size(kVPlane);
  RTC_DCHECK_EQ(total_size, stride_y * height +
                (stride_uv + stride_uv) * ((height + 1) / 2));

  // FFmpeg expects the initial allocation to be zero-initialized according to
  // http://crbug.com/390941.
  // Using a single |av_frame->buf| - YUV is required to be a continuous blob of
  // memory. We can zero-initialize with one memset operation for all planes.
  RTC_DCHECK_EQ(video_frame->buffer(kUPlane),
      video_frame->buffer(kYPlane) + video_frame->allocated_size(kYPlane));
  RTC_DCHECK_EQ(video_frame->buffer(kVPlane),
      video_frame->buffer(kUPlane) + video_frame->allocated_size(kUPlane));
  memset(video_frame->buffer(kYPlane), 0, total_size);

  av_frame->format = context->pix_fmt;
  av_frame->reordered_opaque = context->reordered_opaque;

  // Set |av_frame| members as required by FFmpeg.
  av_frame->data[kYPlaneIndex] = video_frame->buffer(kYPlane);
  av_frame->linesize[kYPlaneIndex] = video_frame->stride(kYPlane);
  av_frame->data[kUPlaneIndex] = video_frame->buffer(kUPlane);
  av_frame->linesize[kUPlaneIndex] = video_frame->stride(kUPlane);
  av_frame->data[kVPlaneIndex] = video_frame->buffer(kVPlane);
  av_frame->linesize[kVPlaneIndex] = video_frame->stride(kVPlane);
  RTC_DCHECK_EQ(av_frame->extended_data, av_frame->data);

  av_frame->buf[0] = av_buffer_create(av_frame->data[kYPlaneIndex],
                                      total_size,
                                      AVFreeBuffer2,
                                      static_cast<void*>(video_frame),
                                      0);
  RTC_CHECK(av_frame->buf[0]);
  return 0;
}

}  // namespace

H264DecoderImpl::H264DecoderImpl()
    : decoded_image_callback_(nullptr) {
}

H264DecoderImpl::~H264DecoderImpl() {
  Release();
}

int32_t H264DecoderImpl::InitDecode(const VideoCodec* codec_settings,
                                    int32_t number_of_cores) {
  if (codec_settings &&
      codec_settings->codecType != kVideoCodecH264) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // In Chromium FFmpeg will be initialized outside of WebRTC and we should not
  // attempt to do so ourselves or it will be initialized twice.
  // TODO(hbos): Put behind a different flag in case non-chromium project wants
  // to initialize externally.
  // See https://bugs.chromium.org/p/webrtc/issues/detail?id=5427.
#if !defined(WEBRTC_CHROMIUM_BUILD)
  // Make sure FFmpeg has been initialized.
  InitializeFFmpeg();
#endif

  // Release necessary in case of re-initializing.
  int32_t ret = Release();
  if (ret != WEBRTC_VIDEO_CODEC_OK)
    return ret;
  RTC_DCHECK(!av_context_);

  // Initialize AVCodecContext.
  av_context_.reset(avcodec_alloc_context3(nullptr));

  av_context_->codec_type = AVMEDIA_TYPE_VIDEO;
  av_context_->codec_id = AV_CODEC_ID_H264;
  if (codec_settings) {
    av_context_->coded_width = codec_settings->width;
    av_context_->coded_height = codec_settings->height;
  }
  av_context_->pix_fmt = kPixelFormat;
  av_context_->extradata = nullptr;
  av_context_->extradata_size = 0;

  av_context_->thread_count = 1;
  av_context_->thread_type = FF_THREAD_SLICE;

  // FFmpeg will get video buffers from our AVGetBuffer2, memory managed by us.
  av_context_->get_buffer2 = AVGetBuffer2;
  // get_buffer2 is called with the context, there |opaque| can be used to get a
  // pointer |this|.
  av_context_->opaque = this;
  // Use ref counted frames (av_frame_unref).
  av_context_->refcounted_frames = 1;  // true

  AVCodec* codec = avcodec_find_decoder(av_context_->codec_id);
  if (!codec) {
    // This is an indication that FFmpeg has not been initialized or it has not
    // been compiled/initialized with the correct set of codecs.
    LOG(LS_ERROR) << "FFmpeg H.264 decoder not found.";
    Release();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  int res = avcodec_open2(av_context_.get(), codec, nullptr);
  if (res < 0) {
    LOG(LS_ERROR) << "avcodec_open2 error: " << res;
    Release();
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  av_frame_.reset(av_frame_alloc());
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264DecoderImpl::Release() {
  av_context_.reset();
  av_frame_.reset();
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264DecoderImpl::Reset() {
  if (!IsInitialized())
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  InitDecode(nullptr, 1);
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264DecoderImpl::RegisterDecodeCompleteCallback(
    DecodedImageCallback* callback) {
  decoded_image_callback_ = callback;
  return WEBRTC_VIDEO_CODEC_OK;
}

int32_t H264DecoderImpl::Decode(const EncodedImage& input_image,
                                bool /*missing_frames*/,
                                const RTPFragmentationHeader* /*fragmentation*/,
                                const CodecSpecificInfo* codec_specific_info,
                                int64_t /*render_time_ms*/) {
  if (!IsInitialized())
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  if (!decoded_image_callback_) {
    LOG(LS_WARNING) << "InitDecode() has been called, but a callback function "
        "has not been set with RegisterDecodeCompleteCallback()";
    return WEBRTC_VIDEO_CODEC_UNINITIALIZED;
  }
  if (!input_image._buffer || !input_image._length)
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  if (codec_specific_info &&
      codec_specific_info->codecType != kVideoCodecH264) {
    return WEBRTC_VIDEO_CODEC_ERR_PARAMETER;
  }

  // FFmpeg requires padding due to some optimized bitstream readers reading 32
  // or 64 bits at once and could read over the end. See avcodec_decode_video2.
  RTC_CHECK_GE(input_image._size, input_image._length +
                   EncodedImage::GetBufferPaddingBytes(kVideoCodecH264));
  // "If the first 23 bits of the additional bytes are not 0, then damaged MPEG
  // bitstreams could cause overread and segfault." See
  // AV_INPUT_BUFFER_PADDING_SIZE. We'll zero the entire padding just in case.
  memset(input_image._buffer + input_image._length,
         0,
         EncodedImage::GetBufferPaddingBytes(kVideoCodecH264));

  AVPacket packet;
  av_init_packet(&packet);
  packet.data = input_image._buffer;
  if (input_image._length >
      static_cast<size_t>(std::numeric_limits<int>::max())) {
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  packet.size = static_cast<int>(input_image._length);
  av_context_->reordered_opaque = input_image.ntp_time_ms_ * 1000;  // ms -> μs

  int frame_decoded = 0;
  int result = avcodec_decode_video2(av_context_.get(),
                                     av_frame_.get(),
                                     &frame_decoded,
                                     &packet);
  if (result < 0) {
    LOG(LS_ERROR) << "avcodec_decode_video2 error: " << result;
    return WEBRTC_VIDEO_CODEC_ERROR;
  }
  // |result| is number of bytes used, which should be all of them.
  if (result != packet.size) {
    LOG(LS_ERROR) << "avcodec_decode_video2 consumed " << result << " bytes "
        "when " << packet.size << " bytes were expected.";
    return WEBRTC_VIDEO_CODEC_ERROR;
  }

  if (!frame_decoded) {
    LOG(LS_WARNING) << "avcodec_decode_video2 successful but no frame was "
        "decoded.";
    return WEBRTC_VIDEO_CODEC_OK;
  }

  // Obtain the |video_frame| containing the decoded image.
  VideoFrame* video_frame = static_cast<VideoFrame*>(
      av_buffer_get_opaque(av_frame_->buf[0]));
  RTC_DCHECK(video_frame);
  RTC_CHECK_EQ(av_frame_->data[kYPlane], video_frame->buffer(kYPlane));
  RTC_CHECK_EQ(av_frame_->data[kUPlane], video_frame->buffer(kUPlane));
  RTC_CHECK_EQ(av_frame_->data[kVPlane], video_frame->buffer(kVPlane));
  video_frame->set_timestamp(input_image._timeStamp);

  // The decoded image may be larger than what is supposed to be visible, see
  // |AVGetBuffer2|'s use of |avcodec_align_dimensions|. This crops the image
  // without copying the underlying buffer.
  rtc::scoped_refptr<VideoFrameBuffer> buf = video_frame->video_frame_buffer();
  if (av_frame_->width != buf->width() || av_frame_->height != buf->height()) {
    video_frame->set_video_frame_buffer(
        new rtc::RefCountedObject<WrappedI420Buffer>(
            av_frame_->width, av_frame_->height,
            buf->data(kYPlane), buf->stride(kYPlane),
            buf->data(kUPlane), buf->stride(kUPlane),
            buf->data(kVPlane), buf->stride(kVPlane),
            rtc::KeepRefUntilDone(buf)));
  }

  // Return decoded frame.
  int32_t ret = decoded_image_callback_->Decoded(*video_frame);
  // Stop referencing it, possibly freeing |video_frame|.
  av_frame_unref(av_frame_.get());
  video_frame = nullptr;

  if (ret) {
    LOG(LS_WARNING) << "DecodedImageCallback::Decoded returned " << ret;
    return ret;
  }
  return WEBRTC_VIDEO_CODEC_OK;
}

bool H264DecoderImpl::IsInitialized() const {
  return av_context_ != nullptr;
}

}  // namespace webrtc
