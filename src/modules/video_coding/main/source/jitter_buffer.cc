/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "critical_section_wrapper.h"

#include "frame_buffer.h"
#include "inter_frame_delay.h"
#include "internal_defines.h"
#include "jitter_buffer.h"
#include "jitter_buffer_common.h"
#include "jitter_estimator.h"
#include "media_optimization.h" // hybrid NACK/FEC thresholds.
#include "packet.h"

#include "event.h"
#include "trace.h"
#include "tick_time.h"
#include "list_wrapper.h"

#include <cassert>
#include <string.h>
#include <cmath>

#if defined(_WIN32)
    // VS 2005: Don't warn for default initialized arrays. See help for more info.
    #pragma warning(disable:4351)
#endif

namespace webrtc {

// Criteria used when searching for frames in the frame buffer list
bool
VCMJitterBuffer::FrameEqualTimestamp(VCMFrameBuffer* frame,
                                     const void* timestamp)
{
    if (timestamp == NULL)
    {
        return false;
    }
    return (*static_cast<const WebRtc_UWord32*>(timestamp)) == frame->TimeStamp();
}

bool
VCMJitterBuffer::CompleteDecodableKeyFrameCriteria(VCMFrameBuffer* frame,
                                                   const void* /*notUsed*/)
{
    const VCMFrameBufferStateEnum state = frame->GetState();
    // We can decode key frame or decodable/complete frames.
    return (frame->FrameType() == kVideoFrameKey) &&
           ((state == kStateComplete)
           || (state == kStateDecodable));
}

// Constructor
VCMJitterBuffer::VCMJitterBuffer(WebRtc_Word32 vcmId, WebRtc_Word32 receiverId,
                                 bool master) :
    _vcmId(vcmId),
    _receiverId(receiverId),
    _running(false),
    _critSect(*CriticalSectionWrapper::CreateCriticalSection()),
    _master(master),
    _frameEvent(),
    _packetEvent(),
    _maxNumberOfFrames(kStartNumberOfFrames),
    _frameBuffers(),
    _frameBuffersTSOrder(),
    _lastDecodedSeqNum(),
    _lastDecodedTimeStamp(-1),
    _receiveStatistics(),
    _incomingFrameRate(0),
    _incomingFrameCount(0),
    _timeLastIncomingFrameCount(0),
    _incomingBitCount(0),
    _dropCount(0),
    _numConsecutiveOldFrames(0),
    _numConsecutiveOldPackets(0),
    _jitterEstimate(vcmId, receiverId),
    _rttMs(0),
    _nackMode(kNoNack),
    _NACKSeqNum(),
    _NACKSeqNumLength(0),
    _missingMarkerBits(false),
    _firstPacket(true)
{
    memset(_frameBuffers, 0, sizeof(_frameBuffers));
    memset(_receiveStatistics, 0, sizeof(_receiveStatistics));
    _lastDecodedSeqNum = -1;
    memset(_NACKSeqNumInternal, -1, sizeof(_NACKSeqNumInternal));

    for (int i = 0; i< kStartNumberOfFrames; i++)
    {
        _frameBuffers[i] = new VCMFrameBuffer();
    }
}

// Destructor
VCMJitterBuffer::~VCMJitterBuffer()
{
    Stop();
    for (int i = 0; i< kMaxNumberOfFrames; i++)
    {
        if (_frameBuffers[i])
        {
            delete _frameBuffers[i];
        }
    }
    delete &_critSect;
}

VCMJitterBuffer&
VCMJitterBuffer::operator=(const VCMJitterBuffer& rhs)
{
    if (this != &rhs)
    {
        _critSect.Enter();
        rhs._critSect.Enter();
        _vcmId = rhs._vcmId;
        _receiverId = rhs._receiverId;
        _running = rhs._running;
        _master = !rhs._master;
        _maxNumberOfFrames = rhs._maxNumberOfFrames;
        _lastDecodedTimeStamp = rhs._lastDecodedTimeStamp;
        _incomingFrameRate = rhs._incomingFrameRate;
        _incomingFrameCount = rhs._incomingFrameCount;
        _timeLastIncomingFrameCount = rhs._timeLastIncomingFrameCount;
        _incomingBitCount = rhs._incomingBitCount;
        _dropCount = rhs._dropCount;
        _numConsecutiveOldFrames = rhs._numConsecutiveOldFrames;
        _numConsecutiveOldPackets = rhs._numConsecutiveOldPackets;
        _jitterEstimate = rhs._jitterEstimate;
        _delayEstimate = rhs._delayEstimate;
        _waitingForCompletion = rhs._waitingForCompletion;
        _nackMode = rhs._nackMode;
        _rttMs = rhs._rttMs;
        _NACKSeqNumLength = rhs._NACKSeqNumLength;
        _missingMarkerBits = rhs._missingMarkerBits;
        _firstPacket = rhs._firstPacket;
        _lastDecodedSeqNum =  rhs._lastDecodedSeqNum;
        memcpy(_receiveStatistics, rhs._receiveStatistics,
               sizeof(_receiveStatistics));
        memcpy(_NACKSeqNumInternal, rhs._NACKSeqNumInternal,
               sizeof(_NACKSeqNumInternal));
        memcpy(_NACKSeqNum, rhs._NACKSeqNum, sizeof(_NACKSeqNum));
        for (int i = 0; i < kMaxNumberOfFrames; i++)
        {
            if (_frameBuffers[i] != NULL)
            {
                delete _frameBuffers[i];
                _frameBuffers[i] = NULL;
            }
        }
        while(_frameBuffersTSOrder.Erase(_frameBuffersTSOrder.First()) != -1)
        { }
        for (int i = 0; i < _maxNumberOfFrames; i++)
        {
            _frameBuffers[i] = new VCMFrameBuffer(*(rhs._frameBuffers[i]));
            if (_frameBuffers[i]->Length() > 0)
            {
                _frameBuffersTSOrder.Insert(_frameBuffers[i]);
            }
        }
        rhs._critSect.Leave();
        _critSect.Leave();
    }
    return *this;
}

WebRtc_UWord32
VCMJitterBuffer::LatestTimestamp(const WebRtc_UWord32 existingTimestamp,
                                 const WebRtc_UWord32 newTimestamp)
{
    bool wrap = (newTimestamp < 0x0000ffff && existingTimestamp > 0xffff0000) ||
                (newTimestamp > 0xffff0000 && existingTimestamp < 0x0000ffff);
    if (existingTimestamp > newTimestamp && !wrap)
    {
        return existingTimestamp;
    }
    else if (existingTimestamp <= newTimestamp && !wrap)
    {
        return newTimestamp;
    }
    else if (existingTimestamp < newTimestamp && wrap)
    {
        return existingTimestamp;
    }
    else
    {
        return newTimestamp;
    }
}

// Start jitter buffer
void
VCMJitterBuffer::Start()
{
    CriticalSectionScoped cs(_critSect);
    _running = true;
    _incomingFrameCount = 0;
    _incomingFrameRate = 0;
    _incomingBitCount = 0;
    _timeLastIncomingFrameCount = VCMTickTime::MillisecondTimestamp();
    memset(_receiveStatistics, 0, sizeof(_receiveStatistics));

    _numConsecutiveOldFrames = 0;
    _numConsecutiveOldPackets = 0;

    _frameEvent.Reset(); // start in a non-signaled state
    _packetEvent.Reset(); // start in a non-signaled state
    _waitingForCompletion.frameSize = 0;
    _waitingForCompletion.timestamp = 0;
    _waitingForCompletion.latestPacketTime = -1;
    _missingMarkerBits = false;
    _firstPacket = true;
    _NACKSeqNumLength = 0;
    _rttMs = 0;

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId,
                 _receiverId), "JB(0x%x): Jitter buffer: start", this);
}


