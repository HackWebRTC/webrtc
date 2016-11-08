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
#import "ARDMediaConstraintsModel.h"

NS_ASSUME_NONNULL_BEGIN
@interface ARDSettingsViewController () {
  ARDMediaConstraintsModel *_mediaConstraintsModel;
}

@end

@implementation ARDSettingsViewController

- (instancetype)initWithStyle:(UITableViewStyle)style
        mediaConstraintsModel:(ARDMediaConstraintsModel *)mediaConstraintsModel {
  self = [super initWithStyle:style];
  if (self) {
    _mediaConstraintsModel = mediaConstraintsModel;
  }
  return self;
}

#pragma mark - View lifecycle

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Settings";
  [self addDoneBarButton];
}

- (void)viewDidAppear:(BOOL)animated {
  [super viewDidAppear:animated];
  [self selectCurrentlyStoredOrDefaultMediaConstraints];
}

#pragma mark - Data source

- (NSArray<NSString *> *)mediaConstraintsArray {
  return _mediaConstraintsModel.availableVideoResoultionsMediaConstraints;
}

#pragma mark -

- (void)addDoneBarButton {
  UIBarButtonItem *barItem =
      [[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone
                                                    target:self
                                                    action:@selector(dismissModally:)];
  self.navigationItem.leftBarButtonItem = barItem;
}

- (void)selectCurrentlyStoredOrDefaultMediaConstraints {
  NSString *currentSelection = [_mediaConstraintsModel currentVideoResoultionConstraintFromStore];

  NSUInteger indexOfSelection = [[self mediaConstraintsArray] indexOfObject:currentSelection];
  NSIndexPath *pathToBeSelected = [NSIndexPath indexPathForRow:indexOfSelection inSection:0];
  [self.tableView selectRowAtIndexPath:pathToBeSelected
                              animated:NO
                        scrollPosition:UITableViewScrollPositionNone];
  // Manully invoke the delegate method because the previous invocation will not.
  [self tableView:self.tableView didSelectRowAtIndexPath:pathToBeSelected];
}

#pragma mark - Dismissal of view controller

- (void)dismissModally:(id)sender {
  [self dismissViewControllerAnimated:YES completion:nil];
}

#pragma mark - Table view data source

- (NSInteger)numberOfSectionsInTableView:(UITableView *)tableView {
  return 1;
}

- (NSInteger)tableView:(UITableView *)tableView numberOfRowsInSection:(NSInteger)section {
  return self.mediaConstraintsArray.count;
}

#pragma mark - Table view delegate

- (BOOL)sectionIsMediaConstraints:(int)section {
  return section == 0;
}

- (BOOL)indexPathIsMediaConstraints:(NSIndexPath *)indexPath {
  return [self sectionIsMediaConstraints:indexPath.section];
}

- (nullable NSString *)tableView:(UITableView *)tableView
         titleForHeaderInSection:(NSInteger)section {
  if ([self sectionIsMediaConstraints:section]) {
    return @"Media constraints";
  }
  return @"";
}

- (UITableViewCell *)tableView:(UITableView *)tableView
         cellForRowAtIndexPath:(NSIndexPath *)indexPath {
  if ([self indexPathIsMediaConstraints:indexPath]) {
    return [self mediaConstraintsTableViewCellForTableView:tableView atIndexPath:indexPath];
  }
  return [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                reuseIdentifier:@"identifier"];
}

- (nullable NSIndexPath *)tableView:(UITableView *)tableView
           willSelectRowAtIndexPath:(nonnull NSIndexPath *)indexPath {
  if ([self indexPathIsMediaConstraints:indexPath]) {
    return [self tableView:tableView willDeselectMediaConstraintsRowAtIndexPath:indexPath];
  }
  return indexPath;
}

- (void)tableView:(UITableView *)tableView didSelectRowAtIndexPath:(NSIndexPath *)indexPath {
  if ([self indexPathIsMediaConstraints:indexPath]) {
    [self tableView:tableView didSelectMediaConstraintsCellAtIndexPath:indexPath];
  }
}

#pragma mark - Table view delegate(Media Constraints)

- (UITableViewCell *)mediaConstraintsTableViewCellForTableView:(UITableView *)tableView
                                                   atIndexPath:(NSIndexPath *)indexPath {
  NSString *dequeueIdentifier = @"ARDSettingsMediaConstraintsViewCellIdentifier";
  UITableViewCell *cell = [tableView dequeueReusableCellWithIdentifier:dequeueIdentifier];
  if (!cell) {
    cell = [[UITableViewCell alloc] initWithStyle:UITableViewCellStyleDefault
                                  reuseIdentifier:dequeueIdentifier];
  }
  cell.textLabel.text = self.mediaConstraintsArray[indexPath.row];
  return cell;
}

- (void)tableView:(UITableView *)tableView
    didSelectMediaConstraintsCellAtIndexPath:(NSIndexPath *)indexPath {
  UITableViewCell *cell = [tableView cellForRowAtIndexPath:indexPath];
  cell.accessoryType = UITableViewCellAccessoryCheckmark;

  NSString *mediaConstraintsString = self.mediaConstraintsArray[indexPath.row];
  [_mediaConstraintsModel storeVideoResoultionConstraint:mediaConstraintsString];
}

- (NSIndexPath *)tableView:(UITableView *)tableView
    willDeselectMediaConstraintsRowAtIndexPath:(NSIndexPath *)indexPath {
  NSIndexPath *oldSelection = [tableView indexPathForSelectedRow];
  UITableViewCell *cell = [tableView cellForRowAtIndexPath:oldSelection];
  cell.accessoryType = UITableViewCellAccessoryNone;
  return indexPath;
}

@end
NS_ASSUME_NONNULL_END
