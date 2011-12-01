/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_
#define WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_

#include <cstddef> // size_t, ptrdiff_t

#include "typedefs.h"
#include "rtp_rtcp_config.h"
#include "rtp_rtcp_defines.h"

namespace webrtc {
enum RtpVideoCodecTypes
{
    kRtpNoVideo       = 0,
    kRtpH263Video     = 1,
    kRtpH2631998Video = 2,
    kRtpMpeg4Video    = 5,
    kRtpFecVideo      = 10,
    kRtpVp8Video      = 11
};

const WebRtc_UWord8 kRtpMarkerBitMask = 0x80;

namespace ModuleRTPUtility
{
    // January 1970, in NTP seconds.
    const uint32_t NTP_JAN_1970 = 2208988800UL;

    // Magic NTP fractional unit.
    const double NTP_FRAC = 4.294967296E+9;

    struct AudioPayload
    {
        WebRtc_UWord32    frequency;
        WebRtc_UWord8     channels;
        WebRtc_UWord8     bitsPerSample;
        WebRtc_UWord32    rate;
    };
    struct VideoPayload
    {
        RtpVideoCodecTypes   videoCodecType;
        WebRtc_UWord32       maxRate;
    };
    union PayloadUnion
    {
        AudioPayload Audio;
        VideoPayload Video;
    };
    struct Payload
    {
        WebRtc_Word8 name[RTP_PAYLOAD_NAME_SIZE];
        bool         audio;
        PayloadUnion typeSpecific;
    };

    // Return a clock that reads the time as reported by the operating
    // system. The returned instances are guaranteed to read the same
    // times; in particular, they return relative times relative to
    // the same base.
    RtpRtcpClock* GetSystemClock();

    // Return the current RTP timestamp from the NTP timestamp
    // returned by the specified clock.
    WebRtc_UWord32 GetCurrentRTP(RtpRtcpClock* clock, WebRtc_UWord32 freq);

    // Return the current RTP absolute timestamp.
    WebRtc_UWord32 ConvertNTPTimeToRTP(WebRtc_UWord32 NTPsec,
                                       WebRtc_UWord32 NTPfrac,
                                       WebRtc_UWord32 freq);

    // Return the time in milliseconds corresponding to the specified
    // NTP timestamp.
    WebRtc_UWord32 ConvertNTPTimeToMS(WebRtc_UWord32 NTPsec,
                                      WebRtc_UWord32 NTPfrac);

    WebRtc_UWord32 pow2(WebRtc_UWord8 exp);

    bool StringCompare(const WebRtc_Word8* str1 , const WebRtc_Word8* str2, const WebRtc_UWord32 length);

    void AssignUWord32ToBuffer(WebRtc_UWord8* dataBuffer, WebRtc_UWord32 value);
    void AssignUWord24ToBuffer(WebRtc_UWord8* dataBuffer, WebRtc_UWord32 value);
    void AssignUWord16ToBuffer(WebRtc_UWord8* dataBuffer, WebRtc_UWord16 value);

    /**
     * Converts a network-ordered two-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered two-byte buffer to convert.
     * \return Host-ordered value.
     */
    WebRtc_UWord16 BufferToUWord16(const WebRtc_UWord8* dataBuffer);

    /**
     * Converts a network-ordered three-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered three-byte buffer to convert.
     * \return Host-ordered value.
     */
    WebRtc_UWord32 BufferToUWord24(const WebRtc_UWord8* dataBuffer);

    /**
     * Converts a network-ordered four-byte input buffer to a host-ordered value.
     * \param[in] dataBuffer Network-ordered four-byte buffer to convert.
     * \return Host-ordered value.
     */
    WebRtc_UWord32 BufferToUWord32(const WebRtc_UWord8* dataBuffer);

    class RTPHeaderParser
    {
    public:
        RTPHeaderParser(const WebRtc_UWord8* rtpData,
                        const WebRtc_UWord32 rtpDataLength);
        ~RTPHeaderParser();

        bool RTCP( ) const;
        bool Parse( WebRtcRTPHeader& parsedPacket) const;

    private:
        const WebRtc_UWord8* const _ptrRTPDataBegin;
        const WebRtc_UWord8* const _ptrRTPDataEnd;
    };

