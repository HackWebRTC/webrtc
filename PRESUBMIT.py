# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

def CheckChangeOnUpload(input_api, output_api):
  webrtc_license_header = (
      r'.*? Copyright \(c\) %(year)s The WebRTC project authors\. '
        r'All Rights Reserved\.\n'
      r'.*?\n'
      r'.*? Use of this source code is governed by a BSD-style license\n'
      r'.*? that can be found in the LICENSE file in the root of the source\n'
      r'.*? tree\. An additional intellectual property rights grant can be '
        r'found\n'
      r'.*? in the file PATENTS\.  All contributing project authors may\n'
      r'.*? be found in the AUTHORS file in the root of the source tree\.\n'
  ) % {
      'year': input_api.time.strftime('%Y'),
  }

  results = []
  # Ideally, maxlen would be 80.
  results.extend(input_api.canned_checks.CheckLongLines(
      input_api, output_api, maxlen=95))
  results.extend(input_api.canned_checks.CheckChangeHasNoTabs(
      input_api, output_api))
  results.extend(input_api.canned_checks.CheckLicense(
      input_api, output_api, webrtc_license_header))

  return results

def CheckChangeOnCommit(input_api, output_api):
  results = []
  results.extend(input_api.canned_checks.CheckOwners(input_api, output_api))
  return results
