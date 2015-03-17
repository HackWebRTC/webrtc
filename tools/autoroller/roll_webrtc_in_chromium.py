#!/usr/bin/env python
# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

import argparse
import collections
import getpass
import json
import logging
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import time
import urllib2


SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
ROOT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))
sys.path.insert(1, os.path.join(ROOT_DIR, 'tools'))
import find_depot_tools
find_depot_tools.add_depot_tools_to_path()
import rietveld
import roll_dep
from gclient import GClientKeywords
from third_party import upload

# Avoid depot_tools/third_party/upload.py print verbose messages.
upload.verbosity = 0  # Errors only.

CHROMIUM_GIT_URL = 'https://chromium.googlesource.com/chromium/src.git'
COMMIT_POSITION_RE = re.compile('^Cr-Original-Commit-Position: .*#([0-9]+).*$')
CL_ISSUE_RE = re.compile('^Issue number: ([0-9]+) \((.*)\)$')
RIETVELD_URL_RE = re.compile('^https?://(.*)/(.*)')
ROLL_BRANCH_NAME = 'special_webrtc_roll_branch'
TRYJOB_STATUS_SLEEP_SECONDS = 30

# Use a shell for subcommands on Windows to get a PATH search.
USE_SHELL = sys.platform.startswith('win')
WEBRTC_PATH = 'third_party/webrtc'
LIBJINGLE_PATH = 'third_party/libjingle/source/talk'
LIBJINGLE_README = 'third_party/libjingle/README.chromium'

# Result codes from build/third_party/buildbot_8_4p1/buildbot/status/results.py
# plus the -1 code which is used when there's no result yet.
TRYJOB_STATUS = {
  -1: 'RUNNING',
  0: 'SUCCESS',
  1: 'WARNINGS',
  2: 'FAILURE',
  3: 'SKIPPED',
  4: 'EXCEPTION',
  5: 'RETRY',
}
SUCCESS_STATUS = (0, 1, 3)
FAILURE_STATUS = (2, 4, 5)

CommitInfo = collections.namedtuple('CommitInfo', ['commit_position',
                                                   'git_commit',
                                                   'git_repo_url'])
CLInfo = collections.namedtuple('CLInfo', ['issue', 'url', 'rietveld_server'])


def _ParseGitCommitPosition(description):
  for line in reversed(description.splitlines()):
    m = COMMIT_POSITION_RE.match(line.strip())
    if m:
      return m.group(1)
  logging.error('Failed to parse svn revision id from:\n%s\n', description)
  sys.exit(-1)


def _ParseGitCommitHash(description):
  for line in description.splitlines():
    if line.startswith('commit '):
      return line.split()[1]
  logging.error('Failed to parse git commit id from:\n%s\n', description)
  sys.exit(-1)
  return None


def _ParseDepsFile(filename):
  with open(filename, 'rb') as f:
    deps_content = f.read()
  return _ParseDepsDict(deps_content)


def _ParseDepsDict(deps_content):
  local_scope = {}
  var = GClientKeywords.VarImpl({}, local_scope)
  global_scope = {
    'File': GClientKeywords.FileImpl,
    'From': GClientKeywords.FromImpl,
    'Var': var.Lookup,
    'deps_os': {},
  }
  exec(deps_content, global_scope, local_scope)
  return local_scope


def _WaitForTrybots(issue, rietveld_server):
  """Wait until all trybots have passed or at least one have failed.

  Returns:
    An exit code of 0 if all trybots passed or non-zero otherwise.
  """
  assert type(issue) is int
  print 'Trybot status for https://%s/%d:' % (rietveld_server, issue)
  remote = rietveld.Rietveld('https://' + rietveld_server, None, None)

  attempt = 0
  max_tries = 60*60/TRYJOB_STATUS_SLEEP_SECONDS # Max one hour
  while attempt < max_tries:
    # Get patches for the issue so we can use the latest one.
    data = remote.get_issue_properties(issue, messages=False)
    patchsets = data['patchsets']

    # Get trybot status for the latest patch set.
    data = remote.get_patchset_properties(issue, patchsets[-1])

    tryjob_results = data['try_job_results']
    if len(tryjob_results) == 0:
      logging.debug('No trybots have yet been triggered for https://%s/%d' ,
                    rietveld_server, issue)
    else:
      _PrintTrybotsStatus(tryjob_results)
      if any(r['result'] in FAILURE_STATUS for r in tryjob_results):
        logging.error('Found failing tryjobs (see above)')
        return 1
      if all(r['result'] in SUCCESS_STATUS for r in tryjob_results):
        return 0

    logging.debug('Waiting for %d seconds before next check...',
                  TRYJOB_STATUS_SLEEP_SECONDS)
    time.sleep(TRYJOB_STATUS_SLEEP_SECONDS)
    attempt += 1


