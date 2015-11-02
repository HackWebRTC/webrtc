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

import static android.net.NetworkCapabilities.NET_CAPABILITY_INTERNET;

import android.Manifest.permission;
import android.annotation.SuppressLint;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.net.ConnectivityManager;
import android.net.Network;
import android.net.NetworkCapabilities;
import android.net.NetworkInfo;
import android.net.wifi.WifiInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.telephony.TelephonyManager;
import android.util.Log;

/**
 * Borrowed from Chromium's
 * src/net/android/java/src/org/chromium/net/NetworkChangeNotifierAutoDetect.java
 *
 * Used by the NetworkMonitor to listen to platform changes in connectivity.
 * Note that use of this class requires that the app have the platform
 * ACCESS_NETWORK_STATE permission.
 */
public class NetworkMonitorAutoDetect extends BroadcastReceiver {
  public static enum ConnectionType {
    CONNECTION_UNKNOWN,
    CONNECTION_ETHERNET,
    CONNECTION_WIFI,
    CONNECTION_4G,
    CONNECTION_3G,
    CONNECTION_2G,
    CONNECTION_BLUETOOTH,
    CONNECTION_NONE
  }

  static class NetworkState {
    private final boolean connected;
    // Defined from ConnectivityManager.TYPE_XXX for non-mobile; for mobile, it is
    // further divided into 2G, 3G, or 4G from the subtype.
    private final int type;
    // Defined from NetworkInfo.subtype, which is one of the TelephonyManager.NETWORK_TYPE_XXXs.
    // Will be useful to find the maximum bandwidth.
    private final int subtype;

    public NetworkState(boolean connected, int type, int subtype) {
      this.connected = connected;
      this.type = type;
      this.subtype = subtype;
    }

    public boolean isConnected() {
      return connected;
    }

    public int getNetworkType() {
      return type;
    }

    public int getNetworkSubType() {
      return subtype;
    }
  }

  /** Queries the ConnectivityManager for information about the current connection. */
  static class ConnectivityManagerDelegate {
    private final ConnectivityManager connectivityManager;

