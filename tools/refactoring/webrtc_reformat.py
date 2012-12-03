#!/usr/bin/python
# Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""WebRTC reformat script.

This script is used to reformat WebRTC code from the old code style to Google
C++ code style.

You need to have astyle (http://astyle.sourceforge.net/) in your path. You also
need to put the following contents into ~/.astylerc:

# =======================COPY==============================================
# Google C++ style guide settings.
indent=spaces=2 # Indentation uses two spaces.
style=attach # Attach braces.

indent-switches
indent-preprocessor # Indent preprocessor continuation lines.
min-conditional-indent=0 # Align conditional continuation with "(".
                         # e.g. if (foo &&
                         #          bar
max-instatement-indent=80 # Try not to mess with current alignment.

pad-oper # Padding around operators.
pad-header # Padding after if, for etc.
unpad-paren # No padding around parentheses.
align-pointer=type # e.g. int* foo

convert-tabs # Convert non-indentation tabs as well.

# The following are available in the unreleased svn repo.
# Behvaiour isn't quite what we'd like; more testing needed.
#max-code-length=80
#break-after-logical

lineend=linux
# =========================================================================
"""

# TODO(mflodman)
# x s/type *var/type* var/g
# x : list indention -> 4 spaces.

__author__ = 'mflodman@webrtc.org (Magnus Flodman)'

import fnmatch
import os
import re
import subprocess
import sys


def LowerWord(obj):
  """Helper for DeCamelCase."""
  return obj.group(1) + '_' + obj.group(2).lower() + obj.group(3)


def DeCamelCase(text):
  """De-camelize variable names."""
  pattern = re.compile(r'(?<=[ _*\(\&\!])([a-z]+)(?<!k)([A-Z]+)([a-z])')
  while re.search(pattern, text):
    text = re.sub(pattern, LowerWord, text)
  return text


def TrimLineEndings(text):
  """Removes trailing white spaces."""
  pattern = re.compile(r'[ ]+(\n)')
  return re.sub(pattern, r'\1', text)


def MoveUnderScore(text):
  """Moves the underscore from beginning of variable name to the end."""
  # TODO(mflodman) Replace \1 with ?-expression.
  # We don't want to change macros and #defines though, so don't do anything
  # if the first character is uppercase (normal variables shouldn't have that).
  pattern = r'([ \*\!\&\(])_(?!_)(?![A-Z])(\w+)'
  return re.sub(pattern, r'\1\2_', text)


def RemoveMultipleEmptyLines(text):
  """Remove all multiple blank lines."""
  pattern = r'[\n]{3,}'
  return re.sub(pattern, '\n\n', text)


def CPPComments(text):
  """Remove all C-comments and replace with C++ comments."""

  # Keep the copyright header style.
  line_list = text.splitlines(True)
  copyright_list = line_list[0:10]
  code_list = line_list[10:]
  copy_text = ''.join(copyright_list)
  code_text = ''.join(code_list)

  # Remove */ for C-comments, don't care about trailing blanks.
  comment_end = re.compile(r'\n[ ]*\*/[ ]*')
  code_text = re.sub(comment_end, '', code_text)
  comment_end = re.compile(r'\*/')
  code_text = re.sub(comment_end, '', code_text)
  # Remove comment lines in the middle of comments, replace with C++ comments.
  comment_star = re.compile(r'(?<=\n)[ ]*(?!\*\w)\*[ ]*')
  code_text = re.sub(comment_star, r'// ', code_text)
  # Remove start of C comment and replace with C++ comment.
  comment_start = re.compile(r'/\*[ ]*\n')
  code_text = re.sub(comment_start, '', code_text)
  comment_start = re.compile(r'/\*[ ]*(.)')
  code_text = re.sub(comment_start, r'// \1', code_text)

  # Add copyright info.
  return copy_text + code_text


def SortIncludeHeaders(text, filename):
  """Sorts all include headers in alphabetic order.

  The file's own header goes first, followed by system headers and then
  project headers. This function will exit if we detect any fancy #ifdef logic
  among the includes - that's a lot harder to sort.

  Args:
    text: The file text.
    filename: The file we are reformatting.

  Returns:
    The text with includes sorted.
  """
  # Get all includes in file.
  include_pattern = re.compile('#include.+\n')
  includes = re.findall(include_pattern, text)

  # Sort system headers and project headers separately.
  sys_includes = []
  project_includes = []
  self_include = ''
  sys_pattern = re.compile('#include <')
  h_filename, _ = os.path.splitext(os.path.basename(filename))

  for item in includes:
    if re.search(h_filename, item):
      self_include = item
    elif re.search(sys_pattern, item):
      sys_includes.append(item)
    else:
      project_includes.append(item)

  sys_includes = sorted(sys_includes)
  project_includes = sorted(project_includes)
  headers = (self_include + '\n' + ''.join(sys_includes) + '\n' +
             ''.join(project_includes))

  # Replace existing headers with the sorted string.
  text_no_hdrs = re.sub(include_pattern, r'???', text)

  # Insert sorted headers unless we detect #ifdefs.
  if re.search(r'(#ifdef|#ifndef|#if).*\?{3,}.*#endif',
               text_no_hdrs, re.DOTALL):
    print 'WARNING: Include headers not sorted in ' + filename
    return text

  return_text = re.sub(r'\?{3,}', headers, text_no_hdrs, 1)
  if re.search(r'\?{3,}', text_no_hdrs):
    # Remove possible remaining ???.
    return_text = re.sub(r'\?{3,}', r'', return_text)

  return return_text


def AddPath(match):
  """Helper for adding file path for WebRTC header files, ignoring other."""
  file_to_examine = match.group(1) + '.h'
  # TODO(mflodman) Use current directory and find webrtc/.
  for path, _, files in os.walk('./webrtc'):
    for filename in files:
      if fnmatch.fnmatch(filename, file_to_examine):
        path_name = os.path.join(path, filename).replace('./', '')
        return '#include "%s"\n' % path_name

  # No path found, return original string.
  return '#include "'+ file_to_examine + '"\n'


def AddHeaderPath(text):
  """Add path to all included header files that have no path yet."""
  headers = re.compile('#include "(.+).h"\n')
  return re.sub(headers, AddPath, text)


def AddWebrtcToOldSrcRelativePath(match):
  file_to_examine = match.group(1) + '.h'
  path, filename = os.path.split(file_to_examine)
  dirs_in_webrtc = [name for name in os.listdir('./webrtc')
                    if os.path.isdir(os.path.join('./webrtc', name))]
  for dir_in_webrtc in dirs_in_webrtc:
    if path.startswith(dir_in_webrtc):
      return '#include "%s"\n' % os.path.join('webrtc', path, filename)
  return '#include "%s"\n' % file_to_examine

def AddWebrtcPrefixToOldSrcRelativePaths(text):
  """For all paths starting with for instance video_engine, add webrtc/."""
  headers = re.compile('#include "(.+).h"\n')
  return re.sub(headers, AddWebrtcToOldSrcRelativePath, text)


def IndentLabels(text):
  """Indent public, protected and private one step (astyle doesn't)."""
  pattern = re.compile('(?<=\n)(public:|protected:|private:)')
  return re.sub(pattern, r' \1', text)


def FixIncludeGuards(text, file_name):
  """Change include guard according to the stantard."""
  # Remove a possible webrtc/ from  the path.
  file_name = re.sub(r'(webrtc\/)(.+)', r'\2', file_name)
  new_guard = 'WEBRTC_' + file_name
  new_guard = new_guard.upper()
  new_guard = re.sub(r'([/\.])', r'_', new_guard)
  new_guard += '_'

  text = re.sub(r'#ifndef WEBRTC_.+\n', r'#ifndef ' + new_guard + '\n', text, 1)
  text = re.sub(r'#define WEBRTC_.+\n', r'#define ' + new_guard + '\n', text, 1)
  text = re.sub(r'#endif *\/\/ *WEBRTC_.+\n', r'#endif  // ' + new_guard + '\n',
                text, 1)

  return text


def SaveFile(filename, text):
  os.remove(filename)
  f = open(filename, 'w')
  f.write(text)
  f.close()


def main():
  args = sys.argv[1:]
  if not args:
    print 'Usage: %s <filename>' % sys.argv[0]
    sys.exit(1)

  for filename in args:
    f = open(filename)
    text = f.read()
    f.close()

    text = DeCamelCase(text)
    text = MoveUnderScore(text)
    text = CPPComments(text)
    text = AddHeaderPath(text)
    text = AddWebrtcPrefixToOldSrcRelativePaths(text)
    text = SortIncludeHeaders(text, filename)
    text = RemoveMultipleEmptyLines(text)
    text = TrimLineEndings(text)

    # Remove the original file and re-create it with the reformatted content.
    SaveFile(filename, text)

    # Fix tabs, indentation and '{' using astyle.
    astyle_cmd = 'astyle'
    if sys.platform == 'win32':
      astyle_cmd += '.exe'
    subprocess.call([astyle_cmd, '-n', '-q', filename])

    if filename.endswith('.h'):
      f = open(filename)
      text = f.read()
      f.close()
      text = IndentLabels(text)
      text = FixIncludeGuards(text, filename)
      SaveFile(filename, text)

    print filename + ' done.'


if __name__ == '__main__':
  main()
