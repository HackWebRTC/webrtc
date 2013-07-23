  var localVideo;
  var miniVideo;
  var remoteVideo;
  var localStream;
  var remoteStream;
  var channel;
  var pc;
  var socket;
  var xmlhttp;
  var started = false;
  var turnDone = false;
  var channelReady = false;
  var signalingReady = false;
  var msgQueue = [];
  // Set up audio and video regardless of what devices are present.
  var sdpConstraints = {'mandatory': {
                          'OfferToReceiveAudio': true,
                          'OfferToReceiveVideo': true }};
  var isVideoMuted = false;
  var isAudioMuted = false;

  function initialize() {
    console.log('Initializing; room=' + roomKey + '.');
    card = document.getElementById('card');
    localVideo = document.getElementById('localVideo');
    // Reset localVideo display to center.
    localVideo.addEventListener('loadedmetadata', function(){
      window.onresize();});
    miniVideo = document.getElementById('miniVideo');
    remoteVideo = document.getElementById('remoteVideo');
    resetStatus();
    // NOTE: AppRTCClient.java searches & parses this line; update there when
    // changing here.
    openChannel();
    maybeRequestTurn();
    doGetUserMedia();
    // Caller is always ready to create peerConnection.
    signalingReady = initiator;
  }

  function openChannel() {
    console.log('Opening channel.');
    var channel = new goog.appengine.Channel(channelToken);
    var handler = {
      'onopen': onChannelOpened,
      'onmessage': onChannelMessage,
      'onerror': onChannelError,
      'onclose': onChannelClosed
    };
    socket = channel.open(handler);
  }

  function maybeRequestTurn() {
    // Skipping TURN Http request for Firefox version <=22.
    // Firefox does not support TURN for version <=22.
    if (webrtcDetectedBrowser === 'firefox' && webrtcDetectedVersion <=22) {
      turnDone = true;
      return;
    }

    for (var i = 0, len = pcConfig.iceServers.length; i < len; i++) {
      if (pcConfig.iceServers[i].url.substr(0, 5) === 'turn:') {
        turnDone = true;
        return;
      }
    }

    var currentDomain = document.domain;
    if (currentDomain.search('localhost') === -1 &&
        currentDomain.search('apprtc') === -1) {
      // Not authorized domain. Try with default STUN instead.
      turnDone = true;
      return;
    }

    // No TURN server. Get one from computeengineondemand.appspot.com.
    xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = onTurnResult;
    xmlhttp.open('GET', turnUrl, true);
    xmlhttp.send();
  }

  function onTurnResult() {
    if (xmlhttp.readyState !== 4)
      return;

    if (xmlhttp.status === 200) {
      var turnServer = JSON.parse(xmlhttp.responseText);
      // Create a turnUri using the polyfill (adapter.js).
      var iceServer = createIceServer(turnServer.uris[0], turnServer.username,
                                      turnServer.password);
      if (iceServer !== null) {
        pcConfig.iceServers.push(iceServer);
      }
    } else {
      console.log('Request for TURN server failed.');
    }
    // If TURN request failed, continue the call with default STUN.
    turnDone = true;
    maybeStart();
  }

  function resetStatus() {
    if (!initiator) {
      setStatus('Waiting for someone to join: \
                <a href=' + roomLink + '>' + roomLink + '</a>');
    } else {
      setStatus('Initializing...');
    }
  }

  function doGetUserMedia() {
    // Call into getUserMedia via the polyfill (adapter.js).
    try {
      getUserMedia(mediaConstraints, onUserMediaSuccess,
                   onUserMediaError);
      console.log('Requested access to local media with mediaConstraints:\n' +
                  '  \'' + JSON.stringify(mediaConstraints) + '\'');
    } catch (e) {
      alert('getUserMedia() failed. Is this a WebRTC capable browser?');
      console.log('getUserMedia failed with exception: ' + e.message);
    }
  }

  function createPeerConnection() {
    // For FF, use Mozilla STUN server.
    if (webrtcDetectedBrowser === "firefox") {
      pcConfig = {"iceServers":[{"url":"stun:stun.services.mozilla.com"}]};
    }
    try {
      // Create an RTCPeerConnection via the polyfill (adapter.js).
      pc = new RTCPeerConnection(pcConfig, pcConstraints);
      pc.onicecandidate = onIceCandidate;
      console.log('Created RTCPeerConnnection with:\n' +
                  '  config: \'' + JSON.stringify(pcConfig) + '\';\n' +
                  '  constraints: \'' + JSON.stringify(pcConstraints) + '\'.');
    } catch (e) {
      console.log('Failed to create PeerConnection, exception: ' + e.message);
      alert('Cannot create RTCPeerConnection object; \
            WebRTC is not supported by this browser.');
        return;
    }
    pc.onaddstream = onRemoteStreamAdded;
    pc.onremovestream = onRemoteStreamRemoved;
  }

  function maybeStart() {
    if (!started && signalingReady &&
        localStream && channelReady && turnDone) {
      setStatus('Connecting...');
      console.log('Creating PeerConnection.');
      createPeerConnection();
      console.log('Adding local stream.');
      pc.addStream(localStream);
      started = true;

      if (initiator)
        doCall();
      else
        calleeStart();
    }
  }

  function setStatus(state) {
    document.getElementById('footer').innerHTML = state;
  }

  function doCall() {
    var constraints = mergeConstraints(offerConstraints, sdpConstraints);
    console.log('Sending offer to peer, with constraints: \n' +
                '  \'' + JSON.stringify(constraints) + '\'.')
    pc.createOffer(setLocalAndSendMessage, null, constraints);
  }

  function calleeStart() {
    // Callee starts to process cached offer and other messages.
    while (msgQueue.length > 0) {
      processSignalingMessage(msgQueue.shift());
    }
  }

  function doAnswer() {
    console.log('Sending answer to peer.');
    pc.createAnswer(setLocalAndSendMessage, null, sdpConstraints);
  }

  function mergeConstraints(cons1, cons2) {
    var merged = cons1;
    for (var name in cons2.mandatory) {
      merged.mandatory[name] = cons2.mandatory[name];
    }
    merged.optional.concat(cons2.optional);
    return merged;
  }

  function setLocalAndSendMessage(sessionDescription) {
    // Set Opus as the preferred codec in SDP if Opus is present.
    sessionDescription.sdp = preferOpus(sessionDescription.sdp);
    pc.setLocalDescription(sessionDescription);
    sendMessage(sessionDescription);
  }

  function sendMessage(message) {
    var msgString = JSON.stringify(message);
    console.log('C->S: ' + msgString);
    // NOTE: AppRTCClient.java searches & parses this line; update there when
    // changing here.
    path = '/message?r=' + roomKey + '&u=' + me;
    var xhr = new XMLHttpRequest();
    xhr.open('POST', path, true);
    xhr.send(msgString);
  }

  function processSignalingMessage(message) {
    if (!started) {
      console.log('peerConnection has not been created yet!');
      return;
    }

    if (message.type === 'offer') {
      // Set Opus in Stereo, if stereo enabled.
      if (stereo)
        message.sdp = addStereo(message.sdp);
      pc.setRemoteDescription(new RTCSessionDescription(message));
      doAnswer();
    } else if (message.type === 'answer') {
      // Set Opus in Stereo, if stereo enabled.
      if (stereo)
        message.sdp = addStereo(message.sdp);
      pc.setRemoteDescription(new RTCSessionDescription(message));
    } else if (message.type === 'candidate') {
      var candidate = new RTCIceCandidate({sdpMLineIndex: message.label,
                                           candidate: message.candidate});
      pc.addIceCandidate(candidate);
    } else if (message.type === 'bye') {
      onRemoteHangup();
    }
  }

  function onChannelOpened() {
    console.log('Channel opened.');
    channelReady = true;
    maybeStart();
  }
  function onChannelMessage(message) {
    console.log('S->C: ' + message.data);
    var msg = JSON.parse(message.data);
    // Since the turn response is async and also GAE might disorder the
    // Message delivery due to possible datastore query at server side,
    // So callee needs to cache messages before peerConnection is created.
    if (!initiator && !started) {
      if (msg.type === 'offer') {
        // Add offer to the beginning of msgQueue, since we can't handle
        // Early candidates before offer at present.
        msgQueue.unshift(msg);
        // Callee creates PeerConnection
        signalingReady = true;
        maybeStart();
      } else {
        msgQueue.push(msg);
      }
    } else {
      processSignalingMessage(msg);
    }
  }
  function onChannelError() {
    console.log('Channel error.');
  }
  function onChannelClosed() {
    console.log('Channel closed.');
  }

  function onUserMediaSuccess(stream) {
    console.log('User has granted access to local media.');
    // Call the polyfill wrapper to attach the media stream to this element.
    attachMediaStream(localVideo, stream);
    localVideo.style.opacity = 1;
    localStream = stream;
    // Caller creates PeerConnection.
    maybeStart();
  }

  function onUserMediaError(error) {
    console.log('Failed to get access to local media. Error code was ' +
                error.code);
    alert('Failed to get access to local media. Error code was ' +
          error.code + '.');
  }

  function onIceCandidate(event) {
    if (event.candidate) {
      sendMessage({type: 'candidate',
                   label: event.candidate.sdpMLineIndex,
                   id: event.candidate.sdpMid,
                   candidate: event.candidate.candidate});
    } else {
      console.log('End of candidates.');
    }
  }

  function onRemoteStreamAdded(event) {
    console.log('Remote stream added.');
    reattachMediaStream(miniVideo, localVideo);
    attachMediaStream(remoteVideo, event.stream);
    remoteStream = event.stream;
    waitForRemoteVideo();
  }

  function onRemoteStreamRemoved(event) {
    console.log('Remote stream removed.');
  }

  function onHangup() {
    console.log('Hanging up.');
    transitionToDone();
    stop();
    // will trigger BYE from server
    socket.close();
  }

  function onRemoteHangup() {
    console.log('Session terminated.');
    initiator = 0;
    transitionToWaiting();
    stop();
  }

  function stop() {
    started = false;
    signalingReady = false;
    isAudioMuted = false;
    isVideoMuted = false;
    pc.close();
    pc = null;
    msgQueue.length = 0;
  }

  function waitForRemoteVideo() {
    // Call the getVideoTracks method via adapter.js.
    videoTracks = remoteStream.getVideoTracks();
    if (videoTracks.length === 0 || remoteVideo.currentTime > 0) {
      transitionToActive();
    } else {
      setTimeout(waitForRemoteVideo, 100);
    }
  }

  function transitionToActive() {
    remoteVideo.style.opacity = 1;
    card.style.webkitTransform = 'rotateY(180deg)';
    setTimeout(function() { localVideo.src = ''; }, 500);
    setTimeout(function() { miniVideo.style.opacity = 1; }, 1000);
    // Reset window display according to the asperio of remote video.
    window.onresize();
    setStatus('<input type=\'button\' id=\'hangup\' value=\'Hang up\' \
              onclick=\'onHangup()\' />');
  }

  function transitionToWaiting() {
    card.style.webkitTransform = 'rotateY(0deg)';
    setTimeout(function() {
                 localVideo.src = miniVideo.src;
                 miniVideo.src = '';
                 remoteVideo.src = '' }, 500);
    miniVideo.style.opacity = 0;
    remoteVideo.style.opacity = 0;
    resetStatus();
  }

  function transitionToDone() {
    localVideo.style.opacity = 0;
    remoteVideo.style.opacity = 0;
    miniVideo.style.opacity = 0;
    setStatus('You have left the call. <a href=' + roomLink + '>\
              Click here</a> to rejoin.');
  }

  function enterFullScreen() {
    container.webkitRequestFullScreen();
  }

  function toggleVideoMute() {
    // Call the getVideoTracks method via adapter.js.
    videoTracks = localStream.getVideoTracks();

    if (videoTracks.length === 0) {
      console.log('No local video available.');
      return;
    }

    if (isVideoMuted) {
      for (i = 0; i < videoTracks.length; i++) {
        videoTracks[i].enabled = true;
      }
      console.log('Video unmuted.');
    } else {
      for (i = 0; i < videoTracks.length; i++) {
        videoTracks[i].enabled = false;
      }
      console.log('Video muted.');
    }

    isVideoMuted = !isVideoMuted;
  }

  function toggleAudioMute() {
    // Call the getAudioTracks method via adapter.js.
    audioTracks = localStream.getAudioTracks();

    if (audioTracks.length === 0) {
      console.log('No local audio available.');
      return;
    }

    if (isAudioMuted) {
      for (i = 0; i < audioTracks.length; i++) {
        audioTracks[i].enabled = true;
      }
      console.log('Audio unmuted.');
    } else {
      for (i = 0; i < audioTracks.length; i++){
        audioTracks[i].enabled = false;
      }
      console.log('Audio muted.');
    }

    isAudioMuted = !isAudioMuted;
  }

  // Ctrl-D: toggle audio mute; Ctrl-E: toggle video mute.
  // On Mac, Command key is instead of Ctrl.
  // Return false to screen out original Chrome shortcuts.
  document.onkeydown = function() {
    if (navigator.appVersion.indexOf('Mac') != -1) {
      if (event.metaKey && event.keyCode === 68) {
        toggleAudioMute();
        return false;
      }
      if (event.metaKey && event.keyCode === 69) {
        toggleVideoMute();
        return false;
      }
    } else {
      if (event.ctrlKey && event.keyCode === 68) {
        toggleAudioMute();
        return false;
      }
      if (event.ctrlKey && event.keyCode === 69) {
        toggleVideoMute();
        return false;
      }
    }
  }

  // Set Opus as the default audio codec if it's present.
  function preferOpus(sdp) {
    var sdpLines = sdp.split('\r\n');

    // Search for m line.
    for (var i = 0; i < sdpLines.length; i++) {
        if (sdpLines[i].search('m=audio') !== -1) {
          var mLineIndex = i;
          break;
        }
    }
    if (mLineIndex === null)
      return sdp;

    // If Opus is available, set it as the default in m line.
    for (var i = 0; i < sdpLines.length; i++) {
      if (sdpLines[i].search('opus/48000') !== -1) {
        var opusPayload = extractSdp(sdpLines[i], /:(\d+) opus\/48000/i);
        if (opusPayload)
          sdpLines[mLineIndex] = setDefaultCodec(sdpLines[mLineIndex],
                                                 opusPayload);
        break;
      }
    }

    // Remove CN in m line and sdp.
    sdpLines = removeCN(sdpLines, mLineIndex);

    sdp = sdpLines.join('\r\n');
    return sdp;
  }

  // Set Opus in stereo if stereo is enabled.
  function addStereo(sdp) {
    var sdpLines = sdp.split('\r\n');

    // Find opus payload.
    for (var i = 0; i < sdpLines.length; i++) {
      if (sdpLines[i].search('opus/48000') !== -1) {
        var opusPayload = extractSdp(sdpLines[i], /:(\d+) opus\/48000/i);
        break;
      }
    }

    // Find the payload in fmtp line.
    for (var i = 0; i < sdpLines.length; i++) {
      if (sdpLines[i].search('a=fmtp') !== -1) {
        var payload = extractSdp(sdpLines[i], /a=fmtp:(\d+)/ );
        if (payload === opusPayload) {
          var fmtpLineIndex = i;
          break;
        }
      }
    }
    // No fmtp line found.
    if (fmtpLineIndex === null)
      return sdp;

    // Append stereo=1 to fmtp line.
    sdpLines[fmtpLineIndex] = sdpLines[fmtpLineIndex].concat(' stereo=1');

    sdp = sdpLines.join('\r\n');
    return sdp;
  }

  function extractSdp(sdpLine, pattern) {
    var result = sdpLine.match(pattern);
    return (result && result.length == 2)? result[1]: null;
  }

  // Set the selected codec to the first in m line.
  function setDefaultCodec(mLine, payload) {
    var elements = mLine.split(' ');
    var newLine = new Array();
    var index = 0;
    for (var i = 0; i < elements.length; i++) {
      if (index === 3) // Format of media starts from the fourth.
        newLine[index++] = payload; // Put target payload to the first.
      if (elements[i] !== payload)
        newLine[index++] = elements[i];
    }
    return newLine.join(' ');
  }

  // Strip CN from sdp before CN constraints is ready.
  function removeCN(sdpLines, mLineIndex) {
    var mLineElements = sdpLines[mLineIndex].split(' ');
    // Scan from end for the convenience of removing an item.
    for (var i = sdpLines.length-1; i >= 0; i--) {
      var payload = extractSdp(sdpLines[i], /a=rtpmap:(\d+) CN\/\d+/i);
      if (payload) {
        var cnPos = mLineElements.indexOf(payload);
        if (cnPos !== -1) {
          // Remove CN payload from m line.
          mLineElements.splice(cnPos, 1);
        }
        // Remove CN line in sdp
        sdpLines.splice(i, 1);
      }
    }

    sdpLines[mLineIndex] = mLineElements.join(' ');
    return sdpLines;
  }

  // Send BYE on refreshing(or leaving) a demo page
  // to ensure the room is cleaned for next session.
  window.onbeforeunload = function() {
    sendMessage({type: 'bye'});
  }

  // Set the video diplaying in the center of window.
  window.onresize = function(){
    var aspectRatio;
    if (remoteVideo.style.opacity === '1') {
      aspectRatio = remoteVideo.videoWidth/remoteVideo.videoHeight;
    } else if (localVideo.style.opacity === '1') {
      aspectRatio = localVideo.videoWidth/localVideo.videoHeight;
    } else {
      return;
    }

    var innerHeight = this.innerHeight;
    var innerWidth = this.innerWidth;
    var videoWidth = innerWidth < aspectRatio * window.innerHeight ?
                     innerWidth : aspectRatio * window.innerHeight;
    var videoHeight = innerHeight < window.innerWidth / aspectRatio ?
                      innerHeight : window.innerWidth / aspectRatio;
    containerDiv = document.getElementById('container');
    containerDiv.style.width = videoWidth + 'px';
    containerDiv.style.height = videoHeight + 'px';
    containerDiv.style.left = (innerWidth - videoWidth) / 2 + 'px';
    containerDiv.style.top = (innerHeight - videoHeight) / 2 + 'px';
  };
