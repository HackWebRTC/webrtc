/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_RTP_RTCP_RTP_HEADER_EXTENSION_H_
#define WEBRTC_MODULES_RTP_RTCP_RTP_HEADER_EXTENSION_H_

#include "map_wrapper.h"
#include "rtp_rtcp_defines.h"
#include "typedefs.h"

namespace webrtc {

enum {RTP_ONE_BYTE_HEADER_EXTENSION = 0xbede};

enum ExtensionLength {
   RTP_ONE_BYTE_HEADER_LENGTH_IN_BYTES = 4,
   TRANSMISSION_TIME_OFFSET_LENGTH_IN_BYTES = 4
};

struct HeaderExtension {
  HeaderExtension(RTPExtensionType extension_type)
    : type(extension_type),
      length(0) {
     if (type == kRtpExtensionTransmissionTimeOffset) {
       length = TRANSMISSION_TIME_OFFSET_LENGTH_IN_BYTES;
     }
   }

   const RTPExtensionType type;
   WebRtc_UWord8 length;
};

class RtpHeaderExtensionMap {
 public:
  RtpHeaderExtensionMap();
  ~RtpHeaderExtensionMap();

  void Erase();

  WebRtc_Word32 Register(const RTPExtensionType type, const WebRtc_UWord8 id);

  WebRtc_Word32 Deregister(const RTPExtensionType type);

  WebRtc_Word32 GetType(const WebRtc_UWord8 id, RTPExtensionType* type) const;

  WebRtc_Word32 GetId(const RTPExtensionType type, WebRtc_UWord8* id) const;

  WebRtc_UWord16 GetTotalLengthInBytes() const;

  void GetCopy(RtpHeaderExtensionMap* map) const;

  WebRtc_Word32 Size() const;

  RTPExtensionType First() const;

  RTPExtensionType Next(RTPExtensionType type) const;

 private:
  MapWrapper extensionMap_;
};
}
#endif // WEBRTC_MODULES_RTP_RTCP_RTP_HEADER_EXTENSION_H_
