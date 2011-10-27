/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_coding.h"
#include "trace.h"
#include "encoded_frame.h"
#include "internal_defines.h"
#include "receiver.h"
#include "tick_time.h"

#include <assert.h>

namespace webrtc {

VCMReceiver::VCMReceiver(VCMTiming& timing,
                         WebRtc_Word32 vcmId,
                         WebRtc_Word32 receiverId,
                         bool master)
:
_critSect(*CriticalSectionWrapper::CreateCriticalSection()),
_vcmId(vcmId),
_receiverId(receiverId),
_master(master),
_jitterBuffer(vcmId, receiverId, master),
_timing(timing),
_renderWaitEvent(*new VCMEvent()),
_state(kPassive)
{
}

VCMReceiver::~VCMReceiver()
{
    _renderWaitEvent.Set();
    delete &_renderWaitEvent;
    delete &_critSect;
}

WebRtc_Word32
VCMReceiver::Initialize()
{
    CriticalSectionScoped cs(_critSect);
    if (!_jitterBuffer.Running())
    {
        _jitterBuffer.Start();
    }
    else
    {
        _jitterBuffer.Flush();
    }
    _renderWaitEvent.Reset();
    if (_master)
    {
        _state = kReceiving;
    }
    else
    {
        _state = kPassive;
        SetNackMode(kNoNack);
    }
    return VCM_OK;
}

void VCMReceiver::UpdateRtt(WebRtc_UWord32 rtt)
{
    _jitterBuffer.UpdateRtt(rtt);
}

WebRtc_Word32
VCMReceiver::InsertPacket(const VCMPacket& packet,
                          WebRtc_UWord16 frameWidth,
                          WebRtc_UWord16 frameHeight)
{
    // Find an empty frame
    VCMEncodedFrame *buffer = NULL;
    const WebRtc_Word32 error = _jitterBuffer.GetFrame(packet, buffer);
    if (error == VCM_OLD_PACKET_ERROR)
    {
        return VCM_OK;
    }
    else if (error < 0)
    {
        return error;
    }

    {
        CriticalSectionScoped cs(_critSect);

        if (frameWidth && frameHeight)
        {
            buffer->SetEncodedSize(static_cast<WebRtc_UWord32>(frameWidth),
                                   static_cast<WebRtc_UWord32>(frameHeight));
        }

        if (_master)
        {
            // Only trace the primary receiver to make it possible
            // to parse and plot the trace file.
            WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId, _receiverId),
                       "Packet seqNo %u of frame %u at %u",
                       packet.seqNum, packet.timestamp,
                       MaskWord64ToUWord32(VCMTickTime::MillisecondTimestamp()));
        }

        const WebRtc_Word64 nowMs = VCMTickTime::MillisecondTimestamp();

        WebRtc_Word64 renderTimeMs = _timing.RenderTimeMs(packet.timestamp, nowMs);

        if (renderTimeMs < 0)
        {
            // Render time error. Assume that this is due to some change in
            // the incoming video stream and reset the JB and the timing.
            _jitterBuffer.Flush();
            _timing.Reset();
            return VCM_OK;
        }
        else if (renderTimeMs < nowMs - kMaxVideoDelayMs)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding, VCMId(_vcmId, _receiverId),
                "This frame should have been rendered more than %u ms ago."
                "Flushing jitter buffer and resetting timing.", kMaxVideoDelayMs);
            _jitterBuffer.Flush();
            _timing.Reset();
            return VCM_OK;
        }
        else if (_timing.TargetVideoDelay() > kMaxVideoDelayMs)
        {
            WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideoCoding, VCMId(_vcmId, _receiverId),
                "More than %u ms target delay. Flushing jitter buffer and resetting timing.",
                kMaxVideoDelayMs);
            _jitterBuffer.Flush();
            _timing.Reset();
            return VCM_OK;
        }

        // First packet received belonging to this frame.
        if (buffer->Length() == 0)
        {
            const WebRtc_Word64 nowMs = VCMTickTime::MillisecondTimestamp();
            if (_master)
            {
                // Only trace the primary receiver to make it possible to parse and plot the trace file.
                WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCoding, VCMId(_vcmId, _receiverId),
                           "First packet of frame %u at %u", packet.timestamp,
                           MaskWord64ToUWord32(nowMs));
            }
            renderTimeMs = _timing.RenderTimeMs(packet.timestamp, nowMs);
            if (renderTimeMs >= 0)
            {
                buffer->SetRenderTime(renderTimeMs);
            }
            else
            {
                buffer->SetRenderTime(nowMs);
            }
        }

        // Insert packet into jitter buffer
        // both media and empty packets
        const VCMFrameBufferEnum ret = _jitterBuffer.InsertPacket(buffer, packet);

        if (ret < 0)
        {
            WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCoding, VCMId(_vcmId, _receiverId),
                       "Error inserting packet seqNo=%u, timeStamp=%u",
                       packet.seqNum, packet.timestamp);
            return VCM_JITTER_BUFFER_ERROR;
        }
    }
    return VCM_OK;
}

