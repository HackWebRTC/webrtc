/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef P2P_BASE_JSEPTRANSPORT_H_
#define P2P_BASE_JSEPTRANSPORT_H_

#include <vector>

// TODO(deadbeef): This file used to be a dumping ground of various enums,
// structs, etc., which have now moved to their own files. Delete this file
// once no one is depending on it, and they start including the more specific
// file(s) instead.

#include "api/candidate.h"

// DTLS enums/structs.
#include "p2p/base/dtlstransport.h"
#include "p2p/base/dtlstransportinternal.h"

// ICE enums/structs.
#include "p2p/base/icetransportinternal.h"

// Various constants.
#include "p2p/base/p2pconstants.h"

// ConnectionInfo, among many other things.
#include "p2p/base/port.h"

// SDP structures
#include "p2p/base/sessiondescription.h"
#include "p2p/base/transportinfo.h"

// Legacy typedef.
namespace cricket {
typedef std::vector<Candidate> Candidates;
}

#endif  // P2P_BASE_JSEPTRANSPORT_H_
