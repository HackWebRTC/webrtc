# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../common_settings.gypi', # Common settings
  ],
  'targets': [
    {
      'target_name': 'bwe_standalone',
      'type': 'executable',
      'dependencies': [
        'matlab_plotting',
        '../source/rtp_rtcp.gyp:rtp_rtcp',
        '../../udp_transport/source/udp_transport.gyp:udp_transport',
        '../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
        '../interface',
        '../../interface',
      ],
      'sources': [
        'BWEStandAlone/BWEStandAlone.cc',
        'BWEStandAlone/TestLoadGenerator.cc',
        'BWEStandAlone/TestLoadGenerator.h',
        'BWEStandAlone/TestSenderReceiver.cc',
        'BWEStandAlone/TestSenderReceiver.h',
      ], # source
      'conditions': [
          ['OS=="linux"', {
              'cflags': [
                  '-fexceptions', # enable exceptions
                  ],
              },
           ],
          ],

      'include_dirs': [
          ],
      'link_settings': {
          },
    },

    {
      'target_name': 'matlab_plotting',
      'type': '<(library)',
      'dependencies': [
        'matlab_plotting_include',
        '../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
          '/opt/matlab2010a/extern/include',
          ],
      # 'direct_dependent_settings': {
      #     'defines': [
      #         'MATLAB',
      #         ],
      #     'include_dirs': [
      #         'BWEStandAlone',
      #         ],
      #     },
      'export_dependent_settings': [
          'matlab_plotting_include',
          ],
      'sources': [
          'BWEStandAlone/MatlabPlot.cc',
          'BWEStandAlone/MatlabPlot.h',
          ],
      'link_settings': {
          'ldflags' : [
              '-L/opt/matlab2010a/bin/glnxa64',
              '-leng',
              '-lmx',
              '-Wl,-rpath,/opt/matlab2010a/bin/glnxa64',
              ],
          },
      'defines': [
          'MATLAB',
          ],
      'conditions': [
          ['OS=="linux"', {
              'cflags': [
                  '-fexceptions', # enable exceptions
                  ],
              },
           ],
          ],
      },

    {
      'target_name': 'matlab_plotting_include',
      'type': 'none',
      'direct_dependent_settings': {
          'defines': [
#              'MATLAB',
              ],
          'include_dirs': [
              'BWEStandAlone',
              ],
          },
      },

    {
      'target_name': 'matlab_plotting_test',
      'type': 'executable',
      'dependencies': [
        'matlab_plotting',
        '../../../system_wrappers/source/system_wrappers.gyp:system_wrappers',
      ],
      'include_dirs': [
      ],
      'sources': [
        'BWEStandAlone/matlab_plotting_test.cc',
      ], # source
      'conditions': [
          ['OS=="linux"', {
              'cflags': [
                  '-fexceptions', # enable exceptions
                  ],
              },
           ],
          ],

      'include_dirs': [
          ],
      'link_settings': {
          },
    },

  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
