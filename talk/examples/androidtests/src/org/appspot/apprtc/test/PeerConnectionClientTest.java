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

package org.appspot.apprtc.test;

import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

import org.appspot.apprtc.AppRTCClient.SignalingParameters;
import org.appspot.apprtc.PeerConnectionClient;
import org.appspot.apprtc.PeerConnectionClient.PeerConnectionEvents;
import org.appspot.apprtc.PeerConnectionClient.PeerConnectionParameters;
import org.appspot.apprtc.util.LooperExecutor;
import org.webrtc.IceCandidate;
import org.webrtc.MediaConstraints;
import org.webrtc.PeerConnection;
import org.webrtc.SessionDescription;
import org.webrtc.StatsReport;
import org.webrtc.VideoRenderer;

import android.test.InstrumentationTestCase;
import android.util.Log;

public class PeerConnectionClientTest extends InstrumentationTestCase
    implements PeerConnectionEvents {
  private static final String TAG = "RTCClientTest";
  private static final int ICE_CONNECTION_WAIT_TIMEOUT = 10000;
  private static final int WAIT_TIMEOUT = 7000;
  private static final int CAMERA_SWITCH_ATTEMPTS = 3;
  private static final int VIDEO_RESTART_ATTEMPTS = 3;
  private static final int VIDEO_RESTART_TIMEOUT = 500;
  private static final int EXPECTED_VIDEO_FRAMES = 10;
  private static final String VIDEO_CODEC_VP8 = "VP8";
  private static final String VIDEO_CODEC_VP9 = "VP9";
  private static final String VIDEO_CODEC_H264 = "H264";
  private static final int AUDIO_RUN_TIMEOUT = 1000;
  private static final String DTLS_SRTP_KEY_AGREEMENT_CONSTRAINT = "DtlsSrtpKeyAgreement";

  // The peer connection client is assumed to be thread safe in itself; the
  // reference is written by the test thread and read by worker threads.
  private volatile PeerConnectionClient pcClient;
  private volatile boolean loopback;

  // These are protected by their respective event objects.
  private LooperExecutor signalingExecutor;
  private boolean isClosed;
  private boolean isIceConnected;
  private SessionDescription localSdp;
  private List<IceCandidate> iceCandidates = new LinkedList<IceCandidate>();
  private final Object localSdpEvent = new Object();
  private final Object iceCandidateEvent = new Object();
  private final Object iceConnectedEvent = new Object();
  private final Object closeEvent = new Object();

  // Mock renderer implementation.
  private static class MockRenderer implements VideoRenderer.Callbacks {
    // These are protected by 'this' since we gets called from worker threads.
    private int width = -1;
    private int height = -1;
    private boolean renderFrameCalled = false;
    private boolean setSizeCalledBeforeRenderFrame = false;

    // Thread-safe in itself.
    private CountDownLatch doneRendering;

    public MockRenderer(int expectedFrames) {
      reset(expectedFrames);
    }

    // Resets render to wait for new amount of video frames.
    public synchronized void reset(int expectedFrames) {
      doneRendering = new CountDownLatch(expectedFrames);
    }

    @Override
    public synchronized void setSize(int width, int height) {
      Log.d(TAG, "Set size: " + width + " x " + height);
      this.width = width;
      this.height = height;
      if (!renderFrameCalled) {
        setSizeCalledBeforeRenderFrame = true;
      }
    }

    @Override
    public synchronized void renderFrame(VideoRenderer.I420Frame frame) {
      renderFrameCalled = true;
      doneRendering.countDown();
    }

    public synchronized int getWidth() { return width; }
    public synchronized int getHeight() { return height; }
    public synchronized boolean setSizeCalledBeforeRenderFrame() {
      return setSizeCalledBeforeRenderFrame;
    }

    // This method shouldn't hold any locks or touch member variables since it
    // blocks.
    public boolean waitForFramesRendered(int timeoutMs)
        throws InterruptedException {
      doneRendering.await(timeoutMs, TimeUnit.MILLISECONDS);
      return (doneRendering.getCount() <= 0);
    }
  }

  // Peer connection events implementation.
  @Override
  public void onLocalDescription(SessionDescription sdp) {
    Log.d(TAG, "LocalSDP type: " + sdp.type);
    synchronized (localSdpEvent) {
      localSdp = sdp;
      localSdpEvent.notifyAll();
    }
  }

  @Override
  public void onIceCandidate(final IceCandidate candidate) {
    synchronized(iceCandidateEvent) {
      Log.d(TAG, "IceCandidate #" + iceCandidates.size() + " : " + candidate.toString());
      if (loopback) {
        // Loopback local ICE candidate in a separate thread to avoid adding
        // remote ICE candidate in a local ICE candidate callback.
        signalingExecutor.execute(new Runnable() {
          @Override
          public void run() {
            pcClient.addRemoteIceCandidate(candidate);
          }
        });
      }
      iceCandidates.add(candidate);
      iceCandidateEvent.notifyAll();
    }
  }

  @Override
  public void onIceConnected() {
    Log.d(TAG, "ICE Connected");
    synchronized(iceConnectedEvent) {
      isIceConnected = true;
      iceConnectedEvent.notifyAll();
    }
  }

  @Override
  public void onIceDisconnected() {
    Log.d(TAG, "ICE Disconnected");
    synchronized(iceConnectedEvent) {
      isIceConnected = false;
      iceConnectedEvent.notifyAll();
    }
  }

  @Override
  public void onPeerConnectionClosed() {
    Log.d(TAG, "PeerConnection closed");
    synchronized(closeEvent) {
      isClosed = true;
      closeEvent.notifyAll();
    }
  }

  @Override
  public void onPeerConnectionError(String description) {
    fail("PC Error: " + description);
  }

  @Override
  public void onPeerConnectionStatsReady(StatsReport[] reports) {
  }

  // Helper wait functions.
  private boolean waitForLocalSDP(int timeoutMs)
      throws InterruptedException {
    synchronized(localSdpEvent) {
      if (localSdp == null) {
        localSdpEvent.wait(timeoutMs);
      }
      return (localSdp != null);
    }
  }

  private boolean waitForIceCandidates(int timeoutMs)
      throws InterruptedException {
    synchronized(iceCandidateEvent) {
      if (iceCandidates.size() == 0) {
        iceCandidateEvent.wait(timeoutMs);
      }
      return (iceCandidates.size() > 0);
    }
  }

  private boolean waitForIceConnected(int timeoutMs)
      throws InterruptedException {
    synchronized(iceConnectedEvent) {
      if (!isIceConnected) {
        iceConnectedEvent.wait(timeoutMs);
      }
      if (!isIceConnected) {
        Log.e(TAG, "ICE connection failure");
      }

      return isIceConnected;
    }
  }

  private boolean waitForPeerConnectionClosed(int timeoutMs)
      throws InterruptedException {
    synchronized(closeEvent) {
      if (!isClosed) {
        closeEvent.wait(timeoutMs);
      }
      return isClosed;
    }
  }

  private SignalingParameters getTestSignalingParameters() {
    List<PeerConnection.IceServer> iceServers =
        new LinkedList<PeerConnection.IceServer>();
    MediaConstraints pcConstraints = new MediaConstraints();
    pcConstraints.optional.add(
        new MediaConstraints.KeyValuePair(DTLS_SRTP_KEY_AGREEMENT_CONSTRAINT, "false"));
    MediaConstraints videoConstraints = new MediaConstraints();
    MediaConstraints audioConstraints = new MediaConstraints();
    SignalingParameters signalingParameters = new SignalingParameters(
        iceServers, true,
        pcConstraints, videoConstraints, audioConstraints,
        null, null, null,
        null, null);
    return signalingParameters;
  }

  PeerConnectionClient createPeerConnectionClient(
      MockRenderer localRenderer, MockRenderer remoteRenderer,
      boolean enableVideo, String videoCodec) {
    SignalingParameters signalingParameters = getTestSignalingParameters();
    PeerConnectionParameters peerConnectionParameters =
        new PeerConnectionParameters(
            enableVideo, true, // videoCallEnabled, loopback.
            0, 0, 0, 0, videoCodec, true, // video codec parameters.
            0, "OPUS", true); // audio codec parameters.

    PeerConnectionClient client = new PeerConnectionClient();
    client.createPeerConnectionFactory(
        getInstrumentation().getContext(), null,
        peerConnectionParameters, this);
    client.createPeerConnection(
        localRenderer, remoteRenderer, signalingParameters);
    client.createOffer();
    return client;
  }

  @Override
  public void setUp() {
    signalingExecutor = new LooperExecutor();
    signalingExecutor.requestStart();
  }

  @Override
  public void tearDown() {
    signalingExecutor.requestStop();
  }

  public void testSetLocalOfferMakesVideoFlowLocally()
      throws InterruptedException {
    Log.d(TAG, "testSetLocalOfferMakesVideoFlowLocally");
    MockRenderer localRenderer = new MockRenderer(EXPECTED_VIDEO_FRAMES);
    pcClient = createPeerConnectionClient(
        localRenderer, new MockRenderer(0), true, VIDEO_CODEC_VP8);

    // Wait for local SDP and ice candidates set events.
    assertTrue("Local SDP was not set.", waitForLocalSDP(WAIT_TIMEOUT));
    assertTrue("ICE candidates were not generated.",
        waitForIceCandidates(WAIT_TIMEOUT));

    // Check that local video frames were rendered.
    assertTrue("Local video frames were not rendered.",
        localRenderer.waitForFramesRendered(WAIT_TIMEOUT));

    pcClient.close();
    assertTrue("PeerConnection close event was not received.",
        waitForPeerConnectionClosed(WAIT_TIMEOUT));
    Log.d(TAG, "testSetLocalOfferMakesVideoFlowLocally Done.");
  }

  public void testSizeIsSetBeforeStartingToRender()
      throws InterruptedException {
    Log.d(TAG, "testSizeIsSetBeforeStartingToRender");
    MockRenderer localRenderer = new MockRenderer(EXPECTED_VIDEO_FRAMES);
    pcClient = createPeerConnectionClient(
        localRenderer, new MockRenderer(0), true, VIDEO_CODEC_VP8);

    waitForLocalSDP(WAIT_TIMEOUT);
    waitForIceCandidates(WAIT_TIMEOUT);

    // Check that local video frames were rendered.
    assertTrue("Local video frames were not rendered.",
        localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    assertTrue("Should have set size before rendering frames; size wasn't set",
        localRenderer.setSizeCalledBeforeRenderFrame());
    assertTrue(localRenderer.getWidth() > 0);
    assertTrue(localRenderer.getHeight() > 0);

    pcClient.close();
    waitForPeerConnectionClosed(WAIT_TIMEOUT);
    Log.d(TAG, "testSizeIsSetBeforeStartingToRender Done.");
  }

  private void testLoopback(boolean enableVideo, String videoCodec)
      throws InterruptedException {
    loopback = true;
    MockRenderer localRenderer = null;
    MockRenderer remoteRenderer = null;
    if (enableVideo) {
      Log.d(TAG, "testLoopback for video " + videoCodec);
      localRenderer = new MockRenderer(EXPECTED_VIDEO_FRAMES);
      remoteRenderer = new MockRenderer(EXPECTED_VIDEO_FRAMES);
    } else {
      Log.d(TAG, "testLoopback for audio.");
    }
    pcClient = createPeerConnectionClient(
        localRenderer, remoteRenderer, enableVideo, videoCodec);

    // Wait for local SDP, rename it to answer and set as remote SDP.
    assertTrue("Local SDP was not set.", waitForLocalSDP(WAIT_TIMEOUT));
    SessionDescription remoteSdp = new SessionDescription(
        SessionDescription.Type.fromCanonicalForm("answer"),
        localSdp.description);
    pcClient.setRemoteDescription(remoteSdp);

    // Wait for ICE connection.
    assertTrue("ICE connection failure.", waitForIceConnected(ICE_CONNECTION_WAIT_TIMEOUT));

    if (enableVideo) {
      // Check that local and remote video frames were rendered.
      assertTrue("Local video frames were not rendered.",
          localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
      assertTrue("Remote video frames were not rendered.",
          remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    } else {
      // For audio just sleep for 1 sec.
      // TODO(glaznev): check how we can detect that remote audio was rendered.
      Thread.sleep(AUDIO_RUN_TIMEOUT);
    }

    pcClient.close();
    assertTrue(waitForPeerConnectionClosed(WAIT_TIMEOUT));
    Log.d(TAG, "testLoopback done.");
  }

  public void testLoopbackAudio() throws InterruptedException {
    testLoopback(false, VIDEO_CODEC_VP8);
  }

  public void testLoopbackVp8() throws InterruptedException {
    testLoopback(true, VIDEO_CODEC_VP8);
  }

  public void testLoopbackVp9() throws InterruptedException {
    testLoopback(true, VIDEO_CODEC_VP9);
  }

  public void testLoopbackH264() throws InterruptedException {
    testLoopback(true, VIDEO_CODEC_H264);
  }

  // Checks if default front camera can be switched to back camera and then
  // again to front camera.
  public void testCameraSwitch() throws InterruptedException {
    Log.d(TAG, "testCameraSwitch");
    loopback = true;

    MockRenderer localRenderer = new MockRenderer(EXPECTED_VIDEO_FRAMES);
    MockRenderer remoteRenderer = new MockRenderer(EXPECTED_VIDEO_FRAMES);

    pcClient = createPeerConnectionClient(
        localRenderer, remoteRenderer, true, VIDEO_CODEC_VP8);

    // Wait for local SDP, rename it to answer and set as remote SDP.
    assertTrue("Local SDP was not set.", waitForLocalSDP(WAIT_TIMEOUT));
    SessionDescription remoteSdp = new SessionDescription(
        SessionDescription.Type.fromCanonicalForm("answer"),
        localSdp.description);
    pcClient.setRemoteDescription(remoteSdp);

    // Wait for ICE connection.
    assertTrue("ICE connection failure.", waitForIceConnected(ICE_CONNECTION_WAIT_TIMEOUT));

    // Check that local and remote video frames were rendered.
    assertTrue("Local video frames were not rendered before camera switch.",
        localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    assertTrue("Remote video frames were not rendered before camera switch.",
        remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));

    for (int i = 0; i < CAMERA_SWITCH_ATTEMPTS; i++) {
      // Try to switch camera
      pcClient.switchCamera();

      // Reset video renders and check that local and remote video frames
      // were rendered after camera switch.
      localRenderer.reset(EXPECTED_VIDEO_FRAMES);
      remoteRenderer.reset(EXPECTED_VIDEO_FRAMES);
      assertTrue("Local video frames were not rendered after camera switch.",
          localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
      assertTrue("Remote video frames were not rendered after camera switch.",
          remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    }
    pcClient.close();
    assertTrue(waitForPeerConnectionClosed(WAIT_TIMEOUT));
    Log.d(TAG, "testCameraSwitch done.");
  }

  // Checks if video source can be restarted - simulate app goes to
  // background and back to foreground.
  public void testVideoSourceRestart() throws InterruptedException {
    Log.d(TAG, "testVideoSourceRestart");
    loopback = true;

    MockRenderer localRenderer = new MockRenderer(EXPECTED_VIDEO_FRAMES);
    MockRenderer remoteRenderer = new MockRenderer(EXPECTED_VIDEO_FRAMES);

    pcClient = createPeerConnectionClient(
        localRenderer, remoteRenderer, true, VIDEO_CODEC_VP8);

    // Wait for local SDP, rename it to answer and set as remote SDP.
    assertTrue("Local SDP was not set.", waitForLocalSDP(WAIT_TIMEOUT));
    SessionDescription remoteSdp = new SessionDescription(
        SessionDescription.Type.fromCanonicalForm("answer"),
        localSdp.description);
    pcClient.setRemoteDescription(remoteSdp);

    // Wait for ICE connection.
    assertTrue("ICE connection failure.", waitForIceConnected(ICE_CONNECTION_WAIT_TIMEOUT));

    // Check that local and remote video frames were rendered.
    assertTrue("Local video frames were not rendered before video restart.",
        localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    assertTrue("Remote video frames were not rendered before video restart.",
        remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));

    // Stop and then start video source a few times.
    for (int i = 0; i < VIDEO_RESTART_ATTEMPTS; i++) {
      pcClient.stopVideoSource();
      Thread.sleep(VIDEO_RESTART_TIMEOUT);
      pcClient.startVideoSource();

      // Reset video renders and check that local and remote video frames
      // were rendered after video restart.
      localRenderer.reset(EXPECTED_VIDEO_FRAMES);
      remoteRenderer.reset(EXPECTED_VIDEO_FRAMES);
      assertTrue("Local video frames were not rendered after video restart.",
          localRenderer.waitForFramesRendered(WAIT_TIMEOUT));
      assertTrue("Remote video frames were not rendered after video restart.",
          remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));
    }
    pcClient.close();
    assertTrue(waitForPeerConnectionClosed(WAIT_TIMEOUT));
    Log.d(TAG, "testVideoSourceRestart done.");
  }

}