// Stop jitter buffer
void
VCMJitterBuffer::Stop()
{
    _critSect.Enter();
    _running = false;
    _lastDecodedTimeStamp = -1;
    _lastDecodedSeqNum = -1;
    _frameBuffersTSOrder.Flush();
    for (int i = 0; i < kMaxNumberOfFrames; i++)
    {
        if (_frameBuffers[i] != NULL)
        {
            static_cast<VCMFrameBuffer*>(_frameBuffers[i])->SetState(kStateFree);
        }
    }

    _critSect.Leave();
    _frameEvent.Set(); // Make sure we exit from trying to get a frame to decoder
    _packetEvent.Set(); // Make sure we exit from trying to get a sequence number
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId,
                 _receiverId), "JB(0x%x): Jitter buffer: stop", this);
}

bool
VCMJitterBuffer::Running() const
{
    CriticalSectionScoped cs(_critSect);
    return _running;
}

// Flush jitter buffer
void
VCMJitterBuffer::Flush()
{
    CriticalSectionScoped cs(_critSect);
    FlushInternal();
}

// Must be called under the critical section _critSect
void
VCMJitterBuffer::FlushInternal()
{
    // Erase all frames from the sorted list and set their state to free.
    _frameBuffersTSOrder.Flush();
    for (WebRtc_Word32 i = 0; i < _maxNumberOfFrames; i++)
    {
        ReleaseFrameInternal(_frameBuffers[i]);
    }
    _lastDecodedSeqNum = -1;
    _lastDecodedTimeStamp = -1;

    _frameEvent.Reset();
    _packetEvent.Reset();

    _numConsecutiveOldFrames = 0;
    _numConsecutiveOldPackets = 0;

    // Also reset the jitter and delay estimates
    _jitterEstimate.Reset();
    _delayEstimate.Reset();

    _waitingForCompletion.frameSize = 0;
    _waitingForCompletion.timestamp = 0;
    _waitingForCompletion.latestPacketTime = -1;

    _missingMarkerBits = false;
    _firstPacket = true;

    _NACKSeqNumLength = 0;

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId,
                 _receiverId), "JB(0x%x): Jitter buffer: flush", this);
}

// Set the frame state to free and remove it from the sorted
// frame list. Must be called from inside the critical section _critSect.
void
VCMJitterBuffer::ReleaseFrameInternal(VCMFrameBuffer* frame)
{
    if (frame != NULL)
    {
        frame->SetState(kStateFree);
    }
}

// Update frame state (set as complete if conditions are met)
// Doing it here increases the degree of freedom for e.g. future
// reconstructability of separate layers. Must be called under the
// critical section _critSect.
void
VCMJitterBuffer::UpdateFrameState(VCMFrameBuffer* frame)
{
    if (frame == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId), "JB(0x%x) FB(0x%x): "
                         "UpdateFrameState NULL frame pointer", this, frame);
        return;
    }

    int length = frame->Length();
    if (_master)
    {
        // Only trace the primary jitter buffer to make it possible to parse
        // and plot the trace file.
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),
                     "JB(0x%x) FB(0x%x): Complete frame added to jitter buffer,"
                     " size:%d type %d",
                     this, frame,length,frame->FrameType());
    }

    if (length != 0 && !frame->GetCountedFrame())
    {
        // ignore Ack frames
        _incomingFrameCount++;
        frame->SetCountedFrame(true);
    }

    // Check if we should drop frame
    // an old complete frame can arrive too late
    if (_lastDecodedTimeStamp > 0 &&
            LatestTimestamp(static_cast<WebRtc_UWord32>(_lastDecodedTimeStamp),
                            frame->TimeStamp()) == _lastDecodedTimeStamp)
    {
        // Frame is older than the latest decoded frame, drop it.
        // This will trigger a release in CleanUpSizeZeroFrames later.
        frame->Reset();
        frame->SetState(kStateEmpty);

        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),
                     "JB(0x%x) FB(0x%x): Dropping old frame in Jitter buffer",
                     this, frame);
        _dropCount++;
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),
                     "Jitter buffer drop count: %d, consecutive drops: %u",
                     _dropCount, _numConsecutiveOldFrames);
        // Flush() if this happens consistently.
        _numConsecutiveOldFrames++;
        if (_numConsecutiveOldFrames > kMaxConsecutiveOldFrames)
        {
            FlushInternal();
        }
        return;
    }
    _numConsecutiveOldFrames = 0;
    frame->SetState(kStateComplete);


    // Update receive statistics. We count all layers, thus when you use layers
    // adding all key and delta frames might differ from frame count
    if (frame->IsSessionComplete())
    {
        switch (frame->FrameType())
        {
        case kVideoFrameKey:
            {
                _receiveStatistics[0]++;
                break;
            }
        case kVideoFrameDelta:
            {
                _receiveStatistics[1]++;
                break;
            }
        case kVideoFrameGolden:
            {
                _receiveStatistics[2]++;
                break;
            }
        case kVideoFrameAltRef:
            {
                _receiveStatistics[3]++;
                break;
            }
        default:
            assert(false);

        }
    }
    const VCMFrameListItem* oldFrameListItem = FindOldestCompleteContinuousFrame();
    VCMFrameBuffer* oldFrame = NULL;
    if (oldFrameListItem != NULL)
    {
        oldFrame = oldFrameListItem->GetItem();
    }

    // Only signal if this is the oldest frame.
    // Not necessary the case due to packet reordering or NACK.
    if (!WaitForNack() || (oldFrame != NULL && oldFrame == frame))
    {
        _frameEvent.Set();
    }
}


// Get received key and delta frames
WebRtc_Word32
VCMJitterBuffer::GetFrameStatistics(WebRtc_UWord32& receivedDeltaFrames,
                                    WebRtc_UWord32& receivedKeyFrames) const
{
    {
        CriticalSectionScoped cs(_critSect);
        receivedDeltaFrames = _receiveStatistics[1] + _receiveStatistics[3];
        receivedKeyFrames = _receiveStatistics[0] + _receiveStatistics[2];
    }
    return 0;
}

// Gets frame to use for this timestamp. If no match, get empty frame.
WebRtc_Word32
VCMJitterBuffer::GetFrame(const VCMPacket& packet, VCMEncodedFrame*& frame)
{
    if (!_running) // don't accept incoming packets until we are started
    {
        return VCM_UNINITIALIZED;
    }

    _critSect.Enter();
    if (LatestTimestamp(static_cast<WebRtc_UWord32>(_lastDecodedTimeStamp),
                        packet.timestamp) == _lastDecodedTimeStamp
        && packet.sizeBytes > 0)
      // Make sure that old Empty packets are inserted.
    {
        // Trying to get an old frame.
        _numConsecutiveOldPackets++;
        if (_numConsecutiveOldPackets > kMaxConsecutiveOldPackets)
        {
            FlushInternal();
        }
        _critSect.Leave();
        return VCM_OLD_PACKET_ERROR;
    }
    _numConsecutiveOldPackets = 0;

    frame = _frameBuffersTSOrder.FindFrame(FrameEqualTimestamp,
                                           &packet.timestamp);

    _critSect.Leave();

    if (frame != NULL)
    {
        return VCM_OK;
    }

    // No match, return empty frame
    frame = GetEmptyFrame();
    if (frame != NULL)
    {
        return VCM_OK;
    }
    // No free frame! Try to reclaim some...
    _critSect.Enter();
    RecycleFramesUntilKeyFrame();
    _critSect.Leave();

    frame = GetEmptyFrame();
    if (frame != NULL)
    {
        return VCM_OK;
    }
    return VCM_JITTER_BUFFER_ERROR;
}

// Deprecated! Kept for testing purposes.
VCMEncodedFrame*
VCMJitterBuffer::GetFrame(const VCMPacket& packet)
{
    VCMEncodedFrame* frame = NULL;
    if (GetFrame(packet, frame) < 0)
    {
        return NULL;
    }
    return frame;
}

