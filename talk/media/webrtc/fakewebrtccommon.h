/*
 * libjingle
 * Copyright 2011 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TALK_SESSION_PHONE_FAKEWEBRTCCOMMON_H_
#define TALK_SESSION_PHONE_FAKEWEBRTCCOMMON_H_

#include "webrtc/base/common.h"

namespace cricket {

#define WEBRTC_STUB(method, args) \
  int method args override { return 0; }

#define WEBRTC_STUB_CONST(method, args) \
  int method args const override { return 0; }

#define WEBRTC_BOOL_STUB(method, args) \
  bool method args override { return true; }

#define WEBRTC_BOOL_STUB_CONST(method, args) \
  bool method args const override { return true; }

#define WEBRTC_VOID_STUB(method, args) \
  void method args override {}

#define WEBRTC_FUNC(method, args) int method args override

#define WEBRTC_FUNC_CONST(method, args) int method args const override

#define WEBRTC_BOOL_FUNC(method, args) bool method args override

#define WEBRTC_VOID_FUNC(method, args) void method args override
}  // namespace cricket

#endif  // TALK_SESSION_PHONE_FAKEWEBRTCCOMMON_H_
