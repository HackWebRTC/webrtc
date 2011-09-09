/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef NETEQTEST_RTPPACKET_H
#define NETEQTEST_RTPPACKET_H

#include <map>
#include <stdio.h>
#include "typedefs.h"
#include "webrtc_neteq_internal.h"

enum stereoModes {
    stereoModeMono,
    stereoModeSample1,
    stereoModeSample2,
    stereoModeFrame
};

class NETEQTEST_RTPpacket
{
public:
    NETEQTEST_RTPpacket();
    NETEQTEST_RTPpacket(const NETEQTEST_RTPpacket& copyFromMe);
    NETEQTEST_RTPpacket & operator = (const NETEQTEST_RTPpacket & other);
    bool operator !() const { return (dataLen() < 0); };
    ~NETEQTEST_RTPpacket();
    void reset();
    static int skipFileHeader(FILE *fp);
    int readFromFile(FILE *fp);
    int readFixedFromFile(FILE *fp, size_t len);
    int writeToFile(FILE *fp);
    void blockPT(WebRtc_UWord8 pt);
    //WebRtc_Word16 payloadType();
    void parseHeader();
    void parseHeader(WebRtcNetEQ_RTPInfo & rtpInfo);
    WebRtcNetEQ_RTPInfo const * RTPinfo() const;
    WebRtc_UWord8 * datagram() const;
    WebRtc_UWord8 * payload() const;
    WebRtc_Word16 payloadLen() const;
    WebRtc_Word16 dataLen() const;
    bool isParsed() const;
    bool isLost() const;
    WebRtc_UWord32 time() const { return _receiveTime; };

    WebRtc_UWord8  payloadType() const;
    WebRtc_UWord16 sequenceNumber() const;
    WebRtc_UWord32 timeStamp() const;
    WebRtc_UWord32 SSRC() const;
    WebRtc_UWord8  markerBit() const;

    int setPayloadType(WebRtc_UWord8 pt);
    int setSequenceNumber(WebRtc_UWord16 sn);
    int setTimeStamp(WebRtc_UWord32 ts);
    int setSSRC(WebRtc_UWord32 ssrc);
    int setMarkerBit(WebRtc_UWord8 mb);
    void setTime(WebRtc_UWord32 receiveTime) { _receiveTime = receiveTime; };

    int setRTPheader(const WebRtcNetEQ_RTPInfo *RTPinfo);

    int splitStereo(NETEQTEST_RTPpacket& slaveRtp, enum stereoModes mode);

    WebRtc_UWord8 *       _datagram;
    WebRtc_UWord8 *       _payloadPtr;
    int                 _memSize;
    WebRtc_Word16         _datagramLen;
    WebRtc_Word16         _payloadLen;
    WebRtcNetEQ_RTPInfo  _rtpInfo;
    bool                _rtpParsed;
    WebRtc_UWord32        _receiveTime;
    bool                _lost;
    std::map<WebRtc_UWord8, bool> _blockList;

private:
    void makeRTPheader(unsigned char* rtp_data, WebRtc_UWord8 payloadType, WebRtc_UWord16 seqNo, WebRtc_UWord32 timestamp, WebRtc_UWord32 ssrc, WebRtc_UWord8 markerBit) const;
    WebRtc_UWord16 parseRTPheader(const WebRtc_UWord8 *datagram, int datagramLen, WebRtcNetEQ_RTPInfo *RTPinfo, WebRtc_UWord8 **payloadPtr = NULL) const;
    void splitStereoSample(NETEQTEST_RTPpacket& slaveRtp, int stride);
    void splitStereoFrame(NETEQTEST_RTPpacket& slaveRtp);
};

#endif //NETEQTEST_RTPPACKET_H