// Get empty frame, creates new (i.e. increases JB size) if necessary
VCMFrameBuffer*
VCMJitterBuffer::GetEmptyFrame()
{
    if (!_running) // don't accept incoming packets until we are started
    {
        return NULL;
    }

    _critSect.Enter();

    for (int i = 0; i <_maxNumberOfFrames; ++i)
    {
        if (kStateFree == _frameBuffers[i]->GetState())
        {
            // found a free buffer
            _frameBuffers[i]->SetState(kStateEmpty);
            _critSect.Leave();
            return _frameBuffers[i];
        }
    }

    // Check if we can increase JB size
    if (_maxNumberOfFrames < kMaxNumberOfFrames)
    {
        VCMFrameBuffer* ptrNewBuffer = new VCMFrameBuffer();
        ptrNewBuffer->SetState(kStateEmpty);
        _frameBuffers[_maxNumberOfFrames] = ptrNewBuffer;
        _maxNumberOfFrames++;

        _critSect.Leave();
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
        VCMId(_vcmId, _receiverId), "JB(0x%x) FB(0x%x): Jitter buffer "
        "increased to:%d frames", this, ptrNewBuffer, _maxNumberOfFrames);
        return ptrNewBuffer;
    }
    _critSect.Leave();

    // We have reached max size, cannot increase JB size
    return NULL;
}

// Must be called under the critical section _critSect.
VCMFrameListItem*
VCMJitterBuffer::FindOldestSequenceNum() const
{
    WebRtc_UWord16 currentLow = 0xffff;
    WebRtc_UWord16 sequenceNumber = 0;
    bool first = true;
    VCMFrameListItem* frameListItem = _frameBuffersTSOrder.First();
    VCMFrameListItem* oldestFrameListItem = NULL;

    while (frameListItem != NULL)
    {
        // if we have more than one frame done since last time,
        // pick oldest
        VCMFrameBuffer* ptrFrame = NULL;
        ptrFrame = frameListItem->GetItem();
        sequenceNumber = static_cast<WebRtc_UWord16>(ptrFrame->GetLowSeqNum());

        // Find the oldest, hence lowest, using sequence numbers
        if (first)
        {
            currentLow = sequenceNumber;
            oldestFrameListItem = frameListItem;
            first = false;
        }
        else if ((currentLow < 0x0fff) && (sequenceNumber > 0xf000))
        {
            // We have a wrap and this one is older
            currentLow = sequenceNumber;
            oldestFrameListItem = frameListItem;
        }
        else if ((sequenceNumber < 0x0fff) && (currentLow > 0xf000))
        {
            // This one is after a wrap, leave as is
        }
        else if (currentLow > sequenceNumber)
        {
            // Normal case, this one is lower.
            currentLow = sequenceNumber;
            oldestFrameListItem = frameListItem;
        }
        frameListItem = _frameBuffersTSOrder.Next(frameListItem);
    }
    return oldestFrameListItem;
}

// Find oldest complete frame used for getting next frame to decode
// Must be called under critical section
// Based on sequence number
// Return NULL for lost packets
VCMFrameListItem*
VCMJitterBuffer::FindOldestCompleteContinuousFrame()
{
    // if we have more than one frame done since last time, pick oldest
    VCMFrameBuffer* oldestFrame = NULL;
    int currentLow = -1;

    VCMFrameListItem* oldestFrameItem = _frameBuffersTSOrder.First();
    if (oldestFrameItem != NULL)
    {
        oldestFrame = oldestFrameItem->GetItem();
    }
    // is the frame complete?
    if (oldestFrame != NULL)
    {
        if (kStateComplete != oldestFrame->GetState())
        {
            // Try to see if the frame is complete even though the state is not
            // complete. Can happen if markerbit is not set.
            if (!CheckForCompleteFrame(oldestFrameItem))
            {
                oldestFrame = NULL;
            }
        }
        else
        {
            // we have a complete frame
            currentLow = oldestFrame->GetLowSeqNum();
        }
    }
    if (oldestFrame == NULL)
    {
        // no complete frame no point to continue
        return NULL;
    }

    // we have a complete frame
    // check if it's continuous, otherwise we are missing a full frame
    // Use seqNum not timestamp since a full frame might be lost
    if (_lastDecodedSeqNum != -1)
    {
        // it's not enough that we have complete frame we need the layers to
        // be continuous too
        currentLow = oldestFrame->GetLowSeqNum();

        WebRtc_UWord16 lastDecodedSeqNum = (WebRtc_UWord16)_lastDecodedSeqNum;

        // We could have received the first packet of the last frame before a
        // long period if drop, that case is handled by GetNackList
        if (((WebRtc_UWord16)(lastDecodedSeqNum + 1)) != currentLow)
        {
            // Wait since we want a complete continuous frame
            return NULL;
        }
    }
    return oldestFrameItem;
}

// Check if the oldest frame is complete even though it isn't complete.
// This can happen when makerbit is not set
// Must be called under the critical section _critSect.
// Return false for lost packets
bool
VCMJitterBuffer::CheckForCompleteFrame(VCMFrameListItem* oldestFrameItem)
{
    const VCMFrameListItem*
          nextFrameItem = _frameBuffersTSOrder.Next(oldestFrameItem);
    VCMFrameBuffer* oldestFrame = NULL;
    if (oldestFrameItem != NULL)
    {
        oldestFrame = oldestFrameItem->GetItem();
    }
    if (nextFrameItem != NULL)
    {
       // We have received at least one packet from a later frame.
       if(!oldestFrame->HaveLastPacket()) // If we don't have the markerbit
        {
            VCMFrameBuffer* nextFrame = nextFrameItem->GetItem();
            // Verify that we have received the first packet of the next frame,
            // so we're not missing the last packet.
            if (nextFrame != NULL && nextFrame->GetLowSeqNum() ==
                static_cast<WebRtc_UWord16>(oldestFrame->GetHighSeqNum() + 1))
            {
                _missingMarkerBits = true;
                bool completeSession = oldestFrame->ForceSetHaveLastPacket();
                if (completeSession)
                {
                    UpdateFrameState(oldestFrame);
                }
                const VCMFrameBufferStateEnum state = oldestFrame->GetState();
                if (state == kStateComplete)
                {
                    if (oldestFrame->Length() > 0)
                    {
                        UpdateJitterAndDelayEstimates(*oldestFrame, false);
                    }
                    return true;
                }
            }
        }
    }
    return false;
}

// Call from inside the critical section _critSect
void
VCMJitterBuffer::RecycleFrame(VCMFrameBuffer* frame)
{
    if (frame == NULL)
    {
        return;
    }

    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(_vcmId, _receiverId),
                 "JB(0x%x) FB(0x%x): RecycleFrame, size:%d",
                 this, frame, frame->Length());

    ReleaseFrameInternal(frame);
}


// Calculate frame and bit rates
WebRtc_Word32
VCMJitterBuffer::GetUpdate(WebRtc_UWord32& frameRate, WebRtc_UWord32& bitRate)
{
    CriticalSectionScoped cs(_critSect);
    const WebRtc_Word64 now = VCMTickTime::MillisecondTimestamp();
    WebRtc_Word64 diff = now - _timeLastIncomingFrameCount;
    if (diff < 1000 && _incomingFrameRate > 0 && _incomingBitRate > 0)
    {
        // Make sure we report something even though less than
        // 1 second has passed since last update.
        frameRate = _incomingFrameRate;
        bitRate = _incomingBitRate;
    }
    else if (_incomingFrameCount != 0)
    {
        // We have received frame(s) since last call to this function

        // Prepare calculations
        if (diff <= 0)
        {
            diff = 1;
        }
        // we add 0.5f for rounding
        float rate = 0.5f + ((_incomingFrameCount * 1000.0f) / diff);
        if (rate < 1.0f) // don't go below 1, can crash
        {
            rate = 1.0f;
        }

        // Calculate frame rate
        // Let r be rate.
        // r(0) = 1000*framecount/delta_time.
        // (I.e. frames per second since last calculation.)
        // frameRate = r(0)/2 + r(-1)/2
        // (I.e. fr/s average this and the previous calculation.)
        frameRate = (_incomingFrameRate + (WebRtc_Word32)rate) >> 1;
        _incomingFrameRate = (WebRtc_UWord8)rate;

        // Calculate bit rate
        if (_incomingBitCount == 0)
        {
            bitRate = 0;
        }
        else
        {
            bitRate = 10 * ((100 * _incomingBitCount) /
                      static_cast<WebRtc_UWord32>(diff));
        }
        _incomingBitRate = bitRate;

        // Reset count
        _incomingFrameCount = 0;
        _incomingBitCount = 0;
        _timeLastIncomingFrameCount = now;

    }
    else
    {
        // No frames since last call
        _timeLastIncomingFrameCount = VCMTickTime::MillisecondTimestamp();
        frameRate = 0;
        bitRate = 0;
        _incomingBitRate = 0;
    }

    return 0;
}

