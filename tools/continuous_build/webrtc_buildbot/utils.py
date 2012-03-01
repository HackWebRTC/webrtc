#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = 'ivinnichenko@webrtc.org (Illya Vinnichenko)'

import buildbot
import os
import sys
import urlparse
from buildbot.process import factory
from buildbot.process import properties
from buildbot.process.properties import WithProperties
from buildbot.steps.shell import ShellCommand

# Defines the order of the booleans of the supported platforms in the test
# dictionaries in master.cfg.
SUPPORTED_PLATFORMS = ('Linux', 'Mac', 'Windows')

WEBRTC_SVN_LOCATION = 'http://webrtc.googlecode.com/svn/trunk'

VALGRIND_CMD = ['tools/valgrind-webrtc/webrtc_tests.sh', '-t', 'cmdline']

DEFAULT_COVERAGE_DIR = '/var/www/coverage/'
DEFAULT_MASTER_WORK_DIR = '.'
GCLIENT_RETRIES = 3

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

  def __init__(self, build_status_oracle):
    """Creates the abstract factory.

       Args:
         build_status_oracle: An instance of BuildStatusOracle which is used to
             keep track of our build state.
    """
    factory.BuildFactory.__init__(self)

    self.build_status_oracle = build_status_oracle
    self.properties = properties.Properties()
    self.gyp_params = []
    self.release = False

  def EnableBuild(self):
    """Adds steps for building WebRTC [must be overridden].

       Implementations of this method must add clean and build steps so that
       when all steps have been run, we have an up-to-date, complete and correct
       build of WebRTC for the platform. It is up to the method how to do this.
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
                    number_of_retries=0, halt_build_on_failure=True,
                    warn_on_failure=False):
    """Adds a step which will run as a shell command on the slave.

       NOTE: you are recommended to use this method to add new shell commands
       instead of the base-class addStep method, since steps added here will
       work with the smart-clean system (e.g. only do a full rebuild if the
       previous build failed). Steps handled outside this method will not lead
       to a full rebuild on the next build if they fail.

       Args:
         cmd: The command to run. This command follows the contract for
             ShellCommand, so see that documentation for more details.
         descriptor: A string, or a list of strings, describing what the step
             does. The descriptor gets printed in the waterfall display.
         workdir: The working directory to run the command in.
         number_of_retries: Number of times to retry the command, if it fails.
         halt_build_on_failure: Stops the build dead in its tracks if this step
             fails. Use for critical steps. This option does not make sense with
             warn_on_failure.
         warn_on_failure: If true, this step isn't that important and will not
             cause a failed build on failure.
    """
    flunk_on_failure = not warn_on_failure

    if type(descriptor) is str:
      descriptor = [descriptor]
    # Add spaces to wrap long test names to make waterfall output more compact.
    wrapped_text = self._WrapLongLines(descriptor)

    self.addStep(MonitoredRetryingShellCommand(
        build_status_oracle=self.build_status_oracle,
        number_of_retries=number_of_retries,
        command=cmd,
        workdir=workdir,
        description=wrapped_text + ['running...'],
        descriptionDone=wrapped_text,
        warnOnFailure=warn_on_failure,
        flunkOnFailure=flunk_on_failure,
        haltOnFailure=halt_build_on_failure,
        name='_'.join(descriptor)))

  def AddSmartCleanStep(self):
    """Adds a smart clean step.

       Smart clean only cleans the whole repository if the build status oracle
       thinks the last build failed. Otherwise it cleans just the build output.
    """
    self.addStep(SmartClean(self.build_status_oracle))

  def AddCommonTestRunStep(self, test, descriptor='', cmd=None,
                           workdir='build/trunk'):
    """Adds a step for running a single test [must be overridden].

       In general, failing tests should not halt the build and allow other tests
       to execute. A failing test should fail, or 'flunk', the build though.

       Implementations of this method must add new steps through AddCommonStep
       and not by calling addStep.

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

  def AddGclientSyncStep(self, force_sync):
    """Helper method for invoking gclient sync. Will retry if the operation
       fails.

       Args:
           force_sync: If the sync should be forced, i.e. update even for
           unchanged modules (known to be required for Windows sometimes).
    """
    cmd = ['gclient', 'sync']
    if force_sync:
      cmd.append('--force')
    self.AddCommonStep(cmd, descriptor='Sync',
                       number_of_retries=GCLIENT_RETRIES)

  def AddCommonGYPStep(self, gyp_file, gyp_params=[], descriptor='gyp'):
    """Helper method for invoking GYP on WebRTC.

       GYP will generate makefiles or its equivalent in a platform-specific
       manner. A failed GYP step will halt the build.

       Implementations of this method must add new steps through AddCommonStep
       and not by calling addStep.

       Args:
         gyp_file: The root GYP file to use.
         gyp_params: Custom GYP parameters (same semantics as the GYP_PARAMS
             environment variable).
         descriptor: The descriptor to use for the step.
    """
    cmd = ['./build/gyp_chromium', '--depth=.', gyp_file]
    cmd += gyp_params + self.gyp_params
    self.AddCommonStep(cmd=cmd, workdir='build/trunk', descriptor=descriptor)

  def _WrapLongLines(self, string_list, max_line_length=25, wrap_character='_'):
    """ Creates a list with wrapped strings for lines that are too long.

       This is done by inserting spaces to long lines with the wrap character
       in. It's a simple way to make long test targets wrap nicer in the
       waterfall display.

       This method should only be used for lists that are displayed in the web
       interface!

       Args:
           string_list: List of strings where each string represents one line.
           max_line_length: Number of characters a line may have to avoid
             getting wrapped.
           wrap_character: The character we're looking for when inserting a
             space if a string is larger than max_line_length. If no such
             character is found, no space will be inserted.
        Returns:
            A new list of the same length as the input list, but with strings
            that may contain extra spaces in them, if longer than the max
            length.
    """
    result = []
    for line in string_list:
      if len(line) > max_line_length:
        index = line.rfind(wrap_character)
        if index != -1:
          line = line[:index] + ' ' + line[index:]
      result.append(line)
    return result


