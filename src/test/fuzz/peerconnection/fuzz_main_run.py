#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

# Based on the ClusterFuzz simple fuzzer template.


import getopt
import os
import sys
import tempfile
import time

from common.fuzz_parameters import FillInParameter
import peerconnection_fuzz

# TODO(phoglund): remove duplication wrt getusermedia once this code stabilizes.


def _ReadFile(path):
  file_handle = open(path)
  file_data = file_handle.read()
  file_handle.close()
  return file_data


def _Indent(file_data, num_spaces):
  spaces = ' ' * num_spaces
  return spaces + file_data.replace('\n', '\n' + spaces)


def _IncludeJsFile(js_include_to_replace, js_path, file_data):
  js_file_data = _ReadFile(js_path)
  js_file_data = _Indent(js_file_data, 4)
  js_file_data = ('  <script type="text/javascript">\n' +
                  js_file_data + '\n  </script>\n')
  return FillInParameter(js_include_to_replace, js_file_data, file_data)


def GenerateData():
  this_scripts_path = os.path.dirname(os.path.realpath(__file__))
  corpus_path = os.path.join(this_scripts_path, 'corpus');

  template = _ReadFile(os.path.join(corpus_path, 'template.html'))

  file_extension = 'html'
  file_data = peerconnection_fuzz.Fuzz(template)

  # Paste the javascript code in directly since it's hard to make javascript
  # includes work without data bundles.
  file_data = _IncludeJsFile('INCLUDE_RANDOM_JS',
                             os.path.join(corpus_path, 'random.js'),
                             file_data)
  file_data = _IncludeJsFile('INCLUDE_FUZZ_SDP_JS',
                             os.path.join(corpus_path, 'fuzz_sdp.js'),
                             file_data)

  return file_data, file_extension


if __name__ == '__main__':
  start_time = time.time()

  no_of_files = None
  input_dir = None
  output_dir = None
  optlist, args = getopt.getopt(sys.argv[1:], '', \
      ['no_of_files=', 'output_dir=', 'input_dir='])
  for option, value in optlist:
    if option == '--no_of_files':     no_of_files = int(value)
    elif option == '--output_dir':    output_dir = value
    elif option == '--input_dir':     input_dir = value
  assert no_of_files is not None, 'Missing "--no_of_files" argument'
  assert output_dir is not None, 'Missing "--output_dir" argument'
  assert input_dir is not None, 'Missing "--input_dir" argument'

  for file_no in range(no_of_files):
    file_data, file_extension = GenerateData()
    file_data = file_data.encode('utf-8')
    file_descriptor, file_path = tempfile.mkstemp(
        prefix='fuzz-http-%d-%d' % (start_time, file_no),
        suffix='.' + file_extension,
        dir=output_dir)
    file = os.fdopen(file_descriptor, 'wb')
    print 'Writing %d bytes to "%s"' % (len(file_data), file_path)
    file.write(file_data)
    file.close()