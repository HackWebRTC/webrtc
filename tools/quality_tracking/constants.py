#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Contains tweakable constants for quality dashboard utility scripts."""

# This identifies our application using the information we got when we
# registered the application on Google appengine.
DASHBOARD_SERVER = 'webrtc-dashboard.appspot.com'
DASHBOARD_SERVER_HTTP = 'http://' + DASHBOARD_SERVER
CONSUMER_KEY = DASHBOARD_SERVER
CONSUMER_SECRET_FILE = 'consumer.secret'
ACCESS_TOKEN_FILE = 'access.token'

# OAuth URL:s.
REQUEST_TOKEN_URL = DASHBOARD_SERVER_HTTP + '/_ah/OAuthGetRequestToken'
AUTHORIZE_TOKEN_URL = DASHBOARD_SERVER_HTTP + '/_ah/OAuthAuthorizeToken'
ACCESS_TOKEN_URL = DASHBOARD_SERVER_HTTP + '/_ah/OAuthGetAccessToken'

# Build bot constants.
BUILD_BOT_COVERAGE_WWW_DIRECTORY = '/var/www/coverage'

# Dashboard data input URLs.
ADD_COVERAGE_DATA_URL = DASHBOARD_SERVER_HTTP + '/add_coverage_data'
