/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.webrtc.NetworkMonitorAutoDetect.INVALID_NET_ID;

import android.content.Context;
import android.os.Build;
import java.util.ArrayList;
import java.util.List;
import org.webrtc.NetworkMonitorAutoDetect.ConnectionType;
import org.webrtc.NetworkMonitorAutoDetect.NetworkInformation;

/**
 * Borrowed from Chromium's src/net/android/java/src/org/chromium/net/NetworkChangeNotifier.java
 *
 * <p>Triggers updates to the underlying network state from OS networking events.
 *
 * <p>Thread-safety is ensured for methods that may be called from both native code and java code,
 * including getInstance(), startMonitoring(), and stopMonitoring().
 */
public class NetworkMonitor {
  /**
   * Alerted when the connection type of the network changes. The alert is fired on the UI thread.
   */
  public interface NetworkObserver {
    public void onConnectionTypeChanged(ConnectionType connectionType);
  }

  private static final String TAG = "NetworkMonitor";

  // Lazy initialization holder class idiom for static fields.
  private static class InstanceHolder {
    // We are storing application context so it is okay.
    static final NetworkMonitor instance = new NetworkMonitor();
  }

  // Native observers of the connection type changes.
  private final ArrayList<Long> nativeNetworkObservers;
  // Java observers of the connection type changes.
  private final ArrayList<NetworkObserver> networkObservers;

  private final Object autoDetectorLock = new Object();
  // Object that detects the connection type changes and brings up mobile networks.
  private NetworkMonitorAutoDetect autoDetector;
  // Also guarded by autoDetectorLock.
  private int numMonitors;

  private ConnectionType currentConnectionType = ConnectionType.CONNECTION_UNKNOWN;

  private NetworkMonitor() {
    nativeNetworkObservers = new ArrayList<Long>();
    networkObservers = new ArrayList<NetworkObserver>();
    numMonitors = 0;
  }

  // TODO(sakal): Remove once downstream dependencies have been updated.
  @Deprecated
  public static void init(Context context) {}

  /** Returns the singleton instance. This may be called from native or from Java code. */
  @CalledByNative
  public static NetworkMonitor getInstance() {
    return InstanceHolder.instance;
  }

  private static void assertIsTrue(boolean condition) {
    if (!condition) {
      throw new AssertionError("Expected to be true");
    }
  }

  /**
   * Enables auto detection of the network state change and brings up mobile networks for using
   * multi-networking. This requires the embedding app have the platform ACCESS_NETWORK_STATE and
   * CHANGE_NETWORK_STATE permission.
   */
  public void startMonitoring(Context applicationContext) {
    synchronized (autoDetectorLock) {
      ++numMonitors;
      if (autoDetector == null) {
        autoDetector = createAutoDetector(applicationContext);
      }
      currentConnectionType =
          NetworkMonitorAutoDetect.getConnectionType(autoDetector.getCurrentNetworkState());
    }
  }

  /** Deprecated, pass in application context in startMonitoring instead. */
  @Deprecated
  public void startMonitoring() {
    startMonitoring(ContextUtils.getApplicationContext());
  }

  /**
   * Enables auto detection of the network state change and brings up mobile networks for using
   * multi-networking. This requires the embedding app have the platform ACCESS_NETWORK_STATE and
   * CHANGE_NETWORK_STATE permission.
   */
  @CalledByNative
  private void startMonitoring(Context applicationContext, long nativeObserver) {
    Logging.d(TAG, "Start monitoring with native observer " + nativeObserver);

    startMonitoring();
    // The native observers expect a network list update after they call startMonitoring.
    nativeNetworkObservers.add(nativeObserver);
    updateObserverActiveNetworkList(nativeObserver);
    // currentConnectionType was updated in startMonitoring().
    // Need to notify the native observers here.
    notifyObserversOfConnectionTypeChange(currentConnectionType);
  }

  /** Stop network monitoring. If no one is monitoring networks, destroy and reset autoDetector. */
  public void stopMonitoring() {
    synchronized (autoDetectorLock) {
      if (--numMonitors == 0) {
        autoDetector.destroy();
        autoDetector = null;
      }
    }
  }

  @CalledByNative
  private void stopMonitoring(long nativeObserver) {
    Logging.d(TAG, "Stop monitoring with native observer " + nativeObserver);
    stopMonitoring();
    nativeNetworkObservers.remove(nativeObserver);
  }

  // Returns true if network binding is supported on this platform.
  @CalledByNative
  private boolean networkBindingSupported() {
    synchronized (autoDetectorLock) {
      return autoDetector != null && autoDetector.supportNetworkCallback();
    }
  }

