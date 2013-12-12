#
# libjingle
# Copyright 2013, Google Inc.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
#  1. Redistributions of source code must retain the above copyright notice,
#     this list of conditions and the following disclaimer.
#  2. Redistributions in binary form must reproduce the above copyright notice,
#     this list of conditions and the following disclaimer in the documentation
#     and/or other materials provided with the distribution.
#  3. The name of the author may not be used to endorse or promote products
#     derived from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
# EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

# Copied from Chromium's src/build/isolate.gypi
#
# It was necessary to copy this file to libjingle , because the path to
# build/common.gypi is different for the standalone and Chromium builds. Gyp
# doesn't permit conditional inclusion or variable expansion in include paths.
# http://code.google.com/p/gyp/wiki/InputFormatReference#Including_Other_Files

# This file is meant to be included into a target to provide a rule
# to "build" .isolate files into a .isolated file.
#
# To use this, create a gyp target with the following form:
# 'conditions': [
#   ['test_isolation_mode != "noop"', {
#     'targets': [
#       {
#         'target_name': 'foo_test_run',
#         'type': 'none',
#         'dependencies': [
#           'foo_test',
#         ],
#         'includes': [
#           '../build/isolate.gypi',
#           'foo_test.isolate',
#         ],
#         'sources': [
#           'foo_test.isolate',
#         ],
#       },
#     ],
#   }],
# ],
#
# Note: foo_test.isolate is included and a source file. It is an inherent
# property of the .isolate format. This permits to define GYP variables but is
# a stricter format than GYP so isolate.py can read it.
#
# The generated .isolated file will be:
#   <(PRODUCT_DIR)/foo_test.isolated

{
  'rules': [
    {
      'rule_name': 'isolate',
      'extension': 'isolate',
      'inputs': [
        # Files that are known to be involved in this step.
        '<(DEPTH)/tools/swarming_client/isolate.py',
        '<(DEPTH)/tools/swarming_client/run_isolated.py',
        '<(DEPTH)/tools/swarming_client/googletest/run_test_cases.py',

        # Disable file tracking by the build driver for now. This means the
        # project must have the proper build-time dependency for their runtime
        # dependency. This improves the runtime of the build driver since it
        # doesn't have to stat() all these files.
        #
        # More importantly, it means that even if a isolate_dependency_tracked
        # file is missing, for example if a file was deleted and the .isolate
        # file was not updated, that won't break the build, especially in the
        # case where foo_tests_run is not built! This should be reenabled once
        # the switch-over to running tests on Swarm is completed.
        #'<@(isolate_dependency_tracked)',
      ],
      'outputs': [
        '<(PRODUCT_DIR)/<(RULE_INPUT_ROOT).isolated',
      ],
      'conditions': [
        ["test_isolation_outdir==''", {
          'action': [
            'python',
            '<(DEPTH)/tools/swarming_client/isolate.py',
            '<(test_isolation_mode)',
            # GYP will eliminate duplicate arguments so '<(PRODUCT_DIR)' cannot
            # be provided twice. To work around this behavior, append '/'.
            #
            # Also have a space after <(PRODUCT_DIR) or visual studio will
            # escape the argument wrappping " with the \ and merge it into
            # the following arguments.
            #
            # Other variables should use the -V FOO=<(FOO) form so frequent
            # values, like '0' or '1', aren't stripped out by GYP.
            '--outdir', '<(PRODUCT_DIR)/ ',
            '--variable', 'PRODUCT_DIR', '<(PRODUCT_DIR) ',
            '--variable', 'OS=<(OS)',
            '--result', '<@(_outputs)',
            '--isolate', '<(RULE_INPUT_PATH)',
          ],
        }, {
          'action': [
            'python',
            '<(DEPTH)/tools/swarming_client/isolate.py',
            '<(test_isolation_mode)',
            '--outdir', '<(test_isolation_outdir)',
            # See comment above.
            '--variable', 'PRODUCT_DIR', '<(PRODUCT_DIR) ',
            '--variable', 'OS=<(OS)',
            '--result', '<@(_outputs)',
            '--isolate', '<(RULE_INPUT_PATH)',
          ],
        }],
        ['test_isolation_fail_on_missing == 0', {
            'action': ['--ignore_broken_items'],
          },
        ],
      ],

      'msvs_cygwin_shell': 0,
    },
  ],
}
