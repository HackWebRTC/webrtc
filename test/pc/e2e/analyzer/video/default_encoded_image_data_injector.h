/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_ENCODED_IMAGE_DATA_INJECTOR_H_
#define TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_ENCODED_IMAGE_DATA_INJECTOR_H_

#include <cstdint>
#include <deque>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "api/video/encoded_image.h"
#include "rtc_base/critical_section.h"
#include "test/pc/e2e/analyzer/video/encoded_image_data_injector.h"

namespace webrtc {
namespace test {

// Injects frame id and discard flag into EncodedImage payload buffer. The
// payload buffer will be appended in the injector with 2 bytes frame id and 4
// bytes original buffer length. Discarded flag will be put into the highest bit
// of the length. It is assumed, that frame's data can't be more then 2^31
// bytes. In the decoder, frame id and discard flag will be extracted and the
// length will be used to restore original buffer. We can't put this data in the
// beginning of the payload, because first bytes are used in different parts of
// WebRTC pipeline.
//
// The data in the EncodedImage on encoder side after injection will look like
// this:
//                         4 bytes frame length + discard flag
//  _________________ _ _ _↓_ _ _
// | original buffer |   |       |
//  ¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯ ¯↑¯ ¯ ¯ ¯ ¯
//                     2 bytes frame id
//
// But on decoder side multiple payloads can be concatenated into single
// EncodedImage in jitter buffer and its payload will look like this:
//        _________ _ _ _ _ _ _ _________ _ _ _ _ _ _ _________ _ _ _ _ _ _
//  buf: | payload |   |       | payload |   |       | payload |   |       |
//        ¯¯¯¯¯¯¯¯¯ ¯ ¯ ¯ ¯ ¯ ¯ ¯¯¯¯¯¯¯¯¯ ¯ ¯ ¯ ¯ ¯ ¯ ¯¯¯¯¯¯¯¯¯ ¯ ¯ ¯ ¯ ¯ ¯
// To correctly restore such images we will extract id by this algorithm:
//   1. Make a pass from end to begin of the buffer to restore origin lengths,
//      frame ids and discard flags from length high bit.
//   2. If all discard flags are true - discard this encoded image
//   3. Make a pass from begin to end copying data to the output basing on
//      previously extracted length
// Also it will check, that all extracted ids are equals.
//
// Because EncodedImage doesn't take ownership of its buffer, injector will keep
// ownership of the buffers that will be used for EncodedImages with injected
// data. This is needed because there is no way to inform the injector that
// a buffer can be disposed. To address this issue injector will use a pool
// of buffers in round robin manner and will assume, that when it overlaps
// the buffer can be disposed.
//
// Because single injector can be used for different coding entities (encoders
// or decoders), it will store a |coding_entity_id| in the set for each
// coding entity seen and if the new one arrives, it will extend its buffers
// pool, adding 256 more buffers. During initialization injector will
// preallocate buffers for 2 coding entities, so 512 buffers with initial size
// 2KB. If in some point of time bigger buffer will be required, it will be also
// extended.
class DefaultEncodedImageDataInjector : public EncodedImageDataInjector,
                                        public EncodedImageDataExtractor {
 public:
  DefaultEncodedImageDataInjector();
  ~DefaultEncodedImageDataInjector() override;

  // TODO(titovartem) add support for discard injection and update the doc.
  EncodedImage InjectData(uint16_t id,
                          bool discard,
                          const EncodedImage& source,
                          int coding_entity_id) override;
  EncodedImageExtractionResult ExtractData(const EncodedImage& source,
                                           int coding_entity_id) override;

 private:
  void ExtendIfRequired(int coding_entity_id) RTC_LOCKS_EXCLUDED(lock_);
  std::vector<uint8_t>* NextBuffer() RTC_LOCKS_EXCLUDED(lock_);

  // Because single injector will be used for all encoder and decoders in one
  // peer and in case of the single process for all encoders and decoders in
  // another peer, it can be called from different threads. So we need to ensure
  // that buffers are given consecutively from pools and pool extension won't
  // be interrupted by getting buffer in other thread.
  rtc::CriticalSection lock_;

  // Store coding entities for which buffers pool have been already extended.
  std::set<int> coding_entities_ RTC_GUARDED_BY(lock_);
  std::deque<std::unique_ptr<std::vector<uint8_t>>> bufs_pool_
      RTC_GUARDED_BY(lock_);
};

}  // namespace test
}  // namespace webrtc

#endif  // TEST_PC_E2E_ANALYZER_VIDEO_DEFAULT_ENCODED_IMAGE_DATA_INJECTOR_H_
