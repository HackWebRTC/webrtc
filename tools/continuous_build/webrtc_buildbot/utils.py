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

# Defines the order of the booleans of the supported platforms in the test
# dictionaries in master.cfg.
SUPPORTED_PLATFORMS = ("Linux", "Mac", "Windows")

SVN_LOCATION = "http://webrtc.googlecode.com/svn/trunk"
VALGRIND_CMD = ["tools/valgrind-webrtc/webrtc_tests.sh", "-t", "cmdline"]

# Copied from Chromium's
# trunk/tools/build/scripts/master/factory/chromium_factory.py
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
  """A Build Factory affected by properties."""

  def __init__(self, build_factory_properties=None, steps=None,
               enable_coverage=False, enable_valgrind=False, account=None):
    factory.BuildFactory.__init__(self, steps)
    self.properties = properties.Properties()
    self.enable_build = False
    self.force_sync = False
    self.enable_coverage = enable_coverage
    self.enable_valgrind = enable_valgrind
    self.gyp_params = []
    self.account = account
    self.coverage_dir = ""
    self.coverage_url = ""
    self.release = False
    if self.account is None:
      self.coverage_url = "http://www.corp.google.com/~webrtc-cb"
      self.coverage_dir = "/home/webrtc-cb/www"
    else:
      self.coverage_url = "http://www.corp.google.com/~%s" % self.account
      self.coverage_dir = "/home/%s/www" % self.account

  def EnableBuild(self, force_sync):
    """Build the binary [must be overridden]."""
    pass

  def AddCommonTestSteps(self, test):
    """Add common steps for test.

       test: test to be run.
    """
    self.AddCommonMakeStep(test)
    self.AddCommonTestRunStep(test)

  def AddCommonStep(self, cmd, descriptor="", workdir="build",
                    warnOnFailure=False,):
    """Define common step."""
    if type(descriptor) is str:
      descriptor = [descriptor]
    warn = warnOnFailure
    flunkOnFailure = not warn
    self.addStep(shell.ShellCommand(command=cmd, workdir=workdir,
                                    description=descriptor+["running..."],
                                    descriptionDone=descriptor+["done..."],
                                    warnOnFailure=warn,
                                    flunkOnFailure=flunkOnFailure,
                                    name="".join(descriptor)))

  def AddCommonTestRunStep(self):
    """Define common test run step [must be overridden]."""
    pass

  def AddCommonMakeStep(self):
    """Define common make step [must be overridden]."""
    pass

  def AddCommonGYPStep(self, gyp_file, gyp_params=[], descriptor="gyp"):
    cmd = ["./build/gyp_chromium", "--depth=.", gyp_file]
    cmd += gyp_params + self.gyp_params
    self.addStep(shell.ShellCommand(command=cmd, workdir="build/trunk",
                                    description=[descriptor, "running..."],
                                    descriptionDone=[descriptor, "done..."],
                                    name="gyp_%s" % descriptor))

  def EnableTest(self, test):
    """Enable Test to be run. May be overridden.

    Override to handle special cases for specific platforms.

       test: test to be run.
    """
    self.AddCommonTestRunStep(test)

  def EnableTests(self, tests):
    """Enable Test to be run.

       tests: list of test to be run.
    """
    if self.enable_coverage:
      self.EnableBaseCoverage()
    for test in tests:
      self.EnableTest(test)
    if self.enable_coverage:
      self.EnableCoverage()

  def EnableBaseCoverage(self):
    """Enable base coverage data [must be overridden]."""
    pass

  def EnableCoverage(self):
    """Enable coverage data [must be overridden]."""
    pass


class GenerateCodeCoverage(ShellCommand):
  command = ["genhtml", "final.info", "--output-directory",
      WithProperties("/home/webrtc-cb/www/%(buildername)s_%(buildnumber)s")]
  name = "LCOV_GenHTML"
  warnOnFailure = True

  def __init__(self, coverage_url=None, coverage_dir=None, **kwargs):
    if coverage_url is None or coverage_dir is None:
      raise TypeError("coverage location required")
    print coverage_url, coverage_dir
    ShellCommand.__init__(self, **kwargs)
    self.addFactoryArguments(coverage_url=coverage_url,
                             coverage_dir=coverage_dir)
    self.setDefaultWorkdir("build/trunk")
    self.coverage_url = coverage_url
    self.coverage_dir = coverage_dir
    self.setCommand(["genhtml", "final.info", "--output-directory",
      WithProperties(coverage_dir + "/%(buildername)s_%(buildnumber)s")])

  def createSummary(self, log):
    coverage_url = "%s/%s_%s" % (self.coverage_url,
        self.getProperty("buildername"), self.getProperty("buildnumber"))
    coverage_dir = "%s/%s_%s" % (self.coverage_dir,
        self.getProperty("buildername"), self.getProperty("buildnumber"))
    os.chmod(coverage_dir,0777)
    for root, dirs, files in os.walk(coverage_dir):
      for d in dirs:
        os.chmod(os.path.join(root, d), 0777)
      for f in files:
        os.chmod(os.path.join(root, f), 0777)
    self.addURL("coverage", coverage_url)

  def start(self):
    ShellCommand.start(self)

