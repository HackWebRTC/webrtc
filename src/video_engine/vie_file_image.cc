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
 * vie_file_image.cc
 */

#include "engine_configurations.h"

#ifdef WEBRTC_VIDEO_ENGINE_FILE_API
#include <stdio.h>
#include "vie_file_image.h"
#include "video_image.h"
#include "jpeg.h"
#include "trace.h"

namespace webrtc {

int ViEFileImage::ConvertJPEGToVideoFrame(int engineId,
                                          const char* fileNameUTF8,
                                          VideoFrame& videoFrame)
{
    // read jpeg file into temporary buffer
    EncodedImage imageBuffer;

    FILE* imageFile = fopen(fileNameUTF8, "rb");
    if (NULL == imageFile)
    {
        // error reading file
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, engineId,
                   "%s could not open file %s", __FUNCTION__, fileNameUTF8);
        return -1;
    }
    fseek(imageFile, 0, SEEK_END);
    imageBuffer._size = ftell(imageFile);
    fseek(imageFile, 0, SEEK_SET);
    imageBuffer._buffer = new WebRtc_UWord8[ imageBuffer._size + 1];
    if ( imageBuffer._size != fread(imageBuffer._buffer, sizeof(WebRtc_UWord8),
                                    imageBuffer._size, imageFile))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, engineId,
                   "%s could not read file %s", __FUNCTION__, fileNameUTF8);
        delete [] imageBuffer._buffer;
        return -1;
    }
    fclose(imageFile);

    // if this is a jpeg file, decode it
    JpegDecoder decoder;

    int ret = 0;

    RawImage decodedImage;
    ret = decoder.Decode(imageBuffer, decodedImage);

    // done with this.
    delete [] imageBuffer._buffer;
    imageBuffer._buffer = NULL;

    if (-1 == ret)
    {
        // error decoding the file
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, engineId,
                   "%s could decode file %s from jpeg format", __FUNCTION__,
                   fileNameUTF8);
        return -1;
    } else if (-3 == ret)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, engineId,
                   "%s could not convert jpeg's data to i420 format",
                   __FUNCTION__, fileNameUTF8);
    }

    WebRtc_UWord32 imageLength = (WebRtc_UWord32)(decodedImage._width *
                                                  decodedImage._height * 1.5);
    if (-1 == videoFrame.Swap(decodedImage._buffer, imageLength, imageLength))
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideo,
                   engineId,
                   "%s could not copy frame imageDecodedBuffer to videoFrame ",
                   __FUNCTION__, fileNameUTF8);
        return -1;
    }

    if (decodedImage._buffer)
    {
        delete [] decodedImage._buffer;
        decodedImage._buffer = NULL;
    }

    videoFrame.SetWidth(decodedImage._width);
    videoFrame.SetHeight(decodedImage._height);
    return 0;
}

int ViEFileImage::ConvertPictureToVideoFrame(int engineId,
                                             const ViEPicture& picture,
                                             VideoFrame& videoFrame)
{
    WebRtc_UWord32 pictureLength = (WebRtc_UWord32)(picture.width
              * picture.height * 1.5);
    videoFrame.CopyFrame(pictureLength, picture.data);
    videoFrame.SetWidth(picture.width);
    videoFrame.SetHeight(picture.height);
    videoFrame.SetLength(pictureLength);

    return 0;
}
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_FILE_API
