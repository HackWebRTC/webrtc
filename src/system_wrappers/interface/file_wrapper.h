/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_SYSTEM_WRAPPERS_INTERFACE_FILE_WRAPPER_H_
#define WEBRTC_SYSTEM_WRAPPERS_INTERFACE_FILE_WRAPPER_H_

#include "common_types.h"
#include "typedefs.h"

// Implementation of an InStream and OutStream that can read (exclusive) or
// write from/to a file.

namespace webrtc {
class FileWrapper : public InStream, public OutStream
{
public:
    enum { kMaxFileNameSize = 1024};
    enum { kFileMaxTextMessageSize = 1024};

    // Factory method. Constructor disabled.
    static FileWrapper* Create();

    // Returns true if a file has been opened.
    virtual bool Open() const = 0;

    // Opens a file in read or write mode, decided by the readOnly parameter.
    virtual WebRtc_Word32 OpenFile(const WebRtc_Word8* fileNameUTF8,
                                   const bool readOnly,
                                   const bool loop = false,
                                   const bool text = false) = 0;

    virtual WebRtc_Word32 CloseFile() = 0;

    // Limits the file size.
    virtual WebRtc_Word32 SetMaxFileSize(WebRtc_Word32 bytes)  = 0;

    // Flush any pending writes.
    virtual WebRtc_Word32 Flush() = 0;

    // Returns the opened file's name in fileNameUTF8. size is the allocated
    // size of fileNameUTF8. The name will be truncated if the size of
    // fileNameUTF8 is to small.
    virtual WebRtc_Word32 FileName(WebRtc_Word8* fileNameUTF8,
                                   WebRtc_UWord32 size) const = 0;

    // Write text to the opened file. The written text can contain plain text
    // and text with type specifiers in the same way as sprintf works.
    virtual WebRtc_Word32 WriteText(const WebRtc_Word8* text, ...) = 0;

    // Reads len number of bytes from buf to file.
    virtual int Read(void* buf, int len) = 0;

    // Writes len number of bytes to buf from file. Please note that the actual
    // writing to file may happen some time later. Call flush to force a write
    // to take affect
    virtual bool Write(const void *buf,int len) = 0;

    // Rewinds the file to the start. Only available when OpenFile() has been
    // called with loop argument set to true. Or readOnly argument has been set
    // to false.
    virtual int Rewind() = 0;
};
} // namespace webrtc

#endif // WEBRTC_SYSTEM_WRAPPERS_INTERFACE_FILE_WRAPPER_H_