// Returns immediately or a X ms event hang waiting for a decodable frame,
// X decided by caller
VCMEncodedFrame*
VCMJitterBuffer::GetCompleteFrameForDecoding(WebRtc_UWord32 maxWaitTimeMS)
{
    if (!_running)
    {
        return NULL;
    }

    _critSect.Enter();

    CleanUpOldFrames();
    CleanUpSizeZeroFrames();

    VCMFrameListItem* oldestFrameListItem = FindOldestCompleteContinuousFrame();
    VCMFrameBuffer* oldestFrame = NULL;
    if (oldestFrameListItem != NULL)
    {
        oldestFrame = oldestFrameListItem->GetItem();
    }

    if (oldestFrame == NULL)
    {
        if (maxWaitTimeMS == 0)
        {
            _critSect.Leave();
            return NULL;
        }
        const WebRtc_Word64 endWaitTimeMs = VCMTickTime::MillisecondTimestamp()
                                            + maxWaitTimeMS;
        WebRtc_Word64 waitTimeMs = maxWaitTimeMS;
        while (waitTimeMs > 0)
        {
            _critSect.Leave();
            const EventTypeWrapper ret =
                  _frameEvent.Wait(static_cast<WebRtc_UWord32>(waitTimeMs));
            _critSect.Enter();
            if (ret == kEventSignaled)
            {
                // are we closing down the Jitter buffer
                if (!_running)
                {
                    _critSect.Leave();
                    return NULL;
                }

                // Finding oldest frame ready for decoder, but check
                // sequence number and size
                CleanUpOldFrames();
                CleanUpSizeZeroFrames();
                oldestFrameListItem = FindOldestCompleteContinuousFrame();
                if (oldestFrameListItem != NULL)
                {
                    oldestFrame = oldestFrameListItem->GetItem();
                }
                if (oldestFrame == NULL)
                {
                    waitTimeMs = endWaitTimeMs -
                                 VCMTickTime::MillisecondTimestamp();
                }
                else
                {
                    break;
                }
            }
            else
            {
                _critSect.Leave();
                return NULL;
            }
        }
        // Inside critSect
    }
    else
    {
        // we already have a frame reset the event
        _frameEvent.Reset();
    }

    if (oldestFrame == NULL)
    {
        // Even after signaling we're still missing a complete continuous frame
        _critSect.Leave();
        return NULL;
    }

    // Update jitter estimate
    const bool retransmitted = (oldestFrame->GetNackCount() > 0);
    if (retransmitted)
    {
        _jitterEstimate.FrameNacked();
    }
    else if (oldestFrame->Length() > 0)
    {
        // Ignore retransmitted and empty frames.
        UpdateJitterAndDelayEstimates(*oldestFrame, false);
    }

    // This needs to be done before we clean up old frames,
    // otherwise we'll remove ourselves...
    oldestFrame->SetState(kStateDecoding);
    _frameBuffersTSOrder.Erase(oldestFrameListItem);
    oldestFrameListItem = NULL;

    CleanUpOldFrames();
    CleanUpSizeZeroFrames();

    _critSect.Leave();

    // We have a frame - store seqnum & timestamp
    _lastDecodedSeqNum = oldestFrame->GetHighSeqNum();
    _lastDecodedTimeStamp = oldestFrame->TimeStamp();

    return oldestFrame;
}

WebRtc_UWord32
VCMJitterBuffer::GetEstimatedJitterMS()
{
    CriticalSectionScoped cs(_critSect);
    return GetEstimatedJitterMsInternal();
}

WebRtc_UWord32
VCMJitterBuffer::GetEstimatedJitterMsInternal()
{
    WebRtc_UWord32 estimate = VCMJitterEstimator::OPERATING_SYSTEM_JITTER;

    // Compute RTT multiplier for estimation
    double rttMult = 1.0f;
    if (_nackMode == kNackHybrid && _rttMs > kLowRttNackMs)
    {
        // from here we count on FEC
        rttMult = 0.0f;
    }
    estimate += static_cast<WebRtc_UWord32>
                (_jitterEstimate.GetJitterEstimate(rttMult) + 0.5);
    if (_missingMarkerBits)
    {
        // Since the incoming packets are all missing marker bits we have to
        // wait until the first packet of the next frame arrives, before we can
        // safely say that the frame is complete. Therefore we have to
        // compensate the jitter buffer level with one frame period.
        // TODO(holmer): The timestamp diff should probably be filtered
        // (max filter) since the diff can alternate between e.g. 3000 and 6000
        // if we have a frame rate between 15 and 30 frames per seconds.
        estimate += _delayEstimate.CurrentTimeStampDiffMs();
    }
    return estimate;
}

void
VCMJitterBuffer::UpdateRtt(WebRtc_UWord32 rttMs)
{
    CriticalSectionScoped cs(_critSect);
    _rttMs = rttMs;
    _jitterEstimate.UpdateRtt(rttMs);
}

// wait for the first packet in the next frame to arrive
WebRtc_Word64
VCMJitterBuffer::GetNextTimeStamp(WebRtc_UWord32 maxWaitTimeMS,
                                  FrameType& incomingFrameType,
                                  WebRtc_Word64& renderTimeMs)
{
    if (!_running)
    {
        return -1;
    }

    _critSect.Enter();

    // Finding oldest frame ready for decoder, check sequence number and size
    CleanUpOldFrames();
    CleanUpSizeZeroFrames();

    VCMFrameBuffer* oldestFrame = _frameBuffersTSOrder.FirstFrame();

    if (oldestFrame == NULL)
    {
        _critSect.Leave();
        if (_packetEvent.Wait(maxWaitTimeMS) == kEventSignaled)
        {
            // are we closing down the Jitter buffer
            if (!_running)
            {
                return -1;
            }
            _critSect.Enter();

            CleanUpOldFrames();
            CleanUpSizeZeroFrames();
            oldestFrame = _frameBuffersTSOrder.FirstFrame();
        }
        else
        {
            _critSect.Enter();
        }
    }
    _packetEvent.Reset();

    if (oldestFrame == NULL)
    {
        _critSect.Leave();
        return -1;
    }
    // we have a frame

    // return frame type
    // All layers are assumed to have the same type
    incomingFrameType = oldestFrame->FrameType();

    renderTimeMs = oldestFrame->RenderTimeMs();

    const WebRtc_UWord32 timestamp = oldestFrame->TimeStamp();

    _critSect.Leave();

    // return current time
    return timestamp;
}

