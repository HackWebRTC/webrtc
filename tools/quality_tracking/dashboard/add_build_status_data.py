#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Implements a handler for adding build status data."""

__author__ = 'phoglund@webrtc.org (Patrik Höglund)'

from google.appengine.ext import db

import oauth_post_request_handler


SUCCESSFUL_STRING_TO_BOOLEAN = {'successful': True, 'failed': False}


class BuildStatusData(db.Model):
  """This represents one build status report from the build bot."""
  bot_name = db.StringProperty(required=True)
  build_number = db.IntegerProperty(required=True)
  successful = db.BooleanProperty(required=True)


def _filter_oauth_parameters(post_keys):
  return filter(lambda post_key: not post_key.startswith('oauth_'),
                post_keys)


class AddBuildStatusData(oauth_post_request_handler.OAuthPostRequestHandler):
  """Used to report build status data."""

  def post(self):
    for bot_name in _filter_oauth_parameters(self.request.arguments()):
      status = self.request.get(bot_name)
      parsed_status = status.split('-')
      if len(parsed_status) != 2:
        raise ValueError('Malformed status string %s for bot %s.' %
                         (status, bot_name))

      parsed_build_number = int(parsed_status[0])
      successful = parsed_status[1]

      if successful not in SUCCESSFUL_STRING_TO_BOOLEAN:
        raise ValueError('Malformed status string %s for bot %s.' % (status,
                                                                     bot_name))
      parsed_successful = SUCCESSFUL_STRING_TO_BOOLEAN[successful]

      item = BuildStatusData(bot_name=bot_name,
                             build_number=parsed_build_number,
                             successful=parsed_successful)
      item.put()
