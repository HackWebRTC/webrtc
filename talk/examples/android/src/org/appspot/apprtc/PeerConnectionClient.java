/*
 * libjingle
 * Copyright 2014 Google Inc.
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

import android.content.Context;
import android.opengl.EGLContext;
import android.util.Log;

import org.appspot.apprtc.AppRTCClient.SignalingParameters;
import org.appspot.apprtc.util.LooperExecutor;
import org.webrtc.DataChannel;
import org.webrtc.IceCandidate;
import org.webrtc.MediaCodecVideoEncoder;
import org.webrtc.MediaConstraints;
import org.webrtc.MediaConstraints.KeyValuePair;
import org.webrtc.MediaStream;
import org.webrtc.PeerConnection;
import org.webrtc.PeerConnection.IceConnectionState;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.SdpObserver;
import org.webrtc.SessionDescription;
import org.webrtc.StatsObserver;
import org.webrtc.StatsReport;
import org.webrtc.VideoCapturerAndroid;
import org.webrtc.VideoRenderer;
import org.webrtc.VideoSource;
import org.webrtc.VideoTrack;

import java.util.LinkedList;
import java.util.Timer;
import java.util.TimerTask;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Peer connection client implementation.
 *
 * <p>All public methods are routed to local looper thread.
 * All PeerConnectionEvents callbacks are invoked from the same looper thread.
 */
public class PeerConnectionClient {
  public static final String VIDEO_TRACK_ID = "ARDAMSv0";
  public static final String AUDIO_TRACK_ID = "ARDAMSa0";
  private static final String TAG = "PCRTCClient";
  private static final boolean PREFER_ISAC = false;
  public static final String VIDEO_CODEC_VP8 = "VP8";
  public static final String VIDEO_CODEC_VP9 = "VP9";
  private static final String FIELD_TRIAL_VP9 = "WebRTC-SupportVP9/Enabled/";
  private static final String MAX_VIDEO_WIDTH_CONSTRAINT = "maxWidth";
  private static final String MIN_VIDEO_WIDTH_CONSTRAINT = "minWidth";
  private static final String MAX_VIDEO_HEIGHT_CONSTRAINT = "maxHeight";
  private static final String MIN_VIDEO_HEIGHT_CONSTRAINT = "minHeight";
  private static final String MAX_VIDEO_FPS_CONSTRAINT = "maxFrameRate";
  private static final String MIN_VIDEO_FPS_CONSTRAINT = "minFrameRate";
  private static final int HD_VIDEO_WIDTH = 1280;
  private static final int HD_VIDEO_HEIGHT = 720;
  private static final int MAX_VIDEO_WIDTH = 1280;
  private static final int MAX_VIDEO_HEIGHT = 1280;
  private static final int MAX_VIDEO_FPS = 30;

  private final LooperExecutor executor;
  private PeerConnectionFactory factory = null;
  private PeerConnection peerConnection = null;
  private VideoSource videoSource;
  private boolean videoSourceStopped = false;
  private boolean isError = false;
  private boolean videoCodecHwAcceleration;
  private final Timer statsTimer = new Timer();
  private final PCObserver pcObserver = new PCObserver();
  private final SDPObserver sdpObserver = new SDPObserver();
  private VideoRenderer.Callbacks localRender;
  private VideoRenderer.Callbacks remoteRender;
  private SignalingParameters signalingParameters;
  private MediaConstraints videoConstraints;
  private PeerConnectionParameters peerConnectionParameters;
  // Queued remote ICE candidates are consumed only after both local and
  // remote descriptions are set. Similarly local ICE candidates are sent to
  // remote peer after both local and remote description are set.
  private LinkedList<IceCandidate> queuedRemoteCandidates = null;
  private MediaConstraints sdpMediaConstraints;
  private PeerConnectionEvents events;
  private boolean isInitiator;
  private SessionDescription localSdp = null; // either offer or answer SDP
  private MediaStream mediaStream = null;
  private VideoCapturerAndroid videoCapturer = null;
  // enableVideo is set to true if video should be rendered and sent.
  private boolean renderVideo = true;
  private VideoTrack localVideoTrack = null;
  private VideoTrack remoteVideoTrack = null;

