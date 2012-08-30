#!/usr/bin/env python
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import optparse
import os
import subprocess
import sys

import helper_functions

_DEFAULT_BARCODE_WIDTH = 352


def convert_yuv_to_png_files(yuv_file_name, yuv_frame_width, yuv_frame_height,
                             output_directory = '.'):
  """Converts a YUV video file into PNG frames.

  The function uses ffmpeg to convert the YUV file. The output of ffmpeg is in
  the form frame_xxxx.png, where xxxx is the frame number, starting from 0001.

  Args:
    yuv_file_name(string): The name of the YUV file.
    yuv_frame_width(int): The width of one YUV frame.
    yuv_frame_height(int): The height of one YUV frame.
    output_directory(string): The output directory where the PNG frames will be
      stored.

  Return:
    (bool): True if the conversion was OK.
  """
  size_string = str(yuv_frame_width) + 'x' + str(yuv_frame_height)
  output_files_pattern = os.path.join(output_directory, 'frame_%04d.png')
  command = ['ffmpeg', '-s', '%s' % size_string, '-i', '%s'
             % yuv_file_name, '-f', 'image2', '-vcodec', 'png',
             '%s' % output_files_pattern]
  try:
    helper_functions.run_shell_command(
        command, msg='Error during YUV to PNG conversion')
  except helper_functions.HelperError, err:
    print err
    return False
  return True


def decode_frames(barcode_width, barcode_height, input_directory='.',
                  path_to_zxing='zxing-read-only'):
  """Decodes the barcodes overlaid in each frame.

  The function uses the example Java command-line tool from the Zxing
  distribution to decode the barcode in every PNG frame from the input
  directory. The frames should be named frame_xxxx.png, where xxxx is the frame
  number. The frame numbers should be consecutive and should start from 0001.
  The decoding results in a frame_xxxx.txt file for every successfully decoded
  barcode. This file contains the decoded barcode as 12-digit string (UPC-A
  format: 11 digits content + one check digit).

  Args:
    barcode_width(int): Width of the barcode.
    barcode_height(int): Height of the barcode.
    input_directory(string): The input directory from where the PNG frames are
      read.
    path_to_zxing(string): The path to Zxing.
  Return:
    (bool): True if the decoding went without errors.
  """
  jars = helper_functions.form_jars_string(path_to_zxing)
  command_line_decoder ='com.google.zxing.client.j2se.CommandLineRunner'
  return helper_functions.perform_action_on_all_files(
      directory=input_directory, file_pattern='frame_',
      file_extension='png', start_number=1, action=_decode_barcode_in_file,
      barcode_width=barcode_width, barcode_height=barcode_height, jars=jars,
      command_line_decoder=command_line_decoder)


def _decode_barcode_in_file(file_name, barcode_width, barcode_height, jars,
                            command_line_decoder):
  """Decodes the barcode in the upper left corner of a PNG file.

  Args:
    file_name(string): File name of the PNG file.
    barcode_width(int): Width of the barcode (in pixels).
    barcode_height(int): Height of the barcode (in pixels)
    jars(string): The Zxing core and javase string.
    command_line_decoder(string): The ZXing command-line decoding tool.

  Return:
    (bool): True upon success, False otherwise.
  """
  command = ['java', '-cp', '%s' % jars,
             '%s' % command_line_decoder, '--products_only',
             '--dump_results', '--brief', '--crop=%d,%d,%d,%d' %
             (0, 0, barcode_width, barcode_height),
             '%s' % file_name]
  try:
    out = helper_functions.run_shell_command(
        command, msg='Error during decoding of %s' % file_name)
    if not 'Success' in out:
      sys.stderr.write('Barcode in %s cannot be decoded\n' % file_name)
      return False
  except helper_functions.HelperError, err:
    print err
    return False
  return True


def _generate_stats_file(stats_file_name, input_directory='.'):
  """Generate statistics file.

  The function generates a statistics file. The contents of the file are in the
  format <frame_name> <barcode>, where frame name is the name of every frame
  (effectively the frame number) and barcode is the decoded barcode. The frames
  and the helper .txt files are removed after they have been used.
  """
  file_prefix = os.path.join(input_directory, 'frame_')
  stats_file = open(stats_file_name, 'w')

  for i in range(1, _count_frames_in(input_directory=input_directory) + 1):
    frame_number = helper_functions.zero_pad(i)
    barcode_file_name = file_prefix + frame_number + '.txt'
    png_frame = file_prefix + frame_number + '.png'
    entry_frame_number = helper_functions.zero_pad(i-1)
    entry = 'frame_' + entry_frame_number + ' '

    if os.path.isfile(barcode_file_name):
      barcode = _read_barcode_from_text_file(barcode_file_name)
      os.remove(barcode_file_name)

      if _check_barcode(barcode):
        entry += (helper_functions.zero_pad(int(barcode[0:11])) + '\n')
      else:
        entry += 'Barcode error\n'  # Barcode is wrongly detected.
    else:  # Barcode file doesn't exist.
      entry += 'Barcode error\n'

    stats_file.write(entry)
    os.remove(png_frame)

  stats_file.close()


