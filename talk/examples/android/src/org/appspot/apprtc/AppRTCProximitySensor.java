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

import org.appspot.apprtc.util.AppRTCUtils;
import org.appspot.apprtc.util.AppRTCUtils.NonThreadSafe;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Build;
import android.util.Log;

/**
 * AppRTCProximitySensor manages functions related to the proximity sensor in
 * the AppRTC demo.
 * On most device, the proximity sensor is implemented as a boolean-sensor.
 * It returns just two values "NEAR" or "FAR". Thresholding is done on the LUX
 * value i.e. the LUX value of the light sensor is compared with a threshold.
 * A LUX-value more than the threshold means the proximity sensor returns "FAR".
 * Anything less than the threshold value and the sensor  returns "NEAR".
 */
public class AppRTCProximitySensor implements SensorEventListener {
  private static final String TAG = "AppRTCProximitySensor";

  // This class should be created, started and stopped on one thread
  // (e.g. the main thread). We use |nonThreadSafe| to ensure that this is
  // the case. Only active when |DEBUG| is set to true.
  private final NonThreadSafe nonThreadSafe = new AppRTCUtils.NonThreadSafe();

  private final Runnable onSensorStateListener;
  private final SensorManager sensorManager;
  private Sensor proximitySensor = null;
  private boolean lastStateReportIsNear = false;

  /** Construction */
  static AppRTCProximitySensor create(Context context,
      Runnable sensorStateListener) {
    return new AppRTCProximitySensor(context, sensorStateListener);
  }

  private AppRTCProximitySensor(Context context, Runnable sensorStateListener) {
    Log.d(TAG, "AppRTCProximitySensor" + AppRTCUtils.getThreadInfo());
    onSensorStateListener = sensorStateListener;
    sensorManager = ((SensorManager) context.getSystemService(
        Context.SENSOR_SERVICE));
  }

  /**
   * Activate the proximity sensor. Also do initializtion if called for the
   * first time.
   */
  public boolean start() {
    checkIfCalledOnValidThread();
    Log.d(TAG, "start" + AppRTCUtils.getThreadInfo());
    if (!initDefaultSensor()) {
      // Proximity sensor is not supported on this device.
      return false;
    }
    sensorManager.registerListener(
        this, proximitySensor, SensorManager.SENSOR_DELAY_NORMAL);
    return true;
  }

  /** Deactivate the proximity sensor. */
  public void stop() {
    checkIfCalledOnValidThread();
    Log.d(TAG, "stop" + AppRTCUtils.getThreadInfo());
    if (proximitySensor == null) {
      return;
    }
    sensorManager.unregisterListener(this, proximitySensor);
  }

  /** Getter for last reported state. Set to true if "near" is reported. */
  public boolean sensorReportsNearState() {
    checkIfCalledOnValidThread();
    return lastStateReportIsNear;
  }

  @Override
  public final void onAccuracyChanged(Sensor sensor, int accuracy) {
    checkIfCalledOnValidThread();
    AppRTCUtils.assertIsTrue(sensor.getType() == Sensor.TYPE_PROXIMITY);
    if (accuracy == SensorManager.SENSOR_STATUS_UNRELIABLE) {
      Log.e(TAG, "The values returned by this sensor cannot be trusted");
    }
  }

  @Override
  public final void onSensorChanged(SensorEvent event) {
    checkIfCalledOnValidThread();
    AppRTCUtils.assertIsTrue(event.sensor.getType() == Sensor.TYPE_PROXIMITY);
    // As a best practice; do as little as possible within this method and
    // avoid blocking.
    float distanceInCentimeters = event.values[0];
    if (distanceInCentimeters < proximitySensor.getMaximumRange()) {
      Log.d(TAG, "Proximity sensor => NEAR state");
      lastStateReportIsNear = true;
    } else {
      Log.d(TAG, "Proximity sensor => FAR state");
      lastStateReportIsNear = false;
    }

    // Report about new state to listening client. Client can then call
    // sensorReportsNearState() to query the current state (NEAR or FAR).
    if (onSensorStateListener != null) {
      onSensorStateListener.run();
    }

    Log.d(TAG, "onSensorChanged" + AppRTCUtils.getThreadInfo() + ": "
        + "accuracy=" + event.accuracy
        + ", timestamp=" + event.timestamp + ", distance=" + event.values[0]);
  }

  /**
   * Get default proximity sensor if it exists. Tablet devices (e.g. Nexus 7)
   * does not support this type of sensor and false will be retured in such
   * cases.
   */
  private boolean initDefaultSensor() {
    if (proximitySensor != null) {
      return true;
    }
    proximitySensor = sensorManager.getDefaultSensor(Sensor.TYPE_PROXIMITY);
    if (proximitySensor == null) {
      return false;
    }
    logProximitySensorInfo();
    return true;
  }

  /** Helper method for logging information about the proximity sensor. */
  private void logProximitySensorInfo() {
    if (proximitySensor == null) {
      return;
    }
    StringBuilder info = new StringBuilder("Proximity sensor: ");
    info.append("name=" + proximitySensor.getName());
    info.append(", vendor: " + proximitySensor.getVendor());
    info.append(", power: " + proximitySensor.getPower());
    info.append(", resolution: " + proximitySensor.getResolution());
    info.append(", max range: " + proximitySensor.getMaximumRange());
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.GINGERBREAD) {
      // Added in API level 9.
      info.append(", min delay: " + proximitySensor.getMinDelay());
    }
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT_WATCH) {
      // Added in API level 20.
      info.append(", type: " + proximitySensor.getStringType());
    }
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
      // Added in API level 21.
      info.append(", max delay: " + proximitySensor.getMaxDelay());
      info.append(", reporting mode: " + proximitySensor.getReportingMode());
      info.append(", isWakeUpSensor: " + proximitySensor.isWakeUpSensor());
    }
    Log.d(TAG, info.toString());
  }

  /**
   * Helper method for debugging purposes. Ensures that method is
   * called on same thread as this object was created on.
   */
  private void checkIfCalledOnValidThread() {
    if (!nonThreadSafe.calledOnValidThread()) {
      throw new IllegalStateException("Method is not called on valid thread");
    }
  }
}
