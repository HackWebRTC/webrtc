/*
 * libjingle
 * Copyright 2015 Google Inc.
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

import static org.webrtc.NetworkMonitorAutoDetect.ConnectionType;
import static org.webrtc.NetworkMonitorAutoDetect.ConnectivityManagerDelegate;
import static org.webrtc.NetworkMonitorAutoDetect.INVALID_NET_ID;
import static org.webrtc.NetworkMonitorAutoDetect.NetworkState;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.telephony.TelephonyManager;
import android.test.ActivityTestCase;
import android.test.UiThreadTest;
import android.test.suitebuilder.annotation.MediumTest;
import android.test.suitebuilder.annotation.SmallTest;

/**
 * Tests for org.webrtc.NetworkMonitor.
 */
@SuppressLint("NewApi")
public class NetworkMonitorTest extends ActivityTestCase {
  /**
   * Listens for alerts fired by the NetworkMonitor when network status changes.
   */
  private static class NetworkMonitorTestObserver
      implements NetworkMonitor.NetworkObserver {
    private boolean receivedNotification = false;

    @Override
    public void onConnectionTypeChanged(ConnectionType connectionType) {
      receivedNotification = true;
    }

    public boolean hasReceivedNotification() {
      return receivedNotification;
    }

    public void resetHasReceivedNotification() {
      receivedNotification = false;
    }
  }

  /**
   * Mocks out calls to the ConnectivityManager.
   */
  private static class MockConnectivityManagerDelegate extends ConnectivityManagerDelegate {
    private boolean activeNetworkExists;
    private int networkType;
    private int networkSubtype;

    @Override
    public NetworkState getNetworkState() {
      return new NetworkState(activeNetworkExists, networkType, networkSubtype);
    }

    // Dummy implementations to avoid NullPointerExceptions in default implementations:

    @Override
    public int getDefaultNetId() {
      return INVALID_NET_ID;
    }

    @Override
    public Network[] getAllNetworks() {
      return new Network[0];
    }

    @Override
    public NetworkState getNetworkState(Network network) {
      return new NetworkState(false, -1, -1);
    }

    public void setActiveNetworkExists(boolean networkExists) {
      activeNetworkExists = networkExists;
    }

    public void setNetworkType(int networkType) {
      this.networkType = networkType;
    }

    public void setNetworkSubtype(int networkSubtype) {
      this.networkSubtype = networkSubtype;
    }
  }

  /**
   * Mocks out calls to the WifiManager.
   */
  private static class MockWifiManagerDelegate
      extends NetworkMonitorAutoDetect.WifiManagerDelegate {
    private String wifiSSID;

    @Override
    public String getWifiSSID() {
      return wifiSSID;
    }

    public void setWifiSSID(String wifiSSID) {
      this.wifiSSID = wifiSSID;
    }
  }

  // A dummy NetworkMonitorAutoDetect.Observer.
  private static class TestNetworkMonitorAutoDetectObserver
      implements NetworkMonitorAutoDetect.Observer {

    @Override
    public void onConnectionTypeChanged(ConnectionType newConnectionType) {}
  }

  private static final Object lock = new Object();
  private static Handler uiThreadHandler = null;

  private NetworkMonitorAutoDetect receiver;
  private MockConnectivityManagerDelegate connectivityDelegate;
  private MockWifiManagerDelegate wifiDelegate;

  private static Handler getUiThreadHandler() {
    synchronized (lock) {
      if (uiThreadHandler == null ) {
        uiThreadHandler = new Handler(Looper.getMainLooper());
      }
      return uiThreadHandler;
    }
  }

  /**
   * Helper method to create a network monitor and delegates for testing.
   */
  private void createTestMonitor() {
    Context context = getInstrumentation().getTargetContext();
    NetworkMonitor.resetInstanceForTests(context);
    NetworkMonitor.setAutoDetectConnectivityState(true);
    receiver = NetworkMonitor.getAutoDetectorForTest();
    assertNotNull(receiver);

    connectivityDelegate = new MockConnectivityManagerDelegate();
    connectivityDelegate.setActiveNetworkExists(true);
    receiver.setConnectivityManagerDelegateForTests(connectivityDelegate);

    wifiDelegate = new MockWifiManagerDelegate();
    receiver.setWifiManagerDelegateForTests(wifiDelegate);
    wifiDelegate.setWifiSSID("foo");
  }