// Answers the question:
// Will the packet sequence be complete if the next frame is grabbed for
// decoding right now? That is, have we lost a frame between the last decoded
// frame and the next, or is the next
// frame missing one or more packets?
bool
VCMJitterBuffer::CompleteSequenceWithNextFrame()
{
    CriticalSectionScoped cs(_critSect);
    // Finding oldest frame ready for decoder, check sequence number and size
    CleanUpOldFrames();
    CleanUpSizeZeroFrames();

    VCMFrameListItem* oldestFrameListItem = _frameBuffersTSOrder.First();
    if (oldestFrameListItem == NULL)
    {
        // No frame found
        return true;
    }

    VCMFrameBuffer* oldestFrame = oldestFrameListItem->GetItem();
    const VCMFrameListItem* nextFrameItem =
                            _frameBuffersTSOrder.Next(oldestFrameListItem);
    if (nextFrameItem == NULL && !oldestFrame->HaveLastPacket())
    {
        // Frame not ready to be decoded.
        return true;
    }

    // See if we have lost a frame before this one.
    if (_lastDecodedSeqNum == -1)
    {
        // The sequence is not complete since we haven't yet.
        if (oldestFrame->FrameType() != kVideoFrameKey)
        {
            return false;
        }
    }
    else if (oldestFrame->GetLowSeqNum() == -1)
    {
        return false;
    }
    else if (oldestFrame->GetLowSeqNum() != (_lastDecodedSeqNum + 1)
                                             % 0x00010000)
    {
        return false;
    }
    return true;
}

// Returns immediately
VCMEncodedFrame*
VCMJitterBuffer::GetFrameForDecoding()
{
    CriticalSectionScoped cs(_critSect);
    if (!_running)
    {
        return NULL;
    }

    if (WaitForNack())
    {
        return GetFrameForDecodingNACK();
    }

    CleanUpOldFrames();
    CleanUpSizeZeroFrames();

    VCMFrameListItem* oldestFrameListItem = _frameBuffersTSOrder.First();
    if (oldestFrameListItem == NULL)
    {
        return NULL;
    }
    VCMFrameBuffer* oldestFrame = oldestFrameListItem->GetItem();

    const VCMFrameListItem* nextFrameItem =
                            _frameBuffersTSOrder.Next(oldestFrameListItem);
    if (nextFrameItem == NULL && !oldestFrame->HaveLastPacket())
    {
        return NULL;
    }

    // Incomplete frame pulled out from jitter buffer,
    // update the jitter estimate with what we currently know.
    // This frame shouldn't have been retransmitted, but if we recently
    // turned off NACK this might still happen.
    const bool retransmitted = (oldestFrame->GetNackCount() > 0);
    if (retransmitted)
    {
        _jitterEstimate.FrameNacked();
    }
    else if (oldestFrame->Length() > 0)
    {
        // Ignore retransmitted and empty frames.
        // Update with the previous incomplete frame first
        if (_waitingForCompletion.latestPacketTime >= 0)
        {
            UpdateJitterAndDelayEstimates(_waitingForCompletion, true);
        }
        // Then wait for this one to get complete
        _waitingForCompletion.frameSize = oldestFrame->Length();
        _waitingForCompletion.latestPacketTime =
                              oldestFrame->LatestPacketTimeMs();
        _waitingForCompletion.timestamp = oldestFrame->TimeStamp();
    }
    _frameBuffersTSOrder.Erase(oldestFrameListItem);
    oldestFrameListItem = NULL;

    CleanUpOldFrames();
    CleanUpSizeZeroFrames();

    // Look for previous frame loss
    VerifyAndSetPreviousFrameLost(*oldestFrame);
    // Set as decoding. Propagates the missingFrame bit.
    oldestFrame->SetState(kStateDecoding);

    // Store current seqnum & time
    _lastDecodedSeqNum = oldestFrame->GetHighSeqNum();
    _lastDecodedTimeStamp = oldestFrame->TimeStamp();

    return oldestFrame;
}

VCMEncodedFrame*
VCMJitterBuffer::GetFrameForDecodingNACK()
{
    // when we use NACK we don't release non complete frames
    // unless we have a complete key frame.
    // In hybrid mode, we may release decodable frames (non-complete)

    // Clean up old frames and empty frames
    CleanUpOldFrames();
    CleanUpSizeZeroFrames();

    // First look for a complete _continuous_ frame.
    VCMFrameListItem* oldestFrameListItem = FindOldestCompleteContinuousFrame();
    VCMFrameBuffer* oldestFrame = NULL;
    if (oldestFrameListItem != NULL)
    {
        oldestFrame = oldestFrameListItem->GetItem();
    }
    if (oldestFrame == NULL)
    {
        // If we didn't find one we're good with a complete key/decodable frame.
        oldestFrameListItem = _frameBuffersTSOrder.FindFrameListItem(
                               CompleteDecodableKeyFrameCriteria);
        if (oldestFrameListItem != NULL)
        {
            oldestFrame = oldestFrameListItem->GetItem();
        }
        if (oldestFrame == NULL)
        {
            return NULL;
        }
    }
    // Update jitter estimate
    const bool retransmitted = (oldestFrame->GetNackCount() > 0);
    if (retransmitted)
    {
        _jitterEstimate.FrameNacked();
    }
    else if (oldestFrame->Length() > 0)
    {
        // Ignore retransmitted and empty frames.
        UpdateJitterAndDelayEstimates(*oldestFrame, false);
    }

    // This needs to be done before we clean up old frames,
    // otherwise we might release the frame we want to decode right now.
    oldestFrame->SetState(kStateDecoding);
    _frameBuffersTSOrder.Erase(oldestFrameListItem);

    // Clean up old frames and empty frames
    CleanUpOldFrames();
    CleanUpSizeZeroFrames();

    // We have a complete/decodable continuous frame, decode it.
    // Store seqnum & timestamp
    _lastDecodedSeqNum = oldestFrame->GetHighSeqNum();
    _lastDecodedTimeStamp = oldestFrame->TimeStamp();

    return oldestFrame;
}

// Must be called under the critical section _critSect. Should never be called with
// retransmitted frames, they must be filtered out before this function is called.
void
VCMJitterBuffer::UpdateJitterAndDelayEstimates(VCMJitterSample& sample,
                                               bool incompleteFrame)
{
    if (sample.latestPacketTime == -1)
    {
        return;
    }
    if (incompleteFrame)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId), "Received incomplete frame "
                     "timestamp %u frame size %u at time %u",
                     sample.timestamp, sample.frameSize,
                     MaskWord64ToUWord32(sample.latestPacketTime));
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId), "Received complete frame "
                     "timestamp %u frame size %u at time %u",
                     sample.timestamp, sample.frameSize,
                     MaskWord64ToUWord32(sample.latestPacketTime));
    }
    UpdateJitterAndDelayEstimates(sample.latestPacketTime,
                                  sample.timestamp,
                                  sample.frameSize,
                                  incompleteFrame);
}

// Must be called under the critical section _critSect. Should never be
// called with retransmitted frames, they must be filtered out before this
// function is called.
void
VCMJitterBuffer::UpdateJitterAndDelayEstimates(VCMFrameBuffer& frame,
                                               bool incompleteFrame)
{
    if (frame.LatestPacketTimeMs() == -1)
    {
        return;
    }
    // No retransmitted frames should be a part of the jitter
    // estimate.
    if (incompleteFrame)
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),
                   "Received incomplete frame timestamp %u frame type %d "
                   "frame size %u at time %u, jitter estimate was %u",
                   frame.TimeStamp(), frame.FrameType(), frame.Length(),
                   MaskWord64ToUWord32(frame.LatestPacketTimeMs()),
                   GetEstimatedJitterMsInternal());
    }
    else
    {
        WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),"Received complete frame "
                     "timestamp %u frame type %d frame size %u at time %u, "
                     "jitter estimate was %u",
                     frame.TimeStamp(), frame.FrameType(), frame.Length(),
                     MaskWord64ToUWord32(frame.LatestPacketTimeMs()),
                     GetEstimatedJitterMsInternal());
    }
    UpdateJitterAndDelayEstimates(frame.LatestPacketTimeMs(), frame.TimeStamp(),
                                  frame.Length(), incompleteFrame);
}

