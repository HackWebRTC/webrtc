/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * vie_renderer.h
 */

#ifndef WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RENDERER_H_
#define WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RENDERER_H_


#include "vie_frame_provider_base.h"
#include "map_wrapper.h"
#include "vie_render.h"
#include "video_render_defines.h"

namespace webrtc {

class VideoRender;
class VideoRenderCallback;
class ViERenderManager;


class ViEExternalRendererImpl : public VideoRenderCallback
{
public:
  ViEExternalRendererImpl();
  int SetViEExternalRenderer(ExternalRenderer* externalRenderer, webrtc::RawVideoType videoInputFormat);

  // implements VideoRenderCallback
  virtual WebRtc_Word32 RenderFrame(const WebRtc_UWord32 streamId,
                                  webrtc::VideoFrame&   videoFrame);

  virtual ~ViEExternalRendererImpl(){};

private:
  ExternalRenderer*  _externalRenderer;
  webrtc::RawVideoType      _externalRendererFormat;
  WebRtc_UWord32          _externalRendererWidth;
  WebRtc_UWord32          _externalRendererHeight;
};


class ViERenderer: public ViEFrameCallback
{
public:
  static ViERenderer* CreateViERenderer(const WebRtc_Word32 renderId,
                          const WebRtc_Word32 engineId,
                          VideoRender& renderModule,
                          ViERenderManager& renderManager,
                          const WebRtc_UWord32 zOrder,
                          const float left,
                          const float top,
                          const float right,
                          const float bottom);

  ~ViERenderer(void);

  WebRtc_Word32 StartRender();
  WebRtc_Word32 StopRender();

  WebRtc_Word32 GetLastRenderedFrame(const WebRtc_Word32 renderID, webrtc::VideoFrame& videoFrame);

  WebRtc_Word32 ConfigureRenderer(const unsigned int zOrder,
                const float left,
                const float top,
                const float right,
                const float bottom);


  VideoRender& RenderModule();

  WebRtc_Word32 EnableMirroring(const WebRtc_Word32 renderId, const bool enable, const bool mirrorXAxis, const bool mirrorYAxis);

  WebRtc_Word32 SetTimeoutImage(const webrtc::VideoFrame& timeoutImage,const WebRtc_Word32 timeoutValue);
  WebRtc_Word32 SetRenderStartImage(const webrtc::VideoFrame& startImage);
  WebRtc_Word32 SetExternalRenderer(const WebRtc_Word32 renderId, webrtc::RawVideoType videoInputFormat, ExternalRenderer* externalRenderer);

private:
  WebRtc_Word32 Init(const WebRtc_UWord32 zOrder,
             const float left,
             const float top,
             const float right,
             const float bottom);

  ViERenderer(const WebRtc_Word32 renderId,const WebRtc_Word32 engineId,
              VideoRender& renderModule,
              ViERenderManager& renderManager);


  // Implement ViEFrameCallback

  virtual void DeliverFrame(int id, VideoFrame& videoFrame, int numCSRCs = 0,
                            const WebRtc_UWord32 CSRC[kRtpCsrcSize] = NULL);
  virtual void DelayChanged(int id, int frameDelay){return;}
  virtual int GetPreferedFrameSettings(int &width, int &height,
                                       int &frameRate){return -1;}

  virtual void ProviderDestroyed(int id);


  WebRtc_UWord32          _renderId;
  WebRtc_Word32            _engineId;
  VideoRender&      _renderModule;
  ViERenderManager&        _renderManager;
  VideoRenderCallback*    _ptrRenderCallback;
  ViEExternalRendererImpl*  _ptrIncomingExternalCallback;

};

} //namespace webrtc

#endif // WEBRTC_VIDEO_ENGINE_MAIN_SOURCE_VIE_RENDERER_H_
