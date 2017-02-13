#!/usr/bin/env python

# Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
# Use of this source code is governed by a BSD-style license
# that can be found in the LICENSE file in the root of the source
# tree. An additional intellectual property rights grant can be found
# in the file PATENTS.  All contributing project authors may
# be found in the AUTHORS file in the root of the source tree.

"""WebRTC iOS FAT libraries build script.
Each architecture is compiled separately before being merged together.
By default, the library is created in out_ios_libs/. (Change with -o.)
The headers will be copied to out_ios_libs/include.
"""

import argparse
import distutils.dir_util
import logging
import os
import shutil
import subprocess
import sys


os.environ['PATH'] = '/usr/libexec' + os.pathsep + os.environ['PATH']

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
WEBRTC_BASE_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, '..', '..'))
SDK_OUTPUT_DIR = os.path.join(WEBRTC_BASE_DIR, 'out_ios_libs')
SDK_LIB_NAME = 'librtc_sdk_objc.a'
SDK_FRAMEWORK_NAME = 'WebRTC.framework'

ENABLED_ARCHITECTURES = ['arm', 'arm64', 'x64']
IOS_DEPLOYMENT_TARGET = '8.0'
LIBVPX_BUILD_VP9 = False


def _ParseArgs():
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument('-b', '--build_type', default='framework',
      choices=['framework', 'static_only'],
      help='The build type. Can be "framework" or "static_only". '
           'Defaults to "framework".')
  parser.add_argument('--build_config', default='release',
      choices=['debug', 'release'],
      help='The build config. Can be "debug" or "release". '
           'Defaults to "release".')
  parser.add_argument('-c', '--clean', action='store_true', default=False,
      help='Removes the previously generated build output, if any.')
  parser.add_argument('-o', '--output-dir', default=SDK_OUTPUT_DIR,
      help='Specifies a directory to output the build artifacts to. '
           'If specified together with -c, deletes the dir.')
  parser.add_argument('-r', '--revision', type=int, default=0,
      help='Specifies a revision number to embed if building the framework.')
  parser.add_argument('-e', '--bitcode', action='store_true', default=False,
      help='Compile with bitcode.')
  parser.add_argument('--verbose', action='store_true', default=False,
      help='Debug logging.')
  parser.add_argument('--use-goma', action='store_true', default=False,
      help='Use goma to build.')
  parser.add_argument('--extra-gn-args', default=[], nargs='*',
      help='Additional GN args to be used during Ninja generation.')

  return parser.parse_args()


def _RunCommand(cmd):
  logging.debug('Running: %r', cmd)
  subprocess.check_call(cmd)


def _CleanArtifacts(output_dir):
  if os.path.isdir(output_dir):
    logging.info('Deleting %s', output_dir)
    shutil.rmtree(output_dir)


def BuildWebRTC(output_dir, target_arch, flavor, build_type,
                ios_deployment_target, libvpx_build_vp9, use_bitcode,
                use_goma, extra_gn_args):
  output_dir = os.path.join(output_dir, target_arch + '_libs')
  gn_args = ['target_os="ios"', 'ios_enable_code_signing=false',
             'use_xcode_clang=true', 'is_component_build=false']

  # Add flavor option.
  if flavor == 'debug':
    gn_args.append('is_debug=true')
  elif flavor == 'release':
    gn_args.append('is_debug=false')
  else:
    raise ValueError('Unexpected flavor type: %s' % flavor)

  gn_args.append('target_cpu="%s"' % target_arch)

  gn_args.append('ios_deployment_target="%s"' % ios_deployment_target)

  gn_args.append('rtc_libvpx_build_vp9=' +
                 ('true' if libvpx_build_vp9 else 'false'))

  gn_args.append('enable_ios_bitcode=' +
                 ('true' if use_bitcode else 'false'))
  gn_args.append('use_goma=' + ('true' if use_goma else 'false'))

  # Generate static or dynamic.
  if build_type == 'static_only':
    gn_target_name = 'rtc_sdk_objc'
  elif build_type == 'framework':
    gn_target_name = 'rtc_sdk_framework_objc'
    gn_args.append('enable_dsyms=true')
    gn_args.append('enable_stripping=true')
  else:
    raise ValueError('Build type "%s" is not supported.' % build_type)

  logging.info('Building WebRTC with args: %s', ' '.join(gn_args))
  cmd = ['gn', 'gen', output_dir,
         '--args=' + ' '.join(gn_args + extra_gn_args)]
  _RunCommand(cmd)
  logging.info('Building target: %s', gn_target_name)
  cmd = ['ninja', '-C', output_dir, gn_target_name]
  _RunCommand(cmd)

  # Strip debug symbols to reduce size.
  if build_type == 'static_only':
    gn_target_path = os.path.join(output_dir, 'obj', 'webrtc', 'sdk',
                                  'lib%s.a' % gn_target_name)
    cmd = ['strip', '-S', gn_target_path, '-o',
           os.path.join(output_dir, 'lib%s.a' % gn_target_name)]
    _RunCommand(cmd)


