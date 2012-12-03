/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "gtest/gtest.h"
#include "webrtc/modules/video_coding/codecs/vp8/vp8_impl.h"
#include "webrtc/system_wrappers/interface/scoped_ptr.h"
#include "webrtc/modules/video_coding/codecs/test_framework/video_source.h"
#include "webrtc/system_wrappers/interface/tick_util.h"
#include "webrtc/test/testsupport/fileutils.h"

namespace webrtc {

enum { kMaxWaitEncTimeMs = 100 };
enum { kMaxWaitDecTimeMs = 25 };


// TODO(mikhal/stefan): Replace these with mocks.
class Vp8UnitTestEncodeCompleteCallback : public webrtc::EncodedImageCallback {
 public:
  Vp8UnitTestEncodeCompleteCallback(VideoFrame* frame,
                                    unsigned int decoderSpecificSize = 0,
                                    void* decoderSpecificInfo = NULL) :
                                      encoded_video_frame_(frame),
                                      encode_complete_(false) {}
  int Encoded(EncodedImage& encodedImage,
              const CodecSpecificInfo* codecSpecificInfo,
              const RTPFragmentationHeader*);
  bool EncodeComplete();
  // Note that this only makes sense if an encode has been completed
  VideoFrameType EncodedFrameType() const {return  encoded_frame_type_;}
 private:
  VideoFrame* encoded_video_frame_;
  bool encode_complete_;
  VideoFrameType encoded_frame_type_;
};

int Vp8UnitTestEncodeCompleteCallback::Encoded(EncodedImage& encodedImage,
    const CodecSpecificInfo* codecSpecificInfo,
    const RTPFragmentationHeader*
    fragmentation) {
  encoded_video_frame_->VerifyAndAllocate(encodedImage._size);
  encoded_video_frame_->CopyFrame(encodedImage._size, encodedImage._buffer);
  encoded_video_frame_->SetLength(encodedImage._length);
  // TODO(mikhal): Update frame type API.
  // encoded_video_frame_->SetFrameType(encodedImage._frameType);
  encoded_video_frame_->SetWidth(encodedImage._encodedWidth);
  encoded_video_frame_->SetHeight(encodedImage._encodedHeight);
  encoded_video_frame_->SetTimeStamp(encodedImage._timeStamp);
  encode_complete_ = true;
  encoded_frame_type_ = encodedImage._frameType;
  return 0;
}

bool Vp8UnitTestEncodeCompleteCallback::EncodeComplete() {
  if (encode_complete_) {
    encode_complete_ = false;
    return true;
  }
  return false;
}

class Vp8UnitTestDecodeCompleteCallback : public webrtc::DecodedImageCallback {
 public:
  explicit Vp8UnitTestDecodeCompleteCallback(I420VideoFrame* frame) :
      decoded_video_frame_(frame), decode_complete(false) {}
  int Decoded(webrtc::I420VideoFrame& frame);
  bool DecodeComplete();
 private:
  I420VideoFrame* decoded_video_frame_;
  bool decode_complete;
};

bool Vp8UnitTestDecodeCompleteCallback::DecodeComplete() {
  if (decode_complete) {
    decode_complete = false;
    return true;
  }
  return false;
}

int Vp8UnitTestDecodeCompleteCallback::Decoded(I420VideoFrame& image) {
  decoded_video_frame_->CopyFrame(image);
  decode_complete = true;
  return 0;
}

class TestVp8Impl : public ::testing::Test {
 protected:
  virtual void SetUp() {
    encoder_.reset(new VP8EncoderImpl());
    decoder_.reset(new VP8DecoderImpl());
    memset(&codec_inst_, 0, sizeof(codec_inst_));
    encode_complete_callback_.reset(new
        Vp8UnitTestEncodeCompleteCallback(&encoded_video_frame_));
    decode_complete_callback_.reset(new
        Vp8UnitTestDecodeCompleteCallback(&decoded_video_frame_));
    encoder_->RegisterEncodeCompleteCallback(encode_complete_callback_.get());
    decoder_->RegisterDecodeCompleteCallback(decode_complete_callback_.get());
  }

