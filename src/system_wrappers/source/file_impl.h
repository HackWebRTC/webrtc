/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_SOURCE_FILE_IMPL_H_
#define WEBRTC_SYSTEM_WRAPPERS_SOURCE_FILE_IMPL_H_

#include "file_wrapper.h"

#include <stdio.h>

namespace webrtc {
class FileWrapperImpl : public FileWrapper
{
public:
    FileWrapperImpl();
    virtual ~FileWrapperImpl();

    virtual WebRtc_Word32 FileName(WebRtc_Word8* fileNameUTF8,
                                   WebRtc_UWord32 size) const;

    virtual bool Open() const;

    virtual WebRtc_Word32 OpenFile(const WebRtc_Word8* fileNameUTF8,
                                 const bool readOnly,
                                 const bool loop = false,
                                 const bool text = false);

    virtual WebRtc_Word32 CloseFile();
    virtual WebRtc_Word32 SetMaxFileSize(WebRtc_Word32 bytes);
    virtual WebRtc_Word32 Flush();

    virtual int Read(void* buf, int len);
    virtual bool Write(const void *buf, int len);
    virtual int Rewind();

    virtual WebRtc_Word32 WriteText(const char* format, ...);

private:
    FILE*          _id;
    bool           _open;
    bool           _looping;
    bool           _readOnly;
    bool           _text;
    WebRtc_Word32  _maxSizeInBytes; // -1 indicates file size limitation is off
    WebRtc_UWord32 _sizeInBytes;
    WebRtc_Word8   _fileNameUTF8[kMaxFileNameSize];
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_SOURCE_FILE_IMPL_H_
