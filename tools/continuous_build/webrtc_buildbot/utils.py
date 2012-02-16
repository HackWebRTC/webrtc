#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = 'ivinnichenko@webrtc.org (Illya Vinnichenko)'

from buildbot.process import factory
from buildbot.steps import shell
from buildbot.steps.shell import ShellCommand
from buildbot.process import properties
from buildbot.process.properties import WithProperties
import os
import sys
import urlparse

# Defines the order of the booleans of the supported platforms in the test
# dictionaries in master.cfg.
SUPPORTED_PLATFORMS = ('Linux', 'Mac', 'Windows')

SVN_LOCATION = 'http://webrtc.googlecode.com/svn/trunk'
VALGRIND_CMD = ['tools/valgrind-webrtc/webrtc_tests.sh', '-t', 'cmdline']

DEFAULT_COVERAGE_DIR = '/var/www/coverage/'

# Copied from trunk/tools/build/scripts/master/factory/chromium_factory.py
# but converted to a list since we set defines instead of using an environment
# variable.
#
# On valgrind bots, override the optimizer settings so we don't inline too
# much and make the stacks harder to figure out. Use the same settings
# on all buildbot masters to make it easier to move bots.
MEMORY_TOOLS_GYP_DEFINES = [
  # GCC flags
  'mac_debug_optimization=1 ',
  'mac_release_optimization=1 ',
  'release_optimize=1 ',
  'no_gc_sections=1 ',
  'debug_extra_cflags="-g -fno-inline -fno-omit-frame-pointer '
  '-fno-builtin -fno-optimize-sibling-calls" ',
  'release_extra_cflags="-g -fno-inline -fno-omit-frame-pointer '
  '-fno-builtin -fno-optimize-sibling-calls" ',
  # MSVS flags
  'win_debug_RuntimeChecks=0 ',
  'win_debug_disable_iterator_debugging=1 ',
  'win_debug_Optimization=1 ',
  'win_debug_InlineFunctionExpansion=0 ',
  'win_release_InlineFunctionExpansion=0 ',
  'win_release_OmitFramePointers=0 ',

  'linux_use_tcmalloc=1 ',
  'release_valgrind_build=1 ',
  'werror= ',
]

class WebRTCFactory(factory.BuildFactory):
  """Abstract superclass for all build factories.

     A build factory defines a sequence of steps to take in a build process.
     This class provides some helper methods and some abstract methods that
     can be overridden to create customized build sequences.
  """

  def __init__(self):
    factory.BuildFactory.__init__(self)

    self.properties = properties.Properties()
    self.build_enabled = False
    self.force_sync = False
    self.gyp_params = []
    self.release = False

  def EnableBuild(self, force_sync):
    """Adds steps for building WebRTC [must be overridden].

       Implementations of this method must add clean and build steps so that
       when all steps have been run, we have an up-to-date, complete and correct
       build of WebRTC for the platform. It is up to the method how to do this.

       Args:
         force_sync: the method must pass --force to 'gclient sync' if it is
           used.
    """
    pass

  def EnableTests(self, tests):
    """Adds test run steps for all tests in the list.

       This method must be run after enabling the build.

       Args:
         tests: list of test to be run.
    """
    for test in tests:
      self.EnableTest(test)

  def AddCommonStep(self, cmd, descriptor='', workdir='build',
                    warn_on_failure=False,):
    """Adds a common step which will run as a shell command on the slave.

       A common step can be anything except a test execution step.

       Args:
         cmd: The command to run. This command follows the contract for
           ShellCommand, so see that documentation for more details.
         descriptor: A string, or a list of strings, describing what the step
           does. The descriptor gets printed in the waterfall display.
         workdir: The working directory to run the command in.
         warnOnFailure: Enable if you want a warning on failure instead of
           an error. Enable for less critical commands.
    """
    if type(descriptor) is str:
      descriptor = [descriptor]

    flunk_on_failure = not warn_on_failure
    self.addStep(shell.ShellCommand(command=cmd, workdir=workdir,
                                    description=descriptor + ['running...'],
                                    descriptionDone=descriptor + ['done...'],
                                    warnOnFailure=warn_on_failure,
                                    flunkOnFailure=flunk_on_failure,
                                    name=''.join(descriptor)))

  def AddCommonTestRunStep(self, test, descriptor='', cmd=None,
                           workdir='build/trunk'):
    """Adds a step for running a single test [must be overridden].

       Args:
         test: The test binary name. The step will attempt to execute this
           binary in the binary output folder, except if the cmd argument is
           defined (in that case, we will run cmd instead and just use the
           test name in the descriptor).
         descriptor: This should either be a string or a list of strings. The
           descriptor or descriptors are appended to the test name and
           displayed in the waterfall.
         cmd: If necessary, you can specify this argument to override the
           default behavior, which is to just run the binary specified in
           test without arguments.
         workdir: The base working directory to run the command in. This
           directory will map to the WebRTC project root, e.g. the trunk
           directory. This method will make sure that the test binary is run
           in the correct output directory for the platform.
    """
    pass

  def EnableTest(self, test):
    """Makes a test run in the build sequence. May be overridden.

       Override to handle special cases for specific platforms, for instance if
       a particular test binary requires command line arguments.

       Args:
           test: The test name to enable.
    """
    self.AddCommonTestRunStep(test)

  def AddCommonGYPStep(self, gyp_file, gyp_params=[], descriptor='gyp'):
    """Helper method for invoking GYP on WebRTC.

       GYP will generate makefiles or its equivalent in a platform-specific
       manner.

       Args:
         gyp_file: The root GYP file to use.
         gyp_params: Custom GYP parameters (same semantics as the GYP_PARAMS
           environment variable).
         descriptor: The descriptor to use for the step.
    """
    cmd = ['./build/gyp_chromium', '--depth=.', gyp_file]
    cmd += gyp_params + self.gyp_params
    self.addStep(shell.ShellCommand(command=cmd, workdir='build/trunk',
                                    description=[descriptor, 'running...'],
                                    descriptionDone=[descriptor, 'done...'],
                                    name='gyp_%s' % descriptor))

