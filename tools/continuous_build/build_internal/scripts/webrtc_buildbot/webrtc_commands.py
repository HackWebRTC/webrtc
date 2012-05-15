#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = 'kjellander@webrtc.org (Henrik Kjellander)'

"""Set of utilities to add commands to a buildbot factory.

This is based on chromium_commands.py and adds WebRTC-specific commands."""

from buildbot.steps.shell import ShellCommand

from master.factory import chromium_commands
from webrtc_buildbot import utils

DEFAULT_BLOAT_DIR = '/var/www/bloat'
DEFAULT_BLOAT_URL = 'http://webrtc-chrome.lul/bloat/webrtc_bloat.html'


class PlatformNotSupportedException(Exception): pass
class MissingGypDefineException(Exception): pass


class WebRTCCommands(chromium_commands.ChromiumCommands):
  """Encapsulates methods to add WebRTC commands to a buildbot factory."""

  def __init__(self, factory=None, target=None, build_dir=None,
               target_platform=None, target_arch=None):
    chromium_commands.ChromiumCommands.__init__(self, factory, target,
                                                build_dir, target_platform)

    self._arch = target_arch
    self._factory = factory

  def AddPyAutoTests(self, factory_properties=None, timeout=1200):
    """Adds WebRTC PyAuto test steps."""
    # The WEBRTC group lists the PyAuto tests we have written for WebRTC.
    # It's located at: src/chrome/test/functional/PYAUTO_TESTS
    self.AddPyAutoFunctionalTest('WebRTC functional PyAuto test',
                                 suite='WEBRTC', timeout=timeout, perf=False,
                                 factory_properties=factory_properties)

  def AddCompilePeerConnectionServerStep(self):
    # Add platform dependent peerconnection_server compilation:
    solution = None
    options = None
    if self._target_platform.startswith('linux'):
      options=['peerconnection_server']
    elif self._target_platform.startswith('win'):
      solution=r'..\third_party\libjingle\libjingle.sln;peerconnection_server'
    elif self._target_platform is 'mac':
      options=['--', '-project', '../third_party/libjingle/libjingle.xcodeproj',
               '-target', 'peerconnection_server'],
    else:
      raise PlatformNotSupportedException(
          'Platform "%s" is not currently supported.' % self._target_platform)
    self.AddCompileStep(solution=solution,
                        options=options,
                        description='compiling peerconnection_server',
                        descriptionDone='compile peerconnection_server')

  def AddBloatCalculationStep(self, factory_properties):
    """Runs a bloat calculation, which will yield a size breakdown for Chrome.

    If running in Release mode, you should also run with profiling to get the
    symbols right. Running this on Debug mode will work but it will probably
    take hours.

    This step command is only supported on Linux platforms.
    """
    if self._target is 'Release':
      factory = factory_properties
      if not (factory.has_key('gclient_env') and
              factory['gclient_env'].has_key('GYP_DEFINES') and
              factory['gclient_env']['GYP_DEFINES'].find('profiling=1') != -1):
        raise MissingGypDefineException(
            'You must add a dictionary to the gclient_env factory property'
            'containing a key GYP_DEFINES and a value containing profiling=1.')

    bloat_path = self.PathJoin(utils.WEBRTC_BUILD_DIR, '..', '..', '..', '..',
                               '..', '..', 'build_internal', 'symsrc',
                               'calculate_bloat.py')
    output_filename = self.PathJoin(DEFAULT_BLOAT_DIR, 'bloat_latest.json')
    chrome_binary = self.PathJoin('out', self._target, 'chrome')
    cmd = [bloat_path, '--binary', chrome_binary, '--source-path', '.',
           '--output-file', output_filename]
    self._factory.addStep(ShellCommandWithUrl(command=cmd,
                                              url=DEFAULT_BLOAT_URL,
                                              description='calculate bloat',
                                              warnOnFailure=True,
                                              workdir='build/src',
                                              timeout=7200))


class ShellCommandWithUrl(ShellCommand):
  """A regular shell command which posts a link when it's done."""
  def __init__(self, url, **kwargs):
    ShellCommand.__init__(self, **kwargs)
    self.addFactoryArguments(url=url)
    self.url = url

  def createSummary(self, log):
    self.addURL('click here', self.url)
