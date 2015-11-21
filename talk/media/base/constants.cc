/*
 * libjingle
 * Copyright 2012 Google Inc.
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

#include "talk/media/base/constants.h"

#include <string>

namespace cricket {

const int kVideoCodecClockrate = 90000;
const int kDataCodecClockrate = 90000;
const int kDataMaxBandwidth = 30720;  // bps

const float kHighSystemCpuThreshold = 0.85f;
const float kLowSystemCpuThreshold = 0.65f;
const float kProcessCpuThreshold = 0.10f;

const char kRtxCodecName[] = "rtx";
const char kRedCodecName[] = "red";
const char kUlpfecCodecName[] = "ulpfec";

const char kCodecParamAssociatedPayloadType[] = "apt";

const char kOpusCodecName[] = "opus";
const char kIsacCodecName[] = "isac";
const char kL16CodecName[]  = "l16";
const char kG722CodecName[] = "g722";
const char kIlbcCodecName[] = "ilbc";
const char kPcmuCodecName[] = "pcmu";
const char kPcmaCodecName[] = "pcma";
const char kCnCodecName[]   = "cn";
const char kDtmfCodecName[] = "telephone-event";

// draft-spittka-payload-rtp-opus-03.txt
const char kCodecParamPTime[] = "ptime";
const char kCodecParamMaxPTime[] = "maxptime";
const char kCodecParamMinPTime[] = "minptime";
const char kCodecParamSPropStereo[] = "sprop-stereo";
const char kCodecParamStereo[] = "stereo";
const char kCodecParamUseInbandFec[] = "useinbandfec";
const char kCodecParamUseDtx[] = "usedtx";
const char kCodecParamMaxAverageBitrate[] = "maxaveragebitrate";
const char kCodecParamMaxPlaybackRate[] = "maxplaybackrate";

const char kCodecParamSctpProtocol[] = "protocol";
const char kCodecParamSctpStreams[] = "streams";

const char kParamValueTrue[] = "1";
const char kParamValueEmpty[] = "";

const int kOpusDefaultMaxPTime = 120;
const int kOpusDefaultPTime = 20;
const int kOpusDefaultMinPTime = 3;
const int kOpusDefaultSPropStereo = 0;
const int kOpusDefaultStereo = 0;
const int kOpusDefaultUseInbandFec = 0;
const int kOpusDefaultUseDtx = 0;
const int kOpusDefaultMaxPlaybackRate = 48000;

const int kPreferredMaxPTime = 60;
const int kPreferredMinPTime = 10;
const int kPreferredSPropStereo = 0;
const int kPreferredStereo = 0;
const int kPreferredUseInbandFec = 0;

const char kRtcpFbParamNack[] = "nack";
const char kRtcpFbNackParamPli[] = "pli";
const char kRtcpFbParamRemb[] = "goog-remb";
const char kRtcpFbParamTransportCc[] = "transport-cc";

const char kRtcpFbParamCcm[] = "ccm";
const char kRtcpFbCcmParamFir[] = "fir";
const char kCodecParamMaxBitrate[] = "x-google-max-bitrate";
const char kCodecParamMinBitrate[] = "x-google-min-bitrate";
const char kCodecParamStartBitrate[] = "x-google-start-bitrate";
const char kCodecParamMaxQuantization[] = "x-google-max-quantization";
const char kCodecParamPort[] = "x-google-port";

const int kGoogleRtpDataCodecId = 101;
const char kGoogleRtpDataCodecName[] = "google-data";

const int kGoogleSctpDataCodecId = 108;
const char kGoogleSctpDataCodecName[] = "google-sctp-data";

const char kComfortNoiseCodecName[] = "CN";

const int kRtpAudioLevelHeaderExtensionDefaultId = 1;
const char kRtpAudioLevelHeaderExtension[] =
    "urn:ietf:params:rtp-hdrext:ssrc-audio-level";

const int kRtpTimestampOffsetHeaderExtensionDefaultId = 2;
const char kRtpTimestampOffsetHeaderExtension[] =
    "urn:ietf:params:rtp-hdrext:toffset";

const int kRtpAbsoluteSenderTimeHeaderExtensionDefaultId = 3;
const char kRtpAbsoluteSenderTimeHeaderExtension[] =
    "http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time";

const int kRtpVideoRotationHeaderExtensionDefaultId = 4;
const char kRtpVideoRotationHeaderExtension[] = "urn:3gpp:video-orientation";
const char kRtpVideoRotation6BitsHeaderExtensionForTesting[] =
    "urn:3gpp:video-orientation:6";

const int kRtpTransportSequenceNumberHeaderExtensionDefaultId = 5;
const char kRtpTransportSequenceNumberHeaderExtension[] =
    "http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions";

const int kNumDefaultUnsignalledVideoRecvStreams = 0;

const char kVp8CodecName[] = "VP8";
const char kVp9CodecName[] = "VP9";
const char kH264CodecName[] = "H264";

const int kDefaultVp8PlType = 100;
const int kDefaultVp9PlType = 101;
const int kDefaultH264PlType = 107;
const int kDefaultRedPlType = 116;
const int kDefaultUlpfecType = 117;
const int kDefaultRtxVp8PlType = 96;

const int kDefaultVideoMaxWidth = 640;
const int kDefaultVideoMaxHeight = 400;
const int kDefaultVideoMaxFramerate = 30;
}  // namespace cricket

