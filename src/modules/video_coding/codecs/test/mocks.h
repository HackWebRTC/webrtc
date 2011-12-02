/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef SRC_MODULES_VIDEO_CODING_CODECS_TEST_MOCKS_H_
#define SRC_MODULES_VIDEO_CODING_CODECS_TEST_MOCKS_H_

#include <string>

#include "file_handler.h"
#include "gmock/gmock.h"
#include "packet_manipulator.h"
#include "typedefs.h"

// This file contains mocks that are used by the unit tests.

namespace webrtc {

class MockVideoEncoder : public VideoEncoder {
 public:
  MOCK_CONST_METHOD2(Version,
                     WebRtc_Word32(WebRtc_Word8 *version,
                                   WebRtc_Word32 length));
  MOCK_METHOD3(InitEncode,
               WebRtc_Word32(const VideoCodec* codecSettings,
                             WebRtc_Word32 numberOfCores,
                             WebRtc_UWord32 maxPayloadSize));
  MOCK_METHOD3(Encode,
               WebRtc_Word32(const RawImage& inputImage,
                             const CodecSpecificInfo* codecSpecificInfo,
                             const VideoFrameType* frameType));
  MOCK_METHOD1(RegisterEncodeCompleteCallback,
               WebRtc_Word32(EncodedImageCallback* callback));
  MOCK_METHOD0(Release, WebRtc_Word32());
  MOCK_METHOD0(Reset, WebRtc_Word32());
  MOCK_METHOD2(SetChannelParameters, WebRtc_Word32(WebRtc_UWord32 packetLoss,
                                                   int rtt));
  MOCK_METHOD2(SetRates,
               WebRtc_Word32(WebRtc_UWord32 newBitRate,
                             WebRtc_UWord32 frameRate));
  MOCK_METHOD1(SetPeriodicKeyFrames, WebRtc_Word32(bool enable));
  MOCK_METHOD2(CodecConfigParameters,
               WebRtc_Word32(WebRtc_UWord8* /*buffer*/, WebRtc_Word32));
};

class MockVideoDecoder : public VideoDecoder {
 public:
  MOCK_METHOD2(InitDecode,
      WebRtc_Word32(const VideoCodec* codecSettings,
                    WebRtc_Word32 numberOfCores));
  MOCK_METHOD5(Decode,
               WebRtc_Word32(const EncodedImage& inputImage,
                             bool missingFrames,
                             const RTPFragmentationHeader* fragmentation,
                             const CodecSpecificInfo* codecSpecificInfo,
                             WebRtc_Word64 renderTimeMs));
  MOCK_METHOD1(RegisterDecodeCompleteCallback,
               WebRtc_Word32(DecodedImageCallback* callback));
  MOCK_METHOD0(Release, WebRtc_Word32());
  MOCK_METHOD0(Reset, WebRtc_Word32());
  MOCK_METHOD2(SetCodecConfigParameters,
               WebRtc_Word32(const WebRtc_UWord8* /*buffer*/, WebRtc_Word32));
  MOCK_METHOD0(Copy, VideoDecoder*());
};

namespace test {

class MockFileHandler : public FileHandler {
 public:
  MOCK_METHOD0(Init, bool());
  MOCK_METHOD1(ReadFrame, bool(WebRtc_UWord8* source_buffer));
  MOCK_METHOD1(WriteFrame, bool(WebRtc_UWord8* frame_buffer));
  MOCK_METHOD0(Close, void());
  MOCK_METHOD1(GetFileSize, WebRtc_UWord64(std::string filename));
  MOCK_METHOD0(GetFrameLength, int());
  MOCK_METHOD0(GetNumberOfFrames, int());
};

class MockPacketManipulator : public PacketManipulator {
 public:
  MOCK_METHOD1(ManipulatePackets, int(webrtc::EncodedImage* encoded_image));
};

}  // namespace test
}  // namespace webrtc

#endif  // SRC_MODULES_VIDEO_CODING_CODECS_TEST_MOCKS_H_
