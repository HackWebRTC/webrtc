/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_renderer.h"
#include "video_render.h"
#include "video_render_defines.h"
#include "vie_render_manager.h"
#include "vplib.h"

namespace webrtc {

ViERenderer* ViERenderer::CreateViERenderer(
                            const WebRtc_Word32 renderId,
                            const WebRtc_Word32 engineId,
                            VideoRender& renderModule,
                            ViERenderManager& renderManager,
                            const WebRtc_UWord32 zOrder,
                            const float left,
                            const float top,
                            const float right,
                            const float bottom)
{
    ViERenderer* self=new ViERenderer(renderId,engineId,renderModule,renderManager);
    if(!self || self->Init(zOrder,left,top,right,bottom)!=0)
    {
        delete self;
        self=NULL;
    }
    return self;
}

ViERenderer::~ViERenderer(void)
{
    if(_ptrRenderCallback)
    {
        _renderModule.DeleteIncomingRenderStream( _renderId);
    }

  if(_ptrIncomingExternalCallback){
    delete _ptrIncomingExternalCallback;
  }

}

ViERenderer::ViERenderer(const WebRtc_Word32 renderId,const WebRtc_Word32 engineId,
                        VideoRender& renderModule,
                        ViERenderManager& renderManager)
:
_renderId(renderId),
_engineId(engineId),
_renderModule(renderModule),
_renderManager(renderManager),
_ptrRenderCallback(NULL),
_ptrIncomingExternalCallback(new ViEExternalRendererImpl())
{

}

WebRtc_Word32 ViERenderer::Init(const WebRtc_UWord32 zOrder,
               const float left,
               const float top,
               const float right,
               const float bottom)
{
    _ptrRenderCallback = (VideoRenderCallback*)_renderModule.AddIncomingRenderStream( _renderId, zOrder, left, top, right, bottom);
    if (_ptrRenderCallback == NULL)
    {
        // Logging done
        return -1;
    }

    return 0;
}

WebRtc_Word32 ViERenderer::GetLastRenderedFrame(const WebRtc_Word32 renderID, webrtc::VideoFrame& videoFrame)
{
  return _renderModule.GetLastRenderedFrame(renderID, videoFrame);
}

WebRtc_Word32 ViERenderer::StartRender()
{
  return _renderModule.StartRender(_renderId);
}
WebRtc_Word32 ViERenderer::StopRender()
{
    return _renderModule.StopRender(_renderId);
}

