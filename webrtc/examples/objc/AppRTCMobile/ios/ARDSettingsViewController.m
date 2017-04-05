/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "ARDSettingsViewController.h"
#import "ARDSettingsModel.h"

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(int, ARDSettingsSections) {
  ARDSettingsSectionVideoResolution = 0,
  ARDSettingsSectionVideoCodec,
  ARDSettingsSectionBitRate,
};

@interface ARDSettingsViewController () <UITextFieldDelegate> {
  ARDSettingsModel *_settingsModel;
}

@end

@implementation ARDSettingsViewController

- (instancetype)initWithStyle:(UITableViewStyle)style
                settingsModel:(ARDSettingsModel *)settingsModel {
  self = [super initWithStyle:style];
  if (self) {
    _settingsModel = settingsModel;
  }
  return self;
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Settings";
  [self addDoneBarButton];
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self addCheckmarkInSection:ARDSettingsSectionVideoResolution
                    withArray:[self videoResolutionArray]
                    selecting:[_settingsModel currentVideoResolutionSettingFromStore]];
  [self addCheckmarkInSection:ARDSettingsSectionVideoCodec
                    withArray:[self videoCodecArray]
                    selecting:[_settingsModel currentVideoCodecSettingFromStore]];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
}

#pragma mark - Data source

- (NSArray<NSString *> *)videoResolutionArray {
  return _settingsModel.availableVideoResolutions;
}

- (NSArray<NSString *> *)videoCodecArray {
  return _settingsModel.availableVideoCodecs;
}

#pragma mark -

- (void)addDoneBarButton {
  UIBarButtonItem *barItem =
      [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                    target:self
                                                    action:@selector(dismissModally:)];
  self.navigationItem.leftBarButtonItem = barItem;
}

- (void)addCheckmarkInSection:(int)section
                    withArray:(NSArray<NSString*>*) array
                    selecting:(NSString*)selection {
  NSUInteger indexOfSelection = [array indexOfObject:selection];
  NSIndexPath *pathToBeDecorated = [NSIndexPath indexPathForRow:indexOfSelection
                                                      inSection:section];
  UITableViewCell *cell = [self.tableView cellForRowAtIndexPath:pathToBeDecorated];
  cell.accessoryType = UITableViewCellAccessoryCheckmark;
}

#pragma mark - Dismissal of view controller

- (void)dismissModally:(id)sender {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
  return 3;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
  switch (section) {
    case ARDSettingsSectionVideoResolution:
      return self.videoResolutionArray.count;
    case ARDSettingsSectionVideoCodec:
      return self.videoCodecArray.count;
    default:
      return 1;
  }
}

#pragma mark - Table view delegate helpers

- (void)removeAllAccessories:(UITableView *)tableView
                   inSection:(int)section
{
  for (int i = 0; i < [tableView numberOfRowsInSection:section]; i++) {
    NSIndexPath *rowPath = [NSIndexPath indexPathForRow:i inSection:section];
    UITableViewCell *cell = [tableView cellForRowAtIndexPath:rowPath];
    cell.accessoryType = UITableViewCellAccessoryNone;
  }
}

- (void)tableView:(UITableView *)tableView
updateListSelectionAtIndexPath:(NSIndexPath *)indexPath
        inSection:(int)section {
  [self removeAllAccessories:tableView inSection:section];
  UITableViewCell *cell = [tableView cellForRowAtIndexPath:indexPath];
  cell.accessoryType = UITableViewCellAccessoryCheckmark;
  [tableView deselectRowAtIndexPath:indexPath animated:YES];
}

#pragma mark - Table view delegate

- (nullable NSString *)tableView:(UITableView *)tableView
         titleForHeaderInSection:(NSInteger)section {
  switch (section) {
    case ARDSettingsSectionVideoResolution:
      return @"Video resolution";
    case ARDSettingsSectionVideoCodec:
      return @"Video codec";
    case ARDSettingsSectionBitRate:
      return @"Maximum bitrate";
    default:
      return @"";
  }
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  switch (indexPath.section) {
    case ARDSettingsSectionVideoResolution:
      return [self videoResolutionTableViewCellForTableView:tableView atIndexPath:indexPath];

    case ARDSettingsSectionVideoCodec:
      return [self videoCodecTableViewCellForTableView:tableView atIndexPath:indexPath];

    case ARDSettingsSectionBitRate:
      return [self bitrateTableViewCellForTableView:tableView atIndexPath:indexPath];

    default:
      return [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                    reuseIdentifier:@"identifier"];
  }
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  switch (indexPath.section) {
    case ARDSettingsSectionVideoResolution:
      [self tableView:tableView disSelectVideoResolutionAtIndex:indexPath];
      break;

    case ARDSettingsSectionVideoCodec:
      [self tableView:tableView didSelectVideoCodecCellAtIndexPath:indexPath];
      break;
  }
}

