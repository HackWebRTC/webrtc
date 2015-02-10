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
  private static final String STUN_SERVER = "stun:stun.l.google.com:19302";
  private static final int WAIT_TIMEOUT = 5000;
  private static final int EXPECTED_VIDEO_FRAMES = 15;

  // The peer connection client is assumed to be thread safe in itself; the
  // reference is written by the test thread and read by worker threads.
  private volatile PeerConnectionClient pcClient;
  private volatile boolean loopback;

  // These are protected by their respective event objects.
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
    private final CountDownLatch doneRendering;

    public MockRenderer(int expectedFrames) {
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
  public void onIceCandidate(IceCandidate candidate) {
    Log.d(TAG, "IceCandidate: " + candidate.sdp);
    synchronized(iceCandidateEvent) {
      if (loopback) {
        pcClient.addRemoteIceCandidate(candidate);
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
    PeerConnection.IceServer iceServer = new
        PeerConnection.IceServer(STUN_SERVER, "", "");
    iceServers.add(iceServer);
    MediaConstraints pcConstraints = new MediaConstraints();
    MediaConstraints videoConstraints = new MediaConstraints();
    MediaConstraints audioConstraints = new MediaConstraints();
    SignalingParameters signalingParameters = new SignalingParameters(
        iceServers, true,
        pcConstraints, videoConstraints, audioConstraints,
        null, null, null,
        null, null);
    return signalingParameters;
  }

  PeerConnectionClient createPeerConnectionClient(MockRenderer localRenderer,
                                                  MockRenderer remoteRenderer) {
    SignalingParameters signalingParameters = getTestSignalingParameters();
    PeerConnectionParameters peerConnectionParameters =
        new PeerConnectionParameters(0, 0, 0, 0, false);

    PeerConnectionClient client = new PeerConnectionClient();
    client.createPeerConnectionFactory(
        getInstrumentation().getContext(), "VP8", true, null, this);
    client.createPeerConnection(localRenderer, remoteRenderer,
        signalingParameters, peerConnectionParameters);
    client.createOffer();
    return client;
  }

  public void testSetLocalOfferMakesVideoFlowLocally()
      throws InterruptedException {
    Log.d(TAG, "testSetLocalOfferMakesVideoFlowLocally");
    MockRenderer localRenderer = new MockRenderer(EXPECTED_VIDEO_FRAMES);
    pcClient = createPeerConnectionClient(localRenderer, new MockRenderer(0));

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
    pcClient = createPeerConnectionClient(localRenderer, new MockRenderer(0));

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

  public void testLoopback() throws InterruptedException {
    Log.d(TAG, "testLoopback");
    loopback = true;

    MockRenderer localRenderer = new MockRenderer(EXPECTED_VIDEO_FRAMES);
    MockRenderer remoteRenderer = new MockRenderer(EXPECTED_VIDEO_FRAMES);

    pcClient = createPeerConnectionClient(localRenderer, remoteRenderer);

    // Wait for local SDP, rename it to answer and set as remote SDP.
    assertTrue("Local SDP was not set.", waitForLocalSDP(WAIT_TIMEOUT));
    SessionDescription remoteSdp = new SessionDescription(
        SessionDescription.Type.fromCanonicalForm("answer"),
        localSdp.description);
    pcClient.setRemoteDescription(remoteSdp);

    // Wait for ICE connection.
    assertTrue("ICE connection failure.", waitForIceConnected(WAIT_TIMEOUT));

    // Check that local video frames were rendered.
    assertTrue("Local video frames were not rendered.",
        localRenderer.waitForFramesRendered(WAIT_TIMEOUT));

    // Check that remote video frames were rendered.
    assertTrue("Remote video frames were not rendered.",
        remoteRenderer.waitForFramesRendered(WAIT_TIMEOUT));

    pcClient.close();
    assertTrue(waitForPeerConnectionClosed(WAIT_TIMEOUT));
    Log.d(TAG, "testLoopback Done.");
  }

}