################################################################################
class WebRTCAndroidFactory(WebRTCFactory):
  """A Build Factory affected by properties."""

  def __init__(self, build_factory_properties=None, steps=None,
               enable_coverage=False, account=None):
    WebRTCFactory.__init__(self, build_factory_properties, steps,
                           enable_coverage, account)

  def EnableBuild(self, product="toro"):
    prefix = "rm -rf out/target/product/%s/obj/" % product
    cleanup_list = [
                    "rm -rf external/webrtc",
                    prefix + "STATIC_LIBRARIES/libwebrtc_*",
                    prefix + "SHARE_LIBRARIES/libwebrtc_*",
                    prefix + "EXECUTABLES/webrtc_*"
                    ]
    cmd = " ; ".join(cleanup_list)
    self.addStep(shell.Compile(command=(cmd), workdir="build/trunk",
                 description=["cleanup", "running..."], haltOnFailure=False,
                 warnOnFailure=True, flunkOnFailure =False,
                 descriptionDone=["cleanup", "done..."], name="cleanup"))
    cmd = "svn checkout %s external/webrtc" % SVN_LOCATION
    self.addStep(shell.Compile(command=(cmd),
        workdir="build/trunk", description=["svn", "running..."],
        haltOnFailure=False, descriptionDone=["svn", "done..."], name="svn"))
    cmd = ("source build/envsetup.sh && lunch full_%s-eng "
           "&& mmm external/webrtc showcommands" % product)
    self.addStep(shell.Compile(command=(cmd),
        workdir="build/trunk", description=["build", "running..."],
        haltOnFailure=False,
        descriptionDone=["build", "done..."], name="build"))

################################################################################

################################################################################
class WebRTCChromeFactory(WebRTCFactory):
  """A Build Factory affected by properties."""

  def __init__(self, build_factory_properties=None, steps=None,
               enable_coverage=False, account=None):
    WebRTCFactory.__init__(self, build_factory_properties, steps,
                           enable_coverage, account)

  def EnableBuild(self):
    self.AddCommonStep(["rm", "-rf", "src"], descriptor="Cleanup")
    cmd = ["gclient", "sync", "--force"]
    self.AddCommonStep(cmd, descriptor="Sync")
    self.AddCommonMakeStep("chrome")

  def AddCommonMakeStep(self, make, descriptor="", make_extra=None):
    make_descriptor = [make, descriptor]
    cmd = ["make", make, "-j100"]
    if make_extra is not None:
      cmd.append(make_extra)
    self.addStep(shell.ShellCommand(command=cmd,
        workdir="build/src", description=["Making"]+make_descriptor,
        descriptionDone=make_descriptor+["built"],
        name="_".join(make_descriptor)))

