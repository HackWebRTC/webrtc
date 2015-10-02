/*
 *  Copyright 2015 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDMainViewController.h"

#import "ARDAppClient.h"
#import "ARDMainView.h"
#import "ARDVideoCallViewController.h"

@interface ARDMainViewController () <ARDMainViewDelegate>
@end

@implementation ARDMainViewController

- (void)loadView {
  ARDMainView *mainView = [[ARDMainView alloc] initWithFrame:CGRectZero];
  mainView.delegate = self;
  self.view = mainView;
}

- (void)applicationWillResignActive:(UIApplication *)application {
  // Terminate any calls when we aren't active.
  [self dismissViewControllerAnimated:NO completion:nil];
}

#pragma mark - ARDMainViewDelegate

- (void)mainView:(ARDMainView *)mainView
    didInputRoom:(NSString *)room
      isLoopback:(BOOL)isLoopback
     isAudioOnly:(BOOL)isAudioOnly {
  if (!room.length) {
    [self showAlertWithMessage:@"Missing room name."];
    return;
  }
  // Trim whitespaces.
  NSCharacterSet *whitespaceSet = [NSCharacterSet whitespaceCharacterSet];
  NSString *trimmedRoom = [room stringByTrimmingCharactersInSet:whitespaceSet];

  // Check that room name is valid.
  NSError *error = nil;
  NSRegularExpressionOptions options = NSRegularExpressionCaseInsensitive;
  NSRegularExpression *regex =
      [NSRegularExpression regularExpressionWithPattern:@"\\w+"
                                                options:options
                                                  error:&error];
  if (error) {
    [self showAlertWithMessage:error.localizedDescription];
    return;
  }
  NSRange matchRange =
      [regex rangeOfFirstMatchInString:trimmedRoom
                               options:0
                                 range:NSMakeRange(0, trimmedRoom.length)];
  if (matchRange.location == NSNotFound ||
      matchRange.length != trimmedRoom.length) {
    [self showAlertWithMessage:@"Invalid room name."];
    return;
  }

  // Kick off the video call.
  ARDVideoCallViewController *videoCallViewController =
      [[ARDVideoCallViewController alloc] initForRoom:trimmedRoom
                                           isLoopback:isLoopback
                                          isAudioOnly:isAudioOnly];
  videoCallViewController.modalTransitionStyle =
      UIModalTransitionStyleCrossDissolve;
  [self presentViewController:videoCallViewController
                     animated:YES
                   completion:nil];
}

#pragma mark - Private

- (void)showAlertWithMessage:(NSString*)message {
  UIAlertView* alertView = [[UIAlertView alloc] initWithTitle:nil
                                                      message:message
                                                     delegate:nil
                                            cancelButtonTitle:@"OK"
                                            otherButtonTitles:nil];
  [alertView show];
}

@end
