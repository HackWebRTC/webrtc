/*
 * libjingle
 * Copyright 2014, Google Inc.
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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#import "RTCEAGLVideoRenderer.h"

#import <OpenGLES/ES2/gl.h>
#import "RTCI420Frame.h"

// TODO(tkchin): check and log openGL errors. Methods here return BOOLs in
// anticipation of that happening in the future.

// Convenience macro for writing shader code that converts a code snippet into
// a C string during the C preprocessor step.
#define RTC_STRINGIZE(...) #__VA_ARGS__

// Vertex shader doesn't do anything except pass coordinates through.
static const char kVertexShaderSource[] = RTC_STRINGIZE(
  attribute vec2 position;
  attribute vec2 texcoord;
  varying vec2 v_texcoord;
  void main() {
    gl_Position = vec4(position.x, position.y, 0.0, 1.0);
    v_texcoord = texcoord;
  }
);

// Fragment shader converts YUV values from input textures into a final RGB
// pixel. The conversion formula is from http://www.fourcc.org/fccyvrgb.php.
static const char kFragmentShaderSource[] = RTC_STRINGIZE(
  precision highp float;
  varying vec2 v_texcoord;
  uniform lowp sampler2D s_textureY;
  uniform lowp sampler2D s_textureU;
  uniform lowp sampler2D s_textureV;
  void main() {
    float y, u, v, r, g, b;
    y = texture2D(s_textureY, v_texcoord).r;
    u = texture2D(s_textureU, v_texcoord).r;
    v = texture2D(s_textureV, v_texcoord).r;
    u = u - 0.5;
    v = v - 0.5;
    r = y + 1.403 * v;
    g = y - 0.344 * u - 0.714 * v;
    b = y + 1.770 * u;
    gl_FragColor = vec4(r, g, b, 1.0);
  }
);

// Compiles a shader of the given |type| with GLSL source |source| and returns
// the shader handle or 0 on error.
GLuint CreateShader(GLenum type, const GLchar* source) {
  GLuint shader = glCreateShader(type);
  if (!shader) {
    return 0;
  }
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);
  GLint compileStatus = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
  if (compileStatus == GL_FALSE) {
    glDeleteShader(shader);
    shader = 0;
  }
  return shader;
}

// Links a shader program with the given vertex and fragment shaders and
// returns the program handle or 0 on error.
GLuint CreateProgram(GLuint vertexShader, GLuint fragmentShader) {
  if (vertexShader == 0 || fragmentShader == 0) {
    return 0;
  }
  GLuint program = glCreateProgram();
  if (!program) {
    return 0;
  }
  glAttachShader(program, vertexShader);
  glAttachShader(program, fragmentShader);
  glLinkProgram(program);
  GLint linkStatus = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
  if (linkStatus == GL_FALSE) {
    glDeleteProgram(program);
    program = 0;
  }
  return program;
}

// When modelview and projection matrices are identity (default) the world is
// contained in the square around origin with unit size 2. Drawing to these
// coordinates is equivalent to drawing to the entire screen. The texture is
// stretched over that square using texture coordinates (u, v) that range
// from (0, 0) to (1, 1) inclusive. Texture coordinates are flipped vertically
// here because the incoming frame has origin in upper left hand corner but
// OpenGL expects origin in bottom left corner.
const GLfloat gVertices[] = {
    // X, Y, U, V.
    -1, -1, 0, 1,  // Bottom left.
    1,  -1, 1, 1,  // Bottom right.
    1,  1,  1, 0,  // Top right.
    -1, 1,  0, 0,  // Top left.
};

// |kNumTextures| must not exceed 8, which is the limit in OpenGLES2. Two sets
// of 3 textures are used here, one for each of the Y, U and V planes. Having
// two sets alleviates CPU blockage in the event that the GPU is asked to render
// to a texture that is already in use.
static const GLsizei kNumTextureSets = 2;
static const GLsizei kNumTextures = 3 * kNumTextureSets;

@implementation RTCEAGLVideoRenderer {
  EAGLContext* _context;
  BOOL _isInitialized;
  NSUInteger _currentTextureSet;
  // Handles for OpenGL constructs.
  GLuint _textures[kNumTextures];
  GLuint _program;
  GLuint _vertexBuffer;
  GLint _position;
  GLint _texcoord;
  GLint _ySampler;
  GLint _uSampler;
  GLint _vSampler;
}

+ (void)initialize {
  // Disable dithering for performance.
  glDisable(GL_DITHER);
}

- (instancetype)initWithContext:(EAGLContext*)context {
  NSAssert(context != nil, @"context cannot be nil");
  if (self = [super init]) {
    _context = context;
  }
  return self;
}

- (BOOL)drawFrame:(RTCI420Frame*)frame {
  if (!_isInitialized) {
    return NO;
  }
  if (_lastDrawnFrame == frame) {
    return NO;
  }
  [self ensureGLContext];
  if (![self updateTextureSizesForFrame:frame] ||
      ![self updateTextureDataForFrame:frame]) {
    return NO;
  }
  glClear(GL_COLOR_BUFFER_BIT);
  glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
  glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
  _lastDrawnFrame = frame;
  return YES;
}

- (void)setupGL {
  if (_isInitialized) {
    return;
  }
  [self ensureGLContext];
  if (![self setupProgram]) {
    return;
  }
  if (![self setupTextures]) {
    return;
  }
  if (![self setupVertices]) {
    return;
  }
  glUseProgram(_program);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glClearColor(0, 0, 0, 1);
  _isInitialized = YES;
}

- (void)teardownGL {
  if (!_isInitialized) {
    return;
  }
  [self ensureGLContext];
  glDeleteProgram(_program);
  _program = 0;
  glDeleteTextures(kNumTextures, _textures);
  glDeleteBuffers(1, &_vertexBuffer);
  _vertexBuffer = 0;
  _isInitialized = NO;
}

#pragma mark - Private

- (void)ensureGLContext {
  if ([EAGLContext currentContext] != _context) {
    NSAssert(_context, @"context shouldn't be nil");
    [EAGLContext setCurrentContext:_context];
  }
}

- (BOOL)setupProgram {
  NSAssert(!_program, @"program already set up");
  GLuint vertexShader = CreateShader(GL_VERTEX_SHADER, kVertexShaderSource);
  GLuint fragmentShader =
      CreateShader(GL_FRAGMENT_SHADER, kFragmentShaderSource);
  _program = CreateProgram(vertexShader, fragmentShader);
  // Shaders are created only to generate program.
  if (vertexShader) {
    glDeleteShader(vertexShader);
  }
  if (fragmentShader) {
    glDeleteShader(fragmentShader);
  }
  if (!_program) {
    return NO;
  }
  _position = glGetAttribLocation(_program, "position");
  _texcoord = glGetAttribLocation(_program, "texcoord");
  _ySampler = glGetUniformLocation(_program, "s_textureY");
  _uSampler = glGetUniformLocation(_program, "s_textureU");
  _vSampler = glGetUniformLocation(_program, "s_textureV");
  if (_position < 0 || _texcoord < 0 || _ySampler < 0 || _uSampler < 0 ||
      _vSampler < 0) {
    return NO;
  }
  return YES;
}

- (BOOL)setupTextures {
  glGenTextures(kNumTextures, _textures);
  // Set parameters for each of the textures we created.
  for (GLsizei i = 0; i < kNumTextures; i++) {
    glActiveTexture(GL_TEXTURE0 + i);
    glBindTexture(GL_TEXTURE_2D, _textures[i]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }
  return YES;
}

- (BOOL)updateTextureSizesForFrame:(RTCI420Frame*)frame {
  if (frame.height == _lastDrawnFrame.height &&
      frame.width == _lastDrawnFrame.width &&
      frame.chromaWidth == _lastDrawnFrame.chromaWidth &&
      frame.chromaHeight == _lastDrawnFrame.chromaHeight) {
    return YES;
  }
  GLsizei lumaWidth = frame.width;
  GLsizei lumaHeight = frame.height;
  GLsizei chromaWidth = frame.chromaWidth;
  GLsizei chromaHeight = frame.chromaHeight;
  for (GLint i = 0; i < kNumTextureSets; i++) {
    glActiveTexture(GL_TEXTURE0 + i * 3);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 lumaWidth,
                 lumaHeight,
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 0);

    glActiveTexture(GL_TEXTURE0 + i * 3 + 1);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 chromaWidth,
                 chromaHeight,
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 0);

    glActiveTexture(GL_TEXTURE0 + i * 3 + 2);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_LUMINANCE,
                 chromaWidth,
                 chromaHeight,
                 0,
                 GL_LUMINANCE,
                 GL_UNSIGNED_BYTE,
                 0);
  }
  return YES;
}

- (BOOL)updateTextureDataForFrame:(RTCI420Frame*)frame {
  NSUInteger textureOffset = _currentTextureSet * 3;
  NSAssert(textureOffset + 3 <= kNumTextures, @"invalid offset");
  NSParameterAssert(frame.yPitch == frame.width);
  NSParameterAssert(frame.uPitch == frame.chromaWidth);
  NSParameterAssert(frame.vPitch == frame.chromaWidth);

  glActiveTexture(GL_TEXTURE0 + textureOffset);
  // When setting texture sampler uniforms, the texture index is used not
  // the texture handle.
  glUniform1i(_ySampler, textureOffset);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_LUMINANCE,
               frame.width,
               frame.height,
               0,
               GL_LUMINANCE,
               GL_UNSIGNED_BYTE,
               frame.yPlane);

  glActiveTexture(GL_TEXTURE0 + textureOffset + 1);
  glUniform1i(_uSampler, textureOffset + 1);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_LUMINANCE,
               frame.chromaWidth,
               frame.chromaHeight,
               0,
               GL_LUMINANCE,
               GL_UNSIGNED_BYTE,
               frame.uPlane);

  glActiveTexture(GL_TEXTURE0 + textureOffset + 2);
  glUniform1i(_vSampler, textureOffset + 2);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_LUMINANCE,
               frame.chromaWidth,
               frame.chromaHeight,
               0,
               GL_LUMINANCE,
               GL_UNSIGNED_BYTE,
               frame.vPlane);

  _currentTextureSet = (_currentTextureSet + 1) % kNumTextureSets;
  return YES;
}

- (BOOL)setupVertices {
  NSAssert(!_vertexBuffer, @"vertex buffer already set up");
  glGenBuffers(1, &_vertexBuffer);
  if (!_vertexBuffer) {
    return NO;
  }
  glBindBuffer(GL_ARRAY_BUFFER, _vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(gVertices), gVertices, GL_DYNAMIC_DRAW);

  // Read position attribute from |gVertices| with size of 2 and stride of 4
  // beginning at the start of the array. The last argument indicates offset
  // of data within |gVertices| as supplied to the vertex buffer.
  glVertexAttribPointer(
      _position, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(GLfloat), (void*)0);
  glEnableVertexAttribArray(_position);

  // Read texcoord attribute from |gVertices| with size of 2 and stride of 4
  // beginning at the first texcoord in the array. The last argument indicates
  // offset of data within |gVertices| as supplied to the vertex buffer.
  glVertexAttribPointer(_texcoord,
                        2,
                        GL_FLOAT,
                        GL_FALSE,
                        4 * sizeof(GLfloat),
                        (void*)(2 * sizeof(GLfloat)));
  glEnableVertexAttribArray(_texcoord);

  return YES;
}

@end
