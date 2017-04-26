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

#import "RTCI420TextureCache.h"
#import "RTCShader+Private.h"
#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoFrame.h"

#include "webrtc/base/optional.h"

// Fragment shader converts YUV values from input textures into a final RGB
// pixel. The conversion formula is from http://www.fourcc.org/fccyvrgb.php.
static const char kI420FragmentShaderSource[] =
  SHADER_VERSION
  "precision highp float;"
  FRAGMENT_SHADER_IN " vec2 v_texcoord;\n"
  "uniform lowp sampler2D s_textureY;\n"
  "uniform lowp sampler2D s_textureU;\n"
  "uniform lowp sampler2D s_textureV;\n"
  FRAGMENT_SHADER_OUT
  "void main() {\n"
  "    float y, u, v, r, g, b;\n"
  "    y = " FRAGMENT_SHADER_TEXTURE "(s_textureY, v_texcoord).r;\n"
  "    u = " FRAGMENT_SHADER_TEXTURE "(s_textureU, v_texcoord).r;\n"
  "    v = " FRAGMENT_SHADER_TEXTURE "(s_textureV, v_texcoord).r;\n"
  "    u = u - 0.5;\n"
  "    v = v - 0.5;\n"
  "    r = y + 1.403 * v;\n"
  "    g = y - 0.344 * u - 0.714 * v;\n"
  "    b = y + 1.770 * u;\n"
  "    " FRAGMENT_SHADER_COLOR " = vec4(r, g, b, 1.0);\n"
  "  }\n";

@implementation RTCI420Shader {
  RTCI420TextureCache* textureCache;
  // Handles for OpenGL constructs.
  GLuint _i420Program;
  GLuint _vertexArray;
  GLuint _vertexBuffer;
  GLint _ySampler;
  GLint _uSampler;
  GLint _vSampler;
  // Store current rotation and only upload new vertex data when rotation
  // changes.
  rtc::Optional<RTCVideoRotation> _currentRotation;
}

- (instancetype)initWithContext:(GlContextType *)context {
  if (self = [super init]) {
    textureCache = [[RTCI420TextureCache alloc] initWithContext:context];
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (![self setupI420Program] ||
        !RTCSetupVerticesForProgram(_i420Program, &_vertexBuffer, &_vertexArray)) {
      RTCLog(@"Failed to initialize RTCI420Shader.");
      self = nil;
    }
  }
  return self;
}

- (void)dealloc {
  glDeleteProgram(_i420Program);
  glDeleteBuffers(1, &_vertexBuffer);
  glDeleteVertexArrays(1, &_vertexArray);
}

- (BOOL)setupI420Program {
  _i420Program = RTCCreateProgramFromFragmentSource(kI420FragmentShaderSource);
  if (!_i420Program) {
    return NO;
  }
  _ySampler = glGetUniformLocation(_i420Program, "s_textureY");
  _uSampler = glGetUniformLocation(_i420Program, "s_textureU");
  _vSampler = glGetUniformLocation(_i420Program, "s_textureV");

  return (_ySampler >= 0 && _uSampler >= 0 && _vSampler >= 0);
}

- (BOOL)drawFrame:(RTCVideoFrame*)frame {
  glUseProgram(_i420Program);

  [textureCache uploadFrameToTextures:frame];

#if !TARGET_OS_IPHONE
  glBindVertexArray(_vertexArray);
#endif

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, textureCache.yTexture);
  glUniform1i(_ySampler, 0);

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, textureCache.uTexture);
  glUniform1i(_uSampler, 1);

  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, textureCache.vTexture);
  glUniform1i(_vSampler, 2);

  glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
  if (!_currentRotation || frame.rotation != *_currentRotation) {
    _currentRotation = rtc::Optional<RTCVideoRotation>(frame.rotation);
    RTCSetVertexData(*_currentRotation);
  }
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

  return YES;
}

@end