  /**
   * Peer connection parameters.
   */
  public static class PeerConnectionParameters {
    public final int videoWidth;
    public final int videoHeight;
    public final int videoFps;
    public final int videoStartBitrate;
    public final boolean cpuOveruseDetection;

    public PeerConnectionParameters(int videoWidth, int videoHeight,
        int videoFps, int videoStartBitrate, boolean cpuOveruseDetection) {
      this.videoWidth = videoWidth;
      this.videoHeight = videoHeight;
      this.videoFps = videoFps;
      this.videoStartBitrate = videoStartBitrate;
      this.cpuOveruseDetection = cpuOveruseDetection;
    }
  }

  /**
   * Peer connection events.
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
     * Callback fired once peer connection statistics is ready.
     */
    public void onPeerConnectionStatsReady(final StatsReport[] reports);

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
      final String videoCodec,
      final boolean videoCodecHwAcceleration,
      final EGLContext renderEGLContext,
      final PeerConnectionEvents events) {
    this.events = events;
    this.videoCodecHwAcceleration = videoCodecHwAcceleration;
    executor.requestStart();
    executor.execute(new Runnable() {
      @Override
      public void run() {
        createPeerConnectionFactoryInternal(
            context, videoCodec, renderEGLContext);
      }
    });
  }

  public void createPeerConnection(
      final VideoRenderer.Callbacks localRender,
      final VideoRenderer.Callbacks remoteRender,
      final SignalingParameters signalingParameters,
      final PeerConnectionParameters peerConnectionParameters) {
    this.localRender = localRender;
    this.remoteRender = remoteRender;
    this.signalingParameters = signalingParameters;
    this.peerConnectionParameters = peerConnectionParameters;
    // Merge video constraints from signaling parameters and peer connection
    // parameters.
    videoConstraints = signalingParameters.videoConstraints;
    if (videoConstraints != null && peerConnectionParameters != null) {
      int videoWidth = peerConnectionParameters.videoWidth;
      int videoHeight = peerConnectionParameters.videoHeight;

      // If HW video encoder is supported and video resolution is not
      // specified force it to HD.
      if ((videoWidth == 0 || videoHeight == 0) && videoCodecHwAcceleration &&
          MediaCodecVideoEncoder.isPlatformSupported()) {
        videoWidth = HD_VIDEO_WIDTH;
        videoHeight = HD_VIDEO_HEIGHT;
      }

      // Add video resolution constraints.
      if (videoWidth > 0 && videoHeight > 0) {
        videoWidth = Math.min(videoWidth, MAX_VIDEO_WIDTH);
        videoHeight = Math.min(videoHeight, MAX_VIDEO_HEIGHT);
        videoConstraints.mandatory.add(new KeyValuePair(
            MIN_VIDEO_WIDTH_CONSTRAINT, Integer.toString(videoWidth)));
        videoConstraints.mandatory.add(new KeyValuePair(
            MAX_VIDEO_WIDTH_CONSTRAINT, Integer.toString(videoWidth)));
        videoConstraints.mandatory.add(new KeyValuePair(
            MIN_VIDEO_HEIGHT_CONSTRAINT, Integer.toString(videoHeight)));
        videoConstraints.mandatory.add(new KeyValuePair(
            MAX_VIDEO_HEIGHT_CONSTRAINT, Integer.toString(videoHeight)));
      }

      // Add fps constraints.
      int videoFps = peerConnectionParameters.videoFps;
      if (videoFps > 0) {
        videoFps = Math.min(videoFps, MAX_VIDEO_FPS);
        videoConstraints.mandatory.add(new KeyValuePair(
            MIN_VIDEO_FPS_CONSTRAINT, Integer.toString(videoFps)));
        videoConstraints.mandatory.add(new KeyValuePair(
            MAX_VIDEO_FPS_CONSTRAINT, Integer.toString(videoFps)));
      }
    }

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
      String videoCodec,
      EGLContext renderEGLContext) {
    Log.d(TAG, "Create peer connection factory with EGLContext "
      + renderEGLContext);
    isError = false;
    if (videoCodec.equals(VIDEO_CODEC_VP9)) {
      PeerConnectionFactory.initializeFieldTrials(FIELD_TRIAL_VP9);
    } else {
      PeerConnectionFactory.initializeFieldTrials(null);
    }
    if (!PeerConnectionFactory.initializeAndroidGlobals(
        context, true, true, videoCodecHwAcceleration, renderEGLContext)) {
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
    Log.d(TAG, "Create peer connection. VideoConstraints: "
        + videoConstraints.toString());
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
    peerConnection = factory.createPeerConnection(
        signalingParameters.iceServers, pcConstraints, pcObserver);
    isInitiator = false;

    // Uncomment to get ALL WebRTC tracing and SENSITIVE libjingle logging.
    // NOTE: this _must_ happen while |factory| is alive!
    // Logging.enableTracing(
    //     "logcat:",
    //     EnumSet.of(Logging.TraceLevel.TRACE_ALL),
    //     Logging.Severity.LS_SENSITIVE);

    mediaStream = factory.createLocalMediaStream("ARDAMS");
    if (videoConstraints != null) {
      videoCapturer = VideoCapturerAndroid.create(
          VideoCapturerAndroid.getNameOfFrontFacingDevice());
      mediaStream.addTrack(createVideoTrack(videoCapturer));
    }

    if (signalingParameters.audioConstraints != null) {
      mediaStream.addTrack(factory.createAudioTrack(
          AUDIO_TRACK_ID,
          factory.createAudioSource(signalingParameters.audioConstraints)));
    }
    peerConnection.addStream(mediaStream);

    Log.d(TAG, "Peer connection created.");
  }

  private void closeInternal() {
    Log.d(TAG, "Closing peer connection.");
    statsTimer.cancel();
    if (peerConnection != null) {
      peerConnection.dispose();
      peerConnection = null;
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
    if (videoConstraints == null) {
      return false;
    }
    int minWidth = 0;
    int minHeight = 0;
    for (KeyValuePair keyValuePair : videoConstraints.mandatory) {
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

  private void getStats() {
    if (peerConnection == null || isError) {
      return;
    }
    boolean success = peerConnection.getStats(new StatsObserver() {
      @Override
      public void onComplete(final StatsReport[] reports) {
        events.onPeerConnectionStatsReady(reports);
      }
    }, null);
    if (!success) {
      Log.e(TAG, "getStats() returns false!");
    }
  }

  public void enableStatsEvents(boolean enable, int periodMs) {
    if (enable) {
      statsTimer.schedule(new TimerTask() {
        @Override
        public void run() {
          executor.execute(new Runnable() {
            @Override
            public void run() {
              getStats();
            }
          });
        }
      }, 0, periodMs);
    } else {
      statsTimer.cancel();
    }
  }

  public void setVideoEnabled(final boolean enable) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        renderVideo = enable;
        if (localVideoTrack != null) {
          localVideoTrack.setEnabled(renderVideo);
        }
        if (remoteVideoTrack != null) {
          remoteVideoTrack.setEnabled(renderVideo);
        }
      }
    });
  }

  public void createOffer() {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (peerConnection != null && !isError) {
          Log.d(TAG, "PC Create OFFER");
          isInitiator = true;
          peerConnection.createOffer(sdpObserver, sdpMediaConstraints);
        }
      }
    });
  }

  public void createAnswer() {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (peerConnection != null && !isError) {
          Log.d(TAG, "PC create ANSWER");
          isInitiator = false;
          peerConnection.createAnswer(sdpObserver, sdpMediaConstraints);
        }
      }
    });
  }

  public void addRemoteIceCandidate(final IceCandidate candidate) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (peerConnection != null && !isError) {
          if (queuedRemoteCandidates != null) {
            queuedRemoteCandidates.add(candidate);
          } else {
            peerConnection.addIceCandidate(candidate);
          }
        }
      }
    });
  }

  public void setRemoteDescription(final SessionDescription sdp) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (peerConnection == null || isError) {
          return;
        }
        String sdpDescription = sdp.description;
        if (PREFER_ISAC) {
          sdpDescription = preferISAC(sdpDescription);
        }
        if (peerConnectionParameters.videoStartBitrate > 0) {
          sdpDescription = setStartBitrate(VIDEO_CODEC_VP8,
              sdpDescription, peerConnectionParameters.videoStartBitrate);
          sdpDescription = setStartBitrate(VIDEO_CODEC_VP9,
              sdpDescription, peerConnectionParameters.videoStartBitrate);
        }
        Log.d(TAG, "Set remote SDP.");
        SessionDescription sdpRemote = new SessionDescription(
            sdp.type, sdpDescription);
        peerConnection.setRemoteDescription(sdpObserver, sdpRemote);
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

  private VideoTrack createVideoTrack(VideoCapturerAndroid capturer) {
    videoSource = factory.createVideoSource(
        capturer, signalingParameters.videoConstraints);

    localVideoTrack =
        factory.createVideoTrack(VIDEO_TRACK_ID, videoSource);
    localVideoTrack.setEnabled(renderVideo);
    localVideoTrack.addRenderer(new VideoRenderer(localRender));
    return localVideoTrack;
  }

  // Mangle SDP to add video start bitrate.
  private static String setStartBitrate(String codec,
      String sdpDescription, int bitrateKbps) {
    String[] lines = sdpDescription.split("\r\n");
    int lineIndex = -1;
    String codecRtpMap = null;
    // a=rtpmap:<payload type> <encoding name>/<clock rate> [/<encoding parameters>]
    String regex = "^a=rtpmap:(\\d+) " + codec + "(/\\d+)+[\r]?$";
    Pattern codecPattern = Pattern.compile(regex);
    for (int i = 0; i < lines.length; i++) {
      Matcher codecMatcher = codecPattern.matcher(lines[i]);
      if (codecMatcher.matches()) {
        codecRtpMap = codecMatcher.group(1);
        lineIndex = i;
        break;
      }
    }
    if (codecRtpMap == null) {
      Log.w(TAG, "No rtpmap for " + codec + " codec");
      return sdpDescription;
    }
    Log.d(TAG, "Found " +  codec + " rtpmap " + codecRtpMap
        + " at " + lines[lineIndex]);
    StringBuilder newSdpDescription = new StringBuilder();
    for (int i = 0; i < lines.length; i++) {
      newSdpDescription.append(lines[i]).append("\r\n");
      if (i == lineIndex) {
        String bitrateSet = "a=fmtp:" + codecRtpMap
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
        peerConnection.addIceCandidate(candidate);
      }
      queuedRemoteCandidates = null;
    }
  }

  private void switchCameraInternal() {
    if (videoConstraints == null) {
      return;  // No video is sent.
    }
    Log.d(TAG, "Switch camera");
    videoCapturer.switchCamera();
  }

  public void switchCamera() {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (peerConnection != null && !isError) {
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
          if (peerConnection == null || isError) {
            return;
          }
          if (stream.audioTracks.size() > 1 || stream.videoTracks.size() > 1) {
            reportError("Weird-looking stream: " + stream);
            return;
          }
          if (stream.videoTracks.size() == 1) {
            remoteVideoTrack = stream.videoTracks.get(0);
            remoteVideoTrack.setEnabled(renderVideo);
            remoteVideoTrack.addRenderer(new VideoRenderer(remoteRender));
          }
        }
      });
    }

    @Override
    public void onRemoveStream(final MediaStream stream){
      executor.execute(new Runnable() {
        @Override
        public void run() {
          if (peerConnection == null || isError) {
            return;
          }
          remoteVideoTrack = null;
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
      String sdpDescription = origSdp.description;
      if (PREFER_ISAC) {
        sdpDescription = preferISAC(sdpDescription);
      }
      final SessionDescription sdp = new SessionDescription(
          origSdp.type, sdpDescription);
      localSdp = sdp;
      executor.execute(new Runnable() {
        @Override
        public void run() {
          if (peerConnection != null && !isError) {
            Log.d(TAG, "Set local SDP from " + sdp.type);
            peerConnection.setLocalDescription(sdpObserver, sdp);
          }
        }
      });
    }

    @Override
    public void onSetSuccess() {
      executor.execute(new Runnable() {
        @Override
        public void run() {
          if (peerConnection == null || isError) {
            return;
          }
          if (isInitiator) {
            // For offering peer connection we first create offer and set
            // local SDP, then after receiving answer set remote SDP.
            if (peerConnection.getRemoteDescription() == null) {
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
            if (peerConnection.getLocalDescription() != null) {
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
}