    ConnectivityManagerDelegate(Context context) {
      connectivityManager =
          (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    // For testing.
    ConnectivityManagerDelegate() {
      // All the methods below should be overridden.
      connectivityManager = null;
    }

    /**
     * Returns connection type and status information about the current
     * default network.
     */
    NetworkState getNetworkState() {
      return getNetworkState(connectivityManager.getActiveNetworkInfo());
    }

    /**
     * Returns connection type and status information about |network|.
     * Only callable on Lollipop and newer releases.
     */
    @SuppressLint("NewApi")
    NetworkState getNetworkState(Network network) {
      return getNetworkState(connectivityManager.getNetworkInfo(network));
    }

    /**
     * Returns connection type and status information gleaned from networkInfo.
     */
    NetworkState getNetworkState(NetworkInfo networkInfo) {
      if (networkInfo == null || !networkInfo.isConnected()) {
        return new NetworkState(false, -1, -1);
      }
      return new NetworkState(true, networkInfo.getType(), networkInfo.getSubtype());
    }

    /**
     * Returns all connected networks.
     * Only callable on Lollipop and newer releases.
     */
    @SuppressLint("NewApi")
    Network[] getAllNetworks() {
      return connectivityManager.getAllNetworks();
    }

    /**
     * Returns the NetID of the current default network. Returns
     * INVALID_NET_ID if no current default network connected.
     * Only callable on Lollipop and newer releases.
     */
    @SuppressLint("NewApi")
    int getDefaultNetId() {
      // Android Lollipop had no API to get the default network; only an
      // API to return the NetworkInfo for the default network. To
      // determine the default network one can find the network with
      // type matching that of the default network.
      final NetworkInfo defaultNetworkInfo = connectivityManager.getActiveNetworkInfo();
      if (defaultNetworkInfo == null) {
        return INVALID_NET_ID;
      }
      final Network[] networks = getAllNetworks();
      int defaultNetId = INVALID_NET_ID;
      for (Network network : networks) {
        if (!hasInternetCapability(network)) {
          continue;
        }
        final NetworkInfo networkInfo = connectivityManager.getNetworkInfo(network);
        if (networkInfo != null && networkInfo.getType() == defaultNetworkInfo.getType()) {
          // There should not be multiple connected networks of the
          // same type. At least as of Android Marshmallow this is
          // not supported. If this becomes supported this assertion
          // may trigger. At that point we could consider using
          // ConnectivityManager.getDefaultNetwork() though this
          // may give confusing results with VPNs and is only
          // available with Android Marshmallow.
          assert defaultNetId == INVALID_NET_ID;
          defaultNetId = networkToNetId(network);
        }
      }
      return defaultNetId;
    }

    /**
     * Returns true if {@code network} can provide Internet access. Can be used to
     * ignore specialized networks (e.g. IMS, FOTA).
     */
    @SuppressLint("NewApi")
    boolean hasInternetCapability(Network network) {
      final NetworkCapabilities capabilities =
          connectivityManager.getNetworkCapabilities(network);
      return capabilities != null && capabilities.hasCapability(NET_CAPABILITY_INTERNET);
    }
  }

  /** Queries the WifiManager for SSID of the current Wifi connection. */
  static class WifiManagerDelegate {
    private final Context context;
    private final WifiManager wifiManager;
    private final boolean hasWifiPermission;

    WifiManagerDelegate(Context context) {
      this.context = context;

      hasWifiPermission = context.getPackageManager().checkPermission(
          permission.ACCESS_WIFI_STATE, context.getPackageName())
          == PackageManager.PERMISSION_GRANTED;
      wifiManager = hasWifiPermission
          ? (WifiManager) context.getSystemService(Context.WIFI_SERVICE) : null;
    }

    // For testing.
    WifiManagerDelegate() {
      // All the methods below should be overridden.
      context = null;
      wifiManager = null;
      hasWifiPermission = false;
    }

    String getWifiSSID() {
      final Intent intent = context.registerReceiver(null,
          new IntentFilter(WifiManager.NETWORK_STATE_CHANGED_ACTION));
      if (intent != null) {
        final WifiInfo wifiInfo = intent.getParcelableExtra(WifiManager.EXTRA_WIFI_INFO);
        if (wifiInfo != null) {
          final String ssid = wifiInfo.getSSID();
          if (ssid != null) {
            return ssid;
          }
        }
      }
      return "";
    }

    boolean getHasWifiPermission() {
      return hasWifiPermission;
    }
  }

  static final int INVALID_NET_ID = -1;
  private static final String TAG = "NetworkMonitorAutoDetect";
  private final IntentFilter intentFilter;

  // Observer for the connection type change.
  private final Observer observer;

  private final Context context;
  // connectivityManagerDelegates and wifiManagerDelegate are only non-final for testing.
  private ConnectivityManagerDelegate connectivityManagerDelegate;
  private WifiManagerDelegate wifiManagerDelegate;
  private boolean isRegistered;
  private ConnectionType connectionType;
  private String wifiSSID;

  /**
   * Observer interface by which observer is notified of network changes.
   */
  public static interface Observer {
    /**
     * Called when default network changes.
     */
    public void onConnectionTypeChanged(ConnectionType newConnectionType);
  }

  /**
   * Constructs a NetworkMonitorAutoDetect. Should only be called on UI thread.
   */
  public NetworkMonitorAutoDetect(Observer observer, Context context) {
    this.observer = observer;
    this.context = context;
    connectivityManagerDelegate = new ConnectivityManagerDelegate(context);
    wifiManagerDelegate = new WifiManagerDelegate(context);

    final NetworkState networkState = connectivityManagerDelegate.getNetworkState();
    connectionType = getCurrentConnectionType(networkState);
    wifiSSID = getCurrentWifiSSID(networkState);
    intentFilter = new IntentFilter(ConnectivityManager.CONNECTIVITY_ACTION);
    registerReceiver();
  }

  /**
   * Allows overriding the ConnectivityManagerDelegate for tests.
   */
  void setConnectivityManagerDelegateForTests(ConnectivityManagerDelegate delegate) {
    connectivityManagerDelegate = delegate;
  }

  /**
   * Allows overriding the WifiManagerDelegate for tests.
   */
  void setWifiManagerDelegateForTests(WifiManagerDelegate delegate) {
    wifiManagerDelegate = delegate;
  }

  /**
   * Returns whether the object has registered to receive network connectivity intents.
   * Visible for testing.
   */
  boolean isReceiverRegisteredForTesting() {
    return isRegistered;
  }

  public void destroy() {
    unregisterReceiver();
  }

  /**
   * Registers a BroadcastReceiver in the given context.
   */
  private void registerReceiver() {
    if (!isRegistered) {
      isRegistered = true;
      context.registerReceiver(this, intentFilter);
    }
  }

  /**
   * Unregisters the BroadcastReceiver in the given context.
   */
  private void unregisterReceiver() {
    if (isRegistered) {
      isRegistered = false;
      context.unregisterReceiver(this);
    }
  }

  public NetworkState getCurrentNetworkState() {
    return connectivityManagerDelegate.getNetworkState();
  }

  /**
   * Returns NetID of device's current default connected network used for
   * communication.
   * Only implemented on Lollipop and newer releases, returns INVALID_NET_ID
   * when not implemented.
   */
  public int getDefaultNetId() {
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
      return INVALID_NET_ID;
    }
    return connectivityManagerDelegate.getDefaultNetId();
  }