def _PrintTrybotsStatus(tryjob_results):
  status_to_name = {}
  for trybot_result in tryjob_results:
    status = TRYJOB_STATUS.get(trybot_result['result'], 'UNKNOWN')
    status_to_name.setdefault(status, [])
    status_to_name[status].append(trybot_result['builder'])

  print '\n========== TRYJOBS STATUS =========='
  for status,name_list in status_to_name.iteritems():
    print '%s: %s' % (status, ','.join(sorted(name_list)))


def _GenerateCLDescription(webrtc_current, libjingle_current,
                           webrtc_new, libjingle_new):
  delim = ''
  webrtc_str = ''
  def GetChangeLogURL(git_repo_url, current_hash, new_hash):
    return '%s/+log/%s..%s' % (git_repo_url, current_hash[0:7], new_hash[0:7])

  if webrtc_current.git_commit != webrtc_new.git_commit:
    webrtc_str = 'WebRTC %s:%s' % (webrtc_current.commit_position,
                                   webrtc_new.commit_position)
    webrtc_changelog_url = GetChangeLogURL(webrtc_current.git_repo_url,
                                           webrtc_current.git_commit,
                                           webrtc_new.git_commit)

  libjingle_str = ''
  if libjingle_current.git_commit != libjingle_new.git_commit:
    if webrtc_str:
      delim += ', '
    libjingle_str = 'Libjingle %s:%s' % (libjingle_current.commit_position,
                                         libjingle_new.commit_position)
    libjingle_changelog_url = GetChangeLogURL(libjingle_current.git_repo_url,
                                              libjingle_current.git_commit,
                                              libjingle_new.git_commit)

  description = 'Roll ' + webrtc_str + delim + libjingle_str + '\n\n'
  if webrtc_str:
    description += webrtc_str + '\n'
    description += 'Changes: %s\n\n' % webrtc_changelog_url
  if libjingle_str:
    description += libjingle_str + '\n'
    description += 'Changes: %s\n' % libjingle_changelog_url
  description += '\nTBR='
  return description


def _IsChromiumCheckout(checkout_dir):
  """Checks if the provided directory path is a Chromium checkout."""
  # Look for DEPS file and a chromium directory.
  return (os.path.isfile(os.path.join(checkout_dir, 'DEPS')) and
          os.path.isdir(os.path.join(checkout_dir, 'chrome')))


