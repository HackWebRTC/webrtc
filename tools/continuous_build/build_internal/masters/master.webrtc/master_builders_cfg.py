#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = 'kjellander@webrtc.org (Henrik Kjellander)'

"""Chrome+WebRTC bots configured in Chromium style."""

from buildbot.schedulers import timed

from master import master_config
from webrtc_buildbot import webrtc_chromium_factory

# Used to put builders into different categories by the Helper class.
defaults = {}


def linux():
  return webrtc_chromium_factory.ChromiumWebRTCFactory('src/build', 'linux2')
def mac():
  return webrtc_chromium_factory.ChromiumWebRTCFactory('src/build', 'mac')
def win():
  return webrtc_chromium_factory.ChromiumWebRTCFactory('src/build', 'win32')

CHROME_LKGR = 'http://chromium-status.appspot.com/lkgr'


def ConfigureChromeWebRTCBuilders(c):
  helper = master_config.Helper(defaults)
  B = helper.Builder
  F = helper.Factory
  S = helper.Scheduler

  # Main Scheduler for WebRTC
  S('webrtc_rel', branch='trunk', treeStableTimer=0)

  # Set up all the builders.
  # Don't put spaces or 'funny characters' within the builder names, so that
  # we can safely use the builder name as part of a filepath.

  # Linux...
  defaults['category'] = 'linux'
  B('LinuxChrome', 'chrome_linux_debug_factory', scheduler='webrtc_rel')
  F('chrome_linux_debug_factory', linux().ChromiumWebRTCLatestFactory(
      target='Debug',
      factory_properties={'safesync_url': CHROME_LKGR,
                          'use_xvfb_on_linux': True}))

  # Mac 10.7 (Lion) ...
  defaults['category'] = 'mac-10.7'
  B('MacChrome', 'chrome_mac_debug_factory', scheduler='webrtc_rel')
  F('chrome_mac_debug_factory', mac().ChromiumWebRTCLatestFactory(
      target='Debug',
      factory_properties={'safesync_url': CHROME_LKGR}))

  # Windows...
  defaults['category'] = 'windows'
  B('WinChrome', 'chrome_win32_debug_factory', scheduler='webrtc_rel')
  F('chrome_win32_debug_factory', win().ChromiumWebRTCLatestFactory(
      project=r'..\chrome\chrome.sln',
      target='Debug',
      factory_properties={'safesync_url': CHROME_LKGR}))

  # Use the helper class to connect the builders, factories and schedulers
  # and add them to the BuildmasterConfig (c) dictionary.
  helper.Update(c)


def ConfigureNightlyChromeWebRTCBloatBuilder(c):
  # Nightly Scheduler at 2 AM CST/CDT. This will mean roughly 9 AM in the CET
  # time zone, which should avoid everyone's working hours.
  nightly_scheduler = timed.Nightly(name='webrtc_nightly',
                                    branch='trunk',
                                    builderNames=['LinuxChromeBloat'],
                                    hour=2)
  c['schedulers'].append(nightly_scheduler)

  # The Bloat calculator bot is setup without the helper classes since they
  # don't have support for a Nightly scheduler.
  chrome_bloat_factory = linux().ChromiumWebRTCBloatFactory(
      target='Release',
      factory_properties={'safesync_url': CHROME_LKGR,
                          'gclient_env': {'GYP_DEFINES': 'profiling=1'}})
  chrome_bloat_builder = {
      'name': 'LinuxChromeBloat',
      'factory': chrome_bloat_factory,
      'category': 'linux',
  }
  c['builders'].append(chrome_bloat_builder)
