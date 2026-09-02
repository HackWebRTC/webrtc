/*
 *  Copyright (c) 2026 The WebRTC project authors. All Rights Reserved.
 */

#include "rtc_tools/roq/media_peer.h"

#import <Cocoa/Cocoa.h>

#include <atomic>
#include <chrono>
#include <thread>

namespace webrtc::roq {

MediaPeerResult RunMediaPeerWithPlatformEventLoop(
    const MediaPeerOptions& options) {
  @autoreleasepool {
    [NSApplication sharedApplication];
    std::atomic<bool> complete = false;
    MediaPeerResult result;
    std::thread worker([&] {
      result = RunMediaPeer(options);
      complete.store(true);
    });
    bool saw_visible_window = false;
    while (!complete.load()) {
      @autoreleasepool {
        NSDate* deadline = [NSDate dateWithTimeIntervalSinceNow:0.05];
        NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:deadline
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event) [NSApp sendEvent:event];
        [NSApp updateWindows];
        bool has_visible_window = false;
        for (NSWindow* window in [NSApp windows]) {
          has_visible_window |= [window isVisible];
        }
        saw_visible_window |= has_visible_window;
        if (saw_visible_window && !has_visible_window) RequestMediaPeerStop();
      }
    }
    worker.join();
    return result;
  }
}

}  // namespace webrtc::roq