################################################################################
class WebRTCLinuxFactory(WebRTCFactory):
  """A Build Factory affected by properties."""

  def __init__(self, build_factory_properties=None, steps=None,
               enable_coverage=False, enable_valgrind=False, account=None):
    WebRTCFactory.__init__(self, build_factory_properties, steps,
                           enable_coverage, enable_valgrind, account)

  def EnableBuild(self, force_sync=False, release=False, build32=False,
                  chrome_os=False, clang=False):
    if build32:
      self.gyp_params.append("-Dtarget_arch=ia32")

    self.enable_build = True
    self.force_sync = force_sync
    """Linux specific Build"""
    self.release = release
    self.AddCommonStep(["rm", "-rf", "trunk"], descriptor="Cleanup")
    # Valgrind bots need special GYP defines to enable memory profiling
    # friendly compilation. They already has a custom .gclient
    # configuration file created so they don't need one being
    # generated like the other bots.
    if self.enable_valgrind:
      for gyp_define in MEMORY_TOOLS_GYP_DEFINES:
        self.gyp_params.append("-D" + gyp_define)
    else:
      self.AddCommonStep(["gclient", "config", SVN_LOCATION],
                         descriptor="gclient_config")

    cmd = ["gclient", "sync"]
    if force_sync:
      cmd.append("--force")
    self.AddCommonStep(cmd, descriptor="Sync")
    if chrome_os:
      self.gyp_params.append("-Dchromeos=1")

    if clang:
      self.gyp_params.append("-Dclang=1")

    if self.enable_coverage:
      self.gyp_params.append("-Dcoverage=1")
    self.AddCommonGYPStep("webrtc.gyp", descriptor="CommonGYP")

    if clang:
      self.AddCommonStep(["trunk/tools/clang/scripts/update.sh"],
                          descriptor="Update_Clang")

    if self.release:
      self.AddCommonMakeStep("all", make_extra="BUILDTYPE=Release")
    else:
      self.AddCommonMakeStep("all")

  def AddCommonTestRunStep(self, test, descriptor="", cmd=None,
                           workdir="build/trunk"):
    test_folder = "Release" if self.release else "Debug"
    test_descriptor = [test, descriptor]
    if cmd is None:
      cmd = ["out/%s/%s" % (test_folder, test)]
    if self.enable_valgrind:
      cmd = VALGRIND_CMD + cmd
    self.addStep(shell.ShellCommand(command=cmd,
        workdir=workdir, description=["Running"]+test_descriptor,
        descriptionDone=test_descriptor+["finished"],
        name="_".join(test_descriptor)))

  def AddCommonMakeStep(self, make, descriptor="", make_extra=None):
    make_descriptor = [make, descriptor]
    #cpu = `grep -i \"processor\" /proc/cpuinfo | sort -u | wc -l`
    cmd = ["make", make, "-j100"]
    if make_extra is not None:
      cmd.append(make_extra)
    self.addStep(shell.ShellCommand(command=cmd,
        workdir="build/trunk", description=["Making"]+make_descriptor,
        descriptionDone=make_descriptor+["built"],
        name="_".join(make_descriptor)))

  def EnableBaseCoverage(self):
    self.AddCommonStep(["lcov", "--directory", ".", "--zerocounters"],
                       workdir="build/trunk",
                       warnOnFailure=True,
                       descriptor=["LCOV", "Zero"])
    self.AddCommonStep(["lcov", "--directory", ".", "--capture", "-b",
                        ".", "--initial",
                        "--output-file", "webrtc_base.info"],
                       workdir="build/trunk",
                       warnOnFailure=True,
                       descriptor=["LCOV", "Base", "Capture"])
    self.AddCommonStep(['lcov', '--extract', 'webrtc_base.info', '*/src/*',
                        '--output', 'filtered.info'],
                       workdir="build/trunk",
                       warnOnFailure=True,
                       descriptor=["LCOV", "Base", "Extract"])
    self.AddCommonStep(["lcov", "--remove", "filtered.info", "*/usr/include/*",
                        "/third*", "/testing/*", "*/test/*", "*_unittest.*",
                        "*/mock/*", "--output",
                        "webrtc_base_filtered_final.info"],
                       workdir="build/trunk",
                       warnOnFailure=True,
                       descriptor=["LCOV", "Base", "Filter"])

  def EnableCoverage(self):
    """Enable coverage data."""
    self.AddCommonStep(["lcov", "--directory", ".", "--capture", "-b",
                        ".", "--output-file", "webrtc.info"],
                       warnOnFailure=True,
                       workdir="build/trunk", descriptor=["LCOV", "Capture"])
    self.AddCommonStep(['lcov', '--extract', 'webrtc.info', '*/src/*',
                        '--output', 'test.info'], warnOnFailure=True,
                       workdir="build/trunk", descriptor=["LCOV", "Extract"])
    self.AddCommonStep(["lcov", "--remove", "test.info", "*/usr/include/*", 
                        "/third*", "/testing/*", "*/test/*", "*_unittest.*",
                        "*/mock/*", "--output",
                        "final.info"], warnOnFailure=True,
                       workdir="build/trunk", descriptor=["LCOV", "Filter"])
    self.AddCommonStep(['lcov', '-a', 'webrtc_base_filtered_final.info', '-a',
                        'final.info', '-o', 'final.info'], warnOnFailure=True,
                       workdir="build/trunk", descriptor=["LCOV", "Merge"])
    self.addStep(GenerateCodeCoverage(coverage_url=self.coverage_url,
                                      coverage_dir=self.coverage_dir))

  def EnableTest(self, test):
    """Enable Test to be run.

       test: test to be run.
    """
    if test == "audioproc_unittest":
      self.AddCommonTestRunStep(test)
      self.AddCommonGYPStep("webrtc.gyp", gyp_params=["-Dprefer_fixed_point=1"],
                            descriptor="fixed_point")
      self.AddCommonMakeStep(test, descriptor="make_fixed_point")
      self.AddCommonTestRunStep(test, descriptor="fixed_point")
    elif test == "vie_auto_test":
      # TODO(phoglund): Enable the full stack test once it is completed and
      # nonflaky.
      self.addStep(shell.Compile(command=('xvfb-run --server-args="-screen 0 '
        '800x600x24 -extension Composite" out/Debug/vie_auto_test --automated '
        '--gtest_filter="-ViEVideoVerificationTest.RunsFullStackWithoutErrors:'
        '-ViEExtendedIntegrationTest.*" '
        '--capture_test_ensure_resolution_alignment_in_capture_device=false'),
        workdir="build/trunk", description=[test, "running..."],
        descriptionDone=[test, "done..."], name="%s" % test))
    elif test == "voe_auto_test":
      # TODO(phoglund): Remove this notice and take appropriate action when
      # http://code.google.com/p/webrtc/issues/detail?id=266 is concluded.
      self.addStep(shell.Compile(
        command=('out/Debug/voe_auto_test --automated '
                 '--gtest_filter="-VolumeTest.*"'),
        workdir="build/trunk", description=[test, "running..."],
        descriptionDone=[test, "done..."], name="%s" % test))
    else:
      self.AddCommonTestRunStep(test)


