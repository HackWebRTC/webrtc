/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "file_impl.h"

#include <assert.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <stdarg.h>
#include <string.h>
#endif

namespace webrtc {
FileWrapper* FileWrapper::Create()
{
    return new FileWrapperImpl();
}

FileWrapperImpl::FileWrapperImpl()
    : _id(NULL),
      _open(false),
      _looping(false),
      _readOnly(false),
      _text(false),
      _maxSizeInBytes(-1),
      _sizeInBytes(0)
{
    memset(_fileNameUTF8, 0, kMaxFileNameSize);
}

FileWrapperImpl::~FileWrapperImpl()
{
    if (_id != NULL)
    {
        fclose(_id);
    }
}

WebRtc_Word32 FileWrapperImpl::CloseFile()
{
    if (_id != NULL)
    {
        fclose(_id);
        _id = NULL;
    }
    memset(_fileNameUTF8, 0, kMaxFileNameSize);
    _open = false;
    return 0;
}

int FileWrapperImpl::Rewind()
{
    if(_looping || !_readOnly)
    {
        if (_id != NULL)
        {
            _sizeInBytes = 0;
            return fseek(_id, 0, SEEK_SET);
        }
    }
    return -1;
}

WebRtc_Word32 FileWrapperImpl::SetMaxFileSize(WebRtc_Word32 bytes)
{
    _maxSizeInBytes = bytes;
    return 0;
}

WebRtc_Word32 FileWrapperImpl::Flush()
{
    if (_id != NULL)
    {
        return fflush(_id);
    }
    return -1;
}

WebRtc_Word32 FileWrapperImpl::FileName(WebRtc_Word8* fileNameUTF8,
                                        WebRtc_UWord32 size) const
{
    WebRtc_Word32 len = static_cast<WebRtc_Word32>(strlen(_fileNameUTF8));
    if(len > kMaxFileNameSize)
    {
        assert(false);
        return -1;
    }
    if(len < 1)
    {
        return -1;
    }
    // Make sure to NULL terminate
    if(size < (WebRtc_UWord32)len)
    {
        len = size - 1;
    }
    memcpy(fileNameUTF8, _fileNameUTF8, len);
    fileNameUTF8[len] = 0;
    return 0;
}

bool
FileWrapperImpl::Open() const
{
    return _open;
}

WebRtc_Word32 FileWrapperImpl::OpenFile(const WebRtc_Word8 *fileNameUTF8,
                                        const bool readOnly, const bool loop,
                                        const bool text)
{
    WebRtc_Word32 length = (WebRtc_Word32)strlen(fileNameUTF8);
    if (length > kMaxFileNameSize)
    {
        return -1;
    }

    _readOnly = readOnly;
    _text = text;

    FILE *tmpId = NULL;
#if defined _WIN32
    wchar_t wideFileName[kMaxFileNameSize];
    wideFileName[0] = 0;

    MultiByteToWideChar(CP_UTF8,
                        0 /*UTF8 flag*/,
                        fileNameUTF8,
                        -1 /*Null terminated string*/,
                        wideFileName,
                        kMaxFileNameSize);
    if(text)
    {
        if(readOnly)
        {
            tmpId = _wfopen(wideFileName, L"rt");
        } else {
            tmpId = _wfopen(wideFileName, L"wt");
        }
    } else {
        if(readOnly)
        {
            tmpId = _wfopen(wideFileName, L"rb");
        } else {
            tmpId = _wfopen(wideFileName, L"wb");
        }
    }
#else
    if(text)
    {
        if(readOnly)
        {
            tmpId = fopen(fileNameUTF8, "rt");
        } else {
            tmpId = fopen(fileNameUTF8, "wt");
        }
    } else {
        if(readOnly)
        {
            tmpId = fopen(fileNameUTF8, "rb");
        } else {
            tmpId = fopen(fileNameUTF8, "wb");
        }
    }
#endif

    if (tmpId != NULL)
    {
        // +1 comes from copying the NULL termination character.
        memcpy(_fileNameUTF8, fileNameUTF8, length + 1);
        if (_id != NULL)
        {
            fclose(_id);
        }
        _id = tmpId;
        _looping = loop;
        _open = true;
        return 0;
    }
    return -1;
}

int FileWrapperImpl::Read(void *buf, int len)
{
    if(len < 0)
    {
        return 0;
    }
    if (_id != NULL)
    {
        int res = static_cast<int>(fread(buf, 1, len, _id));
        if (res != len)
        {
            if(!_looping)
            {
                CloseFile();
            }
        }
        return res;
    }
    return -1;
}

WebRtc_Word32 FileWrapperImpl::WriteText(const char* format, ...)
{
    if (_readOnly)
        return -1;

    if (!_text)
        return -1;

    if (_id == NULL)
        return -1;

    if (format == NULL)
        return -1;

    va_list args;
    va_start(args, format);
    int num_bytes = vfprintf(_id, format, args);
    va_end(args);

    if (num_bytes > 0)
    {
        return 0;
    }
    else
    {
        CloseFile();
        return -1;
    }
}

bool FileWrapperImpl::Write(const void* buf, int len)
{
    if (!_readOnly)
    {
        return false;
    }

    if (_id != NULL)
    {
        // Check if it's time to stop writing.
        if ((_maxSizeInBytes != -1) &&
             _sizeInBytes + len > (WebRtc_UWord32)_maxSizeInBytes)
        {
            Flush();
            return false;
        }

        size_t nBytes = fwrite((WebRtc_UWord8*)buf, 1, len, _id);
        if (nBytes > 0)
        {
            _sizeInBytes += static_cast<WebRtc_Word32>(nBytes);
            return true;
        }
        CloseFile();
    }
    return false;
}
} // namespace webrtc
