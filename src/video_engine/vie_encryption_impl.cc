/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * vie_encryption_impl.cc
 */

#include "vie_encryption_impl.h"

// Defines
#include "vie_defines.h"
#include "vie_errors.h"
#include "vie_channel.h"
#include "vie_channel_manager.h"

#include "trace.h"
#include "vie_impl.h"

#ifdef WEBRTC_SRTP
#include "SrtpModule.h"
#endif

namespace webrtc
{

// ----------------------------------------------------------------------------
// GetInterface
// ----------------------------------------------------------------------------

ViEEncryption* ViEEncryption::GetInterface(VideoEngine* videoEngine)
{
#ifdef WEBRTC_VIDEO_ENGINE_ENCRYPTION_API
    if (videoEngine == NULL)
    {
        return NULL;
    }
    VideoEngineImpl* vieImpl = reinterpret_cast<VideoEngineImpl*> (videoEngine);
    ViEEncryptionImpl* vieEncryptionImpl = vieImpl;
    (*vieEncryptionImpl)++; // Increase ref count

    return vieEncryptionImpl;
#else
    return NULL;
#endif
}

// ----------------------------------------------------------------------------
// Release
//
// Releases the interface, i.e. reduces the reference counter. The number of
// remaining references is returned, -1 if released too many times.
// ----------------------------------------------------------------------------

int ViEEncryptionImpl::Release()
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, _instanceId,
               "ViEEncryptionImpl::Release()");
    (*this)--; // Decrease ref count

    WebRtc_Word32 refCount = GetCount();
    if (refCount < 0)
    {
        WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, _instanceId,
                   "ViEEncryptionImpl release too many times");
        SetLastError(kViEAPIDoesNotExist);
        return -1;
    }
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, _instanceId,
               "ViEEncryptionImpl reference count: %d", refCount);
    return refCount;
}

// ----------------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------------

ViEEncryptionImpl::ViEEncryptionImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, _instanceId,
               "ViEEncryptionImpl::ViEEncryptionImpl() Ctor");
}

// ----------------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------------

ViEEncryptionImpl::~ViEEncryptionImpl()
{
    WEBRTC_TRACE(webrtc::kTraceMemory, webrtc::kTraceVideo, _instanceId,
               "ViEEncryptionImpl::~ViEEncryptionImpl() Dtor");
}

// ============================================================================
// SRTP
// ============================================================================

// ----------------------------------------------------------------------------
// EnableSRTPSend
//
// ----------------------------------------------------------------------------

int ViEEncryptionImpl::EnableSRTPSend(
    const int videoChannel, const CipherTypes cipherType,
    const unsigned int cipherKeyLength, const AuthenticationTypes authType,
    const unsigned int authKeyLength, const unsigned int authTagLength,
    const SecurityLevels level, const unsigned char key[kViEMaxSrtpKeyLength],
    const bool useForRTCP)
{

    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo, _instanceId,
               "EnableSRTPSend(channel=%d, cipherType=%d, cipherKeyLength=%d, "
               "authType=%d, authKeyLength=%d, authTagLength=%d, level=%d, "
               "key=?, RTCP=%s",
               videoChannel, cipherType, cipherKeyLength, authType,
               authKeyLength, authTagLength, level,
               useForRTCP ? "true" : "false");
