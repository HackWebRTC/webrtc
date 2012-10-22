#!/usr/bin/env python
#-*- coding: utf-8 -*-
#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

import unittest

import peerconnection_fuzz

SAMPLE_TEMPLATE = """
﻿<html>
<head>
  <title>WebRTC PeerConnection Fuzz Test Template</title>
  <script type="text/javascript">
  var gFirstConnection = null;

// START_OF_POSSIBLE_INJECTED_LOCATION_RELOADS
  function startTest() {
    navigator.webkitGetUserMedia(REQUEST_AUDIO_AND_VIDEO,
                                 getUserMediaOkCallback,
                                 getUserMediaFailedCallback);
  }

  function callUsingStream(localStream) {
    gFirstConnection.createOffer(onOfferCreated);
  }

  function onIceCandidateToFirst(event) {
    if (event.candidate) {
      var candidate = new RTCIceCandidate(event.candidate);
      gSecondConnection.addIceCandidate(candidate);
    }
  }
// END_OF_POSSIBLE_INJECTED_LOCATION_RELOADS
  </script>
</head>"""

RELOADED_TEMPLATE = """
﻿<html>
<head>
  <title>WebRTC PeerConnection Fuzz Test Template</title>
  <script type="text/javascript">
  var gFirstConnection = null;


  function startTest() {
    navigator.webkitGetUserMedia(REQUEST_AUDIO_AND_VIDEO,
                                 getUserMediaOkCallback,
                                 getUserMediaFailedCallback);
    location.reload();
  }

  function callUsingStream(localStream) {
    gFirstConnection.createOffer(onOfferCreated);
    location.reload();
  }

  function onIceCandidateToFirst(event) {
    if (event.candidate) {
      var candidate = new RTCIceCandidate(event.candidate);
    location.reload();
      gSecondConnection.addIceCandidate(candidate);
    location.reload();
    }
  }

  </script>
</head>"""

class PeerConnectionFuzzerTest(unittest.TestCase):
  def testInsertRandomReloadsInsertsAtTheRightPlace(self):
    """Tests we can insert location.reload() in the right places.

    Only tests the case where we replace all since the other case is random.
    """
    result = peerconnection_fuzz._InsertRandomLocationReloadsWithinMarkers(
        SAMPLE_TEMPLATE, replace_all=True)
    self.assertEquals(RELOADED_TEMPLATE, result,
                      'Got "%s", should be "%s"' % (result, RELOADED_TEMPLATE))

if __name__ == '__main__':
  unittest.main()