################################################################################
class WebRTCMacFactory(WebRTCFactory):
  """A Build Factory affected by properties."""

  def __init__(self, build_factory_properties=None, steps=None,
               enable_coverage=False, account=None):
    WebRTCFactory.__init__(self, build_factory_properties, steps,
                           enable_coverage, account)
    self.build_type = "both"
    self.allowed_build_types = ["both", "xcode", "make"]

  def EnableBuild(self, force_sync=True, build_type="both", release=False):
    self.enable_build = True
    self.force_sync = force_sync
    self.release = release
    """Mac specific Build"""
    if build_type not in self.allowed_build_types:
      print "*** INCORRECT BUILD TYPE (%s)!!! ***" % build_type
      sys.exit(0)
    else:
      self.build_type = build_type
    self.AddCommonStep(["rm", "-rf", "trunk"], descriptor="Cleanup")
    self.AddCommonStep(["gclient", "config", SVN_LOCATION],
                       descriptor="gclient_config")
    cmd = ["gclient", "sync"]
    if force_sync:
      cmd.append("--force")
    self.AddCommonStep(cmd, descriptor="Sync")
    if self.build_type == "make" or self.build_type == "both":
      self.AddCommonGYPStep("webrtc.gyp", gyp_params=["-f", "make"],
                            descriptor="EnableMake")
    self.AddCommonMakeStep("all")

  def AddCommonTestRunStep(self, test, descriptor="", cmd=None,
                           workdir="build/trunk"):
    test_folder = "Release" if self.release else "Debug"
    test_descriptor = [test, descriptor]
    if cmd is None:
      if self.build_type == "xcode" or self.build_type == "both":
        cmd = ["xcodebuild/%s/%s" % (test_folder, test)]
        self.AddCommonStep(cmd, descriptor=test_descriptor+["(xcode)"],
                           workdir="build/trunk")
      if self.build_type == "make" or self.build_type == "both":
        cmd = ["out/%s/%s" % (test_folder, test)]
        self.AddCommonStep(cmd, descriptor=test_descriptor+["(make)"],
                           workdir="build/trunk")

  def AddCommonMakeStep(self, make, descriptor="", make_extra=None):
    make_descriptor = [make, descriptor]
    cpu = "`sysctl -n hw.logicalcpu`"
    if self.build_type == "make" or self.build_type == "both":
      cmd = ["make", make, "-j100"]
      if make_extra is not None:
        cmd.append(make_extra)
      if self.release:
        cmd.append("BUILDTYPE=Release")
      self.AddCommonStep(cmd, descriptor=make_descriptor+["(make)"],
                         workdir="build/trunk")
    if self.build_type == "xcode" or self.build_type == "both":
      configuration = "Release" if self.release else "Debug"
      cmd = ["xcodebuild", "-project", "webrtc.xcodeproj", "-configuration", 
             configuration, "-target", "All"]
      self.AddCommonStep(cmd, descriptor=make_descriptor+["(xcode)"],
                         workdir="build/trunk")