class BuildStatusOracle:
  """Keeps track of a particular build's state.

     The oracle uses files in the default master work directory to keep track
     of whether a build has failed. It only keeps track of the most recent build
     until told to forget it.
  """

  def __init__(self, builder_name):
    """Creates the oracle.

       Args:
         builder_name: The name of the associated builder. This name is used
             in the filename on disk. This name should be unique.
    """
    self.builder_name = builder_name
    self.master_work_dir = DEFAULT_MASTER_WORK_DIR

  def LastBuildFailed(self):
    failure_file_path = self._GetFailureBuildPath()
    return os.path.exists(failure_file_path)

  def ForgetLastBuild(self):
    if self.LastBuildFailed():
      os.remove(self._GetFailureBuildPath())

  def SetLastBuildAsFailed(self):
    open(self._GetFailureBuildPath(), 'w').close()

  def _GetFailureBuildPath(self):
    return os.path.join(self.master_work_dir, self.builder_name + ".failed")


class MonitoredRetryingShellCommand(ShellCommand):
  """Wraps a shell command and notifies the oracle if the command fails.

  If the command fails, there's an option to retry it a number of times.
  Default behavior is to not retry."""

  def __init__(self, build_status_oracle, number_of_retries=0, **kwargs):
    ShellCommand.__init__(self, **kwargs)

    self.addFactoryArguments(build_status_oracle=build_status_oracle,
                             number_of_retries=number_of_retries)
    self.build_status_oracle = build_status_oracle
    self.number_of_retries = number_of_retries

  def finished(self, results):
    if (results == buildbot.status.builder.FAILURE or
        results == buildbot.status.builder.EXCEPTION):
      if self.number_of_retries > 0:
        self.number_of_retries -= 1
        self.start()
        return
      else:
        self.build_status_oracle.SetLastBuildAsFailed()
    ShellCommand.finished(self, results)


