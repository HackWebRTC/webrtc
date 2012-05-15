#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = 'kjellander@webrtc.org (Henrik Kjellander)'

"""Utility class to build Chromium with the latest WebRTC.

Based on chromium_factory.py and adds WebRTC-specific custom_deps."""

from master.factory import chromium_factory
from webrtc_buildbot import webrtc_commands


class ChromiumWebRTCFactory(chromium_factory.ChromiumFactory):

  # gclient additional custom deps
  CUSTOM_DEPS_WEBRTC_LATEST = ('src/third_party/webrtc',
                               'http://webrtc.googlecode.com/svn/stable/src')

  def ChromiumWebRTCLatestFactory(self, target='Release', clobber=False,
                                  tests=None, mode=None,
                                  slave_type='BuilderTester', options=None,
                                  compile_timeout=1200, build_url=None,
                                  project=None, factory_properties=None):
    self._solutions[0].custom_deps_list = [self.CUSTOM_DEPS_WEBRTC_LATEST]
    factory = self.ChromiumFactory(target, clobber, tests, mode, slave_type,
                                   options, compile_timeout, build_url, project,
                                   factory_properties)
    webrtc_cmd_obj = webrtc_commands.WebRTCCommands(factory, target,
                                                    self._build_dir,
                                                    self._target_platform)
    webrtc_cmd_obj.AddCompilePeerConnectionServerStep()
    webrtc_cmd_obj.AddPyAutoTests(factory_properties)
    return factory

  def ChromiumWebRTCBloatFactory(self, target='Release', clobber=False,
                                 tests=None, mode=None,
                                 slave_type='BuilderTester', options=None,
                                 compile_timeout=1200, build_url=None,
                                 project=None, factory_properties=None):
    self._solutions[0].custom_deps_list = [self.CUSTOM_DEPS_WEBRTC_LATEST]
    factory = self.ChromiumFactory(target, clobber, tests, mode, slave_type,
                                   options, compile_timeout, build_url, project,
                                   factory_properties)
    webrtc_cmd_obj = webrtc_commands.WebRTCCommands(factory, target,
                                                    self._build_dir,
                                                    self._target_platform)
    webrtc_cmd_obj.AddBloatCalculationStep(factory_properties)
    return factory
