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
import urllib2


SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
ROOT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir, os.pardir))
sys.path.insert(1, os.path.join(ROOT_DIR, 'tools'))
import find_depot_tools
find_depot_tools.add_depot_tools_to_path()
import rietveld
from gclient import GClientKeywords
from third_party import upload

# Avoid depot_tools/third_party/upload.py print verbose messages.
upload.verbosity = 0  # Errors only.

CHROMIUM_GIT_URL = 'https://chromium.googlesource.com/chromium/src.git'
GIT_SVN_ID_RE = re.compile('^Cr-Original-Commit-Position: .*#([0-9]+).*$')
CL_ISSUE_RE = re.compile('^Issue number: ([0-9]+) \((.*)\)$')
RIETVELD_URL_RE = re.compile('^https?://(.*)/(.*)')
ROLL_BRANCH_NAME = 'special_webrtc_roll_branch'

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

CommitInfo = collections.namedtuple('CommitInfo', ['svn_revision',
                                                   'git_commit',
                                                   'git_repo_url'])
CLInfo = collections.namedtuple('CLInfo', ['issue', 'url', 'rietveld_server'])


def _ParseSvnRevisionFromGitDescription(description):
  for line in reversed(description.splitlines()):
    m = GIT_SVN_ID_RE.match(line.strip())
    if m:
      return m.group(1)
  logging.error('Failed to parse svn revision id from:\n%s\n', description)
  sys.exit(-1)


def _ParseGitCommitFromDescription(description):
  # TODO(kjellander): Consider passing --format=%b to the git log command so we
  # don't need to have error-prone parsing like this.
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


def _PrintTrybotStatus(issue, rietveld_server):
  """Prints the status of all trybots for the specified issue.

  Returns:
    True if number of trybots > 0 and all are green. False otherwise.
  """
  assert type(issue) is int

  remote = rietveld.Rietveld('https://' + rietveld_server, None, None)

  # Get patches for the issue so we can use the latest one.
  data = remote.get_issue_properties(issue, messages=False)
  patchsets = data['patchsets']

  # Get trybot status for the latest patch set.
  data = remote.get_patchset_properties(issue, patchsets[-1])

  tryjob_results = data['try_job_results']
  if len(tryjob_results) == 0:
    print 'No trybots have yet been triggered for https://%s/%d' % (
        rietveld_server, issue)
    return False

  status_to_name = {}
  for trybot_result in tryjob_results:
    status = TRYJOB_STATUS.get(trybot_result['result'], 'UNKNOWN')
    status_to_name.setdefault(status, [])
    status_to_name[status].append(trybot_result['builder'])

  # Print these to stdout instead of logging since they will be parsed.
  print ('Status for https://%s/%d:' % (rietveld_server, issue))
  for status,name_list in status_to_name.iteritems():
    print '%s: %s' % (status, ','.join(sorted(name_list)))


def _GenerateCLDescription(webrtc_current, libjingle_current,
                           webrtc_new, libjingle_new):
  delim = ''
  webrtc_str = ''
  def GetChangeLogURL(git_repo_url, current_hash, new_hash):
    return '%s/+log/%s..%s' % (git_repo_url, current_hash[0:7], new_hash[0:7])

  if webrtc_current.git_commit != webrtc_new.git_commit:
    webrtc_str = 'WebRTC %s:%s' % (webrtc_current.svn_revision,
                                   webrtc_new.svn_revision)
    webrtc_changelog_url = GetChangeLogURL(webrtc_current.git_repo_url,
                                           webrtc_current.git_commit,
                                           webrtc_new.git_commit)

  libjingle_str = ''
  if libjingle_current.git_commit != libjingle_new.git_commit:
    if webrtc_str:
      delim += ', '
    libjingle_str = 'Libjingle %s:%s' % (libjingle_current.svn_revision,
                                         libjingle_new.svn_revision)
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
  return description


def _IsChromiumCheckout(checkout_dir):
  """Checks if the provided directory path is a Chromium checkout."""
  # Look for DEPS file and a chromium directory.
  return (os.path.isfile(os.path.join(checkout_dir, 'DEPS')) and
          os.path.isdir(os.path.join(checkout_dir, 'chrome')))


