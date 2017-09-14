import unittest

import PRESUBMIT
from presubmit_test_mocks import MockInputApi, MockOutputApi


class CheckBugEntryField(unittest.TestCase):
  def testCommitMessageBugEntryWithNoError(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()
    mock_input_api.change.BUG = 'webrtc:1234'
    errors = PRESUBMIT.CheckCommitMessageBugEntry(mock_input_api,
                                                   mock_output_api)
    self.assertEqual(0, len(errors))

  def testCommitMessageBugEntryReturnError(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()
    mock_input_api.change.BUG = 'webrtc:1234,webrtc=4321'
    errors = PRESUBMIT.CheckCommitMessageBugEntry(mock_input_api,
                                                   mock_output_api)
    self.assertEqual(1, len(errors))
    self.assertEqual(('Bogus BUG entry: webrtc=4321. Please specify'
                      ' the issue tracker prefix and the issue number,'
                      ' separated by a colon, e.g. webrtc:123 or'
                      ' chromium:12345.'), str(errors[0]))

  def testCommitMessageBugEntryIsNone(self):
    mock_input_api = MockInputApi()
    mock_output_api = MockOutputApi()
    mock_input_api.change.BUG = 'None'
    errors = PRESUBMIT.CheckCommitMessageBugEntry(mock_input_api,
                                                   mock_output_api)
    self.assertEqual(0, len(errors))


if __name__ == '__main__':
  unittest.main()

