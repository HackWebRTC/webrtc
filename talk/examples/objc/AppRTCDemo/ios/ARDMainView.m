/*
 * libjingle
 * Copyright 2015 Google Inc.
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

#import "ARDMainView.h"

#import "UIImage+ARDUtilities.h"

// TODO(tkchin): retrieve status bar height dynamically.
static CGFloat const kStatusBarHeight = 20;

static CGFloat const kRoomTextButtonSize = 40;
static CGFloat const kRoomTextFieldHeight = 40;
static CGFloat const kRoomTextFieldMargin = 8;
static CGFloat const kAppLabelHeight = 20;

@class ARDRoomTextField;
@protocol ARDRoomTextFieldDelegate <NSObject>
- (void)roomTextField:(ARDRoomTextField *)roomTextField
         didInputRoom:(NSString *)room;
@end

// Helper view that contains a text field and a clear button.
@interface ARDRoomTextField : UIView <UITextFieldDelegate>
@property(nonatomic, weak) id<ARDRoomTextFieldDelegate> delegate;
@end

@implementation ARDRoomTextField {
  UITextField *_roomText;
  UIButton *_clearButton;
}

@synthesize delegate = _delegate;

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    _roomText = [[UITextField alloc] initWithFrame:CGRectZero];
    _roomText.borderStyle = UITextBorderStyleNone;
    _roomText.font = [UIFont fontWithName:@"Roboto" size:12];
    _roomText.placeholder = @"Room name";
    _roomText.delegate = self;
    [_roomText addTarget:self
                  action:@selector(textFieldDidChange:)
        forControlEvents:UIControlEventEditingChanged];
    [self addSubview:_roomText];

    _clearButton = [UIButton buttonWithType:UIButtonTypeCustom];
    UIImage *image = [UIImage imageForName:@"ic_clear_black_24dp.png"
                                     color:[UIColor colorWithWhite:0 alpha:.4]];

    [_clearButton setImage:image forState:UIControlStateNormal];
    [_clearButton addTarget:self
                      action:@selector(onClear:)
            forControlEvents:UIControlEventTouchUpInside];
    _clearButton.hidden = YES;
    [self addSubview:_clearButton];

    // Give rounded corners and a light gray border.
    self.layer.borderWidth = 1;
    self.layer.borderColor = [[UIColor lightGrayColor] CGColor];
    self.layer.cornerRadius = 2;
  }
  return self;
}

- (void)layoutSubviews {
  CGRect bounds = self.bounds;
  _clearButton.frame = CGRectMake(CGRectGetMaxX(bounds) - kRoomTextButtonSize,
                                  CGRectGetMinY(bounds),
                                  kRoomTextButtonSize,
                                  kRoomTextButtonSize);
  _roomText.frame = CGRectMake(
      CGRectGetMinX(bounds) + kRoomTextFieldMargin,
      CGRectGetMinY(bounds),
      CGRectGetMinX(_clearButton.frame) - CGRectGetMinX(bounds) -
          kRoomTextFieldMargin,
      kRoomTextFieldHeight);
}

- (CGSize)sizeThatFits:(CGSize)size {
  size.height = kRoomTextFieldHeight;
  return size;
}

#pragma mark - UITextFieldDelegate

- (void)textFieldDidEndEditing:(UITextField *)textField {
  [_delegate roomTextField:self didInputRoom:textField.text];
}

- (BOOL)textFieldShouldReturn:(UITextField *)textField {
  // There is no other control that can take focus, so manually resign focus
  // when return (Join) is pressed to trigger |textFieldDidEndEditing|.
  [textField resignFirstResponder];
  return YES;
}

#pragma mark - Private

- (void)textFieldDidChange:(id)sender {
  [self updateClearButton];
}

- (void)onClear:(id)sender {
  _roomText.text = @"";
  [self updateClearButton];
  [_roomText resignFirstResponder];
}

- (void)updateClearButton {
  _clearButton.hidden = _roomText.text.length == 0;
}

@end

@interface ARDMainView () <ARDRoomTextFieldDelegate>
@end

@implementation ARDMainView {
  UILabel *_appLabel;
  ARDRoomTextField *_roomText;
}

@synthesize delegate = _delegate;

- (instancetype)initWithFrame:(CGRect)frame {
  if (self = [super initWithFrame:frame]) {
    _appLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    _appLabel.text = @"AppRTCDemo";
    _appLabel.font = [UIFont fontWithName:@"Roboto" size:34];
    _appLabel.textColor = [UIColor colorWithWhite:0 alpha:.2];
    [_appLabel sizeToFit];
    [self addSubview:_appLabel];

    _roomText = [[ARDRoomTextField alloc] initWithFrame:CGRectZero];
    _roomText.delegate = self;
    [self addSubview:_roomText];

    self.backgroundColor = [UIColor whiteColor];
  }
  return self;
}

- (void)layoutSubviews {
  CGRect bounds = self.bounds;
  CGFloat roomTextWidth = bounds.size.width - 2 * kRoomTextFieldMargin;
  CGFloat roomTextHeight = [_roomText sizeThatFits:bounds.size].height;
  _roomText.frame = CGRectMake(kRoomTextFieldMargin,
                               kStatusBarHeight + kRoomTextFieldMargin,
                               roomTextWidth,
                               roomTextHeight);
  _appLabel.center = CGPointMake(CGRectGetMidX(bounds), CGRectGetMidY(bounds));
}

#pragma mark - ARDRoomTextFieldDelegate

- (void)roomTextField:(ARDRoomTextField *)roomTextField
         didInputRoom:(NSString *)room {
  [_delegate mainView:self didInputRoom:room];
}

@end