    enum FrameTypes
    {
        kIFrame,    // key frame
        kPFrame         // Delta frame
    };

    struct RTPPayloadH263
    {
        // H.263 and H.263+
        bool                hasPictureStartCode;
        bool                insert2byteStartCode;
        bool                hasPbit;
        WebRtc_UWord16      frameWidth;
        WebRtc_UWord16      frameHeight;

        WebRtc_UWord8         endBits;   // ignore last end bits
        WebRtc_UWord8         startBits; // ignore first bits

        const WebRtc_UWord8*  data;
        WebRtc_UWord16        dataLength;
    };

    struct RTPPayloadMPEG4
    {
        // MPEG4
        bool                   isFirstPacket;
        const WebRtc_UWord8*   data;
        WebRtc_UWord16         dataLength;
    };
    struct RTPPayloadVP8
    {
        bool                 nonReferenceFrame;
        bool                 beginningOfPartition;
        int                  partitionID;
        bool                 hasPictureID;
        bool                 hasTl0PicIdx;
        bool                 hasTID;
        bool                 hasKeyIdx;
        int                  pictureID;
        int                  tl0PicIdx;
        int                  tID;
        int                  keyIdx;
        int                  frameWidth;
        int                  frameHeight;

        const WebRtc_UWord8*   data; 
        WebRtc_UWord16         dataLength;
    };

    union RTPPayloadUnion
    {
        RTPPayloadH263  H263;
        RTPPayloadMPEG4 MPEG4;
        RTPPayloadVP8   VP8;
    };

    struct RTPPayload
    {
        void SetType(RtpVideoCodecTypes videoType);

        RtpVideoCodecTypes  type;
        FrameTypes          frameType;
        RTPPayloadUnion     info;
    };

    // RTP payload parser
    class RTPPayloadParser
    {
    public:
        RTPPayloadParser(const RtpVideoCodecTypes payloadType,
                         const WebRtc_UWord8* payloadData,
                         const WebRtc_UWord16 payloadDataLength, // Length w/o padding.
                         const WebRtc_Word32 id);

        ~RTPPayloadParser();

        bool Parse(RTPPayload& parsedPacket) const;

    private:
        bool ParseGeneric(RTPPayload& parsedPacket) const;

        bool ParseH263(RTPPayload& parsedPacket) const;
        bool ParseH2631998(RTPPayload& parsedPacket) const;

        bool ParseMPEG4(RTPPayload& parsedPacket) const;

        bool ParseVP8(RTPPayload& parsedPacket) const;

        int ParseVP8Extension(RTPPayloadVP8 *vp8,
                              const WebRtc_UWord8 *dataPtr,
                              int dataLength) const;

        int ParseVP8PictureID(RTPPayloadVP8 *vp8,
                              const WebRtc_UWord8 **dataPtr,
                              int *dataLength,
                              int *parsedBytes) const;

        int ParseVP8Tl0PicIdx(RTPPayloadVP8 *vp8,
                              const WebRtc_UWord8 **dataPtr,
                              int *dataLength,
                              int *parsedBytes) const;

        int ParseVP8TIDAndKeyIdx(RTPPayloadVP8 *vp8,
                                 const WebRtc_UWord8 **dataPtr,
                                 int *dataLength,
                                 int *parsedBytes) const;

        int ParseVP8FrameSize(RTPPayload& parsedPacket,
                              const WebRtc_UWord8 *dataPtr,
                              int dataLength) const;

        // H.263
        bool H263PictureStartCode(const WebRtc_UWord8* data,
                                  const bool skipFirst2bytes = false) const;

        void GetH263FrameSize(const WebRtc_UWord8* inputVideoBuffer,
                              WebRtc_UWord16& width,
                              WebRtc_UWord16& height) const;

        FrameTypes GetH263FrameType(const WebRtc_UWord8* inputVideoBuffer) const;

    private:
        WebRtc_Word32               _id;
        const WebRtc_UWord8*        _dataPtr;
        const WebRtc_UWord16        _dataLength;
        const RtpVideoCodecTypes    _videoType;
    };
}
} // namespace webrtc

#endif // WEBRTC_MODULES_RTP_RTCP_SOURCE_RTP_UTILITY_H_
