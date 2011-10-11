/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// This sub-API supports the following functionalities:
//  - Setting send and receive codecs.
//  - Codec specific settings.
//  - Key frame signaling.
//  - Stream management settings.

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_CODEC_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_CODEC_H_

#include "common_types.h"

namespace webrtc
{

class VideoEngine;
struct VideoCodec;

// ----------------------------------------------------------------------------
//	ViEEncoderObserver
// ----------------------------------------------------------------------------

// This class declares an abstract interface for a user defined observer. It is
// up to the VideoEngine user to implement a derived class which implements the
// observer class. The observer is registered using RegisterEncoderObserver()
// and deregistered using DeregisterEncoderObserver().
class WEBRTC_DLLEXPORT ViEEncoderObserver
{
public:
    // This method is called once per second with the current encoded frame rate
    // and bit rate.
    virtual void OutgoingRate(const int videoChannel,
                              const unsigned int framerate,
                              const unsigned int bitrate) = 0;
protected:
    virtual ~ViEEncoderObserver() {};
};

// ----------------------------------------------------------------------------
//  ViEEncoderObserver
// ----------------------------------------------------------------------------

// This class declares an abstract interface for a user defined observer. It is
// up to the VideoEngine user to implement a derived class which implements the
// observer class. The observer is registered using RegisterDecoderObserver()
// and deregistered using DeregisterDecoderObserver().
class WEBRTC_DLLEXPORT ViEDecoderObserver
{
public:
    // This method is called when a new incoming stream is detected, normally
    // triggered by a new incoming SSRC or payload type.
    virtual void IncomingCodecChanged(const int videoChannel,
                                      const VideoCodec& videoCodec) = 0;

    // This method is called once per second containing the frame rate and bit
    // rate for the incoming stream
    virtual void IncomingRate(const int videoChannel,
                              const unsigned int framerate,
                              const unsigned int bitrate) = 0;

    // This method is called when the decoder needs a new key frame from encoder
    // on the sender.
    virtual void RequestNewKeyFrame(const int videoChannel) = 0;

protected:
    virtual ~ViEDecoderObserver() {};
};

// ----------------------------------------------------------------------------
//	ViECodec
// ----------------------------------------------------------------------------

class WEBRTC_DLLEXPORT ViECodec
{
public:
    // Factory for the ViECodec sub‐API and increases an internal reference
    // counter if successful. Returns NULL if the API is not supported or if
    // construction fails.
    static ViECodec* GetInterface(VideoEngine* videoEngine);

    // Releases the ViECodec sub-API and decreases an internal reference
    // counter.
    // Returns the new reference count. This value should be zero
    // for all sub-API:s before the VideoEngine object can be safely deleted.
    virtual int Release() = 0;

    // Gets the number of available codecs for the VideoEngine build.
    virtual int NumberOfCodecs() const = 0;

    // Gets a VideoCodec struct for a codec containing the default configuration
    // for that codec type.
    virtual int GetCodec(const unsigned char listNumber,
                         VideoCodec& videoCodec) const = 0;

    // Sets the send codec to use for a specified channel.
    virtual int SetSendCodec(const int videoChannel,
                             const VideoCodec& videoCodec) = 0;

    // Gets the current send codec settings.
    virtual int GetSendCodec(const int videoChannel,
                             VideoCodec& videoCodec) const = 0;

    // Prepares VideoEngine to receive a certain codec type and setting for a
    // specified payload type.
    virtual int SetReceiveCodec(const int videoChannel,
                                const VideoCodec& videoCodec) = 0;

    // Gets the current receive codec.
    virtual int GetReceiveCodec(const int videoChannel,
                                VideoCodec& videoCodec) const = 0;

    // This function is used to get codec configuration parameters to be
    // signaled from the encoder to the decoder in the call setup.
    virtual int GetCodecConfigParameters(
        const int videoChannel,
        unsigned char configParameters[kConfigParameterSize],
        unsigned char& configParametersSize) const = 0;

    // Enables advanced scaling of the captured video stream if the stream
    // differs from the send codec settings.
    virtual int SetImageScaleStatus(const int videoChannel,
                                    const bool enable) = 0;

    // Gets the number of sent key frames and number of sent delta frames.
    virtual int GetSendCodecStastistics(const int videoChannel,
                                        unsigned int& keyFrames,
                                        unsigned int& deltaFrames) const = 0;

    // Gets the number of decoded key frames and number of decoded delta frames.
    virtual int GetReceiveCodecStastistics(const int videoChannel,
                                           unsigned int& keyFrames,
                                           unsigned int& deltaFrames) const = 0;

    // Gets the number of packets discarded by the jitter buffer because they
    // arrived too late.
    virtual unsigned int GetDiscardedPackets(const int videoChannel) const = 0;

    // Enables key frame request callback in ViEDecoderObserver.
    virtual int SetKeyFrameRequestCallbackStatus(const int videoChannel,
                                                 const bool enable) = 0;

    // Enables key frame requests for detected lost packets.
    virtual int SetSignalKeyPacketLossStatus(
        const int videoChannel, const bool enable,
        const bool onlyKeyFrames = false) = 0;

    // Registers an instance of a user implementation of the ViEEncoderObserver.
    virtual int RegisterEncoderObserver(const int videoChannel,
                                        ViEEncoderObserver& observer) = 0;

    // Removes an already registered instance of ViEEncoderObserver.
    virtual int DeregisterEncoderObserver(const int videoChannel) = 0;

    // Registers an instance of a user implementation of the ViEDecoderObserver.
    virtual int RegisterDecoderObserver(const int videoChannel,
                                        ViEDecoderObserver& observer) = 0;

    // Removes an already registered instance of ViEDecoderObserver.
    virtual int DeregisterDecoderObserver(const int videoChannel) = 0;

    // This function forces the next encoded frame to be a key frame. This is
    // normally used when the remote endpoint only supports out‐band key frame
    // request.
    virtual int SendKeyFrame(const int videoChannel) = 0;

    // This function makes the decoder wait for a key frame before starting to
    // decode the incoming video stream.
    virtual int WaitForFirstKeyFrame(const int videoChannel,
                                     const bool wait) = 0;

    // This function makes VideoEngine decode all incoming H.263 key frames as
    // delta frames and all incoming delta frames as key frames.
    virtual int SetInverseH263Logic(int videoChannel, bool enable) = 0;

protected:
    ViECodec() {};
    virtual ~ViECodec() {};
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_INTERFACE_VIE_CODEC_H_
