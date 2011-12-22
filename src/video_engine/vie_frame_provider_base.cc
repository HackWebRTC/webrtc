/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_frame_provider_base.h"

#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/tick_util.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/vie_defines.h"

namespace webrtc {

ViEFrameProviderBase::ViEFrameProviderBase(int Id, int engine_id)
    : id_(Id),
      engine_id_(engine_id),
      frame_callbacks_(),
      provider_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      extra_frame_(NULL),
      frame_delay_(0) {
}

ViEFrameProviderBase::~ViEFrameProviderBase() {
  if (frame_callbacks_.Size() > 0) {
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, id_),
                 "FrameCallbacks still exist when Provider deleted %d",
                 frame_callbacks_.Size());
  }
  for (MapItem* item = frame_callbacks_.First(); item != NULL;
       item = frame_callbacks_.Next(item)) {
    static_cast<ViEFrameCallback*>(item->GetItem())->ProviderDestroyed(id_);
  }

  while (frame_callbacks_.Erase(frame_callbacks_.First()) == 0) {
  }

  delete extra_frame_;
}

int ViEFrameProviderBase::Id() {
  return id_;
}

void ViEFrameProviderBase::DeliverFrame(
    VideoFrame& video_frame,
    int num_csrcs,
    const WebRtc_UWord32 CSRC[kRtpCsrcSize]) {
#ifdef DEBUG_
  const TickTime start_process_time = TickTime::Now();
#endif
  CriticalSectionScoped cs(provider_cs_.get());

  // Deliver the frame to all registered callbacks.
  if (frame_callbacks_.Size() > 0) {
    if (frame_callbacks_.Size() == 1) {
      // We don't have to copy the frame.
      ViEFrameCallback* frame_observer =
          static_cast<ViEFrameCallback*>(frame_callbacks_.First()->GetItem());
      frame_observer->DeliverFrame(id_, video_frame, num_csrcs, CSRC);
    } else {
      // Make a copy of the frame for all callbacks.
      for (MapItem* map_item = frame_callbacks_.First(); map_item != NULL;
           map_item = frame_callbacks_.Next(map_item)) {
        if (extra_frame_ == NULL) {
          extra_frame_ = new VideoFrame();
        }
        if (map_item != NULL) {
          ViEFrameCallback* frame_observer =
              static_cast<ViEFrameCallback*>(map_item->GetItem());
          if (frame_observer != NULL) {
            // We must copy the frame each time since the previous receiver
            // might swap it to avoid a copy.
            extra_frame_->CopyFrame(video_frame);
            frame_observer->DeliverFrame(id_, *extra_frame_, num_csrcs, CSRC);
          }
        }
      }
    }
  }
#ifdef DEBUG_
  const int process_time =
      static_cast<int>((TickTime::Now() - start_process_time).Milliseconds());
  if (process_time > 25) {
    // Warn if the delivery time is too long.
    WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, id_),
                 "%s Too long time: %ums", __FUNCTION__, process_time);
  }
#endif
}

void ViEFrameProviderBase::SetFrameDelay(int frame_delay) {
  CriticalSectionScoped cs(provider_cs_.get());
  frame_delay_ = frame_delay;

  for (MapItem* map_item = frame_callbacks_.First(); map_item != NULL;
       map_item = frame_callbacks_.Next(map_item)) {
    ViEFrameCallback* frame_observer =
        static_cast<ViEFrameCallback*>(map_item->GetItem());
    assert(frame_observer);
    frame_observer->DelayChanged(id_, frame_delay);
  }
}

int ViEFrameProviderBase::FrameDelay() {
  return frame_delay_;
}

int ViEFrameProviderBase::GetBestFormat(int& best_width,
                                        int& best_height,
                                        int& best_frame_rate) {
  int largest_width = 0;
  int largest_height = 0;
  int highest_frame_rate = 0;

  CriticalSectionScoped cs(provider_cs_.get());

  // Check if this one already exists.
  for (MapItem* map_item = frame_callbacks_.First(); map_item != NULL;
       map_item = frame_callbacks_.Next(map_item)) {
    int prefered_width = 0;
    int prefered_height = 0;
    int prefered_frame_rate = 0;

    ViEFrameCallback* callback_object =
        static_cast<ViEFrameCallback*>(map_item->GetItem());
    assert(callback_object);
    if (callback_object->GetPreferedFrameSettings(prefered_width,
                                                  prefered_height,
                                                  prefered_frame_rate) == 0) {
      if (prefered_width > largest_width) {
        largest_width = prefered_width;
      }
      if (prefered_height > largest_height) {
        largest_height = prefered_height;
      }
      if (prefered_frame_rate > highest_frame_rate) {
        highest_frame_rate = prefered_frame_rate;
      }
    }
  }

  best_width = largest_width;
  best_height = largest_height;
  best_frame_rate = highest_frame_rate;

  return 0;
}

