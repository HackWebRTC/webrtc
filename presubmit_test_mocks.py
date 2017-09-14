# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.


class MockInputApi(object):
  """Mock class for the InputApi class.

  This class can be used for unittests for presubmit by initializing the files
  attribute as the list of changed files.
  """

  def __init__(self):
    self.change = MockChange([])


class MockOutputApi(object):
  """Mock class for the OutputApi class.

  An instance of this class can be passed to presubmit unittests for outputing
  various types of results.
  """

  class PresubmitResult(object):
    def __init__(self, message, items=None, long_text=''):
      self.message = message
      self.items = items
      self.long_text = long_text

    def __repr__(self):
      return self.message

  class PresubmitError(PresubmitResult):
    def __init__(self, message, items=None, long_text=''):
      MockOutputApi.PresubmitResult.__init__(self, message, items, long_text)
      self.type = 'error'

class MockChange(object):
  """Mock class for Change class.

  This class can be used in presubmit unittests to mock the query of the
  current change.
  """

  def __init__(self, changed_files):
    self._changed_files = changed_files
