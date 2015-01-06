/*
 * libjingle
 * Copyright 2014, Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

package org.appspot.apprtc;

import org.appspot.apprtc.AppRTCClient.SignalingParameters;
import org.appspot.apprtc.util.LooperExecutor;

import android.content.Context;
import android.opengl.EGLContext;
import android.util.Log;

import org.webrtc.DataChannel;
import org.webrtc.IceCandidate;
import org.webrtc.MediaConstraints;
import org.webrtc.MediaConstraints.KeyValuePair;
import org.webrtc.MediaStream;
import org.webrtc.MediaStreamTrack;
import org.webrtc.PeerConnection;
import org.webrtc.PeerConnection.IceConnectionState;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.SdpObserver;
import org.webrtc.SessionDescription;
import org.webrtc.StatsObserver;
import org.webrtc.VideoCapturer;
import org.webrtc.VideoRenderer;
import org.webrtc.VideoSource;
import org.webrtc.VideoTrack;

import java.util.LinkedList;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Peer connection client implementation.
 *
 * <p>All public methods are routed to local looper thread.
 * All PeerConnectionEvents callbacks are invoked from the same looper thread.
 */
public class PeerConnectionClient {
  private static final String TAG = "PCRTCClient";
  public static final String VIDEO_TRACK_ID = "ARDAMSv0";
  public static final String AUDIO_TRACK_ID = "ARDAMSa0";

  private final LooperExecutor executor;
  private PeerConnectionFactory factory = null;
  private PeerConnection pc = null;
  private VideoSource videoSource;
  private boolean videoSourceStopped = false;
  private boolean isError = false;
  private final PCObserver pcObserver = new PCObserver();
  private final SDPObserver sdpObserver = new SDPObserver();
  private VideoRenderer.Callbacks localRender;
  private VideoRenderer.Callbacks remoteRender;
  private SignalingParameters signalingParameters;
  // Queued remote ICE candidates are consumed only after both local and
  // remote descriptions are set. Similarly local ICE candidates are sent to
  // remote peer after both local and remote description are set.
  private LinkedList<IceCandidate> queuedRemoteCandidates = null;
  private MediaConstraints sdpMediaConstraints;
  private PeerConnectionEvents events;
  private int startBitrate;
  private boolean isInitiator;
  private boolean useFrontFacingCamera = true;
  private SessionDescription localSdp = null; // either offer or answer SDP
  private MediaStream mediaStream = null;

  /**
   * SDP/ICE ready callbacks.
   */
  public static interface PeerConnectionEvents {
    /**
     * Callback fired once local SDP is created and set.
     */
    public void onLocalDescription(final SessionDescription sdp);

    /**
     * Callback fired once local Ice candidate is generated.
     */
    public void onIceCandidate(final IceCandidate candidate);

    /**
     * Callback fired once connection is established (IceConnectionState is
     * CONNECTED).
     */
    public void onIceConnected();

    /**
     * Callback fired once connection is closed (IceConnectionState is
     * DISCONNECTED).
     */
    public void onIceDisconnected();

    /**
     * Callback fired once peer connection is closed.
     */
    public void onPeerConnectionClosed();

    /**
     * Callback fired once peer connection error happened.
     */
    public void onPeerConnectionError(final String description);

  }

  public PeerConnectionClient() {
    executor = new LooperExecutor();
  }

  public void createPeerConnectionFactory(
      final Context context,
      final boolean vp8HwAcceleration,
      final EGLContext renderEGLContext,
      final PeerConnectionEvents events) {
    this.events = events;
    executor.requestStart();
    executor.execute(new Runnable() {
      @Override
      public void run() {
        createPeerConnectionFactoryInternal(
            context, vp8HwAcceleration, renderEGLContext);
      }
    });
  }

  public void createPeerConnection(
      final VideoRenderer.Callbacks localRender,
      final VideoRenderer.Callbacks remoteRender,
      final SignalingParameters signalingParameters,
      final int startBitrate) {
    this.localRender = localRender;
    this.remoteRender = remoteRender;
    this.signalingParameters = signalingParameters;
    this.startBitrate = startBitrate;
    executor.execute(new Runnable() {
      @Override
      public void run() {
        createPeerConnectionInternal();
      }
    });
  }