// Must be called under the critical section _critSect. Should never be called
// with retransmitted frames, they must be filtered out before this function
// is called.
void
VCMJitterBuffer::UpdateJitterAndDelayEstimates(WebRtc_Word64 latestPacketTimeMs,
                                               WebRtc_UWord32 timestamp,
                                               WebRtc_UWord32 frameSize,
                                               bool incompleteFrame)
{
    if (latestPacketTimeMs == -1)
    {
        return;
    }
    WebRtc_Word64 frameDelay;
    // Calculate the delay estimate
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding,
                 VCMId(_vcmId, _receiverId),
                 "Packet received and sent to jitter estimate with: "
                 "timestamp=%u wallClock=%u", timestamp,
                 MaskWord64ToUWord32(latestPacketTimeMs));
    bool notReordered = _delayEstimate.CalculateDelay(timestamp,
                                                      &frameDelay,
                                                      latestPacketTimeMs);
    // Filter out frames which have been reordered in time by the network
    if (notReordered)
    {
        // Update the jitter estimate with the new samples
        _jitterEstimate.UpdateEstimate(frameDelay, frameSize, incompleteFrame);
    }
}

WebRtc_UWord16*
VCMJitterBuffer::GetNackList(WebRtc_UWord16& nackSize,bool& listExtended)
{
    return CreateNackList(nackSize,listExtended);
}

// Assume called internally with critsect
WebRtc_Word32
VCMJitterBuffer::GetLowHighSequenceNumbers(WebRtc_Word32& lowSeqNum,
                                           WebRtc_Word32& highSeqNum) const
{
    WebRtc_Word32 i = 0;
    WebRtc_Word32 seqNum = -1;

    highSeqNum = -1;
    lowSeqNum = _lastDecodedSeqNum;

    // find highest seq numbers
    for (i = 0; i < _maxNumberOfFrames; ++i)
    {
        seqNum = _frameBuffers[i]->GetHighSeqNum();

        // Ignore free / empty frames
        VCMFrameBufferStateEnum state = _frameBuffers[i]->GetState();

        if ((kStateFree != state) &&
            (kStateEmpty != state) &&
            (kStateDecoding != state) &&
             seqNum != -1)
        {
            if (highSeqNum == -1)
            {
                // first
                highSeqNum = seqNum;
            }
            else if (seqNum < 0x0fff && highSeqNum > 0xf000)
            {
                // wrap
                highSeqNum = seqNum;
            }
            else if(seqNum > 0xf000 && highSeqNum < 0x0fff)
            {
                // Do nothing since it is a wrap and this one is older
            }
            else if (seqNum > highSeqNum)
            {
                highSeqNum = seqNum;
            }
        }
    } // for
    return 0;
}


WebRtc_UWord16*
VCMJitterBuffer::CreateNackList(WebRtc_UWord16& nackSize, bool& listExtended)
{
    CriticalSectionScoped cs(_critSect);
    int i = 0;
    WebRtc_Word32 lowSeqNum = -1;
    WebRtc_Word32 highSeqNum = -1;
    listExtended = false;

    // Don't create list, if we won't wait for it
    if (!WaitForNack())
    {
        nackSize = 0;
        return NULL;
    }

    // Find the lowest (last decoded) sequence number and
    // the highest (highest sequence number of the newest frame)
    // sequence number. The nack list is a subset of the range
    // between those two numbers.
    GetLowHighSequenceNumbers(lowSeqNum, highSeqNum);

    // write a list of all seq num we have
    if (lowSeqNum == -1 || highSeqNum == -1)
    {
        // This happens if we lose the first packet, nothing is popped
        if (highSeqNum == -1)
        {
            // we have not received any packets yet
            nackSize = 0;
        }
        else
        {
            // signal that we want a key frame request to be sent
            nackSize = 0xffff;
        }
        return NULL;
    }

    int numberOfSeqNum = 0;
    if (lowSeqNum > highSeqNum)
    {
        if (lowSeqNum - highSeqNum > 0x00ff)
        {
            // wrap
            numberOfSeqNum = (0xffff-lowSeqNum) + highSeqNum + 1;
        }
    }
    else
    {
        numberOfSeqNum = highSeqNum - lowSeqNum;
    }

    if (numberOfSeqNum > kNackHistoryLength)
    {
        // Nack list is too big, flush and try to restart.
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),
                     "Nack list too large, try to find a key frame and restart "
                     "from seq: %d. Lowest seq in jb %d", highSeqNum,lowSeqNum);

        // This nack size will trigger a key request...
        bool foundIFrame = false;

        while (numberOfSeqNum > kNackHistoryLength)
        {
            foundIFrame = RecycleFramesUntilKeyFrame();

            if (!foundIFrame)
            {
                break;
            }

            // Check if we still have too many packets in JB
            lowSeqNum = -1;
            highSeqNum = -1;
            GetLowHighSequenceNumbers(lowSeqNum, highSeqNum);

            if (highSeqNum == -1)
            {
                assert(lowSeqNum != -1); // This should never happen
                // We can't calculate the nack list length...
                return NULL;
            }

            numberOfSeqNum = 0;
            if (lowSeqNum > highSeqNum)
            {
                if (lowSeqNum - highSeqNum > 0x00ff)
                {
                    // wrap
                    numberOfSeqNum = (0xffff-lowSeqNum) + highSeqNum + 1;
                    highSeqNum=lowSeqNum;
                }
            }
            else
            {
                numberOfSeqNum = highSeqNum - lowSeqNum;
            }

        } // end while

        if (!foundIFrame)
        {
            // No I frame in JB.

            // Set the last decoded sequence number to current high.
            // This is to not get a large nack list again right away
            _lastDecodedSeqNum = highSeqNum;
            // Set to trigger key frame signal
            nackSize = 0xffff;
            listExtended = true;
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, -1,
                    "\tNo key frame found, request one. _lastDecodedSeqNum[0] "
                    "%d", _lastDecodedSeqNum);
        }
        else
        {
            // We have cleaned up the jb and found a key frame
            // The function itself has set last decoded seq.
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, -1,
                    "\tKey frame found. _lastDecodedSeqNum[0] %d",
                    _lastDecodedSeqNum);
            nackSize = 0;
        }

        return NULL;
    }

    WebRtc_UWord16 seqNumberIterator = (WebRtc_UWord16)(lowSeqNum + 1);
    for (i = 0; i < numberOfSeqNum; i++)
    {
        _NACKSeqNumInternal[i] = seqNumberIterator;
        seqNumberIterator++;
    }

    // now we have a list of all sequence numbers that could have been sent

    // zero out the ones we have received
    for (i = 0; i < _maxNumberOfFrames; i++)
    {
        // loop all created frames
        // We don't need to check if frame is decoding since lowSeqNum is based
        // on _lastDecodedSeqNum
        // Ignore free frames
        VCMFrameBufferStateEnum state = _frameBuffers[i]->GetState();

        if ((kStateFree != state) &&
            (kStateEmpty != state) &&
            (kStateDecoding != state))
        {
            // Reaching thus far means we are going to update the nack list
            // When in hybrid mode, we also need to check empty frames, so as
            // not to add empty packets to the nack list
            if (_nackMode == kNackHybrid)
            {
                // build external rttScore based on RTT value
                float rttScore = 1.0f;
                _frameBuffers[i]->ZeroOutSeqNumHybrid(_NACKSeqNumInternal,
                                                      numberOfSeqNum,
                                                      rttScore);
                if (_frameBuffers[i]->IsRetransmitted() == false)
                {
                    // If no retransmission required,set the state to decodable
                    // meaning that we will not wait for NACK.
                    _frameBuffers[i]->SetState(kStateDecodable);
                }
            }
            else
            {
                // Used when the frame is being processed by the decoding thread
                // don't need to use that info in this loop.
                _frameBuffers[i]->ZeroOutSeqNum(_NACKSeqNumInternal,
                                                numberOfSeqNum);
            }
        }
    }

    // compress list
    int emptyIndex = -1;
    for (i = 0; i < numberOfSeqNum; i++)
    {
        if (_NACKSeqNumInternal[i] == -1 || _NACKSeqNumInternal[i] == -2 )
        {
            // this is empty
            if (emptyIndex == -1)
            {
                // no empty index before, remember this position
                emptyIndex = i;
            }
        }
        else
        {
            // this is not empty
            if (emptyIndex == -1)
            {
                // no empty index, continue
            }
            else
            {
                _NACKSeqNumInternal[emptyIndex] = _NACKSeqNumInternal[i];
                _NACKSeqNumInternal[i] = -1;
                emptyIndex++;
            }
        }
    } // for

    if (emptyIndex == -1)
    {
        // no empty
        nackSize = numberOfSeqNum;
    }
    else
    {
        nackSize = emptyIndex;
    }

    if (nackSize > _NACKSeqNumLength)
    {
        // Larger list: nack list was extended since the last call.
        listExtended = true;
    }

    for (WebRtc_UWord32 j = 0; j < nackSize; j++)
    {
        // Check if the list has been extended since it was last created. I.e,
        // new items have been added
        if (_NACKSeqNumLength > j && !listExtended)
        {
            WebRtc_UWord32 k = 0;
            for (k = j; k < _NACKSeqNumLength; k++)
            {
                // Found the item in the last list, i.e, no new items found yet.
                if (_NACKSeqNum[k] == (WebRtc_UWord16)_NACKSeqNumInternal[j])
                {
                   break;
                }
            }
            if (k == _NACKSeqNumLength) // New item not found in last list.
            {
                listExtended = true;
            }
        }
        else
        {
            listExtended = true;
        }
        _NACKSeqNum[j] = (WebRtc_UWord16)_NACKSeqNumInternal[j];
    }

    _NACKSeqNumLength = nackSize;

    return _NACKSeqNum;
}

