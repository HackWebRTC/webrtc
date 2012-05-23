#!/usr/bin/env python
#
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Runs an end-to-end audio quality test on Linux.

Expects the presence of PulseAudio virtual devices (null sinks). These are
configured as default devices for a VoiceEngine audio call. A PulseAudio
utility (pacat) is used to play to and record from the virtual devices.

The input reference file is then compared to the output file.
"""

import optparse
import os
import re
import shlex
import subprocess
import sys
import threading
import time

def main(argv):
  parser = optparse.OptionParser()
  usage = 'Usage: %prog [options]'
  parser.set_usage(usage)
  parser.add_option('--input', default='input.pcm', help='input PCM file')
  parser.add_option('--output', default='output.pcm', help='output PCM file')
  parser.add_option('--codec', default='ISAC', help='codec name')
  parser.add_option('--rate', default='16000', help='sample rate in Hz')
  parser.add_option('--channels', default='1', help='number of channels')
  parser.add_option('--play_sink', default='capture',
      help='name of PulseAudio sink to which to play audio')
  parser.add_option('--rec_sink', default='render',
      help='name of PulseAudio sink whose monitor will be recorded')
  parser.add_option('--harness',
      default=os.path.abspath(os.path.dirname(sys.argv[0]) +
          '/../../../out/Debug/audio_e2e_harness'),
      help='path to audio harness executable')
  parser.add_option('--compare',
                    help='command-line arguments for comparison tool')
  parser.add_option('--regexp',
                    help='regular expression to extract the comparison metric')
  (options, args) = parser.parse_args(argv[1:])

  # Set the default capture device to be used by VoiceEngine. We unfortunately
  # need to do this rather than select the devices directly through the harness
  # because monitor sources don't appear in VoiceEngine except as defaults.
  #
  # We pass the render device for VoiceEngine to select because (for unknown
  # reasons) the virtual device is sometimes not used when the default.
  retcode = subprocess.call(['pacmd', 'set-default-source',
      options.play_sink + '.monitor'], stdout=subprocess.PIPE);
  if retcode != 0:
    return retcode

  command = [options.harness, '--render=' + options.rec_sink,
      '--codec=' + options.codec, '--rate=' + options.rate]
  print ' '.join(command)
  voe_proc = subprocess.Popen(command)

  format_args = ['-n', '--format=s16le', '--rate=' + options.rate,
      '--channels=' + options.channels, '--raw']
  command = (['pacat', '-p', '-d', options.play_sink] + format_args +
      [options.input])
  print ' '.join(command)
  play_proc = subprocess.Popen(command)

  # If recording starts before there is data available, pacat sometimes
  # inexplicably adds a large delay to the start of the file. We wait here in
  # an attempt to prevent that.
  time.sleep(0.2)
  command = (['pacat', '-r', '-d', options.rec_sink + '.monitor'] +
      format_args + [options.output])
  print ' '.join(command)
  record_proc = subprocess.Popen(command)

  retcode = play_proc.wait()
  # If these ended early, an exception will be thrown here.
  record_proc.kill()
  voe_proc.kill()
  if retcode != 0:
    return retcode

  if options.compare and options.regexp:
    command = shlex.split(options.compare) + [options.input, options.output]
    print ' '.join(command)
    compare_proc = subprocess.Popen(command, stdout=subprocess.PIPE)
    compare_output = compare_proc.communicate()[0]
    if compare_proc.returncode != 0:
      return compare_proc.returncode

    # The list should only contain one item.
    print ''.join(re.findall(options.regexp, compare_output))

  return 0

if __name__ == '__main__':
  sys.exit(main(sys.argv))
