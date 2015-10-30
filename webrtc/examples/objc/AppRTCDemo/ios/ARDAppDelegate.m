/*
 *  Copyright 2013 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDAppDelegate.h"

#import "RTCLogging.h"
#import "RTCPeerConnectionFactory.h"

#import "ARDMainViewController.h"

@implementation ARDAppDelegate {
  UIWindow *_window;
}

#pragma mark - UIApplicationDelegate methods

- (BOOL)application:(UIApplication *)application
    didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  [RTCPeerConnectionFactory initializeSSL];
  _window =  [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
  [_window makeKeyAndVisible];
  ARDMainViewController *viewController = [[ARDMainViewController alloc] init];
  _window.rootViewController = viewController;

#if defined(NDEBUG)
  // In debug builds the default level is LS_INFO and in non-debug builds it is
  // disabled. Continue to log to console in non-debug builds, but only
  // warnings and errors.
  RTCSetMinDebugLogLevel(kRTCLoggingSeverityWarning);
#endif

  return YES;
}

- (void)applicationWillResignActive:(UIApplication *)application {
  ARDMainViewController *viewController =
      (ARDMainViewController *)_window.rootViewController;
  [viewController applicationWillResignActive:application];
}

- (void)applicationWillTerminate:(UIApplication *)application {
  [RTCPeerConnectionFactory deinitializeSSL];
}

@end
