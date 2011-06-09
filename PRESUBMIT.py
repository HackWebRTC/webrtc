
	


webrtc_license_header = (
      r'.*? Copyright \(c\) 2011 The WebRTC project authors'
      r'.*?Use of this source code is governed by a BSD-style license\n'
      r'.*? that can be found in the LICENSE file in the root of the source\n'
      r'.*? tree. An additional intellectual property rights grant can be found\n'
r'.*? in the file PATENTS.  All contributing project authors may\n'
r'.*? be found in the AUTHORS file in the root of the source tree\n'
  ) 


def CheckChangeOnUpload(input_api, output_api):
  results = []
  results.extend(input_api.canned_checks.CheckLongLines(input_api, output_api,maxlen=95))
  results.extend(input_api.canned_checks.CheckChangeHasNoTabs(input_api, output_api))
  return results


 #results.extend(CheckChangeLintsClean(input_api, output_api))
 #results.extend(input_api.canned_checks.CheckLicense(input_api, output_api, license_re=webrtc_license_header))
  


def CheckChangeOnCommit (input_api, output_api):
  results = []
  results.extend(input_api.canned_checks.CheckOwners(input_api, output_api))
  return results




#sources = lambda x: input_api.FilterSourceFile(x, black_list=black_list)