class AutoRoller(object):
  def __init__(self, chromium_src, dry_run, ignore_checks):
    self._chromium_src = chromium_src
    self._dry_run = dry_run
    self._ignore_checks = ignore_checks

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
    return CommitInfo(_ParseSvnRevisionFromGitDescription(ret),
                      _ParseGitCommitFromDescription(ret), git_repo_url)

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

    logging.error('Found dirty/unversioned files:\n%s', '\n'.join(lines))
    return False

  def _UpdateReadmeFile(self, readme_path, new_revision):
    readme = open(os.path.join(self._chromium_src, readme_path), 'r+')
    txt = readme.read()
    m = re.sub(re.compile('.*^Revision\: ([0-9]*).*', re.MULTILINE),
        ('Revision: %s' % new_revision), txt)
    readme.seek(0)
    readme.write(m)
    readme.truncate()

  def PrepareRoll(self):
    # TODO(kjellander): use os.path.normcase, os.path.join etc for all paths for
    # cross platform compatibility.

    if not self._ignore_checks:
      if self._GetCurrentBranchName() != 'master':
        logging.error('Please checkout the master branch.')
        return -1
      if not self._IsTreeClean():
        logging.error('Please make sure you don\'t have any modified files.')
        return -1

    logging.debug('Checking for a previous roll branch.')
    # TODO(kjellander): switch to the stale branch, close the issue, switch back
    # to master,
    self._RunCommand(['git', 'branch', '-D', ROLL_BRANCH_NAME],
                     ignore_exit_code=True)
    logging.debug('Pulling latest changes')
    if not self._ignore_checks:
      self._RunCommand(['git', 'pull'])

    self._RunCommand(['git', 'checkout', '-b', ROLL_BRANCH_NAME])

    # Modify Chromium's DEPS file.

    # Parse current hashes.
    deps = _ParseDepsFile(os.path.join(self._chromium_src, 'DEPS'))
    webrtc_current = self._GetDepsCommitInfo(deps, WEBRTC_PATH)
    libjingle_current = self._GetDepsCommitInfo(deps, LIBJINGLE_PATH)

    # Find ToT revisions.
    webrtc_latest = self._GetCommitInfo(WEBRTC_PATH)
    libjingle_latest = self._GetCommitInfo(LIBJINGLE_PATH)

    self._RunCommand(['roll-dep', WEBRTC_PATH, webrtc_latest.git_commit])
    self._RunCommand(['roll-dep', LIBJINGLE_PATH, libjingle_latest.git_commit])

    if self._IsTreeClean():
      logging.debug('Tree is clean - no changes detected.')
      self._RunCommand(['git', 'checkout', 'master'])
      self._RunCommand(['git', 'branch', '-D', ROLL_BRANCH_NAME])
    else:
      self._UpdateReadmeFile(LIBJINGLE_README, libjingle_latest.svn_revision)
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

      if not self._dry_run:
        logging.debug('Starting try jobs...')
        self._RunCommand(['git', 'cl', 'try'])
        logging.debug('Change in progress. Monitor here:\n%s', cl_info.url)

    # TODO(kjellander): Checkout masters/previous branches again.
    return 0

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

  def Commit(self):
    def PresubmitPassed(presubmit):
      return presubmit.find('** Presubmit ERRORS **') == -1

    # First phase of two.  Run the presubmit step for both repos.
    presubmit_passed = True
    active_branch, branches = self._GetBranches()
    if ROLL_BRANCH_NAME in branches and presubmit_passed:
      self._RunCommand(['git', 'checkout', ROLL_BRANCH_NAME])
      presubmit = self._RunCommand(['git', 'cl', 'presubmit'])
      presubmit_passed = PresubmitPassed(presubmit)
      if not presubmit_passed:
        logging.error('Presubmit errors\n%s', presubmit)
      self._RunCommand(['git', 'checkout', active_branch])

    if not presubmit_passed:
      return -1

    # Phase two, we've passed the presubmit test, so let's commit.

    active_branch, branches = self._GetBranches()
    if active_branch == ROLL_BRANCH_NAME:
      active_branch = 'master'
    if ROLL_BRANCH_NAME in branches and not self._dry_run:
      print 'Committing change.'
      self._RunCommand(['git', 'checkout', ROLL_BRANCH_NAME])
      self._RunCommand(['git', 'rebase', 'master'])
      self._RunCommand(['git', 'cl', 'land', '-f'])
      # TODO(tommi): Verify that the issue was successfully closed.
      self._RunCommand(['git', 'checkout', active_branch])
      self._RunCommand(['git', 'branch', '-D', ROLL_BRANCH_NAME])

    return 0

  def Status(self):
    print '\n========== TRYJOBS STATUS =========='
    active_branch, _ = self._GetBranches()
    if active_branch != ROLL_BRANCH_NAME:
      self._RunCommand(['git', 'checkout', ROLL_BRANCH_NAME])
    cl_info = self._GetCLInfo()
    _PrintTrybotStatus(cl_info.issue, cl_info.rietveld_server)
    return 0


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
  parser.add_argument('--commit',
    help=('Commits a prepared roll (that\'s assumed to be green). '
          'Closes any associated issues and deletes the roll branches'),
    action='store_true')
  parser.add_argument('--status',
    help='Display tryjob status for a previously created roll.',
    action='store_true')
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

  autoroller = AutoRoller(args.chromium_checkout, args.dry_run,
                          args.ignore_checks)
  if args.abort:
    return autoroller.Abort()
  elif args.commit:
    return autoroller.Commit()
  elif args.status:
    return autoroller.Status()
  else:
    return autoroller.PrepareRoll()

if __name__ == '__main__':
  sys.exit(main())