class SmartClean(ShellCommand):
  """Cleans the repository fully or partially depending on the build state."""
  def __init__(self, build_status_oracle, **kwargs):
    ShellCommand.__init__(self, **kwargs)

    self.addFactoryArguments(build_status_oracle=build_status_oracle)
    self.haltOnFailure = True
    self.build_status_oracle = build_status_oracle

  def start(self):
    if self.build_status_oracle.LastBuildFailed():
      self.build_status_oracle.ForgetLastBuild()
      self.description = ['Nuke Repository', '(Previous Failed)']
      self.setCommand(['rm', '-rf', 'trunk'])
    else:
      self.description = ['Clean']
      self.setCommand('rm -rf trunk/out && '
                      'rm -rf trunk/xcodebuild &&'
                      'rm -rf trunk/build/Debug &&'
                      'rm -rf trunk/build/Release')
    ShellCommand.start(self)


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
    self.setDefaultWorkdir('build/trunk')
    self.coverage_url = coverage_url
    self.coverage_dir = coverage_dir
    self.coverage_file = coverage_file
    self.description = ['Coverage Report']
    self.warnOnFailure = True
    self.flunkOnFailure = False
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

  def __init__(self, build_status_oracle):
    WebRTCFactory.__init__(self, build_status_oracle)

  def EnableBuild(self, product='toro'):
    prefix = 'rm -rf out/target/product/%s/obj/' % product
    cleanup_list = [
        'rm -rf external/webrtc',
        prefix + 'STATIC_LIBRARIES/libwebrtc_*',
        prefix + 'SHARE_LIBRARIES/libwebrtc_*',
        prefix + 'EXECUTABLES/webrtc_*'
        ]
    cmd = ' ; '.join(cleanup_list)
    self.AddCommonStep(cmd, descriptor='cleanup', workdir='build/trunk')

    cmd = 'svn checkout %s external/webrtc' % WEBRTC_SVN_LOCATION
    self.AddCommonStep(cmd, descriptor='svn (checkout)', workdir='build/trunk')

    cmd = ('source build/envsetup.sh && lunch full_%s-eng '
           '&& mmm external/webrtc showcommands' % product)
    self.AddCommonStep(cmd, descriptor='build', workdir='build/trunk')


class WebRTCChromeFactory(WebRTCFactory):
  """Sets up the Chrome OS build."""

  def __init__(self, build_status_oracle):
    WebRTCFactory.__init__(self, build_status_oracle)

  def EnableBuild(self):
    self.AddCommonStep(['rm', '-rf', 'src'], descriptor='Cleanup')
    self.AddGclientSyncStep(force_sync=True)
    self.AddCommonMakeStep('chrome')

  def AddCommonMakeStep(self, target, make_extra=None):
    descriptor = ['make ' + target]
    cmd = ['make', target, '-j100']
    if make_extra is not None:
      cmd.append(make_extra)
    self.AddCommonStep(cmd=cmd, descriptor=descriptor,
                       warn_on_failure=True, workdir='build/src')


