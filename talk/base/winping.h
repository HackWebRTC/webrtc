/*
 * libjingle
 * Copyright 2004--2005, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, 
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products 
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR 
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_BASE_WINPING_H__
#define TALK_BASE_WINPING_H__

#ifdef WIN32

#include "talk/base/win32.h"
#include "talk/base/basictypes.h"
#include "talk/base/IPAddress.h"

namespace talk_base {

// This class wraps a Win32 API for doing ICMP pinging.  This API, unlike the
// the normal socket APIs (as implemented on Win9x), will return an error if
// an ICMP packet with the dont-fragment bit set is too large.  This means this
// class can be used to detect the MTU to a given address.

typedef struct ip_option_information {
    UCHAR   Ttl;                // Time To Live
    UCHAR   Tos;                // Type Of Service
    UCHAR   Flags;              // IP header flags
    UCHAR   OptionsSize;        // Size in bytes of options data
    PUCHAR  OptionsData;        // Pointer to options data
} IP_OPTION_INFORMATION, * PIP_OPTION_INFORMATION;

typedef HANDLE (WINAPI *PIcmpCreateFile)();

typedef BOOL (WINAPI *PIcmpCloseHandle)(HANDLE icmp_handle);

typedef HANDLE (WINAPI *PIcmp6CreateFile)();

typedef BOOL (WINAPI *PIcmp6CloseHandle)(HANDLE icmp_handle);

typedef DWORD (WINAPI *PIcmpSendEcho)(
    HANDLE                   IcmpHandle,
    ULONG                    DestinationAddress,
    LPVOID                   RequestData,
    WORD                     RequestSize,
    PIP_OPTION_INFORMATION   RequestOptions,
    LPVOID                   ReplyBuffer,
    DWORD                    ReplySize,
    DWORD                    Timeout);

typedef DWORD (WINAPI *PIcmp6SendEcho2)(
    HANDLE IcmpHandle,
    HANDLE Event,
    FARPROC ApcRoutine,
    PVOID ApcContext,
    struct sockaddr_in6 *SourceAddress,
    struct sockaddr_in6 *DestinationAddress,
    LPVOID RequestData,
    WORD RequestSize,
    PIP_OPTION_INFORMATION RequestOptions,
    LPVOID ReplyBuffer,
    DWORD ReplySize,
    DWORD Timeout
);

class WinPing {
public:
    WinPing();
    ~WinPing();

    // Determines whether the class was initialized correctly.
    bool IsValid() { return valid_; }

    // Attempts to send a ping with the given parameters.
    enum PingResult { PING_FAIL, PING_INVALID_PARAMS,
                      PING_TOO_LARGE, PING_TIMEOUT, PING_SUCCESS };
    PingResult Ping(
        IPAddress ip, uint32 data_size, uint32 timeout_millis, uint8 ttl,
        bool allow_fragments);

private:
    HMODULE dll_;
    HANDLE hping_;
    HANDLE hping6_;
    PIcmpCreateFile create_;
    PIcmpCloseHandle close_;
    PIcmpSendEcho send_;
    PIcmp6CreateFile create6_;
    PIcmp6SendEcho2 send6_;
    char* data_;
    uint32 dlen_;
    char* reply_;
    uint32 rlen_;
    bool valid_;
};

} // namespace talk_base

#endif // WIN32

#endif // TALK_BASE_WINPING_H__
