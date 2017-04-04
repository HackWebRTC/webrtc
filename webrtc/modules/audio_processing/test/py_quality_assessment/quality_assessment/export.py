# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import logging
import os

class HtmlExport(object):

  def __init__(self, output_path, output_filename):
    self._output_path = output_path
    self._output_filename = output_filename

  def export(self, scores):
    logging.debug('%d score names found', len(scores))
    output_filepath = os.path.join(self._output_path, self._output_filename)

    # TODO(alessio): remove once implemented
    with open(output_filepath, 'w') as f:
      f.write('APM Quality Assessment scores\n')

    return output_filepath