// Release frame (when done with decoding), forwards to internal function
void
VCMJitterBuffer::ReleaseFrame(VCMEncodedFrame* frame)
{
    CriticalSectionScoped cs(_critSect);
    ReleaseFrameInternal(static_cast<VCMFrameBuffer*>(frame));
}

WebRtc_Word64
VCMJitterBuffer::LastPacketTime(VCMEncodedFrame* frame,
                                bool& retransmitted) const
{
    CriticalSectionScoped cs(_critSect);
    retransmitted = (static_cast<VCMFrameBuffer*>(frame)->GetNackCount() > 0);
    return static_cast<VCMFrameBuffer*>(frame)->LatestPacketTimeMs();
}

WebRtc_Word64
VCMJitterBuffer::LastDecodedTimestamp() const
{
    CriticalSectionScoped cs(_critSect);
    return _lastDecodedTimeStamp;
}

// Insert packet
// Takes crit sect, and inserts packet in frame buffer, possibly does logging
VCMFrameBufferEnum
VCMJitterBuffer::InsertPacket(VCMEncodedFrame* buffer, const VCMPacket& packet)
{
    CriticalSectionScoped cs(_critSect);
    WebRtc_Word64 nowMs = VCMTickTime::MillisecondTimestamp();
    VCMFrameBufferEnum bufferReturn = kSizeError;
    VCMFrameBufferEnum ret = kSizeError;
    VCMFrameBuffer* frame = static_cast<VCMFrameBuffer*>(buffer);

    // Empty packets may bias the jitter estimate (lacking size component),
    // therefore don't let empty packet trigger the following updates:
    if (packet.frameType != kFrameEmpty)
    {
        if (_firstPacket)
        {
            // Now it's time to start estimating jitter
            // reset the delay estimate.
            _delayEstimate.Reset();
            _firstPacket = false;
        }

        if (_waitingForCompletion.timestamp == packet.timestamp)
        {
            // This can get bad if we have a lot of duplicate packets,
            // we will then count some packet multiple times.
            _waitingForCompletion.frameSize += packet.sizeBytes;
            _waitingForCompletion.latestPacketTime = nowMs;
        }
        else if (_waitingForCompletion.latestPacketTime >= 0 &&
                 _waitingForCompletion.latestPacketTime + 2000 <= nowMs)
        {
            // A packet should never be more than two seconds late
            UpdateJitterAndDelayEstimates(_waitingForCompletion, true);
            _waitingForCompletion.latestPacketTime = -1;
            _waitingForCompletion.frameSize = 0;
            _waitingForCompletion.timestamp = 0;
        }
    }

    if (frame != NULL)
    {
        VCMFrameBufferStateEnum state = frame->GetState();
        if ((packet.sizeBytes == 0) &&
            ((state == kStateDecoding) ||
             (state == kStateEmpty &&
              _lastDecodedTimeStamp == packet.timestamp)))
       {
            // Empty packet (sizeBytes = 0), make sure we update the last
            // decoded seq num since this packet belongs either to a frame
            // being decoded (condition 1) or to a frame which was already
            // decoded and freed (condition 2). A new frame will be created
            // for the empty packet. That frame will be empty and later on
            // cleaned up.
            UpdateLastDecodedWithEmpty(packet);
        }

        // Insert packet
        // check for first packet
        // high sequence number will not be set
        bool first = frame->GetHighSeqNum() == -1;
        bufferReturn = frame->InsertPacket(packet, nowMs);
        ret = bufferReturn;

        if (bufferReturn > 0)
        {
            _incomingBitCount += packet.sizeBytes << 3;

            // Has this packet been nacked or is it about to be nacked?
            if (IsPacketRetransmitted(packet))
            {
                frame->IncrementNackCount();
            }

            // Insert each frame once on the arrival of the first packet
            // belonging to that frame (media or empty)
            if (state == kStateEmpty && first)
            {
                ret = kFirstPacket;
                _frameBuffersTSOrder.Insert(frame);
            }
        }
    }
    switch(bufferReturn)
    {
    case kStateError:
    case kTimeStampError:
    case kSizeError:
        {
            // This will trigger a release in CleanUpSizeZeroFrames
            if (frame != NULL)
            {
                frame->Reset();
                frame->SetState(kStateEmpty);
            }
            break;
        }
    case kCompleteSession:
        {
            UpdateFrameState(frame);
            // Signal that we have a received packet
            _packetEvent.Set();
            break;
        }
    case kIncomplete:
        {
            // Signal that we have a received packet
            _packetEvent.Set();
            break;
        }
    case kNoError:
    case kDuplicatePacket:
        {
            break;
        }
    default:
        {
            assert(!"JitterBuffer::InsertPacket: Undefined value");
        }
    }

    return ret;
}

void
VCMJitterBuffer::UpdateLastDecodedWithEmpty(const VCMPacket& packet)
{
    // Empty packet inserted to a frame which
    // is already decoding. Update the last decoded seq no.
    if (_lastDecodedTimeStamp == packet.timestamp &&
        (packet.seqNum > _lastDecodedSeqNum ||
        (packet.seqNum < 0x0fff && _lastDecodedSeqNum > 0xf000)))
    {
        _lastDecodedSeqNum = packet.seqNum;
    }
}

// Must be called from within _critSect
void
VCMJitterBuffer::UpdateOldJitterSample(const VCMPacket& packet)
{
    if (_waitingForCompletion.timestamp != packet.timestamp &&
        LatestTimestamp(_waitingForCompletion.timestamp, packet.timestamp) ==
        packet.timestamp)
    {
        // This is a newer frame than the one waiting for completion.
        _waitingForCompletion.frameSize = packet.sizeBytes;
        _waitingForCompletion.timestamp = packet.timestamp;
    }
    else
    {
        // This can get bad if we have a lot of duplicate packets,
        // we will then count some packet multiple times.
        _waitingForCompletion.frameSize += packet.sizeBytes;
        _jitterEstimate.UpdateMaxFrameSize(_waitingForCompletion.frameSize);
    }
}

// Must be called from within _critSect
bool
VCMJitterBuffer::IsPacketRetransmitted(const VCMPacket& packet) const
{
    if (_NACKSeqNum && _NACKSeqNumLength > 0)
    {
        for (WebRtc_UWord16 i = 0; i < _NACKSeqNumLength; i++)
        {
            if (packet.seqNum == _NACKSeqNum[i])
            {
                return true;
            }
        }
    }
    return false;
}

// Get nack status (enabled/disabled)
VCMNackMode
VCMJitterBuffer::GetNackMode() const
{
    CriticalSectionScoped cs(_critSect);
    return _nackMode;
}