def _read_barcode_from_text_file(barcode_file_name):
  """Reads the decoded barcode for a .txt file.

  Args:
    barcode_file_name(string): The name of the .txt file.
  Return:
    (string): The decoded barcode.
  """
  barcode_file = open(barcode_file_name, 'r')
  barcode = barcode_file.read()
  barcode_file.close()

  return barcode


def _check_barcode(barcode):
  """Check weather the UPC-A barcode was decoded correctly.

  This function calculates the check digit of the provided barcode and compares
  it to the check digit that was decoded.

  Args:
    barcode(string): The barcode (12-digit).
  Return:
    (bool): True if the barcode was decoded correctly.
  """
  if len(barcode) != 12:
    return False

  r1 = range(0, 11, 2)  # Odd digits
  r2 = range(1, 10, 2)  # Even digits except last
  dsum = 0
  # Sum all the even digits
  for i in r1:
    dsum += int(barcode[i])
  # Multiply the sum by 3
  dsum *= 3
  # Add all the even digits except the check digit (12th digit)
  for i in r2:
    dsum += int(barcode[i])
  # Get the modulo 10
  dsum = dsum % 10
  # If not 0 substract from 10
  if dsum != 0:
    dsum = 10 - dsum
  # Compare result and check digit
  return dsum == int(barcode[11])


def _count_frames_in(input_directory = '.'):
  """Calculates the number of frames in the input directory.

  The function calculates the number of frames in the input directory. The
  frames should be named frame_xxxx.png, where xxxx is the number of the frame.
  The numbers should start from 1 and should be consecutive.

  Args:
    input_directory(string): The input directory.
  Return:
    (int): The number of frames.
  """
  file_prefix = os.path.join(input_directory, 'frame_')
  file_exists = True
  num = 1

  while file_exists:
    file_name = (file_prefix + helper_functions.zero_pad(num) + '.png')
    if os.path.isfile(file_name):
      num += 1
    else:
      file_exists = False
  return num - 1


def _parse_args():
  """Registers the command-line options."""
  usage = "usage: %prog [options]"
  parser = optparse.OptionParser(usage=usage)

  parser.add_option('--yuv_frame_width', type='int', default=352,
                    help=('Width of the YUV file\'s frames. '
                          'Default: %default'))
  parser.add_option('--yuv_frame_height', type='int', default=288,
                    help=('Height of the YUV file\'s frames. '
                          'Default: %default'))
  parser.add_option('--barcode_width', type='int',
                    default=_DEFAULT_BARCODE_WIDTH,
                    help=('Width of the barcodes. Default: %default'))
  parser.add_option('--barcode_height', type='int', default=32,
                    help=('Height of the barcodes. Default: %default'))
  parser.add_option('--yuv_file', type='string', default='output.yuv',
                    help=('The YUV file to be decoded. Default: %default'))
  parser.add_option('--stats_file', type='string', default='stats.txt',
                    help=('The output stats file. Default: %default'))
  parser.add_option('--png_output_dir', type='string', default='.',
                    help=('The output directory for the generated PNG files. '
                          'Default: %default'))
  parser.add_option('--png_input_dir', type='string', default='.',
                    help=('The input directory for the generated PNG files. '
                          'Default: %default'))
  parser.add_option('--path_to_zxing', type='string', default='zxing',
                    help=('The path to Zxing. Default: %default'))
  options = parser.parse_args()[0]
  return options


def _main():
  """The main function.

  A simple invocation is:
  ./tools/barcode_tolls/barcode_decoder.py
  --yuv_file=<path_and_name_of_overlaid_yuv_video>
  --yuv_frame_width=352 --yuv_frame_height=288 --barcode_height=32
  --stats_file=<path_and_name_to_stats_file>
  """
  options = _parse_args()

  # The barcodes with will be different than the base frame width only if
  # explicitly specified at the command line.
  if options.barcode_width == _DEFAULT_BARCODE_WIDTH:
    options.barcode_width = options.yuv_frame_width

  script_dir = os.path.dirname(os.path.abspath(sys.argv[0]))
  zxing_dir = os.path.join(script_dir, 'third_party', 'zxing')

  # Convert the overlaid YUV video into a set of PNG frames.
  convert_yuv_to_png_files(options.yuv_file, options.yuv_frame_width,
                           options.yuv_frame_height,
                           output_directory=options.png_output_dir)
  # Decode the barcodes from the PNG frames.
  decode_frames(options.barcode_width, options.barcode_height,
                input_directory=options.png_input_dir, path_to_zxing=zxing_dir)
  # Generate statistics file.
  _generate_stats_file(options.stats_file,
                       input_directory=options.png_input_dir)


if __name__ == '__main__':
  sys.exit(_main())