VCMEncodedFrame*
VCMReceiver::FrameForDecoding(WebRtc_UWord16 maxWaitTimeMs, WebRtc_Word64& nextRenderTimeMs,
                              bool renderTiming, VCMReceiver* dualReceiver)
{
    // No need to enter the critical section here since the jitter buffer
    // is thread-safe.
    FrameType incomingFrameType = kVideoFrameDelta;
    nextRenderTimeMs = -1;
    const WebRtc_Word64 startTimeMs = VCMTickTime::MillisecondTimestamp();
    WebRtc_Word64 ret = _jitterBuffer.GetNextTimeStamp(maxWaitTimeMs,
                                                       incomingFrameType,
                                                       nextRenderTimeMs);
    if (ret < 0)
    {
        // No timestamp in jitter buffer at the moment
        return NULL;
    }
    const WebRtc_UWord32 timeStamp = static_cast<WebRtc_UWord32>(ret);

    // Update the timing
    _timing.SetRequiredDelay(_jitterBuffer.GetEstimatedJitterMS());
    _timing.UpdateCurrentDelay(timeStamp);

    const WebRtc_Word32 tempWaitTime = maxWaitTimeMs -
            static_cast<WebRtc_Word32>(VCMTickTime::MillisecondTimestamp() - startTimeMs);
    WebRtc_UWord16 newMaxWaitTime = static_cast<WebRtc_UWord16>(VCM_MAX(tempWaitTime, 0));

    VCMEncodedFrame* frame = NULL;

    if (renderTiming)
    {
        frame = FrameForDecoding(newMaxWaitTime, nextRenderTimeMs, dualReceiver);
    }
    else
    {
        frame = FrameForRendering(newMaxWaitTime, nextRenderTimeMs, dualReceiver);
    }

    if (frame != NULL)
    {
        bool retransmitted = false;
        const WebRtc_Word64 lastPacketTimeMs =
                _jitterBuffer.LastPacketTime(frame, retransmitted);
        if (lastPacketTimeMs >= 0 && !retransmitted)
        {
            // We don't want to include timestamps which have suffered from retransmission
            // here, since we compensate with extra retransmission delay within
            // the jitter estimate.
            _timing.IncomingTimestamp(timeStamp, lastPacketTimeMs);
        }
        if (dualReceiver != NULL)
        {
            dualReceiver->UpdateState(*frame);
        }
    }
    return frame;
}

VCMEncodedFrame*
VCMReceiver::FrameForDecoding(WebRtc_UWord16 maxWaitTimeMs,
                              WebRtc_Word64 nextRenderTimeMs,
                              VCMReceiver* dualReceiver)
{
    // How long can we wait until we must decode the next frame
    WebRtc_UWord32 waitTimeMs = _timing.MaxWaitingTime(nextRenderTimeMs,
                                          VCMTickTime::MillisecondTimestamp());

    // Try to get a complete frame from the jitter buffer
    VCMEncodedFrame* frame = _jitterBuffer.GetCompleteFrameForDecoding(0);

    if (frame == NULL && maxWaitTimeMs == 0 && waitTimeMs > 0)
    {
        // If we're not allowed to wait for frames to get complete we must
        // calculate if it's time to decode, and if it's not we will just return
        // for now.
        return NULL;
    }

    if (frame == NULL)
    {
        // Wait for a complete frame
        frame = _jitterBuffer.GetCompleteFrameForDecoding(maxWaitTimeMs);
    }
    if (frame == NULL)
    {
        // Get an incomplete frame
        if (_timing.MaxWaitingTime(nextRenderTimeMs,
                                   VCMTickTime::MillisecondTimestamp()) > 0)
        {
            // Still time to wait for a complete frame
            return NULL;
        }

        // No time left to wait, we must decode this frame now.
        const bool dualReceiverEnabledAndPassive = (dualReceiver != NULL &&
                                     dualReceiver->State() == kPassive &&
                                     dualReceiver->NackMode() == kNackInfinite);
        if (dualReceiverEnabledAndPassive &&
            !_jitterBuffer.CompleteSequenceWithNextFrame())
        {
            // Jitter buffer state might get corrupt with this frame.
            dualReceiver->CopyJitterBufferStateFromReceiver(*this);
        }

        frame = _jitterBuffer.GetFrameForDecoding();
    }
    return frame;
}