def main():
  args = _ParseArgs()

  logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO)

  if args.clean:
    _CleanArtifacts(args.output_dir)
    return 0

  # Build all architectures.
  for arch in ENABLED_ARCHITECTURES:
    BuildWebRTC(args.output_dir, arch, args.build_config, args.build_type,
                IOS_DEPLOYMENT_TARGET, LIBVPX_BUILD_VP9, args.bitcode,
                args.use_goma, args.extra_gn_args)

  # Ignoring x86 except for static libraries for now because of a GN build issue
  # where the generated dynamic framework has the wrong architectures.

  # Create FAT archive.
  if args.build_type == 'static_only':
    BuildWebRTC(args.output_dir, 'x86', args.build_config, args.build_type,
                IOS_DEPLOYMENT_TARGET, LIBVPX_BUILD_VP9, args.bitcode,
                args.use_goma, args.extra_gn_args)

    arm_lib_path = os.path.join(args.output_dir, 'arm_libs', SDK_LIB_NAME)
    arm64_lib_path = os.path.join(args.output_dir, 'arm64_libs', SDK_LIB_NAME)
    x64_lib_path = os.path.join(args.output_dir, 'x64_libs', SDK_LIB_NAME)
    x86_lib_path = os.path.join(args.output_dir, 'x86_libs', SDK_LIB_NAME)

    # Combine the slices.
    cmd = ['lipo', arm_lib_path, arm64_lib_path, x64_lib_path, x86_lib_path,
           '-create', '-output', os.path.join(args.output_dir, SDK_LIB_NAME)]
    _RunCommand(cmd)

  elif args.build_type == 'framework':
    arm_lib_path = os.path.join(args.output_dir, 'arm_libs')
    arm64_lib_path = os.path.join(args.output_dir, 'arm64_libs')
    x64_lib_path = os.path.join(args.output_dir, 'x64_libs')

    # Combine the slices.
    dylib_path = os.path.join(SDK_FRAMEWORK_NAME, 'WebRTC')
    # Use distutils instead of shutil to support merging folders.
    distutils.dir_util.copy_tree(
        os.path.join(arm64_lib_path, SDK_FRAMEWORK_NAME),
        os.path.join(args.output_dir, SDK_FRAMEWORK_NAME))
    try:
      os.remove(os.path.join(args.output_dir, dylib_path))
    except OSError:
      pass
    logging.info('Merging framework slices.')
    cmd = ['lipo', os.path.join(arm_lib_path, dylib_path),
           os.path.join(arm64_lib_path, dylib_path),
           os.path.join(x64_lib_path, dylib_path),
           '-create', '-output', os.path.join(args.output_dir, dylib_path)]
    _RunCommand(cmd)

    # Merge the dSYM slices.
    dsym_path = os.path.join('WebRTC.dSYM', 'Contents', 'Resources', 'DWARF',
                             'WebRTC')
    distutils.dir_util.copy_tree(os.path.join(arm64_lib_path, 'WebRTC.dSYM'),
                                 os.path.join(args.output_dir, 'WebRTC.dSYM'))
    try:
      os.remove(os.path.join(args.output_dir, dsym_path))
    except OSError:
      pass
    logging.info('Merging dSYM slices.')
    cmd = ['lipo', os.path.join(arm_lib_path, dsym_path),
           os.path.join(arm64_lib_path, dsym_path),
           os.path.join(x64_lib_path, dsym_path),
           '-create', '-output', os.path.join(args.output_dir, dsym_path)]
    _RunCommand(cmd)

    # Modify the version number.
    # Format should be <Branch cut MXX>.<Hotfix #>.<Rev #>.
    # e.g. 55.0.14986 means branch cut 55, no hotfixes, and revision 14986.
    infoplist_path = os.path.join(args.output_dir, SDK_FRAMEWORK_NAME,
                                  'Info.plist')
    cmd = ['PlistBuddy', '-c',
           'Print :CFBundleShortVersionString', infoplist_path]
    major_minor = subprocess.check_output(cmd).strip()
    version_number = '%s.%s' % (major_minor, args.revision)
    logging.info('Substituting revision number: %s', version_number)
    cmd = ['PlistBuddy', '-c',
           'Set :CFBundleVersion ' + version_number, infoplist_path]
    _RunCommand(cmd)
    _RunCommand(['plutil', '-convert', 'binary1', infoplist_path])

  logging.info('Done.')
  return 0


if __name__ == '__main__':
  sys.exit(main())
