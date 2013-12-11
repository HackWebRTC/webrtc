/*
 * libjingle
 * Copyright 2013, Google Inc.
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

package org.webrtc;

import junit.framework.TestCase;

import org.junit.Test;
import org.webrtc.PeerConnection.IceConnectionState;
import org.webrtc.PeerConnection.IceGatheringState;
import org.webrtc.PeerConnection.SignalingState;

import java.io.File;
import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.util.EnumSet;
import java.util.IdentityHashMap;
import java.util.LinkedList;
import java.util.Map;
import java.util.TreeSet;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/** End-to-end tests for PeerConnection.java. */
public class PeerConnectionTest extends TestCase {
  // Set to true to render video.
  private static final boolean RENDER_TO_GUI = false;

  private static class ObserverExpectations implements PeerConnection.Observer,
                                            VideoRenderer.Callbacks,
                                            DataChannel.Observer,
                                            StatsObserver {
    private final String name;
    private int expectedIceCandidates = 0;
    private int expectedErrors = 0;
    private int expectedSetSize = 0;
    private int previouslySeenWidth = 0;
    private int previouslySeenHeight = 0;
    private int expectedFramesDelivered = 0;
    private LinkedList<SignalingState> expectedSignalingChanges =
        new LinkedList<SignalingState>();
    private LinkedList<IceConnectionState> expectedIceConnectionChanges =
        new LinkedList<IceConnectionState>();
    private LinkedList<IceGatheringState> expectedIceGatheringChanges =
        new LinkedList<IceGatheringState>();
    private LinkedList<String> expectedAddStreamLabels =
        new LinkedList<String>();
    private LinkedList<String> expectedRemoveStreamLabels =
        new LinkedList<String>();
    public LinkedList<IceCandidate> gotIceCandidates =
        new LinkedList<IceCandidate>();
    private Map<MediaStream, WeakReference<VideoRenderer>> renderers =
        new IdentityHashMap<MediaStream, WeakReference<VideoRenderer>>();
    private DataChannel dataChannel;
    private LinkedList<DataChannel.Buffer> expectedBuffers =
        new LinkedList<DataChannel.Buffer>();
    private LinkedList<DataChannel.State> expectedStateChanges =
        new LinkedList<DataChannel.State>();
    private LinkedList<String> expectedRemoteDataChannelLabels =
        new LinkedList<String>();
    private int expectedStatsCallbacks = 0;
    private LinkedList<StatsReport[]> gotStatsReports =
        new LinkedList<StatsReport[]>();

    public ObserverExpectations(String name) {
      this.name = name;
    }

    public synchronized void setDataChannel(DataChannel dataChannel) {
      assertNull(this.dataChannel);
      this.dataChannel = dataChannel;
      this.dataChannel.registerObserver(this);
      assertNotNull(this.dataChannel);
    }

    public synchronized void expectIceCandidates(int count) {
      expectedIceCandidates += count;
    }

    public synchronized void onIceCandidate(IceCandidate candidate) {
      --expectedIceCandidates;
      // We don't assert expectedIceCandidates >= 0 because it's hard to know
      // how many to expect, in general.  We only use expectIceCandidates to
      // assert a minimal count.
      gotIceCandidates.add(candidate);
    }

    public synchronized void expectError() {
      ++expectedErrors;
    }

    public synchronized void onError() {
      assertTrue(--expectedErrors >= 0);
    }

    public synchronized void expectSetSize() {
      if (RENDER_TO_GUI) {
        // When new frames are delivered to the GUI renderer we don't get
        // notified of frame size info.
        return;
      }
      ++expectedSetSize;
    }

    @Override
    public synchronized void setSize(int width, int height) {
      assertFalse(RENDER_TO_GUI);
      assertTrue(--expectedSetSize >= 0);
      // Because different camera devices (fake & physical) produce different
      // resolutions, we only sanity-check the set sizes,
      assertTrue(width > 0);
      assertTrue(height > 0);
      if (previouslySeenWidth > 0) {
        assertEquals(previouslySeenWidth, width);
        assertEquals(previouslySeenHeight, height);
      } else {
        previouslySeenWidth = width;
        previouslySeenHeight = height;
      }
    }

    public synchronized void expectFramesDelivered(int count) {
      assertFalse(RENDER_TO_GUI);
      expectedFramesDelivered += count;
    }

    @Override
    public synchronized void renderFrame(VideoRenderer.I420Frame frame) {
      --expectedFramesDelivered;
    }

    public synchronized void expectSignalingChange(SignalingState newState) {
      expectedSignalingChanges.add(newState);
    }

    @Override
    public synchronized void onSignalingChange(SignalingState newState) {
      assertEquals(expectedSignalingChanges.removeFirst(), newState);
    }

    public synchronized void expectIceConnectionChange(
        IceConnectionState newState) {
      expectedIceConnectionChanges.add(newState);
    }

    @Override
    public synchronized void onIceConnectionChange(
        IceConnectionState newState) {
      assertEquals(expectedIceConnectionChanges.removeFirst(), newState);
    }

    public synchronized void expectIceGatheringChange(
        IceGatheringState newState) {
      expectedIceGatheringChanges.add(newState);
    }

    @Override
    public synchronized void onIceGatheringChange(IceGatheringState newState) {
      // It's fine to get a variable number of GATHERING messages before
      // COMPLETE fires (depending on how long the test runs) so we don't assert
      // any particular count.
      if (newState == IceGatheringState.GATHERING) {
        return;
      }
      assertEquals(expectedIceGatheringChanges.removeFirst(), newState);
    }

    public synchronized void expectAddStream(String label) {
      expectedAddStreamLabels.add(label);
    }

    @Override
    public synchronized void onAddStream(MediaStream stream) {
      assertEquals(expectedAddStreamLabels.removeFirst(), stream.label());
      assertEquals(1, stream.videoTracks.size());
      assertEquals(1, stream.audioTracks.size());
      assertTrue(stream.videoTracks.get(0).id().endsWith("LMSv0"));
      assertTrue(stream.audioTracks.get(0).id().endsWith("LMSa0"));
      assertEquals("video", stream.videoTracks.get(0).kind());
      assertEquals("audio", stream.audioTracks.get(0).kind());
      VideoRenderer renderer = createVideoRenderer(this);
      stream.videoTracks.get(0).addRenderer(renderer);
      assertNull(renderers.put(
          stream, new WeakReference<VideoRenderer>(renderer)));
    }

    public synchronized void expectRemoveStream(String label) {
      expectedRemoveStreamLabels.add(label);
    }

    @Override
    public synchronized void onRemoveStream(MediaStream stream) {
      assertEquals(expectedRemoveStreamLabels.removeFirst(), stream.label());
      WeakReference<VideoRenderer> renderer = renderers.remove(stream);
      assertNotNull(renderer);
      assertNotNull(renderer.get());
      assertEquals(1, stream.videoTracks.size());
      stream.videoTracks.get(0).removeRenderer(renderer.get());
    }

    public synchronized void expectDataChannel(String label) {
      expectedRemoteDataChannelLabels.add(label);
    }

    @Override
    public synchronized void onDataChannel(DataChannel remoteDataChannel) {
      assertEquals(expectedRemoteDataChannelLabels.removeFirst(),
                   remoteDataChannel.label());
      setDataChannel(remoteDataChannel);
      assertEquals(DataChannel.State.CONNECTING, dataChannel.state());
    }

    public synchronized void expectMessage(ByteBuffer expectedBuffer,
                                           boolean expectedBinary) {
      expectedBuffers.add(
          new DataChannel.Buffer(expectedBuffer, expectedBinary));
    }

    @Override
    public synchronized void onMessage(DataChannel.Buffer buffer) {
      DataChannel.Buffer expected = expectedBuffers.removeFirst();
      assertEquals(expected.binary, buffer.binary);
      assertTrue(expected.data.equals(buffer.data));
    }

    @Override
    public synchronized void onStateChange() {
      assertEquals(expectedStateChanges.removeFirst(), dataChannel.state());
    }

    public synchronized void expectStateChange(DataChannel.State state) {
      expectedStateChanges.add(state);
    }

    @Override
    public synchronized void onComplete(StatsReport[] reports) {
      if (--expectedStatsCallbacks < 0) {
        throw new RuntimeException("Unexpected stats report: " + reports);
      }
      gotStatsReports.add(reports);
    }

    public synchronized void expectStatsCallback() {
      ++expectedStatsCallbacks;
    }

    public synchronized LinkedList<StatsReport[]> takeStatsReports() {
      LinkedList<StatsReport[]> got = gotStatsReports;
      gotStatsReports = new LinkedList<StatsReport[]>();
      return got;
    }

    // Return a set of expectations that haven't been satisfied yet, possibly
    // empty if no such expectations exist.
    public synchronized TreeSet<String> unsatisfiedExpectations() {
      TreeSet<String> stillWaitingForExpectations = new TreeSet<String>();
      if (expectedIceCandidates > 0) {  // See comment in onIceCandidate.
        stillWaitingForExpectations.add("expectedIceCandidates");
      }
      if (expectedErrors != 0) {
        stillWaitingForExpectations.add("expectedErrors: " + expectedErrors);
      }
      if (expectedSignalingChanges.size() != 0) {
        stillWaitingForExpectations.add(
            "expectedSignalingChanges: " + expectedSignalingChanges.size());
      }
      if (expectedIceConnectionChanges.size() != 0) {
        stillWaitingForExpectations.add("expectedIceConnectionChanges: " +
                                        expectedIceConnectionChanges.size());
      }
      if (expectedIceGatheringChanges.size() != 0) {
        stillWaitingForExpectations.add("expectedIceGatheringChanges: " +
                                        expectedIceGatheringChanges.size());
      }
      if (expectedAddStreamLabels.size() != 0) {
        stillWaitingForExpectations.add(
            "expectedAddStreamLabels: " + expectedAddStreamLabels.size());
      }
      if (expectedRemoveStreamLabels.size() != 0) {
        stillWaitingForExpectations.add(
            "expectedRemoveStreamLabels: " + expectedRemoveStreamLabels.size());
      }
      if (expectedSetSize != 0) {
        stillWaitingForExpectations.add("expectedSetSize");
      }
      if (expectedFramesDelivered > 0) {
        stillWaitingForExpectations.add(
            "expectedFramesDelivered: " + expectedFramesDelivered);
      }
      if (!expectedBuffers.isEmpty()) {
        stillWaitingForExpectations.add(
            "expectedBuffers: " + expectedBuffers.size());
      }
      if (!expectedStateChanges.isEmpty()) {
        stillWaitingForExpectations.add(
            "expectedStateChanges: " + expectedStateChanges.size());
      }
      if (!expectedRemoteDataChannelLabels.isEmpty()) {
        stillWaitingForExpectations.add("expectedRemoteDataChannelLabels: " +
                                        expectedRemoteDataChannelLabels.size());
      }
      if (expectedStatsCallbacks != 0) {
        stillWaitingForExpectations.add(
            "expectedStatsCallbacks: " + expectedStatsCallbacks);
      }
      return stillWaitingForExpectations;
    }

    public void waitForAllExpectationsToBeSatisfied() {
      // TODO(fischman): problems with this approach:
      // - come up with something better than a poll loop
      // - avoid serializing expectations explicitly; the test is not as robust
      //   as it could be because it must place expectations between wait
      //   statements very precisely (e.g. frame must not arrive before its
      //   expectation, and expectation must not be registered so early as to
      //   stall a wait).  Use callbacks to fire off dependent steps instead of
      //   explicitly waiting, so there can be just a single wait at the end of
      //   the test.
      TreeSet<String> prev = null;
      TreeSet<String> stillWaitingForExpectations = unsatisfiedExpectations();
      while (!stillWaitingForExpectations.isEmpty()) {
        if (!stillWaitingForExpectations.equals(prev)) {
          System.out.println(
              name + " still waiting at\n    " +
              (new Throwable()).getStackTrace()[1] +
              "\n    for: " +
              Arrays.toString(stillWaitingForExpectations.toArray()));
        }
        try {
          Thread.sleep(10);
        } catch (InterruptedException e) {
          throw new RuntimeException(e);
        }
        prev = stillWaitingForExpectations;
        stillWaitingForExpectations = unsatisfiedExpectations();
      }
      if (prev == null) {
        System.out.println(name + " didn't need to wait at\n    " +
                           (new Throwable()).getStackTrace()[1]);
      }
    }
  }