// Set NACK mode
void
VCMJitterBuffer::SetNackMode(VCMNackMode mode)
{
    CriticalSectionScoped cs(_critSect);
    _nackMode = mode;
    if (_nackMode == kNoNack)
    {
        _jitterEstimate.ResetNackCount();
    }
}


// Recycle oldest frames up to a key frame, used if JB is completely full
bool
VCMJitterBuffer::RecycleFramesUntilKeyFrame()
{
    // Throw at least one frame.
    VCMFrameListItem* oldestFrameListItem = _frameBuffersTSOrder.First();
    VCMFrameBuffer* oldestFrame = NULL;
    if (oldestFrameListItem != NULL)
    {
        oldestFrame = oldestFrameListItem->GetItem();
    }

    // Remove up to oldest key frame
    bool foundIFrame = false;
    while (oldestFrameListItem != NULL && !foundIFrame)
    {
        // Throw at least one frame.
        _dropCount++;
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding,
                     VCMId(_vcmId, _receiverId),
                     "Jitter buffer drop count:%d, lowSeq %d", _dropCount,
                     oldestFrame->GetLowSeqNum());
        _frameBuffersTSOrder.Erase(oldestFrameListItem);
        RecycleFrame(oldestFrame);

        oldestFrameListItem = _frameBuffersTSOrder.First();
        if (oldestFrameListItem != NULL)
        {
            oldestFrame = oldestFrameListItem->GetItem();
        }

        if (oldestFrame != NULL)
        {
            foundIFrame = foundIFrame ||
                          (oldestFrame->FrameType() != kVideoFrameDelta);
            if (foundIFrame)
            {
                // fake the last played out to match the start of this key frame
                _lastDecodedSeqNum = (WebRtc_UWord16)((WebRtc_UWord16)
                                     (oldestFrame->GetLowSeqNum()) - 1);
                _lastDecodedTimeStamp = (WebRtc_UWord32)
                                        (oldestFrame->TimeStamp() - 1);
                break;
            }
        }
    }
    _lastDecodedSeqNum = -1;
    return foundIFrame;
}

// Must be called under the critical section _critSect.
void
VCMJitterBuffer::CleanUpOldFrames()
{
    VCMFrameListItem* oldestFrameListItem = _frameBuffersTSOrder.First();
    VCMFrameBuffer* oldestFrame = NULL;

    if (_lastDecodedTimeStamp == -1)
    {
        return;
    }

    while (oldestFrameListItem != NULL)
    {
        oldestFrame = oldestFrameListItem->GetItem();
        WebRtc_UWord32 frameTimeStamp = oldestFrame->TimeStamp();

        // Release the frame if it's older than the last decoded frame.
        if (_lastDecodedTimeStamp > -1 &&
            LatestTimestamp(static_cast<WebRtc_UWord32>(_lastDecodedTimeStamp),
                            frameTimeStamp)
                         == static_cast<WebRtc_UWord32>(_lastDecodedTimeStamp))
        {
            const WebRtc_Word32 frameLowSeqNum = oldestFrame->GetLowSeqNum();
            const WebRtc_Word32 frameHighSeqNum = oldestFrame->GetHighSeqNum();
            if (frameTimeStamp == _lastDecodedTimeStamp &&
                ((frameLowSeqNum == (_lastDecodedSeqNum + 1)) ||
                ((frameLowSeqNum == 0) &&
                (_lastDecodedSeqNum == 0xffff))))
            {
                // Could happen when sending empty packets
                // Empty packet (size = 0) belonging to last decoded frame.
                // Frame: | packet | packet | packet M=1 |
                // empty data (size = 0) | empty data (size = 0)| ...

                // This frame follows the last decoded frame
                _lastDecodedSeqNum = frameHighSeqNum;
            }

            _frameBuffersTSOrder.Erase(oldestFrameListItem);
            ReleaseFrameInternal(oldestFrame);
            oldestFrameListItem = _frameBuffersTSOrder.First();
        }
        else
        {
            break;
        }
    }
}

// This function has changed to use sequence numbers
// Using timestamp won't work since can get
// nack requests with a higher time stamp than
// the following encoded frame, but with a lower sequence number.
// Must be called under _critSect.
void
VCMJitterBuffer::CleanUpSizeZeroFrames()
{
    VCMFrameListItem* frameListItem = FindOldestSequenceNum();

    while (frameListItem != NULL)
    {
        VCMFrameBuffer* ptrTempBuffer = frameListItem->GetItem();

        // pop frame if its size zero but store seqnum
        if (ptrTempBuffer->Length() == 0)
        {
            WebRtc_Word32 frameHighSeqNum = ptrTempBuffer->GetHighSeqNum();
            if (frameHighSeqNum == -1)
            {
                // This frame has been Reset for this function to clean it up
                _frameBuffersTSOrder.Erase(frameListItem);
                ReleaseFrameInternal(ptrTempBuffer);
                frameListItem = FindOldestSequenceNum();
            }
            else
            {
                bool releaseFrame = false;
                const WebRtc_Word32 frameHighSeqNum =
                                    ptrTempBuffer->GetHighSeqNum();
                const WebRtc_Word32 frameLowSeqNum =
                                    ptrTempBuffer->GetLowSeqNum();

                if ((frameLowSeqNum == (_lastDecodedSeqNum + 1)) ||
                    // Frame is next in line
                    ((frameLowSeqNum == 0) && (_lastDecodedSeqNum== 0xffff)))
                {
                    // This frame follows the last decoded frame, release it.
                    _lastDecodedSeqNum = frameHighSeqNum;
                    releaseFrame = true;
                }
                // If frameHighSeqNum < _lastDecodedSeqNum
                // but need to take wrap into account.
                else if (frameHighSeqNum < _lastDecodedSeqNum)
                {
                    if (frameHighSeqNum < 0x0fff &&
                        _lastDecodedSeqNum> 0xf000)
                    {
                        // Wrap, we don't want release this one. It's newer...
                    }
                    else
                    {
                        // This frame has lower seq than last decoded,
                        // and we have no wrap -> it's older.
                        releaseFrame = true;
                    }
                }
                else if (frameHighSeqNum > _lastDecodedSeqNum &&
                        _lastDecodedSeqNum < 0x0fff &&
                        frameHighSeqNum > 0xf000)
                {
                    // Higher seq than last decoded,
                    // but last decoded has recently wrapped.
                    releaseFrame = true;
                }

                if (releaseFrame)
                {
                    _frameBuffersTSOrder.Erase(frameListItem);
                    ReleaseFrameInternal(ptrTempBuffer);
                    frameListItem = FindOldestSequenceNum();
                }
                else
                {
                    // We couldn't release this one and we're using nack,
                    // stop trying...
                    frameListItem = NULL;
                }
            }
        }
        else
        {
            // we have a length
            break;
        }
    }
}

// Used in GetFrameForDecoding
void
VCMJitterBuffer::VerifyAndSetPreviousFrameLost(VCMFrameBuffer& frame)
{
    frame.MakeSessionDecodable(); // make sure the session can be decoded.
    if (_lastDecodedSeqNum == -1)
    {
        // First frame
        frame.SetPreviousFrameLoss();
    }
    else if ((WebRtc_UWord16)frame.GetLowSeqNum() !=
             ((WebRtc_UWord16)_lastDecodedSeqNum + (WebRtc_UWord16)1))
    {
        // Frame loss
        frame.SetPreviousFrameLoss();
    }
}

bool
VCMJitterBuffer::WaitForNack()
{
     // NACK disabled -> can't wait
     if (_nackMode == kNoNack)
     {
         return false;
     }
     // NACK only -> always wait
     else if (_nackMode == kNackInfinite)
     {
         return true;
     }
     // else: hybrid mode, evaluate
     // RTT high, don't wait
     if (_rttMs >= kHighRttNackMs)
     {
         return false;
     }
     // Either NACK only or hybrid
     return true;
}


}