################################################################################
class WebRTCWinFactory(WebRTCFactory):
  """A Build Factory affected by properties."""

  def __init__(self, build_factory_properties=None, steps=None,
               enable_coverage=False, account=None):
    WebRTCFactory.__init__(self, build_factory_properties, steps,
                           enable_coverage, account)
    self.configuration = "Debug"
    self.platform = "x64"
    self.allowed_platforms = ["x64", "Win32"]
    self.allowed_configurations = ["Debug", "Release", "both"]

  def EnableBuild(self, force_sync=True, platform="Win32",
                  configuration="Debug", build_only=False):
    self.enable_build = True
    self.force_sync = force_sync
    """Win specific Build"""
    if platform not in self.allowed_platforms:
      print "*** INCORRECT PLATFORM (%s)!!! ***" % platform
      sys.exit(0)
    else:
      self.platform = platform
    if configuration not in self.allowed_configurations:
      print "*** INCORRECT CONFIGURATION (%s)!!! ***" % configuration
      sys.exit(0)
    else:
      self.configuration = configuration
    if not build_only:
      self.AddCommonStep(["rm", "-rf", "trunk"], descriptor="Cleanup")
      self.AddCommonStep(["gclient", "config", SVN_LOCATION],
                         descriptor="gclient_config")
      cmd = ["gclient", "sync"]
      if force_sync:
        cmd.append("--force")
      self.AddCommonStep(cmd, descriptor="Sync")

    if self.configuration == "Debug" or self.configuration == "both":
      cmd = ["msbuild", "webrtc.sln", "/t:Clean",
             "/p:Configuration=Debug;Platform=%s" % (self.platform)]
      self.AddCommonStep(cmd, descriptor="Build_Clean", workdir="build/trunk")
      cmd = ["msbuild", "webrtc.sln", 
             "/p:Configuration=Debug;Platform=%s" % (self.platform)]
      self.AddCommonStep(cmd, descriptor="Build_Debug", workdir="build/trunk")
    if self.configuration == "Release" or self.configuration == "both":
      cmd = ["msbuild", "webrtc.sln", "/t:Clean",
             "/p:Configuration=Release;Platform=%s" % (self.platform)]
      self.AddCommonStep(cmd, descriptor="Build_Clean", workdir="build/trunk")
      cmd = ["msbuild", "webrtc.sln", 
             "/p:Configuration=Release;Platform=%s" % (self.platform)]
      self.AddCommonStep(cmd, descriptor="Build_Release", workdir="build/trunk")

  def AddCommonTestRunStep(self, test, descriptor="", cmd=None,
                           workdir="build/trunk"):
    test_descriptor = [test, descriptor]
    if cmd is None:
      if self.configuration == "Debug" or self.configuration == "both":
        cmd = ["build\Debug\%s.exe" % test]
        self.AddCommonStep(cmd, descriptor=test_descriptor+["Debug"],
                           workdir=workdir)
      if self.configuration == "Release" or self.configuration == "both":
        cmd = ["build\Release\%s.exe" % test]
        self.AddCommonStep(cmd, descriptor=test_descriptor+["Release"],
                           workdir=workdir)

################################################################################
# Utility functions

class UnsupportedPlatformError(Exception):
  pass


def GetEnabledTests(test_dict, platform):
  """Returns a list of enabled test names for the provided platform.

     Args:
       test_dict: Dictionary mapping test names to tuples representing if the
         test shall be enabled on each platform. Each tuple contains one boolean
         for each platform. The platforms are in the order specified by
         SUPPORTED_PLATFORMS.
       platform: The platform we're looking to get the tests for.

     Returns:
       A list of test names, sorted alphabetically.

     Raises:
       UnsupportedPlatformError: if the platform supplied is not supported.
  """
  if platform not in SUPPORTED_PLATFORMS:
    raise UnsupportedPlatformError("*** UNSUPPORTED PLATFORM (%s)!!! ***" %
                                   platform)
  result = []
  platform_index = SUPPORTED_PLATFORMS.index(platform)
  for test_name, enabled_platforms in test_dict.iteritems():
    if enabled_platforms[platform_index]:
      result.append(test_name)
  result.sort()
  return result
