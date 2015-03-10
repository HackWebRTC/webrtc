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
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import urllib2


SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
CHROME_SRC = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir))
sys.path.insert(1, os.path.join(CHROME_SRC, 'tools'))
import find_depot_tools
sys.path.append(find_depot_tools.add_depot_tools_to_path())
import rietveld
from gclient import GClientKeywords
from third_party import upload

# Avoid depot_tools/third_party/upload.py print verbose messages.
upload.verbosity = 0  # Errors only.

# Use a shell for subcommands on Windows to get a PATH search.
# TODO(kjellander): Remove the git-svn-id regex once we've rolled past the
# revisions that doesn't have the Cr-Original-Commit-Position footer.
GIT_SVN_ID_RE = re.compile('^git-svn-id: .*@([0-9]+) .*$')
GIT_SVN_ID_RE2 = re.compile('^Cr-Original-Commit-Position: .*#([0-9]+).*$')
CL_ISSUE_RE = re.compile('^Issue number: ([0-9]+) \((.*)\)$')
RIETVELD_URL_RE = re.compile('^https?://(.*)/(.*)')
ROLL_BRANCH_NAME = 'special_webrtc_roll_branch'
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


def RunInteractive(command):
  p = subprocess.Popen(command, shell=USE_SHELL, cwd=CHROME_SRC,
                       universal_newlines=True)
  p.communicate()
  if p.returncode != 0:
    sys.exit(p.returncode)
  return

def RunCommand(command, working_dir=None, ignore_exit_code=False):
  """Runs a command and returns the stdout from that command.
  If the command fails (exit code != 0), the function will exit the process."""
  working_dir = working_dir or CHROME_SRC
  p = subprocess.Popen(command, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, shell=USE_SHELL,
                       cwd=working_dir, universal_newlines=True)
  output = p.stdout.read()
  p.wait()
  p.stdout.close()
  p.stderr.close()

  if not ignore_exit_code and p.returncode != 0:
    print 'Command failed: %s\n' % str(command)
    print output
    sys.exit(p.returncode)

  return output

def ParseSvnRevisionFromGitDescription(description):
  for line in reversed(description.splitlines()):
    m = GIT_SVN_ID_RE.match(line.strip())
    if not m:
       m = GIT_SVN_ID_RE2.match(line.strip())
    if m:
      return m.group(1)
  print 'Failed to parse svn revision id from:\n%s\n' % description
  sys.exit(-1)

def ParseGitCommitFromDescription(description):
  # TODO(kjellander): Consider passing --format=%b to the git log command so we
  # don't need to have error-prone parsing like this.
  for line in description.splitlines():
    if line.startswith('commit '):
      return line.split()[1]
  print 'Failed to parse git commit id from:\n%s\n' % description
  sys.exit(-1)
  return None

def ParseDepsFile(filename):
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

def GetCommitInfo(folder, git_hash=None, git_repo_url=None):
  RunCommand(['git', 'fetch', 'origin'], working_dir=folder)
  revision_range = git_hash or 'origin'
  ret = RunCommand(
      ['git', '--no-pager', 'log', revision_range, '--pretty=full', '-1'],
      working_dir=folder)
  return CommitInfo(ParseSvnRevisionFromGitDescription(ret),
                    ParseGitCommitFromDescription(ret), git_repo_url)

def GetDepsCommitInfo(deps_dict, path_below_src):
  entry = deps_dict['deps']['src/%s' % path_below_src]
  at_index = entry.find('@')
  git_repo_url = entry[:at_index]
  git_hash = entry[at_index + 1:]
  return GetCommitInfo(path_below_src, git_hash, git_repo_url)

def GetCLInfo():
  cl_output = RunCommand(['git', 'cl', 'issue'])
  m = CL_ISSUE_RE.match(cl_output.strip())
  if m:
    issue_number = int(m.group(1))
    url = m.group(2)

    # Parse the Rietveld host from the URL.
    m = RIETVELD_URL_RE.match(url)
    if m:
      rietveld_server = m.group(1)
      return CLInfo(issue_number, url, rietveld_server)
    else:
      print ('Cannot parse Rietveld host from URL: %s' % url)
      sys.exit(-1)
  else:
    print ('Cannot find any CL info. Output was:\n%s' % cl_output)
    sys.exit(-1)

