/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_TEST_RTP_PLAYER_H_
#define WEBRTC_MODULES_VIDEO_CODING_TEST_RTP_PLAYER_H_

#include "typedefs.h"
#include "rtp_rtcp.h"
#include "critical_section_wrapper.h"
#include "video_coding_defines.h"
#include "webrtc/system_wrappers/interface/clock.h"

#include <stdio.h>
#include <list>
#include <string>

#define HDR_SIZE 8 // rtpplay packet header size in bytes
#define FIRSTLINELEN 40
#define RAND_VEC_LENGTH 4096

struct PayloadCodecTuple;

struct RawRtpPacket
{
public:
    RawRtpPacket(uint8_t* rtp_data, uint16_t rtp_length);
    ~RawRtpPacket();

    uint8_t* data;
    uint16_t length;
    int64_t resend_time_ms;
};

typedef std::list<PayloadCodecTuple*> PayloadTypeList;
typedef std::list<RawRtpPacket*> RtpPacketList;
typedef RtpPacketList::iterator RtpPacketIterator;
typedef RtpPacketList::const_iterator ConstRtpPacketIterator;

class LostPackets {
 public:
  LostPackets();
  ~LostPackets();

  void AddPacket(RawRtpPacket* packet);
  void SetResendTime(uint16_t sequenceNumber,
                     int64_t resendTime,
                     int64_t nowMs);
  RawRtpPacket* NextPacketToResend(int64_t timeNow);
  int NumberOfPacketsToResend() const;
  void SetPacketResent(uint16_t seqNo, int64_t nowMs);
  void Print() const;

 private:
  webrtc::CriticalSectionWrapper* crit_sect_;
  int loss_count_;
  FILE* debug_file_;
  RtpPacketList packets_;
};

struct PayloadCodecTuple
{
    PayloadCodecTuple(uint8_t plType, std::string codecName, webrtc::VideoCodecType type) :
        name(codecName), payloadType(plType), codecType(type) {};
    const std::string name;
    const uint8_t payloadType;
    const webrtc::VideoCodecType codecType;
};

class RTPPlayer : public webrtc::VCMPacketRequestCallback
{
public:
    RTPPlayer(const char* filename,
              webrtc::RtpData* callback,
              webrtc::Clock* clock);
    virtual ~RTPPlayer();

    int32_t Initialize(const PayloadTypeList* payloadList);
    int32_t NextPacket(const int64_t timeNow);
    uint32_t TimeUntilNextPacket() const;
    int32_t SimulatePacketLoss(float lossRate, bool enableNack = false, uint32_t rttMs = 0);
    int32_t SetReordering(bool enabled);
    int32_t ResendPackets(const uint16_t* sequenceNumbers, uint16_t length);
    void Print() const;

private:
    int32_t SendPacket(uint8_t* rtpData, uint16_t rtpLen);
    int32_t ReadPacket(int16_t* rtpdata, uint32_t* offset);
    int32_t ReadHeader();
    webrtc::Clock*     _clock;
    FILE*              _rtpFile;
    webrtc::RtpRtcp*   _rtpModule;
    uint32_t     _nextRtpTime;
    webrtc::RtpData*   _dataCallback;
    bool               _firstPacket;
    float              _lossRate;
    bool               _nackEnabled;
    LostPackets        _lostPackets;
    uint32_t     _resendPacketCount;
    int32_t      _noLossStartup;
    bool               _endOfFile;
    uint32_t     _rttMs;
    int64_t      _firstPacketRtpTime;
    int64_t      _firstPacketTimeMs;
    RawRtpPacket*      _reorderBuffer;
    bool               _reordering;
    int16_t      _nextPacket[8000];
    int32_t      _nextPacketLength;
    int                _randVec[RAND_VEC_LENGTH];
    int                _randVecPos;
};

#endif // WEBRTC_MODULES_VIDEO_CODING_TEST_RTP_PLAYER_H_