    // Implement ViEFrameCallback
void ViERenderer::DeliverFrame(int id,
                                webrtc::VideoFrame& videoFrame,
                                int numCSRCs,
                                const WebRtc_UWord32 CSRC[kRtpCsrcSize])
{


    _ptrRenderCallback->RenderFrame(_renderId,videoFrame);

}

// Implement ViEFrameCallback
void ViERenderer::ProviderDestroyed(int id)
{
    _renderManager.RemoveRenderStream(_renderId); // Remove the render stream since the provider is destroyed.
}

VideoRender& ViERenderer::RenderModule()
{
    return _renderModule;
}

WebRtc_Word32 ViERenderer::ConfigureRenderer(const unsigned int zOrder,
              const float left,
              const float top,
              const float right,
              const float bottom)
{
  return _renderModule.ConfigureRenderer(_renderId, zOrder, left, top, right, bottom);
}



WebRtc_Word32 ViERenderer::SetTimeoutImage(const webrtc::VideoFrame& timeoutImage,const WebRtc_Word32 timeoutValue)
{
    return _renderModule.SetTimeoutImage(_renderId,timeoutImage,timeoutValue);
}

WebRtc_Word32  ViERenderer::SetRenderStartImage(const webrtc::VideoFrame& startImage)
{
    return _renderModule.SetStartImage(_renderId,startImage);
}



WebRtc_Word32 ViERenderer::EnableMirroring(const WebRtc_Word32 renderId, const bool enable, const bool mirrorXAxis, const bool mirrorYAxis)
{
    return _renderModule.MirrorRenderStream(renderId, enable, mirrorXAxis, mirrorYAxis);
}


WebRtc_Word32 ViERenderer::SetExternalRenderer(const WebRtc_Word32 renderId, webrtc::RawVideoType videoInputFormat, ExternalRenderer* externalRenderer)
{
  if(NULL == _ptrIncomingExternalCallback){
    return -1;
  }

  _ptrIncomingExternalCallback->SetViEExternalRenderer(externalRenderer, videoInputFormat);
  return _renderModule.AddExternalRenderCallback(renderId, _ptrIncomingExternalCallback);
}



ViEExternalRendererImpl::ViEExternalRendererImpl() :
_externalRenderer(NULL),
_externalRendererFormat(),
_externalRendererWidth(0),
_externalRendererHeight(0)
{
}

int ViEExternalRendererImpl::SetViEExternalRenderer(ExternalRenderer* externalRenderer, webrtc::RawVideoType videoInputFormat)
{
  _externalRenderer = externalRenderer;
  _externalRendererFormat = videoInputFormat;
  return 0;
}

// implements VideoRenderCallback
WebRtc_Word32 ViEExternalRendererImpl::RenderFrame(const WebRtc_UWord32 streamId,
                                webrtc::VideoFrame&   videoFrame)
{
  webrtc::VideoFrame convertedFrame;
  webrtc::VideoFrame* pConvertedFrame = &convertedFrame;

  // convert to requested format
  switch(_externalRendererFormat)
  {
  case webrtc::kVideoI420:
    pConvertedFrame = &videoFrame;
    break;
  case webrtc::kVideoYV12:
    convertedFrame.VerifyAndAllocate(webrtc::CalcBufferSize(webrtc::kYV12,videoFrame.Width(),videoFrame.Height()));
    webrtc::ConvertI420ToYV12(videoFrame.Buffer(), convertedFrame.Buffer(), videoFrame.Width(), videoFrame.Height(), 0);
    break;
  case webrtc::kVideoYUY2:
    convertedFrame.VerifyAndAllocate(webrtc::CalcBufferSize(webrtc::kYUY2,videoFrame.Width(),videoFrame.Height()));
    webrtc::ConvertI420ToYUY2(videoFrame.Buffer(), convertedFrame.Buffer(), videoFrame.Width(), videoFrame.Height(), 0);
    break;
  case webrtc::kVideoUYVY:
    convertedFrame.VerifyAndAllocate(webrtc::CalcBufferSize(webrtc::kUYVY,videoFrame.Width(),videoFrame.Height()));
    webrtc::ConvertI420ToUYVY(videoFrame.Buffer(), convertedFrame.Buffer(), videoFrame.Width(), videoFrame.Height(), 0);
    break;
  case webrtc::kVideoIYUV:
    // no conversion available
    break;
  case webrtc::kVideoARGB:
    convertedFrame.VerifyAndAllocate(webrtc::CalcBufferSize(webrtc::kARGB,videoFrame.Width(),videoFrame.Height()));
    webrtc::ConvertI420ToARGB(videoFrame.Buffer(), convertedFrame.Buffer(), videoFrame.Width(), videoFrame.Height(), 0);
    break;
  case webrtc::kVideoRGB24:
    convertedFrame.VerifyAndAllocate(webrtc::CalcBufferSize(webrtc::kRGB24,videoFrame.Width(),videoFrame.Height()));
    webrtc::ConvertI420ToRGB24(videoFrame.Buffer(), convertedFrame.Buffer(), videoFrame.Width(), videoFrame.Height());
    break;
  case webrtc::kVideoRGB565:
    convertedFrame.VerifyAndAllocate(webrtc::CalcBufferSize(webrtc::kRGB565,videoFrame.Width(),videoFrame.Height()));
    webrtc::ConvertI420ToRGB565(videoFrame.Buffer(), convertedFrame.Buffer(), videoFrame.Width(), videoFrame.Height());
    break;
  case webrtc::kVideoARGB4444:
    convertedFrame.VerifyAndAllocate(webrtc::CalcBufferSize(webrtc::kARGB4444,videoFrame.Width(),videoFrame.Height()));
    webrtc::ConvertI420ToARGB4444(videoFrame.Buffer(), convertedFrame.Buffer(), videoFrame.Width(), videoFrame.Height(), 0);
    break;
  case webrtc::kVideoARGB1555 :
    convertedFrame.VerifyAndAllocate(webrtc::CalcBufferSize(webrtc::kARGB1555,videoFrame.Width(),videoFrame.Height()));
    webrtc::ConvertI420ToARGB1555(videoFrame.Buffer(), convertedFrame.Buffer(), videoFrame.Width(), videoFrame.Height(), 0);
    break;
  default:
    // the format is something funny. Should never reach here...
    assert(false);
    pConvertedFrame = NULL;
    break;
  }

  if(_externalRendererWidth!=videoFrame.Width() || _externalRendererHeight!=videoFrame.Height())
  {
    _externalRendererWidth = videoFrame.Width();
    _externalRendererHeight = videoFrame.Height();
    _externalRenderer->FrameSizeChange(_externalRendererWidth, _externalRendererHeight, streamId);
  }

  if(pConvertedFrame)
  {
    _externalRenderer->DeliverFrame(pConvertedFrame->Buffer(), pConvertedFrame->Length());
  }
  return 0;
}

} //namespace webrtc




