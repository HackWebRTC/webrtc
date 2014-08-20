#!/usr/bin/env python
# Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import argparse
import os
import subprocess
import sys


ROOT_DIR = os.path.dirname(os.path.abspath(__file__))


def get_target_os_list():
  try:
    main_gclient = os.path.join(os.path.dirname(ROOT_DIR), '.gclient')
    config_dict = {}
    with open(main_gclient, 'rb') as deps_content:
      exec(deps_content, config_dict)
    return ','.join(config_dict.get('target_os', []))
  except Exception as e:
    print >> sys.stderr, "error while parsing .gclient:", e


def main():
  CR_DIR = os.path.join(ROOT_DIR, 'chromium')

  p = argparse.ArgumentParser()
  p.add_argument('--target-revision', required=True,
                 help='The target chromium git revision [REQUIRED]')
  p.add_argument('--chromium-dir', default=CR_DIR,
                 help=('The path to the chromium directory to sync '
                       '(default: %(default)r)'))
  opts = p.parse_args()
  opts.chromium_dir = os.path.abspath(opts.chromium_dir)

  # Do a quick check to see if we were successful last time to make runhooks
  # sooper fast.
  flag_file = os.path.join(opts.chromium_dir, '.last_sync_chromium')
  if os.path.exists(flag_file):
    with open(flag_file, 'r') as f:
      if f.read() == opts.target_revision:
        print "Chromium already up to date:", opts.target_revision
        return 0
    os.unlink(flag_file)

  env = os.environ.copy()
  env['GYP_CHROMIUM_NO_ACTION'] = '1'
  gclient_cmd = 'gclient.bat' if sys.platform.startswith('win') else 'gclient'
  args = [
      gclient_cmd, 'sync', '--no-history', '--force', '--revision',
      'src@'+opts.target_revision]
  if os.environ.get('CHROME_HEADLESS') == '1':
    args.append('-vvv')

  target_os_list = get_target_os_list()
  if target_os_list:
    args += ['--deps=' + target_os_list]

  print 'Running "%s" in %s' % (' '.join(args), opts.chromium_dir)
  ret = subprocess.call(args, cwd=opts.chromium_dir, env=env)
  if ret == 0:
    with open(flag_file, 'wb') as f:
      f.write(opts.target_revision)

  return ret


if __name__ == '__main__':
  sys.exit(main())