int ViEFrameProviderBase::RegisterFrameCallback(
    int observer_id, ViEFrameCallback* callback_object) {
  if (callback_object == NULL) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "%s: No argument", __FUNCTION__);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, id_), "%s(0x%p)",
               __FUNCTION__, callback_object);

  {
    CriticalSectionScoped cs(provider_cs_.get());

    // Check if the callback already exists.
    for (MapItem* map_item = frame_callbacks_.First();
         map_item != NULL;
         map_item = frame_callbacks_.Next(map_item)) {
      const ViEFrameCallback* observer =
          static_cast<ViEFrameCallback*>(map_item->GetItem());
      if (observer == callback_object) {
        // This callback is already registered.
        WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, id_),
                     "%s 0x%p already registered", __FUNCTION__,
                     callback_object);
        assert("!frameObserver already registered");
        return -1;
      }
    }

    if (frame_callbacks_.Insert(observer_id, callback_object) != 0) {
      WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                   "%s: Could not add 0x%p to list", __FUNCTION__,
                   callback_object);
      return -1;
    }
  }
  // Report current capture delay
  callback_object->DelayChanged(id_, frame_delay_);

  // Notify implementer of this class that the callback list have changed.
  FrameCallbackChanged();
  return 0;
}

int ViEFrameProviderBase::DeregisterFrameCallback(
    const ViEFrameCallback* callback_object) {
  if (!callback_object) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "%s: No argument", __FUNCTION__);
    return -1;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, id_), "%s(0x%p)",
               __FUNCTION__, callback_object);

  {
    CriticalSectionScoped cs(provider_cs_.get());
    bool item_found = false;

    // Try to find the callback in our list.
    for (MapItem* map_item = frame_callbacks_.First(); map_item != NULL;
         map_item = frame_callbacks_.Next(map_item)) {
      const ViEFrameCallback* observer =
          static_cast<ViEFrameCallback*>(map_item->GetItem());
      if (observer == callback_object) {
        // We found it, remove it!
        frame_callbacks_.Erase(map_item);
        WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, id_),
                     "%s 0x%p deregistered", __FUNCTION__, callback_object);
        item_found = true;
        break;
      }
    }
    if (!item_found) {
      WEBRTC_TRACE(kTraceWarning, kTraceVideo, ViEId(engine_id_, id_),
                   "%s 0x%p not found", __FUNCTION__, callback_object);
      return -1;
    }
  }
  // Notify implementer of this class that the callback list have changed.
  FrameCallbackChanged();
  return 0;
}

bool ViEFrameProviderBase::IsFrameCallbackRegistered(
    const ViEFrameCallback* callback_object) {
  if (!callback_object) {
    WEBRTC_TRACE(kTraceError, kTraceVideo, ViEId(engine_id_, id_),
                 "%s: No argument", __FUNCTION__);
    return false;
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, id_),
               "%s(0x%p)", __FUNCTION__, callback_object);

  for (MapItem* map_item = frame_callbacks_.First(); map_item != NULL;
       map_item = frame_callbacks_.Next(map_item)) {
    if (callback_object == map_item->GetItem()) {
      // We found the callback.
      WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, id_),
                   "%s 0x%p is registered", __FUNCTION__, callback_object);
      return true;
    }
  }
  WEBRTC_TRACE(kTraceInfo, kTraceVideo, ViEId(engine_id_, id_),
               "%s 0x%p not registered", __FUNCTION__, callback_object);
  return false;
}

int ViEFrameProviderBase::NumberOfRegisteredFrameCallbacks() {
  CriticalSectionScoped cs(provider_cs_.get());
  return frame_callbacks_.Size();
}
}  // namespac webrtc