  public void close() {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        closeInternal();
      }
    });
    executor.requestStop();
  }

  private void createPeerConnectionFactoryInternal(
      Context context,
      boolean vp8HwAcceleration,
      EGLContext renderEGLContext) {
    Log.d(TAG, "Create peer connection factory.");
    isError = false;
    if (!PeerConnectionFactory.initializeAndroidGlobals(
        context, true, true, vp8HwAcceleration, renderEGLContext)) {
      events.onPeerConnectionError("Failed to initializeAndroidGlobals");
    }
    factory = new PeerConnectionFactory();
    Log.d(TAG, "Peer connection factory created.");
  }

  private void createPeerConnectionInternal() {
    if (factory == null || isError) {
      Log.e(TAG, "Peerconnection factory is not created");
      return;
    }
    Log.d(TAG, "Create peer connection.");
    isInitiator = signalingParameters.initiator;
    queuedRemoteCandidates = new LinkedList<IceCandidate>();

    sdpMediaConstraints = new MediaConstraints();
    sdpMediaConstraints.mandatory.add(new MediaConstraints.KeyValuePair(
        "OfferToReceiveAudio", "true"));
    sdpMediaConstraints.mandatory.add(new MediaConstraints.KeyValuePair(
        "OfferToReceiveVideo", "true"));

    MediaConstraints pcConstraints = signalingParameters.pcConstraints;
    pcConstraints.optional.add(
        new MediaConstraints.KeyValuePair("RtpDataChannels", "true"));
    pc = factory.createPeerConnection(signalingParameters.iceServers,
        pcConstraints, pcObserver);
    isInitiator = false;

    // Uncomment to get ALL WebRTC tracing and SENSITIVE libjingle logging.
    // NOTE: this _must_ happen while |factory| is alive!
    // Logging.enableTracing(
    //     "logcat:",
    //     EnumSet.of(Logging.TraceLevel.TRACE_ALL),
    //     Logging.Severity.LS_SENSITIVE);

    mediaStream = factory.createLocalMediaStream("ARDAMS");
    if (signalingParameters.videoConstraints != null) {
      mediaStream.addTrack(createVideoTrack(useFrontFacingCamera));
    }

    if (signalingParameters.audioConstraints != null) {
      mediaStream.addTrack(factory.createAudioTrack(
          AUDIO_TRACK_ID,
          factory.createAudioSource(signalingParameters.audioConstraints)));
    }
    pc.addStream(mediaStream);
    Log.d(TAG, "Peer connection created.");
  }

  private void closeInternal() {
    Log.d(TAG, "Closing peer connection.");
    if (pc != null) {
      pc.dispose();
      pc = null;
    }
    if (videoSource != null) {
      videoSource.dispose();
      videoSource = null;
    }
    Log.d(TAG, "Closing peer connection factory.");
    if (factory != null) {
      factory.dispose();
      factory = null;
    }
    Log.d(TAG, "Closing peer connection done.");
    events.onPeerConnectionClosed();
  }

  public boolean isHDVideo() {
    if (signalingParameters.videoConstraints == null) {
      return false;
    }
    int minWidth = 0;
    int minHeight = 0;
    for (KeyValuePair keyValuePair :
        signalingParameters.videoConstraints.mandatory) {
      if (keyValuePair.getKey().equals("minWidth")) {
        try {
          minWidth = Integer.parseInt(keyValuePair.getValue());
        } catch (NumberFormatException e) {
          Log.e(TAG, "Can not parse video width from video constraints");
        }
      } else if (keyValuePair.getKey().equals("minHeight")) {
        try {
          minHeight = Integer.parseInt(keyValuePair.getValue());
        } catch (NumberFormatException e) {
          Log.e(TAG, "Can not parse video height from video constraints");
        }
      }
    }
    if (minWidth * minHeight >= 1280 * 720) {
      return true;
    } else {
      return false;
    }
  }

  public boolean getStats(StatsObserver observer, MediaStreamTrack track) {
    if (pc != null && !isError) {
      return pc.getStats(observer, track);
    } else {
      return false;
    }
  }

  public void createOffer() {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (pc != null && !isError) {
          isInitiator = true;
          pc.createOffer(sdpObserver, sdpMediaConstraints);
        }
      }
    });
  }

  public void createAnswer() {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (pc != null && !isError) {
          isInitiator = false;
          pc.createAnswer(sdpObserver, sdpMediaConstraints);
        }
      }
    });
  }

  public void addRemoteIceCandidate(final IceCandidate candidate) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (pc != null && !isError) {
          if (queuedRemoteCandidates != null) {
            queuedRemoteCandidates.add(candidate);
          } else {
            pc.addIceCandidate(candidate);
          }
        }
      }
    });
  }

  public void setRemoteDescription(final SessionDescription sdp) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (pc == null || isError) {
          return;
        }
        String sdpDescription = preferISAC(sdp.description);
        if (startBitrate > 0) {
          sdpDescription = setStartBitrate(sdpDescription, startBitrate);
        }
        Log.d(TAG, "Set remote SDP.");
        SessionDescription sdpRemote = new SessionDescription(
            sdp.type, sdpDescription);
        pc.setRemoteDescription(sdpObserver, sdpRemote);
      }
    });
  }

  public void stopVideoSource() {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (videoSource != null && !videoSourceStopped) {
          Log.d(TAG, "Stop video source.");
          videoSource.stop();
          videoSourceStopped = true;
        }
      }
    });
  }

  public void startVideoSource() {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (videoSource != null && videoSourceStopped) {
          Log.d(TAG, "Restart video source.");
          videoSource.restart();
          videoSourceStopped = false;
        }
      }
    });
  }

  private void reportError(final String errorMessage) {
    Log.e(TAG, "Peerconnection error: " + errorMessage);
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (!isError) {
          events.onPeerConnectionError(errorMessage);
          isError = true;
        }
      }
    });
  }

  // Cycle through likely device names for the camera and return the first
  // capturer that works, or crash if none do.
  private VideoCapturer getVideoCapturer(boolean useFrontFacing) {
    String[] cameraFacing = { "front", "back" };
    if (!useFrontFacing) {
      cameraFacing[0] = "back";
      cameraFacing[1] = "front";
    }
    for (String facing : cameraFacing) {
      int[] cameraIndex = { 0, 1 };
      int[] cameraOrientation = { 0, 90, 180, 270 };
      for (int index : cameraIndex) {
        for (int orientation : cameraOrientation) {
          String name = "Camera " + index + ", Facing " + facing
              + ", Orientation " + orientation;
          VideoCapturer capturer = VideoCapturer.create(name);
          if (capturer != null) {
            Log.d(TAG, "Using camera: " + name);
            return capturer;
          }
        }
      }
    }
    reportError("Failed to open capturer");
    return null;
  }

  private VideoTrack createVideoTrack(boolean frontFacing) {
    VideoCapturer capturer = getVideoCapturer(frontFacing);
    if (videoSource != null) {
      videoSource.stop();
      videoSource.dispose();
    }

    videoSource = factory.createVideoSource(
        capturer, signalingParameters.videoConstraints);
    String trackExtension = frontFacing ? "frontFacing" : "backFacing";
    VideoTrack videoTrack =
        factory.createVideoTrack(VIDEO_TRACK_ID + trackExtension, videoSource);
    videoTrack.addRenderer(new VideoRenderer(localRender));
    return videoTrack;
  }

  private static String setStartBitrate(
      String sdpDescription, int bitrateKbps) {
    String[] lines = sdpDescription.split("\r\n");
    int lineIndex = -1;
    String vp8RtpMap = null;
    Pattern vp8Pattern =
        Pattern.compile("^a=rtpmap:(\\d+) VP8/90000[\r]?$");
    for (int i = 0; i < lines.length; i++) {
      Matcher vp8Matcher = vp8Pattern.matcher(lines[i]);
      if (vp8Matcher.matches()) {
        vp8RtpMap = vp8Matcher.group(1);
        lineIndex = i;
        break;
      }
    }
    if (vp8RtpMap == null) {
      Log.e(TAG, "No rtpmap for VP8 codec");
      return sdpDescription;
    }
    Log.d(TAG, "Found rtpmap " + vp8RtpMap + " at " + lines[lineIndex]);
    StringBuilder newSdpDescription = new StringBuilder();
    for (int i = 0; i < lines.length; i++) {
      newSdpDescription.append(lines[i]).append("\r\n");
      if (i == lineIndex) {
        String bitrateSet = "a=fmtp:" + vp8RtpMap
            + " x-google-start-bitrate=" + bitrateKbps;
        Log.d(TAG, "Add remote SDP line: " + bitrateSet);
        newSdpDescription.append(bitrateSet).append("\r\n");
      }
    }
    return newSdpDescription.toString();
  }

  // Mangle SDP to prefer ISAC/16000 over any other audio codec.
  private static String preferISAC(String sdpDescription) {
    String[] lines = sdpDescription.split("\r\n");
    int mLineIndex = -1;
    String isac16kRtpMap = null;
    Pattern isac16kPattern =
        Pattern.compile("^a=rtpmap:(\\d+) ISAC/16000[\r]?$");
    for (int i = 0;
         (i < lines.length) && (mLineIndex == -1 || isac16kRtpMap == null);
         ++i) {
      if (lines[i].startsWith("m=audio ")) {
        mLineIndex = i;
        continue;
      }
      Matcher isac16kMatcher = isac16kPattern.matcher(lines[i]);
      if (isac16kMatcher.matches()) {
        isac16kRtpMap = isac16kMatcher.group(1);
        continue;
      }
    }
    if (mLineIndex == -1) {
      Log.d(TAG, "No m=audio line, so can't prefer iSAC");
      return sdpDescription;
    }
    if (isac16kRtpMap == null) {
      Log.d(TAG, "No ISAC/16000 line, so can't prefer iSAC");
      return sdpDescription;
    }
    String[] origMLineParts = lines[mLineIndex].split(" ");
    StringBuilder newMLine = new StringBuilder();
    int origPartIndex = 0;
    // Format is: m=<media> <port> <proto> <fmt> ...
    newMLine.append(origMLineParts[origPartIndex++]).append(" ");
    newMLine.append(origMLineParts[origPartIndex++]).append(" ");
    newMLine.append(origMLineParts[origPartIndex++]).append(" ");
    newMLine.append(isac16kRtpMap);
    for (; origPartIndex < origMLineParts.length; ++origPartIndex) {
      if (!origMLineParts[origPartIndex].equals(isac16kRtpMap)) {
        newMLine.append(" ").append(origMLineParts[origPartIndex]);
      }
    }
    lines[mLineIndex] = newMLine.toString();
    StringBuilder newSdpDescription = new StringBuilder();
    for (String line : lines) {
      newSdpDescription.append(line).append("\r\n");
    }
    return newSdpDescription.toString();
  }

  private void drainCandidates() {
    if (queuedRemoteCandidates != null) {
      Log.d(TAG, "Add " + queuedRemoteCandidates.size() + " remote candidates");
      for (IceCandidate candidate : queuedRemoteCandidates) {
        pc.addIceCandidate(candidate);
      }
      queuedRemoteCandidates = null;
    }
  }

  private void switchCameraInternal() {
    if (signalingParameters.videoConstraints == null) {
      return;  // No video is sent.
    }
    if (pc.signalingState() != PeerConnection.SignalingState.STABLE) {
      Log.e(TAG, "Switching camera during negotiation is not handled.");
      return;
    }

    Log.d(TAG, "Switch camera");
    pc.removeStream(mediaStream);
    VideoTrack currentTrack = mediaStream.videoTracks.get(0);
    mediaStream.removeTrack(currentTrack);

    String trackId = currentTrack.id();
    // On Android, there can only be one camera open at the time and we
    // need to release our implicit references to the videoSource before the
    // PeerConnectionFactory is released. Since createVideoTrack creates a new
    // videoSource and frees the old one, we need to release the track here.
    currentTrack.dispose();

    useFrontFacingCamera = !useFrontFacingCamera;
    VideoTrack newTrack = createVideoTrack(useFrontFacingCamera);
    mediaStream.addTrack(newTrack);
    pc.addStream(mediaStream);

    SessionDescription remoteDesc = pc.getRemoteDescription();
    if (localSdp == null || remoteDesc == null) {
      Log.d(TAG, "Switching camera before the negotiation started.");
      return;
    }

    localSdp = new SessionDescription(localSdp.type,
        localSdp.description.replaceAll(trackId, newTrack.id()));

    if (isInitiator) {
      pc.setLocalDescription(new SwitchCameraSdbObserver(), localSdp);
      pc.setRemoteDescription(new SwitchCameraSdbObserver(), remoteDesc);
    } else {
      pc.setRemoteDescription(new SwitchCameraSdbObserver(), remoteDesc);
      pc.setLocalDescription(new SwitchCameraSdbObserver(), localSdp);
    }
    Log.d(TAG, "Switch camera done");
  }

  public void switchCamera() {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (pc != null && !isError) {
          switchCameraInternal();
        }
      }
    });
  }

  // Implementation detail: observe ICE & stream changes and react accordingly.
  private class PCObserver implements PeerConnection.Observer {
    @Override
    public void onIceCandidate(final IceCandidate candidate){
      executor.execute(new Runnable() {
        @Override
        public void run() {
          events.onIceCandidate(candidate);
        }
      });
    }

    @Override
    public void onSignalingChange(
        PeerConnection.SignalingState newState) {
      Log.d(TAG, "SignalingState: " + newState);
    }

    @Override
    public void onIceConnectionChange(
        final PeerConnection.IceConnectionState newState) {
      executor.execute(new Runnable() {
        @Override
        public void run() {
          Log.d(TAG, "IceConnectionState: " + newState);
          if (newState == IceConnectionState.CONNECTED) {
            events.onIceConnected();
          } else if (newState == IceConnectionState.DISCONNECTED) {
            events.onIceDisconnected();
          } else if (newState == IceConnectionState.FAILED) {
            reportError("ICE connection failed.");
          }
        }
      });
    }

    @Override
    public void onIceGatheringChange(
      PeerConnection.IceGatheringState newState) {
      Log.d(TAG, "IceGatheringState: " + newState);
    }

    @Override
    public void onAddStream(final MediaStream stream){
      executor.execute(new Runnable() {
        @Override
        public void run() {
          if (pc == null || isError) {
            return;
          }
          if (stream.audioTracks.size() > 1 || stream.videoTracks.size() > 1) {
            reportError("Weird-looking stream: " + stream);
            return;
          }
          if (stream.videoTracks.size() == 1) {
            stream.videoTracks.get(0).addRenderer(
                new VideoRenderer(remoteRender));
          }
        }
      });
    }

    @Override
    public void onRemoveStream(final MediaStream stream){
      executor.execute(new Runnable() {
        @Override
        public void run() {
          if (pc == null || isError) {
            return;
          }
          stream.videoTracks.get(0).dispose();
        }
      });
    }

    @Override
    public void onDataChannel(final DataChannel dc) {
      reportError("AppRTC doesn't use data channels, but got: " + dc.label()
          + " anyway!");
    }

    @Override
    public void onRenegotiationNeeded() {
      // No need to do anything; AppRTC follows a pre-agreed-upon
      // signaling/negotiation protocol.
    }
  }

  // Implementation detail: handle offer creation/signaling and answer setting,
  // as well as adding remote ICE candidates once the answer SDP is set.
  private class SDPObserver implements SdpObserver {
    @Override
    public void onCreateSuccess(final SessionDescription origSdp) {
      if (localSdp != null) {
        reportError("Multiple SDP create.");
        return;
      }
      final SessionDescription sdp = new SessionDescription(
          origSdp.type, preferISAC(origSdp.description));
      localSdp = sdp;
      executor.execute(new Runnable() {
        @Override
        public void run() {
          if (pc != null && !isError) {
            Log.d(TAG, "Set local SDP from " + sdp.type);
            pc.setLocalDescription(sdpObserver, sdp);
          }
        }
      });
    }

    @Override
    public void onSetSuccess() {
      executor.execute(new Runnable() {
        @Override
        public void run() {
          if (pc == null || isError) {
            return;
          }
          if (isInitiator) {
            // For offering peer connection we first create offer and set
            // local SDP, then after receiving answer set remote SDP.
            if (pc.getRemoteDescription() == null) {
              // We've just set our local SDP so time to send it.
              Log.d(TAG, "Local SDP set succesfully");
              events.onLocalDescription(localSdp);
            } else {
              // We've just set remote description, so drain remote
              // and send local ICE candidates.
              Log.d(TAG, "Remote SDP set succesfully");
              drainCandidates();
            }
          } else {
            // For answering peer connection we set remote SDP and then
            // create answer and set local SDP.
            if (pc.getLocalDescription() != null) {
              // We've just set our local SDP so time to send it, drain
              // remote and send local ICE candidates.
              Log.d(TAG, "Local SDP set succesfully");
              events.onLocalDescription(localSdp);
              drainCandidates();
            } else {
              // We've just set remote SDP - do nothing for now -
              // answer will be created soon.
              Log.d(TAG, "Remote SDP set succesfully");
            }
          }
        }
      });
    }

    @Override
    public void onCreateFailure(final String error) {
      reportError("createSDP error: " + error);
    }

    @Override
    public void onSetFailure(final String error) {
      reportError("setSDP error: " + error);
    }
  }

  private class SwitchCameraSdbObserver implements SdpObserver {
    @Override
    public void onCreateSuccess(SessionDescription sdp) {
    }

    @Override
    public void onSetSuccess() {
      Log.d(TAG, "Camera switch SDP set succesfully");
    }

    @Override
    public void onCreateFailure(final String error) {
    }

    @Override
    public void onSetFailure(final String error) {
      reportError("setSDP error while switching camera: " + error);
    }
  }
}
