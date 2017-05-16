/*
 *  Copyright 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCShader.h"

#import "RTCNV12TextureCache.h"
#import "RTCShader+Private.h"
#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoFrame.h"

#include "webrtc/base/checks.h"
#include "webrtc/base/optional.h"

static const char kNV12FragmentShaderSource[] =
  SHADER_VERSION
  "precision mediump float;"
  FRAGMENT_SHADER_IN " vec2 v_texcoord;\n"
  "uniform lowp sampler2D s_textureY;\n"
  "uniform lowp sampler2D s_textureUV;\n"
  FRAGMENT_SHADER_OUT
  "void main() {\n"
  "    mediump float y;\n"
  "    mediump vec2 uv;\n"
  "    y = " FRAGMENT_SHADER_TEXTURE "(s_textureY, v_texcoord).r;\n"
  "    uv = " FRAGMENT_SHADER_TEXTURE "(s_textureUV, v_texcoord).ra -\n"
  "        vec2(0.5, 0.5);\n"
  "    " FRAGMENT_SHADER_COLOR " = vec4(y + 1.403 * uv.y,\n"
  "                                     y - 0.344 * uv.x - 0.714 * uv.y,\n"
  "                                     y + 1.770 * uv.x,\n"
  "                                     1.0);\n"
  "  }\n";

@implementation RTCNativeNV12Shader {
  GLuint _vertexBuffer;
  GLuint _nv12Program;
  GLint _ySampler;
  GLint _uvSampler;
  RTCNV12TextureCache *_textureCache;
  // Store current rotation and only upload new vertex data when rotation
  // changes.
  rtc::Optional<RTCVideoRotation> _currentRotation;
}

- (instancetype)initWithContext:(GlContextType *)context {
  if (self = [super init]) {
    _textureCache = [[RTCNV12TextureCache alloc] initWithContext:context];
    if (!_textureCache || ![self setupNV12Program] ||
        !RTCSetupVerticesForProgram(_nv12Program, &_vertexBuffer, nullptr)) {
      RTCLog(@"Failed to initialize RTCNativeNV12Shader.");
      self = nil;
    }
  }
  return self;
}

- (void)dealloc {
  glDeleteProgram(_nv12Program);
  glDeleteBuffers(1, &_vertexBuffer);
}

- (BOOL)setupNV12Program {
  _nv12Program = RTCCreateProgramFromFragmentSource(kNV12FragmentShaderSource);
  if (!_nv12Program) {
    return NO;
  }
  _ySampler = glGetUniformLocation(_nv12Program, "s_textureY");
  _uvSampler = glGetUniformLocation(_nv12Program, "s_textureUV");

  return (_ySampler >= 0 && _uvSampler >= 0);
}

- (BOOL)drawFrame:(RTCVideoFrame *)frame {
  glUseProgram(_nv12Program);
  if (![_textureCache uploadFrameToTextures:frame]) {
    return NO;
  }

  // Y-plane.
  glActiveTexture(GL_TEXTURE0);
  glUniform1i(_ySampler, 0);
  glBindTexture(GL_TEXTURE_2D, _textureCache.yTexture);

  // UV-plane.
  glActiveTexture(GL_TEXTURE1);
  glUniform1i(_uvSampler, 1);
  glBindTexture(GL_TEXTURE_2D, _textureCache.uvTexture);

  glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
  if (!_currentRotation || frame.rotation != *_currentRotation) {
    _currentRotation = rtc::Optional<RTCVideoRotation>(frame.rotation);
    RTCSetVertexData(*_currentRotation);
  }
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  [_textureCache releaseTextures];

  return YES;
}

@end