  private static class SdpObserverLatch implements SdpObserver {
    private boolean success = false;
    private SessionDescription sdp = null;
    private String error = null;
    private CountDownLatch latch = new CountDownLatch(1);

    public SdpObserverLatch() {}

    public void onCreateSuccess(SessionDescription sdp) {
      this.sdp = sdp;
      onSetSuccess();
    }

    public void onSetSuccess() {
      success = true;
      latch.countDown();
    }

    public void onCreateFailure(String error) {
      onSetFailure(error);
    }

    public void onSetFailure(String error) {
      this.error = error;
      latch.countDown();
    }

    public boolean await() {
      try {
        assertTrue(latch.await(1000, TimeUnit.MILLISECONDS));
        return getSuccess();
      } catch (Exception e) {
        throw new RuntimeException(e);
      }
    }

    public boolean getSuccess() {
      return success;
    }

    public SessionDescription getSdp() {
      return sdp;
    }

    public String getError() {
      return error;
    }
  }

  static int videoWindowsMapped = -1;

  private static class TestRenderer implements VideoRenderer.Callbacks {
    public int width = -1;
    public int height = -1;
    public int numFramesDelivered = 0;

    public void setSize(int width, int height) {
      assertEquals(this.width, -1);
      assertEquals(this.height, -1);
      this.width = width;
      this.height = height;
    }