class GenerateCodeCoverage(ShellCommand):
  """This custom shell command generates coverage HTML using genhtml.

     The command will dump the HTML output into coverage_dir, in a directory
     whose name is generated from the build number and slave name. We will
     expect that the coverage directory is somewhere under the web server root
     (i.e. public html root) that corresponds to the web server URL. That is, if
     we write Foo to the coverage directory we expect that directory to be
     reachable from url/Foo.
  """

  def __init__(self, coverage_url, coverage_dir, coverage_file, **kwargs):
    """Prepares the coverage command.

       Args:
         coverage_url: The base URL for the serving web server we will use
           when we generate the link to the coverage. This will generally
           be the slave's URL (something like http://slave-hostname/).
         coverage_dir: Where to write coverage HTML.
         coverage_file: The LCOV file to generate the coverage from.
    """

    ShellCommand.__init__(self, **kwargs)
    self.addFactoryArguments(coverage_url=coverage_url,
                             coverage_dir=coverage_dir,
                             coverage_file=coverage_file)
    self.setDefaultWorkdir("build/trunk")
    self.coverage_url = coverage_url
    self.coverage_dir = coverage_dir
    self.coverage_file = coverage_file
    self.description = ["Coverage Report"]
    output_dir = os.path.join(coverage_dir,
                              '%(buildername)s_%(buildnumber)s')
    self.setCommand(['./tools/continuous_build/generate_coverage_html.sh',
                     coverage_file, WithProperties(output_dir)])

  def createSummary(self, log):
    coverage_url = urlparse.urljoin(self.coverage_url,
                                    '%s_%s' % (self.getProperty('buildername'),
                                               self.getProperty('buildnumber')))
    self.addURL('click here', coverage_url)

  def start(self):
    ShellCommand.start(self)


