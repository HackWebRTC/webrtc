#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

import random
import re
import string
import unicodedata

from common.fuzz_parameters import FillInParameter, MissingParameterException
from common.random_javascript import *


def _ArrayOfRandomRolls(num_rolls):
  return str([random.random() for x in xrange(num_rolls)])


def _RandomAudioOrVideo():
  roll = random.random()
  if roll < 0.5:
    return '{ video: true, audio:true }'
  elif roll < 0.7:
    return '{ video: true, audio: false }'
  elif roll < 0.9:
    return '{ video: false, audio: true }'
  else:
    return '{ video: false, audio: true }'


def _ReturnFirstArgument():
  return 'function(arg) { return arg; }'


def _ReturnRandomUtf8String():
  unicode_glyphs = ''.join(unichr(char) for char in xrange(0x10ffff + 1)
    if unicodedata.category(unichr(char))[0] in ('LMNPSZ'))
  oh_dear = random.sample(unicode_glyphs, random.randint(50, 1500))
  return 'function(arg) { return "%s"; }' % ''.join(oh_dear)


def _ReturnFuzzedSdp():
  return 'function(arg) { return fuzzSdp(arg); }'


def _RandomSdpTransform():
  roll = random.random()
  if roll < 0.1:
    return _ReturnRandomUtf8String()
  elif roll < 0.5:
    return _ReturnFuzzedSdp()
  else:
    return _ReturnFirstArgument()


def _InsertRandomLocationReload(list_of_lines, num_to_insert):
  length = len(list_of_lines)
  assert length > num_to_insert

  # Randomly choose insertion points to insert at (if
  # num_to_insert == length - 1, all will be replaced).
  lines_to_insert_behind = sorted(random.sample(xrange(length - 1),
                                                num_to_insert))

  result = list(list_of_lines)
  num_inserted = 0
  for i in lines_to_insert_behind:
    # We're just guessing the indentation the reloads will be at, but that's
    # just cosmetic anyway.
    result.insert(num_inserted + i + 1, '    location.reload()')
    num_inserted += 1

  return result


def _InsertRandomLocationReloads(file_data, replace_all):
  lines = file_data.split(';\n')
  if replace_all:
    lines = _InsertRandomLocationReload(lines, len(lines) - 1)
  else:
    num_lines_to_insert = random.randint(1, 3)
    lines = _InsertRandomLocationReload(lines, num_lines_to_insert)
  return ';\n'.join(lines)


def _InsertRandomLocationReloadsWithinMarkers(file_data,
                                              insert_everywhere=False):
  """Inserts random location.reload() statements in the file.

  We can insert statements after other statements (e.g. after ; and newline).
  We only consider the text between the "reload injection markers" so that we
  can avoid injecting location.reload()s into the HTML or after variable
  declarations, for instance. Therefore, the markers must be present in the
  file data passed into this function.

  Args:
    file_data: The template file data as a string.
    insert_everywhere: If true, will replace at all possible injection points.
        If false, we will randomly choose 1-3 injection points.
"""
  start_marker = '// START_OF_POSSIBLE_INJECTED_LOCATION_RELOADS'
  end_marker = '// END_OF_POSSIBLE_INJECTED_LOCATION_RELOADS'
  within_markers_regex = re.compile(start_marker + '(.+)' + end_marker,
                                    re.DOTALL)
  within_markers = within_markers_regex.search(file_data)
  if not within_markers:
    raise MissingParameterException(
        'Missing %s and/or %s in template.' % (start_marker, end_marker))

  # Now insert the location.reload()s.
  modified_data = _InsertRandomLocationReloads(
      within_markers.group(1), insert_everywhere)

  return within_markers_regex.sub(modified_data, file_data)


def Fuzz(file_data):
  """Fuzzes the passed in template."""
  file_data = file_data.decode('utf-8')

  # Generate a bunch of random numbers and encode them into the page. Since the
  # values get hard-coded into the page the page's choices will be reproducible.
  file_data = FillInParameter('ARRAY_OF_RANDOM_ROLLS',
                              _ArrayOfRandomRolls(500),
                              file_data)

  # Randomly decide how to fuzz SDP data.
  file_data = FillInParameter('REQUEST_AUDIO_AND_VIDEO',
                              _RandomAudioOrVideo(),
                              file_data)
  file_data = FillInParameter('TRANSFORM_OFFER_SDP',
                              _RandomSdpTransform(),
                              file_data)
  file_data = FillInParameter('TRANSFORM_ANSWER_SDP',
                              _RandomSdpTransform(),
                              file_data)

  # Random location.reload() calls in the call sequence can be challenging for
  # the code to deal with, so insert some here and there.
  if random.random() < 0.3:
    file_data = _InsertRandomLocationReloadsWithinMarkers(file_data)

  return file_data


def MakeWorkingFile(file_data):
  """Fills in arguments to make a basic working file.

  Used for ensuring that the basic template is standards-compliant.
  """
  file_data = file_data.decode('utf-8')

  file_data = FillInParameter('ARRAY_OF_RANDOM_ROLLS',
                              _ArrayOfRandomRolls(500),
                              file_data)
  file_data = FillInParameter('REQUEST_AUDIO_AND_VIDEO',
                              '{ video: true, audio: true }',
                              file_data)
  file_data = FillInParameter('TRANSFORM_OFFER_SDP',
                              _ReturnFirstArgument(),
                              file_data)
  file_data = FillInParameter('TRANSFORM_ANSWER_SDP',
                              _ReturnFirstArgument(),
                              file_data)

  return file_data