#ifdef WEBRTC_SRTP
    if (!IsInitialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s - ViE instance %d not initialized", __FUNCTION__,
                   _instanceId);
        return -1;
    }

    bool cipherAllZero = (kCipherNull == cipherType) && (0 == cipherKeyLength);
    bool authAllZero = (kAuthNull == authType) &&
                       (0 == authKeyLength) &&
                       (0 == authTagLength);

    // 1. For no protection all cipher and auth must be zero
    // 2. For encryption only all auth must be zero
    // 3. For authentication only all cipher must be zero
    if (((kNoProtection == level) && (!cipherAllZero || !authAllZero))
        || ((kEncryption == level) && !authAllZero)
        || ((kAuthentication == level) && !cipherAllZero))

    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceId,
                   "  Invalid input argument");
        SetLastError(kViEEncryptionInvalidSrtpParameter);
        return -1;
    }

    // 16 <= cipherKeyLength <= 256
    if (((kEncryption == level) || (kEncryptionAndAuthentication == level))
        && ((cipherKeyLength < kViEMinSrtpEncryptLength)
            || (cipherKeyLength > kViEMaxSrtpEncryptLength)))

    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceId,
                   "  Invalid cipher key length");
        SetLastError(kViEEncryptionInvalidSrtpParameter);
        return -1;
    }

    // For HMAC_SHA1 auth:
    // authKeyLength <= 20, authTagLength <= 20
    if (((kAuthentication == level) || (kEncryptionAndAuthentication == level))
        && (kAuthHmacSha1 == authType)
        && ((authKeyLength > kViEMaxSrtpAuthSh1Length)
            || (authTagLength > kViEMaxSrtpAuthSh1Length)))

    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceId,
                   "  Invalid auth key or tag length");
        SetLastError(kViEEncryptionInvalidSrtpParameter);
        return -1;
    }

    // For NULL auth:
    // authKeyLength <= 256, authTagLength <= 12
    if (((kAuthentication == level) || (kEncryptionAndAuthentication == level))
        && (kAuthNull == authType)
        && ((authKeyLength > kViEMaxSrtpKeyAuthNullLength)
            || (authTagLength > kViEMaxSrtpTagAuthNullLength)))

    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceId,
                   "  Invalid auth key or tag length");
        SetLastError(kViEEncryptionInvalidSrtpParameter);
        return -1;
    }
    if (!key)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceId,
                   "  key NULL pointer");
        SetLastError(kViEEncryptionInvalidSrtpParameter);
        return -1;
    }

    const SrtpModule::CipherTypes cipher_type =
        static_cast<const SrtpModule::CipherTypes> (cipherType);
    const SrtpModule::AuthenticationTypes auth_type =
        static_cast<const SrtpModule::AuthenticationTypes> (authType);
    const SrtpModule::SecurityLevels security_level =
        static_cast<const SrtpModule::SecurityLevels> (level);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViEEncryptionInvalidChannelId);
        return -1;
    }

    if (0 != vieChannel->EnableSRTPSend(cipher_type, cipherKeyLength, auth_type,
                                        authKeyLength, authTagLength,
                                        security_level, key, useForRTCP))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, _instanceId,
                   "Failed to configure SRTP Encryption for sending");
        SetLastError(kViEEncryptionUnknownError);
        return -1;
    }

    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo, _instanceId,
               "SRTP Enabled for sending");
    return 0;
#else    
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVoice,
               ViEId(_instanceId, videoChannel),
               "  _SRTP is undefined => _lastError = %d",
               LastErrorInternal());
    SetLastError(kViEEncryptionSrtpNotSupported);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// DisableSRTPSend
//
// ----------------------------------------------------------------------------

int ViEEncryptionImpl::DisableSRTPSend(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel),
               "DisableSRTPSend(videoChannel=%d)", videoChannel);
#ifdef WEBRTC_SRTP
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViEEncryptionInvalidChannelId);
        return -1;
    }

    if (0 != vieChannel->DisableSRTPSend())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "Failed to Disable SRTP Encryption for sending");
        SetLastError(kViEEncryptionUnknownError);
        return -1;
    }

    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel),
               "SRTP Disabled for sending");
    return 0;
#else    
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVoice,
               ViEId(_instanceId, videoChannel),
               "  _SRTP is undefined => _lastError = %d",
               LastErrorInternal());
    SetLastError(kViEEncryptionSrtpNotSupported);
    return -1;
#endif

}

// ----------------------------------------------------------------------------
// EnableSRTPReceive
//
// ----------------------------------------------------------------------------

int ViEEncryptionImpl::EnableSRTPReceive(
    const int videoChannel, const CipherTypes cipherType,
    const unsigned int cipherKeyLength, const AuthenticationTypes authType,
    const unsigned int authKeyLength, const unsigned int authTagLength,
    const SecurityLevels level,
    const unsigned char key[kViEMaxSrtpKeyLength], const bool useForRTCP)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel),
               "EnableSRTPReceive(channel=%d, cipherType=%d, "
               "cipherKeyLength=%d, authType=%d, authKeyLength=%d, "
               "authTagLength=%d, level=%d, key=?, RTCP=%s)",
               videoChannel, cipherType, cipherKeyLength, authType,
               authKeyLength, authTagLength, level,
               useForRTCP ? "true" : "false");

