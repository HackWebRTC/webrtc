#!/usr/bin/env python

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Script for publishing WebRTC AAR on Bintray.

Set BINTRAY_USER and BINTRAY_API_KEY environment variables before running
this script for authentication.
"""

import argparse
import logging
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time


SCRIPT_DIR = os.path.dirname(os.path.realpath(sys.argv[0]))
CHECKOUT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))

sys.path.append(os.path.join(CHECKOUT_ROOT, 'third_party'))
import requests
import jinja2

sys.path.append(os.path.join(CHECKOUT_ROOT, 'tools_webrtc'))
from android.build_aar import BuildAar


ARCHS = ['armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64']
REPOSITORY_API = 'https://api.bintray.com/content/google/webrtc/google-webrtc'
GROUP_ID = 'org/webrtc'
ARTIFACT_ID = 'google-webrtc'
COMMIT_POSITION_REGEX = r'^Cr-Commit-Position: refs/heads/master@{#(\d+)}$'
UPLOAD_TIMEOUT_SECONDS = 10.0
UPLOAD_TRIES = 3
# The sleep time is increased exponentially.
UPLOAD_RETRY_BASE_SLEEP_SECONDS = 2.0


def _ParseArgs():
  parser = argparse.ArgumentParser(description='Releases WebRTC on Bintray.')
  parser.add_argument('--use-goma', action='store_true', default=False,
      help='Use goma.')
  parser.add_argument('--verbose', action='store_true', default=False,
      help='Debug logging.')
  return parser.parse_args()


def _GetCommitHash():
  commit_hash = subprocess.check_output(
    ['git', 'rev-parse', 'HEAD'], cwd=CHECKOUT_ROOT).strip()
  return commit_hash


def _GetCommitPos():
  commit_message = subprocess.check_output(
      ['git', 'rev-list', '--format=%B', '--max-count=1', 'HEAD'],
      cwd=CHECKOUT_ROOT)
  commit_pos_match = re.search(
      COMMIT_POSITION_REGEX, commit_message, re.MULTILINE)
  if not commit_pos_match:
    raise Exception('Commit position not found in the commit message: %s'
                      % commit_message)
  return commit_pos_match.group(1)


def _UploadFile(user, password, filename, version, target_file):
# URL is of format:
  # <repository_api>/<version>/<group_id>/<artifact_id>/<version>/<target_file>
  # Example:
  # https://api.bintray.com/content/google/webrtc/google-webrtc/1.0.19742/org/webrtc/google-webrtc/1.0.19742/google-webrtc-1.0.19742.aar

  target_dir = version + '/' + GROUP_ID + '/' + ARTIFACT_ID + '/' + version
  target_path = target_dir + '/' + target_file
  url = REPOSITORY_API + '/' + target_path

  logging.info('Uploading %s to %s', filename, url)
  with open(filename) as fh:
    file_data = fh.read()

  for attempt in xrange(UPLOAD_TRIES):
    try:
      response = requests.put(url, data=file_data, auth=(user, password),
                              timeout=UPLOAD_TIMEOUT_SECONDS)
      break
    except requests.exceptions.Timeout as e:
      logging.warning('Timeout while uploading: %s', e)
      time.sleep(UPLOAD_RETRY_BASE_SLEEP_SECONDS ** attempt)
  else:
    raise Exception('Failed to upload %s' % filename)

  if not response.ok:
    raise Exception('Failed to upload %s. Response: %s' % (filename, response))
  logging.info('Uploaded %s: %s', filename, response)


def _GeneratePom(target_file, version, commit):
  env = jinja2.Environment(
    loader=jinja2.PackageLoader('release_aar'),
  )
  template = env.get_template('pom.jinja')
  pom = template.render(version=version, commit=commit)
  with open(target_file, 'w') as fh:
    fh.write(pom)


def ReleaseAar(use_goma):
  version = '1.0.' + _GetCommitPos()
  commit = _GetCommitHash()
  logging.info('Releasing AAR version %s with hash %s', version, commit)

  user = os.environ.get('BINTRAY_USER', None)
  api_key = os.environ.get('BINTRAY_API_KEY', None)
  if not user or not api_key:
    raise Exception('Environment variables BINTRAY_USER and BINTRAY_API_KEY '
                    'must be defined.')

  tmp_dir = tempfile.mkdtemp()

  try:
    base_name = ARTIFACT_ID + '-' + version
    aar_file = os.path.join(tmp_dir, base_name + '.aar')
    third_party_licenses_file = os.path.join(tmp_dir, 'LICENSE.md')
    pom_file = os.path.join(tmp_dir, base_name + '.pom')

    logging.info('Building at %s', tmp_dir)
    BuildAar(ARCHS, aar_file,
             use_goma=use_goma,
             ext_build_dir=os.path.join(tmp_dir, 'aar-build'))
    _GeneratePom(pom_file, version, commit)

    _UploadFile(user, api_key, aar_file, version, base_name + '.aar')
    _UploadFile(user, api_key, third_party_licenses_file, version,
                'THIRD_PARTY_LICENSES.md')
    _UploadFile(user, api_key, pom_file, version, base_name + '.pom')
  finally:
    shutil.rmtree(tmp_dir, True)

  logging.info('Library successfully uploaded. Please test and publish it on '
               'Bintray.')


def main():
  args = _ParseArgs()
  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)
  ReleaseAar(args.use_goma)


if __name__ == '__main__':
  sys.exit(main())
