#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Fuzzer for peerconnection.

Based on the ClusterFuzz simple fuzzer template.

I generally use it like this when developing:

./src/test/fuzz/peerconnection/fuzz_main_run.py --no_of_files=1 \
  --output_dir=. --input_dir=src/test/fuzz/peerconnection/corpus/; \
  cat fuzz-*; mv fuzz-* /home/phoglund/www/fuzz/fuzz.html; \
  cp src/test/fuzz/peerconnection/corpus/* /home/phoglund/www/fuzz/; \
  chmod a+r /home/phoglund/www/fuzz/

Add the --be_nice flag to the fuzzer to generate a page that should be able
to set up a call. If a --be_nice-generated page doesn't get a call up, the
code doesn't work with whatever version of the WebRTC spec your current version
of Chrome implements.
"""

import getopt
import os
import random
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


def GenerateData(be_nice):
  """Generates a html page from the template, with or without fuzzing.

  Args:
    be_nice: If true, we won't fuzz the data but rather produce a complete
        standard-compliant file.

  Returns:
    A tuple (file_data, file_extension).
  """
  this_scripts_path = os.path.dirname(os.path.realpath(__file__))
  corpus_path = os.path.join(this_scripts_path, 'corpus');

  # Choose the newest version of the API more often than the old one.
  if random.random() < 0.8:
    template_to_use = 'template01.html'
  else:
    template_to_use = 'template00.html'
  template = _ReadFile(os.path.join(corpus_path, template_to_use))

  file_extension = 'html'

  if be_nice:
    file_data = peerconnection_fuzz.MakeWorkingFile(template)
  else:
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
  be_nice = False
  optlist, args = getopt.getopt(sys.argv[1:], '', \
      ['no_of_files=', 'output_dir=', 'input_dir=', 'be_nice'])
  for option, value in optlist:
    if option == '--no_of_files':     no_of_files = int(value)
    elif option == '--output_dir':    output_dir = value
    elif option == '--input_dir':     input_dir = value
    elif option == '--be_nice':       be_nice = True
  assert no_of_files is not None, 'Missing "--no_of_files" argument'
  assert output_dir is not None, 'Missing "--output_dir" argument'
  assert input_dir is not None, 'Missing "--input_dir" argument'

  for file_no in range(no_of_files):
    file_data, file_extension = GenerateData(be_nice)
    file_data = file_data.encode('utf-8')
    file_descriptor, file_path = tempfile.mkstemp(
        prefix='fuzz-http-%d-%d' % (start_time, file_no),
        suffix='.' + file_extension,
        dir=output_dir)
    file = os.fdopen(file_descriptor, 'wb')
    print 'Writing %d bytes to "%s"' % (len(file_data), file_path)
    file.write(file_data)
    file.close()