    public void renderFrame(VideoRenderer.I420Frame frame) {
      ++numFramesDelivered;
    }
  }

  private static VideoRenderer createVideoRenderer(
      VideoRenderer.Callbacks videoCallbacks) {
    if (!RENDER_TO_GUI) {
      return new VideoRenderer(videoCallbacks);
    }
    ++videoWindowsMapped;
    assertTrue(videoWindowsMapped < 4);
    int x = videoWindowsMapped % 2 != 0 ? 700 : 0;
    int y = videoWindowsMapped >= 2 ? 0 : 500;
    return VideoRenderer.createGui(x, y);
  }

  // Return a weak reference to test that ownership is correctly held by
  // PeerConnection, not by test code.
  private static WeakReference<MediaStream> addTracksToPC(
      PeerConnectionFactory factory, PeerConnection pc,
      VideoSource videoSource,
      String streamLabel, String videoTrackId, String audioTrackId,
      VideoRenderer.Callbacks videoCallbacks) {
    MediaStream lMS = factory.createLocalMediaStream(streamLabel);
    VideoTrack videoTrack =
        factory.createVideoTrack(videoTrackId, videoSource);
    assertNotNull(videoTrack);
    VideoRenderer videoRenderer = createVideoRenderer(videoCallbacks);
    assertNotNull(videoRenderer);
    videoTrack.addRenderer(videoRenderer);
    lMS.addTrack(videoTrack);
    // Just for fun, let's remove and re-add the track.
    lMS.removeTrack(videoTrack);
    lMS.addTrack(videoTrack);
    lMS.addTrack(factory.createAudioTrack(audioTrackId));
    pc.addStream(lMS, new MediaConstraints());
    return new WeakReference<MediaStream>(lMS);
  }

