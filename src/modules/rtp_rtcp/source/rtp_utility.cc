/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtp_utility.h"

#include <cstring> // memcpy
#include <cmath>   // ceil
#include <cassert>

#if defined(_WIN32)
    #include <Windows.h> // FILETIME
    #include <WinSock.h> // timeval
    #include <MMSystem.h> //timeGetTime
#elif ((defined WEBRTC_LINUX) || (defined WEBRTC_MAC))
    #include <sys/time.h> // gettimeofday
    #include <time.h>
#endif

#if (defined(_DEBUG) && defined(_WIN32) && (_MSC_VER >= 1400))
#include <stdio.h>
#define DEBUG_PRINT(...)        \
{                                \
    char msg[256];                \
    sprintf(msg, __VA_ARGS__);    \
    OutputDebugString(msg);        \
}
#else
// special fix for visual 2003
#define DEBUG_PRINT(exp)        ((void)0)
#endif  // defined(_DEBUG) && defined(_WIN32)

namespace
{
    const float FRAC = 4.294967296E9;
}

namespace webrtc {
// PLATFORM SPECIFIC [BEGIN]
#if defined(_WIN32)
    #include <Windows.h>

    namespace ModuleRTPUtility
    {
        WebRtc_UWord32 GetTimeInMS()
        {
            return timeGetTime();
        }
        bool StringCompare(const WebRtc_Word8* str1 , const WebRtc_Word8* str2, const WebRtc_UWord32 length)
        {
            return (_strnicmp(str1, str2, length) == 0)?true: false;
        }

        class HelpTimer
        {
        public:
            struct reference_point
            {
                FILETIME      file_time;
                LARGE_INTEGER counterMS;
            };

            HelpTimer()
            {
                // set timer accuracy to 1 ms
                timeBeginPeriod(1);

                _timeInMs = 0;
                _numWrapTimeInMs = 0;
                synchronize();
            };

            virtual ~HelpTimer()
            {
                timeEndPeriod(1);
            };

            void get_time(FILETIME& current_time)
            {
                // we can't use query performance counter due to speed stepping
                DWORD t = timeGetTime();
                // NOTE: we have a miss match in sign between _timeInMs(LONG) and t(DWORD) however we only use it here without +- etc
                volatile LONG* timeInMsPtr = &_timeInMs;
                DWORD old = InterlockedExchange(timeInMsPtr, t); // make sure that we only inc wrapper once
                if(old > t)
                {
                    // wrap
                    _numWrapTimeInMs++;
                }
                LARGE_INTEGER elapsedMS;
                elapsedMS.HighPart = _numWrapTimeInMs;
                elapsedMS.LowPart = t;

                elapsedMS.QuadPart = elapsedMS.QuadPart - _ref_point.counterMS.QuadPart;

                //
                // Translate to 100-nanoseconds intervals (FILETIME resolution) and add to
                // reference FILETIME to get current FILETIME.
                //
                ULARGE_INTEGER filetime_ref_as_ul;

                filetime_ref_as_ul.HighPart = _ref_point.file_time.dwHighDateTime;
                filetime_ref_as_ul.LowPart = _ref_point.file_time.dwLowDateTime;
                filetime_ref_as_ul.QuadPart += (ULONGLONG)((elapsedMS.QuadPart)*1000*10);
                //
                // Copy to result
                //
                current_time.dwHighDateTime = filetime_ref_as_ul.HighPart;
                current_time.dwLowDateTime = filetime_ref_as_ul.LowPart;
            };

        private:
            void synchronize()
            {
                FILETIME    ft0 = { 0, 0 },
                            ft1 = { 0, 0 };
                //
                // Spin waiting for a change in system time. Get the matching
                // performance counter value for that time.
                //
                ::GetSystemTimeAsFileTime(&ft0);
                do
                {
                    ::GetSystemTimeAsFileTime(&ft1);
                    _ref_point.counterMS.QuadPart = ::timeGetTime();
                    ::Sleep(0);
                }
                while ((ft0.dwHighDateTime == ft1.dwHighDateTime) &&
                        (ft0.dwLowDateTime == ft1.dwLowDateTime));

                _ref_point.file_time = ft1;
            }
            volatile LONG           _timeInMs;              // this needs to be long due to Windows, not an issue due to its usage
            volatile WebRtc_UWord32   _numWrapTimeInMs;
            reference_point         _ref_point;
        };