#ifdef WEBRTC_SRTP
    if (!IsInitialized())
    {
        SetLastError(kViENotInitialized);
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo, ViEId(_instanceId),
                   "%s - ViE instance %d not initialized", __FUNCTION__,
                   _instanceId);
        return -1;
    }

    bool cipherAllZero = (kCipherNull == cipherType) && (0 == cipherKeyLength);
    bool authAllZero = (kAuthNull == authType)
                       && (0 == authKeyLength)
                       && (0 == authTagLength);

    // 1. For no protection all cipher and auth must be zero
    // 2. For encryption only all auth must be zero
    // 3. For authentication only all cipher must be zero
    if (((kNoProtection == level) && (!cipherAllZero || !authAllZero)) ||
        ((kEncryption == level) && !authAllZero) ||
        ((kAuthentication == level) && !cipherAllZero))

    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "  Invalid input argument");
        SetLastError(kViEEncryptionInvalidSrtpParameter);
        return -1;
    }

    // 16 <= cipherKeyLength <= 256
    if (((kEncryption == level) || (kEncryptionAndAuthentication == level))
        && ((cipherKeyLength < kViEMinSrtpEncryptLength)
            || (cipherKeyLength > kViEMaxSrtpEncryptLength)))

    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "  Invalid cipher key length");
        SetLastError(kViEEncryptionInvalidSrtpParameter);
        return -1;
    }

    // For HMAC_SHA1 auth:
    // authKeyLength <= 20, authTagLength <= 20
    if (((kAuthentication == level) || (kEncryptionAndAuthentication == level))
        && (kAuthHmacSha1 == authType)
        && ((authKeyLength > kViEMaxSrtpAuthSh1Length)
            || (authTagLength > kViEMaxSrtpAuthSh1Length)))

    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "  Invalid auth key or tag length");
        SetLastError(kViEEncryptionInvalidSrtpParameter);
        return -1;
    }

    // For NULL auth:
    // authKeyLength <= 256, authTagLength <= 12
    if (((kAuthentication == level) || (kEncryptionAndAuthentication == level))
        && (kAuthNull == authType)
        && ((authKeyLength > kViEMaxSrtpKeyAuthNullLength)
            || (authTagLength > kViEMaxSrtpTagAuthNullLength)))

    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "  Invalid auth key or tag length");
        SetLastError(kViEEncryptionInvalidSrtpParameter);
        return -1;
    }

    if (!key)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "  key NULL pointer");
        SetLastError(kViEEncryptionInvalidSrtpParameter);
        return -1;
    }

    const SrtpModule::CipherTypes cipher_type =
        static_cast<const SrtpModule::CipherTypes> (cipherType);
    const SrtpModule::AuthenticationTypes auth_type =
        static_cast<const SrtpModule::AuthenticationTypes> (authType);
    const SrtpModule::SecurityLevels security_level =
        static_cast<const SrtpModule::SecurityLevels> (level);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViEEncryptionInvalidChannelId);
        return -1;
    }

    if (0 != vieChannel->EnableSRTPReceive(cipher_type, cipherKeyLength,
                                           auth_type, authKeyLength,
                                           authTagLength, security_level, key,
                                           useForRTCP))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "Failed to configure SRTP Encryption for receiving");
        SetLastError(kViEEncryptionUnknownError);
        return -1;
    }

    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel),
               "SRTP Enabled for receiving");
    return 0;

#else    
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVoice,
               ViEId(_instanceId, videoChannel),
               "  _SRTP is undefined => _lastError = %d",
               LastErrorInternal());
    SetLastError(kViEEncryptionSrtpNotSupported);
    return -1;
#endif
}

// ----------------------------------------------------------------------------
// DisableSRTPReceive
//
// ----------------------------------------------------------------------------

int ViEEncryptionImpl::DisableSRTPReceive(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel),
               "DisableSRTPReceive(videoChannel=%d)", videoChannel);

#ifdef WEBRTC_SRTP
    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViEEncryptionInvalidChannelId);
        return -1;
    }

    if (0 != vieChannel->DisableSRTPReceive())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel),
                   "Failed to Disable SRTP Encryption for receiving");
        SetLastError(kViEEncryptionUnknownError);
        return -1;
    }

    WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel), "SRTP Disabled for receiving");
    return 0;
#else    
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVoice,
               ViEId(_instanceId, videoChannel),
               "  _SRTP is undefined => _lastError = %d",
               LastErrorInternal());
    SetLastError(kViEEncryptionSrtpNotSupported);
    return -1;
#endif
}

// ============================================================================
// External encryption
// ============================================================================

// ----------------------------------------------------------------------------
// RegisterExternalEncryption
//
// ----------------------------------------------------------------------------

int ViEEncryptionImpl::RegisterExternalEncryption(const int videoChannel,
                                                  Encryption& encryption)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel),
               "RegisterExternalEncryption(videoChannel=%d)", videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViEEncryptionInvalidChannelId);
        return -1;
    }
    if (vieChannel->RegisterExternalEncryption(&encryption) != 0)
    {
        SetLastError(kViEEncryptionUnknownError);
        return -1;
    }

    return 0;
}

// ----------------------------------------------------------------------------
// DeregisterExternalEncryption
//
// ----------------------------------------------------------------------------

int ViEEncryptionImpl::DeregisterExternalEncryption(const int videoChannel)
{
    WEBRTC_TRACE(webrtc::kTraceApiCall, webrtc::kTraceVideo,
               ViEId(_instanceId, videoChannel),
               "RegisterExternalEncryption(videoChannel=%d)", videoChannel);

    ViEChannelManagerScoped cs(_channelManager);
    ViEChannel* vieChannel = cs.Channel(videoChannel);
    if (vieChannel == NULL)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideo,
                   ViEId(_instanceId, videoChannel), "%s: No channel %d",
                   __FUNCTION__, videoChannel);
        SetLastError(kViEEncryptionInvalidChannelId);
        return -1;
    }

    if (vieChannel->DeRegisterExternalEncryption() != 0)
    {
        SetLastError(kViEEncryptionUnknownError);
        return -1;
    }
    return 0;
}
} // namespace webrtc