#pragma mark - Table view delegate(Video Resolution)

- (UITableViewCell *)videoResolutionTableViewCellForTableView:(UITableView *)tableView
                                                  atIndexPath:(NSIndexPath *)indexPath {
  NSString *dequeueIdentifier = @"ARDSettingsVideoResolutionViewCellIdentifier";
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:dequeueIdentifier];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                  reuseIdentifier:dequeueIdentifier];
  }
  cell.textLabel.text = self.videoResolutionArray[indexPath.row];
  return cell;
}

- (void)tableView:(UITableView *)tableView
    disSelectVideoResolutionAtIndex:(NSIndexPath *)indexPath {
  [self tableView:tableView
      updateListSelectionAtIndexPath:indexPath
                           inSection:ARDSettingsSectionVideoResolution];

  NSString *videoResolution = self.videoResolutionArray[indexPath.row];
  [_settingsModel storeVideoResolutionSetting:videoResolution];
}

#pragma mark - Table view delegate(Video Codec)

- (UITableViewCell *)videoCodecTableViewCellForTableView:(UITableView *)tableView
                                             atIndexPath:(NSIndexPath *)indexPath {
  NSString *dequeueIdentifier = @"ARDSettingsVideoCodecCellIdentifier";
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:dequeueIdentifier];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                  reuseIdentifier:dequeueIdentifier];
  }
  cell.textLabel.text = self.videoCodecArray[indexPath.row];

  return cell;
}

- (void)tableView:(UITableView *)tableView
    didSelectVideoCodecCellAtIndexPath:(NSIndexPath *)indexPath {
  [self tableView:tableView
    updateListSelectionAtIndexPath:indexPath
        inSection:ARDSettingsSectionVideoCodec];

  NSString *videoCodec = self.videoCodecArray[indexPath.row];
  [_settingsModel storeVideoCodecSetting:videoCodec];
}

#pragma mark - Table view delegate(Bitrate)

- (UITableViewCell *)bitrateTableViewCellForTableView:(UITableView *)tableView
                                          atIndexPath:(NSIndexPath *)indexPath {
  NSString *dequeueIdentifier = @"ARDSettingsBitrateCellIdentifier";
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:dequeueIdentifier];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                  reuseIdentifier:dequeueIdentifier];

    UITextField *textField = [[UITextField alloc]
        initWithFrame:CGRectMake(10, 0, cell.bounds.size.width - 20, cell.bounds.size.height)];
    NSString *currentMaxBitrate = [_settingsModel currentMaxBitrateSettingFromStore].stringValue;
    textField.text = currentMaxBitrate;
    textField.placeholder = @"Enter max bit rate (kbps)";
    textField.keyboardType = UIKeyboardTypeNumberPad;
    textField.delegate = self;

    // Numerical keyboards have no return button, we need to add one manually.
    UIToolbar *numberToolbar =
        [[UIToolbar alloc] initWithFrame:CGRectMake(0, 0, self.view.bounds.size.width, 50)];
    numberToolbar.items = @[
      [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace
                                                    target:nil
                                                    action:nil],
      [[UIBarButtonItem alloc] initWithTitle:@"Apply"
                                       style:UIBarButtonItemStyleDone
                                      target:self
                                      action:@selector(numberTextFieldDidEndEditing:)]
    ];
    [numberToolbar sizeToFit];

    textField.inputAccessoryView = numberToolbar;
    [cell addSubview:textField];
  }
  return cell;
}

- (void)numberTextFieldDidEndEditing:(id)sender {
  [self.view endEditing:YES];
}

- (void)textFieldDidEndEditing:(UITextField *)textField {
  NSNumber *bitrateNumber = nil;

  if (textField.text.length != 0) {
    bitrateNumber = [NSNumber numberWithInteger:textField.text.intValue];
  }

  [_settingsModel storeMaxBitrateSetting:bitrateNumber];
}

@end
NS_ASSUME_NONNULL_END