class WebRTCAndroidFactory(WebRTCFactory):
  """Sets up the Android build."""

  def __init__(self):
    WebRTCFactory.__init__(self)

  def EnableBuild(self, product='toro'):
    prefix = 'rm -rf out/target/product/%s/obj/' % product
    cleanup_list = [
                    'rm -rf external/webrtc',
                    prefix + 'STATIC_LIBRARIES/libwebrtc_*',
                    prefix + 'SHARE_LIBRARIES/libwebrtc_*',
                    prefix + 'EXECUTABLES/webrtc_*'
                    ]
    cmd = ' ; '.join(cleanup_list)
    self.addStep(shell.Compile(command=(cmd), workdir='build/trunk',
                 description=['cleanup', 'running...'], haltOnFailure=False,
                 warnOnFailure=True, flunkOnFailure=False,
                 descriptionDone=['cleanup', 'done...'], name='cleanup'))
    cmd = 'svn checkout %s external/webrtc' % SVN_LOCATION
    self.addStep(shell.Compile(command=(cmd),
        workdir='build/trunk', description=['svn', 'running...'],
        haltOnFailure=False, descriptionDone=['svn', 'done...'], name='svn'))
    cmd = ('source build/envsetup.sh && lunch full_%s-eng '
           '&& mmm external/webrtc showcommands' % product)
    self.addStep(shell.Compile(command=(cmd),
        workdir='build/trunk', description=['build', 'running...'],
        haltOnFailure=False,
        descriptionDone=['build', 'done...'], name='build'))


class WebRTCChromeFactory(WebRTCFactory):
  """Sets up the Chrome OS build."""

  def __init__(self):
    WebRTCFactory.__init__(self)

  def EnableBuild(self):
    self.AddCommonStep(['rm', '-rf', 'src'], descriptor='Cleanup')
    cmd = ['gclient', 'sync', '--force']
    self.AddCommonStep(cmd, descriptor='Sync')
    self.AddCommonMakeStep('chrome')

  def AddCommonMakeStep(self, make, descriptor='', make_extra=None):
    make_descriptor = [make, descriptor]
    cmd = ['make', make, '-j100']
    if make_extra is not None:
      cmd.append(make_extra)
    self.addStep(shell.ShellCommand(command=cmd,
        workdir='build/src', description=['Making'] + make_descriptor,
        descriptionDone=make_descriptor + ['built'],
        name='_'.join(make_descriptor)))


