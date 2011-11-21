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
 * vie_encryption_impl.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_ENCRYPTION_IMPL_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_ENCRYPTION_IMPL_H_

#include "vie_defines.h"

#include "typedefs.h"
#include "vie_ref_count.h"
#include "vie_encryption.h"
#include "vie_shared_data.h"

namespace webrtc
{

// ----------------------------------------------------------------------------
//	ViEEncryptionImpl
// ----------------------------------------------------------------------------

class ViEEncryptionImpl : public virtual ViESharedData,
    public ViEEncryption,
    public ViERefCount
{
public:
    virtual int Release();

    // SRTP calls
    virtual int EnableSRTPSend(const int videoChannel,
                               const CipherTypes cipherType,
                               const unsigned int cipherKeyLength,
                               const AuthenticationTypes authType,
                               const unsigned int authKeyLength,
                               const unsigned int authTagLength,
                               const SecurityLevels level,
                               const unsigned char key[kViEMaxSrtpKeyLength],
                               const bool useForRTCP);

    virtual int DisableSRTPSend(const int videoChannel);

    virtual int EnableSRTPReceive(const int videoChannel,
                                  const CipherTypes cipherType,
                                  const unsigned int cipherKeyLength,
                                  const AuthenticationTypes authType,
                                  const unsigned int authKeyLength,
                                  const unsigned int authTagLength,
                                  const SecurityLevels level,
                                  const unsigned char key[kViEMaxSrtpKeyLength],
                                  const bool useForRTCP);

    virtual int DisableSRTPReceive(const int videoChannel);

    // External encryption
    virtual int RegisterExternalEncryption(const int videoChannel,
                                           Encryption& encryption);

    virtual int DeregisterExternalEncryption(const int videoChannel);

protected:
    ViEEncryptionImpl();
    virtual ~ViEEncryptionImpl();
};
} // namespace webrtc
#endif  // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_ENCRYPTION_IMPL_H_