  private static void assertEquals(
      SessionDescription lhs, SessionDescription rhs) {
    assertEquals(lhs.type, rhs.type);
    assertEquals(lhs.description, rhs.description);
  }

  @Test
  public void testCompleteSession() throws Exception {
    CountDownLatch testDone = new CountDownLatch(1);
    System.gc();  // Encourage any GC-related threads to start up.
    TreeSet<String> threadsBeforeTest = allThreads();

    PeerConnectionFactory factory = new PeerConnectionFactory();
    // Uncomment to get ALL WebRTC tracing and SENSITIVE libjingle logging.
    // NOTE: this _must_ happen while |factory| is alive!
    // Logging.enableTracing(
    //     "/tmp/PeerConnectionTest-log.txt",
    //     EnumSet.of(Logging.TraceLevel.TRACE_ALL),
    //     Logging.Severity.LS_SENSITIVE);

    MediaConstraints pcConstraints = new MediaConstraints();
    pcConstraints.mandatory.add(
        new MediaConstraints.KeyValuePair("DtlsSrtpKeyAgreement", "true"));
    pcConstraints.optional.add(
        new MediaConstraints.KeyValuePair("RtpDataChannels", "true"));
    // TODO(fischman): replace above with below to test SCTP channels when
    // supported (https://code.google.com/p/webrtc/issues/detail?id=1408).
    // pcConstraints.optional.add(new MediaConstraints.KeyValuePair(
    //     "internalSctpDataChannels", "true"));

    LinkedList<PeerConnection.IceServer> iceServers =
        new LinkedList<PeerConnection.IceServer>();
    iceServers.add(new PeerConnection.IceServer(
        "stun:stun.l.google.com:19302"));
    iceServers.add(new PeerConnection.IceServer(
        "turn:fake.example.com", "fakeUsername", "fakePassword"));
    ObserverExpectations offeringExpectations =
        new ObserverExpectations("PCTest:offerer");
    PeerConnection offeringPC = factory.createPeerConnection(
        iceServers, pcConstraints, offeringExpectations);
    assertNotNull(offeringPC);

    ObserverExpectations answeringExpectations =
        new ObserverExpectations("PCTest:answerer");
    PeerConnection answeringPC = factory.createPeerConnection(
        iceServers, pcConstraints, answeringExpectations);
    assertNotNull(answeringPC);

    // We want to use the same camera for offerer & answerer, so create it here
    // instead of in addTracksToPC.
    VideoSource videoSource = factory.createVideoSource(
        VideoCapturer.create(""), new MediaConstraints());

    // TODO(fischman): the track ids here and in the addTracksToPC() call
    // below hard-code the <mediaStreamLabel>[av]<index> scheme used in the
    // serialized SDP, because the C++ API doesn't auto-translate.
    // Drop |label| params from {Audio,Video}Track-related APIs once
    // https://code.google.com/p/webrtc/issues/detail?id=1253 is fixed.
    offeringExpectations.expectSetSize();
    WeakReference<MediaStream> oLMS = addTracksToPC(
        factory, offeringPC, videoSource, "oLMS", "oLMSv0", "oLMSa0",
        offeringExpectations);

    DataChannel offeringDC = offeringPC.createDataChannel(
        "offeringDC", new DataChannel.Init());
    assertEquals("offeringDC", offeringDC.label());

    offeringExpectations.setDataChannel(offeringDC);
    SdpObserverLatch sdpLatch = new SdpObserverLatch();
    offeringPC.createOffer(sdpLatch, new MediaConstraints());
    assertTrue(sdpLatch.await());
    SessionDescription offerSdp = sdpLatch.getSdp();
    assertEquals(offerSdp.type, SessionDescription.Type.OFFER);
    assertFalse(offerSdp.description.isEmpty());

    sdpLatch = new SdpObserverLatch();
    answeringExpectations.expectSignalingChange(
        SignalingState.HAVE_REMOTE_OFFER);
    answeringExpectations.expectAddStream("oLMS");
    answeringExpectations.expectDataChannel("offeringDC");
    answeringPC.setRemoteDescription(sdpLatch, offerSdp);
    assertEquals(
        PeerConnection.SignalingState.STABLE, offeringPC.signalingState());
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    answeringExpectations.expectSetSize();
    WeakReference<MediaStream> aLMS = addTracksToPC(
        factory, answeringPC, videoSource, "aLMS", "aLMSv0", "aLMSa0",
        answeringExpectations);

    sdpLatch = new SdpObserverLatch();
    answeringPC.createAnswer(sdpLatch, new MediaConstraints());
    assertTrue(sdpLatch.await());
    SessionDescription answerSdp = sdpLatch.getSdp();
    assertEquals(answerSdp.type, SessionDescription.Type.ANSWER);
    assertFalse(answerSdp.description.isEmpty());

    offeringExpectations.expectIceCandidates(2);
    answeringExpectations.expectIceCandidates(2);

    offeringExpectations.expectIceGatheringChange(IceGatheringState.COMPLETE);
    answeringExpectations.expectIceGatheringChange(IceGatheringState.COMPLETE);

    sdpLatch = new SdpObserverLatch();
    answeringExpectations.expectSignalingChange(SignalingState.STABLE);
    answeringPC.setLocalDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.HAVE_LOCAL_OFFER);
    offeringPC.setLocalDescription(sdpLatch, offerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());
    sdpLatch = new SdpObserverLatch();
    offeringExpectations.expectSignalingChange(SignalingState.STABLE);
    offeringExpectations.expectAddStream("aLMS");
    offeringPC.setRemoteDescription(sdpLatch, answerSdp);
    assertTrue(sdpLatch.await());
    assertNull(sdpLatch.getSdp());