class WebRTCLinuxFactory(WebRTCFactory):
  """Sets up the Linux build.

     This factory is quite configurable and can run a variety of builds.
  """

  def __init__(self, build_status_oracle, valgrind_enabled=False):
    WebRTCFactory.__init__(self, build_status_oracle)

    self.build_enabled = False
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

  def EnableBuild(self, release=False, build32=False, chrome_os=False,
                  clang=False):
    if build32:
      self.gyp_params.append('-Dtarget_arch=ia32')

    self.build_enabled = True
    self.release = release

    self.AddSmartCleanStep()

    # Valgrind bots need special GYP defines to enable memory profiling
    # friendly compilation. They already has a custom .gclient configuration
    # file created so they don't need one being generated like the other bots.
    if self.valgrind_enabled:
      for gyp_define in MEMORY_TOOLS_GYP_DEFINES:
        self.gyp_params.append('-D' + gyp_define)
    else:
      self.AddCommonStep(['gclient', 'config', WEBRTC_SVN_LOCATION],
                         descriptor='gclient_config')
    self.AddGclientSyncStep(force_sync=False)

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

  def AddCommonTestRunStep(self, test, extra_text=None, cmd=None,
                           workdir='build/trunk'):
    descriptor = [test, extra_text] if extra_text else [test]
    if cmd is None:
      test_folder = 'Release' if self.release else 'Debug'
      cmd = ['out/%s/%s' % (test_folder, test)]
    if self.valgrind_enabled:
      cmd = VALGRIND_CMD + cmd
    self.AddCommonStep(cmd, descriptor=descriptor, workdir=workdir,
                       halt_build_on_failure=False)

  def AddXvfbTestRunStep(self, test_name, test_binary, test_arguments=''):
    """ Adds a test to be run inside a XVFB window manager."""
    cmd = ('xvfb-run '
           '--server-args="-screen 0 800x600x24 -extension Composite" '
           '%s %s' % (test_binary, test_arguments))
    self.AddCommonTestRunStep(test=test_name, cmd=cmd)

  def AddCommonMakeStep(self, target, extra_text=None, make_extra=None):
    descriptor = ['make ' + target, extra_text] if extra_text else ['make ' +
                                                                    target]
    cmd = ['make', target, '-j100']
    if make_extra:
      cmd.append(make_extra)
    self.AddCommonStep(cmd=cmd, descriptor=descriptor, workdir='build/trunk')

  def AddStepsToEstablishCoverageBaseline(self):
    self.AddCommonStep(['lcov', '--directory', '.', '--capture', '-b',
                        '.', '--initial',
                        '--output-file', 'webrtc_base.info'],
                       workdir='build/trunk',
                       warn_on_failure=True,
                       halt_build_on_failure=False,
                       descriptor='LCOV (Baseline Capture)')
    self.AddCommonStep(['lcov', '--extract', 'webrtc_base.info', '*/src/*',
                        '--output', 'filtered.info'],
                       workdir='build/trunk',
                       warn_on_failure=True,
                       halt_build_on_failure=False,
                       descriptor='LCOV (Baseline Extract)')
    self.AddCommonStep(['lcov', '--remove', 'filtered.info', '*/usr/include/*',
                        '/third*', '/testing/*', '*/test/*', '*_unittest.*',
                        '*/mock/*', '--output',
                        'webrtc_base_filtered_final.info'],
                       workdir='build/trunk',
                       warn_on_failure=True,
                       halt_build_on_failure=False,
                       descriptor='LCOV (Baseline Filter)')

  def AddStepsToComputeCoverage(self):
    """Enable coverage data."""

    # Delete all third-party .gcda files to save time and work around a bug
    # in lcov which tends to hang when capturing on libjpgturbo.
    self.AddCommonStep(['./tools/continuous_build/clean_third_party_gcda.sh'],
                       warn_on_failure=True,
                       halt_build_on_failure=False,
                       workdir='build/trunk',
                       descriptor='LCOV (Delete 3rd party)')
    self.AddCommonStep(['lcov', '--directory', '.', '--capture', '-b',
                        '.', '--output-file', 'webrtc.info'],
                       warn_on_failure=True,
                       halt_build_on_failure=False,
                       workdir='build/trunk',
                       descriptor='LCOV (Capture)')
    self.AddCommonStep(['lcov', '--extract', 'webrtc.info', '*/src/*',
                        '--output', 'test.info'],
                       warn_on_failure=True,
                       halt_build_on_failure=False,
                       workdir='build/trunk',
                       descriptor='LCOV (Extract)')
    self.AddCommonStep(['lcov', '--remove', 'test.info', '*/usr/include/*',
                        '/third*', '/testing/*', '*/test/*', '*_unittest.*',
                        '*/mock/*', '--output',
                        'final.info'],
                       warn_on_failure=True,
                       halt_build_on_failure=False,
                       workdir='build/trunk',
                       descriptor='LCOV (Filter)')
    self.AddCommonStep(['lcov', '-a', 'webrtc_base_filtered_final.info', '-a',
                        'final.info', '-o', 'final.info'],
                       warn_on_failure=True,
                       halt_build_on_failure=False,
                       workdir='build/trunk',
                       descriptor='LCOV (Merge)')
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
                            descriptor='GYP fixed point')
      self.AddCommonMakeStep(test, extra_text='(fixed point)')
      self.AddCommonTestRunStep(test, extra_text='(fixed point)')
    elif test == 'vie_auto_test':
      # TODO(phoglund): Enable the full stack test once it is completed and
      # nonflaky.
      binary = 'out/Debug/vie_auto_test'
      args = (
          '--automated --gtest_filter="'
          '-ViEVideoVerificationTest.RunsFullStackWithoutErrors" '
          '--capture_test_ensure_resolution_alignment_in_capture_device=false')
      self.AddXvfbTestRunStep(test_name=test, test_binary=binary,
                              test_arguments=args)
    elif test == 'video_render_module_test':
      self.AddXvfbTestRunStep(test_name=test,
                              test_binary='out/Debug/video_render_module_test')
    elif test == 'voe_auto_test':
      cmd = 'out/Debug/voe_auto_test --automated'
      self.AddCommonTestRunStep(test=test, cmd=cmd)
    else:
      self.AddCommonTestRunStep(test)


