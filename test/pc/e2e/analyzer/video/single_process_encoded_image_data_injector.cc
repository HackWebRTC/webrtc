/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/analyzer/video/single_process_encoded_image_data_injector.h"

#include <cstddef>

#include "absl/memory/memory.h"
#include "api/video/encoded_image.h"
#include "rtc_base/checks.h"

namespace webrtc {
namespace test {
namespace {

// Number of bytes from the beginning of the EncodedImage buffer that will be
// used to store frame id and sub id.
constexpr size_t kUsedBufferSize = 3;

}  // namespace

SingleProcessEncodedImageDataInjector::SingleProcessEncodedImageDataInjector() =
    default;
SingleProcessEncodedImageDataInjector::
    ~SingleProcessEncodedImageDataInjector() = default;

EncodedImage SingleProcessEncodedImageDataInjector::InjectData(
    uint16_t id,
    bool discard,
    const EncodedImage& source,
    int coding_entity_id) {
  RTC_CHECK(source.size() >= kUsedBufferSize);

  ExtractionInfo info;
  info.length = source.size();
  info.discard = discard;
  memcpy(info.origin_data, source.data(), kUsedBufferSize);
  {
    rtc::CritScope crit(&lock_);
    // Will create new one if missed.
    ExtractionInfoVector& ev = extraction_cache_[id];
    info.sub_id = ev.next_sub_id++;
    ev.infos[info.sub_id] = info;
  }

  EncodedImage out = source;
  out.data()[0] = id & 0x00ff;
  out.data()[1] = (id & 0xff00) >> 8;
  out.data()[2] = info.sub_id;
  return out;
}

EncodedImageExtractionResult SingleProcessEncodedImageDataInjector::ExtractData(
    const EncodedImage& source,
    int coding_entity_id) {
  EncodedImage out = source;

  // Both |source| and |out| image will share the same buffer for payload or
  // out will have a copy for it, so we can operate on the |out| buffer only.
  uint8_t* buffer = out.data();
  size_t size = out.size();

  size_t pos = 0;
  absl::optional<uint16_t> id = absl::nullopt;
  bool discard = true;
  while (pos < size) {
    // Extract frame id from first 2 bytes of the payload.
    uint16_t next_id = buffer[pos] + (buffer[pos + 1] << 8);
    // Extract frame sub id from second 2 byte of the payload.
    uint16_t sub_id = buffer[pos + 2];

    RTC_CHECK(!id || id.value() == next_id)
        << "Different frames encoded into single encoded image: " << id.value()
        << " vs " << next_id;
    id = next_id;

    ExtractionInfo info;
    {
      rtc::CritScope crit(&lock_);
      auto ext_vector_it = extraction_cache_.find(next_id);
      RTC_CHECK(ext_vector_it != extraction_cache_.end())
          << "Unknown frame id " << next_id;

      auto info_it = ext_vector_it->second.infos.find(sub_id);
      RTC_CHECK(info_it != ext_vector_it->second.infos.end())
          << "Unknown sub id " << sub_id << " for frame " << next_id;
      info = info_it->second;
      ext_vector_it->second.infos.erase(info_it);
    }

    if (info.discard) {
      // If this encoded image is marked to be discarded - erase it's payload
      // from the buffer.
      memmove(&buffer[pos], &buffer[pos + info.length],
              size - pos - info.length);
      size -= info.length;
    } else {
      memmove(&buffer[pos], info.origin_data, kUsedBufferSize);
      pos += info.length;
    }
    // We need to discard encoded image only if all concatenated encoded images
    // have to be discarded.
    discard = discard & info.discard;
  }
  out.set_size(pos);

  return EncodedImageExtractionResult{id.value(), out, discard};
}

SingleProcessEncodedImageDataInjector::ExtractionInfoVector::
    ExtractionInfoVector() = default;
SingleProcessEncodedImageDataInjector::ExtractionInfoVector::
    ~ExtractionInfoVector() = default;

}  // namespace test
}  // namespace webrtc
