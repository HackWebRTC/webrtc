/*
 * libjingle
 * Copyright 2013, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "APPRTCViewController.h"

@interface APPRTCViewController ()

@end

@implementation APPRTCViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.textField.delegate = self;
  [self.textField becomeFirstResponder];
}

- (void)displayText:(NSString *)text {
  dispatch_async(dispatch_get_main_queue(), ^(void) {
    NSString *output =
        [NSString stringWithFormat:@"%@\n%@", self.textOutput.text, text];
    self.textOutput.text = output;
  });
}

- (void)resetUI {
  self.textField.text = nil;
  self.textField.hidden = NO;
  self.textInstructions.hidden = NO;
  self.textOutput.hidden = YES;
  self.textOutput.text = nil;
}

#pragma mark - UITextFieldDelegate

- (void)textFieldDidEndEditing:(UITextField *)textField {
  NSString *room = textField.text;
  if ([room length] == 0) {
    return;
  }
  textField.hidden = YES;
  self.textInstructions.hidden = YES;
  self.textOutput.hidden = NO;
  // TODO(hughv): Instead of launching a URL with apprtc scheme, change to
  // prepopulating the textField with a valid URL missing the room.  This allows
  // the user to have the simplicity of just entering the room or the ability to
  // override to a custom appspot instance.  Remove apprtc:// when this is done.
  NSString *url =
      [NSString stringWithFormat:@"apprtc://apprtc.appspot.com/?r=%@", room];
  [[UIApplication sharedApplication] openURL:[NSURL URLWithString:url]];
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
  // There is no other control that can take focus, so manually resign focus
  // when return (Join) is pressed to trigger |textFieldDidEndEditing|.
  [textField resignFirstResponder];
  return YES;
}

@end
