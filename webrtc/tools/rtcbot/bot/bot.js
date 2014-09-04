// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

var botExposedApi = {
  ping: function (callback) {
    callback("pong");
  },

  createPeerConnection: function (doneCallback) {
    console.log("Creating peer connection");
    var pc = new webkitRTCPeerConnection(null);
    var obj = {};
    expose(obj, pc, "close");
    expose(obj, pc, "createOffer");
    expose(obj, pc, "createAnswer");
    expose(obj, pc, "setRemoteDescription", { 0: RTCSessionDescription });
    expose(obj, pc, "setLocalDescription", { 0: RTCSessionDescription });
    doneCallback(obj);
  },
};

connectToServer(botExposedApi);