class WebRTCMacFactory(WebRTCFactory):
  """Sets up the Mac build, both for make and xcode."""

  def __init__(self, build_status_oracle):
    WebRTCFactory.__init__(self, build_status_oracle)
    self.build_type = 'both'
    self.allowed_build_types = ['both', 'xcode', 'make']

  def EnableBuild(self, build_type='both', release=False):
    self.release = release

    if build_type not in self.allowed_build_types:
      print '*** INCORRECT BUILD TYPE (%s)!!! ***' % build_type
      sys.exit(0)
    else:
      self.build_type = build_type
    self.AddSmartCleanStep()
    self.AddCommonStep(['gclient', 'config', WEBRTC_SVN_LOCATION],
                       descriptor='gclient_config')
    self.AddGclientSyncStep(force_sync=True)

    if self.build_type == 'make' or self.build_type == 'both':
      self.AddCommonGYPStep('webrtc.gyp', gyp_params=['-f', 'make'],
                            descriptor='EnableMake')
    self.AddCommonMakeStep('all')

  def AddCommonTestRunStep(self, test, extra_text=None, cmd=None,
                           workdir='build/trunk'):
    descriptor = [test, extra_text] if extra_text else [test]
    if cmd is None:
      out_path = 'xcodebuild' if self.build_type == 'xcode' else 'out'
      test_folder = 'Release' if self.release else 'Debug'
      cmd = ['%s/%s/%s' % (out_path, test_folder, test)]

    if self.build_type == 'xcode' or self.build_type == 'both':
      self.AddCommonStep(cmd, descriptor=descriptor + ['(xcode)'],
                         halt_build_on_failure=False, workdir=workdir)
    # Execute test only for 'make' build type.
    # If 'both' is enabled we'll only execute the 'xcode' built ones.
    if self.build_type == 'make':
      self.AddCommonStep(cmd, descriptor=descriptor + ['(make)'],
                         halt_build_on_failure=False, workdir=workdir)

  def AddCommonMakeStep(self, target, extra_text=None, make_extra=None):
    descriptor = [target, extra_text] if extra_text else [target]
    if self.build_type == 'make' or self.build_type == 'both':
      cmd = ['make', target, '-j100']
      if make_extra is not None:
        cmd.append(make_extra)
      if self.release:
        cmd.append('BUILDTYPE=Release')
      self.AddCommonStep(cmd, descriptor=descriptor + ['(make)'],
                         workdir='build/trunk')
    if self.build_type == 'xcode' or self.build_type == 'both':
      configuration = 'Release' if self.release else 'Debug'
      cmd = ['xcodebuild', '-project', 'webrtc.xcodeproj', '-configuration',
             configuration, '-target', 'All']
      self.AddCommonStep(cmd, descriptor=descriptor + ['(xcode)'],
                         workdir='build/trunk')

