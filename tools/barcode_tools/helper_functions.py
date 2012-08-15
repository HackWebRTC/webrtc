#!/usr/bin/env python
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import os
import subprocess
import sys

_DEFAULT_PADDING = 4


class HelperError(Exception):
  """Exception raised for errors in the helper."""
  pass


def zero_pad(number, padding=_DEFAULT_PADDING):
  """Converts an int into a zero padded string.

  Args:
    number(int): The number to convert.
    padding(int): The number of chars in the output. Note that if you pass for
      example number=23456 and padding=4, the output will still be '23456',
      i.e. it will not be cropped. If you pass number=2 and padding=4, the
      return value will be '0002'.
  Return:
    (string): The zero padded number converted to string.
  """
  return str(number).zfill(padding)


def delete_file(file_name):
  """Deletes the file with file_name.

  Args:
    file_name(string): The file to be deleted.
  Return:
    (bool): True on success, False otherwise.
  """
  try:
    subprocess.check_call(['rm', '%s' % file_name])
  except subprocess.CalledProcessError, err:
    sys.stderr.write('Error in deleting file %s' % file_name)
    return False
  return True


def run_shell_command(command, msg=None):
  """Executes a command.

  Args:
    command(list): Command list to execute.
    msg(string): Message describing the error in case the command fails.

  Return:
    (string): The standard output from running the command.

  Raise:
    HelperError: If command fails.
  """
  cmd_list = [str(x) for x in command]
  cmd = ' '.join(cmd_list)

  process = subprocess.Popen(cmd_list, stdout=subprocess.PIPE,
                             stderr=subprocess.PIPE)
  output, error = process.communicate()
  if process.returncode != 0:
    if msg:
      print msg
    raise HelperError('Failed to run %s: command returned %d and printed '
                      '%s and %s' % (cmd, process.returncode, output, error))
  return output.strip()


def form_jars_string(path_to_zxing):
  """Forms the the Zxing core and javase jars argument.

  Args:
    path_to_zxing(string): The path to the Zxing checkout folder.
  Return:
    (string): The newly formed jars argument.
  """
  javase_jar = os.path.join(path_to_zxing, "javase", "javase.jar")
  core_jar = os.path.join(path_to_zxing, "core", "core.jar")
  delimiter = ':'
  if os.name != 'posix':
    delimiter = ';'
  return javase_jar + delimiter + core_jar


def perform_action_on_all_files(directory, file_pattern, file_extension,
                                start_number, action, **kwargs):
  """Function that performs a given action on all files matching a pattern.

  It is assumed that the files are named file_patternxxxx.file_extension, where
  xxxx are digits. The file names start from
  file_patern0..start_number>.file_extension.

  Args:
    directory(string): The directory where the files live.
    file_pattern(string): The name pattern of the files.
    file_extension(string): The files' extension.
    start_number(int): From where to start to count frames.
    action(function): The action to be performed over the files.

  Return:
    (bool): Whether performing the action over all files was successful or not.
  """
  file_prefix = os.path.join(directory, file_pattern)
  file_exists = True
  file_number = start_number
  errors = False

  while file_exists:
    zero_padded_file_number = zero_pad(file_number)
    file_name = file_prefix + zero_padded_file_number + '.' + file_extension
    if os.path.isfile(file_name):
      if not action(file_name=file_name, **kwargs):
        errors = True
      file_number += 1
    else:
      file_exists = False
  return not errors