    offeringExpectations.waitForAllExpectationsToBeSatisfied();
    answeringExpectations.waitForAllExpectationsToBeSatisfied();

    assertEquals(offeringPC.getLocalDescription().type, offerSdp.type);
    assertEquals(offeringPC.getRemoteDescription().type, answerSdp.type);
    assertEquals(answeringPC.getLocalDescription().type, answerSdp.type);
    assertEquals(answeringPC.getRemoteDescription().type, offerSdp.type);

    if (!RENDER_TO_GUI) {
      // Wait for at least some frames to be delivered at each end (number
      // chosen arbitrarily).
      offeringExpectations.expectFramesDelivered(10);
      answeringExpectations.expectFramesDelivered(10);
      offeringExpectations.expectSetSize();
      answeringExpectations.expectSetSize();
    }

    offeringExpectations.expectIceConnectionChange(
        IceConnectionState.CHECKING);
    offeringExpectations.expectIceConnectionChange(
        IceConnectionState.CONNECTED);
    answeringExpectations.expectIceConnectionChange(
        IceConnectionState.CHECKING);
    answeringExpectations.expectIceConnectionChange(
        IceConnectionState.CONNECTED);

    offeringExpectations.expectStateChange(DataChannel.State.OPEN);
    answeringExpectations.expectStateChange(DataChannel.State.OPEN);

