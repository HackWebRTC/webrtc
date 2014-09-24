#
# libjingle
# Copyright 2012, Google Inc.
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

# This file contains common settings for building libjingle components.

{
  'variables': {
    'webrtc_root%': '<(DEPTH)/webrtc',
    # TODO(ronghuawu): Chromium build will need a different libjingle_root.
    'libjingle_tests_additional_deps%': [],
    'libjingle_root%': '<(DEPTH)',
    # TODO(ronghuawu): For now, disable the Chrome plugins, which causes a
    # flood of chromium-style warnings.
    'clang_use_chrome_plugins%': 0,
    'libpeer_target_type%': 'static_library',
    'conditions': [
      ['OS=="android" or OS=="linux"', {
        # TODO(henrike): make sure waterfall bots have $JAVA_HOME configured
        # properly and remove the default value below. See issue 2113.
        'java_home%': '<!(python -c "import os; dir=os.getenv(\'JAVA_HOME\', \'/usr/lib/jvm/java-6-sun\'); assert os.path.exists(os.path.join(dir, \'include/jni.h\')), \'Point \\$JAVA_HOME or the java_home gyp variable to a directory containing include/jni.h!\'; print dir")',
      }],
    ],
  },
  'target_defaults': {
    'include_dirs': [
      '<(libjingle_root)',
      '<(DEPTH)',
      '../../third_party',
      '../../third_party/webrtc',
      '../../webrtc',
    ],
    'defines': [
      'EXPAT_RELATIVE_PATH',
      'FEATURE_ENABLE_VOICEMAIL',
      'GTEST_RELATIVE_PATH',
      'JSONCPP_RELATIVE_PATH',
      'LOGGING=1',
      'SRTP_RELATIVE_PATH',

      # Feature selection
      'FEATURE_ENABLE_SSL',
      'FEATURE_ENABLE_VOICEMAIL',
      'FEATURE_ENABLE_PSTN',
      'HAVE_SCTP',
      'HAVE_SRTP',
      'HAVE_WEBRTC_VIDEO',
      'HAVE_WEBRTC_VOICE',
      'USE_WEBRTC_DEV_BRANCH',
    ],
    'conditions': [
      # TODO(ronghuawu): Support dynamic library build.
      ['"<(libpeer_target_type)"=="static_library"', {
        'defines': [ 'LIBPEERCONNECTION_LIB=1' ],
      }],
      ['OS=="linux"', {
        'defines': [
          'LINUX',
          'WEBRTC_LINUX',
        ],
        'conditions': [
          ['clang==1', {
            'cflags': [
              '-Wall',
              '-Wextra',
              '-Wunused-variable',
              # TODO(ronghuawu): Fix the warning caused by
              # LateBindingSymbolTable::TableInfo from
              # latebindingsymboltable.cc.def and remove below flag.
              '-Wno-address-of-array-temporary',
              '-Wthread-safety',
            ],
          }],
        ],
      }],
      ['OS=="mac"', {
        'defines': [
          'OSX',
          'WEBRTC_MAC',
        ],
      }],
      ['OS=="win"', {
        'defines': [
          'WEBRTC_WIN',
        ],
        'msvs_disabled_warnings': [
          # https://code.google.com/p/chromium/issues/detail?id=372451#c20
          # Warning 4702 ("Unreachable code") should be re-enabled once
          # users are updated to VS2013 Update 2.
            4702,
        ],
      }],
      ['OS=="ios"', {
        'defines': [
          'IOS',
          'WEBRTC_MAC',
          'WEBRTC_IOS',
        ],
      }],
      ['OS=="ios" or (OS=="mac" and target_arch!="ia32")', {
        'defines': [
          'CARBON_DEPRECATED=YES',
        ],
      }],
      ['os_posix==1', {
        'configurations': {
          'Debug_Base': {
            'defines': [
              # Chromium's build/common.gypi defines this for all posix _except_
              # for ios & mac.  We want it there as well, e.g. because ASSERT
              # and friends trigger off of it.
              '_DEBUG',
            ],
          },
        },
        'defines': [
          'HASH_NAMESPACE=__gnu_cxx',
          'POSIX',
          'WEBRTC_POSIX',
          'DISABLE_DYNAMIC_CAST',
          # The POSIX standard says we have to define this.
          '_REENTRANT',
        ],
      }],
    ],
  }, # target_defaults
}