  @CalledByNative
  private static int androidSdkInt() {
    return Build.VERSION.SDK_INT;
  }

  private ConnectionType getCurrentConnectionType() {
    return currentConnectionType;
  }

  private long getCurrentDefaultNetId() {
    synchronized (autoDetectorLock) {
      return autoDetector == null ? INVALID_NET_ID : autoDetector.getDefaultNetId();
    }
  }

  private NetworkMonitorAutoDetect createAutoDetector(Context appContext) {
    return new NetworkMonitorAutoDetect(new NetworkMonitorAutoDetect.Observer() {

      @Override
      public void onConnectionTypeChanged(ConnectionType newConnectionType) {
        updateCurrentConnectionType(newConnectionType);
      }

      @Override
      public void onNetworkConnect(NetworkInformation networkInfo) {
        notifyObserversOfNetworkConnect(networkInfo);
      }

      @Override
      public void onNetworkDisconnect(long networkHandle) {
        notifyObserversOfNetworkDisconnect(networkHandle);
      }
    }, appContext);
  }

  private void updateCurrentConnectionType(ConnectionType newConnectionType) {
    currentConnectionType = newConnectionType;
    notifyObserversOfConnectionTypeChange(newConnectionType);
  }

  /** Alerts all observers of a connection change. */
  private void notifyObserversOfConnectionTypeChange(ConnectionType newConnectionType) {
    for (long nativeObserver : nativeNetworkObservers) {
      nativeNotifyConnectionTypeChanged(nativeObserver);
    }
    for (NetworkObserver observer : networkObservers) {
      observer.onConnectionTypeChanged(newConnectionType);
    }
  }

  private void notifyObserversOfNetworkConnect(NetworkInformation networkInfo) {
    for (long nativeObserver : nativeNetworkObservers) {
      nativeNotifyOfNetworkConnect(nativeObserver, networkInfo);
    }
  }

  private void notifyObserversOfNetworkDisconnect(long networkHandle) {
    for (long nativeObserver : nativeNetworkObservers) {
      nativeNotifyOfNetworkDisconnect(nativeObserver, networkHandle);
    }
  }

  private void updateObserverActiveNetworkList(long nativeObserver) {
    List<NetworkInformation> networkInfoList;
    synchronized (autoDetectorLock) {
      networkInfoList = (autoDetector == null) ? null : autoDetector.getActiveNetworkList();
    }
    if (networkInfoList == null || networkInfoList.size() == 0) {
      return;
    }

    NetworkInformation[] networkInfos = new NetworkInformation[networkInfoList.size()];
    networkInfos = networkInfoList.toArray(networkInfos);
    nativeNotifyOfActiveNetworkList(nativeObserver, networkInfos);
  }

  /**
   * Adds an observer for any connection type changes.
   *
   * @deprecated Use getInstance(appContext).addObserver instead.
   */
  @Deprecated
  public static void addNetworkObserver(NetworkObserver observer) {
    getInstance().addObserver(observer);
  }

  public void addObserver(NetworkObserver observer) {
    networkObservers.add(observer);
  }

  /**
   * Removes an observer for any connection type changes.
   *
   * @deprecated Use getInstance(appContext).removeObserver instead.
   */
  @Deprecated
  public static void removeNetworkObserver(NetworkObserver observer) {
    getInstance().removeObserver(observer);
  }

  public void removeObserver(NetworkObserver observer) {
    networkObservers.remove(observer);
  }

  /** Checks if there currently is connectivity. */
  public static boolean isOnline() {
    ConnectionType connectionType = getInstance().getCurrentConnectionType();
    return connectionType != ConnectionType.CONNECTION_NONE;
  }

  @NativeClassQualifiedName("webrtc::jni::AndroidNetworkMonitor")
  private native void nativeNotifyConnectionTypeChanged(long nativePtr);

  @NativeClassQualifiedName("webrtc::jni::AndroidNetworkMonitor")
  private native void nativeNotifyOfNetworkConnect(long nativePtr, NetworkInformation networkInfo);

  @NativeClassQualifiedName("webrtc::jni::AndroidNetworkMonitor")
  private native void nativeNotifyOfNetworkDisconnect(long nativePtr, long networkHandle);

  @NativeClassQualifiedName("webrtc::jni::AndroidNetworkMonitor")
  private native void nativeNotifyOfActiveNetworkList(
      long nativePtr, NetworkInformation[] networkInfos);

  // For testing only.
  static NetworkMonitorAutoDetect getAutoDetectorForTest(Context context) {
    NetworkMonitor networkMonitor = getInstance();
    NetworkMonitorAutoDetect autoDetector = networkMonitor.createAutoDetector(context);
    return networkMonitor.autoDetector = autoDetector;
  }
}
