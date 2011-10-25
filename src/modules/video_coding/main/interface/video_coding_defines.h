/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_INTERFACE_VIDEO_CODING_DEFINES_H_
#define WEBRTC_MODULES_INTERFACE_VIDEO_CODING_DEFINES_H_

#include "typedefs.h"
#include "module_common_types.h"

namespace webrtc
{

// Error codes
#define VCM_FRAME_NOT_READY      3
#define VCM_REQUEST_SLI          2
#define VCM_MISSING_CALLBACK     1
#define VCM_OK                   0
#define VCM_GENERAL_ERROR       -1
#define VCM_LEVEL_EXCEEDED      -2
#define VCM_MEMORY              -3
#define VCM_PARAMETER_ERROR     -4
#define VCM_UNKNOWN_PAYLOAD     -5
#define VCM_CODEC_ERROR         -6
#define VCM_UNINITIALIZED       -7
#define VCM_NO_CODEC_REGISTERED -8
#define VCM_JITTER_BUFFER_ERROR -9
#define VCM_OLD_PACKET_ERROR    -10
#define VCM_NO_FRAME_DECODED    -11
#define VCM_ERROR_REQUEST_SLI   -12
#define VCM_NOT_IMPLEMENTED     -20

#define VCM_H263_PAYLOAD_TYPE       34
#define VCM_RED_PAYLOAD_TYPE        96
#define VCM_ULPFEC_PAYLOAD_TYPE     97
#define VCM_H263_1998_PAYLOAD_TYPE 121
#define VCM_VP8_PAYLOAD_TYPE       120
#define VCM_I420_PAYLOAD_TYPE      124

enum VCMNackProperties
{
    kNackHistoryLength = 450
};

enum VCMH263FrameDrop
{
    kDecodePFrames,
    kDropPFrames
};

enum VCMVideoProtection
{
    kProtectionNack,                // Both send-side and receive-side
    kProtectionNackSender,          // Send-side only
    kProtectionNackReceiver,        // Receive-side only
    kProtectionDualDecoder,
    kProtectionFEC,
    kProtectionNackFEC,
    kProtectionKeyOnLoss,
    kProtectionKeyOnKeyLoss,
    kProtectionPeriodicKeyFrames
};

enum VCMTemporalDecimation
{
    kBitrateOverUseDecimation,
};

struct VCMFrameCount
{
    WebRtc_UWord32 numKeyFrames;
    WebRtc_UWord32 numDeltaFrames;
};


// Callback class used for sending data ready to be packetized
class VCMPacketizationCallback
{
public:
    virtual WebRtc_Word32 SendData(
        const FrameType frameType,
        const WebRtc_UWord8 payloadType,
        const WebRtc_UWord32 timeStamp,
        const WebRtc_UWord8* payloadData,
        const WebRtc_UWord32 payloadSize,
        const RTPFragmentationHeader& fragmentationHeader,
        const RTPVideoHeader* rtpVideoHdr) = 0;
protected:
    virtual ~VCMPacketizationCallback() {}
};

// Callback class used for passing decoded frames which are ready to be rendered.
class VCMFrameStorageCallback
{
public:
    virtual WebRtc_Word32 StoreReceivedFrame(const EncodedVideoData& frameToStore) = 0;

protected:
    virtual ~VCMFrameStorageCallback() {}
};

// Callback class used for passing decoded frames which are ready to be rendered.
class VCMReceiveCallback
{
public:
    virtual WebRtc_Word32 FrameToRender(VideoFrame& videoFrame) = 0;
    virtual WebRtc_Word32 ReceivedDecodedReferenceFrame(const WebRtc_UWord64 pictureId) {return -1;}

protected:
    virtual ~VCMReceiveCallback() {}
};

// Callback class used for informing the user of the bit rate and frame rate produced by the
// encoder.
class VCMSendStatisticsCallback
{
public:
    virtual WebRtc_Word32 SendStatistics(const WebRtc_UWord32 bitRate,
                                         const WebRtc_UWord32 frameRate) = 0;

protected:
    virtual ~VCMSendStatisticsCallback() {}
};

// Callback class used for informing the user of the incoming bit rate and frame rate.
class VCMReceiveStatisticsCallback
{
public:
    virtual WebRtc_Word32 ReceiveStatistics(const WebRtc_UWord32 bitRate,
                                            const WebRtc_UWord32 frameRate) = 0;

protected:
    virtual ~VCMReceiveStatisticsCallback() {}
};

// Callback class used for telling the user about the requested amount of
// bit stream protection: FEC rate for key and delta frame;
// whether the FEC uses unequal protection (UEP) across packets,
// for key and delta frame;
// and whether NACK should be on or off.
class VCMProtectionCallback
{
public:
    virtual WebRtc_Word32 ProtectionRequest(const WebRtc_UWord8 deltaFECRate,
                                            const WebRtc_UWord8 keyFECRate,
                                            const bool deltaUseUepProtection,
                                            const bool keyUseUepProtection,
                                            const bool nack) = 0;

protected:
    virtual ~VCMProtectionCallback() {}
};

// Callback class used for telling the user about what frame type needed to continue decoding.
// Typically a key frame when the stream has been corrupted in some way.
class VCMFrameTypeCallback
{
public:
    virtual WebRtc_Word32 FrameTypeRequest(const FrameType frameType) = 0;
    virtual WebRtc_Word32 SliceLossIndicationRequest(const WebRtc_UWord64 pictureId) {return -1;}

protected:
    virtual ~VCMFrameTypeCallback() {}
};

// Callback class used for telling the user about which packet sequence numbers are currently
// missing and need to be resent.
class VCMPacketRequestCallback
{
public:
    virtual WebRtc_Word32 ResendPackets(const WebRtc_UWord16* sequenceNumbers,
                                        WebRtc_UWord16 length) = 0;

protected:
    virtual ~VCMPacketRequestCallback() {}
};

// Callback used to inform the user of the the desired resolution
// as subscribed by Media Optimization (Quality Modes)
class VCMQMSettingsCallback
{
public:
    virtual WebRtc_Word32 SetVideoQMSettings(const WebRtc_UWord32 frameRate,
                                             const WebRtc_UWord32 width,
                                             const WebRtc_UWord32 height) = 0;

protected:
    virtual ~VCMQMSettingsCallback() {}
};

} // namespace webrtc

#endif // WEBRTC_MODULES_INTERFACE_VIDEO_CODING_DEFINES_H_
