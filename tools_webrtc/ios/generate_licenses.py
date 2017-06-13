#!/usr/bin/python

#  Copyright 2016 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Generates license HTML for a prebuilt version of WebRTC for iOS."""

import sys

import argparse
import cgi
import os
import re
import textwrap
import subprocess


LIB_TO_LICENSES_DICT = {
    'boringssl': ['third_party/boringssl/src/LICENSE'],
    'expat': ['third_party/expat/files/COPYING'],
    'jsoncpp': ['third_party/jsoncpp/LICENSE'],
    'opus': ['third_party/opus/src/COPYING'],
    'protobuf': ['third_party/protobuf/LICENSE'],
    'libsrtp': ['third_party/libsrtp/LICENSE'],
    'usrsctp': ['third_party/usrsctp/LICENSE'],
    'webrtc': ['webrtc/LICENSE', 'webrtc/LICENSE_THIRD_PARTY'],
    'libvpx': ['third_party/libvpx/source/libvpx/LICENSE'],
    'libyuv': ['third_party/libyuv/LICENSE'],
}

SCRIPT_DIR = os.path.dirname(os.path.realpath(sys.argv[0]))
CHECKOUT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))
WEBRTC_ROOT = os.path.join(CHECKOUT_ROOT, 'webrtc')


def GetThirdPartyLibraries(buildfile_dir, target_name):
  def ExtractLibName(string_list):
    # Sample input:
    # ["   //third_party/usrsctp:usrsctp", "    //webrtc:webrtc_common"]
    # Sample output:
    # ["usrsctp"]
    return re.sub(r'\(.*\)', '', string_list).strip().split(
        os.path.sep)[-1].split(':')[0]
  output = subprocess.check_output(
    ["gn", "desc", buildfile_dir, target_name, '--all']) .split(os.linesep)
  return [ExtractLibName(x) for x in output if re.search(r'third_party', x)]


class LicenseBuilder(object):

  def __init__(self, buildfile_dirs, target_name):
    self.buildfile_dirs = buildfile_dirs
    self.target_name = target_name

  def GenerateLicenseText(self, output_dir):
    # Get a list of third_party libs from gn. For fat libraries we must consider
    # all architectures, hence the multiple buildfile directories.
    # The `sum` function flattens the 2d list.
    third_party_libs = sum([GetThirdPartyLibraries(buildfile, self.target_name)
                            for buildfile in self.buildfile_dirs], [])
    assert len(third_party_libs) > 0

    # Generate amalgamated list of libraries. Will exit with error if a
    # lib is unrecognized.
    license_libs = set()
    for static_lib in third_party_libs:
      license_path = LIB_TO_LICENSES_DICT.get(static_lib)
      if static_lib == 'yasm':
        # yasm is a build-time dep only, and doesn't need a license.
        continue
      if license_path is None:
        print 'Missing license path for lib: %s' % static_lib
        return 1
      license_libs.add(static_lib)

    # Put webrtc at the front of the list.
    license_libs = sorted(license_libs)
    license_libs.insert(0, 'webrtc')

    # Generate HTML.
    output_license_file = open(os.path.join(output_dir, 'LICENSE.html'), 'w+')
    output_license_file.write('<!DOCTYPE html>\n')
    output_license_file.write('<html>\n<head>\n')
    output_license_file.write('<meta charset="UTF-8">\n')
    output_license_file.write('<title>Licenses</title>\n')
    style_tag = textwrap.dedent('''\
    <style>
      body { margin: 0; font-family: sans-serif; }
      pre { background-color: #eeeeee; padding: 1em; white-space: pre-wrap; }
      p { margin: 1em; white-space: nowrap; }
    </style>
    ''')
    output_license_file.write(style_tag)
    output_license_file.write('</head>\n')

    for license_lib in license_libs:
      output_license_file.write('<p>%s<br/></p>\n' % license_lib)
      output_license_file.write('<pre>\n')
      for path in LIB_TO_LICENSES_DICT[license_lib]:
        license_path = os.path.join(CHECKOUT_ROOT, path)
        with open(license_path, 'r') as license_file:
          license_text = cgi.escape(license_file.read(), quote=True)
          output_license_file.write(license_text)
          output_license_file.write('\n')
      output_license_file.write('</pre>\n')

    output_license_file.write('</body>\n')
    output_license_file.write('</html>')
    output_license_file.close()
    return 0


def main():
  parser = argparse.ArgumentParser(description='Generate WebRTC LICENSE.html')
  parser.add_argument('target_name',
                      help='Name of the GN target to generate a license for')
  parser.add_argument('output_dir',
                      help='Directory to output LICENSE.html to.')
  parser.add_argument('buildfile_dirs', nargs="+",
                      help='Directories containing gn generated ninja files')
  args = parser.parse_args()
  builder = LicenseBuilder(args.buildfile_dirs, args.target_name)
  sys.exit(builder.GenerateLicenseText(args.output_dir))


if __name__ == '__main__':
  main()
