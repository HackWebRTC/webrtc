#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Chrome+WebRTC bots customized for Libvpx build."""

from buildbot.schedulers import timed

from master import master_config
from webrtc_buildbot import webrtc_chromium_factory

def linux():
  return webrtc_chromium_factory.ChromiumWebRTCFactory('src/build', 'linux2')
def mac():
  return webrtc_chromium_factory.ChromiumWebRTCFactory('src/build', 'mac')
def win():
  return webrtc_chromium_factory.ChromiumWebRTCFactory('src/build', 'win32')

CHROME_LKGR = 'http://chromium-status.appspot.com/lkgr'


def ConfigureChromeWebRTCBuilders(c, custom_deps_list=[]):
  # The Libvpx Chrome bots are setup without the Chromium helper classes since
  # they don't have support for a Nightly scheduler.

  # Linux
  chrome_linux_debug_factory = linux().ChromiumWebRTCLatestFactory(
      target='Debug',
      factory_properties={'safesync_url': CHROME_LKGR},
      custom_deps_list=custom_deps_list)

  chrome_linux_debug_builder = {
      'name': 'LinuxChrome',
      'factory': chrome_linux_debug_factory,
  }
  c['builders'].append(chrome_linux_debug_builder)
  c['schedulers'][0].builderNames.append('LinuxChrome')

  # Mac 10.7 (Lion) ...
  chrome_mac_debug_factory = mac().ChromiumWebRTCLatestFactory(
      target='Debug',
      factory_properties={'safesync_url': CHROME_LKGR},
      custom_deps_list=custom_deps_list)

  chrome_mac_debug_builder = {
      'name': 'MacChrome',
      'factory': chrome_mac_debug_factory,
  }
  c['builders'].append(chrome_mac_debug_builder)
  c['schedulers'][0].builderNames.append('MacChrome')

  # Windows...
  chrome_win_debug_factory = win().ChromiumWebRTCLatestFactory(
      target='Debug',
      factory_properties={'safesync_url': CHROME_LKGR},
      custom_deps_list=custom_deps_list)

  chrome_win_debug_builder = {
      'name': 'WinChrome',
      'factory': chrome_win_debug_factory,
  }
  c['builders'].append(chrome_win_debug_builder)
  c['schedulers'][0].builderNames.append('WinChrome')


def ConfigureNightlyChromeWebRTCBloatBuilder(c, custom_deps_list=[]):
  # Must add a Bloat builder for Libvpx to avoid an error during the
  # slave configuration check. This builder will be removed at the last stage of
  # the configuration loading (see master.libvpx/master.cfg).
  chrome_bloat_factory = linux().ChromiumWebRTCBloatFactory(
      target='Release',
      factory_properties={'safesync_url': CHROME_LKGR,
                          'gclient_env': {'GYP_DEFINES': 'profiling=1'}},
      custom_deps_list=custom_deps_list)

  chrome_bloat_builder = {
      'name': 'LinuxChromeBloat',
      'factory': chrome_bloat_factory,
      'category': 'linux',
  }
  c['builders'].append(chrome_bloat_builder)
