/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMTLI420Renderer.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoFrame.h"

#include "webrtc/api/video/video_rotation.h"

#define MTL_STRINGIFY(s) @ #s

// As defined in shaderSource.
static NSString *const vertexFunctionName = @"vertexPassthrough";
static NSString *const fragmentFunctionName = @"fragmentColorConversion";

static NSString *const pipelineDescriptorLabel = @"RTCPipeline";
static NSString *const commandBufferLabel = @"RTCCommandBuffer";
static NSString *const renderEncoderLabel = @"RTCEncoder";
static NSString *const renderEncoderDebugGroup = @"RTCDrawFrame";

static const float cubeVertexData[64] = {
  -1.0, -1.0, 0.0, 1.0, 1.0, -1.0, 1.0, 1.0, -1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0,

  // rotation = 90, offset = 16.
  -1.0, -1.0, 1.0, 1.0, 1.0, -1.0, 1.0, 0.0, -1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0,

  // rotation = 180, offset = 32.
  -1.0, -1.0, 1.0, 0.0, 1.0, -1.0, 0.0, 0.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0,

  // rotation = 270, offset = 48.
  -1.0, -1.0, 0.0, 0.0, 1.0, -1.0, 0.0, 1.0, -1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0,
};

static inline int offsetForRotation(webrtc::VideoRotation rotation) {
  switch (rotation) {
    case webrtc::kVideoRotation_0:
      return 0;
    case webrtc::kVideoRotation_90:
      return 16;
    case webrtc::kVideoRotation_180:
      return 32;
    case webrtc::kVideoRotation_270:
      return 48;
  }
  return 0;
}

static NSString *const shaderSource = MTL_STRINGIFY(
    using namespace metal; typedef struct {
      packed_float2 position;
      packed_float2 texcoord;
    } Vertex;

    typedef struct {
      float4 position[[position]];
      float2 texcoord;
    } Varyings;

    vertex Varyings vertexPassthrough(device Vertex * verticies[[buffer(0)]],
                                      unsigned int vid[[vertex_id]]) {
      Varyings out;
      device Vertex &v = verticies[vid];
      out.position = float4(float2(v.position), 0.0, 1.0);
      out.texcoord = v.texcoord;

      return out;
    }

    fragment half4 fragmentColorConversion(
        Varyings in[[stage_in]], texture2d<float, access::sample> textureY[[texture(0)]],
        texture2d<float, access::sample> textureU[[texture(1)]],
        texture2d<float, access::sample> textureV[[texture(2)]]) {
      constexpr sampler s(address::clamp_to_edge, filter::linear);
      float y;
      float u;
      float v;
      float r;
      float g;
      float b;
      // Conversion for YUV to rgb from http://www.fourcc.org/fccyvrgb.php
      y = textureY.sample(s, in.texcoord).r;
      u = textureU.sample(s, in.texcoord).r;
      v = textureV.sample(s, in.texcoord).r;
      u = u - 0.5;
      v = v - 0.5;
      r = y + 1.403 * v;
      g = y - 0.344 * u - 0.714 * v;
      b = y + 1.770 * u;

      float4 out = float4(r, g, b, 1.0);

      return half4(out);
    });

// The max number of command buffers in flight.
// For now setting it up to 1.
// In future we might use triple buffering method if it improves performance.

static const NSInteger kMaxInflightBuffers = 1;

@implementation RTCMTLI420Renderer {
  __kindof MTKView *_view;

  // Controller.
  dispatch_semaphore_t _inflight_semaphore;

  // Renderer.
  id<MTLDevice> _device;
  id<MTLCommandQueue> _commandQueue;
  id<MTLLibrary> _defaultLibrary;
  id<MTLRenderPipelineState> _pipelineState;

  // Textures.
  id<MTLTexture> _yTexture;
  id<MTLTexture> _uTexture;
  id<MTLTexture> _vTexture;

  MTLTextureDescriptor *_descriptor;
  MTLTextureDescriptor *_chromaDescriptor;

  int _width;
  int _height;
  int _chromaWidth;
  int _chromaHeight;

  // Buffers.
  id<MTLBuffer> _vertexBuffer;

  // RTC Frame parameters.
  int _offset;
}

- (instancetype)init {
  if (self = [super init]) {
    // Offset of 0 is equal to rotation of 0.
    _offset = 0;
    _inflight_semaphore = dispatch_semaphore_create(kMaxInflightBuffers);
  }

  return self;
}

- (BOOL)addRenderingDestination:(__kindof MTKView *)view {
  return [self setupWithView:view];
}

#pragma mark - Private

- (BOOL)setupWithView:(__kindof MTKView *)view {
  BOOL success = NO;
  if ([self setupMetal]) {
    [self setupView:view];
    [self loadAssets];
    [self setupBuffers];
    success = YES;
  }
  return success;
}

#pragma mark - GPU methods

- (BOOL)setupMetal {
  // Set the view to use the default device.
  _device = MTLCreateSystemDefaultDevice();
  if (!_device) {
    return NO;
  }

  // Create a new command queue.
  _commandQueue = [_device newCommandQueue];

  // Load metal library from source.
  NSError *libraryError = nil;

  id<MTLLibrary> sourceLibrary =
      [_device newLibraryWithSource:shaderSource options:NULL error:&libraryError];

  if (libraryError) {
    RTCLogError(@"Metal: Library with source failed\n%@", libraryError);
    return NO;
  }

  if (!sourceLibrary) {
    RTCLogError(@"Metal: Failed to load library. %@", libraryError);
    return NO;
  }
  _defaultLibrary = sourceLibrary;

  return YES;
}

