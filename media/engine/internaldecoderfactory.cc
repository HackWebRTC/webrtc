/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "media/engine/internaldecoderfactory.h"

#include "api/video_codecs/sdp_video_format.h"
#include "media/base/mediaconstants.h"
#if defined(USE_BUILTIN_SW_CODECS)
#include "modules/video_coding/codecs/h264/include/h264.h"
#include "modules/video_coding/codecs/vp8/include/vp8.h"  // nogncheck
#include "modules/video_coding/codecs/vp9/include/vp9.h"  // nogncheck
#endif
#include "rtc_base/checks.h"
#include "rtc_base/logging.h"

namespace webrtc {

std::vector<SdpVideoFormat> InternalDecoderFactory::GetSupportedFormats()
    const {
  std::vector<SdpVideoFormat> formats;
#if defined(USE_BUILTIN_SW_CODECS)
  formats.push_back(SdpVideoFormat(cricket::kVp8CodecName));
  if (VP9Decoder::IsSupported())
    formats.push_back(SdpVideoFormat(cricket::kVp9CodecName));
  for (const SdpVideoFormat& h264_format : SupportedH264Codecs())
    formats.push_back(h264_format);
#endif
  return formats;
}

std::unique_ptr<VideoDecoder> InternalDecoderFactory::CreateVideoDecoder(
    const SdpVideoFormat& format) {
#if defined(USE_BUILTIN_SW_CODECS)
  if (cricket::CodecNamesEq(format.name, cricket::kVp8CodecName))
    return VP8Decoder::Create();

  if (cricket::CodecNamesEq(format.name, cricket::kVp9CodecName)) {
    RTC_DCHECK(VP9Decoder::IsSupported());
    return VP9Decoder::Create();
  }

  if (cricket::CodecNamesEq(format.name, cricket::kH264CodecName))
    return H264Decoder::Create();
#endif

  RTC_LOG(LS_ERROR) << "Trying to create decoder for unsupported format";
  return nullptr;
}

}  // namespace webrtc