VCMEncodedFrame*
VCMReceiver::FrameForRendering(WebRtc_UWord16 maxWaitTimeMs,
                               WebRtc_Word64 nextRenderTimeMs,
                               VCMReceiver* dualReceiver)
{
    // How long MUST we wait until we must decode the next frame. This is different for the case
    // where we have a renderer which can render at a specified time. Here we must wait as long
    // as possible before giving the frame to the decoder, which will render the frame as soon
    // as it has been decoded.
    WebRtc_UWord32 waitTimeMs = _timing.MaxWaitingTime(nextRenderTimeMs,
                                                       VCMTickTime::MillisecondTimestamp());
    if (maxWaitTimeMs < waitTimeMs)
    {
        // If we're not allowed to wait until the frame is supposed to be rendered
        // we will have to return NULL for now.
        return NULL;
    }
    // Wait until it's time to render
    _renderWaitEvent.Wait(waitTimeMs);

    // Get a complete frame if possible
    VCMEncodedFrame* frame = _jitterBuffer.GetCompleteFrameForDecoding(0);

    if (frame == NULL)
    {
        // Get an incomplete frame
        const bool dualReceiverEnabledAndPassive = dualReceiver != NULL &&
                                                   dualReceiver->State() == kPassive &&
                                                   dualReceiver->NackMode() == kNackInfinite;
        if (dualReceiverEnabledAndPassive && !_jitterBuffer.CompleteSequenceWithNextFrame())
        {
            // Jitter buffer state might get corrupt with this frame.
            dualReceiver->CopyJitterBufferStateFromReceiver(*this);
        }

        frame = _jitterBuffer.GetFrameForDecoding();
    }
    return frame;
}

void
VCMReceiver::ReleaseFrame(VCMEncodedFrame* frame)
{
    _jitterBuffer.ReleaseFrame(frame);
}

WebRtc_Word32
VCMReceiver::ReceiveStatistics(WebRtc_UWord32& bitRate, WebRtc_UWord32& frameRate)
{
    const WebRtc_Word32 ret = _jitterBuffer.GetUpdate(frameRate, bitRate);
    bitRate /= 1000; // Should be in kbps
    return ret;
}

WebRtc_Word32
VCMReceiver::ReceivedFrameCount(VCMFrameCount& frameCount) const
{
    return _jitterBuffer.GetFrameStatistics(frameCount.numDeltaFrames,
                                            frameCount.numKeyFrames);
}

WebRtc_UWord32 VCMReceiver::DiscardedPackets() const {
  return _jitterBuffer.DiscardedPackets();
}

void
VCMReceiver::SetNackMode(VCMNackMode nackMode)
{
    CriticalSectionScoped cs(_critSect);
    _jitterBuffer.SetNackMode(nackMode);
    if (!_master)
    {
        _state = kPassive; // The dual decoder defaults to passive
    }
}

VCMNackMode
VCMReceiver::NackMode() const
{
    CriticalSectionScoped cs(_critSect);
    return _jitterBuffer.GetNackMode();
}

VCMNackStatus
VCMReceiver::NackList(WebRtc_UWord16* nackList, WebRtc_UWord16& size)
{
    bool extended = false;
    WebRtc_UWord16 nackListSize = 0;
    WebRtc_UWord16* internalNackList = _jitterBuffer.GetNackList(nackListSize, extended);
    if (internalNackList == NULL && nackListSize == 0xffff)
    {
        // This combination is used to trigger key frame requests.
        size = 0;
        return kNackKeyFrameRequest;
    }
    if (nackListSize > size)
    {
        size = nackListSize;
        return kNackNeedMoreMemory;
    }
    memcpy(nackList, internalNackList, nackListSize * sizeof(WebRtc_UWord16));
    size = nackListSize;
    return kNackOk;
}

// Decide whether we should change decoder state. This should be done if the dual decoder
// has caught up with the decoder decoding with packet losses.
bool
VCMReceiver::DualDecoderCaughtUp(VCMEncodedFrame* dualFrame, VCMReceiver& dualReceiver) const
{
    if (dualFrame == NULL)
    {
        return false;
    }
    if (_jitterBuffer.LastDecodedTimestamp() == dualFrame->TimeStamp())
    {
        dualReceiver.UpdateState(kWaitForPrimaryDecode);
        return true;
    }
    return false;
}

void
VCMReceiver::CopyJitterBufferStateFromReceiver(const VCMReceiver& receiver)
{
    _jitterBuffer = receiver._jitterBuffer;
}

VCMReceiverState
VCMReceiver::State() const
{
    CriticalSectionScoped cs(_critSect);
    return _state;
}

void
VCMReceiver::UpdateState(VCMReceiverState newState)
{
    CriticalSectionScoped cs(_critSect);
    assert(!(_state == kPassive && newState == kWaitForPrimaryDecode));
//    assert(!(_state == kReceiving && newState == kPassive));
    _state = newState;
}

void
VCMReceiver::UpdateState(VCMEncodedFrame& frame)
{
    if (_jitterBuffer.GetNackMode() == kNoNack)
    {
        // Dual decoder mode has not been enabled.
        return;
    }
    // Update the dual receiver state
    if (frame.Complete() && frame.FrameType() == kVideoFrameKey)
    {
        UpdateState(kPassive);
    }
    if (State() == kWaitForPrimaryDecode &&
        frame.Complete() && !frame.MissingFrame())
    {
        UpdateState(kPassive);
    }
    if (frame.MissingFrame() || !frame.Complete())
    {
        // State was corrupted, enable dual receiver.
        UpdateState(kReceiving);
    }
}

}