class WebRTCLinuxFactory(WebRTCFactory):
  """Sets up the Linux build.

     This factory is quite configurable and can run a variety of builds.
  """

  def __init__(self, valgrind_enabled=False):
    WebRTCFactory.__init__(self)

    self.coverage_enabled = False
    self.valgrind_enabled = valgrind_enabled

  def EnableCoverage(self, coverage_url, coverage_dir=DEFAULT_COVERAGE_DIR):
    """Enables coverage measurements using LCOV/GCOV.

       This method must be called before enabling build.

       Args:
         coverage_url: See the GenerateCodeCoverage command's contract for
           this argument.
         coverage_dir: See the GenerateCodeCoverage command's contract for
           this argument.
    """

    assert self.build_enabled is False

    self.coverage_enabled = True
    self.coverage_url = coverage_url
    self.coverage_dir = coverage_dir

  def EnableBuild(self, force_sync=False, release=False, build32=False,
                  chrome_os=False, clang=False):
    if build32:
      self.gyp_params.append('-Dtarget_arch=ia32')

    self.build_enabled = True
    self.force_sync = force_sync
    self.release = release

    self.AddCommonStep(['rm', '-rf', 'trunk'], descriptor='Cleanup')

    # Valgrind bots need special GYP defines to enable memory profiling
    # friendly compilation. They already has a custom .gclient configuration
    # file created so they don't need one being generated like the other bots.
    if self.valgrind_enabled:
      for gyp_define in MEMORY_TOOLS_GYP_DEFINES:
        self.gyp_params.append('-D' + gyp_define)
    else:
      self.AddCommonStep(['gclient', 'config', SVN_LOCATION],
                         descriptor='gclient_config')

    cmd = ['gclient', 'sync']
    if force_sync:
      cmd.append('--force')
    self.AddCommonStep(cmd, descriptor='Sync')
    if chrome_os:
      self.gyp_params.append('-Dchromeos=1')

    if clang:
      self.gyp_params.append('-Dclang=1')

    if self.coverage_enabled:
      self.gyp_params.append('-Dcoverage=1')
    self.AddCommonGYPStep('webrtc.gyp', descriptor='CommonGYP')

    if clang:
      self.AddCommonStep(['trunk/tools/clang/scripts/update.sh'],
                          descriptor='Update_Clang')

    if self.release:
      self.AddCommonMakeStep('all', make_extra='BUILDTYPE=Release')
    else:
      self.AddCommonMakeStep('all')

  def AddCommonTestRunStep(self, test, descriptor='', cmd=None,
                           workdir='build/trunk'):
    test_folder = 'Release' if self.release else 'Debug'
    test_descriptor = [test, descriptor]
    if cmd is None:
      cmd = ['out/%s/%s' % (test_folder, test)]
    if self.valgrind_enabled:
      cmd = VALGRIND_CMD + cmd
    self.addStep(shell.ShellCommand(command=cmd,
        workdir=workdir, description=['Running'] + test_descriptor,
        descriptionDone=test_descriptor + ['finished'],
        name='_'.join(test_descriptor)))

  def AddXvfbTestRunStep(self, test_name, test_binary, test_arguments=''):
    """ Adds a test to be run inside a XVFB window manager."""
    cmd = ('xvfb-run '
           '--server-args="-screen 0 800x600x24 -extension Composite" '
           '%s %s' % (test_binary, test_arguments))
    self.AddCommonTestRunStep(test=test_name, cmd=cmd)

  def AddCommonMakeStep(self, make, descriptor='', make_extra=None):
    make_descriptor = [make, descriptor]
    cmd = ['make', make, '-j100']
    if make_extra is not None:
      cmd.append(make_extra)
    self.addStep(shell.ShellCommand(command=cmd,
        workdir='build/trunk', description=['Making'] + make_descriptor,
        descriptionDone=make_descriptor + ['built'],
        name='_'.join(make_descriptor)))

  def AddStepsToEstablishCoverageBaseline(self):
    self.AddCommonStep(['lcov', '--directory', '.', '--capture', '-b',
                        '.', '--initial',
                        '--output-file', 'webrtc_base.info'],
                       workdir='build/trunk',
                       warn_on_failure=True,
                       descriptor=['LCOV', 'Baseline', 'Capture'])
    self.AddCommonStep(['lcov', '--extract', 'webrtc_base.info', '*/src/*',
                        '--output', 'filtered.info'],
                       workdir='build/trunk',
                       warn_on_failure=True,
                       descriptor=['LCOV', 'Baseline', 'Extract'])
    self.AddCommonStep(['lcov', '--remove', 'filtered.info', '*/usr/include/*',
                        '/third*', '/testing/*', '*/test/*', '*_unittest.*',
                        '*/mock/*', '--output',
                        'webrtc_base_filtered_final.info'],
                       workdir='build/trunk',
                       warn_on_failure=True,
                       descriptor=['LCOV', 'Baseline', 'Filter'])

  def AddStepsToComputeCoverage(self):
    """Enable coverage data."""

    # Delete all third-party .gcda files to save time and work around a bug
    # in lcov which tends to hang when capturing on libjpgturbo.
    self.AddCommonStep(['./tools/continuous_build/clean_third_party_gcda.sh'],
                       warn_on_failure=True,
                       workdir='build/trunk', descriptor=['LCOV',
                                                          'Delete 3rd party'])

    self.AddCommonStep(['lcov', '--directory', '.', '--capture', '-b',
                        '.', '--output-file', 'webrtc.info'],
                       warn_on_failure=True,
                       workdir='build/trunk', descriptor=['LCOV', 'Capture'])
    self.AddCommonStep(['lcov', '--extract', 'webrtc.info', '*/src/*',
                        '--output', 'test.info'], warn_on_failure=True,
                       workdir='build/trunk', descriptor=['LCOV', 'Extract'])
    self.AddCommonStep(['lcov', '--remove', 'test.info', '*/usr/include/*',
                        '/third*', '/testing/*', '*/test/*', '*_unittest.*',
                        '*/mock/*', '--output',
                        'final.info'], warn_on_failure=True,
                       workdir='build/trunk', descriptor=['LCOV', 'Filter'])
    self.AddCommonStep(['lcov', '-a', 'webrtc_base_filtered_final.info', '-a',
                        'final.info', '-o', 'final.info'], warn_on_failure=True,
                       workdir='build/trunk', descriptor=['LCOV', 'Merge'])
    self.addStep(GenerateCodeCoverage(coverage_url=self.coverage_url,
                                      coverage_dir=self.coverage_dir,
                                      coverage_file='final.info'))

  def EnableTests(self, tests):
    if self.coverage_enabled:
      self.AddStepsToEstablishCoverageBaseline()

    WebRTCFactory.EnableTests(self, tests)

    if self.coverage_enabled:
      self.AddStepsToComputeCoverage()

  def EnableTest(self, test):
    """Adds a step for running a test on Linux.

       In general, this method will interpret the name as the name of a binary
       in the default build output directory, except for a few special cases
       which require custom command lines.

       Args:
         test: the test name as a string.
    """
    if test == 'audioproc_unittest':
      self.AddCommonTestRunStep(test)
      self.AddCommonGYPStep('webrtc.gyp', gyp_params=['-Dprefer_fixed_point=1'],
                            descriptor='fixed_point')
      self.AddCommonMakeStep(test, descriptor='make_fixed_point')
      self.AddCommonTestRunStep(test, descriptor='fixed_point')
    elif test == 'vie_auto_test':
      # TODO(phoglund): Enable the full stack test once it is completed and
      # nonflaky.
      binary = "out/Debug/vie_auto_test"
      args = (
          '--automated --gtest_filter="'
          '-ViEVideoVerificationTest.RunsFullStackWithoutErrors:'
          'ViEExtendedIntegrationTest.*" '
          '--capture_test_ensure_resolution_alignment_in_capture_device=false')
      self.AddXvfbTestRunStep(test_name=test, test_binary=binary,
                              test_arguments=args)
    elif test == "video_render_module_test":
      self.AddXvfbTestRunStep(test_name=test,
                              test_binary='out/Debug/video_render_module_test')
    elif test == "voe_auto_test":
      # TODO(phoglund): Remove this notice and take appropriate action when
      # http://code.google.com/p/webrtc/issues/detail?id=266 is concluded.
      self.addStep(shell.Compile(
        command=('out/Debug/voe_auto_test --automated '
                 '--gtest_filter="-VolumeTest.*"'),
        workdir='build/trunk', description=[test, 'running...'],
        descriptionDone=[test, 'done...'], name='%s' % test))
    else:
      self.AddCommonTestRunStep(test)


