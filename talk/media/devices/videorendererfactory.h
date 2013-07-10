// libjingle
// Copyright 2010 Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. The name of the author may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// A factory to create a GUI video renderer.

#ifndef TALK_MEDIA_DEVICES_VIDEORENDERERFACTORY_H_
#define TALK_MEDIA_DEVICES_VIDEORENDERERFACTORY_H_

#include "talk/media/base/videorenderer.h"
#if defined(LINUX) && defined(HAVE_GTK)
#include "talk/media/devices/gtkvideorenderer.h"
#elif defined(OSX) && !defined(CARBON_DEPRECATED)
#include "talk/media/devices/carbonvideorenderer.h"
#elif defined(WIN32)
#include "talk/media/devices/gdivideorenderer.h"
#endif

namespace cricket {

class VideoRendererFactory {
 public:
  static VideoRenderer* CreateGuiVideoRenderer(int x, int y) {
  #if defined(LINUX) && defined(HAVE_GTK)
    return new GtkVideoRenderer(x, y);
  #elif defined(OSX) && !defined(CARBON_DEPRECATED)
    CarbonVideoRenderer* renderer = new CarbonVideoRenderer(x, y);
    // Needs to be initialized on the main thread.
    if (renderer->Initialize()) {
      return renderer;
    } else {
      delete renderer;
      return NULL;
    }
  #elif defined(WIN32)
    return new GdiVideoRenderer(x, y);
  #else
    return NULL;
  #endif
  }
};

}  // namespace cricket

#endif  // TALK_MEDIA_DEVICES_VIDEORENDERERFACTORY_H_
