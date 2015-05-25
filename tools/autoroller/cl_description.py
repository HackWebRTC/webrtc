#!/usr/bin/env python
# Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""Creates a CL description for auto-rolling chromium_revision in WebRTC."""

import argparse
import base64
import collections
import os
import re
import sys
import urllib


CHROMIUM_LKGR_URL = 'https://chromium-status.appspot.com/lkgr'
CHROMIUM_SRC_URL = 'https://chromium.googlesource.com/chromium/src'
CHROMIUM_COMMIT_TEMPLATE = CHROMIUM_SRC_URL + '/+/%s'
CHROMIUM_FILE_TEMPLATE = CHROMIUM_SRC_URL + '/+/%s/%s'

GIT_NUMBER_RE = re.compile('^Cr-Commit-Position: .*#([0-9]+).*$')
CLANG_REVISION_RE = re.compile(r'^CLANG_REVISION=(\d+)$')
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
CHECKOUT_ROOT_DIR = os.path.join(SCRIPT_DIR, os.pardir, os.pardir)
sys.path.append(CHECKOUT_ROOT_DIR)
import setup_links

sys.path.append(os.path.join(CHECKOUT_ROOT_DIR, 'tools'))
import find_depot_tools
find_depot_tools.add_depot_tools_to_path()
from gclient import GClientKeywords

CLANG_UPDATE_SCRIPT_URL_PATH = 'tools/clang/scripts/update.sh'
CLANG_UPDATE_SCRIPT_LOCAL_PATH = os.path.join('tools', 'clang', 'scripts',
                                              'update.sh')

DepsEntry = collections.namedtuple('DepsEntry', 'path url revision')
ChangedDep = collections.namedtuple('ChangedDep', 'path current_rev new_rev')


def parse_deps_dict(deps_content):
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


def parse_local_deps_file(filename):
  with open(filename, 'rb') as f:
    deps_content = f.read()
  return parse_deps_dict(deps_content)


def parse_remote_cr_deps_file(revision):
  deps_content = read_remote_cr_file('DEPS', revision)
  return parse_deps_dict(deps_content)

def parse_git_number(commit_message):
  for line in reversed(commit_message.splitlines()):
    m = GIT_NUMBER_RE.match(line.strip())
    if m:
      return m.group(1)
  print 'Failed to parse svn revision id from:\n%s\n' % commit_message
  sys.exit(-1)

def _read_gittiles_content(url):
  # Download and decode BASE64 content until
  # https://code.google.com/p/gitiles/issues/detail?id=7 is fixed.
  base64_content = read_url_content(url + '?format=TEXT')
  return base64.b64decode(base64_content[0])

def read_remote_cr_file(path_below_src, revision):
  """Reads a remote Chromium file of a specific revision. Returns a string."""
  return _read_gittiles_content(CHROMIUM_FILE_TEMPLATE % (revision,
                                                         path_below_src))

def read_remote_cr_commit(revision):
  """Reads a remote Chromium commit message. Returns a string."""
  return _read_gittiles_content(CHROMIUM_COMMIT_TEMPLATE % revision)

def read_url_content(url):
  """Connect to a remote host and read the contents. Returns a list of lines."""
  try:
    conn = urllib.urlopen(url)
    return conn.readlines()
  except IOError as e:
    print >> sys.stderr, 'Error connecting to %s. Error: ' % url, e
    return None
  finally:
    conn.close()


def get_matching_deps_entries(depsentry_dict, dir_path):
  """Gets all deps entries matching the provided path

  This list may contain more than one DepsEntry object.
  Example: dir_path='src/testing' would give results containing both
  'src/testing/gtest' and 'src/testing/gmock' deps entries for Chromium's DEPS.

  Returns:
    A list DepsEntry objects.
  """
  result = []
  for path, depsentry in depsentry_dict.iteritems():
    if (path == dir_path or
        path.startswith(dir_path) and path[len(dir_path):][0] == '/'):
      result.append(depsentry)
  return result

def build_depsentry_dict(deps_dict):
  """Builds a dict of DepsEntry object from a raw parsed deps dict."""
  result = {}
  def add_depsentries(deps_subdict):
    for path, deps_url in deps_subdict.iteritems():
      if not result.has_key(path):
        url, revision = deps_url.split('@') if deps_url else (None, None)
        result[path] = DepsEntry(path, url, revision)

  add_depsentries(deps_dict['deps'])
  for deps_os in ['win', 'mac', 'unix', 'android', 'ios', 'unix']:
    add_depsentries(deps_dict['deps_os'].get(deps_os, {}))
  return result