def PrintTrybotStatus(issue, rietveld_server):
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
    print ('No trybots have yet been triggered for https://%s/%d' %
           (rietveld_server, issue))
    return False

  status_to_name = {}
  for trybot_result in tryjob_results:
    status = TRYJOB_STATUS.get(trybot_result['result'], 'UNKNOWN')
    status_to_name.setdefault(status, [])
    status_to_name[status].append(trybot_result['builder'])

  print ('Status for https://%s/%d:' % (rietveld_server, issue))
  for status,name_list in status_to_name.iteritems():
    print '%s: %s' % (status, ','.join(sorted(name_list)))
  print ''

def GetCurrentBranchName():
  return RunCommand(
      ['git', 'rev-parse', '--abbrev-ref', 'HEAD']).splitlines()[0]

def IsTreeClean():
  lines = RunCommand(['git', 'status', '--porcelain']).splitlines()
  if len(lines) == 0:
    return True

  print 'Found dirty/unversioned files:\n%s' % '\n'.join(lines)
  return False

def GitPull():
  RunCommand(['git', 'pull'])

def UpdateReadmeFile(path, new_revision):
  readme = open(path, 'r+')
  txt = readme.read()
  m = re.sub(re.compile('.*^Revision\: ([0-9]*).*', re.MULTILINE),
      ('Revision: %s' % new_revision), txt)
  readme.seek(0)
  readme.write(m)
  readme.truncate()

