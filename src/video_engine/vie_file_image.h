/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * vie_file_image.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FILE_IMAGE_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FILE_IMAGE_H_

#include "typedefs.h"
#include "vie_file.h"
#include "module_common_types.h"
namespace webrtc {
class ViEFileImage
{
public:
    static int ConvertJPEGToVideoFrame(int engineId,
                                       const char* fileNameUTF8,
                                       VideoFrame& videoFrame);
    static int ConvertPictureToVideoFrame(int engineId,
                                          const ViEPicture& picture,
                                          VideoFrame& videoFrame);
};
} // namespace webrtc

#endif // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_FILE_IMAGE_H_