def calculate_changed_deps(current_deps, new_deps):
  result = []
  current_entries = build_depsentry_dict(current_deps)
  new_entries = build_depsentry_dict(new_deps)

  all_deps_dirs = setup_links.DIRECTORIES
  for deps_dir in all_deps_dirs:
    # All deps have 'src' prepended to the path in the Chromium DEPS file.
    dir_path = 'src/%s' % deps_dir

    for entry in get_matching_deps_entries(current_entries, dir_path):
      new_matching_entries = get_matching_deps_entries(new_entries, entry.path)
      assert len(new_matching_entries) <= 1, (
          'Should never find more than one entry matching %s in %s, found %d' %
          (entry.path, new_entries, len(new_matching_entries)))
      if not new_matching_entries:
        result.append(ChangedDep(entry.path, entry.revision, 'None'))
      elif entry != new_matching_entries[0]:
        result.append(ChangedDep(entry.path, entry.revision,
                                 new_matching_entries[0].revision))
  return result


def calculate_changed_clang(new_cr_rev):
  def get_clang_rev(lines):
    for line in lines:
      match = CLANG_REVISION_RE.match(line)
      if match:
        return match.group(1)
    return None

  chromium_src_path = os.path.join(CHECKOUT_ROOT_DIR, 'chromium', 'src',
                                   CLANG_UPDATE_SCRIPT_LOCAL_PATH)
  with open(chromium_src_path, 'rb') as f:
    current_lines = f.readlines()
  current_rev = get_clang_rev(current_lines)

  new_clang_update_sh = read_remote_cr_file(CLANG_UPDATE_SCRIPT_URL_PATH,
                                            new_cr_rev).splitlines()
  new_rev = get_clang_rev(new_clang_update_sh)
  return ChangedDep(CLANG_UPDATE_SCRIPT_LOCAL_PATH, current_rev, new_rev)


def generate_commit_message(current_cr_rev, new_cr_rev, changed_deps_list,
                            clang_change):
  current_cr_rev = current_cr_rev[0:7]
  new_cr_rev = new_cr_rev[0:7]
  rev_interval = '%s..%s' % (current_cr_rev, new_cr_rev)

  current_git_number = parse_git_number(read_remote_cr_commit(current_cr_rev))
  new_git_number = parse_git_number(read_remote_cr_commit(new_cr_rev))
  git_number_interval = '%s:%s' % (current_git_number, new_git_number)

  commit_msg = ['Roll chromium_revision %s (%s)' % (rev_interval,
                                                    git_number_interval)]

  if changed_deps_list:
    commit_msg.append('\nRelevant changes:')

    for c in changed_deps_list:
      commit_msg.append('* %s: %s..%s' % (c.path, c.current_rev[0:7],
                                          c.new_rev[0:7]))

    change_url = CHROMIUM_FILE_TEMPLATE % (rev_interval, 'DEPS')
    commit_msg.append('Details: %s' % change_url)

  if clang_change.current_rev != clang_change.new_rev:
    commit_msg.append('\nClang version changed %s:%s' %
                      (clang_change.current_rev, clang_change.new_rev))
    change_url = CHROMIUM_FILE_TEMPLATE % (rev_interval,
                                           CLANG_UPDATE_SCRIPT_URL_PATH)
    commit_msg.append('Details: %s' % change_url)
  else:
    commit_msg.append('\nClang version was not updated in this roll.')
  return commit_msg


def main():
  p = argparse.ArgumentParser()
  p.add_argument('-r', '--revision',
                 help=('Chromium Git revision to roll to. Defaults to the '
                       'Chromium LKGR revision if omitted.'))
  opts = p.parse_args()

  if not opts.revision:
    lkgr_contents = read_url_content(CHROMIUM_LKGR_URL)
    print 'No revision specified. Using LKGR: %s' % lkgr_contents[0]
    opts.revision = lkgr_contents[0]

  local_deps = parse_local_deps_file(os.path.join(CHECKOUT_ROOT_DIR, 'DEPS'))
  current_cr_rev = local_deps['vars']['chromium_revision']

  current_cr_deps = parse_remote_cr_deps_file(current_cr_rev)
  new_cr_deps = parse_remote_cr_deps_file(opts.revision)

  changed_deps = sorted(calculate_changed_deps(current_cr_deps, new_cr_deps))
  clang_change = calculate_changed_clang(opts.revision)
  if changed_deps or clang_change:
    commit_msg = generate_commit_message(current_cr_rev, opts.revision,
                                         changed_deps, clang_change)
    print '\n'.join(commit_msg)
  else:
    print ('No deps changes detected when rolling from %s to %s. Aborting '
           'without action.') % (current_cr_rev, opts.revision,)
  return 0

if __name__ == '__main__':
  sys.exit(main())