  private NetworkMonitorAutoDetect.ConnectionType getCurrentConnectionType() {
    final NetworkMonitorAutoDetect.NetworkState networkState =
        receiver.getCurrentNetworkState();
    return receiver.getCurrentConnectionType(networkState);
  }

  @Override
  protected void setUp() throws Exception {
    super.setUp();
    getUiThreadHandler().post(new Runnable() {
      public void run() {
        createTestMonitor();
      }
    });
  }

  /**
   * Tests that the receiver registers for connectivity intents during construction.
   */
  @UiThreadTest
  @SmallTest
  public void testNetworkMonitorRegistersInConstructor() throws InterruptedException {
    Context context = getInstrumentation().getTargetContext();

    NetworkMonitorAutoDetect.Observer observer = new TestNetworkMonitorAutoDetectObserver();

    NetworkMonitorAutoDetect receiver = new NetworkMonitorAutoDetect(observer, context);

    assertTrue(receiver.isReceiverRegisteredForTesting());
  }

  /**
   * Tests that when there is an intent indicating a change in network connectivity, it sends a
   * notification to Java observers.
   */
  @UiThreadTest
  @MediumTest
  public void testNetworkMonitorJavaObservers() throws InterruptedException {
    // Initialize the NetworkMonitor with a connection.
    Intent connectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
    receiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);

    // We shouldn't be re-notified if the connection hasn't actually changed.
    NetworkMonitorTestObserver observer = new NetworkMonitorTestObserver();
    NetworkMonitor.addNetworkObserver(observer);
    receiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);
    assertFalse(observer.hasReceivedNotification());

    // We shouldn't be notified if we're connected to non-Wifi and the Wifi SSID changes.
    wifiDelegate.setWifiSSID("bar");
    receiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);
    assertFalse(observer.hasReceivedNotification());

    // We should be notified when we change to Wifi.
    connectivityDelegate.setNetworkType(ConnectivityManager.TYPE_WIFI);
    receiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);
    assertTrue(observer.hasReceivedNotification());
    observer.resetHasReceivedNotification();

    // We should be notified when the Wifi SSID changes.
    wifiDelegate.setWifiSSID("foo");
    receiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);
    assertTrue(observer.hasReceivedNotification());
    observer.resetHasReceivedNotification();

    // We shouldn't be re-notified if the Wifi SSID hasn't actually changed.
    receiver.onReceive(getInstrumentation().getTargetContext(), connectivityIntent);
    assertFalse(observer.hasReceivedNotification());

    // Mimic that connectivity has been lost and ensure that the observer gets the notification.
    connectivityDelegate.setActiveNetworkExists(false);
    Intent noConnectivityIntent = new Intent(ConnectivityManager.CONNECTIVITY_ACTION);
    receiver.onReceive(getInstrumentation().getTargetContext(), noConnectivityIntent);
    assertTrue(observer.hasReceivedNotification());
  }

  /**
   * Tests that ConnectivityManagerDelegate doesn't crash. This test cannot rely on having any
   * active network connections so it cannot usefully check results, but it can at least check
   * that the functions don't crash.
   */
  @UiThreadTest
  @SmallTest
  public void testConnectivityManagerDelegateDoesNotCrash() {
    ConnectivityManagerDelegate delegate =
        new ConnectivityManagerDelegate(getInstrumentation().getTargetContext());
    delegate.getNetworkState();
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
      Network[] networks = delegate.getAllNetworks();
      if (networks.length >= 1) {
        delegate.getNetworkState(networks[0]);
        delegate.hasInternetCapability(networks[0]);
      }
      delegate.getDefaultNetId();
    }
  }

  /**
   * Tests that NetworkMonitorAutoDetect queryable APIs don't crash. This test cannot rely
   * on having any active network connections so it cannot usefully check results, but it can at
   * least check that the functions don't crash.
   */
  @UiThreadTest
  @SmallTest
  public void testQueryableAPIsDoNotCrash() {
    NetworkMonitorAutoDetect.Observer observer = new TestNetworkMonitorAutoDetectObserver();
    NetworkMonitorAutoDetect ncn =
        new NetworkMonitorAutoDetect(observer, getInstrumentation().getTargetContext());
    ncn.getDefaultNetId();
  }
}