  public ConnectionType getCurrentConnectionType(NetworkState networkState) {
    if (!networkState.isConnected()) {
      return ConnectionType.CONNECTION_NONE;
    }

    switch (networkState.getNetworkType()) {
      case ConnectivityManager.TYPE_ETHERNET:
        return ConnectionType.CONNECTION_ETHERNET;
      case ConnectivityManager.TYPE_WIFI:
        return ConnectionType.CONNECTION_WIFI;
      case ConnectivityManager.TYPE_WIMAX:
        return ConnectionType.CONNECTION_4G;
      case ConnectivityManager.TYPE_BLUETOOTH:
        return ConnectionType.CONNECTION_BLUETOOTH;
      case ConnectivityManager.TYPE_MOBILE:
        // Use information from TelephonyManager to classify the connection.
        switch (networkState.getNetworkSubType()) {
          case TelephonyManager.NETWORK_TYPE_GPRS:
          case TelephonyManager.NETWORK_TYPE_EDGE:
          case TelephonyManager.NETWORK_TYPE_CDMA:
          case TelephonyManager.NETWORK_TYPE_1xRTT:
          case TelephonyManager.NETWORK_TYPE_IDEN:
            return ConnectionType.CONNECTION_2G;
          case TelephonyManager.NETWORK_TYPE_UMTS:
          case TelephonyManager.NETWORK_TYPE_EVDO_0:
          case TelephonyManager.NETWORK_TYPE_EVDO_A:
          case TelephonyManager.NETWORK_TYPE_HSDPA:
          case TelephonyManager.NETWORK_TYPE_HSUPA:
          case TelephonyManager.NETWORK_TYPE_HSPA:
          case TelephonyManager.NETWORK_TYPE_EVDO_B:
          case TelephonyManager.NETWORK_TYPE_EHRPD:
          case TelephonyManager.NETWORK_TYPE_HSPAP:
            return ConnectionType.CONNECTION_3G;
          case TelephonyManager.NETWORK_TYPE_LTE:
            return ConnectionType.CONNECTION_4G;
          default:
            return ConnectionType.CONNECTION_UNKNOWN;
        }
      default:
        return ConnectionType.CONNECTION_UNKNOWN;
    }
  }

  private String getCurrentWifiSSID(NetworkState networkState) {
    if (getCurrentConnectionType(networkState) != ConnectionType.CONNECTION_WIFI) return "";
    return wifiManagerDelegate.getWifiSSID();
  }

  // BroadcastReceiver
  @Override
  public void onReceive(Context context, Intent intent) {
    final NetworkState networkState = getCurrentNetworkState();
    if (ConnectivityManager.CONNECTIVITY_ACTION.equals(intent.getAction())) {
      connectionTypeChanged(networkState);
    }
  }

  private void connectionTypeChanged(NetworkState networkState) {
    ConnectionType newConnectionType = getCurrentConnectionType(networkState);
    String newWifiSSID = getCurrentWifiSSID(networkState);
    if (newConnectionType == connectionType && newWifiSSID.equals(wifiSSID)) return;

    connectionType = newConnectionType;
    wifiSSID = newWifiSSID;
    Log.d(TAG, "Network connectivity changed, type is: " + connectionType);
    observer.onConnectionTypeChanged(newConnectionType);
  }

  /**
   * Extracts NetID of network. Only available on Lollipop and newer releases.
   */
  @SuppressLint("NewApi")
  private static int networkToNetId(Network network) {
    // NOTE(pauljensen): This depends on Android framework implementation details.
    // Fortunately this functionality is unlikely to ever change.
    // TODO(honghaiz): When we update to Android M SDK, use Network.getNetworkHandle().
    return Integer.parseInt(network.toString());
  }
}