  unsigned int WaitForEncodedFrame() const {
    int64_t startTime = TickTime::MillisecondTimestamp();
    while (TickTime::MillisecondTimestamp() - startTime < kMaxWaitEncTimeMs) {
      if (encode_complete_callback_->EncodeComplete()) {
        return encoded_video_frame_.Length();
      }
    }
    return 0;
  }

  unsigned int WaitForDecodedFrame() const {
    int64_t startTime = TickTime::MillisecondTimestamp();
    while (TickTime::MillisecondTimestamp() - startTime < kMaxWaitDecTimeMs) {
      if (decode_complete_callback_->DecodeComplete()) {
        return CalcBufferSize(kI420, decoded_video_frame_.width(),
                              decoded_video_frame_.height());
      }
    }
    return 0;
  }

  void VideoFrameToEncodedImage(VideoFrame& frame, EncodedImage &image) {
    image._buffer = frame.Buffer();
    image._length = frame.Length();
    image._size = frame.Size();
    image._timeStamp = frame.TimeStamp();
    image._encodedWidth = frame.Width();
    image._encodedHeight = frame.Height();
    image._completeFrame = true;
  }

  scoped_ptr<Vp8UnitTestEncodeCompleteCallback> encode_complete_callback_;
  scoped_ptr<Vp8UnitTestDecodeCompleteCallback> decode_complete_callback_;
  scoped_array<uint8_t> source_buffer_;
  FILE* source_file_;
  scoped_ptr<VideoEncoder> encoder_;
  scoped_ptr<VideoDecoder> decoder_;
  VideoFrame encoded_video_frame_;
  I420VideoFrame decoded_video_frame_;
  unsigned int length_source_frame_;
  VideoCodec codec_inst_;
};

TEST_F(TestVp8Impl, AlignedStrideEncodeDecode) {
  // Using a QCIF image (aligned stride (u,v planse) > width).
  // Processing only one frame.
  const VideoSource source(test::ResourcePath("paris_qcif", "yuv"), kQCIF);
  length_source_frame_ = source.GetFrameLength();
  source_buffer_.reset(new uint8_t[length_source_frame_]);
  source_file_ = fopen(source.GetFileName().c_str(), "rb");
  ASSERT_TRUE(source_file_ != NULL);
  codec_inst_.maxFramerate = source.GetFrameRate();
  codec_inst_.startBitrate = 300;
  codec_inst_.maxBitrate = 4000;
  codec_inst_.width = source.GetWidth();
  codec_inst_.height = source.GetHeight();
  codec_inst_.codecSpecific.VP8.denoisingOn = true;

  // Get input frame.
  ASSERT_EQ(fread(source_buffer_.get(), 1, length_source_frame_, source_file_),
            length_source_frame_);
  // Setting aligned stride values.
  int stride_uv = 0;
  int stride_y = 0;
  Calc16ByteAlignedStride(codec_inst_.width, &stride_y, &stride_uv);
  EXPECT_EQ(stride_y, 176);
  EXPECT_EQ(stride_uv, 96);

  I420VideoFrame input_frame;
  input_frame.CreateEmptyFrame(codec_inst_.width, codec_inst_.height,
                                     stride_y, stride_uv, stride_uv);
  // Using ConvertToI420 to add stride to the image.
  EXPECT_EQ(0, ConvertToI420(kI420, source_buffer_.get(), 0, 0,
                             codec_inst_.width, codec_inst_.height,
                             0, kRotateNone, &input_frame));

  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, encoder_->InitEncode(&codec_inst_, 1, 1440));
  encoder_->Encode(input_frame, NULL, NULL);
  EXPECT_GT(WaitForEncodedFrame(), 0u);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->InitDecode(&codec_inst_, 1));
  EncodedImage encodedImage;
  VideoFrameToEncodedImage(encoded_video_frame_, encodedImage);
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Decode(encodedImage, false, NULL));
  EXPECT_GT(WaitForDecodedFrame(), 0u);
  // Compute PSNR on all planes (faster than SSIM).
  EXPECT_GT(I420PSNR(&input_frame, &decoded_video_frame_), 36);
}

}  // namespace webrtc