def GenerateCLDescription(webrtc_current, libjingle_current,
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

def PrepareRoll(dry_run, ignore_checks):
  # TODO(kjellander): use os.path.normcase, os.path.join etc for all paths for
  # cross platform compatibility.

  if not ignore_checks:
    if GetCurrentBranchName() != 'master':
      print 'Please checkout the master branch.'
      return -1
    if not IsTreeClean():
      print 'Please make sure you don\'t have any modified files.'
      return -1

  print 'Checking for a previous roll branch.'
  # TODO(kjellander): switch to the stale branch, close the issue, switch back
  # to master,
  RunCommand(['git', 'branch', '-D', ROLL_BRANCH_NAME], ignore_exit_code=True)
  print 'Pulling latest changes'
  if not ignore_checks:
    GitPull()

  RunCommand(['git', 'checkout', '-b', ROLL_BRANCH_NAME])

  # Modify Chromium's DEPS file.

  # Parse current hashes.
  deps = ParseDepsFile(os.path.join(CHROME_SRC, 'DEPS'))
  webrtc_current = GetDepsCommitInfo(deps, WEBRTC_PATH)
  libjingle_current = GetDepsCommitInfo(deps, LIBJINGLE_PATH)

  # Find ToT revisions.
  webrtc_latest = GetCommitInfo(WEBRTC_PATH)
  libjingle_latest = GetCommitInfo(LIBJINGLE_PATH)

  RunCommand(['roll-dep', WEBRTC_PATH, webrtc_latest.git_commit])
  RunCommand(['roll-dep', LIBJINGLE_PATH, libjingle_latest.git_commit])

  if IsTreeClean():
    print 'No changes detected.'
    RunCommand(['git', 'checkout', 'master'])
    RunCommand(['git', 'branch', '-D', ROLL_BRANCH_NAME])
  else:
    UpdateReadmeFile(LIBJINGLE_README, libjingle_latest.svn_revision)
    description = GenerateCLDescription(webrtc_current, libjingle_current,
                                        webrtc_latest, libjingle_latest)
    print 'Committing changes locally.'
    RunCommand(['git', 'add', '--update', '.'])
    RunCommand(['git', 'commit', '-m', description])
    print 'Uploading changes...'
    RunInteractive(['git', 'cl', 'upload', '-m', description])
    cl_info = GetCLInfo()
    print 'Issue: %d URL: %s' % (cl_info.issue, cl_info.url)

    if not dry_run:
      print 'Starting try jobs...'
      RunCommand(['git', 'cl', 'try'])
      print 'Change in progress. Monitor here:\n%s' % cl_info.url

  # TODO(kjellander): Checkout masters/previous branches again.
  return 0

def GetBranches():
  """Returns a tuple of active,branches where 'active' is the name of the
  currently active branch and 'branches' is a list of all branches.
  """
  lines = RunCommand(['git', 'branch']).split('\n')
  branches = []
  active = ''
  for l in lines:
    if '*' in l:
      # The assumption is that the first char will always be the '*'
      active = l[1:].strip()
      branches.append(active)
    else:
      b = l.strip()
      if b:
        branches.append(b)
  return (active, branches)

def Abort():
  active_branch, branches = GetBranches()
  if active_branch == ROLL_BRANCH_NAME:
    active_branch = 'master'
  if ROLL_BRANCH_NAME in branches:
    print ('Aborting pending roll.')
    RunCommand(['git', 'checkout', ROLL_BRANCH_NAME])
    # Ignore an error here in case an issue wasn't created for some reason.
    RunCommand(['git', 'cl', 'set_close'], ignore_exit_code=True)
    RunCommand(['git', 'checkout', active_branch])
    RunCommand(['git', 'branch', '-D', ROLL_BRANCH_NAME])
  return 0

def PresubmitPassed(presubmit):
  return presubmit.find('** Presubmit ERRORS **') == -1

def Commit():
  # First phase of two.  Run the presubmit step for both repos.
  presubmit_passed = True
  active_branch, branches = GetBranches()
  if ROLL_BRANCH_NAME in branches and presubmit_passed:
    RunCommand(['git', 'checkout', ROLL_BRANCH_NAME])
    presubmit = RunCommand(['git', 'cl', 'presubmit'])
    presubmit_passed = PresubmitPassed(presubmit)
    if not presubmit_passed:
      print 'Presubmit errors\n%s' % presubmit
    RunCommand(['git', 'checkout', active_branch])

  if not presubmit_passed:
    return -1

  # Phase two, we've passed the presubmit test, so let's commit.

  active_branch, branches = GetBranches()
  if active_branch == ROLL_BRANCH_NAME:
    active_branch = 'master'
  if ROLL_BRANCH_NAME in branches:
    print ('Committing change.')
    RunCommand(['git', 'checkout', ROLL_BRANCH_NAME])
    RunCommand(['git', 'rebase', 'master'])
    RunInteractive(['git', 'cl', 'land'])
    # TODO(tommi): Verify that the issue was successfully closed.
    RunCommand(['git', 'checkout', active_branch])
    RunCommand(['git', 'branch', '-D', ROLL_BRANCH_NAME])

  return 0

def Status():
  print '\n========== TRYJOBS STATUS =========='
  active_branch, _ = GetBranches()
  if active_branch != ROLL_BRANCH_NAME:
    RunCommand(['git', 'checkout', ROLL_BRANCH_NAME])
  cl_info = GetCLInfo()
  PrintTrybotStatus(cl_info.issue, cl_info.rietveld_server)
  return 0

def main():
  if sys.platform in ('win32', 'cygwin'):
    print >> sys.stderr, (
        'Unfortunately this script is only supported on Linux and Mac.')
    return -1

  parser = argparse.ArgumentParser(
      description='Find webrtc and libjingle revisions for roll.')
  parser.add_argument('--abort',
    help='Aborts a previously prepared roll. '\
         'Closes any associated issues and deletes the roll branches',
    action='store_true')
  parser.add_argument('--commit',
    help='Commits a prepared roll (that\'s assumed to be green). '\
         'Closes any associated issues and deletes the roll branches',
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
  args = parser.parse_args()

  if args.abort and not args.dry_run:
    return Abort()

  if args.commit and not args.dry_run:
    return Commit()

  if args.status:
    return Status()

  return PrepareRoll(args.dry_run, args.ignore_checks)

if __name__ == '__main__':
  sys.exit(main())