class WebRTCMacFactory(WebRTCFactory):
  """Sets up the Mac build, both for make and xcode."""

  def __init__(self):
    WebRTCFactory.__init__(self)
    self.build_type = 'both'
    self.allowed_build_types = ['both', 'xcode', 'make']

  def EnableBuild(self, force_sync=True, build_type='both', release=False):
    self.build_enabled = True
    self.force_sync = force_sync
    self.release = release
    """Mac specific Build"""
    if build_type not in self.allowed_build_types:
      print '*** INCORRECT BUILD TYPE (%s)!!! ***' % build_type
      sys.exit(0)
    else:
      self.build_type = build_type
    self.AddCommonStep(['rm', '-rf', 'trunk'], descriptor='Cleanup')
    self.AddCommonStep(['gclient', 'config', SVN_LOCATION],
                       descriptor='gclient_config')
    cmd = ['gclient', 'sync']
    if force_sync:
      cmd.append('--force')
    self.AddCommonStep(cmd, descriptor='Sync')
    if self.build_type == 'make' or self.build_type == 'both':
      self.AddCommonGYPStep('webrtc.gyp', gyp_params=['-f', 'make'],
                            descriptor='EnableMake')
    self.AddCommonMakeStep('all')

  def AddCommonTestRunStep(self, test, descriptor='', cmd=None,
                           workdir='build/trunk'):
    test_folder = 'Release' if self.release else 'Debug'
    test_descriptor = [test, descriptor]
    if cmd is None:
      if self.build_type == 'xcode' or self.build_type == 'both':
        cmd = ['xcodebuild/%s/%s' % (test_folder, test)]
        self.AddCommonStep(cmd, descriptor=test_descriptor + ['(xcode)'],
                           workdir='build/trunk')
      if self.build_type == 'make' or self.build_type == 'both':
        cmd = ['out/%s/%s' % (test_folder, test)]
        self.AddCommonStep(cmd, descriptor=test_descriptor + ['(make)'],
                           workdir='build/trunk')

  def AddCommonMakeStep(self, make, descriptor='', make_extra=None):
    make_descriptor = [make, descriptor]
    if self.build_type == 'make' or self.build_type == 'both':
      cmd = ['make', make, '-j100']
      if make_extra is not None:
        cmd.append(make_extra)
      if self.release:
        cmd.append('BUILDTYPE=Release')
      self.AddCommonStep(cmd, descriptor=make_descriptor + ['(make)'],
                         workdir='build/trunk')
    if self.build_type == 'xcode' or self.build_type == 'both':
      configuration = 'Release' if self.release else 'Debug'
      cmd = ['xcodebuild', '-project', 'webrtc.xcodeproj', '-configuration',
             configuration, '-target', 'All']
      self.AddCommonStep(cmd, descriptor=make_descriptor + ['(xcode)'],
                         workdir='build/trunk')


