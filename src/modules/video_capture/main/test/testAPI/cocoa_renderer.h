/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_TEST_TESTAPI_COCOA_RENDERER_H_
#define WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_TEST_TESTAPI_COCOA_RENDERER_H_

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <OpenGL/glu.h>
#import <OpenGL/OpenGL.h>

@interface CocoaRenderer : NSOpenGLView {
	NSOpenGLContext*	_nsOpenGLContext;
	int					_screen;
}

@property (nonatomic, retain)NSOpenGLContext* _nsOpenGLContext;
@property int screen;

- (void)initCocoaRenderer:(NSOpenGLPixelFormat*)fmt;

@end

#endif // WEBRTC_MODULES_VIDEO_CAPTURE_MAIN_TEST_TESTAPI_COCOA_RENDERER_H_