    for (IceCandidate candidate : offeringExpectations.gotIceCandidates) {
      answeringPC.addIceCandidate(candidate);
    }
    offeringExpectations.gotIceCandidates.clear();
    for (IceCandidate candidate : answeringExpectations.gotIceCandidates) {
      offeringPC.addIceCandidate(candidate);
    }
    answeringExpectations.gotIceCandidates.clear();

    offeringExpectations.waitForAllExpectationsToBeSatisfied();
    answeringExpectations.waitForAllExpectationsToBeSatisfied();

    assertEquals(
        PeerConnection.SignalingState.STABLE, offeringPC.signalingState());
    assertEquals(
        PeerConnection.SignalingState.STABLE, answeringPC.signalingState());

    // Test send & receive UTF-8 text.
    answeringExpectations.expectMessage(
        ByteBuffer.wrap("hello!".getBytes(Charset.forName("UTF-8"))), false);
    DataChannel.Buffer buffer = new DataChannel.Buffer(
        ByteBuffer.wrap("hello!".getBytes(Charset.forName("UTF-8"))), false);
    assertTrue(offeringExpectations.dataChannel.send(buffer));
    answeringExpectations.waitForAllExpectationsToBeSatisfied();

    // TODO(fischman): add testing of binary messages when SCTP channels are
    // supported (https://code.google.com/p/webrtc/issues/detail?id=1408).
    // // Construct this binary message two different ways to ensure no
    // // shortcuts are taken.
    // ByteBuffer expectedBinaryMessage = ByteBuffer.allocateDirect(5);
    // for (byte i = 1; i < 6; ++i) {
    //   expectedBinaryMessage.put(i);
    // }
    // expectedBinaryMessage.flip();
    // offeringExpectations.expectMessage(expectedBinaryMessage, true);
    // assertTrue(answeringExpectations.dataChannel.send(
    //     new DataChannel.Buffer(
    //         ByteBuffer.wrap(new byte[] { 1, 2, 3, 4, 5 } ), true)));
    // offeringExpectations.waitForAllExpectationsToBeSatisfied();