class WebRTCWinFactory(WebRTCFactory):
  """Sets up the Windows build."""

  def __init__(self):
    WebRTCFactory.__init__(self)

    self.configuration = 'Debug'
    self.platform = 'x64'
    self.allowed_platforms = ['x64', 'Win32']
    self.allowed_configurations = ['Debug', 'Release', 'both']

  def EnableBuild(self, force_sync=True, platform='Win32',
                  configuration='Debug', build_only=False):
    self.build_enabled = True
    self.force_sync = force_sync
    """Win specific Build"""
    if platform not in self.allowed_platforms:
      print '*** INCORRECT PLATFORM (%s)!!! ***' % platform
      sys.exit(0)
    else:
      self.platform = platform
    if configuration not in self.allowed_configurations:
      print '*** INCORRECT CONFIGURATION (%s)!!! ***' % configuration
      sys.exit(0)
    else:
      self.configuration = configuration
    if not build_only:
      self.AddCommonStep(['rm', '-rf', 'trunk'], descriptor='Cleanup')
      self.AddCommonStep(['gclient', 'config', SVN_LOCATION],
                         descriptor='gclient_config')
      cmd = ['gclient', 'sync']
      if force_sync:
        cmd.append('--force')
      self.AddCommonStep(cmd, descriptor='Sync')

    if self.configuration == 'Debug' or self.configuration == 'both':
      cmd = ['msbuild', 'webrtc.sln', '/t:Clean',
             '/p:Configuration=Debug;Platform=%s' % (self.platform)]
      self.AddCommonStep(cmd, descriptor='Build_Clean', workdir='build/trunk')
      cmd = ['msbuild', 'webrtc.sln',
             '/p:Configuration=Debug;Platform=%s' % (self.platform)]
      self.AddCommonStep(cmd, descriptor='Build_Debug', workdir='build/trunk')
    if self.configuration == 'Release' or self.configuration == 'both':
      cmd = ['msbuild', 'webrtc.sln', '/t:Clean',
             '/p:Configuration=Release;Platform=%s' % (self.platform)]
      self.AddCommonStep(cmd, descriptor='Build_Clean', workdir='build/trunk')
      cmd = ['msbuild', 'webrtc.sln',
             '/p:Configuration=Release;Platform=%s' % (self.platform)]
      self.AddCommonStep(cmd, descriptor='Build_Release', workdir='build/trunk')

  def AddCommonTestRunStep(self, test, descriptor='', cmd=None,
                           workdir='build/trunk'):
    test_descriptor = [test, descriptor]
    if cmd is None:
      if self.configuration == 'Debug' or self.configuration == 'both':
        cmd = ['build\Debug\%s.exe' % test]
        self.AddCommonStep(cmd, descriptor=test_descriptor + ['Debug'],
                           workdir=workdir)
      if self.configuration == 'Release' or self.configuration == 'both':
        cmd = ['build\Release\%s.exe' % test]
        self.AddCommonStep(cmd, descriptor=test_descriptor + ['Release'],
                           workdir=workdir)

# Utility functions


class UnsupportedPlatformError(Exception):
  pass


def GetEnabledTests(test_dict, platform):
  """Returns a list of enabled test names for the provided platform.

     Args:
       test_dict: Dictionary mapping test names to tuples representing if the
         test shall be enabled on each platform. Each tuple contains one
         boolean for each platform. The platforms are in the order specified
         by SUPPORTED_PLATFORMS.
       platform: The platform we're looking to get the tests for.

     Returns:
       A list of test names, sorted alphabetically.

     Raises:
       UnsupportedPlatformError: if the platform supplied is not supported.
  """
  if platform not in SUPPORTED_PLATFORMS:
    raise UnsupportedPlatformError('*** UNSUPPORTED PLATFORM (%s)!!! ***' %
                                   platform)
  result = []
  platform_index = SUPPORTED_PLATFORMS.index(platform)
  for test_name, enabled_platforms in test_dict.iteritems():
    if enabled_platforms[platform_index]:
      result.append(test_name)
  result.sort()
  return result