class WebRTCWinFactory(WebRTCFactory):
  """Sets up the Windows build.

     Allows building with Debug, Release or both in sequence.
  """

  def __init__(self, build_status_oracle):
    WebRTCFactory.__init__(self, build_status_oracle)
    self.configuration = 'Debug'
    self.platform = 'x64'
    self.allowed_platforms = ['x64', 'Win32']
    self.allowed_configurations = ['Debug', 'Release', 'both']

  def EnableBuild(self, platform='Win32', configuration='Debug'):
    if platform not in self.allowed_platforms:
      raise UnsupportedConfigurationError('Platform %s is not supported.'
                                          % platform)
    if configuration not in self.allowed_configurations:
      raise UnsupportedConfigurationError('Configuration %s is not supported.'
                                          % configuration)
    self.platform = platform
    self.configuration = configuration

    # List possible interfering processes here to make it easier to debug what
    # processes can interfere with us.
    cmd = '%WINDIR%\\system32\\tasklist || set ERRORLEVEL=0'
    self.AddCommonStep(cmd, 'list_processes')

    # Since Windows is very picky about locking files, make sure to kill
    # any interfering processes. Feel free to add more process kill steps if
    # necessary.
    cmd = '%WINDIR%\\system32\\taskkill /f /im svn.exe || set ERRORLEVEL=0'
    self.AddCommonStep(cmd, 'svnkill')

    # Now do the clean + build.
    self.AddSmartCleanStep()
    self.AddCommonStep(['gclient', 'config', WEBRTC_SVN_LOCATION],
                       descriptor='gclient_config')
    self.AddGclientSyncStep(force_sync=True)

    if self.configuration == 'Debug' or self.configuration == 'both':
      cmd = ['msbuild', 'webrtc.sln', '/t:Clean', '/verbosity:diagnostic',
             '/p:Configuration=Debug;Platform=%s' % (self.platform)]
      self.AddCommonStep(cmd, descriptor='Build(Clean)', workdir='build/trunk')
      cmd = ['msbuild', 'webrtc.sln', '/verbosity:diagnostic',
             '/p:Configuration=Debug;Platform=%s' % (self.platform)]
      self.AddCommonStep(cmd, descriptor='Build(Debug)', workdir='build/trunk')
    if self.configuration == 'Release' or self.configuration == 'both':
      cmd = ['msbuild', 'webrtc.sln', '/t:Clean', '/verbosity:diagnostic',
             '/p:Configuration=Release;Platform=%s' % (self.platform)]
      self.AddCommonStep(cmd, descriptor='Build(Clean)', workdir='build/trunk')
      cmd = ['msbuild', 'webrtc.sln', '/verbosity:diagnostic',
             '/p:Configuration=Release;Platform=%s' % (self.platform)]
      self.AddCommonStep(cmd, descriptor='Build(Release)',
                         workdir='build/trunk')

  def AddCommonTestRunStep(self, test, cmd=None, workdir='build/trunk'):
    descriptor = [test]
    if self.configuration == 'Debug' or self.configuration == 'both':
      if cmd is None:
        cmd = ['build\Debug\%s.exe' % test]
      self.AddCommonStep(cmd, descriptor=descriptor,
                         halt_build_on_failure=False, workdir=workdir)
    if self.configuration == 'Release' or self.configuration == 'both':
      if cmd is None:
        cmd = ['build\Release\%s.exe' % test]
      self.AddCommonStep(cmd, descriptor=descriptor,
                         halt_build_on_failure=False, workdir=workdir)

# Utility functions


class UnsupportedConfigurationError(Exception):
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
       UnsupportedConfigurationError: if the platform supplied is not supported.
  """
  if platform not in SUPPORTED_PLATFORMS:
    raise UnsupportedConfigurationError('Platform %s is not supported.'
                                        % platform)
  result = []
  platform_index = SUPPORTED_PLATFORMS.index(platform)
  for test_name, enabled_platforms in test_dict.iteritems():
    if enabled_platforms[platform_index]:
      result.append(test_name)
  result.sort()
  return result
