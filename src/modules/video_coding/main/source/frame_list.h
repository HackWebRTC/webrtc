/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_FRAME_LIST_H_
#define WEBRTC_MODULES_VIDEO_CODING_FRAME_LIST_H_

#include "list_wrapper.h"
#include "typedefs.h"
#include <stdlib.h>

namespace webrtc
{

class VCMFrameBuffer;

typedef bool (*FindFrameCriteria)(VCMFrameBuffer*, const void*);

class VCMFrameListItem : public ListItem
{
    friend class VCMFrameListTimestampOrderAsc;
public:
    VCMFrameListItem(const VCMFrameBuffer* ptr) : ListItem(ptr) {}
    ~VCMFrameListItem() {};

    VCMFrameBuffer* GetItem() const
            { return static_cast<VCMFrameBuffer*>(ListItem::GetItem()); }
};

class VCMFrameListTimestampOrderAsc : public ListWrapper
{
public:
    VCMFrameListTimestampOrderAsc() : ListWrapper() {};
    ~VCMFrameListTimestampOrderAsc();

    void Flush();

    // Inserts frame in timestamp order, with the oldest timestamp first.
    // Takes wrap arounds into account.
    WebRtc_Word32 Insert(VCMFrameBuffer* frame);
    VCMFrameBuffer* FirstFrame() const;
    VCMFrameListItem* Next(VCMFrameListItem* item) const
            { return static_cast<VCMFrameListItem*>(ListWrapper::Next(item)); }
    VCMFrameListItem* Previous(VCMFrameListItem* item) const
            { return static_cast<VCMFrameListItem*>(ListWrapper::Previous(item)); }
    VCMFrameListItem* First() const
            { return static_cast<VCMFrameListItem*>(ListWrapper::First()); }
    VCMFrameListItem* Last() const
            { return static_cast<VCMFrameListItem*>(ListWrapper::Last()); }
    VCMFrameListItem* FindFrameListItem(FindFrameCriteria criteria,
                              const void* compareWith = NULL,
                              VCMFrameListItem* startItem = NULL) const;
    VCMFrameBuffer* FindFrame(FindFrameCriteria criteria,
                                             const void* compareWith = NULL,
                                             VCMFrameListItem* startItem = NULL) const;
};

} // namespace webrtc

#endif // WEBRTC_MODULES_VIDEO_CODING_FRAME_LIST_H_