class AutoRoller(object):
  def __init__(self, chromium_src):
    self._chromium_src = chromium_src

  def _RunCommand(self, command, working_dir=None, ignore_exit_code=False,
                  extra_env=None):
    """Runs a command and returns the stdout from that command.

    If the command fails (exit code != 0), the function will exit the process.
    """
    working_dir = working_dir or self._chromium_src
    logging.debug('cmd: %s cwd: %s', ' '.join(command), working_dir)
    env = os.environ.copy()
    if extra_env:
      logging.debug('extra env: %s', extra_env)
      env.update(extra_env)
    p = subprocess.Popen(command, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, shell=USE_SHELL, env=env,
                         cwd=working_dir, universal_newlines=True)
    output = p.stdout.read()
    p.wait()
    p.stdout.close()
    p.stderr.close()

    if not ignore_exit_code and p.returncode != 0:
      logging.error('Command failed: %s\n%s', str(command), output)
      sys.exit(p.returncode)
    return output

  def _GetCommitInfo(self, path_below_src, git_hash=None, git_repo_url=None):
    working_dir = os.path.join(self._chromium_src, path_below_src)
    self._RunCommand(['git', 'fetch', 'origin'], working_dir=working_dir)
    revision_range = git_hash or 'origin'
    ret = self._RunCommand(
        ['git', '--no-pager', 'log', revision_range, '--pretty=full', '-1'],
        working_dir=working_dir)
    return CommitInfo(_ParseGitCommitPosition(ret), _ParseGitCommitHash(ret),
                      git_repo_url)

  def _GetDepsCommitInfo(self, deps_dict, path_below_src):
    entry = deps_dict['deps']['src/%s' % path_below_src]
    at_index = entry.find('@')
    git_repo_url = entry[:at_index]
    git_hash = entry[at_index + 1:]
    return self._GetCommitInfo(path_below_src, git_hash, git_repo_url)

  def _GetCLInfo(self):
    cl_output = self._RunCommand(['git', 'cl', 'issue'])
    m = CL_ISSUE_RE.match(cl_output.strip())
    if not m:
      logging.error('Cannot find any CL info. Output was:\n%s', cl_output)
      sys.exit(-1)
    issue_number = int(m.group(1))
    url = m.group(2)

    # Parse the Rietveld host from the URL.
    m = RIETVELD_URL_RE.match(url)
    if not m:
      logging.error('Cannot parse Rietveld host from URL: %s', url)
      sys.exit(-1)
    rietveld_server = m.group(1)
    return CLInfo(issue_number, url, rietveld_server)

  def _GetCurrentBranchName(self):
    return self._RunCommand(
        ['git', 'rev-parse', '--abbrev-ref', 'HEAD']).splitlines()[0]

  def _IsTreeClean(self):
    lines = self._RunCommand(['git', 'status', '--porcelain']).splitlines()
    if len(lines) == 0:
      return True

    logging.debug('Dirty/unversioned files:\n%s', '\n'.join(lines))
    return False

  def _UpdateReadmeFile(self, readme_path, new_revision):
    readme = open(os.path.join(self._chromium_src, readme_path), 'r+')
    txt = readme.read()
    m = re.sub(re.compile('.*^Revision\: ([0-9]*).*', re.MULTILINE),
        ('Revision: %s' % new_revision), txt)
    readme.seek(0)
    readme.write(m)
    readme.truncate()

  def PrepareRoll(self, dry_run, ignore_checks, no_commit, close_previous_roll):
    # TODO(kjellander): use os.path.normcase, os.path.join etc for all paths for
    # cross platform compatibility.

    if not ignore_checks:
      if self._GetCurrentBranchName() != 'master':
        logging.error('Please checkout the master branch.')
        return -1
      if not self._IsTreeClean():
        logging.error('Please make sure you don\'t have any modified files.')
        return -1

    logging.debug('Checking for a previous roll branch.')
    if close_previous_roll:
      self.Abort()

    logging.debug('Pulling latest changes')
    if not ignore_checks:
      self._RunCommand(['git', 'pull'])

    self._RunCommand(['git', 'checkout', '-b', ROLL_BRANCH_NAME])

    # Modify Chromium's DEPS file.

    # Parse current hashes.
    deps_filename = os.path.join(self._chromium_src, 'DEPS')
    deps = _ParseDepsFile(deps_filename)
    webrtc_current = self._GetDepsCommitInfo(deps, WEBRTC_PATH)
    libjingle_current = self._GetDepsCommitInfo(deps, LIBJINGLE_PATH)

    # Find ToT revisions.
    webrtc_latest = self._GetCommitInfo(WEBRTC_PATH)
    libjingle_latest = self._GetCommitInfo(LIBJINGLE_PATH)

    self._UpdateDep(deps_filename, WEBRTC_PATH, webrtc_latest)
    self._UpdateDep(deps_filename, LIBJINGLE_PATH, libjingle_latest)

    if self._IsTreeClean():
      logging.debug('Tree is clean - no changes detected.')
      self._DeleteRollBranch()
    else:
      self._UpdateReadmeFile(LIBJINGLE_README, libjingle_latest.commit_position)
      description = _GenerateCLDescription(webrtc_current, libjingle_current,
                                           webrtc_latest, libjingle_latest)
      logging.debug('Committing changes locally.')
      self._RunCommand(['git', 'add', '--update', '.'])
      self._RunCommand(['git', 'commit', '-m', description])
      logging.debug('Uploading changes...')
      self._RunCommand(['git', 'cl', 'upload', '-m', description],
                       extra_env={'EDITOR': 'true'})
      cl_info = self._GetCLInfo()
      logging.debug('Issue: %d URL: %s', cl_info.issue, cl_info.url)

      if not dry_run and not no_commit:
        logging.debug('Sending the CL to the CQ...')
        self._RunCommand(['git', 'cl', 'set_commit'])
        logging.debug('Sent the CL to the CQ. Monitor here: %s', cl_info.url)

    # TODO(kjellander): Checkout masters/previous branches again.
    return 0

  def _UpdateDep(self, deps_filename, dep_relative_to_src, commit_info):
    dep_name = os.path.join('src', dep_relative_to_src)
    comment = 'commit position %s' % commit_info.commit_position

    # roll_dep.py relies on cwd being the Chromium checkout, so let's
    # temporarily change the working directory and then change back.
    cwd = os.getcwd()
    os.chdir(os.path.dirname(deps_filename))
    roll_dep.update_deps(deps_filename, dep_relative_to_src, dep_name,
                         commit_info.git_commit, comment)
    os.chdir(cwd)

  def _DeleteRollBranch(self):
     self._RunCommand(['git', 'checkout', 'master'])
     self._RunCommand(['git', 'branch', '-D', ROLL_BRANCH_NAME])
     logging.debug('Deleted the local roll branch (%s)', ROLL_BRANCH_NAME)


  def _GetBranches(self):
    """Returns a tuple of active,branches.

    The 'active' is the name of the currently active branch and 'branches' is a
    list of all branches.
    """
    lines = self._RunCommand(['git', 'branch']).split('\n')
    branches = []
    active = ''
    for l in lines:
      if '*' in l:
        # The assumption is that the first char will always be the '*'.
        active = l[1:].strip()
        branches.append(active)
      else:
        b = l.strip()
        if b:
          branches.append(b)
    return (active, branches)

  def Abort(self):
    active_branch, branches = self._GetBranches()
    if active_branch == ROLL_BRANCH_NAME:
      active_branch = 'master'
    if ROLL_BRANCH_NAME in branches:
      print 'Aborting pending roll.'
      self._RunCommand(['git', 'checkout', ROLL_BRANCH_NAME])
      # Ignore an error here in case an issue wasn't created for some reason.
      self._RunCommand(['git', 'cl', 'set_close'], ignore_exit_code=True)
      self._RunCommand(['git', 'checkout', active_branch])
      self._RunCommand(['git', 'branch', '-D', ROLL_BRANCH_NAME])
    return 0

  def WaitForTrybots(self):
    active_branch, _ = self._GetBranches()
    if active_branch != ROLL_BRANCH_NAME:
      self._RunCommand(['git', 'checkout', ROLL_BRANCH_NAME])
    cl_info = self._GetCLInfo()
    return _WaitForTrybots(cl_info.issue, cl_info.rietveld_server)


