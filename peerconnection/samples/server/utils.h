/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#ifndef PEERCONNECTION_SAMPLES_SERVER_UTILS_H_
#define PEERCONNECTION_SAMPLES_SERVER_UTILS_H_
#pragma once

#ifndef assert
#ifndef WIN32
#include <assert.h>
#else
#ifndef NDEBUG
#define assert(expr)  ((void)((expr) ? true : __debugbreak()))
#else
#define assert(expr)  ((void)0)
#endif  // NDEBUG
#endif  // WIN32
#endif  // assert

#include <string>

#ifndef ARRAYSIZE
#define ARRAYSIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

std::string int2str(int i);
std::string size_t2str(size_t i);

#endif  // PEERCONNECTION_SAMPLES_SERVER_UTILS_H_