    offeringExpectations.expectStateChange(DataChannel.State.CLOSING);
    answeringExpectations.expectStateChange(DataChannel.State.CLOSING);
    answeringExpectations.dataChannel.close();
    offeringExpectations.dataChannel.close();
    // TODO(fischman): implement a new offer/answer exchange to finalize the
    // closing of the channel in order to see the CLOSED state reached.
    // offeringExpectations.expectStateChange(DataChannel.State.CLOSED);
    // answeringExpectations.expectStateChange(DataChannel.State.CLOSED);

    if (RENDER_TO_GUI) {
      try {
        Thread.sleep(3000);
      } catch (Throwable t) {
        throw new RuntimeException(t);
      }
    }

    // TODO(fischman) MOAR test ideas:
    // - Test that PC.removeStream() works; requires a second
    //   createOffer/createAnswer dance.
    // - audit each place that uses |constraints| for specifying non-trivial
    //   constraints (and ensure they're honored).
    // - test error cases
    // - ensure reasonable coverage of _jni.cc is achieved.  Coverage is
    //   extra-important because of all the free-text (class/method names, etc)
    //   in JNI-style programming; make sure no typos!
    // - Test that shutdown mid-interaction is crash-free.

    // Free the Java-land objects, collect them, and sleep a bit to make sure we
    // don't get late-arrival crashes after the Java-land objects have been
    // freed.
    shutdownPC(offeringPC, offeringExpectations);
    offeringPC = null;
    shutdownPC(answeringPC, answeringExpectations);
    answeringPC = null;
    videoSource.dispose();
    factory.dispose();
    System.gc();
    TreeSet<String> threadsAfterTest = allThreads();
    assertEquals(threadsBeforeTest, threadsAfterTest);
    Thread.sleep(100);
  }

  private static void shutdownPC(
      PeerConnection pc, ObserverExpectations expectations) {
    expectations.dataChannel.unregisterObserver();
    expectations.dataChannel.dispose();
    expectations.expectStatsCallback();
    assertTrue(pc.getStats(expectations, null));
    expectations.waitForAllExpectationsToBeSatisfied();
    expectations.expectIceConnectionChange(IceConnectionState.CLOSED);
    expectations.expectSignalingChange(SignalingState.CLOSED);
    pc.close();
    expectations.waitForAllExpectationsToBeSatisfied();
    expectations.expectStatsCallback();
    assertTrue(pc.getStats(expectations, null));
    expectations.waitForAllExpectationsToBeSatisfied();

    System.out.println("FYI stats: ");
    int reportIndex = -1;
    for (StatsReport[] reports : expectations.takeStatsReports()) {
      System.out.println(" Report #" + (++reportIndex));
      for (int i = 0; i < reports.length; ++i) {
        System.out.println("  " + reports[i].toString());
      }
    }
    assertEquals(1, reportIndex);
    System.out.println("End stats.");

    pc.dispose();
  }

  // Returns a set of thread IDs belonging to this process, as Strings.
  private static TreeSet<String> allThreads() {
    TreeSet<String> threads = new TreeSet<String>();
    // This pokes at /proc instead of using the Java APIs because we're also
    // looking for libjingle/webrtc native threads, most of which won't have
    // attached to the JVM.
    for (String threadId : (new File("/proc/self/task")).list()) {
      threads.add(threadId);
    }
    return threads;
  }

  // Return a String form of |strings| joined by |separator|.
  private static String joinStrings(String separator, TreeSet<String> strings) {
    StringBuilder builder = new StringBuilder();
    for (String s : strings) {
      if (builder.length() > 0) {
        builder.append(separator);
      }
      builder.append(s);
    }
    return builder.toString();
  }
}
