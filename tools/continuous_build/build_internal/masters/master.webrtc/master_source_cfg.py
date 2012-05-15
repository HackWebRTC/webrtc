#!/usr/bin/env python
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

__author__ = 'kjellander@webrtc.org (Henrik Kjellander)'

"""Source control poller for the WebRTC code."""

from buildbot.changes import svnpoller
from master import build_utils


def WebRTCFileSplitter(path):
  """Splits the SVN path into branch and filename sections."""
  # We're currently only using trunk branch but we may soon start building
  # multiple branches.
  projects = ['trunk']
  return build_utils.SplitPath(projects, path)


def ConfigureChangeSource(config, c):
  webrtc_revlinktmpl = 'http://code.google.com/p/webrtc/source/browse?r=%s'
  webrtc_poller = svnpoller.SVNPoller(svnurl=config.Master.webrtc_root_url,
                                      split_file=WebRTCFileSplitter,
                                      pollinterval=30,
                                      histmax=10,
                                      revlinktmpl=webrtc_revlinktmpl)
  c['change_source'] = webrtc_poller