def main():
  if sys.platform in ('win32', 'cygwin'):
    logging.error('Only Linux and Mac platforms are supported right now.')
    return -1

  parser = argparse.ArgumentParser(
      description='Find webrtc and libjingle revisions for roll.')
  parser.add_argument('--chromium-checkout', type=str,
    help=('Path to the Chromium checkout (src/ dir) to perform the roll in '
          '(optional if the current working dir is a Chromium checkout).'))
  parser.add_argument('--abort',
    help=('Aborts a previously prepared roll. '
          'Closes any associated issues and deletes the roll branches'),
    action='store_true')
  parser.add_argument('--no-commit',
    help=('Don\'t send the CL to the CQ. This is useful if additional changes '
          'are needed to the CL (like for API changes).'),
    action='store_true')
  parser.add_argument('--wait-for-trybots',
    help=('Waits until all trybots from a previously created roll are either '
          'successful or at least one has failed. This is useful to be able to '
          'continuously run this script but not initiating new rolls until a '
          'previous one is known to have passed or failed.'),
    action='store_true')
  parser.add_argument('--close-previous-roll', action='store_true',
                      help='Abort a previous roll if one exists.')
  parser.add_argument('--dry-run', action='store_true', default=False,
      help='Create branches and CLs but doesn\'t send tryjobs or commit.')
  parser.add_argument('--ignore-checks', action='store_true', default=False,
      help=('Skips checks for being on the master branch, dirty workspaces and '
            'the updating of the checkout. Will still delete and create local '
            'Git branches.'))
  parser.add_argument('-v', '--verbose', action='store_true', default=False,
      help='Be extra verbose in printing of log messages.')
  args = parser.parse_args()

  if args.verbose:
    logging.basicConfig(level=logging.DEBUG)
  else:
    logging.basicConfig(level=logging.ERROR)

  if args.chromium_checkout:
    if not _IsChromiumCheckout(args.chromium_checkout):
      logging.error('Cannot find the specified Chromium checkout at: %s',
                    args.chromium_checkout)
      return -2
  else:
    # Try fallback on current working directory:
    cwd = os.getcwd()
    if _IsChromiumCheckout(cwd):
      args.chromium_checkout = cwd
    else:
      logging.error(
         '--chromium-checkout not specified and the current working directory '
         ' is not a Chromium checkout. Fix either and try again.')
      return -2

  autoroller = AutoRoller(args.chromium_checkout)
  if args.abort:
    return autoroller.Abort()
  elif args.wait_for_trybots:
    return autoroller.WaitForTrybots()
  else:
    return autoroller.PrepareRoll(args.dry_run, args.ignore_checks,
                                  args.no_commit, args.close_previous_roll)

if __name__ == '__main__':
  sys.exit(main())
