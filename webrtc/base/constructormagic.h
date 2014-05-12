/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_CONSTRUCTORMAGIC_H_
#define WEBRTC_BASE_CONSTRUCTORMAGIC_H_

#define DISALLOW_ASSIGN(TypeName) \
  void operator=(const TypeName&)

// A macro to disallow the evil copy constructor and operator= functions
// This should be used in the private: declarations for a class.
// Undefine this, just in case. Some third-party includes have their own
// version.
#undef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName)    \
  TypeName(const TypeName&);                    \
  DISALLOW_ASSIGN(TypeName)

// Alternative, less-accurate legacy name.
#define DISALLOW_EVIL_CONSTRUCTORS(TypeName) \
  DISALLOW_COPY_AND_ASSIGN(TypeName)

// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
//
// This should be used in the private: declarations for a class
// that wants to prevent anyone from instantiating it. This is
// especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName) \
  TypeName();                                    \
  DISALLOW_EVIL_CONSTRUCTORS(TypeName)


#endif  // WEBRTC_BASE_CONSTRUCTORMAGIC_H_
