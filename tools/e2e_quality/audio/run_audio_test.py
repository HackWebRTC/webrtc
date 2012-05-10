#!/usr/bin/env python
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

__author__ = 'andrew@webrtc.org (Andrew MacDonald)'

import optparse
import os
import shlex
import subprocess
import sys
import threading
import time

"""Runs an end-to-end audio quality test on Linux.

Expects the presence of PulseAudio virtual devices (null sinks). These are
configured as default devices for a VoiceEngine audio call. A PulseAudio
utility (pacat) is used to play to and record from the virtual devices.

The input reference file is then compared to the output file.
"""

def popen_and_call(popen_args, call_on_exit):
  """Executes the arguments, and triggers the callback when finished."""

  def run_in_thread(popen_args, call_on_exit):
    proc = subprocess.Popen(popen_args)
    proc.wait()
    call_on_exit()
    return
  thread = threading.Thread(target=run_in_thread,
                            args=(popen_args, call_on_exit))
  thread.start()
  return thread

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
      default=os.path.dirname(sys.argv[0]) +
          '/../../../out/Debug/audio_e2e_harness',
      help='path to audio harness executable')
  (options, args) = parser.parse_args(argv[1:])

  # Set default devices to be used by VoiceEngine.
  subprocess.call(['pacmd', 'set-default-sink', options.rec_sink]);
  subprocess.call(['pacmd', 'set-default-source',
      options.play_sink + '.monitor']);

  print 'Start an audio call'
  print options.harness
  voe_proc = subprocess.Popen([options.harness,
      '--codec=' + options.codec, '--rate=' + options.rate]);

  print 'Start recording to ' + options.output
  format_args = ('-n --format=s16le --rate=' + options.rate + ' --channels=' +
      options.channels + ' --raw')
  command = ('pacat -r -d ' + options.rec_sink + '.monitor ' + format_args +
      ' ' + options.output)
  record_proc = subprocess.Popen(shlex.split(command))
  def stop_recording():
    record_proc.kill()

  print 'Start playing from ' + options.input
  command = ('pacat -p -d ' + options.play_sink + ' ' + format_args + ' ' +
      options.input)
  popen_and_call(shlex.split(command), stop_recording)

  # record_proc will be killed after playout finishes.
  record_proc.wait()

  print 'Shutdown audio call'
  voe_proc.kill()

  # TODO(andrew): compare files.

if __name__ == '__main__':
  sys.exit(main(sys.argv))
