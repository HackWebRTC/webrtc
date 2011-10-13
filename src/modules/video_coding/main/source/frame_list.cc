/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "frame_list.h"
#include "frame_buffer.h"
#include "jitter_buffer.h"
#include <cstdlib>

namespace webrtc {

VCMFrameListTimestampOrderAsc::~VCMFrameListTimestampOrderAsc()
{
    Flush();
}

void
VCMFrameListTimestampOrderAsc::Flush()
{
    while(Erase(First()) != -1) { }
}

// Inserts frame in timestamp order, with the oldest timestamp first. Takes wrap
// arounds into account
WebRtc_Word32
VCMFrameListTimestampOrderAsc::Insert(VCMFrameBuffer* frame)
{
    VCMFrameListItem* item = static_cast<VCMFrameListItem*>(First());
    VCMFrameListItem* newItem = new VCMFrameListItem(frame);
    bool inserted = false;
    if (newItem == NULL)
    {
        return -1;
    }
    while (item != NULL)
    {
        const WebRtc_UWord32 itemTimestamp = item->GetItem()->TimeStamp();
        if (LatestTimestamp(itemTimestamp, frame->TimeStamp(), NULL) ==
            itemTimestamp)
        {
            if (InsertBefore(item, newItem) < 0)
            {
                delete newItem;
                return -1;
            }
            inserted = true;
            break;
        }
        item = Next(item);
    }
    if (!inserted && ListWrapper::Insert(ListWrapper::Last(), newItem) < 0)
    {
        delete newItem;
        return -1;
    }
    return 0;
}

VCMFrameBuffer*
VCMFrameListTimestampOrderAsc::FirstFrame() const
{
    VCMFrameListItem* item = First();
    if (item != NULL)
    {
        return item->GetItem();
    }
    return NULL;
}

VCMFrameListItem*
VCMFrameListTimestampOrderAsc::FindFrameListItem(FindFrameCriteria criteria,
                                         const void* compareWith,
                                         VCMFrameListItem* startItem) const
{
    if (startItem == NULL)
    {
        startItem = First();
    }
    if (criteria == NULL)
    {
        return NULL;
    }
    while (startItem != NULL)
    {
        if (criteria(startItem->GetItem(), compareWith))
        {
            return startItem;
        }
        startItem = Next(startItem);
    }
    // No frame found
    return NULL;
}

VCMFrameBuffer*
VCMFrameListTimestampOrderAsc::FindFrame(FindFrameCriteria criteria,
                                         const void* compareWith,
                                         VCMFrameListItem* startItem) const
{
    const VCMFrameListItem* frameListItem = FindFrameListItem(criteria,
                                                              compareWith,
                                                              startItem);
    if (frameListItem == NULL)
    {
        return NULL;
    }
    return frameListItem->GetItem();
}

}

