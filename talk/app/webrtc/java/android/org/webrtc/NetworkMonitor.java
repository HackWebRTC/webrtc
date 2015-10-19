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
import static org.webrtc.NetworkMonitorAutoDetect.INVALID_NET_ID;

import android.content.Context;
import android.util.Log;

import java.util.ArrayList;

/**
 * Borrowed from Chromium's src/net/android/java/src/org/chromium/net/NetworkChangeNotifier.java
 *
 * Triggers updates to the underlying network state from OS networking events.
 *
 * WARNING: This class is not thread-safe.
 */
public class NetworkMonitor {
  /**
   * Alerted when the connection type of the network changes.
   * The alert is fired on the UI thread.
   */
  public interface NetworkObserver {
    public void onConnectionTypeChanged(ConnectionType connectionType);
  }

  private static final String TAG = "NetworkMonitor";
  private static NetworkMonitor instance;

  private final Context applicationContext;

  // Native observers of the connection type changes.
  private final ArrayList<Long> nativeNetworkObservers;
  // Java observers of the connection type changes.
  private final ArrayList<NetworkObserver> networkObservers;

  // Object that detects the connection type changes.
  private NetworkMonitorAutoDetect autoDetector;

  private ConnectionType currentConnectionType = ConnectionType.CONNECTION_UNKNOWN;

  private NetworkMonitor(Context context) {
    assertIsTrue(context != null);
    applicationContext =
        context.getApplicationContext() == null ? context : context.getApplicationContext();

    nativeNetworkObservers = new ArrayList<Long>();
    networkObservers = new ArrayList<NetworkObserver>();
  }

  /**
   * Initializes the singleton once.
   * Called from the native code.
   */
  public static NetworkMonitor init(Context context) {
    if (!isInitialized()) {
      instance = new NetworkMonitor(context);
    }
    return instance;
  }

  public static boolean isInitialized() {
    return instance != null;
  }

  /**
   * Returns the singleton instance.
   */
  public static NetworkMonitor getInstance() {
    return instance;
  }

  /**
   * Enables auto detection of the current network state based on notifications from the system.
   * Note that passing true here requires the embedding app have the platform ACCESS_NETWORK_STATE
   * permission.
   *
   * @param shouldAutoDetect true if the NetworkMonitor should listen for system changes in
   *  network connectivity.
   */
  public static void setAutoDetectConnectivityState(boolean shouldAutoDetect) {
    getInstance().setAutoDetectConnectivityStateInternal(shouldAutoDetect);
  }

  private static void assertIsTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected to be true");
    }
  }

  // Called by the native code.
  private void startMonitoring(long nativeObserver) {
    Log.d(TAG, "Start monitoring from native observer " + nativeObserver);
    nativeNetworkObservers.add(nativeObserver);
    setAutoDetectConnectivityStateInternal(true);
  }

  // Called by the native code.
  private void stopMonitoring(long nativeObserver) {
    Log.d(TAG, "Stop monitoring from native observer " + nativeObserver);
    setAutoDetectConnectivityStateInternal(false);
    nativeNetworkObservers.remove(nativeObserver);
  }

  private ConnectionType getCurrentConnectionType() {
    return currentConnectionType;
  }

  private int getCurrentDefaultNetId() {
    return autoDetector == null ? INVALID_NET_ID : autoDetector.getDefaultNetId();
  }

  private void destroyAutoDetector() {
    if (autoDetector != null) {
      autoDetector.destroy();
      autoDetector = null;
    }
  }

  private void setAutoDetectConnectivityStateInternal(boolean shouldAutoDetect) {
    if (!shouldAutoDetect) {
      destroyAutoDetector();
      return;
    }
    if (autoDetector == null) {
      autoDetector = new NetworkMonitorAutoDetect(
        new NetworkMonitorAutoDetect.Observer() {
          @Override
          public void onConnectionTypeChanged(ConnectionType newConnectionType) {
            updateCurrentConnectionType(newConnectionType);
          }
        },
        applicationContext);
      final NetworkMonitorAutoDetect.NetworkState networkState =
          autoDetector.getCurrentNetworkState();
      updateCurrentConnectionType(autoDetector.getCurrentConnectionType(networkState));
    }
  }

  private void updateCurrentConnectionType(ConnectionType newConnectionType) {
    currentConnectionType = newConnectionType;
    notifyObserversOfConnectionTypeChange(newConnectionType);
  }

  /**
   * Alerts all observers of a connection change.
   */
  private void notifyObserversOfConnectionTypeChange(ConnectionType newConnectionType) {
    for (long nativeObserver : nativeNetworkObservers) {
      nativeNotifyConnectionTypeChanged(nativeObserver);
    }
    for (NetworkObserver observer : networkObservers) {
      observer.onConnectionTypeChanged(newConnectionType);
    }
  }

  /**
   * Adds an observer for any connection type changes.
   */
  public static void addNetworkObserver(NetworkObserver observer) {
    getInstance().addNetworkObserverInternal(observer);
  }

  private void addNetworkObserverInternal(NetworkObserver observer) {
    networkObservers.add(observer);
  }

  /**
   * Removes an observer for any connection type changes.
   */
  public static void removeNetworkObserver(NetworkObserver observer) {
    getInstance().removeNetworkObserverInternal(observer);
  }

  private void removeNetworkObserverInternal(NetworkObserver observer) {
    networkObservers.remove(observer);
  }

  /**
   * Checks if there currently is connectivity.
   */
  public static boolean isOnline() {
    ConnectionType connectionType = getInstance().getCurrentConnectionType();
    return connectionType != ConnectionType.CONNECTION_UNKNOWN
        && connectionType != ConnectionType.CONNECTION_NONE;
  }

  private native long nativeCreateNetworkMonitor();

  private native void nativeNotifyConnectionTypeChanged(long nativePtr);

  // For testing only.
  static void resetInstanceForTests(Context context) {
    instance = new NetworkMonitor(context);
  }

  // For testing only.
  public static NetworkMonitorAutoDetect getAutoDetectorForTest() {
    return getInstance().autoDetector;
  }
}
