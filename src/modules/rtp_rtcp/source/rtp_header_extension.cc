/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <cassert>

#include "common_types.h"
#include "rtp_header_extension.h"

namespace webrtc {

RtpHeaderExtensionMap::RtpHeaderExtensionMap() {
}

RtpHeaderExtensionMap::~RtpHeaderExtensionMap() {
  Erase();
}

void RtpHeaderExtensionMap::Erase() {
  while (extensionMap_.Size() != 0) {
    MapItem* item = extensionMap_.First();
    assert(item);
    HeaderExtension* extension = (HeaderExtension*)item->GetItem();
    extensionMap_.Erase(item);
    delete extension;
  }
}

int32_t RtpHeaderExtensionMap::Register(const RTPExtensionType type,
                                        const uint8_t id) {
  if (id < 1 || id > 14) {
    return -1;
  }
  MapItem* item = extensionMap_.Find(id);
  if (item != NULL) {
    return -1;
  }
  HeaderExtension* extension = new HeaderExtension(type);
  extensionMap_.Insert(id, extension);
  return 0;
}

int32_t RtpHeaderExtensionMap::Deregister(const RTPExtensionType type) {
  uint8_t id;
  if (GetId(type, &id) != 0) {
    return -1;
  }
  MapItem* item = extensionMap_.Find(id);
  if (item == NULL) {
    return -1;
  }
  HeaderExtension* extension = (HeaderExtension*)item->GetItem();
  extensionMap_.Erase(item);
  delete extension;
  return 0;
}

int32_t RtpHeaderExtensionMap::GetType(const uint8_t id,
                                       RTPExtensionType* type) const {
  assert(type);
  MapItem* item = extensionMap_.Find(id);
  if (item == NULL) {
    return -1;
  }
  HeaderExtension* extension = (HeaderExtension*)item->GetItem();
  *type = extension->type;
  return 0;
}

int32_t RtpHeaderExtensionMap::GetId(const RTPExtensionType type,
                                     uint8_t* id) const {
  assert(id);
  MapItem* item = extensionMap_.First();
  while (item != NULL) {
    HeaderExtension* extension = (HeaderExtension*)item->GetItem();
    if (extension->type == type) {
      *id = item->GetId();
      return 0;
    }
    item = extensionMap_.Next(item);
  }
  return -1;
}

uint16_t RtpHeaderExtensionMap::GetTotalLengthInBytes() const {
  // Get length for each extension block.
  uint16_t length = 0;
  MapItem* item = extensionMap_.First();
  while (item != NULL) {
    HeaderExtension* extension = (HeaderExtension*)item->GetItem();
    length += extension->length;
    item = extensionMap_.Next(item);
  }
  // Add RTP extension header length.
  if (length > 0) {
    length += RTP_ONE_BYTE_HEADER_LENGTH_IN_BYTES;
  }
  return length;
}

int32_t RtpHeaderExtensionMap::GetLengthUntilBlockStartInBytes(
    const RTPExtensionType type) const {
  uint8_t id;
  if (GetId(type, &id) != 0) {
    // Not registered.
    return -1;
  }
  // Get length until start of extension block type.
  uint16_t length = RTP_ONE_BYTE_HEADER_LENGTH_IN_BYTES;
  MapItem* item = extensionMap_.First();
  while (item != NULL) {
    HeaderExtension* extension = (HeaderExtension*)item->GetItem();
    if (extension->type == type) {
      break;
    } else {
      length += extension->length;
    }
    item = extensionMap_.Next(item);
  }
  return length;
}

int32_t RtpHeaderExtensionMap::Size() const {
  return extensionMap_.Size();
}

RTPExtensionType RtpHeaderExtensionMap::First() const {
  MapItem* item = extensionMap_.First();
  if (item == NULL) {
     return kRtpExtensionNone;
  }
  HeaderExtension* extension = (HeaderExtension*)item->GetItem();
  return extension->type;
}

RTPExtensionType RtpHeaderExtensionMap::Next(RTPExtensionType type) const {
  uint8_t id;
  if (GetId(type, &id) != 0) {
    return kRtpExtensionNone;
  }
  MapItem* item = extensionMap_.Find(id);
  if (item == NULL) {
    return kRtpExtensionNone;
  }
  item = extensionMap_.Next(item);
  if (item == NULL) {
    return kRtpExtensionNone;
  }
  HeaderExtension* extension = (HeaderExtension*)item->GetItem();
  return extension->type;
}

void RtpHeaderExtensionMap::GetCopy(RtpHeaderExtensionMap* map) const {
  assert(map);
  MapItem* item = extensionMap_.First();
  while (item != NULL) {
    HeaderExtension* extension = (HeaderExtension*)item->GetItem();
    map->Register(extension->type, item->GetId());
    item = extensionMap_.Next(item);
  }
}
} // namespace webrtc
