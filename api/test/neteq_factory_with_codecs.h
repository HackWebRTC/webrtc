/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_NETEQ_FACTORY_WITH_CODECS_H_
#define API_TEST_NETEQ_FACTORY_WITH_CODECS_H_

#include <memory>

#include "api/neteq/neteq_factory.h"

namespace webrtc {

// This NetEq factory will use WebRTC's built-in AudioDecoders as well as the
// built-in NetEqController logic.
std::unique_ptr<NetEqFactory> CreateNetEqFactoryWithCodecs();

}  // namespace webrtc
#endif  // API_TEST_NETEQ_FACTORY_WITH_CODECS_H_
