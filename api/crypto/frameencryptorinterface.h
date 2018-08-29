/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_CRYPTO_FRAMEENCRYPTORINTERFACE_H_
#define API_CRYPTO_FRAMEENCRYPTORINTERFACE_H_

#include "api/array_view.h"
#include "api/mediatypes.h"
#include "rtc_base/refcount.h"

namespace webrtc {

// FrameEncryptorInterface allows users to provide a custom encryption
// implementation to encrypt all outgoing audio and video frames. The user must
// also provide a FrameDecryptorInterface to be able to decrypt the frames on
// the receiving device. Note this is an additional layer of encryption in
// addition to the standard SRTP mechanism and is not intended to be used
// without it. Implementations of this interface will have the same lifetime as
// the RTPSenders it is attached to.
// This interface is not ready for production use.
class FrameEncryptorInterface : public rtc::RefCountInterface {
 public:
  virtual ~FrameEncryptorInterface() {}

  // Attempts to encrypt the provided frame. You may assume the encrypted_frame
  // will match the size returned by GetOutputSize for a give frame. You may
  // assume that the frames will arrive in order if SRTP is enabled. The ssrc
  // will simply identify which stream the frame is travelling on.
  // TODO(benwright) integrate error codes.
  virtual bool Encrypt(cricket::MediaType media_type,
                       uint32_t ssrc,
                       rtc::ArrayView<const uint8_t> frame,
                       rtc::ArrayView<uint8_t> encrypted_frame) = 0;

  // Returns the total required length in bytes for the output of the
  // encryption.
  virtual size_t GetOutputSize(cricket::MediaType media_type,
                               size_t frame_size) = 0;
};

}  // namespace webrtc

#endif  // API_CRYPTO_FRAMEENCRYPTORINTERFACE_H_