- (void)setupView:(__kindof MTKView *)view {
  view.device = _device;

  view.preferredFramesPerSecond = 30;
  view.autoResizeDrawable = NO;

  // We need to keep reference to the view as it's needed down the rendering pipeline.
  _view = view;
}

- (void)loadAssets {
  id<MTLFunction> vertexFunction = [_defaultLibrary newFunctionWithName:vertexFunctionName];
  id<MTLFunction> fragmentFunction = [_defaultLibrary newFunctionWithName:fragmentFunctionName];

  MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
  pipelineDescriptor.label = pipelineDescriptorLabel;
  pipelineDescriptor.vertexFunction = vertexFunction;
  pipelineDescriptor.fragmentFunction = fragmentFunction;
  pipelineDescriptor.colorAttachments[0].pixelFormat = _view.colorPixelFormat;
  pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
  NSError *error = nil;
  _pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];

  if (!_pipelineState) {
    RTCLogError(@"Metal: Failed to create pipeline state. %@", error);
  }
}

- (void)setupBuffers {
  _vertexBuffer = [_device newBufferWithBytes:cubeVertexData
                                       length:sizeof(cubeVertexData)
                                      options:MTLStorageModeShared];
}

- (void)render {
  dispatch_semaphore_wait(_inflight_semaphore, DISPATCH_TIME_FOREVER);

  id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
  commandBuffer.label = commandBufferLabel;

  __block dispatch_semaphore_t block_semaphore = _inflight_semaphore;
  [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull) {
    dispatch_semaphore_signal(block_semaphore);
  }];

  MTLRenderPassDescriptor *_renderPassDescriptor = _view.currentRenderPassDescriptor;
  if (_renderPassDescriptor) {  // Valid drawable.
    id<MTLRenderCommandEncoder> renderEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:_renderPassDescriptor];
    renderEncoder.label = renderEncoderLabel;

    // Set context state.
    [renderEncoder pushDebugGroup:renderEncoderDebugGroup];
    [renderEncoder setRenderPipelineState:_pipelineState];
    [renderEncoder setVertexBuffer:_vertexBuffer offset:_offset * sizeof(float) atIndex:0];
    [renderEncoder setFragmentTexture:_yTexture atIndex:0];
    [renderEncoder setFragmentTexture:_uTexture atIndex:1];
    [renderEncoder setFragmentTexture:_vTexture atIndex:2];

    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                      vertexStart:0
                      vertexCount:4
                    instanceCount:1];
    [renderEncoder popDebugGroup];
    [renderEncoder endEncoding];

    [commandBuffer presentDrawable:_view.currentDrawable];
  }

  [commandBuffer commit];
}

#pragma mark - RTCMTLRenderer

- (void)drawFrame:(RTCVideoFrame *)frame {
  if (!frame) {
    return;
  }
  if ([self setupTexturesForFrame:frame]) {
    @autoreleasepool {
      [self render];
    }
  }
}

- (BOOL)setupTexturesForFrame:(nonnull RTCVideoFrame *)frame {
  // Luma (y) texture.

  if (!_descriptor || (_width != frame.width && _height != frame.height)) {
    _width = frame.width;
    _height = frame.height;
    _descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                     width:_width
                                                                    height:_height
                                                                 mipmapped:NO];
    _descriptor.usage = MTLTextureUsageShaderRead;
    _yTexture = [_device newTextureWithDescriptor:_descriptor];
  }

  // Chroma (u,v) textures
  [_yTexture replaceRegion:MTLRegionMake2D(0, 0, _width, _height)
               mipmapLevel:0
                 withBytes:frame.dataY
               bytesPerRow:frame.strideY];

  if (!_chromaDescriptor ||
      (_chromaWidth != frame.width / 2 && _chromaHeight != frame.height / 2)) {
    _chromaWidth = frame.width / 2;
    _chromaHeight = frame.height / 2;
    _chromaDescriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                           width:_chromaWidth
                                                          height:_chromaHeight
                                                       mipmapped:NO];
    _chromaDescriptor.usage = MTLTextureUsageShaderRead;
    _uTexture = [_device newTextureWithDescriptor:_chromaDescriptor];
    _vTexture = [_device newTextureWithDescriptor:_chromaDescriptor];
  }

  [_uTexture replaceRegion:MTLRegionMake2D(0, 0, _chromaWidth, _chromaHeight)
               mipmapLevel:0
                 withBytes:frame.dataU
               bytesPerRow:frame.strideU];
  [_vTexture replaceRegion:MTLRegionMake2D(0, 0, _chromaWidth, _chromaHeight)
               mipmapLevel:0
                 withBytes:frame.dataV
               bytesPerRow:frame.strideV];

  _offset = offsetForRotation((webrtc::VideoRotation)frame.rotation);

  return (_uTexture != nil) && (_yTexture != nil) && (_vTexture != nil);
}

@end
