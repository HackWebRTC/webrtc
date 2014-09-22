// Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
//
// A unidirectional video and audio flowing test from bot 1 to bot 2.
//
// Note: the source of the video and audio stream is getUserMedia().
//
// TODO(houssainy): get a condition to terminate the test.
//
function testVideoStreaming(bot1, bot2) {
  var pc1 = null;
  var pc2 = null;

  test.wait([
      createPeerConnection.bind(bot1),
      createPeerConnection.bind(bot2) ],
    onPeerConnectionCreated);

  function createPeerConnection(done) {
    this.createPeerConnection(done, test.fail);
  }

  function onPeerConnectionCreated(peer1, peer2) {
    test.log("RTC Peers created.");
    pc1 = peer1;
    pc2 = peer2;
    pc1.addEventListener('addstream', test.fail);
    pc2.addEventListener('addstream', onAddStream);
    pc1.addEventListener('icecandidate', onIceCandidate.bind(pc2));
    pc2.addEventListener('icecandidate', onIceCandidate.bind(pc1));

    bot1.getUserMedia({video:true, audio:true}, onUserMediaSuccess, test.fail);

    function onUserMediaSuccess(stream) {
      test.log("User has granted access to local media.");
      pc1.addStream(stream);
      bot1.showStream(stream.id, true, true);

      createOfferAndAnswer();
    }
  }

  function onAddStream(event) {
    test.log("On Add stream.");
    bot2.showStream(event.stream.id, true, false);
  }

  function onIceCandidate(event) {
    if(event.candidate){
      test.log(event.candidate.candidate);
      this.addIceCandidate(event.candidate,
         onAddIceCandidateSuccess, test.fail);
    };

    function onAddIceCandidateSuccess() {
      test.log("Candidate added successfully");
    };
  }

  function createOfferAndAnswer() {
    test.log("Creating offer.");
    pc1.createOffer(gotOffer, test.fail);

    function gotOffer(offer) {
      test.log("Got offer");
      pc1.setLocalDescription(offer, onSetSessionDescriptionSuccess, test.fail);
      pc2.setRemoteDescription(offer, onSetSessionDescriptionSuccess,
          test.fail);
      test.log("Creating answer");
      pc2.createAnswer(gotAnswer, test.fail);
    }

    function gotAnswer(answer) {
      test.log("Got answer");
      pc2.setLocalDescription(answer, onSetSessionDescriptionSuccess,
          test.fail);
      pc1.setRemoteDescription(answer, onSetSessionDescriptionSuccess,
          test.fail);
    }

    function onSetSessionDescriptionSuccess() {
      test.log("Set session description success.");
    }
  }
}

test.wait( [ test.spawnBot.bind(test, "alice"),
             test.spawnBot.bind(test, "bob") ],
          testVideoStreaming);