        static HelpTimer helpTimer;

    }; // end namespace

#elif defined(WEBRTC_LINUX) || defined(WEBRTC_MAC)
    #include <sys/time.h> // gettimeofday
    #include <time.h> // nanosleep, gettimeofday

    WebRtc_UWord32
    ModuleRTPUtility::GetTimeInMS()
    {
        struct timeval tv;
        struct timezone tz;
        WebRtc_UWord32 val;

        gettimeofday(&tv, &tz);
        val = (WebRtc_UWord32)(tv.tv_sec*1000 + tv.tv_usec/1000);
        return val;
    }
    bool ModuleRTPUtility::StringCompare(const WebRtc_Word8* str1 , const WebRtc_Word8* str2, const WebRtc_UWord32 length)
    {
        return (strncasecmp(str1, str2, length) == 0)?true: false;
    }
#endif // PLATFORM SPECIFIC [END]

#if !defined(WEBRTC_LITTLE_ENDIAN) && !defined(WEBRTC_BIG_ENDIAN)
    #error Either WEBRTC_LITTLE_ENDIAN or WEBRTC_BIG_ENDIAN must be defined
#endif

/* for RTP/RTCP
    All integer fields are carried in network byte order, that is, most
    significant byte (octet) first.  AKA big-endian.
*/
void
ModuleRTPUtility::AssignUWord32ToBuffer(WebRtc_UWord8* dataBuffer, WebRtc_UWord32 value)
{
#if defined(WEBRTC_LITTLE_ENDIAN)
    dataBuffer[0] = static_cast<WebRtc_UWord8>(value>>24);
    dataBuffer[1] = static_cast<WebRtc_UWord8>(value>>16);
    dataBuffer[2] = static_cast<WebRtc_UWord8>(value>>8);
    dataBuffer[3] = static_cast<WebRtc_UWord8>(value);
#else
    WebRtc_UWord32* ptr = reinterpret_cast<WebRtc_UWord32*>(dataBuffer);
    ptr[0] = value;
#endif
}

void
ModuleRTPUtility::AssignUWord24ToBuffer(WebRtc_UWord8* dataBuffer, WebRtc_UWord32 value)
{
#if defined(WEBRTC_LITTLE_ENDIAN)
    dataBuffer[0] = static_cast<WebRtc_UWord8>(value>>16);
    dataBuffer[1] = static_cast<WebRtc_UWord8>(value>>8);
    dataBuffer[2] = static_cast<WebRtc_UWord8>(value);
#else
    dataBuffer[0] = static_cast<WebRtc_UWord8>(value);
    dataBuffer[1] = static_cast<WebRtc_UWord8>(value>>8);
    dataBuffer[2] = static_cast<WebRtc_UWord8>(value>>16);
#endif
}

void
ModuleRTPUtility::AssignUWord16ToBuffer(WebRtc_UWord8* dataBuffer, WebRtc_UWord16 value)
{
#if defined(WEBRTC_LITTLE_ENDIAN)
    dataBuffer[0] = static_cast<WebRtc_UWord8>(value>>8);
    dataBuffer[1] = static_cast<WebRtc_UWord8>(value);
#else
    WebRtc_UWord16* ptr = reinterpret_cast<WebRtc_UWord16*>(dataBuffer);
    ptr[0] = value;
#endif
}

WebRtc_UWord16
ModuleRTPUtility::BufferToUWord16(const WebRtc_UWord8* dataBuffer)
{
#if defined(WEBRTC_LITTLE_ENDIAN)
    return (dataBuffer[0] << 8) + dataBuffer[1];
#else
    return *reinterpret_cast<const WebRtc_UWord16*>(dataBuffer);
#endif
}

WebRtc_UWord32
ModuleRTPUtility::BufferToUWord24(const WebRtc_UWord8* dataBuffer)
{
    return (dataBuffer[0] << 16) + (dataBuffer[1] << 8) + dataBuffer[2];
}

WebRtc_UWord32
ModuleRTPUtility::BufferToUWord32(const WebRtc_UWord8* dataBuffer)
{
#if defined(WEBRTC_LITTLE_ENDIAN)
    return (dataBuffer[0] << 24) + (dataBuffer[1] << 16) + (dataBuffer[2] << 8) +
        dataBuffer[3];
#else
    return *reinterpret_cast<const WebRtc_UWord32*>(dataBuffer);
#endif
}

WebRtc_UWord32
ModuleRTPUtility::pow2(WebRtc_UWord8 exp)
{
    return 1 << exp;
}

WebRtc_Word32
ModuleRTPUtility::CurrentNTP(WebRtc_UWord32& secs, WebRtc_UWord32& frac)
{
    /*
    *  Use the system time (roughly synchronised to the tick, and
    *  extrapolated using the system performance counter.
    */
    const  WebRtc_UWord32 JAN_1970 = 2208988800UL; // NTP seconds

#if defined(_WIN32)
    const WebRtc_UWord64 FILETIME_1970 = 0x019db1ded53e8000;

    FILETIME StartTime;
    WebRtc_UWord64 Time;
    struct timeval tv;

    // we can't use query performance counter since they can change depending on speed steping
    helpTimer.get_time(StartTime);

    Time = (((WebRtc_UWord64) StartTime.dwHighDateTime) << 32) +
        (WebRtc_UWord64) StartTime.dwLowDateTime;

    // Convert the hecto-nano second time to tv format
    Time -= FILETIME_1970;

    tv.tv_sec = (WebRtc_UWord32) ( Time / (WebRtc_UWord64)10000000);
    tv.tv_usec = (WebRtc_UWord32) (( Time % (WebRtc_UWord64)10000000) / 10);

    double dtemp;

    secs = tv.tv_sec + JAN_1970;
    dtemp = tv.tv_usec / 1e6;

    if (dtemp >= 1)
    {
        dtemp -= 1;
        secs++;
    } else if (dtemp < -1)
    {
        dtemp += 1;
        secs--;
    }
    dtemp *= FRAC;
    frac = (WebRtc_UWord32)dtemp;
    return 0;
#endif

#if ((defined WEBRTC_LINUX) || (defined WEBRTC_MAC))

    double dtemp;
    struct timeval tv;
    struct timezone tz;
    tz.tz_minuteswest  = 0;
    tz.tz_dsttime = 0;
    gettimeofday(&tv,&tz);

    secs = tv.tv_sec + JAN_1970;
    dtemp = tv.tv_usec / 1e6;
    if (dtemp >= 1)
    {
        dtemp -= 1;
        secs++;
    } else if (dtemp < -1)
    {
        dtemp += 1;
        secs--;
    }
    dtemp *= FRAC;
    frac = (WebRtc_UWord32)dtemp;

    return(0);
#endif
}

WebRtc_UWord32
ModuleRTPUtility::ConvertNTPTimeToMS(WebRtc_UWord32 NTPsec, WebRtc_UWord32 NTPfrac)
{
    int freq = 1000;
    float ftemp = (float)NTPfrac/(float)FRAC;
    WebRtc_UWord32 tmp = (WebRtc_UWord32)(ftemp * freq);
    WebRtc_UWord32 MStime= NTPsec*freq + tmp;
    return MStime;
}

WebRtc_UWord32
ModuleRTPUtility::CurrentRTP(WebRtc_UWord32 freq)
{
    WebRtc_UWord32 NTPsec = 0;
    WebRtc_UWord32 NTPfrac = 0;
    CurrentNTP( NTPsec, NTPfrac);
    float ftemp = (float)NTPfrac/(float)FRAC;
    WebRtc_UWord32 tmp = (WebRtc_UWord32)(ftemp * freq);
    return NTPsec*freq + tmp;
}

void
ModuleRTPUtility::RTPPayload::SetType(RtpVideoCodecTypes videoType)
{
    type = videoType;

    switch (type)
    {
    case kRtpNoVideo:
        break;
    case kRtpH263Video:
    case kRtpH2631998Video:
    {
        info.H263.hasPictureStartCode  = false;
        info.H263.insert2byteStartCode = false;
        info.H263.hasPbit              = false;
        info.H263.frameWidth           = 0;
        info.H263.frameHeight          = 0;
        info.H263.startBits            = 0;
        info.H263.endBits              = 0;
        info.H263.data                 = 0;
        info.H263.dataLength           = 0;
        break;
    }
    case kRtpMpeg4Video:
    {
        info.MPEG4.isFirstPacket = false;
        info.MPEG4.data          = 0;
        info.MPEG4.dataLength    = 0;
        break;
    }
    case kRtpVp8Video:
    {
        info.VP8.beginningOfFrame = false;
        info.VP8.nonReferenceFrame = false;
        info.VP8.hasPictureID = false;
        info.VP8.fragments = false;
        info.VP8.startFragment = false;
        info.VP8.stopFragment = false;
        break;
    }
    default:
        break;
    }
}

ModuleRTPUtility::RTPHeaderParser::RTPHeaderParser(const WebRtc_UWord8* rtpData,
                                                   const WebRtc_UWord32 rtpDataLength):
    _ptrRTPDataBegin(rtpData),
    _ptrRTPDataEnd(rtpData ? (rtpData + rtpDataLength) : NULL)
{
}

ModuleRTPUtility::RTPHeaderParser::~RTPHeaderParser()
{
}

bool
ModuleRTPUtility::RTPHeaderParser::RTCP() const
{
    // 72 to 76 is reserved for RTP
    // 77 to 79 is not reserver but  they are not assigned we will block them
    // for RTCP 200 SR  == marker bit + 72
    // for RTCP 204 APP == marker bit + 76
    /*
    *       RTCP
    *
    * FIR      full INTRA-frame request             192     [RFC2032]   supported
    * NACK     negative acknowledgement             193     [RFC2032]
    * IJ       Extended inter-arrival jitter report 195     [RFC-ietf-avt-rtp-toffset-07.txt] http://tools.ietf.org/html/draft-ietf-avt-rtp-toffset-07
    * SR       sender report                        200     [RFC3551]   supported
    * RR       receiver report                      201     [RFC3551]   supported
    * SDES     source description                   202     [RFC3551]   supported
    * BYE      goodbye                              203     [RFC3551]   supported
    * APP      application-defined                  204     [RFC3551]   ignored
    * RTPFB    Transport layer FB message           205     [RFC4585]   supported
    * PSFB     Payload-specific FB message          206     [RFC4585]   supported
    * XR       extended report                      207     [RFC3611]   supported
    */

   /* 205       RFC 5104
    * FMT 1      NACK       supported
    * FMT 2      reserved
    * FMT 3      TMMBR      supported
    * FMT 4      TMMBN      supported
    */

    /* 206      RFC 5104
    * FMT 1:     Picture Loss Indication (PLI)                      supported
    * FMT 2:     Slice Lost Indication (SLI)
    * FMT 3:     Reference Picture Selection Indication (RPSI)
    * FMT 4:     Full Intra Request (FIR) Command                   supported
    * FMT 5:     Temporal-Spatial Trade-off Request (TSTR)
    * FMT 6:     Temporal-Spatial Trade-off Notification (TSTN)
    * FMT 7:     Video Back Channel Message (VBCM)
    * FMT 15:    Application layer FB message
    */

    const WebRtc_UWord8  payloadType = _ptrRTPDataBegin[1];

    bool RTCP = false;

    // check if this is a RTCP packet
    switch(payloadType)
    {
    case 192:
        RTCP = true;
        break;
    case 193:
    case 195:
        // not supported
        // pass through and check for a potential RTP packet
        break;
    case 200:
    case 201:
    case 202:
    case 203:
    case 204:
    case 205:
    case 206:
    case 207:
        RTCP = true;
        break;
    }
    return RTCP;
}

bool
ModuleRTPUtility::RTPHeaderParser::Parse(WebRtcRTPHeader& parsedPacket) const
{
    const ptrdiff_t length = _ptrRTPDataEnd - _ptrRTPDataBegin;

    if (length < 12)
    {
        return false;
    }

    const WebRtc_UWord8 V  = _ptrRTPDataBegin[0] >> 6 ;                          // Version
    const bool          P  = ((_ptrRTPDataBegin[0] & 0x20) == 0) ? false : true; // Padding
    const bool          X  = ((_ptrRTPDataBegin[0] & 0x10) == 0) ? false : true; // eXtension
    const WebRtc_UWord8 CC = _ptrRTPDataBegin[0] & 0x0f;
    const bool          M  = ((_ptrRTPDataBegin[1] & 0x80) == 0) ? false : true;

    const WebRtc_UWord8 PT = _ptrRTPDataBegin[1] & 0x7f;

    const WebRtc_UWord16 sequenceNumber = (_ptrRTPDataBegin[2] << 8) + _ptrRTPDataBegin[3];

    const WebRtc_UWord8* ptr = &_ptrRTPDataBegin[4];

    WebRtc_UWord32 RTPTimestamp = *ptr++ << 24;
    RTPTimestamp += *ptr++ << 16;
    RTPTimestamp += *ptr++ << 8;
    RTPTimestamp += *ptr++;

    WebRtc_UWord32 SSRC = *ptr++ << 24;
    SSRC += *ptr++ << 16;
    SSRC += *ptr++ << 8;
    SSRC += *ptr++;

    if (V != 2)
    {
        return false;
    }

    const WebRtc_UWord8 CSRCocts = CC * 4;

    if ((ptr + CSRCocts) > _ptrRTPDataEnd)
    {
        return false;
    }

    parsedPacket.header.markerBit      = M;
    parsedPacket.header.payloadType    = PT;
    parsedPacket.header.sequenceNumber = sequenceNumber;
    parsedPacket.header.timestamp      = RTPTimestamp;
    parsedPacket.header.ssrc           = SSRC;
    parsedPacket.header.numCSRCs       = CC;
    parsedPacket.header.paddingLength  = P ? *(_ptrRTPDataEnd - 1) : 0;

    for (unsigned int i = 0; i < CC; ++i)
    {
        WebRtc_UWord32 CSRC = *ptr++ << 24;
        CSRC += *ptr++ << 16;
        CSRC += *ptr++ << 8;
        CSRC += *ptr++;
        parsedPacket.header.arrOfCSRCs[i] = CSRC;
    }
    parsedPacket.type.Audio.numEnergy = parsedPacket.header.numCSRCs;

    parsedPacket.header.headerLength   = 12 + CSRCocts;
    if (X)
    {
        const ptrdiff_t remain = _ptrRTPDataEnd - ptr;
        if (remain < 4)
        {
            return false;
        }

        parsedPacket.header.headerLength += 4;

        WebRtc_UWord16 definedByProfile = *ptr++ << 8;
        definedByProfile += *ptr++;

        WebRtc_UWord16 XLen = *ptr++ << 8;
        XLen += *ptr++; // in 32 bit words
        XLen *= 4; // in octs

        if (remain < (4 + XLen))
        {
            return false;
        }
        if(definedByProfile == RTP_AUDIO_LEVEL_UNIQUE_ID && XLen == 4)
        {
            // --- Only used for debugging ---

            /*
            0                   1                   2                   3
            0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |      0xBE     |      0xDE     |            length=1           |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |  ID   | len=0 |V|   level     |      0x00     |      0x00     |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            */

            // Parse out the fields but only use it for debugging for now.
            //const WebRtc_UWord8 ID = (*ptr & 0xf0) >> 4;
            //const WebRtc_UWord8 len = (*ptr & 0x0f);
            ptr++;
            //const WebRtc_UWord8 V = (*ptr & 0x80) >> 7;
            //const WebRtc_UWord8 level = (*ptr & 0x7f);
            // DEBUG_PRINT("RTP_AUDIO_LEVEL_UNIQUE_ID: ID=%u, len=%u, V=%u, level=%u", ID, len, V, level);
        }
        parsedPacket.header.headerLength += XLen;
    }

    return true;
}

// RTP payload parser
ModuleRTPUtility::RTPPayloadParser::RTPPayloadParser(const RtpVideoCodecTypes videoType,
                                                     const WebRtc_UWord8* payloadData,
                                                     const WebRtc_UWord16 payloadDataLength)
    :
    _dataPtr(payloadData),
    _dataLength(payloadDataLength),
    _videoType(videoType)
{
}

ModuleRTPUtility::RTPPayloadParser::~RTPPayloadParser()
{
}

bool
ModuleRTPUtility::RTPPayloadParser::Parse( RTPPayload& parsedPacket) const
{
    parsedPacket.SetType(_videoType);

    switch (_videoType)
    {
    case kRtpNoVideo:
        return ParseGeneric(parsedPacket);
    case kRtpH263Video:
        return ParseH263(parsedPacket);
    case kRtpH2631998Video:
        return ParseH2631998(parsedPacket);
    case kRtpMpeg4Video:
        return ParseMPEG4(parsedPacket);
    case kRtpVp8Video:
        return ParseVP8(parsedPacket);
    default:
        return false;
    }
}

bool
ModuleRTPUtility::RTPPayloadParser::ParseGeneric(RTPPayload& /*parsedPacket*/) const
{
    return false;
}

bool
ModuleRTPUtility::RTPPayloadParser::ParseH263(RTPPayload& parsedPacket) const
{
    if(_dataLength <= 2)
    {
        // data length sanity check.
        return false;
    }

    const WebRtc_UWord8 header1 = _dataPtr[0];
    const WebRtc_UWord8 header2 = _dataPtr[1];

    parsedPacket.frameType = ((header2 & 0x10) == 0) ? kIFrame : kPFrame;

    unsigned int h263HeaderLength = 0;
    if ((header1 & 0x80) == 0)
    {
        // Mode A
        h263HeaderLength = 4;
    }
    else
    {
        // In Mode B and Mode C, I bit is in 5th byte of header
        const WebRtc_UWord8 header5 = _dataPtr[4];
        parsedPacket.frameType = ((header5 & 0x80) == 0) ? kIFrame : kPFrame;

        if((header1 & 0x40) == 0)
        {
            // Mode B
            // IMPROVEMENT use the information in the H263 header?
            // GQuant of the first MB
            h263HeaderLength = 8;
        }else
        {
            // Mode C
            h263HeaderLength = 12;
        }
    }

    if (_dataLength < h263HeaderLength)
    {
        // Received empty H263 packet
        return false;
    }

    // Get SBIT and EBIT
    WebRtc_UWord8 sbit = 0;
    WebRtc_UWord8 ebit = 0;
    const WebRtc_UWord8 seBit = header1 & 0x3f;
    if (seBit)
    {
        // We got SBIT or EBIT
        sbit = (seBit >> 3) & 0x07;
        ebit = seBit & 0x07;
    }

    const bool isH263PictureStartCode = H263PictureStartCode(_dataPtr + h263HeaderLength);
    if (isH263PictureStartCode)
    {
        // parse out real size and inform the decoder
        WebRtc_UWord16 width  = 0;
        WebRtc_UWord16 height = 0;

        GetH263FrameSize(_dataPtr + h263HeaderLength, width, height);

        parsedPacket.info.H263.hasPictureStartCode = true;
        parsedPacket.info.H263.frameWidth          = width;
        parsedPacket.info.H263.frameHeight         = height;
    }
    parsedPacket.info.H263.startBits = sbit;
    parsedPacket.info.H263.endBits = ebit;
    parsedPacket.info.H263.data       = _dataPtr + h263HeaderLength;
    parsedPacket.info.H263.dataLength = _dataLength - h263HeaderLength;
    parsedPacket.info.H263.insert2byteStartCode = false; // not used in this mode
    parsedPacket.info.H263.hasPbit              = true;  // not used in this mode
    return true;
}

bool
ModuleRTPUtility::RTPPayloadParser::ParseH2631998( RTPPayload& parsedPacket) const
{
    unsigned int h2631998HeaderLength = 2;
    if(_dataLength <= h2631998HeaderLength)
    {
        // Received empty H263 (1998) packet
        return false;
    }

    const WebRtc_UWord8 header1 = _dataPtr[0];
    const WebRtc_UWord8 header2 = _dataPtr[1];

    parsedPacket.frameType = kPFrame;

    WebRtc_UWord8 p = (header1 >> 2) & 0x01;   // picture start or a picture segment
    WebRtc_UWord8 vrc = header1 & 0x02;        // Video Redundancy Coding (VRC)
    WebRtc_UWord8 pLen = ((header1 & 0x01) << 5) + ((header2 >> 3) & 0x1f);  // Length, in bytes, of the extra picture header
    //WebRtc_UWord8 peBit = (header2 & 0x07); // number of bits that shall be ignored in the last byte of the extra picture header

    if (vrc)
    {
        return false;
    }
    if (pLen > 0)
    {
        h2631998HeaderLength += pLen;
        //get extra header

        // IMPROVEMENT we don't use the redundant picture header
    }

    if (_dataLength <= h2631998HeaderLength)
    {
        // Received empty H263 (1998) packet
        return false;
    }
    // if p == 0
    // it's a follow-on packet, hence it's not independently decodable

    const bool isH263PictureStartCode = H263PictureStartCode(_dataPtr + h2631998HeaderLength, (p>0)?true:false);
    if (isH263PictureStartCode)
    {
        // parse out real size and inform the decoder
        WebRtc_UWord16 width  = 0;
        WebRtc_UWord16 height = 0;

        if(p)
        {
            parsedPacket.frameType = GetH263FrameType(_dataPtr + h2631998HeaderLength - 2);
            GetH263FrameSize(_dataPtr + h2631998HeaderLength - 2, width, height);
        }

        parsedPacket.info.H263.hasPictureStartCode = true;
        parsedPacket.info.H263.frameWidth          = width;
        parsedPacket.info.H263.frameHeight         = height;

    }
    parsedPacket.info.H263.hasPbit              = (p>0)?true:false;
    parsedPacket.info.H263.insert2byteStartCode = (p>0)?true:false;
    parsedPacket.info.H263.data                 = _dataPtr + h2631998HeaderLength;
    parsedPacket.info.H263.dataLength           = _dataLength - h2631998HeaderLength;
    return true;
}

bool
ModuleRTPUtility::RTPPayloadParser::ParseMPEG4(
    RTPPayload& parsedPacket) const
{
    if (_dataLength <= 5)
    {
        // Received empty MPEG4 packet
        return false;
    }

    parsedPacket.frameType = kPFrame;

    if (_dataPtr[0] == 0 && _dataPtr[1] == 0 && _dataPtr[2] == 1)
    {
        parsedPacket.info.MPEG4.isFirstPacket = true;
        if (!(_dataPtr[4] & 0x40))
        {
            parsedPacket.frameType = kIFrame;
        }
    }

    parsedPacket.info.MPEG4.data       = _dataPtr;
    parsedPacket.info.MPEG4.dataLength = _dataLength;

    return true;
}

bool
ModuleRTPUtility::RTPPayloadParser::ParseVP8(RTPPayload& parsedPacket) const
{
    parsedPacket.info.VP8.hasPictureID = (_dataPtr[0] & 0x10)?true:false;
    parsedPacket.info.VP8.nonReferenceFrame = (_dataPtr[0] & 0x08)?true:false;
    parsedPacket.info.VP8.fragments = (_dataPtr[0] & 0x06)?true:false;
    parsedPacket.info.VP8.beginningOfFrame = (_dataPtr[0] & 0x01)?true:false;

    if(parsedPacket.info.VP8.fragments)
    {
        WebRtc_UWord8 fragments = (_dataPtr[0] >> 1) & 0x03;
        if( fragments == 1)
        {
            parsedPacket.info.VP8.startFragment = true;
            parsedPacket.info.VP8.stopFragment = false;
        } else if(fragments == 3)
        {
            parsedPacket.info.VP8.startFragment = false;
            parsedPacket.info.VP8.stopFragment = true;
        } else
        {
            parsedPacket.info.VP8.startFragment = false;
            parsedPacket.info.VP8.stopFragment = false;
        }
    } else
    {
        parsedPacket.info.VP8.startFragment = true;
        parsedPacket.info.VP8.stopFragment = true;
    }
    if(parsedPacket.info.VP8.hasPictureID)
    {
        WebRtc_UWord8 numBytesPictureId = 1;
        while(_dataPtr[numBytesPictureId] & 0x80)
        {
            numBytesPictureId++;
        }

        parsedPacket.frameType = (_dataPtr[1+numBytesPictureId] & 0x01) ? kPFrame : kIFrame;   // first bit after picture id

        if(!parsedPacket.info.VP8.startFragment)
        {
            // if not start fragment parse away all picture IDs
            parsedPacket.info.VP8.hasPictureID = false;
            parsedPacket.info.VP8.data       = _dataPtr+numBytesPictureId;
            parsedPacket.info.VP8.dataLength = _dataLength-numBytesPictureId;
            return true;
        }
    } else
    {
        parsedPacket.frameType = (_dataPtr[1] & 0x01) ? kPFrame : kIFrame;   // first bit after picture id
    }
    parsedPacket.info.VP8.data       = _dataPtr+1;
    parsedPacket.info.VP8.dataLength = _dataLength-1;

    return true;
}

bool
ModuleRTPUtility::RTPPayloadParser::H263PictureStartCode(const WebRtc_UWord8* data, const bool skipFirst2bytes) const
{
    // data is at least 3 bytes!

    if(skipFirst2bytes)
    {
        const WebRtc_UWord8 h3 = *(data);
        if((h3 & 0x7C) == 0 && (h3 & 0x80))
        {
            return true;
        }
    }else
    {
        // first part of the frame
        const WebRtc_UWord8 h1 = *(data);
        const WebRtc_UWord8 h2 = *(data+1);
        const WebRtc_UWord8 h3 = *(data+2);
        if(h1 == 0 && h2 == 0 && (h3 & 0x7C) == 0 && (h3 & 0x80))
        {
            return true;
        }
    }

    return false;
}

void
ModuleRTPUtility::RTPPayloadParser::GetH263FrameSize(const WebRtc_UWord8* inputVideoBuffer,
                                                WebRtc_UWord16& width,
                                                WebRtc_UWord16& height) const
{
    WebRtc_UWord8 uiH263PTypeFmt = (inputVideoBuffer[4] >> 2) & 0x07;
    if (uiH263PTypeFmt == 7)         //extended PTYPE (for QQVGA, QVGA, VGA)
    {
        const WebRtc_UWord8 uiH263PlusPTypeUFEP = ((inputVideoBuffer[4] & 0x03) << 1) + ((inputVideoBuffer[5] >> 7) & 0x01);
        if (uiH263PlusPTypeUFEP == 1)    //optional part included
        {
            WebRtc_UWord8 uiH263PlusPTypeFmt = (inputVideoBuffer[5] >> 4) & 0x07;
            if(uiH263PlusPTypeFmt == 6) //custom picture format
            {
                const WebRtc_UWord16 uiH263PlusPTypeCPFmt_PWI = ((inputVideoBuffer[9] & 0x7F) << 2) + ((inputVideoBuffer[10] >> 6) & 0x03);
                const WebRtc_UWord16 uiH263PlusPTypeCPFmt_PHI = ((inputVideoBuffer[10] & 0x1F) << 4) + ((inputVideoBuffer[11] >> 4) & 0x0F);
                width = (uiH263PlusPTypeCPFmt_PWI + 1)*4;
                width = uiH263PlusPTypeCPFmt_PHI*4;
            }
            else
            {
                switch (uiH263PlusPTypeFmt)
                {
                case 1: // SQCIF
                    width = 128;
                    height = 96;
                    break;
                case 2: // QCIF
                    width = 176;
                    height = 144;
                    break;
                case 3: // CIF
                    width = 352;
                    height = 288;
                    break;
                case 4: // 4CIF
                    width = 704;
                    height = 576;
                    break;
                case 5: // 16CIF
                    width = 1408;
                    height = 1152;
                    break;
                default:
                    assert(false);
                    break;
                }
            }
        }
    }
    else
    {
        switch (uiH263PTypeFmt)
        {
        case 1: // SQCIF
            width = 128;
            height = 96;
            break;
        case 2: // QCIF
            width = 176;
            height = 144;
            break;
        case 3: // CIF
            width = 352;
            height = 288;
            break;
        case 4: // 4CIF
            width = 704;
            height = 576;
            break;
        case 5: // 16CIF
            width = 1408;
            height = 1152;
            break;
        default:
            assert(false);
            break;
        }
    }
}

ModuleRTPUtility::FrameTypes
ModuleRTPUtility::RTPPayloadParser::GetH263FrameType(
    const WebRtc_UWord8* inputVideoBuffer) const
{
    FrameTypes frameType = kPFrame;
    const WebRtc_UWord8 uiH263PTypeFmt = (inputVideoBuffer[4] >> 2) & 0x07;
    WebRtc_UWord8 pType = 1;
    if (uiH263PTypeFmt != 7)
    {
        pType = (inputVideoBuffer[4] >> 1) & 0x01;
    }
    else
    {
        const WebRtc_UWord8 uiH263PlusPTypeUFEP = ((inputVideoBuffer[4] & 0x03) << 1) + ((inputVideoBuffer[5] >> 7) & 0x01);
        if (uiH263PlusPTypeUFEP == 1)
        {
            pType = ((inputVideoBuffer[7] >> 2) & 0x07);
        }
        else if (uiH263PlusPTypeUFEP == 0)
        {
            pType = ((inputVideoBuffer[5] >> 4) & 0x07);
        }
    }

    if (pType == 0)
    {
        frameType = kIFrame;
    }

    return frameType;
}
} // namespace webrtc
